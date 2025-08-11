#include "sync.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace {

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

        if (strcmp(std::getenv("RDMASYNC_ROLE"), "AUTO") == 0) {
            if (mpi_rank != 0) {
                setenv("RDMASYNC_ROLE", "participant", 1);
            } else {
                setenv("RDMASYNC_ROLE", "coordinator", 1);
            }
        }
        const char* role = std::getenv("RDMASYNC_ROLE");
        if (!role) { enabled_ = false; inited_ = true; return; }
        role_ = role;

        const char* port = std::getenv("RDMASYNC_PORT");
        port_ = port ? port : "7471";

        const char* spin = std::getenv("RDMASYNC_SPIN_US");
        spin_us_ = spin ? std::max(0, std::atoi(spin)) : 200;

        if (role_ == "coordinator") {
            const char* bind = std::getenv("RDMASYNC_BIND_IP");
            bind_ip_ = bind ? bind : "0.0.0.0";
            const char* exp  = std::getenv("RDMASYNC_EXPECTED");
            expected_ = exp ? std::atoi(exp) : 0;
            if (expected_ <= 0) { enabled_ = false; inited_ = true; return; }
            setup_coordinator();
        } else if (role_ == "participant") {
            const char* server = std::getenv("RDMASYNC_SERVER_IP");
            if (!server) { enabled_ = false; inited_ = true; return; }
            server_ip_ = server;
            setup_participant();
        } else {
            enabled_ = false;
        }
        inited_ = true;
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
    void setup_participant() {
        ec_ = rdma_create_event_channel();                  if (!ec_) return;
        if (rdma_create_id(ec_, &id_p_, nullptr, RDMA_PS_TCP)) return;

        // Resolve address and connect
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(server_ip_.c_str(), port_.c_str(), &hints, &res)) return;
        if (rdma_resolve_addr(id_p_, nullptr, res->ai_addr, 2000)) { freeaddrinfo(res); return; }
        freeaddrinfo(res);

        rdma_cm_event* ev=nullptr;
        if (rdma_get_cm_event(ec_, &ev)) return;
        if (ev->event != RDMA_CM_EVENT_ADDR_RESOLVED) { rdma_ack_cm_event(ev); return; }
        rdma_ack_cm_event(ev);

        if (rdma_resolve_route(id_p_, 2000)) return;
        if (rdma_get_cm_event(ec_, &ev)) return;
        if (ev->event != RDMA_CM_EVENT_ROUTE_RESOLVED) { rdma_ack_cm_event(ev); return; }
        rdma_ack_cm_event(ev);

        pd_ = ibv_alloc_pd(id_p_->verbs);                   if (!pd_) return;
        cq_ = ibv_create_cq(id_p_->verbs, 32, nullptr, nullptr, 0); if (!cq_) return;

        ibv_qp_init_attr qpia{};
        qpia.qp_type = IBV_QPT_RC;
        qpia.send_cq = cq_; qpia.recv_cq = cq_;
        qpia.cap.max_send_wr = 32; qpia.cap.max_recv_wr = 32;
        qpia.cap.max_send_sge = 1;  qpia.cap.max_recv_sge = 1;
        if (rdma_create_qp(id_p_, pd_, &qpia)) return;

        // Control-buffer MR (only to obtain an lkey for SEND WQEs)
        MsgPart2C dummy{};
        mr_ctrl_send_ = ibv_reg_mr(pd_, &dummy, sizeof(dummy), IBV_ACCESS_LOCAL_WRITE);
        mr_ctrl_recv_ = ibv_reg_mr(pd_, &dummy, sizeof(dummy), IBV_ACCESS_LOCAL_WRITE);

        // Flag buffer (64B-aligned)
        void* mem=nullptr; posix_memalign(&mem, 64, 64);
        flag_mem_ = mem; *(uint64_t*)flag_mem_ = 0;
        mr_flag_ = ibv_reg_mr(pd_, flag_mem_, 64, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

        // Establish connection
        rdma_conn_param cp{}; cp.initiator_depth=1; cp.responder_resources=1; cp.rnr_retry_count=7;
        if (rdma_connect(id_p_, &cp)) return;
        if (rdma_get_cm_event(ec_, &ev)) return;
        if (ev->event != RDMA_CM_EVENT_ESTABLISHED) { rdma_ack_cm_event(ev); return; }
        rdma_ack_cm_event(ev);

        // Send own flag {addr, rkey} to the coordinator
        MsgPart2C info{ (uint64_t)(uintptr_t)flag_mem_, mr_flag_->rkey, 0 };
        ibv_sge sge{ .addr=(uintptr_t)&info, .length=(uint32_t)sizeof(info), .lkey=mr_ctrl_send_->lkey };
        ibv_send_wr swr{}, *bad=nullptr;
        swr.opcode = IBV_WR_SEND; swr.send_flags = IBV_SEND_SIGNALED;
        swr.sg_list = &sge; swr.num_sge = 1;
        ibv_post_send(id_p_->qp, &swr, &bad);
        poll_cq_one(cq_);
    }

    // Coordinator initialization
    void setup_coordinator() {
        ec_ = rdma_create_event_channel();                       if (!ec_) return;
        if (rdma_create_id(ec_, &listen_id_, nullptr, RDMA_PS_TCP)) return;

        sockaddr_in sin{}; sin.sin_family = AF_INET;
        sin.sin_port = htons((uint16_t)std::atoi(port_.c_str()));
        if (!bind_ip_.empty()) inet_pton(AF_INET, bind_ip_.c_str(), &sin.sin_addr);
        else sin.sin_addr.s_addr = INADDR_ANY;

        if (rdma_bind_addr(listen_id_, (sockaddr*)&sin)) return;
        if (rdma_listen(listen_id_, expected_)) return;

        // Accept `expected_` connections
        conns_.reserve(expected_);
        for (int i=0;i<expected_;++i) {
            rdma_cm_event* ev=nullptr;
            if (rdma_get_cm_event(ec_, &ev)) return;
            if (ev->event != RDMA_CM_EVENT_CONNECT_REQUEST) { rdma_ack_cm_event(ev); return; }
            rdma_cm_id* id = ev->id; rdma_ack_cm_event(ev);

            if (!pd_) {
                pd_ = ibv_alloc_pd(id->verbs);
                cq_ = ibv_create_cq(id->verbs, 1024, nullptr, nullptr, 0);
                // Reserve a "ready" counter (unused in current impl; future extension)
                void* mem=nullptr; posix_memalign(&mem, 64, 64);
                ready_mem_ = mem; *(uint64_t*)ready_mem_ = 0;
                mr_ready_ = ibv_reg_mr(pd_, ready_mem_, 64, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
            }

            ibv_qp_init_attr qpia{};
            qpia.qp_type = IBV_QPT_RC;
            qpia.send_cq = cq_; qpia.recv_cq = cq_;
            qpia.cap.max_send_wr = 128; qpia.cap.max_recv_wr = 128;
            qpia.cap.max_send_sge = 1;  qpia.cap.max_recv_sge = 1;
            if (rdma_create_qp(id, pd_, &qpia)) return;

            Conn c; c.id = id;

            // First, post a RECV for the participant's flag info
            c.mr_ctrl_recv = ibv_reg_mr(pd_, &c.part_info, sizeof(c.part_info), IBV_ACCESS_LOCAL_WRITE);
            ibv_sge rsge{ .addr=(uintptr_t)&c.part_info, .length=(uint32_t)sizeof(c.part_info), .lkey=c.mr_ctrl_recv->lkey };
            ibv_recv_wr rwr{}, *rbad=nullptr;
            rwr.sg_list = &rsge; rwr.num_sge = 1;
            ibv_post_recv(id->qp, &rwr, &rbad);

            rdma_conn_param cp{}; cp.initiator_depth=1; cp.responder_resources=1; cp.rnr_retry_count=7;
            if (rdma_accept(id, &cp)) return;

            // Wait for ESTABLISHED
            rdma_cm_event* ev2=nullptr;
            if (rdma_get_cm_event(ec_, &ev2)) return;
            if (ev2->event != RDMA_CM_EVENT_ESTABLISHED) { rdma_ack_cm_event(ev2); return; }
            rdma_ack_cm_event(ev2);

            // Wait for the RECV completion (flag info is now available)
            poll_cq_one(cq_);

            // Spare control-send MR
            c.mr_ctrl_send = ibv_reg_mr(pd_, &c.part_info, sizeof(c.part_info), IBV_ACCESS_LOCAL_WRITE);
            conns_.push_back(c);
        }
        // Pre-post RECVs for the first hook round
        ensure_posted_recvs(expected_);
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
void rdmasync_init_if_needed() { rdmasync::init_if_needed(); }
void rdmasync_finalize()       { rdmasync::finalize(); }
void rdmasync_hook(const char* name) { rdmasync::hook(name); }
}
