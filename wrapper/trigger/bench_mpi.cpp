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
    bool mpi_initialized_by_me_;

public:
    MPIBench() : rank_(0), size_(1), mpi_initialized_by_me_(false) {}
    
    void initialize(int argc, char** argv) override {
        int provided;
        MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
        mpi_initialized_by_me_ = true;
        
        MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
        MPI_Comm_size(MPI_COMM_WORLD, &size_);
        
        if (rank_ == 0) {
            std::cout << "[MPI] Initialized with " << size_ << " processes" << std::endl;
        }
    }

    void setup_buffers(size_t buffer_size) override {
        send_buffer_.resize(buffer_size, 'A' + rank_);
        recv_buffer_.resize(buffer_size, 0);
        
        // Initialize send buffer with some pattern
        for (size_t i = 0; i < buffer_size; ++i) {
            send_buffer_[i] = (char)((i + rank_) % 256);
        }
    }

    void run_benchmark(const std::string& func_name, size_t data_size) override {
        if (func_name == "allreduce") {
            run_allreduce(data_size);
        } else if (func_name == "send_recv" || func_name == "sendrecv") {
            run_send_recv(data_size);
        } else if (func_name == "bcast" || func_name == "broadcast") {
            run_bcast(data_size);
        } else if (func_name == "alltoall") {
            run_alltoall(data_size);
        } else if (func_name == "reduce") {
            run_reduce(data_size);
        } else if (func_name == "barrier") {
            run_barrier();
        } else {
            if (rank_ == 0) {
                std::cerr << "[MPI] Unknown function: " << func_name << std::endl;
                std::cerr << "[MPI] Available functions: allreduce, send_recv, bcast, alltoall, reduce, barrier" << std::endl;
            }
        }
    }

    void finalize() override {
        if (mpi_initialized_by_me_) {
            MPI_Finalize();
        }
    }

private:
    void run_allreduce(size_t data_size) {
        size_t count = data_size / sizeof(double);
        if (count == 0) count = 1;
        
        std::vector<double> send_data(count, rank_);
        std::vector<double> recv_data(count, 0.0);
        
        // Warmup
        for (int i = 0; i < 3; ++i) {
            MPI_Allreduce(send_data.data(), recv_data.data(), count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        auto start = get_timestamp_us();
        MPI_Allreduce(send_data.data(), recv_data.data(), count, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        auto end = get_timestamp_us();
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size * 2, time_us); // Send and receive
        
        if (rank_ == 0) {
            log_performance("MPI", "allreduce", data_size, time_us, bandwidth);
        }
    }

    void run_send_recv(size_t data_size) {
        if (size_ < 2) {
            if (rank_ == 0) {
                std::cerr << "[MPI] send_recv requires at least 2 processes" << std::endl;
            }
            return;
        }
        
        setup_buffers(data_size);
        
        // Warmup
        for (int i = 0; i < 3; ++i) {
            if (rank_ == 0) {
                MPI_Send(send_buffer_.data(), data_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(recv_buffer_.data(), data_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else if (rank_ == 1) {
                MPI_Recv(recv_buffer_.data(), data_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(send_buffer_.data(), data_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
            }
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        uint64_t start = get_timestamp_us();
        
        if (rank_ == 0) {
            MPI_Send(send_buffer_.data(), data_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
            MPI_Recv(recv_buffer_.data(), data_size, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else if (rank_ == 1) {
            MPI_Recv(recv_buffer_.data(), data_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(send_buffer_.data(), data_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
        }
        
        uint64_t end = get_timestamp_us();
        MPI_Barrier(MPI_COMM_WORLD);
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size * 2, time_us);
        
        if (rank_ == 0) {
            log_performance("MPI", "send_recv", data_size, time_us, bandwidth);
        }
    }

    void run_bcast(size_t data_size) {
        setup_buffers(data_size);
        
        // Warmup
        for (int i = 0; i < 3; ++i) {
            MPI_Bcast(send_buffer_.data(), data_size, MPI_CHAR, 0, MPI_COMM_WORLD);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        auto start = get_timestamp_us();
        MPI_Bcast(send_buffer_.data(), data_size, MPI_CHAR, 0, MPI_COMM_WORLD);
        auto end = get_timestamp_us();
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size, time_us);
        
        if (rank_ == 0) {
            log_performance("MPI", "bcast", data_size, time_us, bandwidth);
        }
    }

    void run_alltoall(size_t data_size) {
        size_t per_process_size = data_size / size_;
        if (per_process_size == 0) per_process_size = 1;
        
        std::vector<char> send_data(per_process_size * size_);
        std::vector<char> recv_data(per_process_size * size_);
        
        // Initialize send data
        for (size_t i = 0; i < send_data.size(); ++i) {
            send_data[i] = (char)((i + rank_) % 256);
        }
        
        // Warmup
        for (int i = 0; i < 3; ++i) {
            MPI_Alltoall(send_data.data(), per_process_size, MPI_CHAR,
                        recv_data.data(), per_process_size, MPI_CHAR,
                        MPI_COMM_WORLD);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        auto start = get_timestamp_us();
        MPI_Alltoall(send_data.data(), per_process_size, MPI_CHAR,
                    recv_data.data(), per_process_size, MPI_CHAR,
                    MPI_COMM_WORLD);
        auto end = get_timestamp_us();
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size * 2, time_us);
        
        if (rank_ == 0) {
            log_performance("MPI", "alltoall", data_size, time_us, bandwidth);
        }
    }

    void run_reduce(size_t data_size) {
        size_t count = data_size / sizeof(double);
        if (count == 0) count = 1;
        
        std::vector<double> send_data(count, rank_ + 1.0);
        std::vector<double> recv_data(count, 0.0);
        
        // Warmup
        for (int i = 0; i < 3; ++i) {
            MPI_Reduce(send_data.data(), recv_data.data(), count, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        auto start = get_timestamp_us();
        MPI_Reduce(send_data.data(), recv_data.data(), count, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        auto end = get_timestamp_us();
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        uint64_t time_us = end - start;
        double bandwidth = calculate_bandwidth(data_size, time_us);
        
        if (rank_ == 0) {
            log_performance("MPI", "reduce", data_size, time_us, bandwidth);
        }
    }

    void run_barrier() {
        // Warmup
        for (int i = 0; i < 3; ++i) {
            MPI_Barrier(MPI_COMM_WORLD);
        }
        
        auto start = get_timestamp_us();
        MPI_Barrier(MPI_COMM_WORLD);
        auto end = get_timestamp_us();
        
        uint64_t time_us = end - start;
        
        if (rank_ == 0) {
            log_performance("MPI", "barrier", 0, time_us, 0.0);
        }
    }
};

// Factory function
BenchmarkBase* create_mpi_benchmark() {
    return new MPIBench();
}

// Standalone main function for direct execution
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <function> <size>" << std::endl;
        std::cerr << "Functions: allreduce, send_recv, bcast, alltoall, reduce, barrier" << std::endl;
        std::cerr << "Size: data size in bytes (ignored for barrier)" << std::endl;
        return 1;
    }
    
    std::string function = argv[1];
    size_t size = std::stoull(argv[2]);
    
    MPIBench bench;
    
    try {
        bench.initialize(argc, argv);
        bench.setup_buffers(size);
        bench.run_benchmark(function, size);
        bench.finalize();
    } catch (const std::exception& e) {
        std::cerr << "[MPI] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
