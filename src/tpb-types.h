/** 
 * @file tpb-types.h
 * @brief Type definitions for TPBench.
 */
#ifndef TPB_TYPES_H
#define TPB_TYPES_H

#include <limits.h>
#include <linux/limits.h>
#include <stdint.h>

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
    TPBE_CLI_ARG_FAIL,
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

/**
 * @brief Kernel function pointers
 */
typedef struct tpb_k_func {
    int (*kfunc_register)(struct tpb_kernel *kernel);  // Registration function
    int (*kfunc_run)(void *args);                       // Run function with arguments
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
    tpb_k_arg_token_t kargs_def;    // Supported parameters definition
    tpb_k_metric_t metric_unit;     // Performance metric and unit
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

#endif