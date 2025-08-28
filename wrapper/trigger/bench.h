#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <chrono>
#include <iostream>

// Common benchmark interface
class BenchmarkBase {
public:
    virtual ~BenchmarkBase() = default;
    virtual void initialize(int argc, char** argv) = 0;
    virtual void setup_buffers(size_t buffer_size) = 0;
    virtual void run_benchmark(const std::string& func_name) = 0;
    virtual void run_warmup(const std::string& func_name, size_t data_size) = 0;
    virtual void finalize() = 0;

    virtual int get_rank() {
        return 0;
    };
    virtual int get_world_size() {
        return 1;
    }

protected:
    // Helper function to get current timestamp in microseconds
    static uint64_t get_timestamp_us() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
    
    // Helper function to calculate bandwidth in MB/s
    static double calculate_bandwidth(size_t bytes, uint64_t time_us) {
        if (time_us == 0) return 0.0;
        return (double)bytes / time_us; // MB/s
    }
    
    // Helper function to log performance results
    static void log_performance(const std::string& benchmark_type, 
                               const std::string& function_name,
                               size_t data_size, 
                               uint64_t time_us, 
                               double bandwidth = 0.0) {
        std::cout << "[PERF] " << benchmark_type << "::" << function_name 
                  << " size=" << data_size 
                  << " time=" << time_us << "us";
        if (bandwidth > 0.0) {
            std::cout << " bandwidth=" << bandwidth << "MB/s";
        }
        std::cout << std::endl;
    }
};

// Benchmark factory function declarations
BenchmarkBase* create_mpi_benchmark();
BenchmarkBase* create_rdma_benchmark();
BenchmarkBase* create_io_benchmark();