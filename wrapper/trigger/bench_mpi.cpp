#include "bench.h"
#include <mpi.h>
#include <vector>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>


class MPIBench : public BenchmarkBase {
private:
    int rank_, size_;
    std::vector<char> send_buffer_, recv_buffer_;
    std::vector<double> send_data_, recv_data_;
    bool mpi_initialized_by_me_;
    size_t data_size;

public:
    MPIBench() : rank_(0), size_(1), mpi_initialized_by_me_(false) {
        __getcy_init;
        __getcy_grp_init;
    }
    
    void initialize(int argc, char** argv) override {
        int provided;
        MPI_Init(&argc, &argv);
        mpi_initialized_by_me_ = true;
        
        MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
        MPI_Comm_size(MPI_COMM_WORLD, &size_);

        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        hostname[sizeof(hostname) - 1] = '\0'; // Ensure null-termination
        auto node_name = hostname;
        
        if (rank_ == 0) {
            std::cout << "[MPI] Initialized with " << size_ << " processes on node " << node_name << std::endl;
        }
    }

    rdmasync::TRIGGER_MSG bench_bcast(rdmasync::TRIGGER_MSG send_msg) override {
        auto msg = send_msg;
        MPI_Bcast(&msg, sizeof(msg), MPI_BYTE, 0, MPI_COMM_WORLD);
        return msg;
    };

    int bench_sync() override {
        return MPI_Barrier(MPI_COMM_WORLD);
    }

    int get_rank() override {
        return rank_;
    }

    int get_world_size() override {
        return size_;
    }

    void setup_buffers(size_t buffer_size) override {
        data_size = buffer_size;
        send_buffer_.resize(buffer_size, 'A' + rank_);
        recv_buffer_.resize(buffer_size, 0);

        send_data_.resize(buffer_size / sizeof(double), rank_);
        recv_data_.resize(buffer_size / sizeof(double), 0.0);
        // Initialize send buffer with some pattern
        for (size_t i = 0; i < buffer_size; ++i) {
            send_buffer_[i] = (char)((i + rank_) % 256);
        }
        for (size_t i = 0; i < send_data_.size(); ++i) {
            send_data_[i] = static_cast<double>(rank_);
        }
    }

    void run_warmup(const std::string& func_name, size_t data_size) override {
        if (func_name == "allreduce") {
            warmup_allreduce();
        } else if (func_name == "send_recv" || func_name == "sendrecv") {
            warmup_send_recv();
        } else if (func_name == "bcast" || func_name == "broadcast") {
            warmup_bcast();
        } else if (func_name == "alltoall") {
            warmup_alltoall();
        } else if (func_name == "reduce") {
            warmup_reduce();
        } else {
            if (rank_ == 0) {
                std::cerr << "[MPI] Unknown function: " << func_name << std::endl;
                std::cerr << "[MPI] Available functions: allreduce, send_recv, bcast, alltoall, reduce" << std::endl;
            }
        }
    }

    void run_benchmark(const std::string& func_name) override {
        if (func_name == "allreduce") {
            run_allreduce();
        } else if (func_name == "send_recv" || func_name == "sendrecv") {
            run_send_recv();
        } else if (func_name == "bcast" || func_name == "broadcast") {
            run_bcast();
        } else if (func_name == "alltoall") {
            run_alltoall();
        } else if (func_name == "reduce") {
            run_reduce();
        } else {
            if (rank_ == 0) {
                std::cerr << "[MPI] Unknown function: " << func_name << std::endl;
                std::cerr << "[MPI] Available functions: allreduce, send_recv, bcast, alltoall, reduce" << std::endl;
            }
        }
    }

    void finalize() override {
        print_performance();
        if (mpi_initialized_by_me_) {
            MPI_Finalize();
        }
    }

private:

    void run_allreduce() {
        size_t count = send_data_.size();
        if (count == 0) count = 1;
        
        std::vector<double> send_data(count, rank_);
        std::vector<double> recv_data(count, 0.0);
        
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        MPI_Allreduce(send_data.data(), recv_data.data(), count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        __getcy_end_t;
        end_time = GET_NS;

        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        if (rank_ == 0) {
            log_performance("MPI", "allreduce", data_size, elapsed_time, elapsed_cy);
        }
    }

    void run_send_recv() {
        size_t data_size = send_buffer_.size();
        if (size_ < 2) {
            if (rank_ == 0) {
                std::cerr << "[MPI] send_recv requires at least 2 processes" << std::endl;
            }
            return;
        }

        auto rank_offset = size_ >> 1;
        
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        
        if (rank_ < rank_offset) {
            MPI_Send(send_buffer_.data(), data_size, MPI_CHAR, rank_ + rank_offset, 0, MPI_COMM_WORLD);
        } else {
            MPI_Recv(recv_buffer_.data(), data_size, MPI_CHAR, rank_ - rank_offset, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        __getcy_end_t;
        end_time = GET_NS;
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        if (rank_ == 0) {
            log_performance("MPI", "send_recv", data_size, elapsed_time, elapsed_cy);
        }
    }

    void run_bcast() {
        size_t data_size = send_buffer_.size();
        
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        MPI_Bcast(send_buffer_.data(), data_size, MPI_CHAR, 0, MPI_COMM_WORLD);
        __getcy_end_t;
        end_time = GET_NS;
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        if (rank_ == 0) {
            log_performance("MPI", "bcast", data_size, elapsed_time, elapsed_cy);
        }
    }

    void run_alltoall() {
        size_t per_process_size = send_buffer_.size();
        if (per_process_size == 0) per_process_size = 1;
        
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        MPI_Alltoall(send_buffer_.data(), per_process_size, MPI_CHAR,
                    recv_buffer_.data(), per_process_size, MPI_CHAR,
                    MPI_COMM_WORLD);
        __getcy_end_t;
        end_time = GET_NS;
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        if (rank_ == 0) {
            log_performance("MPI", "alltoall", per_process_size, elapsed_time, elapsed_cy);
        }
    }

    void run_reduce() {
        size_t data_size = send_data_.size();
        if (data_size == 0) data_size = 1;

        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        MPI_Reduce(send_data_.data(), recv_data_.data(), data_size, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        __getcy_end_t;
        end_time = GET_NS;
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        if (rank_ == 0) {
            log_performance("MPI", "reduce", data_size, elapsed_time, elapsed_cy);
        }
    }

    void warmup_allreduce() {
        size_t count = send_data_.size();
        if (count == 0) count = 1;
        
        std::vector<double> send_data(count, rank_);
        std::vector<double> recv_data(count, 0.0);
        
        for (int i = 0; i < 3; ++i) {
            MPI_Allreduce(send_data.data(), recv_data.data(), count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        }
    }

    void warmup_send_recv() {
        if (size_ < 2) return;
        
        size_t warmup_size = std::min(data_size, (size_t)1024);
        auto rank_offset = size_ >> 1;
        
        for (int i = 0; i < 3; ++i) {
            if (rank_ < rank_offset) {
                MPI_Send(send_buffer_.data(), warmup_size, MPI_CHAR, rank_ + rank_offset, 0, MPI_COMM_WORLD);
            } else {
                MPI_Recv(recv_buffer_.data(), warmup_size, MPI_CHAR, rank_ - rank_offset, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
    }

    void warmup_bcast() {
        size_t warmup_size = std::min(data_size, (size_t)1024);
        for (int i = 0; i < 3; ++i) {
            MPI_Bcast(send_buffer_.data(), warmup_size, MPI_CHAR, 0, MPI_COMM_WORLD);
        }
    }

    void warmup_alltoall() {
        size_t warmup_size = std::min(data_size, (size_t)1024);
        for (int i = 0; i < 3; ++i) {
            MPI_Alltoall(send_buffer_.data(), warmup_size, MPI_CHAR,
                        recv_buffer_.data(), warmup_size, MPI_CHAR,
                        MPI_COMM_WORLD);
        }
    }

    void warmup_reduce() {
        size_t count = std::min(send_data_.size(), (size_t)256);
        if (count == 0) count = 1;
        
        for (int i = 0; i < 3; ++i) {
            MPI_Reduce(send_data_.data(), recv_data_.data(), count, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        }
    }
};

// Factory function
BenchmarkBase* create_mpi_benchmark() {
    return new MPIBench();
}

