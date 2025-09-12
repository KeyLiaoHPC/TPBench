#include "bench.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstring>
#include <aio.h>

class IOBench : public BenchmarkBase {
private:
    std::vector<char> buffer_;
    std::string test_file_prefix_;
    std::string test_dir_;
    size_t file_size_;
    size_t block_size_;
    size_t data_size;

    bool use_direct_io_;
    bool use_sync_;
    
public:
    IOBench() : file_size_(0), block_size_(4096), use_direct_io_(false), use_sync_(true) {
        __getcy_init;
        __getcy_grp_init;
        // Use process ID to avoid conflicts
        pid_t pid = getpid();
        test_file_prefix_ = "/tmp/tpbench_io_test_" + std::to_string(pid);
        test_dir_ = "/tmp";
    }

    void initialize(int argc, char** argv) override {
        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
                test_dir_ = argv[i + 1];
                test_file_prefix_ = test_dir_ + "/tpbench_io_test_" + std::to_string(getpid());
                i++;
            } else if (strcmp(argv[i], "--block-size") == 0 && i + 1 < argc) {
                block_size_ = std::stoull(argv[i + 1]);
                i++;
            } else if (strcmp(argv[i], "--direct") == 0) {
                use_direct_io_ = true;
            } else if (strcmp(argv[i], "--no-sync") == 0) {
                use_sync_ = false;
            }
        }
        
        // Check if test directory exists and is writable
        if (access(test_dir_.c_str(), W_OK) != 0) {
            throw std::runtime_error("Test directory " + test_dir_ + " is not writable");
        }
        
        // Get filesystem info
        struct statvfs fs_stat;
        if (statvfs(test_dir_.c_str(), &fs_stat) == 0) {
            uint64_t free_space = fs_stat.f_bavail * fs_stat.f_bsize;
            std::cout << "[IO] Available space: " << (free_space / (1024*1024)) << " MB" << std::endl;
        }
        
        std::cout << "[IO] Initialized - test_dir: " << test_dir_ 
                  << ", block_size: " << block_size_ 
                  << ", direct_io: " << (use_direct_io_ ? "yes" : "no")
                  << ", sync: " << (use_sync_ ? "yes" : "no") << std::endl;
    }

    void setup_buffers(size_t buffer_size) override {
        file_size_ = buffer_size;
        data_size = buffer_size;

        // Align buffer size to block size for direct I/O
        size_t aligned_size = ((buffer_size + block_size_ - 1) / block_size_) * block_size_;
        
        if (use_direct_io_) {
            // Allocate aligned memory for direct I/O
            void* aligned_mem = nullptr;
            if (posix_memalign(&aligned_mem, 4096, aligned_size) != 0) {
                throw std::runtime_error("Failed to allocate aligned memory");
            }
            buffer_.resize(aligned_size);
            memcpy(buffer_.data(), aligned_mem, aligned_size);
            free(aligned_mem);
        } else {
            buffer_.resize(aligned_size);
        }

        // Initialize buffer with pattern
        for (size_t i = 0; i < buffer_.size(); ++i) {
            buffer_[i] = (char)(i % 256);
        }

        std::vector<std::string> test_file_list = {
            test_file_prefix_ + "_mixed",
            test_file_prefix_ + "_seq_read",
            test_file_prefix_ + "_seq_write",
            test_file_prefix_ + "_rand_read",
            test_file_prefix_ + "_rand_write"};
        for (auto test_file: test_file_list) {
            // Create the test file with the specified size
            create_test_file(test_file, data_size);
        }

        std::cout << "[IO] Buffer allocated: " << buffer_.size() << " bytes" << std::endl;
    }

    void run_warmup(const std::string& func_name, size_t data_size) override {
        if (func_name == "read" || func_name == "sequential_read") {
            warmup_sequential_read();
        } else if (func_name == "write" || func_name == "sequential_write") {
            warmup_sequential_write();
        } else if (func_name == "random_read") {
            warmup_random_read();
        } else if (func_name == "random_write") {
            warmup_random_write();
        } else if (func_name == "mixed") {
            warmup_mixed_io();
        } else {
            std::cerr << "[IO] Unknown function: " << func_name << std::endl;
            std::cerr << "[IO] Available functions: read, write, random_read, random_write, mixed" << std::endl;
        }
    }

    void run_benchmark(const std::string& func_name) override {
        if (func_name == "read" || func_name == "sequential_read") {
            run_sequential_read();
        } else if (func_name == "write" || func_name == "sequential_write") {
            run_sequential_write();
        } else if (func_name == "random_read") {
            run_random_read();
        } else if (func_name == "random_write") {
            run_random_write();
        } else if (func_name == "mixed") {
            run_mixed_io();
        } else {
            std::cerr << "[IO] Unknown function: " << func_name << std::endl;
            std::cerr << "[IO] Available functions: read, write, random_read, random_write, mixed" << std::endl;
        }
    }

    void finalize() override {
        // Clean up test files
        print_performance();
        cleanup_test_files();
        std::cout << "[IO] Cleanup completed" << std::endl;
    }

private:
    void run_sequential_write() {
        std::string filename = test_file_prefix_ + "_seq_write";
        // Prepare file for writing
        int flags = O_CREAT | O_WRONLY | O_TRUNC;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags, 0644);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
        
        // Measured write
        lseek(fd, 0, SEEK_SET);
        
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        write_data(fd, data_size);
        if (use_sync_) {
            fsync(fd);
        }
        __getcy_end_t;
        end_time = GET_NS;
        
        close(fd);
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        log_performance("IO", "sequential_write", data_size, elapsed_time, elapsed_cy);
    }

    void run_sequential_read() {
        std::string filename = test_file_prefix_ + "_seq_read";

        // Prepare file for reading
        int flags = O_RDONLY;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for reading: " + filename);
        }
        
        // Measured read
        lseek(fd, 0, SEEK_SET);
        
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        read_data(fd, data_size);
        __getcy_end_t;
        end_time = GET_NS;
        
        close(fd);
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        log_performance("IO", "sequential_read", data_size, elapsed_time, elapsed_cy);
    }
    
    void run_random_write() {
        std::string filename = test_file_prefix_ + "_random_write";
        
        int flags = O_WRONLY;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for random writing: " + filename);
        }
        
        // Calculate number of operations
        size_t num_ops = std::min((size_t)1000, data_size / block_size_);
        if (num_ops == 0) num_ops = 1;
        
        // Generate random offsets
        std::vector<off_t> offsets;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> dis(0, (data_size - block_size_) / block_size_);
        
        for (size_t i = 0; i < num_ops; ++i) {
            offsets.push_back(dis(gen) * block_size_);
        }
        
        // Measured random writes
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        for (size_t i = 0; i < num_ops; ++i) {
            lseek(fd, offsets[i], SEEK_SET);
            write(fd, buffer_.data(), block_size_);
        }
        if (use_sync_) {
            fsync(fd);
        }
        __getcy_end_t;
        end_time = GET_NS;
        
        close(fd);
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        std::cout << "[PERF] IO::random_write size=" << data_size 
                  << " ops=" << num_ops 
                  << " time=" << elapsed_time << "ns"
                  << " cycles=" << elapsed_cy << std::endl;
    }
    
    void run_random_read() {
        std::string filename = test_file_prefix_ + "_random_read";
        
        int flags = O_RDONLY;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for random reading: " + filename);
        }
        
        // Calculate number of operations
        size_t num_ops = std::min((size_t)1000, data_size / block_size_);
        if (num_ops == 0) num_ops = 1;
        
        // Generate random offsets
        std::vector<off_t> offsets;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> dis(0, (data_size - block_size_) / block_size_);
        
        for (size_t i = 0; i < num_ops; ++i) {
            offsets.push_back(dis(gen) * block_size_);
        }
        
        // Measured random reads
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        for (size_t i = 0; i < num_ops; ++i) {
            lseek(fd, offsets[i], SEEK_SET);
            read(fd, buffer_.data(), block_size_);
        }
        __getcy_end_t;
        end_time = GET_NS;
        
        close(fd);
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        std::cout << "[PERF] IO::random_read size=" << data_size 
                  << " ops=" << num_ops 
                  << " time=" << elapsed_time << "ns"
                  << " cycles=" << elapsed_cy << std::endl;
    }
    
    void run_mixed_io() {
        std::string filename = test_file_prefix_ + "_mixed";
        
        int flags = O_RDWR;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for mixed I/O: " + filename);
        }
        
        // Calculate number of operations (70% read, 30% write)
        size_t total_ops = std::min((size_t)1000, data_size / block_size_);
        if (total_ops == 0) total_ops = 1;
        
        size_t read_ops = total_ops * 7 / 10;
        size_t write_ops = total_ops - read_ops;
        
        // Generate random offsets and operations
        std::vector<std::pair<bool, off_t>> operations; // true=read, false=write
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> dis(0, (data_size - block_size_) / block_size_);
        
        for (size_t i = 0; i < read_ops; ++i) {
            operations.push_back({true, dis(gen) * block_size_});
        }
        for (size_t i = 0; i < write_ops; ++i) {
            operations.push_back({false, dis(gen) * block_size_});
        }
        
        // Shuffle operations
        std::shuffle(operations.begin(), operations.end(), gen);
        
        // Measured mixed I/O
        uint64_t start_time, end_time, elapsed_time;
        uint64_t start_cy, end_cy, elapsed_cy;
        uint64_t hi1, lo1, hi2, lo2;
        
        start_time = GET_NS;
        __getcy_st_t;
        for (const auto& op : operations) {
            lseek(fd, op.second, SEEK_SET);
            if (op.first) {
                read(fd, buffer_.data(), block_size_);
            } else {
                write(fd, buffer_.data(), block_size_);
            }
        }
        if (use_sync_) {
            fsync(fd);
        }
        __getcy_end_t;
        end_time = GET_NS;
        
        close(fd);
        
        start_cy = ((hi1 << 32) | lo1);
        end_cy = ((hi2 << 32) | lo2);
        elapsed_time = end_time - start_time;
        elapsed_cy = end_cy - start_cy;
        
        std::cout << "[PERF] IO::mixed size=" << data_size 
                  << " ops=" << total_ops << " (r:" << read_ops << " w:" << write_ops << ")"
                  << " time=" << elapsed_time << "ns"
                  << " cycles=" << elapsed_cy << std::endl;
    }
    
    void create_test_file(const std::string& filename, size_t size) {
        int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            throw std::runtime_error("Failed to create test file: " + filename);
        }
        
        write_data(fd, size);
        fsync(fd);
        close(fd);
    }
    
    void write_data(int fd, size_t size) {
        size_t written = 0;
        while (written < size) {
            size_t to_write = std::min(buffer_.size(), size - written);
            ssize_t result = write(fd, buffer_.data(), to_write);
            if (result < 0) {
                throw std::runtime_error("Write error: " + std::string(strerror(errno)));
            }
            written += result;
        }
    }
    
    void read_data(int fd, size_t size) {
        size_t read_total = 0;
        while (read_total < size) {
            size_t to_read = std::min(buffer_.size(), size - read_total);
            ssize_t result = read(fd, buffer_.data(), to_read);
            if (result < 0) {
                throw std::runtime_error("Read error: " + std::string(strerror(errno)));
            }
            if (result == 0) break; // EOF
            read_total += result;
        }
    }
    
    void cleanup_test_files() {
        std::vector<std::string> suffixes = {"_seq_write", "_seq_read", "_random_write", "_random_read", "_mixed"};
        for (const auto& suffix : suffixes) {
            std::string filename = test_file_prefix_ + suffix;
            unlink(filename.c_str());
        }
    }

    void warmup_sequential_write() {
        std::string filename = test_file_prefix_ + "_seq_write_warmup";
        create_test_file(filename, data_size);
        int flags = O_CREAT | O_WRONLY | O_TRUNC;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags, 0644);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for warmup writing: " + filename);
        }
        
        for (int i = 0; i < 3; ++i) {
            lseek(fd, 0, SEEK_SET);
            write_data(fd, data_size);
            if (use_sync_) {
                fsync(fd);
            }
        }
        
        close(fd);
        unlink(filename.c_str());
    }

    void warmup_sequential_read() {
        std::string filename = test_file_prefix_ + "_seq_read_warmup";
        create_test_file(filename, data_size);
        
        int flags = O_RDONLY;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for warmup reading: " + filename);
        }
        
        for (int i = 0; i < 3; ++i) {
            lseek(fd, 0, SEEK_SET);
            read_data(fd, data_size);
        }
        
        close(fd);
        unlink(filename.c_str());
    }

    void warmup_random_write() {
        std::string filename = test_file_prefix_ + "_random_write_warmup";
        create_test_file(filename, data_size);
        
        int flags = O_WRONLY;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for warmup random writing: " + filename);
        }
        
        size_t num_ops = std::min((size_t)100, data_size / block_size_);
        if (num_ops == 0) num_ops = 1;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> dis(0, (data_size - block_size_) / block_size_);
        
        for (int w = 0; w < 3; ++w) {
            for (size_t i = 0; i < std::min(num_ops, (size_t)10); ++i) {
                off_t offset = dis(gen) * block_size_;
                lseek(fd, offset, SEEK_SET);
                write(fd, buffer_.data(), block_size_);
            }
            if (use_sync_) {
                fsync(fd);
            }
        }
        
        close(fd);
        unlink(filename.c_str());
    }

    void warmup_random_read() {
        std::string filename = test_file_prefix_ + "_random_read_warmup";
        create_test_file(filename, data_size);
        
        int flags = O_RDONLY;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for warmup random reading: " + filename);
        }
        
        size_t num_ops = std::min((size_t)100, data_size / block_size_);
        if (num_ops == 0) num_ops = 1;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> dis(0, (data_size - block_size_) / block_size_);
        
        for (int w = 0; w < 3; ++w) {
            for (size_t i = 0; i < std::min(num_ops, (size_t)10); ++i) {
                off_t offset = dis(gen) * block_size_;
                lseek(fd, offset, SEEK_SET);
                read(fd, buffer_.data(), block_size_);
            }
        }
        
        close(fd);
        unlink(filename.c_str());
    }

    void warmup_mixed_io() {
        std::string filename = test_file_prefix_ + "_mixed_warmup";
        create_test_file(filename, data_size);
        
        int flags = O_RDWR;
        if (use_direct_io_) {
            flags |= O_DIRECT;
        }
        
        int fd = open(filename.c_str(), flags);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file for warmup mixed I/O: " + filename);
        }
        
        size_t total_ops = std::min((size_t)100, data_size / block_size_);
        if (total_ops == 0) total_ops = 1;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<off_t> dis(0, (data_size - block_size_) / block_size_);
        std::uniform_int_distribution<int> op_dis(0, 1);
        
        for (int w = 0; w < 3; ++w) {
            for (size_t i = 0; i < std::min(total_ops, (size_t)10); ++i) {
                off_t offset = dis(gen) * block_size_;
                lseek(fd, offset, SEEK_SET);
                if (op_dis(gen)) {
                    read(fd, buffer_.data(), block_size_);
                } else {
                    write(fd, buffer_.data(), block_size_);
                }
            }
            if (use_sync_) {
                fsync(fd);
            }
        }
        
        close(fd);
        unlink(filename.c_str());
    }
};

// Factory function
BenchmarkBase* create_io_benchmark() {
    return new IOBench();
}

