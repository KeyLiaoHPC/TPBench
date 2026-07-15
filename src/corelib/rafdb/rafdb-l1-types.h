/**
 * @file rafdb-l1-types.h
 * @brief L1 rafdb constants, magic signatures, and path limits.
 */

#ifndef RAFDB_L1_TYPES_H
#define RAFDB_L1_TYPES_H

#include <stdint.h>
#include <limits.h>
#include "../../include/tpb-public.h"

/* Magic / position constants: TPB_RAF_MAGIC_* and TPB_RAF_POS_* in tpb-public.h */

#define TPB_RAF_ENTRY_SIZE    264

#define TPB_BATCH_TYPE_RUN       0
#define TPB_BATCH_TYPE_BENCHMARK 1

#define TPB_SHA1_DIGEST_LEN     20
#define TPB_SHA1_HEX_LEN        40

#define TPB_RAF_HDR_FIXED_SIZE 2840

#define TPB_RAF_KERNEL_ATTR_RESERVE  (TPB_RAF_RESERVE_SIZE + 52)

#define TPB_RAF_PATH_MAX       PATH_MAX

#define TPB_RAF_DEFAULT_DIR    ".tpbench"
#define TPB_RAF_CONFIG_REL     "etc/config.json"
#define TPB_RAF_TBATCH_DIR     "rafdb/task_batch"
#define TPB_RAF_KERNEL_DIR     "rafdb/kernel"
#define TPB_RAF_TASK_DIR       "rafdb/task"
#define TPB_RAF_LOG_REL        "rafdb/log"
#define TPB_RAF_TBATCH_ENTRY   "task_batch.tpbe"
#define TPB_RAF_KERNEL_ENTRY   "kernel.tpbe"
#define TPB_RAF_TASK_ENTRY     "task.tpbe"
#define TPB_RAF_RTENV_DIR      "rafdb/runtime_environment"
#define TPB_RAF_RTENV_ENTRY    "runtime_environment.tpbe"

/** @brief On-disk .tpbe row size for runtime_environment (bytes) */
#define TPB_RAF_RTENV_ENTRY_SIZE       1828
/** @brief On-disk .tpbr fixed meta size before headers (bytes) */
#define TPB_RAF_RTENV_RECORD_META_SIZE 1832

#endif /* RAFDB_L1_TYPES_H */
