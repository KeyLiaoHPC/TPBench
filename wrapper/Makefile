# Compiler and flags
MPI_PATH = /dssg/opt/icelake/linux-centos8-icelake/gcc-8.5.0/intel-oneapi-mpi-2021.4.0-5xzcg7kxlx4ytkz2plqalrg424so43cy/mpi/2021.4.0/bin
C = $(MPI_PATH)/mpicc
CFLAGS = -O3 

CXX = $(MPI_PATH)/mpicxx
CXXFLAGS = -fPIC -shared -std=c++17 -O3 -Wno-write-strings -Wno-unused-result -w
LDFLAGS = -lstdc++fs -lrdmacm -libverbs -lpthread

# Directories
CURRENT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
TPBENCH_DIR := $(abspath $(CURRENT_DIR)/..)
SRC_DIR = instrumentation
OBJ_DIR = obj
LIB_DIR = lib
TPBENCH_LIB_DIR = $(TPBENCH_DIR)/lib
SYNC_INC = -I$(CURRENT_DIR)/sync

# Source files and object files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
SYNC_SOURCES = $(wildcard sync/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES)) \
          $(patsubst sync/%.cpp,$(OBJ_DIR)/sync_%.o,$(SYNC_SOURCES))
TPBENCH_INCLUDES = -I$(TPBENCH_DIR)/src/include
TPBENCH_LIB = $(TPBENCH_LIB_DIR)/libtpbench.a

CXXFLAGS += $(TPBENCH_INCLUDES) $(SYNC_INC)
LDFLAGS += -L$(TPBENCH_LIB_DIR) -ltpbench

# Target libraries and binaries
TARGET_LIB = $(LIB_DIR)/libmpi_trace.so
TRIGGER_BIN = trigger/trigger
COORDINATOR_BIN = trigger/coordinator
PARTICIPANT_BIN = trigger/participant
BENCH_MPI_BIN = trigger/bench_mpi
BENCH_RDMA_BIN = trigger/bench_rdma
BENCH_IO_BIN = trigger/bench_io
BENCH_UNIFIED_BIN = trigger/bench_unified

# Default target
all: $(TARGET_LIB) $(TRIGGER_BIN) $(COORDINATOR_BIN) $(PARTICIPANT_BIN) $(BENCH_MPI_BIN) $(BENCH_RDMA_BIN) $(BENCH_IO_BIN) $(BENCH_UNIFIED_BIN)

# Ensure TPBench library is built first
$(TPBENCH_LIB):
	$(MAKE) -C $(TPBENCH_DIR) lib

# Rule to create the shared library for MPI trace
$(TARGET_LIB): $(OBJECTS) $(TPBENCH_LIB) | $(LIB_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

# Rule to compile instrumentation source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule to compile sync sources into object files
$(OBJ_DIR)/sync_%.o: sync/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule to build trigger binary
$(TRIGGER_BIN): trigger/trigger.cpp $(TARGET_LIB)
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ $< -L$(LIB_DIR) -lmpi_trace -lstdc++fs -lrdmacm -libverbs -lpthread -L$(TPBENCH_LIB_DIR) -ltpbench

# Rule to build coordinator binary
$(COORDINATOR_BIN): trigger/coordinator.cpp $(TARGET_LIB)
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ $< -L$(LIB_DIR) -lmpi_trace -lstdc++fs -lrdmacm -libverbs -lpthread -L$(TPBENCH_LIB_DIR) -ltpbench

# Rule to build participant binary
$(PARTICIPANT_BIN): trigger/participant.cpp trigger/bench_mpi.cpp trigger/bench_rdma.cpp trigger/bench_io.cpp trigger/bench.h $(TARGET_LIB)
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ trigger/participant.cpp trigger/bench_mpi.cpp trigger/bench_rdma.cpp trigger/bench_io.cpp -L$(LIB_DIR) -lmpi_trace $(LDFLAGS)

# Rule to build MPI benchmark binary
$(BENCH_MPI_BIN): trigger/bench_mpi.cpp trigger/bench.h
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ $< $(LDFLAGS)

# Rule to build RDMA benchmark binary
$(BENCH_RDMA_BIN): trigger/bench_rdma.cpp trigger/bench.h
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ $< -lrdmacm -libverbs -lpthread

# Rule to build IO benchmark binary
$(BENCH_IO_BIN): trigger/bench_io.cpp trigger/bench.h
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ $< -lpthread

# Rule to build unified benchmark binary
$(BENCH_UNIFIED_BIN): trigger/bench_unified.cpp trigger/bench_mpi.cpp trigger/bench_rdma.cpp trigger/bench_io.cpp trigger/bench.h
	$(CXX) -std=c++17 -O3 $(TPBENCH_INCLUDES) $(SYNC_INC) -o $@ trigger/bench_unified.cpp trigger/bench_mpi.cpp trigger/bench_rdma.cpp trigger/bench_io.cpp $(LDFLAGS)

# Create directories if they don't exist
$(OBJ_DIR) $(LIB_DIR):
	mkdir -p $@

# Clean rule (keep prebuilt libs in lib/)
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET_LIB)
	rm -f $(TRIGGER_BIN)
	rm -f $(COORDINATOR_BIN)
	rm -f $(PARTICIPANT_BIN)
	rm -f $(BENCH_MPI_BIN)
	rm -f $(BENCH_RDMA_BIN)
	rm -f $(BENCH_IO_BIN)
	rm -f $(BENCH_UNIFIED_BIN)

# Clean both wrapper and TPBench libraries
cleanall: clean
	$(MAKE) -C $(TPBENCH_DIR) clean

# Phony targets
.PHONY: all clean cleanall
