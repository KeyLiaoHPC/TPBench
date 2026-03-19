/**
 * @file tpb-rawdb-types.h
 * @brief Internal rawdb constants, magic signatures, and on-disk struct defs.
 */

#ifndef TPB_RAWDB_TYPES_H
#define TPB_RAWDB_TYPES_H

#include <stdint.h>
#include <limits.h>

/* Magic signature template: E1 54 50 42 <X> <Y> 31 E0 */
#define TPB_RAWDB_MAGIC_LEN     8
#define TPB_RAWDB_MAGIC_B0      0xE1
#define TPB_RAWDB_MAGIC_B1      0x54  /* 'T' */
#define TPB_RAWDB_MAGIC_B2      0x50  /* 'P' */
#define TPB_RAWDB_MAGIC_B3      0x42  /* 'B' */
#define TPB_RAWDB_MAGIC_B6      0x31  /* '1' */
#define TPB_RAWDB_MAGIC_B7      0xE0

/* X byte (byte 4): high nibble = file type, low nibble = domain */
#define TPB_RAWDB_FTYPE_ENTRY   0xE0
#define TPB_RAWDB_FTYPE_RECORD  0xD0
#define TPB_RAWDB_DOM_TBATCH    0x00
#define TPB_RAWDB_DOM_KERNEL    0x01
#define TPB_RAWDB_DOM_TASK      0x02

/* Y byte (byte 5): position mark */
#define TPB_RAWDB_POS_START     0x53  /* 'S' */
#define TPB_RAWDB_POS_SPLIT     0x44  /* 'D' */
#define TPB_RAWDB_POS_END       0x45  /* 'E' */

/* Entry size in bytes (all domains) */
#define TPB_RAWDB_ENTRY_SIZE    128

/* TBatch type values */
#define TPB_BATCH_TYPE_RUN       0
#define TPB_BATCH_TYPE_BENCHMARK 1

/* SHA1 digest size */
#define TPB_SHA1_DIGEST_LEN     20
#define TPB_SHA1_HEX_LEN        40

/* On-disk header size (fixed size with embedded dimension info) */
/* block_size(4) + ndim(4) + data_size(8) + type_bits(8) + name(256) + note(2048) + dimsizes[7](56) + dimnames[7][64](448) */
#define TPB_RAWDB_HDR_FIXED_SIZE 2832

/* Reserved block size in .tpbr meta section */
#define TPB_RAWDB_RESERVE_SIZE   64

/* Maximum workspace path length */
#define TPB_RAWDB_PATH_MAX       PATH_MAX

/* Default workspace directory name under $HOME */
#define TPB_RAWDB_DEFAULT_DIR    ".tpbench"
#define TPB_RAWDB_CONFIG_REL     "etc/config.json"
#define TPB_RAWDB_TBATCH_DIR     "rawdb/task_batch"
#define TPB_RAWDB_KERNEL_DIR     "rawdb/kernel"
#define TPB_RAWDB_TASK_DIR       "rawdb/task"
#define TPB_RAWDB_TBATCH_ENTRY   "task_batch.tpbe"
#define TPB_RAWDB_KERNEL_ENTRY   "kernel.tpbe"
#define TPB_RAWDB_TASK_ENTRY     "task.tpbe"

#endif /* TPB_RAWDB_TYPES_H */
