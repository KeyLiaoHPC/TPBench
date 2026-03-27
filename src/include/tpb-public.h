/**
 * @file tpb-public.h
 * @brief Public API header for TPBench kernel development.
 *
 * This is the single source of truth for all public types, macros, and
 * function declarations used by kernel code.  Internal corelib headers
 * include this file and add internal-only definitions on top.
 */

#ifndef TPB_PUBLIC_H
#define TPB_PUBLIC_H

#include <stdint.h>
#include <stddef.h>
#include "tpb-unitdefs.h"

typedef uint64_t TPB_DTYPE;
typedef uint64_t TPB_MASK;
typedef uint32_t TPB_DTYPE_U32;

/* Limits */
#define TPBM_CLI_STR_MAX_LEN 4096
#define TPBM_NAME_UNIX_MAX_LEN 64
#define TPBM_NAME_STR_MAX_LEN 256
#define TPBM_NOTE_STR_MAX_LEN 2048

/* Print modes */
#define TPBM_PRTN_M_DIRECT 0x00
#define TPBM_PRTN_M_TS     0x01
#define TPBM_PRTN_M_TAG    0x02
#define TPBM_PRTN_M_TSTAG  0x03

/* Error tags */
#define TPBE_NOTE   0x00
#define TPBE_WARN   0x10
#define TPBE_FAIL   0x20
#define TPBE_UNKN   0x30

/* Parameter source flags (bits 24-31) */
#define TPB_PARM_SOURCE_MASK    ((TPB_MASK)0xFF000000)      // The import source of paramters
#define TPB_PARM_CLI            ((TPB_DTYPE)0x01000000)     // Command-line parameters.
#define TPB_PARM_MACRO          ((TPB_DTYPE)0x02000000)     // Predefined macros, might trigger kernel rebuild.
#define TPB_PARM_WRAPPER_CLI    ((TPB_DTYPE)0x03000000)     // Parameters set to the wrapper/launcher of kernel execution, e.g. MPI.
#define TPB_PARM_FILE           ((TPB_DTYPE)0x04000000)     // Parameters from files.
#define TPB_PARM_ENV            ((TPB_DTYPE)0x08000000)     // Environmental variables.

/* Parameter validation/check mode flags (bits 16-23) */
#define TPB_PARM_CHECK_MASK ((TPB_MASK)0x00FF0000)
#define TPB_PARM_NOCHECK    ((TPB_DTYPE)0x00000000)
#define TPB_PARM_RANGE      ((TPB_DTYPE)0x00010000)
#define TPB_PARM_LIST       ((TPB_DTYPE)0x00020000)
#define TPB_PARM_CUSTOM     ((TPB_DTYPE)0x00040000)

/* Parameter type flags (bits 0-15) */
#define TPB_PARM_TYPE_MASK  ((TPB_MASK)0x0000FFFF)

/* 1-byte types (sorted by type bits) */
#define TPB_CHAR_T          ((TPB_DTYPE)0x00000101)
#define TPB_UNSIGNED_CHAR_T ((TPB_DTYPE)0x00000102)
#define TPB_BYTE_T          ((TPB_DTYPE)0x0000010d)
#define TPB_PACKED_T        ((TPB_DTYPE)0x0000010f)
#define TPB_SIGNED_CHAR_T   ((TPB_DTYPE)0x00000118)
#define TPB_INT8_T          ((TPB_DTYPE)0x00000137)
#define TPB_UINT8_T         ((TPB_DTYPE)0x0000013b)

/* 2-byte types (sorted by type bits) */
#define TPB_SHORT_T         ((TPB_DTYPE)0x00000201)
#define TPB_UNSIGNED_SHORT_T ((TPB_DTYPE)0x00000202)
#define TPB_INT16_T         ((TPB_DTYPE)0x00000238)
#define TPB_UINT16_T        ((TPB_DTYPE)0x0000023c)

/* 4-byte types (sorted by type bits) */
#define TPB_INT_T           ((TPB_DTYPE)0x00000405)
#define TPB_UNSIGNED_T      ((TPB_DTYPE)0x00000406)
#define TPB_FLOAT_T         ((TPB_DTYPE)0x0000040a)
#define TPB_WCHAR_T         ((TPB_DTYPE)0x0000040e)
#define TPB_INT32_T         ((TPB_DTYPE)0x00000439)
#define TPB_UINT32_T        ((TPB_DTYPE)0x0000043d)

/* 8-byte types (sorted by type bits) */
#define TPB_LONG_T          ((TPB_DTYPE)0x00000807)
#define TPB_UNSIGNED_LONG_T ((TPB_DTYPE)0x00000808)
#define TPB_LONG_LONG_T     ((TPB_DTYPE)0x00000809)
#define TPB_UNSIGNED_LONG_LONG_T ((TPB_DTYPE)0x00000819)
#define TPB_DOUBLE_T        ((TPB_DTYPE)0x0000080b)
#define TPB_C_FLOAT_COMPLEX_T ((TPB_DTYPE)0x00000823)
#define TPB_INT64_T         ((TPB_DTYPE)0x0000083a)
#define TPB_UINT64_T        ((TPB_DTYPE)0x0000083e)
#define TPB_DTYPE_TIMER_T   ((TPB_DTYPE)0x0000083F)

/* 16-byte types (sorted by type bits) */
#define TPB_STRING_T        ((TPB_DTYPE)0x00001000)
#define TPB_LONG_DOUBLE_T   ((TPB_DTYPE)0x0000100c)
#define TPB_C_DOUBLE_COMPLEX_T ((TPB_DTYPE)0x00001024)

/* 32-byte types */
#define TPB_C_LONG_DOUBLE_COMPLEX_T ((TPB_DTYPE)0x00002025)

/* Kernel integration types */
typedef uint32_t TPB_K_CTRL;
#define TPB_KTYPE_MASK      ((TPB_K_CTRL)0x0000000F)
#define TPB_KTYPE_PLI       ((TPB_K_CTRL)(1 << 1))

/* Error codes */
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
    TPBE_ILLEGAL_CALL,
    TPBE_KERNEL_NE_FAIL,
    TPBE_KARG_NE_FAIL,
    TPBE_KERNEL_INCOMPLETE,
    TPBE_DLOPEN_FAIL,
    TPBE_MERGE_MISMATCH,
    TPBE_MERGE_FAIL
};
typedef enum _tpb_errno tpb_errno_t;

/** @brief Process context that last completed tpb_corelib_init / tpb_k_corelib_init. */
typedef enum {
    TPB_CORELIB_CTX_CALLER_TPBCLI = 1,
    TPB_CORELIB_CTX_CALLER_KERNEL = 2
} tpb_corelib_caller_t;

/**
 * @brief Initialize TPBench corelib for this process: resolve workspace, create rawdb
 *        layout under that root, open the run log, and record caller as tpbcli.
 *        Call before any other corelib API that depends on workspace or logging.
 * @param tpb_workspace_path Optional workspace root override, or NULL to use $TPB_WORKSPACE then $HOME/.tpbench.
 * @return TPBE_SUCCESS, TPBE_ILLEGAL_CALL if already initialized, or another TPBE_* code.
 */
int tpb_corelib_init(const char *tpb_workspace_path);

/**
 * @brief Same as tpb_corelib_init but sets caller context to KERNEL after success (PLI .tpbx entry).
 * @param tpb_workspace_path Same as tpb_corelib_init.
 * @return Same as tpb_corelib_init.
 */
int tpb_k_corelib_init(const char *tpb_workspace_path);

/** @brief Timer structure */
typedef struct tpb_timer {
    char name[TPBM_NAME_STR_MAX_LEN];
    TPB_UNIT_T unit;
    TPB_DTYPE dtype;
    int (*init)(void);
    void (*tick)(int64_t *ts);
    void (*tock)(int64_t *ts);
    void (*get_stamp)(int64_t *ts);
} tpb_timer_t;

/** @brief Union for parameter values */
typedef union tpb_parm_value {
    int64_t i64;
    uint64_t u64;
    float f32;
    double f64;
    char c;
} tpb_parm_value_t;

/** @brief Runtime parameter definition */
typedef struct tpb_rt_parm {
    char name[TPBM_NAME_STR_MAX_LEN];
    char note[TPBM_NOTE_STR_MAX_LEN];
    tpb_parm_value_t value;
    TPB_DTYPE ctrlbits;
    int nlims;
    tpb_parm_value_t *plims;
} tpb_rt_parm_t;

/** @brief Kernel output definition */
typedef struct tpb_k_output {
    char name[TPBM_NAME_STR_MAX_LEN];
    char note[TPBM_NOTE_STR_MAX_LEN];
    TPB_DTYPE dtype;
    TPB_UNIT_T unit;
    int n;
    void *p;
} tpb_k_output_t;

/** @brief Result package for kernel execution */
typedef struct tpb_respack {
    int n;
    tpb_k_output_t *outputs;
} tpb_respack_t;

/** @brief Argument package for kernel execution */
typedef struct tpb_argpack {
    int n;
    tpb_rt_parm_t *args;
} tpb_argpack_t;

/** @brief Static kernel information */
typedef struct tpb_k_static_info {
    char name[TPBM_NAME_STR_MAX_LEN];
    char note[TPBM_NOTE_STR_MAX_LEN];
    unsigned char kernel_id[20];   /**< Resolved KernelID for this kernel */
    int kernel_record_ok;          /**< 1 if workspace kernel record update succeeded */
    TPB_K_CTRL kctrl;
    int nparms, nouts;
    tpb_rt_parm_t *parms;
    tpb_k_output_t *outs;
} tpb_k_static_info_t;

/** @brief Kernel function pointers */
typedef struct tpb_k_func {
    int (*k_output_decorator)(void);
} tpb_k_func_t;

/** @brief Kernel structure with metadata and function pointers */
typedef struct tpb_kernel {
    tpb_k_static_info_t info;
    tpb_k_func_t func;
} tpb_kernel_t;

/** @brief Environment variable entry */
typedef struct tpb_env_entry {
    char name[TPBM_NAME_STR_MAX_LEN];     /**< Environment variable name */
    char value[TPBM_CLI_STR_MAX_LEN];     /**< Environment variable value */
} tpb_env_entry_t;

/** @brief Environment variable package */
typedef struct tpb_envpack {
    int n;                     /**< Number of environment variables */
    tpb_env_entry_t *envs;     /**< Array of environment entries */
} tpb_envpack_t;

/** @brief MPI argument package */
typedef struct tpb_mpipack {
    char *mpiargs;             /**< MPI args string to pass to launcher as-is */
} tpb_mpipack_t;

/** @brief Runtime handle for kernel execution */
typedef struct tpb_k_rthdl {
    tpb_argpack_t argpack;
    tpb_envpack_t envpack;     /**< Environment variables for kernel */
    tpb_mpipack_t mpipack;     /**< MPI arguments for kernel (e.g., np=4) */
    tpb_respack_t respack;
    tpb_kernel_t kernel;
} tpb_k_rthdl_t;

/* ===== Kernel Registration API ===== */

/**
 * @brief Register a new kernel with a name and description.
 * @param name Kernel name (must be unique)
 * @param note Kernel description
 * @param kctrl Kernel attribution control bits
 * @return 0 on success, error code otherwise
 */
int tpb_k_register(const char *name, const char *note, TPB_K_CTRL kctrl);

/**
 * @brief Add a runtime parameter to the current kernel.
 *
 * The dtype parameter uses a 32-bit encoding: 0xSSCCTTTT
 *   - SS (bits 24-31): Parameter Source flags
 *   - CC (bits 16-23): Check/Validation mode flags
 *   - TTTT (bits 0-15): Type code (MPI-compatible)
 *
 * @param name Parameter name (used for CLI argument matching)
 * @param note Human-readable parameter description
 * @param default_val String representation of default value
 * @param dtype Combined data type: source | check | type
 * @param ... Variable arguments based on validation mode:
 *            - TPB_PARM_RANGE: (lo, hi) range bounds
 *            - TPB_PARM_LIST: (n, plist) valid values
 *            - TPB_PARM_NOCHECK: no additional arguments
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_parm(const char *name, const char *note,
                   const char *default_val, TPB_DTYPE dtype, ...);



/**
 * @brief Register a new output data definition for the current kernel.
 *
 * Must be called during kernel registration (after tpb_k_register,
 * before tpb_k_finalize_pli) to define output metrics.
 *
 * @param name   Output name (used to look up when allocating/reporting).
 * @param note   Human-readable description.
 * @param dtype  Data type of the output (TPB_INT64_T, TPB_DOUBLE_T, etc.).
 * @param unit   Unit type (TPB_UNIT_NS, TPB_UNIT_BYTE, etc.).
 * @return 0 on success, error code otherwise.
 */
int tpb_k_add_output(const char *name, const char *note, TPB_DTYPE dtype, TPB_UNIT_T unit);

/* ===== Kernel Runtime API ===== */

/**
 * @brief Get argument value from runtime handle.
 * @param name Parameter name
 * @param dtype Expected data type
 * @param argptr Pointer to receive parameter value
 * @return 0 on success, error code otherwise
 */
int tpb_k_get_arg(const char *name, TPB_DTYPE dtype, void *argptr);

/**
 * @brief Get timer for the current kernel.
 * @param timer Non-NULL pointer to receive timer via value-copy.
 * @return 0 on success, error code otherwise
 */
int tpb_k_get_timer(tpb_timer_t *timer);

/**
 * @brief Allocate memory for output data in the TPB framework.
 * @param name The name of an output variable.
 * @param n The number of elements with 'dtype' defined in tpb_k_add_output.
 * @param ptr Pointer to the header of allocated memory, NULL if failed.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_alloc_output(const char *name, uint64_t n, void *ptr);

/**
 * @brief Write a task record after kernel execution.
 *
 * Records kernel inputs and outputs to the raw database.
 * Reads TPB_TBATCH_ID / TPB_HANDLE_INDEX / TPB_WORKSPACE from env.
 * Outputs are written as 1-D arrays.
 *
 * @param hdl       Runtime handle with argpack and respack populated.
 * @param exit_code Kernel exit code (0 = success).
 * @return 0 on success, error code otherwise.
 */
int tpb_k_write_task(tpb_k_rthdl_t *hdl, int exit_code);

/* ===== PLI (Process-Level Integration) API ===== */

/**
 * @brief Finalize kernel registration.
 *
 * Increments the kernel count and completes registration. Called after all
 * parameters and outputs have been added via tpb_k_add_parm/tpb_k_add_output.
 *
 * @return 0 on success, error code otherwise.
 */
int tpb_k_finalize_pli(void);

/**
 * @brief Set timer for PLI kernel executable.
 *
 * Intended to be called from .tpbx executables. Sets up the timer based
 * on the provided timer name.
 *
 * @param timer_name Timer name (e.g., "clock_gettime", "tsc_asym")
 * @return 0 on success, error code otherwise.
 */
int tpb_k_pli_set_timer(const char *timer_name);

/**
 * @brief Build handle from positional arguments for PLI executable.
 *
 * Parses positional arguments (in parameter registration order) and
 * builds a runtime handle. Called from .tpbx executables.
 *
 * @param handle Pointer to runtime handle to populate.
 * @param argc Number of positional argument values.
 * @param argv Array of argument value strings.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_pli_build_handle(tpb_k_rthdl_t *handle, int argc, char **argv);

/* ===== Driver Cleanup ===== */

/**
 * @brief Clean up and free all memory in the kernel runtime handle.
 *
 * Releases all dynamic allocations made for output variables and kernel
 * arguments, ensuring no memory leaks after a kernel execution.
 *
 * @param handle Pointer to the kernel runtime handle.
 * @return 0 on successful cleanup, or an error code if any issues occurred.
 */
int tpb_driver_clean_handle(tpb_k_rthdl_t *handle);

/* ===== Kernel Runner API ===== */

/**
 * @brief Run a PLI kernel via fork/exec.
 *
 * Builds the execution command with environment variables, MPI arguments,
 * and kernel parameters, then launches the .tpbx executable via shell.
 * Captures and forwards stdout/stderr to both console and log file.
 *
 * @param hdl Runtime handle for the kernel (must be non-NULL).
 * @return 0 on success, error code otherwise.
 */
int tpb_run_pli(tpb_k_rthdl_t *hdl);

/* ===== CLI Output Helpers ===== */

/**
 * @brief TPBench formatted stdout.
 *
 * Output syntax: YYYY-mm-dd HH:MM:SS [TAG] *msg
 *
 * @param mode_bit Mode bit for message and error header type.
 * @param fmt Format string.
 * @param ... Varargs for fmt printf.
 */
void tpb_printf(uint64_t mode_bit, char *fmt, ...);

/**
 * @brief Output kernel arguments to the command-line interface.
 * @param handle Pointer to the kernel runtime handle.
 * @return TPBE_SUCCESS on success, TPBE_NULLPTR_ARG if handle is NULL.
 */
int tpb_cliout_args(tpb_k_rthdl_t *handle);

/**
 * @brief Output kernel execution results to the command-line interface.
 * @param handle Pointer to the kernel runtime handle.
 * @return TPBE_SUCCESS on success, TPBE_NULLPTR_ARG if handle is NULL.
 */
int tpb_cliout_results(tpb_k_rthdl_t *handle);

/* ===== Record Data Types (rawdb) ===== */

/** @brief Opaque reserve size in .tpbr meta and tail of .tpbe entry rows (bytes) */
#define TPB_RAWDB_RESERVE_SIZE 128

/** @brief 64-bit bit-packed datetime representation */
typedef uint64_t tpb_dtbits_t;

/** @brief Maximum number of dimensions for record data */
#define TPBM_DATA_NDIM_MAX 7

/**
 * @brief Metadata header describing one record data block
 *
 * Dimension layout: dimsizes[0] is the innermost dimension (d0),
 * dimsizes[ndim-1] is the outermost dimension.
 * For a 7D array: data[d6][d5][d4][d3][d2][d1][d0] where
 *   dimsizes[0]=d0, dimsizes[1]=d1, ..., dimsizes[6]=d6
 */
typedef struct tpb_meta_header {
    uint32_t block_size;                      /**< Header size on disk in bytes */
    uint32_t ndim;                            /**< Number of dimensions, in [0, TPBM_DATA_NDIM_MAX] */
    uint64_t data_size;                       /**< Record data size in bytes */
    uint32_t type_bits;                       /**< Data type control bits: TPB_PARM_SOURCE_MASK|TPB_PARM_CHECK_MASK|TPB_PARM_TYPE_MASK */
    uint32_t _reserve;                        /**< Reserved padding for alignment */
    uint64_t uattr_bits;                      /**< Metric unit encoding, aligned to TPB_UNIT_T */
    char name[TPBM_NAME_STR_MAX_LEN];       /**< Header name (256 bytes) */
    char note[TPBM_NOTE_STR_MAX_LEN];       /**< Notes and descriptions (2048 bytes) */
    uint64_t dimsizes[TPBM_DATA_NDIM_MAX];  /**< Dimension sizes: dimsizes[0]=innermost (d0) */
    char dimnames[TPBM_DATA_NDIM_MAX][64];  /**< Dimension names, parallel to dimsizes */
} tpb_meta_header_t;

/* TBatch type */
#define TPB_BATCH_TYPE_RUN       0
#define TPB_BATCH_TYPE_BENCHMARK 1

/** @brief TBatch full attributes (.tpbr meta section) */
typedef struct tbatch_attr {
    unsigned char tbatch_id[20];  /**< Primary Link ID (SHA-1) */
    unsigned char dup_to[20];     /**< Duplicate tracking */
    unsigned char dup_from[20];   /**< Lineage: source record ID, or zero */
    tpb_dtbits_t utc_bits;        /**< Batch start datetime */
    uint64_t btime;               /**< Boot time at batch start (ns) */
    uint64_t duration;            /**< Total batch duration (ns) */
    char hostname[64];            /**< Execution host name */
    char username[64];            /**< Username */
    uint32_t front_pid;           /**< Front-end process ID */
    uint32_t nkernel;             /**< Non-repeat kernels executed */
    uint32_t ntask;               /**< Task records in this batch */
    uint32_t nscore;              /**< Score records in this batch */
    uint32_t batch_type;          /**< 0=run, 1=benchmark */
    uint32_t nheader;             /**< Number of headers */
    tpb_meta_header_t *headers;   /**< Header array */
} tbatch_attr_t;

/** @brief TBatch entry (slim record in .tpbe) */
typedef struct tbatch_entry {
    unsigned char tbatch_id[20];  /**< TBatchID */
    unsigned char dup_from[20];   /**< Lineage: source TBatchID, or zero */
    tpb_dtbits_t start_utc_bits;  /**< Batch start datetime */
    uint64_t duration;            /**< Duration in nanoseconds */
    char hostname[64];            /**< Hostname */
    uint32_t nkernel;             /**< Number of kernels */
    uint32_t ntask;               /**< Number of tasks */
    uint32_t nscore;              /**< Number of scores */
    uint32_t batch_type;          /**< 0=run, 1=benchmark */
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE]; /**< Reserved */
} tbatch_entry_t;

/** @brief Kernel full attributes (.tpbr meta section) */
typedef struct kernel_attr {
    unsigned char kernel_id[20];  /**< KernelID (SHA-1) */
    unsigned char dup_to[20];     /**< Duplicate tracking */
    unsigned char dup_from[20];   /**< Lineage: source KernelID, or zero */
    unsigned char src_sha1[20];   /**< Source files SHA-1 */
    unsigned char so_sha1[20];    /**< Shared library SHA-1 */
    unsigned char bin_sha1[20];   /**< Executable SHA-1 */
    char kernel_name[256];        /**< Kernel name */
    char version[64];             /**< Version string */
    char description[2048];       /**< Description */
    uint32_t nparm;               /**< Registered parameters */
    uint32_t nmetric;             /**< Registered output metrics */
    uint32_t kctrl;               /**< Kernel control bits */
    uint32_t nheader;             /**< Number of headers */
    uint32_t reserve;             /**< Padding for alignment */
    tpb_meta_header_t *headers;   /**< Header array */
} kernel_attr_t;

/** @brief Kernel entry (slim record in .tpbe) */
typedef struct kernel_entry {
    unsigned char kernel_id[20];  /**< KernelID */
    unsigned char dup_from[20];   /**< Lineage: source KernelID, or zero */
    char kernel_name[64];         /**< Kernel name */
    unsigned char so_sha1[20];    /**< Shared library SHA-1 */
    uint32_t kctrl;               /**< Kernel control bits */
    uint32_t nparm;               /**< Number of parameters */
    uint32_t nmetric;             /**< Number of metrics */
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE]; /**< Reserved */
} kernel_entry_t;

/** @brief Task full attributes (.tpbr meta section) */
typedef struct task_attr {
    unsigned char task_record_id[20]; /**< TaskRecordID (SHA-1) */
    unsigned char dup_to[20];         /**< Duplicate tracking */
    unsigned char dup_from[20];       /**< Lineage: source TaskRecordID, or zero */
    unsigned char tbatch_id[20];      /**< Foreign key: TBatchID */
    unsigned char kernel_id[20];      /**< Foreign key: KernelID */
    tpb_dtbits_t utc_bits;            /**< Invocation datetime */
    uint64_t btime;                   /**< Boot time (ns) */
    uint64_t duration;                /**< Execution duration (ns) */
    uint32_t exit_code;               /**< Kernel exit code */
    uint32_t handle_index;            /**< Handle index (0-based) */
    uint32_t pid;                     /**< Writer process ID */
    uint32_t tid;                     /**< Writer thread ID */
    uint32_t ninput;                  /**< Input argument headers */
    uint32_t noutput;                 /**< Output metric headers */
    uint32_t nheader;                 /**< Total headers */
    uint32_t reserve;                 /**< Padding */
    tpb_meta_header_t *headers;       /**< Header array */
} task_attr_t;

/** @brief Task entry (slim record in .tpbe) */
typedef struct task_entry {
    unsigned char task_record_id[20]; /**< TaskRecordID */
    unsigned char dup_from[20];       /**< Lineage: source TaskRecordID, or zero */
    unsigned char tbatch_id[20];      /**< TBatchID */
    unsigned char kernel_id[20];      /**< KernelID */
    tpb_dtbits_t utc_bits;            /**< Invocation datetime */
    uint64_t duration;                /**< Duration (ns) */
    uint32_t exit_code;               /**< Exit code */
    uint32_t handle_index;            /**< Handle index */
    unsigned char dup_to[20];         /**< Merge target: merged TaskRecordID, or zero */
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE - 20]; /**< Reserved */
} task_entry_t;

/* ===== rawdb Workspace API ===== */

/**
 * @brief Copy the workspace path set by tpb_corelib_init into out_path.
 *
 * Requires a prior successful tpb_corelib_init (or tpb_k_corelib_init) in this process.
 * Optionally verifies the path exists as a directory.
 *
 * @param out_path Buffer to receive resolved path
 * @param pathlen  Buffer size
 * @return TPBE_SUCCESS, TPBE_ILLEGAL_CALL if corelib not initialized, or TPBE_FILE_IO_FAIL
 */
int tpb_rawdb_resolve_workspace(char *out_path, size_t pathlen);

/**
 * @brief Initialize workspace directory structure.
 *
 * Creates etc/config.json and rawdb/{task_batch,kernel,task,log}
 * directories if they do not exist.
 *
 * @param workspace_path Root workspace directory
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_init_workspace(const char *workspace_path);

/* ===== rawdb Magic API ===== */

/** @brief Entry file (.tpbe) type nibble for magic byte 4 (high nibble) */
#define TPB_RAWDB_FTYPE_ENTRY   ((uint8_t)0xE0)
/** @brief Record file (.tpbr) type nibble for magic byte 4 (high nibble) */
#define TPB_RAWDB_FTYPE_RECORD  ((uint8_t)0xD0)
/** @brief Task batch domain (magic byte 4 low nibble) */
#define TPB_RAWDB_DOM_TBATCH    ((uint8_t)0x00)
/** @brief Kernel domain (magic byte 4 low nibble) */
#define TPB_RAWDB_DOM_KERNEL    ((uint8_t)0x01)
/** @brief Task domain (magic byte 4 low nibble) */
#define TPB_RAWDB_DOM_TASK      ((uint8_t)0x02)

/**
 * @brief Build an 8-byte magic signature.
 * @param ftype  File type (TPB_RAWDB_FTYPE_ENTRY or _RECORD)
 * @param domain Domain (TPB_RAWDB_DOM_TBATCH/KERNEL/TASK)
 * @param pos    Position (TPB_RAWDB_POS_START/SPLIT/END)
 * @param out    Output buffer, at least 8 bytes
 */
void tpb_rawdb_build_magic(uint8_t ftype, uint8_t domain,
                           uint8_t pos, unsigned char out[8]);

/**
 * @brief Validate an 8-byte magic signature.
 * @param magic  8-byte buffer to check
 * @param ftype  Expected file type
 * @param domain Expected domain
 * @param pos    Expected position
 * @return 1 if valid, 0 if invalid
 */
int tpb_rawdb_validate_magic(const unsigned char magic[8],
                             uint8_t ftype, uint8_t domain,
                             uint8_t pos);

/**
 * @brief Scan a buffer for TPBench magic signatures.
 * @param buf     Buffer to scan
 * @param len     Buffer length
 * @param offsets Output array of found offsets
 * @param nfound  Output: number of magics found
 * @param max_results Maximum offsets to return
 * @return 0 on success
 */
int tpb_rawdb_magic_scan(const void *buf, size_t len,
                         size_t *offsets, int *nfound,
                         int max_results);

/**
 * @brief Detect rawdb file type and domain from the first 8-byte magic.
 * @param filepath Path to a .tpbe or .tpbr file
 * @param ftype_out Receives TPB_RAWDB_FTYPE_ENTRY or TPB_RAWDB_FTYPE_RECORD
 * @param domain_out Receives TPB_RAWDB_DOM_TBATCH, _KERNEL, or _TASK
 * @return 0 on success, TPBE_FILE_IO_FAIL if unreadable or invalid magic
 */
int tpb_rawdb_detect_file(const char *filepath,
                          uint8_t *ftype_out,
                          uint8_t *domain_out);

/* ===== rawdb Entry API (.tpbe) ===== */

/**
 * @brief Append a tbatch entry to the .tpbe file.
 * @param workspace Workspace root path
 * @param entry     Entry to append
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_entry_append_tbatch(const char *workspace,
                                  const tbatch_entry_t *entry);

/**
 * @brief Append a kernel entry to the .tpbe file.
 * @param workspace Workspace root path
 * @param entry     Entry to append
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_entry_append_kernel(const char *workspace,
                                  const kernel_entry_t *entry);

/**
 * @brief Append a task entry to the .tpbe file.
 * @param workspace Workspace root path
 * @param entry     Entry to append
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_entry_append_task(const char *workspace,
                                const task_entry_t *entry);

/**
 * @brief List all tbatch entries from the .tpbe file.
 * @param workspace Workspace root path
 * @param entries   Output: allocated array (caller must free)
 * @param count     Output: number of entries
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_entry_list_tbatch(const char *workspace,
                                tbatch_entry_t **entries,
                                int *count);

/**
 * @brief List all kernel entries from the .tpbe file.
 * @param workspace Workspace root path
 * @param entries   Output: allocated array (caller must free)
 * @param count     Output: number of entries
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_entry_list_kernel(const char *workspace,
                                kernel_entry_t **entries,
                                int *count);

/**
 * @brief List all task entries from the .tpbe file.
 * @param workspace Workspace root path
 * @param entries   Output: allocated array (caller must free)
 * @param count     Output: number of entries
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_entry_list_task(const char *workspace,
                              task_entry_t **entries,
                              int *count);

/* ===== rawdb Record API (.tpbr) ===== */

/**
 * @brief Write a tbatch .tpbr record file.
 * @param workspace Workspace root path
 * @param attr      TBatch attributes with headers populated
 * @param data      Record data buffer (may be NULL if datasize==0)
 * @param datasize  Size of record data in bytes
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_record_write_tbatch(const char *workspace,
                                  const tbatch_attr_t *attr,
                                  const void *data,
                                  uint64_t datasize);

/**
 * @brief Read a tbatch .tpbr record file.
 * @param workspace Workspace root path
 * @param tbatch_id 20-byte TBatchID
 * @param attr      Output attributes (headers allocated; free with tpb_rawdb_free_headers)
 * @param data      Output data buffer (caller must free, may be NULL)
 * @param datasize  Output data size
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_record_read_tbatch(const char *workspace,
                                 const unsigned char tbatch_id[20],
                                 tbatch_attr_t *attr,
                                 void **data,
                                 uint64_t *datasize);

/**
 * @brief Write a kernel .tpbr record file.
 * @param workspace Workspace root path
 * @param attr      Kernel attributes with headers populated
 * @param data      Record data buffer
 * @param datasize  Size of record data in bytes
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_record_write_kernel(const char *workspace,
                                  const kernel_attr_t *attr,
                                  const void *data,
                                  uint64_t datasize);

/**
 * @brief Read a kernel .tpbr record file.
 * @param workspace Workspace root path
 * @param kernel_id 20-byte KernelID
 * @param attr      Output attributes
 * @param data      Output data buffer (caller must free)
 * @param datasize  Output data size
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_record_read_kernel(const char *workspace,
                                 const unsigned char kernel_id[20],
                                 kernel_attr_t *attr,
                                 void **data,
                                 uint64_t *datasize);

/**
 * @brief Write a task .tpbr record file.
 * @param workspace Workspace root path
 * @param attr      Task attributes with headers populated
 * @param data      Record data buffer
 * @param datasize  Size of record data in bytes
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_record_write_task(const char *workspace,
                                const task_attr_t *attr,
                                const void *data,
                                uint64_t datasize);

/**
 * @brief Read a task .tpbr record file.
 * @param workspace Workspace root path
 * @param task_id   20-byte TaskRecordID
 * @param attr      Output attributes
 * @param data      Output data buffer (caller must free)
 * @param datasize  Output data size
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_record_read_task(const char *workspace,
                               const unsigned char task_id[20],
                               task_attr_t *attr,
                               void **data,
                               uint64_t *datasize);

/**
 * @brief Free header array allocated by record read functions.
 * @param headers Header array
 * @param nheader Number of headers
 */
void tpb_rawdb_free_headers(tpb_meta_header_t *headers,
                            uint32_t nheader);

/* ===== rawdb ID Generation ===== */

/**
 * @brief Generate TBatchID via SHA1.
 * @param utc_bits  Batch start datetime
 * @param btime     Boot time in nanoseconds
 * @param hostname  Hostname string
 * @param username  Username string
 * @param front_pid Front-end process ID
 * @param id_out    20-byte output buffer
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_gen_tbatch_id(tpb_dtbits_t utc_bits,
                            uint64_t btime,
                            const char *hostname,
                            const char *username,
                            uint32_t front_pid,
                            unsigned char id_out[20]);

/**
 * @brief Generate KernelID via SHA1.
 * @param kernel_name Kernel name string
 * @param so_sha1     20-byte SO hash
 * @param bin_sha1    20-byte binary hash
 * @param id_out      20-byte output buffer
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_gen_kernel_id(const char *kernel_name,
                            const unsigned char so_sha1[20],
                            const unsigned char bin_sha1[20],
                            unsigned char id_out[20]);

/**
 * @brief Generate TaskRecordID via SHA1.
 * @param utc_bits   Invocation datetime
 * @param btime      Boot time in nanoseconds
 * @param hostname   Hostname string
 * @param username   Username string
 * @param tbatch_id  20-byte TBatchID
 * @param kernel_id  20-byte KernelID
 * @param order      Order in batch (0-based)
 * @param pid        Writer process ID
 * @param tid        Writer thread ID
 * @param id_out     20-byte output buffer
 * @return 0 on success, error code otherwise
 */
int tpb_rawdb_gen_task_id(tpb_dtbits_t utc_bits,
                          uint64_t btime,
                          const char *hostname,
                          const char *username,
                          const unsigned char tbatch_id[20],
                          const unsigned char kernel_id[20],
                          uint32_t order,
                          uint32_t pid,
                          uint32_t tid,
                          unsigned char id_out[20]);

/**
 * @brief Convert 20-byte ID to 40-char hex string.
 * @param id  20-byte ID
 * @param hex Output buffer, at least 41 bytes (40 hex + NUL)
 */
void tpb_rawdb_id_to_hex(const unsigned char id[20],
                         char hex[41]);

/**
 * @brief Parse exactly 40 hex digits into a 20-byte ID.
 * @param hex 40 hex digits [0-9A-Fa-f], no prefix, no spaces
 * @param id  Output 20-byte buffer
 * @return 0 on success, TPBE_NULLPTR_ARG or TPBE_CLI_FAIL on error
 */
int tpb_rawdb_hex_to_id(const char *hex, unsigned char id[20]);

/**
 * @brief Find which domain directory contains a .tpbr for the given ID.
 * @param workspace Workspace root
 * @param id 20-byte record ID
 * @param domain_out Receives TPB_RAWDB_DOM_TBATCH, _KERNEL, or _TASK
 * @return 0 on success, TPBE_FILE_IO_FAIL if no matching file
 */
int tpb_rawdb_find_record(const char *workspace,
                          const unsigned char id[20],
                          uint8_t *domain_out);

/* ===== Task Record Merge API ===== */

/**
 * @brief Merge multiple task records from threads within one process.
 *
 * Combines per-thread task records into a single merged record.
 * Source records must share the same tbatch_id and kernel_id.
 * Adds SourceTaskIDs and ThreadIDs headers to the merged record.
 *
 * @param task_ids     Array of 20-byte source task IDs
 * @param n_tasks      Number of source tasks (must be >= 2)
 * @param merged_id_out Output: 20-byte merged task ID
 * @return TPBE_SUCCESS on success,
 *         TPBE_MERGE_MISMATCH or TPBE_MERGE_FAIL on error
 */
int tpb_k_merge_record_thread(const unsigned char task_ids[][20],
                              int n_tasks,
                              unsigned char merged_id_out[20]);

/**
 * @brief Merge multiple task records from processes (possibly multi-node).
 *
 * Combines per-process task records into a single merged record.
 * Source records must share the same tbatch_id and kernel_id.
 * Adds SourceTaskIDs, ThreadIDs, ProcessIDs, and Hosts headers
 * to the merged record.
 *
 * @param task_ids     Array of 20-byte source task IDs
 * @param n_tasks      Number of source tasks (must be >= 2)
 * @param merged_id_out Output: 20-byte merged task ID
 * @return TPBE_SUCCESS on success,
 *         TPBE_MERGE_MISMATCH or TPBE_MERGE_FAIL on error
 */
int tpb_k_merge_record_process(const unsigned char task_ids[][20],
                               int n_tasks,
                               unsigned char merged_id_out[20]);

/* ===== Kernel Query API ===== */

/**
 * @brief Query kernel information by ID or name.
 *
 * Returns the total number of registered kernels. If kernel_out is non-NULL,
 * attempts to look up a kernel and allocate a fully isolated copy.
 *
 * @param id Kernel index (>=0). If id >= 0, kernel_name is ignored.
 *           If id < 0, kernel_name is used to look up the kernel.
 * @param kernel_name Kernel name (used only when id < 0). Can be NULL if id >= 0.
 * @param kernel_out Pointer to a NULL tpb_kernel_t* pointer. On success,
 *                   *kernel_out will point to an allocated kernel copy.
 *                   On failure or if kernel_out is NULL, *kernel_out is unchanged.
 *                   Caller must free with tpb_free_kernel().
 * @return Total number of registered kernels. Check *kernel_out to determine
 *         if the specific kernel lookup succeeded (non-NULL) or failed (NULL).
 */
int tpb_query_kernel(int id, const char *kernel_name, tpb_kernel_t **kernel_out);

/**
 * @brief Free memory allocated by tpb_query_kernel().
 *
 * Frees all nested structures (parms, plims, outs) within the kernel instance.
 * Note: This does NOT free the kernel struct itself.
 *
 * For heap-allocated kernel from tpb_query_kernel:
 *   tpb_free_kernel(kernel);
 *   free(kernel);
 *
 * For inline kernel struct (like hdl->kernel):
 *   tpb_free_kernel(&hdl->kernel);
 *
 * @param kernel Kernel instance to clean up. Can be NULL (no-op).
 */
void tpb_free_kernel(tpb_kernel_t *kernel);

#endif /* TPB_PUBLIC_H */

