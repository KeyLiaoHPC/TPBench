#include "sync.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace {

enum class SYNC_ERROR {
    OK = 0,    
};


#define DBGPRINT(info, var, debug) \
    if (debug) { \
        std::cout << "[TPSYNC] " << info << ": " << var << std::endl; \
    } \

#define ERRORPRINT(info, var, debug) \
    if (debug) { \
        std::cerr << "[TPSYNC ERROR] " << info << ": " << var << std::endl; \
    } \

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

#pragma pack(push,1)
struct MsgPart2C {          // participant -> coordinator: expose local flag {addr, rkey}
    uint64_t flag_addr;
    uint32_t flag_rkey;
    uint32_t pad;
};
struct MsgEpoch {           // participant -> coordinator: send current epoch
    uint64_t epoch;
};
#pragma pack(pop)

struct Conn {
    rdma_cm_id* id = nullptr;
    ibv_mr* mr_ctrl_send = nullptr;
    ibv_mr* mr_ctrl_recv = nullptr;
    MsgPart2C part_info{};     // participant's flag info received from RECV
    MsgEpoch  recv_epoch{};    // participant's epoch received via RECV
    ibv_qp* qp() const { return id ? id->qp : nullptr; }
};

class RDMASyncImpl {
public:
    static RDMASyncImpl& inst() {
        static RDMASyncImpl s;
        return s;
    }

    // Public entry points
    void init_if_needed(size_t mpi_rank = 0) {
        if (inited_) return;
        const char* en = std::getenv("RDMASYNC_ENABLE");
        if (!en || std::strcmp(en, "1") != 0) { enabled_ = false; inited_ = true; return; }

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

        const char* port = std::getenv("RDMASYNC_PORT");
        port_ = port ? port : "2673";

        const char* spin = std::getenv("RDMASYNC_SPIN_US");
        spin_us_ = spin ? std::max(0, std::atoi(spin)) : 200;

        if (role_ == "coordinator") {
            // const char* bind = std::getenv("RDMASYNC_BIND_IP");
            bind_ip_ = "0.0.0.0";
            const char* exp  = std::getenv("RDMASYNC_EXPECTED");
            expected_ = exp ? std::atoi(exp) : 0;
            if (expected_ <= 0) { enabled_ = false; inited_ = true; return; }
            if ( setup_coordinator() == false ) {
                enabled_ = false;
                inited_ = true;
                ERRORPRINT("Coordinator setup failed", bind_ip_, debug_);
                return;
            }
        } else if (role_ == "participant") {
            const char* server = std::getenv("RDMASYNC_SERVER_IP");
            if (!server) { 
                if (role_env && std::strcmp(role_env, "AUTO") == 0) {
                    ERRORPRINT("AUTO mode requires RDMASYNC_SERVER_IP to be set to coordinator's IP (not 127.0.0.1)", "missing", debug_);
                } else {
                    ERRORPRINT("RDMASYNC_SERVER_IP not set", "missing", debug_);
                }
                enabled_ = false; 
                inited_ = true; 
                return; 
            }
            server_ip_ = server;
            if (setup_participant() == false) {
                enabled_ = false;
                inited_ = true;
                ERRORPRINT("Participant setup failed", server_ip_, debug_);
                return;
            }
        } else {
            enabled_ = false;
        }
        inited_ = true;

        DBGPRINT("RDMASync initialized: ", role_, debug_);
    }

    void finalize() {
        if (!enabled_) return;
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

    void hook(const char* /*name*/) {
        if (!enabled_) return;
        uint64_t e = epoch_.fetch_add(1, std::memory_order_relaxed) + 1;

        if (role_ == "participant") {
            // Send the current epoch to the coordinator
            MsgEpoch msg{ e };
            ibv_sge sge{ .addr=(uintptr_t)&msg, .length=(uint32_t)sizeof(msg), .lkey=mr_ctrl_send_->lkey };
            ibv_send_wr wr{}, *bad=nullptr;
            wr.opcode = IBV_WR_SEND;
            wr.send_flags = IBV_SEND_SIGNALED;
            wr.sg_list = &sge; wr.num_sge = 1;
            if (ibv_post_send(id_p_->qp, &wr, &bad) == 0) poll_cq_one(cq_);

            // Wait for the coordinator to write back GO (flag == epoch)
            volatile uint64_t* flag = (volatile uint64_t*)flag_mem_;
            while (__builtin_expect(*flag != e, 1)) cpu_relax();
            return;
        }

        // Coordinator: wait for `expected_` RECV completions, then broadcast GO
        ensure_posted_recvs(expected_ - posted_recvs_);
        int got = 0;
        while (got < expected_) {
            ibv_wc wc{};
            int n = ibv_poll_cq(cq_, 1, &wc);
            if (n <= 0) continue;
            if (wc.status != IBV_WC_SUCCESS) continue;
            if (wc.opcode == IBV_WC_RECV) got++;
        }
        posted_recvs_ -= expected_;

        // Write the epoch to each participant's flag (RDMA WRITE, inline)
        for (auto& c : conns_) {
            uint64_t val = e;
            ibv_sge sge{ .addr=(uintptr_t)&val, .length=(uint32_t)sizeof(val), .lkey=0 };
            ibv_send_wr wr{}, *bad=nullptr;
            wr.opcode = IBV_WR_RDMA_WRITE;
            wr.send_flags = IBV_SEND_INLINE | IBV_SEND_SIGNALED;
            wr.wr.rdma.remote_addr = c.part_info.flag_addr;
            wr.wr.rdma.rkey = c.part_info.flag_rkey;
            wr.sg_list = &sge; wr.num_sge = 1;
            ibv_post_send(c.qp(), &wr, &bad);
        }
        int done = 0;
        while (done < expected_) {
            ibv_wc wc{};
            int n = ibv_poll_cq(cq_, 1, &wc);
            if (n <= 0) continue;
            if (wc.status != IBV_WC_SUCCESS) continue;
            if (wc.opcode == IBV_WC_RDMA_WRITE) done++;
        }
    }

private:
    // Participant initialization
    bool setup_participant() {
        // In AUTO mode, add small delay to let coordinator start first
        const char* role_env = std::getenv("RDMASYNC_ROLE");
        if (role_env && std::strcmp(role_env, "AUTO") == 0) {
            usleep(200000); // 200ms delay
        }
        
        DBGPRINT("Setting up participant", server_ip_, debug_);
        ec_ = rdma_create_event_channel();                  
        if (!ec_) return false;
        if (rdma_create_id(ec_, &id_p_, nullptr, RDMA_PS_TCP)){
            ERRORPRINT("Failed to create RDMA ID", server_ip_, debug_);
            return false;
        }

        // Resolve address and connect (with retry for AUTO mode)
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(server_ip_.c_str(), port_.c_str(), &hints, &res)) {
            ERRORPRINT("Failed to resolve address", server_ip_, debug_);
            return false;
        }
        
        int resolve_retries = 5;
        while (resolve_retries > 0) {
            if (rdma_resolve_addr(id_p_, nullptr, res->ai_addr, 2000) == 0) {
                break;
            }
            DBGPRINT("Address resolve retry", resolve_retries, debug_);
            usleep(100000); // 100ms
            resolve_retries--;
        }
        if (resolve_retries == 0) {
            ERRORPRINT("Failed to resolve RDMA address after retries", server_ip_, debug_);
            freeaddrinfo(res);
            return false;
        }
        freeaddrinfo(res);

        rdma_cm_event* ev=nullptr;
        if (rdma_get_cm_event(ec_, &ev)) return false;
        if (ev->event != RDMA_CM_EVENT_ADDR_RESOLVED) { rdma_ack_cm_event(ev); return false; }
        rdma_ack_cm_event(ev);

        if (rdma_resolve_route(id_p_, 2000)) return false;
        if (rdma_get_cm_event(ec_, &ev)) return false;
        if (ev->event != RDMA_CM_EVENT_ROUTE_RESOLVED) { rdma_ack_cm_event(ev); return false; }
        rdma_ack_cm_event(ev);

        pd_ = ibv_alloc_pd(id_p_->verbs);                   
        if (!pd_) {
            ERRORPRINT("Failed to allocate PD", "", debug_);
            return false;
        }
        cq_ = ibv_create_cq(id_p_->verbs, 32, nullptr, nullptr, 0); 
        if (!cq_) {
            ERRORPRINT("Failed to create CQ", "", debug_);
            return false;
        }

        ibv_qp_init_attr qpia{};
        qpia.qp_type = IBV_QPT_RC;
        qpia.send_cq = cq_; 
        qpia.recv_cq = cq_;
        qpia.cap.max_send_wr = 32; 
        qpia.cap.max_recv_wr = 32;
        qpia.cap.max_send_sge = 1;  
        qpia.cap.max_recv_sge = 1;
        if (rdma_create_qp(id_p_, pd_, &qpia)) {
            ERRORPRINT("Failed to create QP", "", debug_);
            return false;
        }

        // Control-buffer MR (allocate memory for control messages)
        void* ctrl_mem = nullptr; 
        posix_memalign(&ctrl_mem, 64, sizeof(MsgPart2C));
        ctrl_mem_ = ctrl_mem;
        mr_ctrl_send_ = ibv_reg_mr(pd_, ctrl_mem_, sizeof(MsgPart2C), IBV_ACCESS_LOCAL_WRITE);
        if (!mr_ctrl_send_) {
            ERRORPRINT("Failed to register control send MR", "", debug_);
            return false;
        }
        
        mr_ctrl_recv_ = ibv_reg_mr(pd_, ctrl_mem_, sizeof(MsgPart2C), IBV_ACCESS_LOCAL_WRITE);
        if (!mr_ctrl_recv_) {
            ERRORPRINT("Failed to register control recv MR", "", debug_);
            return false;
        }

        // Flag buffer (64B-aligned)
        void* mem=nullptr; 
        if (posix_memalign(&mem, 64, 64) != 0) {
            ERRORPRINT("Failed to allocate flag memory", "", debug_);
            return false;
        }
        flag_mem_ = mem; 
        *(uint64_t*)flag_mem_ = 0;
        mr_flag_ = ibv_reg_mr(pd_, flag_mem_, 64, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        if (!mr_flag_) {
            ERRORPRINT("Failed to register flag MR", "", debug_);
            return false;
        }

        // Establish connection (with retry)
        rdma_conn_param cp{}; cp.initiator_depth=1; cp.responder_resources=1; cp.rnr_retry_count=7;
        
        int connect_retries = 5;
        while (connect_retries > 0) {
            if (rdma_connect(id_p_, &cp) == 0) {
                break;
            }
            DBGPRINT("Connection retry", connect_retries, debug_);
            usleep(10000); // 10ms
            connect_retries--;
        }
        if (connect_retries == 0) {
            ERRORPRINT("Failed to connect after retries", "", debug_);
            return false;
        }
        
        connect_retries = 5;
        while (connect_retries > 0) {
            if (rdma_get_cm_event(ec_, &ev) == 0) {
                if (ev->event == RDMA_CM_EVENT_ESTABLISHED) {
                    break;
                } else if (connect_retries == 1) {
                    ERRORPRINT("Failed to establish connection", ev->event, debug_);
                    rdma_ack_cm_event(ev);
                    return false;
                }
                rdma_ack_cm_event(ev);
            } else {
                ERRORPRINT("Failed to get connect event", "", debug_);
                return false;   
            }
            DBGPRINT("Connection event retry", 5 - connect_retries + 1, debug_);
            usleep(50000); // 50ms
            connect_retries--;
        }

        rdma_ack_cm_event(ev);

        // Send own flag {addr, rkey} to the coordinator
        MsgPart2C* info = (MsgPart2C*)ctrl_mem_;
        info->flag_addr = (uint64_t)(uintptr_t)flag_mem_;
        info->flag_rkey = mr_flag_->rkey;
        info->pad = 0;
        
        ibv_sge sge{ .addr=(uintptr_t)info, .length=(uint32_t)sizeof(*info), .lkey=mr_ctrl_send_->lkey };
        ibv_send_wr swr{}, *bad=nullptr;
        swr.opcode = IBV_WR_SEND; swr.send_flags = IBV_SEND_SIGNALED;
        swr.sg_list = &sge; swr.num_sge = 1;
        ibv_post_send(id_p_->qp, &swr, &bad);
        poll_cq_one(cq_);
        DBGPRINT("Participant setup complete", server_ip_, debug_);
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
        if (!bind_ip_.empty()) inet_pton(AF_INET, bind_ip_.c_str(), &sin.sin_addr);
        else sin.sin_addr.s_addr = INADDR_ANY;

        if (rdma_bind_addr(listen_id_, (sockaddr*)&sin)) return false;
        if (rdma_listen(listen_id_, expected_)) return false;

        // Step 1: Accept all connections first
        std::vector<rdma_cm_id*> pending_ids;
        for (int i = 0; i < expected_; ++i) {
            rdma_cm_event* ev = nullptr;
            if (rdma_get_cm_event(ec_, &ev)) return false;
            if (ev->event != RDMA_CM_EVENT_CONNECT_REQUEST) { 
                rdma_ack_cm_event(ev); 
                return false; 
            }
            rdma_cm_id* id = ev->id; 
            rdma_ack_cm_event(ev);
            pending_ids.push_back(id);
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

            // Post RECV for participant's flag info
            c.mr_ctrl_recv = ibv_reg_mr(pd_, &c.part_info, sizeof(c.part_info), IBV_ACCESS_LOCAL_WRITE);
            ibv_sge rsge{ 
                .addr = (uintptr_t)&c.part_info, 
                .length = (uint32_t)sizeof(c.part_info), 
                .lkey = c.mr_ctrl_recv->lkey 
            };
            ibv_recv_wr rwr{}, *rbad = nullptr;
            rwr.sg_list = &rsge; 
            rwr.num_sge = 1;
            ibv_post_recv(id->qp, &rwr, &rbad);

            rdma_conn_param cp{}; 
            cp.initiator_depth = 1; 
            cp.responder_resources = 1; 
            cp.rnr_retry_count = 7;
            if (rdma_accept(id, &cp)) return false;

            conns_.push_back(c);
        }

        // Step 4: Wait for all ESTABLISHED events
        for (int i = 0; i < expected_; ++i) {
            rdma_cm_event* ev = nullptr;
            if (rdma_get_cm_event(ec_, &ev)) return false;
            if (ev->event != RDMA_CM_EVENT_ESTABLISHED) { 
                rdma_ack_cm_event(ev); 
                return false; 
            }
            rdma_ack_cm_event(ev);
        }

        // Step 5: Wait for all participant flag info (RECV completions)
        for (int i = 0; i < expected_; ++i) {
            poll_cq_one(cq_);
        }

        // Step 6: Setup control send MRs for all connections
        for (auto& c : conns_) {
            c.mr_ctrl_send = ibv_reg_mr(pd_, &c.part_info, sizeof(c.part_info), IBV_ACCESS_LOCAL_WRITE);
        }

        // Pre-post RECVs for the first hook round
        ensure_posted_recvs(expected_);

        DBGPRINT("Coordinator setup complete with", expected_, debug_);
        return true;
    }

    void ensure_posted_recvs(int need) {
        for (int i=0;i < need; i++) {
            Conn& c = conns_[i % conns_.size()];
            if (!c.mr_ctrl_recv) {
                c.mr_ctrl_recv = ibv_reg_mr(pd_, &c.recv_epoch, sizeof(c.recv_epoch),
                                            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
            }
            ibv_sge rsge{ .addr=(uintptr_t)&c.recv_epoch, .length=(uint32_t)sizeof(c.recv_epoch),
                          .lkey=c.mr_ctrl_recv->lkey };
            ibv_recv_wr rwr{}, *rbad=nullptr;
            rwr.sg_list = &rsge; rwr.num_sge = 1;
            ibv_post_recv(c.qp(), &rwr, &rbad);
            posted_recvs_++;
        }
    }

private:
    // State
    bool enabled_ = false, inited_ = false;
    bool debug_ = false;                     // Debug mode (print extra info)
    std::string role_;
    std::string port_ = "7471";
    int spin_us_ = 200;                         // Reserved (current impl uses busy-wait; no sleep)

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
    // Reuse pd_/cq_ as coordinator resource handles
};
} // anonymous namespace


// --------- Public API implementation ---------
namespace rdmasync {
void init_if_needed(uint32_t mpi_rank) { RDMASyncImpl::inst().init_if_needed(mpi_rank); }
void finalize()       { RDMASyncImpl::inst().finalize(); }
void hook(const char* name) { (void)name; RDMASyncImpl::inst().hook(name); }
} // namespace rdmasync

extern "C" {
void rdmasync_init_if_needed(uint32_t mpi_rank) { rdmasync::init_if_needed(mpi_rank); }
void rdmasync_finalize()       { rdmasync::finalize(); }
void rdmasync_hook(const char* name) { rdmasync::hook(name); }
}
