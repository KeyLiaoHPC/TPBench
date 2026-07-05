/**
 * @file rafdb-l1-types.h
 * @brief L1 rafdb constants, magic signatures, and path limits.
 */

#ifndef RAFDB_L1_TYPES_H
#define RAFDB_L1_TYPES_H

#include <stdint.h>
#include <limits.h>
#include "../../include/tpb-public.h"

/* Magic signature template: E1 54 50 42 <X> <Y> 31 E0 */
#define TPB_RAF_MAGIC_LEN     8
#define TPB_RAF_MAGIC_B0      0xE1
#define TPB_RAF_MAGIC_B1      0x54  /* 'T' */
#define TPB_RAF_MAGIC_B2      0x50  /* 'P' */
#define TPB_RAF_MAGIC_B3      0x42  /* 'B' */
#define TPB_RAF_MAGIC_B6      0x31  /* '1' */
#define TPB_RAF_MAGIC_B7      0xE0

#define TPB_RAF_POS_START     0x53  /* 'S' */
#define TPB_RAF_POS_SPLIT     0x44  /* 'D' */
#define TPB_RAF_POS_END       0x45  /* 'E' */

#define TPB_RAF_ENTRY_SIZE    264

#define TPB_BATCH_TYPE_RUN       0
#define TPB_BATCH_TYPE_BENCHMARK 1

#define TPB_SHA1_DIGEST_LEN     20
#define TPB_SHA1_HEX_LEN        40

#define TPB_RAF_HDR_FIXED_SIZE 2840

#define TPB_RAF_KERNEL_ATTR_RESERVE  (TPB_RAF_RESERVE_SIZE + 60)

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

#endif /* RAFDB_L1_TYPES_H */
