#pragma once
#include <cstdint>

namespace rdmasync {

// Initialize on-demand; no-op if env vars are not set
void init_if_needed(uint32_t mpi_rank=0);

// Finalize and clean up; no-op if not initialized/enabled
void finalize();

// Call at each point-to-point synchronization hook (e.g., at the very beginning of MPI_* wrappers)
void hook(const char* name);

} // namespace rdmasync

// Optional C API (convenient for C/LD_PRELOAD usage and C++ wrappers with C linkage)
extern "C" {
void rdmasync_init_if_needed();
void rdmasync_finalize();
void rdmasync_hook(const char* name);
}
