/***
 * This file is the trigger helper which plays a role of participant,
 * and recieves the coordinator's messages.  
 * According to the recieved message, the trigger helper will take 
 * appropriate actions as below:
 * - ONCE: run the preset program once
 * - STOP: stop the preset program
 * - REPEAT: repeat the preset program until receive a stop message
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <memory>

#include "bench.h"
#include "sync.h"

class ParticipantBench {
private:
    std::atomic<bool> running_{true};
    std::atomic<bool> repeat_mode_{false};
    std::string benchmark_type_;
    std::string function_name_;
    size_t data_size_;
    int rank_, world_size_;
    std::unique_ptr<BenchmarkBase> benchmark_;
    std::thread worker_thread_;
    
public:
    ParticipantBench(const std::string& bench_type, const std::string& func, size_t size) 
        : benchmark_type_(bench_type), function_name_(func), data_size_(size) {
        
        // Create appropriate benchmark
        if (bench_type == "mpi") {
            benchmark_.reset(create_mpi_benchmark());
        } else if (bench_type == "rdma") {
            benchmark_.reset(create_rdma_benchmark());
        } else if (bench_type == "io") {
            benchmark_.reset(create_io_benchmark());
        } else {
            throw std::runtime_error("Unknown benchmark type: " + bench_type);
        }
    }
    
    ~ParticipantBench() {
        stop();
    }
    
    void initialize(int argc, char** argv) {
        // Initialize the benchmark
        benchmark_->initialize(argc, argv);
        benchmark_->setup_buffers(std::max(data_size_, (size_t)4096));

        rank_ = benchmark_->get_rank();
        world_size_ = benchmark_->get_world_size(); 

        rdmasync::init(rank_);
        
        if(rank_ == 0) std::cout << "[ParticipantBench] Initialized " << benchmark_type_ 
                  << " benchmark with function " << function_name_ 
                  << " and size " << data_size_ << std::endl;
    }
    
    void start() {
        if(rank_ == 0) std::cout << "[ParticipantBench] Started as participant, waiting for coordinator messages..." << std::endl;
        
        running_ = true;
        
        // Main loop waiting for trigger messages
        while (running_) {
            try {
                // This will block until a trigger message is received
                // The tp_trigger function in sync.cpp will handle the message reception
                // and call our callback through the RDMA sync mechanism
                rdmasync::TRIGGER_MSG received_msg = rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
                rdmasync::tp_sync(__func__);
                if (received_msg == rdmasync::TRIGGER_MSG::ONCE) {
                    if(rank_ == 0) std::cout << "[ParticipantBench] Received ONCE message, executing benchmark..." << std::endl;
                    execute_once();
                } else if (received_msg == rdmasync::TRIGGER_MSG::REPEAT) {
                    execute_repeat();
                } else if (received_msg == rdmasync::TRIGGER_MSG::STOP) {
                    stop_repeat();
                } else if (received_msg == rdmasync::TRIGGER_MSG::FINISH) {
                    break;
                }
                rdmasync::tp_sync(__func__);
            } catch (const std::exception& e) {
                std::cerr << "[ParticipantBench] Error in main loop: " << e.what() << std::endl;
                break;
            }
        }
        finalize();
        if(rank_ == 0) std::cout << "[ParticipantBench] Exiting..." << std::endl;
    }
    
    void stop() {
        running_ = false;
        repeat_mode_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    void execute_once() {
        if (!benchmark_) {
            if(rank_ == 0) std::cout << "[ParticipantBench] No benchmark configured" << std::endl;
            return;
        }
        
        if(rank_ == 0) std::cout << "[ParticipantBench] Executing " << benchmark_type_ 
                  << "::" << function_name_ << " once..." << std::endl;
        
        try {
            benchmark_->run_benchmark(function_name_);
        } catch (const std::exception& e) {
            std::cerr << "[ParticipantBench] Error executing benchmark: " << e.what() << std::endl;
        }
    }
    
    void execute_repeat() {
        if (!benchmark_) {
            if(rank_ == 0) std::cout << "[ParticipantBench] No benchmark configured" << std::endl;
            return;
        }
        
        repeat_mode_ = true;
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        worker_thread_ = std::thread([this]() {
            if(rank_ == 0) std::cout << "[ParticipantBench] Starting repeat mode for " 
                      << benchmark_type_ << "::" << function_name_ << "..." << std::endl;
            
            while (repeat_mode_ && running_) {
                try {
                    benchmark_->run_benchmark(function_name_);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                } catch (const std::exception& e) {
                    std::cerr << "[ParticipantBench] Error in repeat mode: " << e.what() << std::endl;
                    break;
                }
            }
            if(rank_ == 0) std::cout << "[ParticipantBench] Repeat mode stopped" << std::endl;
        });
    }
    
    void stop_repeat() {
        repeat_mode_ = false;
        if(rank_ == 0) std::cout << "[ParticipantBench] Stopping repeat mode..." << std::endl;
    }
    
    void finalize() {
        if (benchmark_) {
            benchmark_->finalize();
        }
        rdmasync::finalize();
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <benchmark_type> <function> <size> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Benchmark types:" << std::endl;
    std::cout << "  mpi    - MPI communication benchmarks" << std::endl;
    std::cout << "  rdma   - RDMA communication benchmarks" << std::endl;
    std::cout << "  io     - I/O performance benchmarks" << std::endl;
    std::cout << std::endl;
    std::cout << "Environment variables for RDMA sync:" << std::endl;
    std::cout << "  RDMASYNC_ENABLE=1        - Enable RDMA synchronization" << std::endl;
    std::cout << "  RDMASYNC_ROLE=participant - Set role as participant" << std::endl;
    std::cout << "  RDMASYNC_SERVER_IP=<ip>  - Coordinator's IP address" << std::endl;
    std::cout << "  RDMASYNC_DEBUG=1         - Enable debug output" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " mpi allreduce 1024" << std::endl;
    std::cout << "  " << program_name << " rdma write 4096 --client 192.168.1.100" << std::endl;
    std::cout << "  " << program_name << " io read 1048576" << std::endl;
}

/**
 * Receive and set environment variables
 * -env VAR value
 */
void parse_and_set_envs(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "-env" && i + 2 < argc) {
            const char* var = argv[i + 1];
            const char* value = argv[i + 2];
            setenv(var, value, 1);
            i += 2;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string benchmark_type = argv[1];
    std::string function = argv[2];
    size_t size = 0;

    parse_and_set_envs(argc, argv);
    
    try {
        size = std::stoull(argv[3]);
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid size parameter: " << argv[3] << std::endl;
        return 1;
    }
    
    try {
        ParticipantBench participant(benchmark_type, function, size);
        participant.initialize(argc, argv);
        participant.start();
    } catch (const std::exception& e) {
        std::cerr << "[ParticipantBench] Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}