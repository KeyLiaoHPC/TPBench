#include "bench.h"
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

class RDMABench : public BenchmarkBase {
private:
    struct rdma_cm_id* listen_id_;
    struct rdma_cm_id* conn_id_;
    struct rdma_event_channel* ec_;
    struct ibv_context* context_;
    struct ibv_pd* pd_;
    struct ibv_cq* send_cq_;
    struct ibv_cq* recv_cq_;
    struct ibv_mr* send_mr_;
    struct ibv_mr* recv_mr_;
    struct ibv_qp* qp_;
    
    void* send_buffer_;
    void* recv_buffer_;
    size_t buffer_size_;
    
    bool is_server_;
    std::string server_ip_;
    int port_;
    
    // Remote memory info
    uint64_t remote_addr_;
    uint32_t remote_rkey_;
    size_t data_size;
    
public:
    RDMABench() : listen_id_(nullptr), conn_id_(nullptr), ec_(nullptr),
                  context_(nullptr), pd_(nullptr), send_cq_(nullptr), recv_cq_(nullptr),
                  send_mr_(nullptr), recv_mr_(nullptr), qp_(nullptr),
                  send_buffer_(nullptr), recv_buffer_(nullptr), buffer_size_(0),
                  is_server_(false), port_(12345), remote_addr_(0), remote_rkey_(0) {}

    void initialize(int argc, char** argv) override {
        // Parse arguments for server/client mode
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--server") == 0) {
                is_server_ = true;
            } else if (strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
                is_server_ = false;
                server_ip_ = argv[i + 1];
                i++;
            } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
                port_ = std::atoi(argv[i + 1]);
                i++;
            }
        }
        
        // If no mode specified, try to determine from environment or default to client
        if (server_ip_.empty() && !is_server_) {
            const char* server_env = getenv("RDMA_SERVER_IP");
            if (server_env) {
                server_ip_ = server_env;
                is_server_ = false;
            } else {
                is_server_ = true; // Default to server if no IP specified
            }
        }
        
        std::cout << "[RDMA] Initializing as " << (is_server_ ? "server" : "client");
        if (!is_server_) {
            std::cout << " connecting to " << server_ip_;
        }
        std::cout << " on port " << port_ << std::endl;
        
        // Create event channel
        ec_ = rdma_create_event_channel();
        if (!ec_) {
            throw std::runtime_error("Failed to create RDMA event channel");
        }
        
        if (is_server_) {
            setup_server();
        } else {
            setup_client();
        }
    }

    void setup_buffers(size_t buffer_size) override {
        buffer_size_ = buffer_size;
        data_size = buffer_size;
        
        // Allocate aligned buffers
        if (posix_memalign(&send_buffer_, 4096, buffer_size_) != 0) {
            throw std::runtime_error("Failed to allocate send buffer");
        }
        if (posix_memalign(&recv_buffer_, 4096, buffer_size_) != 0) {
            throw std::runtime_error("Failed to allocate recv buffer");
        }
        
        // Initialize send buffer with pattern
        char* send_ptr = (char*)send_buffer_;
        for (size_t i = 0; i < buffer_size_; ++i) {
            send_ptr[i] = (char)(i % 256);
        }
        memset(recv_buffer_, 0, buffer_size_);
        
        // Register memory regions
        send_mr_ = ibv_reg_mr(pd_, send_buffer_, buffer_size_,
                             IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
        if (!send_mr_) {
            throw std::runtime_error("Failed to register send MR");
        }
        
        recv_mr_ = ibv_reg_mr(pd_, recv_buffer_, buffer_size_,
                             IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
        if (!recv_mr_) {
            throw std::runtime_error("Failed to register recv MR");
        }
        
        std::cout << "[RDMA] Buffers allocated and registered (size: " << buffer_size_ << ")" << std::endl;
    }

    void run_warmup(const std::string& func_name, size_t data_size) override {
        if (func_name == "write") {
            warmup_rdma_write();
        } else if (func_name == "read") {
            warmup_rdma_read();
        } else if (func_name == "send") {
            warmup_rdma_send();
        } else {
            std::cerr << "[RDMA] Unknown function: " << func_name << std::endl;
            std::cerr << "[RDMA] Available functions: write, read, send" << std::endl;
        }
    }

    void run_benchmark(const std::string& func_name) override {
        if (data_size > buffer_size_) {
            std::cerr << "[RDMA] Data size " << data_size << " exceeds buffer size " << buffer_size_ << std::endl;
            return;
        }
        
        if (func_name == "write") {
            run_rdma_write();
        } else if (func_name == "read") {
            run_rdma_read();
        } else if (func_name == "send") {
            run_rdma_send();
        } else {
            std::cerr << "[RDMA] Unknown function: " << func_name << std::endl;
            std::cerr << "[RDMA] Available functions: write, read, send" << std::endl;
        }
    }

    void finalize() override {
        if (send_mr_) ibv_dereg_mr(send_mr_);
        if (recv_mr_) ibv_dereg_mr(recv_mr_);
        if (send_buffer_) free(send_buffer_);
        if (recv_buffer_) free(recv_buffer_);
        if (qp_) ibv_destroy_qp(qp_);
        if (send_cq_) ibv_destroy_cq(send_cq_);
        if (recv_cq_) ibv_destroy_cq(recv_cq_);
        if (pd_) ibv_dealloc_pd(pd_);
        if (conn_id_) {
            rdma_disconnect(conn_id_);
            rdma_destroy_id(conn_id_);
        }
        if (listen_id_) rdma_destroy_id(listen_id_);
        if (ec_) rdma_destroy_event_channel(ec_);
        
        std::cout << "[RDMA] Cleanup completed" << std::endl;
    }

private:
    void setup_server() {
        // Create listening ID
        if (rdma_create_id(ec_, &listen_id_, nullptr, RDMA_PS_TCP) != 0) {
            throw std::runtime_error("Failed to create listening ID");
        }
        
        // Bind to address
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);
        
        if (rdma_bind_addr(listen_id_, (struct sockaddr*)&addr) != 0) {
            throw std::runtime_error("Failed to bind address");
        }
        
        // Listen for connections
        if (rdma_listen(listen_id_, 1) != 0) {
            throw std::runtime_error("Failed to listen");
        }
        
        std::cout << "[RDMA] Server listening on port " << port_ << std::endl;
        
        // Wait for connection request
        struct rdma_cm_event* event;
        if (rdma_get_cm_event(ec_, &event) != 0) {
            throw std::runtime_error("Failed to get CM event");
        }
        
        if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
            rdma_ack_cm_event(event);
            throw std::runtime_error("Unexpected event type");
        }
        
        conn_id_ = event->id;
        rdma_ack_cm_event(event);
        
        setup_connection();
        accept_connection();
    }
    
    void setup_client() {
        // Create connection ID
        if (rdma_create_id(ec_, &conn_id_, nullptr, RDMA_PS_TCP) != 0) {
            throw std::runtime_error("Failed to create connection ID");
        }
        
        // Resolve address
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(server_ip_.c_str(), std::to_string(port_).c_str(), &hints, &res) != 0) {
            throw std::runtime_error("Failed to resolve server address");
        }
        
        if (rdma_resolve_addr(conn_id_, nullptr, res->ai_addr, 2000) != 0) {
            freeaddrinfo(res);
            throw std::runtime_error("Failed to resolve address");
        }
        freeaddrinfo(res);
        
        // Wait for address resolution
        struct rdma_cm_event* event;
        if (rdma_get_cm_event(ec_, &event) != 0) {
            throw std::runtime_error("Failed to get address resolution event");
        }
        if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
            rdma_ack_cm_event(event);
            throw std::runtime_error("Address resolution failed");
        }
        rdma_ack_cm_event(event);
        
        // Resolve route
        if (rdma_resolve_route(conn_id_, 2000) != 0) {
            throw std::runtime_error("Failed to resolve route");
        }
        
        // Wait for route resolution
        if (rdma_get_cm_event(ec_, &event) != 0) {
            throw std::runtime_error("Failed to get route resolution event");
        }
        if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
            rdma_ack_cm_event(event);
            throw std::runtime_error("Route resolution failed");
        }
        rdma_ack_cm_event(event);
        
        setup_connection();
        connect_to_server();
    }
    
    void setup_connection() {
        context_ = conn_id_->verbs;
        
        // Allocate protection domain
        pd_ = ibv_alloc_pd(context_);
        if (!pd_) {
            throw std::runtime_error("Failed to allocate PD");
        }
        
        // Create completion queues
        send_cq_ = ibv_create_cq(context_, 16, nullptr, nullptr, 0);
        recv_cq_ = ibv_create_cq(context_, 16, nullptr, nullptr, 0);
        if (!send_cq_ || !recv_cq_) {
            throw std::runtime_error("Failed to create CQs");
        }
        
        // Create queue pair
        struct ibv_qp_init_attr qp_attr;
        memset(&qp_attr, 0, sizeof(qp_attr));
        qp_attr.send_cq = send_cq_;
        qp_attr.recv_cq = recv_cq_;
        qp_attr.qp_type = IBV_QPT_RC;
        qp_attr.cap.max_send_wr = 16;
        qp_attr.cap.max_recv_wr = 16;
        qp_attr.cap.max_send_sge = 1;
        qp_attr.cap.max_recv_sge = 1;
        
        if (rdma_create_qp(conn_id_, pd_, &qp_attr) != 0) {
            throw std::runtime_error("Failed to create QP");
        }
        qp_ = conn_id_->qp;
    }
    
    void accept_connection() {
        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof(conn_param));
        conn_param.responder_resources = 1;
        conn_param.initiator_depth = 1;
        
        if (rdma_accept(conn_id_, &conn_param) != 0) {
            throw std::runtime_error("Failed to accept connection");
        }
        
        // Wait for established event
        struct rdma_cm_event* event;
        if (rdma_get_cm_event(ec_, &event) != 0) {
            throw std::runtime_error("Failed to get established event");
        }
        if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
            rdma_ack_cm_event(event);
            throw std::runtime_error("Connection establishment failed");
        }
        rdma_ack_cm_event(event);
        
        std::cout << "[RDMA] Connection established (server)" << std::endl;
    }
    
    void connect_to_server() {
        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof(conn_param));
        conn_param.responder_resources = 1;
        conn_param.initiator_depth = 1;
        
        if (rdma_connect(conn_id_, &conn_param) != 0) {
            throw std::runtime_error("Failed to connect");
        }
        
        // Wait for established event
        struct rdma_cm_event* event;
        if (rdma_get_cm_event(ec_, &event) != 0) {
            throw std::runtime_error("Failed to get established event");
        }
        if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
            rdma_ack_cm_event(event);
            throw std::runtime_error("Connection establishment failed");
        }
        rdma_ack_cm_event(event);
        
        std::cout << "[RDMA] Connection established (client)" << std::endl;
    }
    
    void run_rdma_write() {
        if (is_server_) {
            std::cout << "[RDMA] Server waiting for RDMA write operations..." << std::endl;
            sleep(5); // Wait for client operations
            return;
        }
        
        // Client performs RDMA write to server
        // For simplicity, assume remote address and key are known
        // In real implementation, these would be exchanged during connection setup
        remote_addr_ = (uint64_t)(uintptr_t)recv_buffer_; // Simplified
        remote_rkey_ = recv_mr_->rkey; // Simplified
        
        auto start = get_timestamp_us();
        post_rdma_write(data_size);
        poll_send_completion();
        auto end = get_timestamp_us();
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size, time_us);
        
        log_performance("RDMA", "write", data_size, time_us, bandwidth);
    }
    
    void run_rdma_read() {
        if (is_server_) {
            std::cout << "[RDMA] Server waiting for RDMA read operations..." << std::endl;
            sleep(5); // Wait for client operations
            return;
        }
        
        // Client performs RDMA read from server
        remote_addr_ = (uint64_t)(uintptr_t)send_buffer_; // Simplified
        remote_rkey_ = send_mr_->rkey; // Simplified
        
        auto start = get_timestamp_us();
        post_rdma_read(data_size);
        poll_send_completion();
        auto end = get_timestamp_us();
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size, time_us);
        
        log_performance("RDMA", "read", data_size, time_us, bandwidth);
    }
    
    void run_rdma_send() {
        if (is_server_) {
            // Server receives
            for (int i = 0; i < 1; ++i) { // Only measured operation
                post_recv(data_size);
                poll_recv_completion();
            }
            std::cout << "[RDMA] Server completed receive operations" << std::endl;
        } else {
            // Client sends
            auto start = get_timestamp_us();
            post_send(data_size);
            poll_send_completion();
            auto end = get_timestamp_us();
            
            uint64_t time_us = end - start;
            double bandwidth = calculate_bandwidth(data_size, time_us);
            
            log_performance("RDMA", "send", data_size, time_us, bandwidth);
        }
    }
    
    void post_rdma_write(size_t size) {
        struct ibv_sge sge;
        sge.addr = (uintptr_t)send_buffer_;
        sge.length = size;
        sge.lkey = send_mr_->lkey;
        
        struct ibv_send_wr wr, *bad_wr;
        memset(&wr, 0, sizeof(wr));
        wr.wr_id = 1;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_RDMA_WRITE;
        wr.send_flags = IBV_SEND_SIGNALED;
        wr.wr.rdma.remote_addr = remote_addr_;
        wr.wr.rdma.rkey = remote_rkey_;
        
        if (ibv_post_send(qp_, &wr, &bad_wr) != 0) {
            throw std::runtime_error("Failed to post RDMA write");
        }
    }
    
    void post_rdma_read(size_t size) {
        struct ibv_sge sge;
        sge.addr = (uintptr_t)recv_buffer_;
        sge.length = size;
        sge.lkey = recv_mr_->lkey;
        
        struct ibv_send_wr wr, *bad_wr;
        memset(&wr, 0, sizeof(wr));
        wr.wr_id = 2;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_RDMA_READ;
        wr.send_flags = IBV_SEND_SIGNALED;
        wr.wr.rdma.remote_addr = remote_addr_;
        wr.wr.rdma.rkey = remote_rkey_;
        
        if (ibv_post_send(qp_, &wr, &bad_wr) != 0) {
            throw std::runtime_error("Failed to post RDMA read");
        }
    }
    
    void post_send(size_t size) {
        struct ibv_sge sge;
        sge.addr = (uintptr_t)send_buffer_;
        sge.length = size;
        sge.lkey = send_mr_->lkey;
        
        struct ibv_send_wr wr, *bad_wr;
        memset(&wr, 0, sizeof(wr));
        wr.wr_id = 3;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_SEND;
        wr.send_flags = IBV_SEND_SIGNALED;
        
        if (ibv_post_send(qp_, &wr, &bad_wr) != 0) {
            throw std::runtime_error("Failed to post send");
        }
    }
    
    void post_recv(size_t size) {
        struct ibv_sge sge;
        sge.addr = (uintptr_t)recv_buffer_;
        sge.length = size;
        sge.lkey = recv_mr_->lkey;
        
        struct ibv_recv_wr wr, *bad_wr;
        memset(&wr, 0, sizeof(wr));
        wr.wr_id = 4;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        
        if (ibv_post_recv(qp_, &wr, &bad_wr) != 0) {
            throw std::runtime_error("Failed to post recv");
        }
    }
    
    void poll_send_completion() {
        struct ibv_wc wc;
        do {
            int n = ibv_poll_cq(send_cq_, 1, &wc);
            if (n < 0) {
                throw std::runtime_error("CQ polling error");
            }
            if (n == 1) {
                if (wc.status != IBV_WC_SUCCESS) {
                    throw std::runtime_error("Send completion error: " + std::to_string(wc.status));
                }
                return;
            }
            usleep(1);
        } while (true);
    }
    
    void poll_recv_completion() {
        struct ibv_wc wc;
        do {
            int n = ibv_poll_cq(recv_cq_, 1, &wc);
            if (n < 0) {
                throw std::runtime_error("CQ polling error");
            }
            if (n == 1) {
                if (wc.status != IBV_WC_SUCCESS) {
                    throw std::runtime_error("Recv completion error: " + std::to_string(wc.status));
                }
                return;
            }
            usleep(1);
        } while (true);
    }

    void warmup_rdma_write() {
        if (is_server_) {
            std::cout << "[RDMA] Server waiting for RDMA write warmup..." << std::endl;
            sleep(2); // Wait for client warmup operations
            return;
        }
        
        remote_addr_ = (uint64_t)(uintptr_t)recv_buffer_;
        remote_rkey_ = recv_mr_->rkey;
        
        size_t warmup_size = std::min(data_size, (size_t)1024);
        for (int i = 0; i < 3; ++i) {
            post_rdma_write(warmup_size);
            poll_send_completion();
        }
    }

    void warmup_rdma_read() {
        if (is_server_) {
            std::cout << "[RDMA] Server waiting for RDMA read warmup..." << std::endl;
            sleep(2); // Wait for client warmup operations
            return;
        }
        
        remote_addr_ = (uint64_t)(uintptr_t)send_buffer_;
        remote_rkey_ = send_mr_->rkey;
        
        size_t warmup_size = std::min(data_size, (size_t)1024);
        for (int i = 0; i < 3; ++i) {
            post_rdma_read(warmup_size);
            poll_send_completion();
        }
    }

    void warmup_rdma_send() {
        if (is_server_) {
            // Server receives warmup operations
            size_t warmup_size = std::min(data_size, (size_t)1024);
            for (int i = 0; i < 3; ++i) {
                post_recv(warmup_size);
                poll_recv_completion();
            }
            std::cout << "[RDMA] Server completed warmup receive operations" << std::endl;
        } else {
            // Client sends warmup operations
            size_t warmup_size = std::min(data_size, (size_t)1024);
            for (int i = 0; i < 3; ++i) {
                post_send(warmup_size);
                poll_send_completion();
            }
        }
    }
};

// Factory function
BenchmarkBase* create_rdma_benchmark() {
    return new RDMABench();
}