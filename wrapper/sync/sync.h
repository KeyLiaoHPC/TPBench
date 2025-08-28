#pragma once
#include <cstdint>

namespace rdmasync {

enum class TRIGGER_MSG {
    FINISH = 0,
    ONCE,
    REPEAT,
    STOP
};

// Initialize on-demand; no-op if env vars are not set
void init(uint32_t mpi_rank=0);

// Finalize and clean up; no-op if not initialized/enabled
void finalize();

// Call at each point-to-point synchronization tp_sync (e.g., at the very beginning of MPI_* wrappers)
void tp_sync(const char* name);

// Trigger message types for asymmetric communication
// Trigger function for asymmetric coordination
TRIGGER_MSG tp_trigger(TRIGGER_MSG send_msg);

bool is_enabled();

} // namespace rdmasync

// Optional C API (convenient for C/LD_PRELOAD usage and C++ wrappers with C linkage)
extern "C" {
void rdmasync_init(uint32_t mpi_rank);
void rdmasync_finalize();
void rdmasync_hook(const char* name);
int rdmasync_trigger(int send_msg);
int rdmasync_is_enabled();
}



