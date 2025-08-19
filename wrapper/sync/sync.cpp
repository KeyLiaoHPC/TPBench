#include "sync.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>


using rdmasync::TRIGGER_MSG;

namespace {

#ifdef DEBUG
#define DBGPRINT(info, var, debug) \
    if (debug) { \
        std::cout << "[TPSYNC ERROR]"<< "[" << __FILE__ << ":" << __LINE__ << "] " << info << ": " << var << std::endl; \
    } \

#define WARNINGPRINT(info, var, debug) \
    if (debug) { \
        std::cout << "[TPSYNC ERROR]"<< "[" << __FILE__ << ":" << __LINE__ << "] " << info << ": " << var << std::endl; \
    } \

#else

#define DBGPRINT(info, var, debug) 
#define WARNINGPRINT(info, var, debug) 
#endif

#define ERRORPRINT(info, var) \
    std::cerr << "[TPSYNC ERROR]"<< "[" << __FILE__ << ":" << __LINE__ << "] " << info << ": " << var << std::endl; \

// --------- tools ----------
static inline void cpu_relax() {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#endif
}

// Poll for one completion on a CQ (simple busy-wait loop)
static void poll_cq_one(ibv_cq* cq) {
    while (true) {
        ibv_wc wc{};
        int n = ibv_poll_cq(cq, 1, &wc);
        if (n < 0) return;          // Be tolerant: just return on error
        if (n == 0) continue;
        if (wc.status != IBV_WC_SUCCESS) continue;
        return;
    }
}

// Poll for one completion on a CQ with timeout (returns false on timeout)
static bool poll_cq_one_timeout(ibv_cq* cq, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);
    
    while (true) {
        ibv_wc wc{};
        int n = ibv_poll_cq(cq, 1, &wc);
        if (n < 0) return false;    // Error
        if (n > 0 && wc.status == IBV_WC_SUCCESS) return true;  // Success
        
        // Check timeout
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - start_time >= timeout_duration) {
            return false;  // Timeout
        }
        
        // Small sleep to avoid busy-waiting
        usleep(100);  // 100 microseconds
    }
}

// Helper function to post RDMA write operation
static bool post_rdma_write(ibv_qp* qp, void* local_addr, size_t length, uint64_t remote_addr, uint32_t rkey, ibv_mr* local_mr) {
    if (!qp || !local_addr || !local_mr) return false;
    
    ibv_sge sge{};
    sge.addr = (uintptr_t)local_addr;
    sge.length = (uint32_t)length;
    sge.lkey = local_mr->lkey;
    
    ibv_send_wr wr{}, *bad = nullptr;
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = remote_addr;
    wr.wr.rdma.rkey = rkey;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    
    return ibv_post_send(qp, &wr, &bad) == 0;
}

#pragma pack(push,1)
struct MsgPart2C {          // participant -> coordinator: expose local flag {addr, rkey}
    uint64_t flag_addr;
    uint32_t flag_rkey;
    uint32_t pad;
};
struct MsgEpoch {           // participant -> coordinator: send current epoch
    uint64_t epoch;
};
struct MsgC2P {             // coordinator -> participant: trigger message
    uint64_t flag_addr;     // participant's flag address (for verification)
    uint32_t flag_rkey;     // participant's flag rkey (for verification)  
    TRIGGER_MSG send_msg;   // trigger message type
    uint32_t pad;
};
#pragma pack(pop)
struct Conn {
    int client_fd;
    struct ibv_qp* qp;
    bool established;
    MsgPart2C part_info;  // participant flag info
    MsgPart2C recv_buffer;  // separate buffer for receiving data
    struct ibv_mr* flag_mr;  // participant's flag MR info
    uint32_t last_epoch;
    
    // Additional fields needed for coordination
    struct ibv_mr* mr_ctrl_recv;
    struct ibv_mr* mr_ctrl_send;
    MsgEpoch recv_epoch;
    rdma_cm_id* id;  // RDMA connection ID
};

class RDMASyncImpl {
public:
    static RDMASyncImpl& inst() {
        static RDMASyncImpl s;
        return s;
    }

    // Public entry points
    void init(size_t mpi_rank = 0) {
        if (inited_) return;
        const char* en = std::getenv("RDMASYNC_ENABLE");
        if (!en || std::strcmp(en, "1") != 0) { enabled_ = false; inited_ = true; return; }
        enabled_ = true;

        const char* dbg = std::getenv("RDMASYNC_DEBUG");
        if (dbg && std::strcmp(dbg, "1") == 0) {
            debug_ = true;
        }

        const char* role_env = std::getenv("RDMASYNC_ROLE");
        std::string auto_role = role_env ? role_env : "";
        if (auto_role == "AUTO") {
            if (mpi_rank != 0) {
                auto_role = "participant";
            } else {
                auto_role = "coordinator";
            }
        }
        if (auto_role.empty()) { enabled_ = false; inited_ = true; return; }
        role_ = auto_role;
        
        // Store MPI rank for later use
        mpi_rank_ = mpi_rank;

        const char* port = std::getenv("RDMASYNC_PORT");
        port_ = port ? port : "2673";

        const char* spin = std::getenv("RDMASYNC_SPIN_US");
        spin_us_ = spin ? std::max(0, std::atoi(spin)) : 200;

        if (role_ == "coordinator") {
            // In coordinator role, only rank 0 sets up the coordinator
            if (mpi_rank == 0) {
                const char* bind = std::getenv("RDMASYNC_BIND_IP");
                bind_ip_ = bind ? bind : "0.0.0.0";
                const char* exp  = std::getenv("RDMASYNC_EXPECTED");
                expected_ = exp ? std::atoi(exp) : 0;
                if (expected_ <= 0) { enabled_ = false; inited_ = true; return; }
                if ( setup_coordinator() == false ) {
                    enabled_ = false;
                    inited_ = true;
                    ERRORPRINT("Coordinator setup failed", bind_ip_);
                    return;
                }
            } else {
                // Non-rank-0 in coordinator group are passive
                is_passive_participant_ = true;
                // DBGPRINT("Passive coordinator participant (rank > 0)", mpi_rank, debug_);
            }
        } else if (role_ == "participant") {
            const char* server = std::getenv("RDMASYNC_SERVER_IP");
            if (!server) {
                if (role_env && std::strcmp(role_env, "AUTO") == 0) {
                    ERRORPRINT("AUTO mode requires RDMASYNC_SERVER_IP to be set to coordinator's IP (not 127.0.0.1)", "missing");
                } else {
                    ERRORPRINT("RDMASYNC_SERVER_IP not set", "missing");
                }
                ERRORPRINT("Please set RDMASYNC_SERVER_IP to the coordinator's IP address", "missing");
                enabled_ = false; 
                inited_ = true; 
                return; 
            }
            server_ip_ = server;
            
            // In participant role, only rank 0 establishes RDMA connection
            if (mpi_rank == 0) {
                if (setup_participant() == false) {
                    enabled_ = false;
                    inited_ = false;
                    ERRORPRINT("Participant setup failed", server_ip_);
                    return;
                }
            } else {
                // Non-rank-0 participants are just passive followers
                is_passive_participant_ = true;
                // DBGPRINT("Passive participant (rank > 0)", mpi_rank, debug_);
            }
        } else {
            enabled_ = false;
        }
        inited_ = true;

        // DBGPRINT("RDMASync initialized: ", role_, debug_);
    }

    void finalize() {
        if (!enabled_) return;
        
        // Passive participants don't have RDMA resources
        if (is_passive_participant_) {
            enabled_ = false;
            return;
        }
        
        if (role_ == "participant") {
            if (id_p_) {
                rdma_disconnect(id_p_);
                rdma_destroy_qp(id_p_);
                if (mr_flag_)      ibv_dereg_mr(mr_flag_);
                if (mr_ctrl_send_) ibv_dereg_mr(mr_ctrl_send_);
                if (mr_ctrl_recv_) ibv_dereg_mr(mr_ctrl_recv_);
                if (cq_)           ibv_destroy_cq(cq_);
                if (pd_)           ibv_dealloc_pd(pd_);
                rdma_destroy_id(id_p_);
            }
            if (ec_) rdma_destroy_event_channel(ec_);
            if (flag_mem_) free(flag_mem_);
            if (ctrl_mem_) free(ctrl_mem_);
        } else if (role_ == "coordinator") {
            for (auto& c : conns_) {
                rdma_disconnect(c.id);
                rdma_destroy_qp(c.id);
                if (c.mr_ctrl_send) ibv_dereg_mr(c.mr_ctrl_send);
                if (c.mr_ctrl_recv) ibv_dereg_mr(c.mr_ctrl_recv);
                rdma_destroy_id(c.id);
            }
            if (mr_ready_) ibv_dereg_mr(mr_ready_);
            if (cq_)       ibv_destroy_cq(cq_);
            if (pd_)       ibv_dealloc_pd(pd_);
            if (listen_id_) rdma_destroy_id(listen_id_);
            if (ec_)        rdma_destroy_event_channel(ec_);
            if (ready_mem_) free(ready_mem_);
        }
        enabled_ = false;
    }

    void tp_sync(const char* /*name*/) {
        if (!enabled_) {
            WARNINGPRINT("RDMASync not enabled, skipping tp_sync", "", debug_);
            return;
        }
        
        // Passive participants (rank > 0 in participant mpirun) just wait
        if (is_passive_participant_) {
            // WARNINGPRINT("Passive participant, skipping tp_sync", "", debug_);
            // In cross-mpirun mode, non-rank-0 participants don't have RDMA connection
            // They synchronize via MPI barrier within their own communicator
            // For now, just return as they don't participate in cross-mpirun sync
            return;
        }
        
        uint64_t e = epoch_.fetch_add(1, std::memory_order_relaxed) + 1;

        if (role_ == "participant") {
            // Send the current epoch to the coordinator
            // Use the ctrl_mem_ buffer to store the epoch message
            MsgEpoch* msg = reinterpret_cast<MsgEpoch*>(ctrl_mem_);
            msg->epoch = e;
            
            ibv_sge sge{ .addr=(uintptr_t)msg, .length=(uint32_t)sizeof(*msg), .lkey=mr_ctrl_send_->lkey };
            ibv_send_wr wr{}, *bad=nullptr;
            wr.opcode = IBV_WR_SEND;
            wr.send_flags = IBV_SEND_SIGNALED;
            wr.sg_list = &sge; wr.num_sge = 1;
            // Send epoch flag notification to coordinator
            DBGPRINT("Participant sending for epoch flag", e, debug_);
            int ret = ibv_post_send(id_p_->qp, &wr, &bad);
            if (ret != 0) {
                ERRORPRINT("Failed to post send", ret);
                return; // Early return on error
            }
            poll_cq_one(cq_);
            // Wait for the coordinator to write back GO (flag == epoch)
            DBGPRINT("Participant waiting for epoch flag", e, debug_);
            volatile uint64_t* flag = (volatile uint64_t*) flag_mem_;
            while (__builtin_expect(*flag != e, 1)) {
                cpu_relax();
                // Reduce CPU usage without affecting latency too much
                // if (*flag == 0) {
                //     usleep(10); // Only sleep if flag is still 0
                // }
            }
            DBGPRINT("Participant tp_sync completed", e, debug_);
            return;
        }

        // Coordinator: wait for `expected_` RECV completions, then broadcast GO
        DBGPRINT("About to ensure_posted_recvs, expected", expected_, debug_);
        DBGPRINT("Current posted_recvs", posted_recvs_, debug_);
        int need_to_post = expected_ - posted_recvs_;
        DBGPRINT("Need to post RECVs", need_to_post, debug_);
        ensure_posted_recvs(need_to_post);
        DBGPRINT("Coordinator waiting for RECVs", posted_recvs_, debug_);
        int got = 0;
        long long poll_attempts = 0;
        const long long max_poll_attempts = 6000000000; // Add timeout
        // Recieve
        while (got < expected_ && poll_attempts < max_poll_attempts) {
            ibv_wc wc{};
            int n = ibv_poll_cq(cq_, 1, &wc);
            poll_attempts++;
            
            if (n < 0) {
                ERRORPRINT("CQ poll error", n);
                break;
            }
            if (n == 0) continue;
            
            DBGPRINT("CQ poll returned completion", n, debug_);
            DBGPRINT("Work completion status", wc.status, debug_);
            DBGPRINT("Work completion opcode", wc.opcode, debug_);
            
            if (wc.status != IBV_WC_SUCCESS) {
                ERRORPRINT("Work completion error", wc.status);
                continue;
            }
            if (wc.opcode == IBV_WC_RECV) {
                got++;
                DBGPRINT("Received RECV completion", got, debug_);
            }
        }
        
        if (poll_attempts >= max_poll_attempts) {
            ERRORPRINT("Timeout waiting for RECVs after attempts", poll_attempts);
            return; // Early return on timeout
        }
        DBGPRINT("Hook coordinator received all RECVs", got, debug_);
        posted_recvs_ -= expected_;

        DBGPRINT("Coordinator sending", "<epoch>", debug_);
        // Write the epoch to each participant's flag (RDMA WRITE, inline)
        for (auto& c : conns_) {
            uint64_t val = e;
            
            // DEBUG: Check flag info right before RDMA WRITE
            DBGPRINT("RDMA WRITE: flag_addr", c.part_info.flag_addr, debug_);
            DBGPRINT("RDMA WRITE: flag_rkey", c.part_info.flag_rkey, debug_);
            
            ibv_sge sge{ .addr=(uintptr_t)&val, .length=(uint32_t)sizeof(val), .lkey=0 };
            ibv_send_wr wr{}, *bad=nullptr;
            wr.opcode = IBV_WR_RDMA_WRITE;
            wr.send_flags = IBV_SEND_INLINE | IBV_SEND_SIGNALED;
            wr.wr.rdma.remote_addr = c.part_info.flag_addr;
            wr.wr.rdma.rkey = c.part_info.flag_rkey;
            wr.sg_list = &sge; wr.num_sge = 1;
            DBGPRINT("Posting RDMA WRITE to participant flag", c.part_info.flag_addr, debug_);
            DBGPRINT("Using rkey", c.part_info.flag_rkey, debug_);
            DBGPRINT("Writing epoch value", val, debug_);
            int ret = ibv_post_send(c.qp, &wr, &bad);
            if (ret != 0) {
                ERRORPRINT("Failed to post RDMA WRITE", ret);
                return;
            }
        }
        int done = 0;
        while (done < expected_) {
            ibv_wc wc{};
            int n = ibv_poll_cq(cq_, 1, &wc);
            if (n <= 0) continue;
            if (wc.status != IBV_WC_SUCCESS) {
                WARNINGPRINT("RDMA WRITE completion error", wc.status, debug_);
                continue;
            }
            if (wc.opcode == IBV_WC_RDMA_WRITE) {
                done++;
                DBGPRINT("RDMA WRITE completed", done, debug_);
            }
        }
        DBGPRINT("All RDMA WRITEs completed", done, debug_);
        DBGPRINT("Coordinator tp_sync completed", 0, debug_);
    }

    /**
     * tp_trigger function can be used to trigger a specific function in other participants,
     * when coordinator sends a trigger message, the participant should respond accordingly
     * the preset behavior
     * @param send_msg The message to send to other participants
     */
    TRIGGER_MSG tp_trigger(TRIGGER_MSG send_msg) {
        if (!enabled_) {
            WARNINGPRINT("RDMASync not enabled, skipping tp_trigger", "", debug_);
            return TRIGGER_MSG::FINISH;
        }
        
        // Passive participants (rank > 0 in participant mpirun) just wait
        if (is_passive_participant_) {
            // WARNINGPRINT("Passive participant, skipping tp_trigger", "", debug_);
            return TRIGGER_MSG::FINISH;
        }

        // When role_ is coordinator,
        // send message "send_msg" to all participant, and the participant should respond
        // try 10 times until success
        if (role_ == "coordinator") {
            for (int i = 0; i < 10; i++) {
                // Send trigger message to all participants
                for (auto& c : conns_) {
                    MsgC2P* msg = reinterpret_cast<MsgC2P*>(ctrl_mem_);
                    msg->flag_addr = c.part_info.flag_addr;
                    msg->flag_rkey = c.part_info.flag_rkey;
                    msg->send_msg = send_msg;
                    msg->pad = 0;
                    
                    // Post RDMA WRITE to participant's flag memory with trigger message
                    if (!post_rdma_write(c.qp, msg, sizeof(MsgC2P), c.part_info.flag_addr, c.part_info.flag_rkey, mr_ctrl_send_)) {
                        DBGPRINT("Failed to post RDMA WRITE", i, debug_);
                        continue;
                    }
                }
                // Wait for all participants to respond
                if (wait_for_responses()) {
                    DBGPRINT("All participants responded", "", debug_);
                    return send_msg;  // Return the sent message
                }
                usleep(100000);  // Sleep 100ms before retrying
            }
            ERRORPRINT("Failed to trigger participants", "retries_exhausted");
        }

        // When role_ is participant,
        // wait for message from coordinator and respond accordingly
        if (role_ == "participant") {
            // Wait for the coordinator to write a trigger message to our flag memory
            volatile uint64_t* flag = (volatile uint64_t*) flag_mem_;

            // Wait for the coordinator to write a trigger message
            while (__builtin_expect(*flag == 0, 1)) {
                cpu_relax();
            }
            
            // The flag memory now contains the MsgC2P structure
            MsgC2P* received_msg = reinterpret_cast<MsgC2P*>(flag_mem_);
            TRIGGER_MSG received_trigger = received_msg->send_msg;
            
            DBGPRINT("Received trigger message", static_cast<int>(received_trigger), debug_);
            
            // Reset flag after processing
            *flag = 0;
            
            // Send acknowledgment back to coordinator by writing to ctrl_mem_
            // This will be detected by coordinator's wait_for_responses()
            uint64_t ack = 1;
            if (mr_ctrl_send_ && cq_) {
                ibv_sge sge{};
                sge.addr = (uintptr_t)&ack;
                sge.length = sizeof(ack);
                sge.lkey = mr_ctrl_send_->lkey;
                
                ibv_send_wr wr{}, *bad = nullptr;
                wr.opcode = IBV_WR_RDMA_WRITE;
                wr.send_flags = IBV_SEND_INLINE | IBV_SEND_SIGNALED;
                wr.wr.rdma.remote_addr = (uint64_t)(uintptr_t)ctrl_mem_; // Write back to our ctrl memory
                wr.wr.rdma.rkey = mr_ctrl_send_->rkey;
                wr.sg_list = &sge;
                wr.num_sge = 1;
                
                if (ibv_post_send(id_p_->qp, &wr, &bad) == 0) {
                    // Wait for completion
                    poll_cq_one(cq_);
                    DBGPRINT("Sent acknowledgment to coordinator", "", debug_);
                }
            }
            return received_trigger;  // Return the received trigger message
        }
    }


private:
    // Helper method to wait for responses from all participants
    bool wait_for_responses() {
        if (!enabled_ || role_ != "coordinator") return false;
        
        int responses_received = 0;
        const int timeout_ms = 5000; // 5 second timeout
        auto start_time = std::chrono::steady_clock::now();
        
        while (responses_received < expected_) {
            ibv_wc wc{};
            int n = ibv_poll_cq(cq_, 1, &wc);
            
            if (n > 0 && wc.status == IBV_WC_SUCCESS) {
                if (wc.opcode == IBV_WC_RDMA_WRITE) {
                    responses_received++;
                    DBGPRINT("Response received", responses_received, debug_);
                }
            }
            
            // Check timeout
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
            if (elapsed.count() > timeout_ms) {
                DBGPRINT("Timeout waiting for responses", responses_received, debug_);
                return false;
            }
            
            usleep(1000); // Small delay to avoid busy waiting
        }
        
        return true;
    }

    // Participant initialization
    bool setup_participant() {
        // Optional small delay to let the coordinator come up first
        const char* role_env = std::getenv("RDMASYNC_ROLE");
        if (role_env && std::strcmp(role_env, "AUTO") == 0) {
            usleep(200000); // 200ms
        } else if (role_env && std::strcmp(role_env, "participant") == 0) {
            usleep(500000); // 500ms
        }

        DBGPRINT("Setting up participant", server_ip_, debug_);

        // Create (or reuse) event channel once
        if (!ec_) {
            ec_ = rdma_create_event_channel();
            if (!ec_) {
                ERRORPRINT("Failed to create event channel", "");
                return false;
            }
        }

        // Resolve server address once (reusable across attempts)
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(server_ip_.c_str(), port_.c_str(), &hints, &res)) {
            ERRORPRINT("Failed to resolve address", server_ip_);
            return false;
        }

        const int kMaxAttempts = 40;
        int attempt = 0;
        bool connected = false;
        size_t ctrl_size = std::max(sizeof(MsgPart2C), sizeof(MsgEpoch));

        // We allocate ctrl/flag memory once; MRs are per-attempt (bound to PD)
        if (!ctrl_mem_) {
            void* ctrl_mem = nullptr;
            // Allocate enough space for both MsgPart2C and MsgEpoch
            posix_memalign(&ctrl_mem, 64, ctrl_size);
            ctrl_mem_ = ctrl_mem;
        }
        if (!flag_mem_) {
            void* mem=nullptr;
            if (posix_memalign(&mem, 64, 64) != 0) {
                ERRORPRINT("Failed to allocate flag memory", "");
                freeaddrinfo(res);
                return false;
            }
            flag_mem_ = mem;
        }

        while (attempt++ < kMaxAttempts && !connected) {
            // Clean any leftovers from previous attempt
            if (id_p_) {
                if (id_p_->qp)      rdma_destroy_qp(id_p_);
                if (mr_flag_)      { ibv_dereg_mr(mr_flag_);      mr_flag_ = nullptr; }
                if (mr_ctrl_send_) { ibv_dereg_mr(mr_ctrl_send_); mr_ctrl_send_ = nullptr; }
                if (mr_ctrl_recv_) { ibv_dereg_mr(mr_ctrl_recv_); mr_ctrl_recv_ = nullptr; }
                if (cq_)           { ibv_destroy_cq(cq_);         cq_ = nullptr; }
                if (pd_)           { ibv_dealloc_pd(pd_);         pd_ = nullptr; }
                rdma_destroy_id(id_p_); id_p_ = nullptr;
            }
            *(uint64_t*)flag_mem_ = 0;  // reset flag each attempt

            // 1) create id
            if (rdma_create_id(ec_, &id_p_, nullptr, RDMA_PS_TCP)) {
                ERRORPRINT("rdma_create_id failed", attempt);
                goto BACKOFF;
            }

            // 2) resolve addr
            if (rdma_resolve_addr(id_p_, nullptr, res->ai_addr, 2000)) {
                ERRORPRINT("rdma_resolve_addr failed", attempt);
                goto BACKOFF;
            }
            {
                rdma_cm_event* ev = nullptr;
                if (rdma_get_cm_event(ec_, &ev) || ev->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
                    if (ev) rdma_ack_cm_event(ev);
                    ERRORPRINT("ADDR_RESOLVED not received", attempt);
                    goto BACKOFF;
                }
                rdma_ack_cm_event(ev);
            }

            // 3) resolve route
            if (rdma_resolve_route(id_p_, 2000)) {
                ERRORPRINT("rdma_resolve_route failed", attempt);
                goto BACKOFF;
            }
            {
                rdma_cm_event* ev = nullptr;
                if (rdma_get_cm_event(ec_, &ev) || ev->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
                    if (ev) rdma_ack_cm_event(ev);
                    ERRORPRINT("ROUTE_RESOLVED not received", attempt);
                    goto BACKOFF;
                }
                rdma_ack_cm_event(ev);
            }

            // 4) PD/CQ/QP
            pd_ = ibv_alloc_pd(id_p_->verbs);
            if (!pd_) { ERRORPRINT("alloc PD failed", attempt); goto BACKOFF; }
            cq_ = ibv_create_cq(id_p_->verbs, 32, nullptr, nullptr, 0);
            if (!cq_) { ERRORPRINT("create CQ failed", attempt); goto BACKOFF; }
            {
                ibv_qp_init_attr qpia{};
                qpia.qp_type = IBV_QPT_RC;
                qpia.send_cq = cq_; qpia.recv_cq = cq_;
                qpia.cap.max_send_wr = 32; qpia.cap.max_recv_wr = 32;
                qpia.cap.max_send_sge = 1;  qpia.cap.max_recv_sge = 1;
                if (rdma_create_qp(id_p_, pd_, &qpia)) {
                    ERRORPRINT("create QP failed", attempt);
                    goto BACKOFF;
                }
            }

            // 5) per-attempt MRs (bound to this attempt's PD)
            mr_ctrl_send_ = ibv_reg_mr(pd_, ctrl_mem_, ctrl_size, IBV_ACCESS_LOCAL_WRITE);
            mr_ctrl_recv_ = ibv_reg_mr(pd_, ctrl_mem_, ctrl_size, IBV_ACCESS_LOCAL_WRITE);
            if (!mr_ctrl_send_ || !mr_ctrl_recv_) {
                ERRORPRINT("register ctrl MRs failed", attempt);
                goto BACKOFF;
            }
            mr_flag_ = ibv_reg_mr(pd_, flag_mem_, 64, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
            if (!mr_flag_) {
                ERRORPRINT("register flag MR failed", attempt);
                goto BACKOFF;
            }

            // 6) connect
            {
                rdma_conn_param cp{}; cp.initiator_depth=1; cp.responder_resources=1; cp.rnr_retry_count=7;
                if (rdma_connect(id_p_, &cp)) {
                    ERRORPRINT("rdma_connect submit failed", attempt);
                    goto BACKOFF;
                }
            }

            // 7) wait for ESTABLISHED (this is the line you referenced)
            {
                rdma_cm_event* ev = nullptr;
                if (rdma_get_cm_event(ec_, &ev)) {
                    WARNINGPRINT("get connect event failed", attempt, debug_);
                    goto BACKOFF;
                }
                if (ev->event != RDMA_CM_EVENT_ESTABLISHED) {
                    WARNINGPRINT("not ESTABLISHED, will retry. event", ev->event, debug_);
                    rdma_ack_cm_event(ev);
                    goto BACKOFF;
                }
                rdma_ack_cm_event(ev);
            }

            // 8) send our flag info
            {
                auto* info = reinterpret_cast<MsgPart2C*>(ctrl_mem_);
                info->flag_addr = (uint64_t)(uintptr_t)flag_mem_;
                info->flag_rkey = mr_flag_->rkey;
                info->pad = 0;

                DBGPRINT("Participant sending flag info addr", info->flag_addr, debug_);
                DBGPRINT("Participant sending flag info rkey", info->flag_rkey, debug_);
                DBGPRINT("MsgPart2C size", sizeof(MsgPart2C), debug_);

                ibv_sge sge{ .addr=(uintptr_t)info, .length=(uint32_t)sizeof(*info), .lkey=mr_ctrl_send_->lkey };
                ibv_send_wr swr{}, *bad=nullptr;
                swr.opcode = IBV_WR_SEND; swr.send_flags = IBV_SEND_SIGNALED;
                swr.sg_list = &sge; swr.num_sge = 1;
                
                DBGPRINT("About to post SEND", 0, debug_);
                // Add small delay to ensure coordinator RECV is posted first
                usleep(100000);  // 100ms delay
                DBGPRINT("Participant posting SEND on QP", id_p_->qp->qp_num, debug_);
                DBGPRINT("Delay complete, posting SEND now", 0, debug_);
                if (ibv_post_send(id_p_->qp, &swr, &bad)) {
                    ERRORPRINT("post SEND flag info failed", attempt);
                    goto BACKOFF;
                }
                DBGPRINT("SEND posted, waiting for completion", 0, debug_);
                
                // Wait for send completion
                bool send_completed = false;
                while (!send_completed) {
                    ibv_wc wc{};
                    int n = ibv_poll_cq(cq_, 1, &wc);
                    if (n > 0) {
                        DBGPRINT("Send completion status", wc.status, debug_);
                        DBGPRINT("Send completion opcode", wc.opcode, debug_);
                        if (wc.status == IBV_WC_SUCCESS) {
                            DBGPRINT("SEND completed successfully", 0, debug_);
                        } else {
                            ERRORPRINT("SEND completion error", wc.status);
                        }
                        send_completed = true;
                    }
                }
            }

            DBGPRINT("Participant setup complete (attempt)", attempt, debug_);
            connected = true;
            break;

    BACKOFF:
            // Tear down attempt-scoped resources (safe if null)
            if (id_p_) {
                if (id_p_->qp) rdma_destroy_qp(id_p_);
                id_p_->qp = nullptr;
            }
            if (mr_flag_)      { ibv_dereg_mr(mr_flag_);      mr_flag_ = nullptr; }
            if (mr_ctrl_send_) { ibv_dereg_mr(mr_ctrl_send_); mr_ctrl_send_ = nullptr; }
            if (mr_ctrl_recv_) { ibv_dereg_mr(mr_ctrl_recv_); mr_ctrl_recv_ = nullptr; }
            if (cq_)           { ibv_destroy_cq(cq_);         cq_ = nullptr; }
            if (pd_)           { ibv_dealloc_pd(pd_);         pd_ = nullptr; }
            if (id_p_)         { rdma_destroy_id(id_p_);      id_p_ = nullptr; }

            // Exponential backoff (10, 20, 40, 80, 160 ms)
            int backoff_ms = std::min(160, 10 << (attempt-1));
            DBGPRINT("Reconnect backoff(ms)", backoff_ms, debug_);
            usleep(backoff_ms * 1000);
        }

        freeaddrinfo(res);

        if (!connected) {
            ERRORPRINT("Participant failed to connect after attempts", kMaxAttempts);
            return false;
        }
        return true;
    }

    // Coordinator initialization
    bool setup_coordinator() {
        DBGPRINT("Setting up coordinator", bind_ip_, debug_);
        ec_ = rdma_create_event_channel();                       
        if (!ec_) return false;
        if (rdma_create_id(ec_, &listen_id_, nullptr, RDMA_PS_TCP)) return false;

        sockaddr_in sin{}; 
        sin.sin_family = AF_INET;
        sin.sin_port = htons((uint16_t)std::atoi(port_.c_str()));
        if (!bind_ip_.empty() && bind_ip_ != "0.0.0.0") {
            inet_pton(AF_INET, bind_ip_.c_str(), &sin.sin_addr);
        } else {
            sin.sin_addr.s_addr = INADDR_ANY;
        }

        if (rdma_bind_addr(listen_id_, (sockaddr*)&sin)) {
            ERRORPRINT("Failed to bind address", bind_ip_);
            return false;
        }
        if (rdma_listen(listen_id_, expected_)) {
            ERRORPRINT("Failed to listen", expected_);
            return false;
        }
        
        DBGPRINT("Coordinator listening on port", port_, debug_);

        // Get coordinator timeout from environment (default 30 seconds)
        const char* timeout_env = std::getenv("RDMASYNC_COORDINATOR_TIMEOUT");
        int coordinator_timeout_ms = timeout_env ? std::atoi(timeout_env) : 5000;

        // Step 1: Accept all connections with timeout
        std::vector<rdma_cm_id*> pending_ids;
        for (int i = 0; i < expected_; ++i) {
            rdma_cm_event* ev = nullptr;
            
            // Use poll to wait for events with timeout
            struct pollfd pfd;
            pfd.fd = ec_->fd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            
            int poll_ret = poll(&pfd, 1, coordinator_timeout_ms);
            if (poll_ret == 0) {
                ERRORPRINT("Timeout waiting for connection", i + 1);
                ERRORPRINT("Expected connections", expected_);
                ERRORPRINT("Received connections", i);
                return false;
            } else if (poll_ret < 0) {
                ERRORPRINT("Poll error waiting for connection", i + 1);
                return false;
            }
            
            if (rdma_get_cm_event(ec_, &ev)) {
                ERRORPRINT("Failed to get CM event", i + 1);
                return false;
            }
            if (ev->event != RDMA_CM_EVENT_CONNECT_REQUEST) { 
                ERRORPRINT("Unexpected event type", ev->event);
                rdma_ack_cm_event(ev); 
                return false; 
            }
            rdma_cm_id* id = ev->id; 
            rdma_ack_cm_event(ev);
            pending_ids.push_back(id);
            
            DBGPRINT("Accepted connection", i + 1, debug_);
        }

        // Step 2: Initialize PD and CQ once
        if (!pending_ids.empty()) {
            pd_ = ibv_alloc_pd(pending_ids[0]->verbs);
            cq_ = ibv_create_cq(pending_ids[0]->verbs, 1024, nullptr, nullptr, 0);
            
            // Reserve a "ready" counter
            void* mem = nullptr; 
            posix_memalign(&mem, 64, 64);
            ready_mem_ = mem; 
            *(uint64_t*)ready_mem_ = 0;
            mr_ready_ = ibv_reg_mr(pd_, ready_mem_, 64, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
        }

        // Step 3: Create QPs and establish connections
        conns_.reserve(expected_);
        for (rdma_cm_id* id : pending_ids) {
            ibv_qp_init_attr qpia{};
            qpia.qp_type = IBV_QPT_RC;
            qpia.send_cq = cq_; qpia.recv_cq = cq_;
            qpia.cap.max_send_wr = 128; qpia.cap.max_recv_wr = 128;
            qpia.cap.max_send_sge = 1;  qpia.cap.max_recv_sge = 1;
            if (rdma_create_qp(id, pd_, &qpia)) return false;

            Conn c; 
            c.id = id;
            c.qp = id->qp;  // Set the QP pointer
            
            // Initialize recv_buffer to zero
            memset(&c.recv_buffer, 0, sizeof(c.recv_buffer));
            
            // Add to vector first, then register MR with the final address
            conns_.push_back(c);
            Conn& final_c = conns_.back();

            // Post RECV for participant's flag info using the final address
            final_c.mr_ctrl_recv = ibv_reg_mr(pd_, &final_c.recv_buffer, sizeof(final_c.recv_buffer), IBV_ACCESS_LOCAL_WRITE);
            
            DBGPRINT("Coordinator RECV buffer addr", (uintptr_t)&final_c.recv_buffer, debug_);
            DBGPRINT("Coordinator RECV buffer size", sizeof(final_c.recv_buffer), debug_);
            
            ibv_sge rsge{ 
                .addr = (uintptr_t)&final_c.recv_buffer, 
                .length = (uint32_t)sizeof(final_c.recv_buffer), 
                .lkey = final_c.mr_ctrl_recv->lkey 
            };
            ibv_recv_wr rwr{}, *rbad = nullptr;
            rwr.sg_list = &rsge; 
            rwr.num_sge = 1;
            
            DBGPRINT("Coordinator posting RECV on QP", id->qp->qp_num, debug_);
            ibv_post_recv(id->qp, &rwr, &rbad);

            rdma_conn_param cp{}; 
            cp.initiator_depth = 1; 
            cp.responder_resources = 1; 
            cp.rnr_retry_count = 7;
            if (rdma_accept(id, &cp)) return false;
        }

        // Step 4: Wait for all ESTABLISHED events with timeout
        for (int i = 0; i < expected_; ++i) {
            rdma_cm_event* ev = nullptr;
            
            // Use poll to wait for ESTABLISHED events with timeout
            struct pollfd pfd;
            pfd.fd = ec_->fd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            
            int poll_ret = poll(&pfd, 1, coordinator_timeout_ms);
            if (poll_ret == 0) {
                ERRORPRINT("Timeout waiting for ESTABLISHED event", i + 1);
                return false;
            } else if (poll_ret < 0) {
                ERRORPRINT("Poll error waiting for ESTABLISHED event", i + 1);
                return false;
            }
            
            if (rdma_get_cm_event(ec_, &ev)) {
                ERRORPRINT("Failed to get ESTABLISHED event", i + 1);
                return false;
            }
            if (ev->event != RDMA_CM_EVENT_ESTABLISHED) { 
                ERRORPRINT("Unexpected event, expected ESTABLISHED", ev->event);
                rdma_ack_cm_event(ev); 
                return false; 
            }
            rdma_ack_cm_event(ev);
            DBGPRINT("Connection established", i + 1, debug_);
        }

        // Step 5: Wait for all participant flag info (RECV completions) with timeout
        for (int i = 0; i < expected_; ++i) {
            if (!poll_cq_one_timeout(cq_, coordinator_timeout_ms)) {
                ERRORPRINT("Timeout waiting for participant flag info", i + 1);
                return false;
            }
            DBGPRINT("Received participant flag info", i + 1, debug_);
            
            // DEBUG: Check recv_buffer content before copying
            DBGPRINT("recv_buffer flag_addr", conns_[i].recv_buffer.flag_addr, debug_);
            DBGPRINT("recv_buffer flag_rkey", conns_[i].recv_buffer.flag_rkey, debug_);
            
            // Copy received data from recv_buffer to part_info
            conns_[i].part_info = conns_[i].recv_buffer;
            
            // DEBUG: Check flag info after copying from recv_buffer  
            DBGPRINT("Copied flag addr", conns_[i].part_info.flag_addr, debug_);
            DBGPRINT("Copied flag rkey", conns_[i].part_info.flag_rkey, debug_);
        }

        // Step 6: Setup control send MRs for all connections  
        for (auto& c : conns_) {
            // DEBUG: Check received flag info before modifying MRs
            DBGPRINT("Coordinator received flag addr", c.part_info.flag_addr, debug_);
            DBGPRINT("Coordinator received flag rkey", c.part_info.flag_rkey, debug_);
            
            // Create separate MR for sending control messages (reuse recv_epoch buffer)
            c.mr_ctrl_send = ibv_reg_mr(pd_, &c.recv_epoch, sizeof(c.recv_epoch), IBV_ACCESS_LOCAL_WRITE);
            if (!c.mr_ctrl_send) {
                ERRORPRINT("Failed to register send MR", 0);
                return false;
            }
            
            // Deregister the old MR that was used for part_info and register new one for recv_epoch
            if (c.mr_ctrl_recv) {
                ibv_dereg_mr(c.mr_ctrl_recv);
            }
            c.mr_ctrl_recv = ibv_reg_mr(pd_, &c.recv_epoch, sizeof(c.recv_epoch), IBV_ACCESS_LOCAL_WRITE);
            if (!c.mr_ctrl_recv) {
                ERRORPRINT("Failed to register recv MR", 0);
                return false;
            }
        }

        // Pre-post RECVs for the first tp_sync round
        // Reset counter since the RECVs posted during setup were consumed for flag info
        posted_recvs_ = 0;  
        ensure_posted_recvs(expected_);
        DBGPRINT("Pre-posted RECVs for tp_sync operations", posted_recvs_, debug_);

        DBGPRINT("Coordinator setup complete with", expected_, debug_);
        return true;
    }

    void ensure_posted_recvs(int need) {
        DBGPRINT("ensure_posted_recvs called with need", need, debug_);
        for (int i=0;i < need; i++) {
            Conn& c = conns_[i % conns_.size()];
            
            // Always post receive for epoch messages using existing recv_epoch buffer
            ibv_sge rsge{ .addr=(uintptr_t)&c.recv_epoch, .length=(uint32_t)sizeof(c.recv_epoch),
                          .lkey=c.mr_ctrl_recv->lkey };
            ibv_recv_wr rwr{}, *rbad=nullptr;
            rwr.sg_list = &rsge; rwr.num_sge = 1;
            int ret = ibv_post_recv(c.qp, &rwr, &rbad);
            if (ret == 0) {
                posted_recvs_++;
                DBGPRINT("Posted RECV for connection", i, debug_);
                DBGPRINT("Total posted_recvs now", posted_recvs_, debug_);
            } else {
                ERRORPRINT("Failed to post RECV", ret);
            }
        }
    }

private:
    // State
    bool enabled_ = false, inited_ = false;
    bool debug_ = false;                     // Debug mode (print extra info)
    bool is_passive_participant_ = false;    // True for non-rank-0 participants in cross-mpirun mode
    std::string role_;
    std::string port_ = "7471";
    int spin_us_ = 200;                         // Reserved (current impl uses busy-wait; no sleep)
    size_t mpi_rank_ = 0;                    // MPI rank within this process

    std::atomic<uint64_t> epoch_{0};

    // participant
    rdma_event_channel* ec_ = nullptr;
    rdma_cm_id* id_p_ = nullptr;
    ibv_pd*  pd_ = nullptr;
    ibv_cq*  cq_ = nullptr;
    ibv_mr*  mr_ctrl_send_ = nullptr;
    ibv_mr*  mr_ctrl_recv_ = nullptr;
    ibv_mr*  mr_flag_ = nullptr;
    void*    ctrl_mem_ = nullptr;
    void*    flag_mem_ = nullptr;
    std::string server_ip_;

    // coordinator
    rdma_cm_id* listen_id_ = nullptr;
    std::vector<Conn> conns_;
    ibv_pd*  pd_c_ = nullptr; // Unused; reuse pd_
    ibv_cq*  cq_c_ = nullptr; // Unused; reuse cq_
    void*    ready_mem_ = nullptr;
    ibv_mr*  mr_ready_ = nullptr;
    ibv_pd*  pd__ = nullptr;  // Placeholder to avoid confusion
    int expected_ = 0;
    int posted_recvs_ = 0;
    std::string bind_ip_;
    uint64_t dummy_flag_ = 0;  // Dummy flag for debugging
    // Reuse pd_/cq_ as coordinator resource handles
};
} // anonymous namespace


// --------- Public API implementation ---------
namespace rdmasync {
void init(uint32_t mpi_rank) { RDMASyncImpl::inst().init(mpi_rank); }
void finalize()       { RDMASyncImpl::inst().finalize(); }
void tp_sync(const char* name) { 
    // Auto-initialize with rank 0 if not already initialized
    RDMASyncImpl::inst().tp_sync(name); 
}

TRIGGER_MSG tp_trigger(TRIGGER_MSG send_msg) {
    return RDMASyncImpl::inst().tp_trigger(send_msg);
}

} // namespace rdmasync

extern "C" {
void rdmasync_init(uint32_t mpi_rank) { ::rdmasync::init(mpi_rank); }
void rdmasync_finalize()       { ::rdmasync::finalize(); }
void rdmasync_hook(const char* name) { 
    // Auto-initialize with rank 0 if not already initialized
    ::rdmasync::tp_sync(name); 
}
int rdmasync_trigger(int send_msg) {
    return static_cast<int>(::rdmasync::tp_trigger(static_cast<::rdmasync::TRIGGER_MSG>(send_msg)));
}
}
