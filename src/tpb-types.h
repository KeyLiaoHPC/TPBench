/** 
 * @file tpb-types.h
 * @brief Type definitions for TPBench.
 */
#ifndef TPB_TYPES_H
#define TPB_TYPES_H

#include <limits.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stddef.h>

#define TPBM_CLI_STR_MAX_LEN 4096
#define TPBM_NAME_STR_MAX_LEN 256
#define TPBM_NOTE_STR_MAX_LEN 2048
#define TPBM_CLI_K_MAX 128
#define TPBM_PRTN_M_DIRECT 0x00     // Directly print, ignore headers.
#define TPBM_PRTN_M_TS 0x01         // Only print the timestamp header.
#define TPBM_PRTN_M_TAG 0x02        // Only print the tag header.
#define TPBM_PRTN_M_TSTAG 0x03      // Print both timestamp and tag headers.

#define TPBE_NOTE   0x00            // Tag: NOTE
#define TPBE_WARN   0x10            // Tag: WARN
#define TPBE_FAIL   0x20            // Tag: FAIL
#define TPBE_UNKN   0x30            // Tag: UNKN

typedef uint64_t TPB_DTYPE;
typedef uint64_t TPB_MASK;
typedef uint64_t TPB_UNIT_T;
typedef uint32_t TPB_DTYPE_U32;

// Mask for extracting flags
// Type definition for parameter data types
// Format: 0xSSCCTTTT where:
//   SS   = Parameter Source (bits 24-31)
//   CC   = Check Mode (bits 16-23)
//   TTTT = Type Code (bits 0-15)
// Parameter source flags (bits 24-31)
#define TPB_PARM_SOURCE_MASK    ((TPB_MASK)0xFF000000)  // Mask for parameter sources bit.
#define TPB_PARM_CLI            ((TPB_DTYPE)0x01000000)  // Parameter from CLI
#define TPB_PARM_MACRO          ((TPB_DTYPE)0x02000000)  // Parameter from macro
#define TPB_PARM_FILE           ((TPB_DTYPE)0x04000000)  // Parameter from config file
#define TPB_PARM_ENV            ((TPB_DTYPE)0x08000000)  // Parameter from env var

// Parameter validation/check mode flags (bits 16-23)
#define TPB_PARM_CHECK_MASK ((TPB_MASK)0x00FF0000)  // Mask for parameter check modes.
#define TPB_PARM_NOCHECK    ((TPB_DTYPE)0x00000000)  // No check 
#define TPB_PARM_RANGE      ((TPB_DTYPE)0x00010000)  // Check range [lo, hi]
#define TPB_PARM_LIST       ((TPB_DTYPE)0x00020000)  // Check against list (count, *ptr)
#define TPB_PARM_CUSTOM     ((TPB_DTYPE)0x00040000)  // Custom check pfunc

// Parameter type flags (bits 0-15) - Aligned with MPI_Datatype encoding
// Type codes match MPI convention (lower 16 bits of MPI_* constants)
// From tpb-mpi_stub.h: MPI_INT8_T=0x4c000137, etc.
#define TPB_PARM_TYPE_MASK  ((TPB_MASK)0x0000FFFF)  // Mask for parameter datatype.
#define TPB_INT_T           ((TPB_DTYPE)0x00000405)
#define TPB_INT8_T          ((TPB_DTYPE)0x00000137)
#define TPB_INT16_T         ((TPB_DTYPE)0x00000238)
#define TPB_INT32_T         ((TPB_DTYPE)0x00000439)
#define TPB_INT64_T         ((TPB_DTYPE)0x0000083a)
#define TPB_UINT8_T         ((TPB_DTYPE)0x0000013b)
#define TPB_UINT16_T        ((TPB_DTYPE)0x0000023c)
#define TPB_UINT32_T        ((TPB_DTYPE)0x0000043d)
#define TPB_UINT64_T        ((TPB_DTYPE)0x0000083e)
#define TPB_FLOAT_T         ((TPB_DTYPE)0x0000040a)
#define TPB_DOUBLE_T        ((TPB_DTYPE)0x0000080b)
#define TPB_LONG_DOUBLE_T   ((TPB_DTYPE)0x0000100c)
#define TPB_CHAR_T          ((TPB_DTYPE)0x00000101)
#define TPB_STRING_T        ((TPB_DTYPE)0x00001000)
#define TPB_DTYPE_TIMER_T   ((TPB_DTYPE)0x0000083F)

/* === Common Metric Unit Encoding (TPB_UNIT_T - 64-bit) ===
 * Bit layout (from LSB to MSB):
 *   Bits 0-31  (0x00000000FFFFFFFF): Exponent OR Multiplier value
 *   Bits 32-35 (0x0000000F00000000): Base type (EXP vs MUL, BIN/DEC/OCT/HEX, P/N)
 *   Bits 36-43 (0x00000FF000000000): Unit name identifier
 *   Bits 44-47 (0x0000F00000000000): Unit kind (TIME, VOL, OPS)
 *   Bits 48-63 (0xFFFF000000000000): Reserved
 *
 * Value interpretation by base type:
 *   - *_EXP_P: scale = base^value (positive exponent)
 *   - *_EXP_N: scale = base^(-value) (negative exponent)
 *   - *_MUL_P: scale = value (direct multiplier)
 */

#define TPB_URESV_MASK          ((TPB_UNIT_T) 0xFFFF000000000000)
#define TPB_UKIND_MASK          ((TPB_UNIT_T) 0x0000F00000000000)
#define TPB_UKIND_UNDEF         ((TPB_UNIT_T) 0x0000000000000000)
#define TPB_UKIND_TIME          ((TPB_UNIT_T) 0x0000100000000000)
#define TPB_UKIND_VOL           ((TPB_UNIT_T) 0x0000200000000000)
#define TPB_UKIND_OPS           ((TPB_UNIT_T) 0x0000300000000000)

#define TPB_UNAME_MASK           ((TPB_UNIT_T)0x00000FF000000000)
#define TPB_UNAME_UNDEF          ((TPB_UNIT_T)0x0000000000000000)
#define TPB_UNAME_WALLTIME      (((TPB_UNIT_T)0x0000001000000000) | TPB_UKIND_TIME)   // Wall-clock time
#define TPB_UNAME_PHYSTIME      (((TPB_UNIT_T)0x0000002000000000) | TPB_UKIND_TIME)   // Chip physical time
#define TPB_UNAME_DATETIME      (((TPB_UNIT_T)0x0000003000000000) | TPB_UKIND_TIME)   // Date time
#define TPB_UNAME_UNKNTIME      (((TPB_UNIT_T)0x0000004000000000) | TPB_UKIND_TIME)   // Time unit following the timer
#define TPB_UNAME_DATASIZE      (((TPB_UNIT_T)0x0000001000000000) | TPB_UKIND_VOL)    // Data size
#define TPB_UNAME_OP            (((TPB_UNIT_T)0x0000002000000000) | TPB_UKIND_VOL)    // Operation counts
#define TPB_UNAME_GRIDSIZE      (((TPB_UNIT_T)0x0000003000000000) | TPB_UKIND_VOL)    // The number of grids.
#define TPB_UNAME_BITSIZE       (((TPB_UNIT_T)0x0000004000000000) | TPB_UKIND_VOL)    // Binary bit size
#define TPB_UNAME_OPS           (((TPB_UNIT_T)0x0000001000000000) | TPB_UKIND_OPS)    // Operation per second
#define TPB_UNAME_FLOPS         (((TPB_UNIT_T)0x0000002000000000) | TPB_UKIND_OPS)    // Flop/s
#define TPB_UNAME_TOKENPS       (((TPB_UNIT_T)0x0000003000000000) | TPB_UKIND_OPS)    // Token per second
#define TPB_UNAME_TPS           (((TPB_UNIT_T)0x0000004000000000) | TPB_UKIND_OPS)    // Transactions per second
// TPB_UBASE_BASE: This is a base unit.
// TPB_UBASE_<base_num>[BIN/DEC/...]_<convert_ops>[MUL/EXP]_<positive/nagative>[P/N]
#define TPB_UBASE_MASK          ((TPB_UNIT_T) 0x0000000F00000000)
#define TPB_UBASE_UNDEF         ((TPB_UNIT_T) 0x0000000000000000)
#define TPB_UBASE_BASE          ((TPB_UNIT_T) 0x0000000100000000)
#define TPB_UBASE_BIN_EXP_P     ((TPB_UNIT_T) 0x0000000200000000)
#define TPB_UBASE_BIN_EXP_N     ((TPB_UNIT_T) 0x0000000300000000)
#define TPB_UBASE_BIN_MUL_P     ((TPB_UNIT_T) 0x0000000400000000)
#define TPB_UBASE_OCT_EXP_P     ((TPB_UNIT_T) 0x0000000500000000)
#define TPB_UBASE_OCT_EXP_N     ((TPB_UNIT_T) 0x0000000600000000)
#define TPB_UBASE_DEC_EXP_P     ((TPB_UNIT_T) 0x0000000700000000)
#define TPB_UBASE_DEC_EXP_N     ((TPB_UNIT_T) 0x0000000800000000)
#define TPB_UBASE_DEC_MUL_P     ((TPB_UNIT_T) 0x0000000900000000)
#define TPB_UBASE_HEX_EXP_P     ((TPB_UNIT_T) 0x0000000a00000000)
#define TPB_UBASE_HEX_EXP_N     ((TPB_UNIT_T) 0x0000000b00000000)

#define TPB_UNIT_MASK           ((TPB_UNIT_T) 0x00000000FFFFFFFF)
#define TPB_UNIT_UNDEF          ((TPB_UNIT_T) 0x00000000FFFFFFFF)
// OP: The number of operations
#define TPB_UNIT_OP             (((TPB_UNIT_T)0)  | TPB_UNAME_OP | TPB_UBASE_BASE)
#define TPB_UNIT_KOP            (((TPB_UNIT_T)3)  | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MOP            (((TPB_UNIT_T)6)  | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GOP            (((TPB_UNIT_T)9)  | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TOP            (((TPB_UNIT_T)12) | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_POP            (((TPB_UNIT_T)15) | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_EOP            (((TPB_UNIT_T)18) | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZOP            (((TPB_UNIT_T)21) | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YOP            (((TPB_UNIT_T)24) | TPB_UNAME_OP | TPB_UBASE_DEC_EXP_P)
// Bit size
#define TPB_UNIT_BIT       (((TPB_UNIT_T)0)  | TPB_UNAME_BITSIZE | TPB_UBASE_BASE)
#define TPB_UNIT_BYTE      (((TPB_UNIT_T)3)  | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_KIB       (((TPB_UNIT_T)13) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_MIB       (((TPB_UNIT_T)23) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_GIB       (((TPB_UNIT_T)33) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_TIB       (((TPB_UNIT_T)43) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_PIB       (((TPB_UNIT_T)53) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_EIB       (((TPB_UNIT_T)63) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_ZIB       (((TPB_UNIT_T)73) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_YIB       (((TPB_UNIT_T)83) | TPB_UNAME_BITSIZE | TPB_UBASE_BIN_EXP_P)
// Data size
#define TPB_UNIT_B         (((TPB_UNIT_T)0)  | TPB_UNAME_DATASIZE | TPB_UBASE_BASE)
#define TPB_UNIT_KB        (((TPB_UNIT_T)3)  | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MB        (((TPB_UNIT_T)6)  | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GB        (((TPB_UNIT_T)9)  | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TB        (((TPB_UNIT_T)12) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PB        (((TPB_UNIT_T)15) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_EB        (((TPB_UNIT_T)18) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZB        (((TPB_UNIT_T)21) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YB        (((TPB_UNIT_T)24) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
// Wall clock time based on nanosecond
#define TPB_UNIT_NS        (((TPB_UNIT_T)0)  | TPB_UNAME_WALLTIME | TPB_UBASE_BASE)
#define TPB_UNIT_AS        (((TPB_UNIT_T)9)  | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_N)
#define TPB_UNIT_FS        (((TPB_UNIT_T)6)  | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_N)
#define TPB_UNIT_PS        (((TPB_UNIT_T)3)  | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_N)
#define TPB_UNIT_US        (((TPB_UNIT_T)3)  | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MS        (((TPB_UNIT_T)6)  | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_SS        (((TPB_UNIT_T)9)  | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_P)
// Physical tick/cycle
#define TPB_UNIT_CY        (((TPB_UNIT_T)0)  | TPB_UNAME_PHYSTIME | TPB_UBASE_BASE)
#define TPB_UNIT_KCY       (((TPB_UNIT_T)3)  | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MCY       (((TPB_UNIT_T)6)  | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GCY       (((TPB_UNIT_T)9)  | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TCY       (((TPB_UNIT_T)12) | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PCY       (((TPB_UNIT_T)15) | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ECY       (((TPB_UNIT_T)18) | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZCY       (((TPB_UNIT_T)21) | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YCY       (((TPB_UNIT_T)24) | TPB_UNAME_PHYSTIME | TPB_UBASE_DEC_EXP_P)
// Date time no macro-defined convert, !!!NOTE: Sidereal year, might be a little drifts to the calendar!!!
#define TPB_UNIT_SEC       (((TPB_UNIT_T)0)        | TPB_UNAME_DATETIME | TPB_UBASE_BASE)
#define TPB_UNIT_MIN       (((TPB_UNIT_T)60)       | TPB_UNAME_DATETIME | TPB_UBASE_DEC_MUL_P)
#define TPB_UNIT_HOU       (((TPB_UNIT_T)3600)     | TPB_UNAME_DATETIME | TPB_UBASE_DEC_MUL_P)
#define TPB_UNIT_DAY       (((TPB_UNIT_T)86400)    | TPB_UNAME_DATETIME | TPB_UBASE_DEC_MUL_P)
#define TPB_UNIT_MON       (((TPB_UNIT_T)2629846)  | TPB_UNAME_DATETIME | TPB_UBASE_DEC_MUL_P) // ~30.438days, calculated by S-year / 12 = 2629845.833
#define TPB_UNIT_YEA       (((TPB_UNIT_T)31558150) | TPB_UNAME_DATETIME | TPB_UBASE_DEC_MUL_P) // Sidereal year
// Time unit set by the timer
#define TPB_UNIT_TIMER     (((TPB_UNIT_T)0x00000000) | TPB_UNAME_UNKNTIME | TPB_UBASE_BASE)
// Operation per second
#define TPB_UNIT_OPS    (((TPB_UNIT_T)0)  | TPB_UNAME_OPS | TPB_UBASE_BASE)
#define TPB_UNIT_KOPS   (((TPB_UNIT_T)3)  | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MOPS   (((TPB_UNIT_T)6)  | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GOPS   (((TPB_UNIT_T)9)  | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TOPS   (((TPB_UNIT_T)12) | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_POPS   (((TPB_UNIT_T)15) | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_EOPS   (((TPB_UNIT_T)18) | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZOPS   (((TPB_UNIT_T)21) | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YOPS   (((TPB_UNIT_T)24) | TPB_UNAME_OPS | TPB_UBASE_DEC_EXP_P)
// Float-point operation per second
#define TPB_UNIT_FLOPS     (((TPB_UNIT_T)0)  | TPB_UNAME_FLOPS | TPB_UBASE_BASE)
#define TPB_UNIT_KFLOPS    (((TPB_UNIT_T)3)  | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MFLOPS    (((TPB_UNIT_T)6)  | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GFLOPS    (((TPB_UNIT_T)9)  | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TFLOPS    (((TPB_UNIT_T)12) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PFLOPS    (((TPB_UNIT_T)15) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_EFLOPS    (((TPB_UNIT_T)18) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZFLOPS    (((TPB_UNIT_T)21) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YFLOPS    (((TPB_UNIT_T)24) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
// Token per second
#define TPB_UNIT_TOKENPS     (((TPB_UNIT_T)0)  | TPB_UNAME_TOKENPS | TPB_UBASE_BASE)
#define TPB_UNIT_KTOKENPS    (((TPB_UNIT_T)3)  | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MTOKENPS    (((TPB_UNIT_T)6)  | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GTOKENPS    (((TPB_UNIT_T)9)  | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TTOKENPS    (((TPB_UNIT_T)12) | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PTOKENPS    (((TPB_UNIT_T)15) | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ETOKENPS    (((TPB_UNIT_T)18) | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZTOKENPS    (((TPB_UNIT_T)21) | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YTOKENPS    (((TPB_UNIT_T)24) | TPB_UNAME_TOKENPS | TPB_UBASE_DEC_EXP_P)
// Transactions per second
#define TPB_UNIT_TPS    (((TPB_UNIT_T)0)  | TPB_UNAME_TPS | TPB_UBASE_BASE)
#define TPB_UNIT_KTPS   (((TPB_UNIT_T)3)  | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MTPS   (((TPB_UNIT_T)6)  | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GTPS   (((TPB_UNIT_T)9)  | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TTPS   (((TPB_UNIT_T)12) | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PTPS   (((TPB_UNIT_T)15) | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ETPS   (((TPB_UNIT_T)18) | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZTPS   (((TPB_UNIT_T)21) | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YTPS   (((TPB_UNIT_T)24) | TPB_UNAME_TPS | TPB_UBASE_DEC_EXP_P)

// === Errors ===
/**
 * @brief Error codes for tpbench. Error has three levels:[NOTE], [WARNING], [FAIL].
 */
enum _tpb_errno {
    TPBE_SUCCESS = 0,
    TPBE_EXIT_ON_HELP,
    TPBE_CLI_FAIL,
    TPBE_FILE_IO_FAIL,
    TPBE_MALLOC_FAIL,
    TPBE_MPI_FAIL,
    TPBE_KERN_ARG_FAIL,
    TPBE_KERN_VERIFY_FAIL,
    TPBE_LIST_NOT_FOUND,
    TPBE_LIST_DUP,
    TPBE_NULLPTR_ARG,
    TPBE_DTYPE_NOT_SUPPORTED,
    TPBE_ILLEGAL_CALL
};
typedef enum _tpb_errno tpb_errno_t;
typedef struct _tpb_error {
    tpb_errno_t err_code;
    unsigned err_type;
    char err_msg[256];
} tpb_error_type;

typedef struct tpb_timer {
    char name[TPBM_NAME_STR_MAX_LEN];
    TPB_UNIT_T unit;
    TPB_DTYPE dtype;
    int (*init)(void);
    void (*tick)(int64_t *ts);
    void (*tock)(int64_t *ts);
    void (*get_stamp)(int64_t *ts);
} tpb_timer_t;

/**
 * @brief tpbench run-time parameters
 */
typedef struct tpb_args {
    int mode;
    char data_dir[PATH_MAX]; // [Mandatory] group and kernels name
    char timer_name[TPBM_CLI_STR_MAX_LEN];
    tpb_timer_t timer;
    int nkern;
    int list_only_flag; // [Optinal] flags for list mode and consecutive run
} tpb_args_t;

/**
 * @brief Metric-unit pair for performance reporting
 */
typedef struct tpb_k_metric {
    char *metric;  // e.g., "bandwidth", "throughput", "latency"
    char *unit;    // e.g., "GB/s", "GFLOPS", "ns"
} tpb_k_metric_t;

/**
 * @brief Union for parameter values
 */
typedef union tpb_parm_value {
    int64_t i64;
    uint64_t u64;
    float f32;
    double f64;
    char c;
} tpb_parm_value_t;

/**
 * @brief Runtime parameter definition
 */
typedef struct tpb_rt_parm {
    char name[TPBM_NAME_STR_MAX_LEN];   // Parameter name
    char note[TPBM_NOTE_STR_MAX_LEN];   // Parameter description
    tpb_parm_value_t value;             // Parameter value
    tpb_parm_value_t default_value;     // Default value
    TPB_DTYPE dtype;                    // Data type with source, check, and type flags
                                        // For validation (range or list)
    int nlims;                          // Number of limit values (2 for range, n for list)
    tpb_parm_value_t *plims;            // Limit values: [lo, hi] for range, [v1, v2, ..., vn] for list
} tpb_rt_parm_t;

typedef struct tpb_k_output {
    char name[TPBM_NAME_STR_MAX_LEN];
    char note[TPBM_NOTE_STR_MAX_LEN];
    TPB_DTYPE dtype;
    TPB_UNIT_T unit;
    int n;
    void *p;
} tpb_k_output_t;

/**
 * @brief Result package for kernel execution
 */
typedef struct tpb_respack {
    int n;
    tpb_k_output_t *outputs;
} tpb_respack_t;

typedef struct tpb_argpack {
    int n;
    tpb_rt_parm_t *args;
} tpb_argpack_t;

/**
 * @brief Static kernel information
 */
typedef struct tpb_k_static_info {
    char name[TPBM_NAME_STR_MAX_LEN];   // Kernel name
    char note[TPBM_NOTE_STR_MAX_LEN];   // Kernel description/notes
    int nparms, nouts;                   // Number of parameters
    tpb_rt_parm_t *parms;               // Runtime parameter definitions
    tpb_k_output_t *outs;            // Static result definitions
} tpb_k_static_info_t;

/**
 * @brief Kernel function pointers
 */
typedef struct tpb_k_func {
    int (*k_run)(void);                 // Run function with runtime handle
    int (*k_output_decorator)(void);    // Data processor of the output.
} tpb_k_func_t;

/**
 * @brief Kernel structure with metadata and function pointers
 */
typedef struct tpb_kernel {
    tpb_k_static_info_t info;       // Static kernel information
    tpb_k_func_t func;              // Kernel functions
} tpb_kernel_t;

/**
 * @brief Runtime handle used by kernel runners to access parameters, results, and kernel metadata.
 *
 * This structure is provided to every kernel runner and contains all runtime information needed 
 * to execute the kernel and store its results. It includes:
 *   - The intput runtime arguments (including parameter metadata and parsed user input values)
 *   - The result/output package for storing computation results
 *   - The associated kernel's static metadata and function pointers
 */
typedef struct tpb_k_rthdl {
    tpb_argpack_t argpack;     // Pointer to array of resolved runtime parameters
    tpb_respack_t respack;     // Pointer to result package (output storage)
    tpb_kernel_t kernel;       // Pointer to associated kernel metadata/functions
} tpb_k_rthdl_t;

/**
 * @brief tpbench result data file struct
 */
typedef struct tpb_res {
    char header[1024];
    char fname[1024];
    char fdir[PATH_MAX];
    char fpath[PATH_MAX];
    int64_t **data; //data[col][row], row for run id, col for different tests.
} tpb_res_t;

/* === CLI Output Format Controller (file-scoped) === */

/**
 * @brief CLI output format configuration structure.
 *        Controls terminal output formatting for kernel results.
 */
typedef struct tpb_cliout_format {
    int max_col;            /* Maximum terminal columns before wrapping */
    size_t nq;              /* Number of quantiles */
    double *qtiles;         /* Array of quantile positions */
    int sigbit, decbit;     /* Significant bit for unit conversion */
    int castctrl_autocast;  /* If number with more than dec bit should be casted. */
    int castctrl_same_unit; /* Casting to the same unit for the same UNAME */
    int initialized;        /* Initialization flag */
} tpb_cliout_format_t;

#endif
