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
 *  @brief Type for kernel arguments token
 *  For CLI parsing: nkern=number of kernels (0 for common kargs), 
 *                   ntoken[i]=number of tokens for kernel i,
 *                   kname[i]=kernel names, token[j]=key=value pairs
 *  For kernel definition: nkern=1, ntoken[0]=number of supported params,
 *                        kname[0]=kernel name, 
 *                        token[j]="key=default_value=description"
 */
typedef struct tpb_k_arg_token {
    int nkern;
    int *ntoken;
    char **kname;
    char **token;
} tpb_k_arg_token_t;

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
    tpb_k_arg_token_t kargs_kernel;  // Kernel-specific arguments from -k
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
    tpb_k_arg_token_t kargs_def;    // Supported parameters definition (deprecated)
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
 * @brief General run-time arguments for kernels
 * @param ntest The number of repeat tests per kernel. This parameter will be used
 *              to set the number of steps of the outmost loop by default.
 *              Default: 10
 * @param nignore Ignore the first nignore tests.
 *                Default: 2
 * @param twarm The ms of warmup time.
 *              Default: 100
 * @param memsize The memory size of a kernel's working data set in KiB, including
 *                all of the intput, output and intermediate data. For a kernel with
 *                multiple arrays, the actual memory footprint will be rounded down
 *                to fit the nearest possible array settings. 
 *                1 KiB = 1024 Bytes = 8192 Bits.
 *                Default: 32 KiB
 */
typedef struct tpb_kargs_common {
    int ntest, nwarm, twarm;
    uint64_t memsize;
} tpb_kargs_common_t;

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
typedef uint64_t TPB_DTYPE;

// Parameter source flags (bits 24-31)
#define TPB_PARM_CLI        0x01000000  // Parameter can be set from CLI
#define TPB_PARM_MACRO      0x02000000  // Parameter can be set from macro (future use)
#define TPB_PARM_CONFIG     0x04000000  // Parameter can be set from config file (future use)
#define TPB_PARM_ENV        0x08000000  // Parameter can be set from environment (future use)

// Parameter validation/check mode flags (bits 16-23)
#define TPB_PARM_NOCHECK    0x00000000  // No validation
#define TPB_PARM_RANGE      0x00010000  // Check range [lo, hi]
#define TPB_PARM_LIST       0x00020000  // Check against list (count, array)
#define TPB_PARM_CUSTOM     0x00040000  // Custom validation function (future use)

// Parameter type flags (bits 0-15) - Aligned with MPI_Datatype encoding
// Type codes match MPI convention (lower 16 bits of MPI_* constants)
// From tpb-mpi_stub.h: MPI_INT8_T=0x4c000137, etc.
#define TPB_INT8_T          0x00000137  // int8_t - matches MPI_INT8_T
#define TPB_INT16_T         0x00000238  // int16_t - matches MPI_INT16_T
#define TPB_INT32_T         0x00000439  // int32_t - matches MPI_INT32_T
#define TPB_INT64_T         0x0000083a  // int64_t - matches MPI_INT64_T
#define TPB_UINT8_T         0x0000013b  // uint8_t - matches MPI_UINT8_T
#define TPB_UINT16_T        0x0000023c  // uint16_t - matches MPI_UINT16_T
#define TPB_UINT32_T        0x0000043d  // uint32_t - matches MPI_UINT32_T
#define TPB_UINT64_T        0x0000083e  // uint64_t - matches MPI_UINT64_T
#define TPB_FLOAT_T         0x0000040a  // float - matches MPI_FLOAT
#define TPB_DOUBLE_T        0x0000080b  // double - matches MPI_DOUBLE
#define TPB_LONG_DOUBLE_T   0x0000100c  // long double - matches MPI_LONG_DOUBLE
#define TPB_CHAR_T          0x00000101  // char - matches MPI_CHAR
#define TPB_STRING_T        0x00001000  // char* (string, TPBench extension)

// Mask for extracting flags
#define TPB_PARM_SOURCE_MASK    0xFF000000
#define TPB_PARM_CHECK_MASK     0x00FF0000
#define TPB_PARM_TYPE_MASK      0x0000FFFF

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
    TPB_DTYPE dtype;                // Data type with source, check, and type flags
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