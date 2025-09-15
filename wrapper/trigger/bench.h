#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <chrono>
#include <iostream>
#include <vector>
#include "sync.h"

extern "C" {
#include "tptimer.h"
}

#define GET_NS \
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

// Common benchmark interface
class BenchmarkBase {
public:
    virtual ~BenchmarkBase() = default;
    virtual void initialize(int argc, char** argv) = 0;
    virtual void setup_buffers(size_t buffer_size) = 0;
    virtual void run_benchmark(const std::string& func_name) = 0;
    virtual void run_warmup(const std::string& func_name, size_t data_size) = 0;
    virtual void finalize() = 0;
    
    virtual rdmasync::TRIGGER_MSG bench_bcast(rdmasync::TRIGGER_MSG send_msg) {
        return send_msg;
    };

    virtual int bench_sync() {
        return 0;
    };
    virtual int get_rank() {
        return 0;
    };
    virtual int get_world_size() {
        return 1;
    };

protected:
    // Helper function to get current timestamp in nanoseconds  
    static uint64_t get_timestamp_ns() {
        return GET_NS;
    }
    

    static void log_performance(const std::string& benchmark_type, 
                               const std::string& function_name,
                               size_t data_size, 
                               uint64_t time_ns, 
                               uint64_t cycles = 0) {
        performance_records_.push_back({benchmark_type, function_name, data_size, time_ns, cycles});
    }
    
    static void print_performance() {
        for (const auto& record : performance_records_) {
            std::cout << "[PERF] " << record.benchmark_type << "::" << record.function_name 
                      << " size=" << record.data_size 
                      << " time=" << record.time_ns << "ns";
            if (record.cycles > 0) {
                std::cout << " cycles=" << record.cycles;
            }
            std::cout << std::endl;
        }
    }
    
    static void clear_performance() {
        performance_records_.clear();
    }
private:
    // Helper function to log performance results
    struct PerformanceRecord {
        std::string benchmark_type;
        std::string function_name;
        size_t data_size;
        uint64_t time_ns;
        uint64_t cycles;
    };
    inline static std::vector<PerformanceRecord> performance_records_;
};

// Benchmark factory function declarations
BenchmarkBase* create_mpi_benchmark();
BenchmarkBase* create_rdma_benchmark();
BenchmarkBase* create_io_benchmark();