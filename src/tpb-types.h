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
#include "tpb-unitdefs.h"

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
    int sigbit, intbit;     /* sigbit=significant figures, intbit=integer digits */
    int initialized;        /* Initialization flag */
} tpb_cliout_format_t;

#endif
