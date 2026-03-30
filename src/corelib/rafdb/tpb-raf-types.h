/**
 * @file tpb-raf-types.h
 * @brief Internal rafdb constants, magic signatures, and on-disk struct defs.
 */

#ifndef TPB_RAF_TYPES_H
#define TPB_RAF_TYPES_H

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

/* X byte (byte 4): high nibble = file type, low nibble = domain
 * Values TPB_RAF_FTYPE_* / TPB_RAF_DOM_* are in tpb-public.h */

/* Y byte (byte 5): position mark */
#define TPB_RAF_POS_START     0x53  /* 'S' */
#define TPB_RAF_POS_SPLIT     0x44  /* 'D' */
#define TPB_RAF_POS_END       0x45  /* 'E' */

/* Max .tpbe row size in bytes (all domains; task includes tail alignment) */
#define TPB_RAF_ENTRY_SIZE    264

/* TBatch type values */
#define TPB_BATCH_TYPE_RUN       0
#define TPB_BATCH_TYPE_BENCHMARK 1

/* SHA1 digest size */
#define TPB_SHA1_DIGEST_LEN     20
#define TPB_SHA1_HEX_LEN        40

/* On-disk header size (fixed size with embedded dimension info) */
/* block_size(4) + ndim(4) + data_size(8) + type_bits(4) + _reserve(4) + uattr_bits(8) + name(256) + note(2048) + dimsizes[7](56) + dimnames[7][64](448) */
#define TPB_RAF_HDR_FIXED_SIZE 2840

/* TPB_RAF_RESERVE_SIZE is in tpb-public.h (shared with entry tail reserve) */

/* Maximum workspace path length */
#define TPB_RAF_PATH_MAX       PATH_MAX

/* Default workspace directory name under $HOME */
#define TPB_RAF_DEFAULT_DIR    ".tpbench"
#define TPB_RAF_CONFIG_REL     "etc/config.json"
#define TPB_RAF_TBATCH_DIR     "rafdb/task_batch"
#define TPB_RAF_KERNEL_DIR     "rafdb/kernel"
#define TPB_RAF_TASK_DIR       "rafdb/task"
#define TPB_RAF_LOG_REL        "rafdb/log"
#define TPB_RAF_TBATCH_ENTRY   "task_batch.tpbe"
#define TPB_RAF_KERNEL_ENTRY   "kernel.tpbe"
#define TPB_RAF_TASK_ENTRY     "task.tpbe"

#endif /* TPB_RAF_TYPES_H */
