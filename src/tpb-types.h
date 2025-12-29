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
#define TPBM_PRTN_M_DIRECT 0x00     // Directly print, ignore headers.
#define TPBM_PRTN_M_TS 0x01         // Only print the timestamp header.
#define TPBM_PRTN_M_TAG 0x02        // Only print the tag header.
#define TPBM_PRTN_M_TSTAG 0x03      // Print both timestamp and tag headers.

#define TPBE_NOTE   0x00            // Tag: NOTE
#define TPBE_WARN   0x10            // Tag: WARN
#define TPBE_FAIL   0x20            // Tag: FAIL
#define TPBE_UNKN   0x30            // Tag: UNKN

typedef uint64_t TPB_DTYPE_U64;
typedef uint64_t TPB_MASK_U64;

// Parameter source flags (bits 24-31)
#define TPB_PARM_CLI        ((TPB_DTYPE_U64)0x01000000)  // Parameter from CLI
#define TPB_PARM_MACRO      ((TPB_DTYPE_U64)0x02000000)  // Parameter from macro
#define TPB_PARM_FILE       ((TPB_DTYPE_U64)0x04000000)  // Parameter from config file
#define TPB_PARM_ENV        ((TPB_DTYPE_U64)0x08000000)  // Parameter from env var

// Parameter validation/check mode flags (bits 16-23)
#define TPB_PARM_NOCHECK    ((TPB_DTYPE_U64)0x00000000)  // No check 
#define TPB_PARM_RANGE      ((TPB_DTYPE_U64)0x00010000)  // Check range [lo, hi]
#define TPB_PARM_LIST       ((TPB_DTYPE_U64)0x00020000)  // Check against list (count, *ptr)
#define TPB_PARM_CUSTOM     ((TPB_DTYPE_U64)0x00040000)  // Custom check pfunc

// Parameter type flags (bits 0-15) - Aligned with MPI_Datatype encoding
// Type codes match MPI convention (lower 16 bits of MPI_* constants)
// From tpb-mpi_stub.h: MPI_INT8_T=0x4c000137, etc.
#define TPB_INT8_T          ((TPB_DTYPE_U64)0x00000137)
#define TPB_INT16_T         ((TPB_DTYPE_U64)0x00000238)
#define TPB_INT32_T         ((TPB_DTYPE_U64)0x00000439)
#define TPB_INT64_T         ((TPB_DTYPE_U64)0x0000083a)
#define TPB_UINT8_T         ((TPB_DTYPE_U64)0x0000013b)
#define TPB_UINT16_T        ((TPB_DTYPE_U64)0x0000023c)
#define TPB_UINT32_T        ((TPB_DTYPE_U64)0x0000043d)
#define TPB_UINT64_T        ((TPB_DTYPE_U64)0x0000083e)
#define TPB_FLOAT_T         ((TPB_DTYPE_U64)0x0000040a)
#define TPB_DOUBLE_T        ((TPB_DTYPE_U64)0x0000080b)
#define TPB_LONG_DOUBLE_T   ((TPB_DTYPE_U64)0x0000100c)
#define TPB_CHAR_T          ((TPB_DTYPE_U64)0x00000101)
#define TPB_STRING_T        ((TPB_DTYPE_U64)0x00001000)

// Mask for extracting flags
#define TPB_PARM_SOURCE_MASK    ((TPB_MASK_U64)0xFF000000)  // Mask for parameter sources bit.
#define TPB_PARM_CHECK_MASK     ((TPB_MASK_U64)0x00FF0000)  // Mask for parameter check modes.
#define TPB_PARM_TYPE_MASK      ((TPB_MASK_U64)0x0000FFFF)  // Mask for parameter datatype.

/* The common metric unit */
// TPB_UNIT_MASK: The exponent to the base unit.
// TPB_UBASE_MASK: Mask for conversion of current unit from the base.
// TPB_UNIT_NAME_MASK: Mask for base name.
// 0x00: Undefined.
// 0x01: Data size
// TPB_UNIT_KIND_MASK: Mask for base conversion.
// 0x
// TPB_UNIT_RESV_MASK: Reserved bits.
#define TPB_URESV_MASK      ((TPB_MASK_U64)0xFF000000)
#define TPB_UKIND_MASK      ((TPB_MASK_U64)0x00F00000)
#define TPB_UKIND_UNDEF     ((TPB_MASK_U64)0x00000000)
#define TPB_UKIND_TIME      ((TPB_MASK_U64)0x00100000)
#define TPB_UKIND_VOL       ((TPB_MASK_U64)0x00200000)
#define TPB_UKIND_OPS       ((TPB_MASK_U64)0x00300000)

#define TPB_UNAME_MASK      ((TPB_MASK_U64)0x000FF000)
#define TPB_UNAME_UNDEF     ((TPB_MASK_U64)0x00000000)
#define TPB_UNAME_WALLTIME  (((TPB_MASK_U64)0x00001000) | TPB_UKIND_TIME)   // Wall-clock time
#define TPB_UNAME_PHYSTIME  (((TPB_MASK_U64)0x00002000) | TPB_UKIND_TIME)   // Chip physical time
#define TPB_UNAME_DATETIME  (((TPB_MASK_U64)0x00003000) | TPB_UKIND_TIME)   // Date time
#define TPB_UNAME_DATASIZE  (((TPB_MASK_U64)0x00001000) | TPB_UKIND_VOL)    // Data size
#define TPB_UNAME_OP        (((TPB_MASK_U64)0x00002000) | TPB_UKIND_VOL)    // Operation counts
#define TPB_UNAME_GRIDSIZE  (((TPB_MASK_U64)0x00003000) | TPB_UKIND_VOL)    // The number of grids.
#define TPB_UNAME_BITSIZE   (((TPB_MASK_U64)0x00004000) | TPB_UKIND_VOL)    // Binary bit size
#define TPB_UNAME_OPS       (((TPB_MASK_U64)0x00001000) | TPB_UKIND_OPS)    // Operation per second
#define TPB_UNAME_FLOPS     (((TPB_MASK_U64)0x00002000) | TPB_UKIND_OPS)    // Flop/s
#define TPB_UNAME_TOKENPS   (((TPB_MASK_U64)0x00003000) | TPB_UKIND_OPS)    // Token per second

#define TPB_UBASE_MASK      ((TPB_MASK_U64)0x00000F00)
#define TPB_UBASE_UNDEF     ((TPB_MASK_U64)0x00000000)
#define TPB_UBASE_BASE      ((TPB_MASK_U64)0x00000100)
#define TPB_UBASE_BIN_EXP_P ((TPB_MASK_U64)0x00000200)
#define TPB_UBASE_BIN_EXP_N ((TPB_MASK_U64)0x00000300)
#define TPB_UBASE_BIN_MUL_P ((TPB_MASK_U64)0x00000400)
#define TPB_UBASE_OCT_EXP_P ((TPB_MASK_U64)0x00000500)
#define TPB_UBASE_OCT_EXP_N ((TPB_MASK_U64)0x00000600)
#define TPB_UBASE_DEC_EXP_P ((TPB_MASK_U64)0x00000700)
#define TPB_UBASE_DEC_EXP_N ((TPB_MASK_U64)0x00000800)
#define TPB_UBASE_DEC_MUL_P ((TPB_MASK_U64)0x00000900)
#define TPB_UBASE_HEX_EXP_P ((TPB_MASK_U64)0x00000a00)
#define TPB_UBASE_HEX_EXP_N ((TPB_MASK_U64)0x00000b00)

#define TPB_UNIT_MASK       ((TPB_MASK_U64)0x000000FF)
#define TPB_UNIT_UNDEF      ((TPB_MASK_U64)0x000000FF)
// Bit size
#define TPB_UNIT_BIT       (((TPB_MASK_U64)0x00000000) | TPB_UNAME_BITSIZE | TPB_UBASE_BASE)
// Data size
#define TPB_UNIT_BYTE      (((TPB_MASK_U64)0x00000000) | TPB_UNAME_DATASIZE | TPB_UBASE_BASE)
#define TPB_UNIT_KIB       (((TPB_MASK_U64)0x0000000a) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_MIB       (((TPB_MASK_U64)0x00000014) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_GIB       (((TPB_MASK_U64)0x0000001e) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_TIB       (((TPB_MASK_U64)0x00000028) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_PIB       (((TPB_MASK_U64)0x00000032) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_EIB       (((TPB_MASK_U64)0x0000003c) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_ZIB       (((TPB_MASK_U64)0x00000046) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_YIB       (((TPB_MASK_U64)0x00000050) | TPB_UNAME_DATASIZE | TPB_UBASE_BIN_EXP_P)
#define TPB_UNIT_KB        (((TPB_MASK_U64)0x00000003) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MB        (((TPB_MASK_U64)0x00000006) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GB        (((TPB_MASK_U64)0x00000009) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TB        (((TPB_MASK_U64)0x0000000c) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PB        (((TPB_MASK_U64)0x0000000f) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_EB        (((TPB_MASK_U64)0x000000a2) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZB        (((TPB_MASK_U64)0x000000a5) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YB        (((TPB_MASK_U64)0x000000a8) | TPB_UNAME_DATASIZE | TPB_UBASE_DEC_EXP_P)
// Wall clock time based on nanosecond
#define TPB_UNIT_NS        (((TPB_MASK_U64)0x00000000) | TPB_UNAME_WALLTIME | TPB_UBASE_BASE)
#define TPB_UNIT_AS        (((TPB_MASK_U64)0x00000009) | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_N)
#define TPB_UNIT_FS        (((TPB_MASK_U64)0x00000006) | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_N)
#define TPB_UNIT_PS        (((TPB_MASK_U64)0x00000003) | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_N)
#define TPB_UNIT_US        (((TPB_MASK_U64)0x00000003) | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MS        (((TPB_MASK_U64)0x00000006) | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_SS        (((TPB_MASK_U64)0x00000009) | TPB_UNAME_WALLTIME | TPB_UBASE_DEC_EXP_P)
// Date time based on date second
#define TPB_UNIT_SE        (((TPB_MASK_U64)0x00000000) | TPB_UNAME_DATETIME | TPB_UBASE_BASE)
// Float-point operation per second
#define TPB_UNIT_FLOPS     (((TPB_MASK_U64)0x00000000) | TPB_UNAME_FLOPS | TPB_UBASE_BASE)
#define TPB_UNIT_KFLOPS    (((TPB_MASK_U64)0x00000003) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_MFLOPS    (((TPB_MASK_U64)0x00000006) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_GFLOPS    (((TPB_MASK_U64)0x00000009) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TFLOPS    (((TPB_MASK_U64)0x0000000c) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_PFLOPS    (((TPB_MASK_U64)0x0000000f) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_EFLOPS    (((TPB_MASK_U64)0x000000a2) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_ZFLOPS    (((TPB_MASK_U64)0x000000a5) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_YFLOPS    (((TPB_MASK_U64)0x000000a8) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)

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
    TPBE_KERN_NOT_FOUND
};
typedef enum _tpb_errno tpb_errno_t;
typedef struct _tpb_error {
    tpb_errno_t err_code;
    unsigned err_type;
    char err_msg[256];
} tpb_error_type;


/**
 * @brief tpbench run-time parameters
 */
typedef struct tpb_args {
    int mode; 
    char kstr[TPBM_CLI_STR_MAX_LEN];
    char kargstr[TPBM_CLI_STR_MAX_LEN];
    char data_dir[PATH_MAX]; // [Mandatory] group and kernels name
    char timer[TPBM_CLI_STR_MAX_LEN];
    int *klist; 
    int nkern;
    int list_only_flag; // [Optinal] flags for list mode and consecutive run
    struct tpb_rt_parm *common_rt_parms;  // Common runtime parameters
    int common_nparms;
    struct tpb_rt_parm **kernel_rt_parms;  // Runtime parameters for each kernel instance
    int *kernel_nparms;  // Number of parameters for each kernel instance
} tpb_args_t;

/**
 * @brief Metric-unit pair for performance reporting
 */
typedef struct tpb_k_metric {
    char *metric;  // e.g., "bandwidth", "throughput", "latency"
    char *unit;    // e.g., "GB/s", "GFLOPS", "ns"
} tpb_k_metric_t;

// Forward declarations
struct tpb_kernel;
struct tpb_timer;
struct tpb_rt_handle;
struct tpb_rt_parm;

/**
 * @brief Kernel function pointers
 */
typedef struct tpb_k_func {
    int (*kfunc_register)(struct tpb_kernel *kernel);  // Registration function (deprecated)
    int (*kfunc_run)(struct tpb_rt_handle *handle);    // Run function with runtime handle
} tpb_k_func_t;

/**
 * @brief Static kernel information
 */
typedef struct tpb_k_static_info {
    char *kname;                    // Kernel name
    char *note;                     // Kernel description/notes
    int rid;                        // Kernel routine id
    uint64_t nbyte;                 // Bytes through core per iteration
    uint64_t nop;                   // Arithmetic (FL)OPs per iteration
    tpb_k_metric_t metric_unit;     // Performance metric and unit
    struct tpb_rt_parm *parms;      // Runtime parameter definitions
    int nparms;                     // Number of parameters
    int ndim;                       // Number of dimensions in result array
} tpb_k_static_info_t;

/**
 * @brief Kernel structure with metadata and function pointers
 */
typedef struct tpb_kernel {
    tpb_k_static_info_t info;       // Static kernel information
    tpb_k_func_t func;              // Kernel functions
} tpb_kernel_t;

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

typedef struct tpb_timer {
    int (*init)(void);
    int (*tick)(int64_t *ts);
    int (*tock)(int64_t *ts);
    void (*get_stamp)(int64_t *ts);
} tpb_timer_t; 

// === Parameter Definition Type System ===
// Type definition for parameter data types
// Format: 0xSSCCTTTT where:
//   SS   = Parameter Source (bits 24-31)
//   CC   = Check Mode (bits 16-23)
//   TTTT = Type Code (bits 0-15)



/**
 * @brief Union for parameter values
 */
typedef union tpb_parm_value {
    int64_t i64;
    uint64_t u64;
    double f64;
    char *str;
} tpb_parm_value_t;

/**
 * @brief Runtime parameter definition
 */
typedef struct tpb_rt_parm {
    char *name;                     // Parameter name
    tpb_parm_value_t value;         // Parameter value
    tpb_parm_value_t default_value; // Default value
    char *description;              // Parameter description
    TPB_DTYPE_U64 dtype;                // Data type with source, check, and type flags
    // For validation (range or list)
    int nlims;                      // Number of limit values (2 for range, n for list)
    tpb_parm_value_t *plims;        // Limit values: [lo, hi] for range, [v1, v2, ..., vn] for list
} tpb_rt_parm_t;

/**
 * @brief Result package for kernel execution
 */
typedef struct tpb_respack {
    int64_t *time_arr;              // Array of timing results
    int ntest;                      // Number of tests
    size_t nbyte;                   // Bytes per iteration
    size_t nsize;                   // Number of elements
} tpb_respack_t;

/**
 * @brief Runtime handle passed to kernel runners
 */
typedef struct tpb_rt_handle {
    tpb_rt_parm_t *rt_parms;        // Array of runtime parameters
    int nparms;                     // Number of parameters
    tpb_timer_t *timer;             // Timer handle
    tpb_respack_t *respack;         // Result package
} tpb_rt_handle_t;

#endif
