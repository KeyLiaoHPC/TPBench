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
#include <stdio.h>
#include "tpb-unitdefs.h"

typedef uint64_t TPB_DTYPE;
typedef uint64_t TPB_MASK;
typedef uint32_t TPB_DTYPE_U32;

/* Limits */
#define TPBM_CLI_STR_MAX_LEN 4096
#define TPBM_NAME_UNIX_MAX_LEN 64
#define TPBM_NAME_STR_MAX_LEN 256
/** Max characters of user-supplied tag text (excludes trailing NUL). */
#define TPBM_TAG_USER_MAX_LEN 191
#define TPBM_NOTE_STR_MAX_LEN 2048

/** Shared-library filename suffix for PLI kernels (libtpbk_<name>TPB_SHLIB_EXT). */
#ifdef __APPLE__
#define TPB_SHLIB_EXT ".dylib"
#else
#define TPB_SHLIB_EXT ".so"
#endif

/** System role tag appended to kernel argument headers. */
#define TPB_TAG_ARG  "TPBARG"
/** System role tag appended to kernel output headers. */
#define TPB_TAG_OUT  "TPBOUT"
/** Internal linkage tag for cross-record ID arrays. */
#define TPB_TAG_LINK "TPBLINK"

/* tpblog line tag types; rendered as [INFO], [WARN], or [ERRO] */
#define TPB_LOG_TAG_NOTE  0x00U
#define TPB_LOG_TAG_WARN  0x10U
#define TPB_LOG_TAG_FAIL  0x20U
#define TPB_LOG_TAG_UNKN  0x30U

/** @brief tpblog verbosity threshold. Messages below the active level are suppressed. */
typedef enum {
    TPB_LOG_LEVEL_TRACE = 0,
    TPB_LOG_LEVEL_DEBUG,
    TPB_LOG_LEVEL_INFO,
    TPB_LOG_LEVEL_WARN,
    TPB_LOG_LEVEL_ERROR,
    TPB_LOG_LEVEL_NONE
} tpb_log_level_t;

/** @brief tpblog print flags (timestamp and/or tag prefix). */
#define TPBLOG_FLAG_DIRECT 0x00U
#define TPBLOG_FLAG_TS     0x01U
#define TPBLOG_FLAG_TAG    0x02U
#define TPBLOG_FLAG_TSTAG  (TPBLOG_FLAG_TS | TPBLOG_FLAG_TAG)

#define TPBLOG_TYPE_INFO  TPB_LOG_TAG_NOTE
#define TPBLOG_TYPE_WARN  TPB_LOG_TAG_WARN
#define TPBLOG_TYPE_ERRO  TPB_LOG_TAG_FAIL

/** @brief Environment variable for deterministic column-width tests. */
#define TPBLOG_TEST_WIDTH_ENV "TPBLOG_TEST_WIDTH"

/** @brief Thin wrapper around snprintf for column cell pre-formatting. */
#define tpblog_snprintf(buf, bufsz, fmt, ...) \
    snprintf((buf), (bufsz), (fmt), ##__VA_ARGS__)

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
/* Legacy names; same value as TPB_KTYPE_PLI (no separate integration mode). */
#define TPB_KTYPE_FLI       TPB_KTYPE_PLI
#define TPB_KTYPE_ALI       TPB_KTYPE_PLI

/* Error codes (cause field, low 8 bits of encoded return value) */
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
    TPBE_METRIC_MISSING
};
typedef enum _tpb_errno tpb_errno_t;

/* Module codes (high byte of encoded return value) */
enum _tpb_module {
    TPB_MOD_NONE = 0,
    TPB_MOD_DRIVER,
    TPB_MOD_ARGP,
    TPB_MOD_IMPL,
    TPB_MOD_IO,
    TPB_MOD_MISC,
    TPB_MOD_RAF_L1,
    TPB_MOD_RAF_L2_KERNEL,
    TPB_MOD_RAF_L2_TASK,
    TPB_MOD_RAF_L2_TBATCH,
    TPB_MOD_RAF_L2_RTENV,
    TPB_MOD_RAF_L3_KERNEL,
    TPB_MOD_RAF_L3_TASK,
    TPB_MOD_RAF_L3_TBATCH,
    TPB_MOD_RAF_L3_RTENV,
    TPB_MOD_RAF_MISC,
    TPB_MOD_CLI_RUN,
    TPB_MOD_CLI_BENCHMARK,
    TPB_MOD_CLI_KERNEL,
    TPB_MOD_CLI_MISC,
    TPB_MOD_CLI_RTENV
};
typedef enum _tpb_module tpb_module_t;

#define TPBE_CAUSE_MASK 0x000000FFu
#define TPBE_MOD_SHIFT  8
#define TPBE_MOD_MASK   0x0000FF00u
#define TPBE_MAKE(mod, cause) \
    ((int)(((unsigned)(mod) << TPBE_MOD_SHIFT) \
           | ((unsigned)(cause) & TPBE_CAUSE_MASK)))
#define TPBE_CAUSE(err) \
    ((int)((unsigned)(err) & TPBE_CAUSE_MASK))
#define TPBE_MODULE(err) \
    ((int)(((unsigned)(err) & TPBE_MOD_MASK) >> TPBE_MOD_SHIFT))
#define TPBE_IS_ENCODED(err) \
    ((err) != 0 && ((unsigned)(err) & TPBE_MOD_MASK) != 0)

/**
 * @brief Human-readable message for a TPBE_* cause code.
 * @param err Encoded or bare cause code.
 * @return Message string; never NULL.
 */
const char *tpb_get_err_msg(int err);

/**
 * @brief Short name for a TPB_MOD_* module code.
 * @param mod Module code from tpb_module_t.
 * @return Module name string; never NULL.
 */
const char *tpb_err_module_name(int mod);

/**
 * @brief Re-encode err with cur_mod; cause unchanged. No logging.
 * @param cur_mod Reporting module for the caller.
 * @param err Encoded or bare error from a callee.
 * @return TPBE_SUCCESS, TPBE_EXIT_ON_HELP, or TPBE_MAKE(cur_mod, cause).
 */
int tpb_err_propagate(int cur_mod, int err);

/**
 * @brief Map an encoded error to a process exit status (cause only).
 * @param err Encoded or bare error code.
 * @return Cause value suitable for exit(3).
 */
int tpb_err_to_exit_status(int err);

/**
 * @brief Log an error at file/func and return TPBE_MAKE(mod, cause).
 * @param file Source file (__FILE__).
 * @param func Source function (__func__).
 * @param mod Reporting module code.
 * @param cause Bare TPBE_* cause code.
 * @param is_origin Non-zero at failure origin (includes reason string).
 * @param msg Optional context; NULL to omit.
 * @return TPBE_MAKE(mod, cause).
 */
int tpb_report_error_at(const char *file, const char *func, int mod,
                        int cause, int is_origin, const char *msg);

/* Origin: local statement or non-TPBench external call failed. */
#define TPB_FAIL(mod, cause, msg) \
    return tpb_report_error_at(__FILE__, __func__, (mod), (cause), 1, (msg))

/* Propagate callee error: swap module, keep cause, log without reason. */
#define TPB_PROPAGATE(mod, err, msg) \
    do { \
        int _tpb_prop_e = (err); \
        if (_tpb_prop_e == TPBE_EXIT_ON_HELP) { \
            return _tpb_prop_e; \
        } \
        if (_tpb_prop_e != TPBE_SUCCESS) { \
            return tpb_report_error_at(__FILE__, __func__, (mod), \
                                      TPBE_CAUSE(_tpb_prop_e), 0, (msg)); \
        } \
    } while (0)

/** @brief Process context that last completed corelib init in this process. */
typedef enum {
    TPB_CORELIB_CTX_CALLER_TPBCLI = 1,
    TPB_CORELIB_CTX_CALLER_KERNEL = 2
} tpb_corelib_caller_t;

/**
 * @brief Initialize TPBench corelib for this process: resolve workspace, create rafdb
 *        layout under that root, open the run log, and record caller as tpbcli.
 *        Call before any other corelib API that depends on workspace or logging.
 * @param tpb_workspace_path Optional workspace root override, or NULL to use $TPB_WORKSPACE then $HOME/.tpbench.
 * @return TPBE_SUCCESS, TPBE_ILLEGAL_CALL if already initialized, or another TPBE_* code.
 */
int tpb_corelib_init(const char *tpb_workspace_path);

/**
 * @brief Same as tpb_corelib_init but sets caller context to KERNEL after success (PLI kernel entry).
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

/** @brief Union for argument values */
typedef union tpb_parm_value {
    int64_t i64;
    uint64_t u64;
    float f32;
    double f64;
    char c;
} tpb_parm_value_t;

/**
 * @brief Runtime kernel argument definition.
 *
 * "Argument" is the recorded/CLI-facing input; TPB_PARM_* dtype flags remain
 * the type/validation encoding and are unrelated to the TPBARG role tag.
 */
typedef struct tpb_rt_arg {
    char name[TPBM_NAME_STR_MAX_LEN];
    char tag[TPBM_NAME_STR_MAX_LEN];  /**< Normalized tags (includes TPBARG) */
    char note[TPBM_NOTE_STR_MAX_LEN];
    tpb_parm_value_t value;
    TPB_DTYPE ctrlbits;
    int nlims;
    tpb_parm_value_t *plims;
} tpb_rt_arg_t;

/** @brief Kernel output definition */
typedef struct tpb_k_output {
    char name[TPBM_NAME_STR_MAX_LEN];
    char tag[TPBM_NAME_STR_MAX_LEN];  /**< Normalized tags (includes TPBOUT) */
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
    tpb_rt_arg_t *args;
} tpb_argpack_t;

/** @brief Static kernel information */
typedef struct tpb_k_static_info {
    char name[TPBM_NAME_STR_MAX_LEN];
    char note[TPBM_NOTE_STR_MAX_LEN];
    unsigned char kernel_id[20];   /**< Resolved KernelID for this kernel */
    int kernel_record_ok;          /**< 1 if workspace kernel record update succeeded */
    TPB_K_CTRL kctrl;
    int nargs, nouts;
    tpb_rt_arg_t *args;
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

/** @brief One link in an ordered PLI wrapper chain */
typedef struct tpb_wrapper_link {
    char app[TPBM_NAME_STR_MAX_LEN]; /**< Wrapper executable name or path */
    char *args;                      /**< Wrapper arguments (may be NULL) */
} tpb_wrapper_link_t;

/** @brief Ordered wrapper chain for kernel execution */
typedef struct tpb_wrapperpack {
    int nlinks;                        /**< Number of wrapper links */
    tpb_wrapper_link_t *links;         /**< Ordered wrapper links */
} tpb_wrapperpack_t;

/** @brief Runtime handle for kernel execution */
typedef struct tpb_k_rthdl {
    tpb_argpack_t argpack;
    tpb_envpack_t envpack;     /**< Environment variables for kernel */
    tpb_wrapperpack_t wrapperpack; /**< Ordered PLI wrapper chain */
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
 * @brief Add a runtime argument to the current kernel.
 *
 * The dtype argument uses a 32-bit encoding: 0xSSCCTTTT
 *   - SS (bits 24-31): Argument Source flags (TPB_PARM_* source bits)
 *   - CC (bits 16-23): Check/Validation mode flags
 *   - TTTT (bits 0-15): Type code (MPI-compatible)
 *
 * @param name         Argument local name (unique vs other args/outputs; no ':').
 * @param tag          Optional user tags (comma-separated; NULL/"" allowed).
 *                     System appends TPBARG then normalizes (dedupe/upper/sort).
 * @param note         Human-readable argument description
 * @param default_val  String representation of default value
 * @param dtype        Combined data type: source | check | type
 * @param ...          Variable arguments based on validation mode:
 *                     - TPB_PARM_RANGE: (lo, hi) range bounds
 *                     - TPB_PARM_LIST: (n, plist) valid values
 *                     - TPB_PARM_NOCHECK: no additional arguments
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_arg(const char *name, const char *tag, const char *note,
                  const char *default_val, TPB_DTYPE dtype, ...);



/**
 * @brief Register a new output data definition for the current kernel.
 *
 * Must be called during kernel registration (after tpb_k_register,
 * before tpb_k_finalize_pli) to define output metrics.
 *
 * @param name   Output local name (lookup key for alloc; no ':').
 * @param tag    Optional user tags (comma-separated; NULL/"" allowed).
 *               System appends TPBOUT then normalizes.
 * @param note   Human-readable description.
 * @param dtype  Data type of the output (TPB_INT64_T, TPB_DOUBLE_T, etc.).
 * @param unit   Unit type (TPB_UNIT_NS, TPB_UNIT_BYTE, etc.).
 * @return 0 on success, error code otherwise.
 */
int tpb_k_add_output(const char *name, const char *tag, const char *note,
                     TPB_DTYPE dtype, TPB_UNIT_T unit);

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
 * Records kernel inputs and outputs to the RAFDB database.
 * Reads TPB_TBATCH_ID / TPB_HANDLE_INDEX / TPB_WORKSPACE from env.
 * Outputs are written as 1-D arrays.
 *
 * @param hdl          Runtime handle with argpack and respack populated.
 * @param exit_code    Kernel exit code (0 = success).
 * @param task_id_out  Optional; if non-NULL, receives the 20-byte TaskRecordID.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_write_task(tpb_k_rthdl_t *hdl, int exit_code,
                     unsigned char *task_id_out);

/**
 * @brief Patch task record derive_to in task .tpbr and matching row in task.tpbe.
 *        Seeks fixed offsets for current on-disk layout; does not rewrite full
 *        records. Uses flock on task.tpbe.
 * @param task_id       20-byte TaskRecordID whose files to update.
 * @param derive_to_id  20-byte target ID (e.g. TaskCapsuleRecordID).
 * @return TPBE_SUCCESS, TPBE_NULLPTR_ARG, or TPBE_FILE_IO_FAIL.
 */
int tpb_k_task_set_derive_to(const unsigned char task_id[20],
                             const unsigned char derive_to_id[20]);

/**
 * @brief Create a task capsule record (leader rank / primary thread).
 *
 * Reads the first unit's task record to match utc_bits, btime, and linkage
 * fields, then writes a capsule .tpbr/.tpbe and publishes the capsule ID
 * to POSIX shared memory for tpb_k_sync_capsule_task().
 *
 * @param first_task_id  TaskRecordID of the leader's own task record.
 * @param capsule_id_out Output 20-byte TaskCapsuleRecordID.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_create_capsule_task(const unsigned char first_task_id[20],
                              unsigned char capsule_id_out[20]);

/**
 * @brief Read the task capsule ID from POSIX shm (non-leader ranks).
 *
 * Shm object name is derived from kernel_id and handle_id. Blocks until the
 * leader sets the ready flag after tpb_k_create_capsule_task().
 *
 * @param kernel_id       20-byte KernelID (same as env TPB_KERNEL_ID).
 * @param handle_index    Handle index (same as env TPB_HANDLE_INDEX).
 * @param capsule_id_out  Output 20-byte TaskCapsuleRecordID.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_sync_capsule_task(const unsigned char kernel_id[20],
                            uint32_t handle_index,
                            unsigned char capsule_id_out[20]);

/**
 * @brief Append a TaskRecordID to an existing task capsule .tpbr.
 *
 * Uses advisory locking on the capsule file. The leader's ID must already
 * be present from tpb_k_create_capsule_task(); this appends one more ID.
 *
 * @param capsule_id 20-byte TaskCapsuleRecordID.
 * @param task_id    20-byte TaskRecordID to append.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_append_capsule_task(const unsigned char capsule_id[20],
                              const unsigned char task_id[20]);

/**
 * @brief Remove the capsule sync shm object (call on leader after all
 *        ranks have read the capsule ID and finished appends).
 *
 * @param kernel_id    20-byte KernelID.
 * @param handle_index Handle index.
 * @return 0 on success, error code otherwise (including ENOENT as success).
 */
int tpb_k_unlink_capsule_sync_shm(const unsigned char kernel_id[20],
                                    uint32_t handle_index);

/* ===== PLI (Process-Level Integration) API ===== */

/**
 * @brief Finalize kernel registration.
 *
 * Increments the kernel count and completes registration. Called after all
 * parameters and outputs have been added via tpb_k_add_arg/tpb_k_add_output.
 *
 * @return 0 on success, error code otherwise.
 */
int tpb_k_finalize_pli(void);

/**
 * @brief Set timer for PLI kernel executable.
 *
 * Intended to be called from PLI kernel entry functions. Sets up the timer based
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
 * builds a runtime handle. Called from PLI kernel entry functions.
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
 * and kernel parameters, then launches the kernel `.so` via `tpbcli-pli-launcher`.
 * Captures and forwards stdout/stderr to both console and log file.
 *
 * @param hdl Runtime handle for the kernel (must be non-NULL).
 * @return 0 on success, error code otherwise.
 */
int tpb_run_pli(tpb_k_rthdl_t *hdl);

/**
 * @brief Register common parameters, scan all PLI kernels, and sync kernel records
 *        with the active workspace. Call once before tpb_query_kernel or kernel ls.
 * @return 0 on success, error code otherwise.
 */
int tpb_register_kernel(void);

/**
 * @brief Register common parameters and scan only the named PLI kernels.
 *
 * Used by tpbcli run to avoid loading every kernel. Any named kernel that fails
 * to scan returns an error.
 *
 * @param n Number of kernel names in names
 * @param names Array of kernel name strings (may be NULL when n is 0)
 * @return 0 on success, error code otherwise
 */
int tpb_register_kernels(int n, const char *const *names);

/* ===== Driver Orchestration API ===== */

/**
 * @brief Set kernel argument value for the current handle.
 * @param parm_name Parameter name.
 * @param v Pointer to value to set.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_set_hdl_karg(const char *parm_name, void *v);

/**
 * @brief Set environment variable for the current handle.
 * @param env_name Environment variable name.
 * @param env_value Environment variable value.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_set_hdl_env(const char *env_name, const char *env_value);

/**
 * @brief Replace the ordered wrapper chain for the current handle.
 * @param links Array of wrapper links (deep-copied).
 * @param nlinks Number of links (0 clears the chain).
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_set_hdl_wrappers(const tpb_wrapper_link_t *links, int nlinks);

/**
 * @brief Replace the ordered wrapper chain for a handle by index.
 * @param hdl_idx Handle index (0 .. nhdl-1).
 * @param links Array of wrapper links (deep-copied).
 * @param nlinks Number of links (0 clears the chain).
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_set_hdl_wrappers_idx(int hdl_idx,
                                    const tpb_wrapper_link_t *links,
                                    int nlinks);

/**
 * @brief Copy argpack/envpack/wrapperpack from a source handle index.
 * @param src_idx Source handle index.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_copy_hdl_from(int src_idx);

/**
 * @brief Get the current handle index.
 * @return Current handle index, or -1 if none.
 */
int tpb_driver_get_current_hdl_idx(void);

/**
 * @brief Add a handle for a kernel by name.
 * @param kernel_name Kernel name.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_add_handle(const char *kernel_name);

/**
 * @brief Run all handles (indices 0 .. nhdl-1).
 * @return TPBE_SUCCESS or error code on first failure.
 */
int tpb_driver_run_all(void);

/**
 * @brief Enable dry-run mode for tpb_run_pli.
 * @param enabled Nonzero to enable, zero to disable.
 */
void tpb_driver_set_dry_run(int enabled);

/**
 * @brief Reset handles for the next batch run.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_driver_reset_handles(void);

/**
 * @brief Get number of handles in the current batch.
 * @return Number of handles.
 */
int tpb_get_nhdl(void);

/* ===== Host Argument Parsing API ===== */

/**
 * @brief Validate kernel arguments against kernel parameter definitions.
 * @param common_tokens Common tokens parsed from CLI.
 * @param ncommon Number of common tokens.
 * @param kernel_tokens Kernel-specific tokens parsed from CLI.
 * @param nkernel Number of kernel-specific tokens.
 * @param kernel Pointer to kernel definition.
 * @param rt_args_out Output runtime parameters array.
 * @param nargs_out Output number of parameters.
 * @return TPBE_SUCCESS or validation error code.
 */
int tpb_check_kargs(char **common_tokens, int ncommon,
                    char **kernel_tokens, int nkernel,
                    tpb_kernel_t *kernel,
                    tpb_rt_arg_t **rt_args_out, int *nargs_out);

/**
 * @brief Parse comma-separated key=value string and set kernel arguments.
 * @param nchar Number of characters in tokstr (unused, compatibility).
 * @param tokstr Comma-separated key=value string.
 * @param narg Optional counter incremented per token.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_argp_set_kargs_tokstr(int nchar, char *tokstr, int *narg);

/**
 * @brief Set the timer for the driver by name.
 * @param timer_name Timer name ("clock_gettime", "tsc_asym").
 * @return TPBE_SUCCESS or error code.
 */
int tpb_argp_set_timer(const char *timer_name);

/* ===== Batch Recording API (host process) ===== */

/**
 * @brief Begin a new task batch and cache batch context.
 * @param batch_type TPB_BATCH_TYPE_RUN or TPB_BATCH_TYPE_BENCHMARK.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_record_begin_batch(uint32_t batch_type);

/**
 * @brief Get the 40-char hex string of the current TBatchID.
 * @return Pointer to static hex buffer, or NULL if no batch is active.
 */
const char *tpb_record_get_tbatch_id_hex(void);

/**
 * @brief Get the workspace path cached by tpb_record_begin_batch().
 * @return Pointer to static path buffer, or NULL if no batch is active.
 */
const char *tpb_record_get_workspace(void);

/**
 * @brief End the current task batch and finalize tbatch records.
 * @param ntask Number of tasks executed in this batch (reserved).
 * @return TPBE_SUCCESS or error code.
 */
int tpb_record_end_batch(int ntask);

/* ===== TPBlog API ===== */

/**
 * @brief Open or reopen the run log for this process.
 *
 * When environment variable TPBLOG_FILE_ENV ("TPB_LOG_FILE") is set, opens that
 * path in append mode without writing a session header. Otherwise creates
 * <workspace>/rafdb/log/tpbrunlog_YYYYMMDDThhmmss_<host>.log.
 *
 * @return TPBE_SUCCESS on success, TPBE_FILE_IO_FAIL on failure.
 */
int tpblog_init(void);

/**
 * @brief Close the active run log file.
 */
void tpblog_cleanup(void);

/**
 * @brief Get the active run log file path.
 * @return Path string, or NULL if no log is open.
 */
const char *tpblog_get_filepath(void);

/**
 * @brief Set the global tpblog verbosity threshold.
 * @param level Minimum level to emit; TPB_LOG_LEVEL_NONE suppresses all output.
 */
void tpblog_set_level(tpb_log_level_t level);

/**
 * @brief Get the global tpblog verbosity threshold.
 */
tpb_log_level_t tpblog_get_level(void);

/**
 * @brief Formatted output to stdout and the active run log.
 *
 * Output prefix when flags request it:
 * YYYY-mm-dd HH:MM:SS [INFO|WARN|ERRO] message
 *
 * @param level Message verbosity level; filtered by tpblog_get_level().
 * @param log_type One of TPBLOG_TYPE_INFO, TPBLOG_TYPE_WARN, TPBLOG_TYPE_ERRO.
 * @param flags TPBLOG_FLAG_DIRECT, TPBLOG_FLAG_TS, TPBLOG_FLAG_TAG, or TPBLOG_FLAG_TSTAG.
 * @param fmt printf-style format string.
 */
void tpblog_printf(tpb_log_level_t level, uint32_t log_type, uint32_t flags,
                  const char *fmt, ...);

/**
 * @brief Same dual-write semantics as tpblog_printf().
 *
 * Provided as a distinct entry point for call sites that previously used file-backed
 * logging helpers. Both functions write to stdout and the active run log.
 */
void tpblog_printf_f(tpb_log_level_t level, uint32_t log_type, uint32_t flags,
                    const char *fmt, ...);

/** @brief Wrap long cell text with hyphenation when breaking mid-word (default). */
#define TPBLOG_WRAP_HYPHEN    0u
/** @brief Wrap long cell text without appending a hyphen at line breaks. */
#define TPBLOG_WRAP_NO_HYPHEN 1u

/** @brief Left-align cell text within its column width (default). */
#define TPBLOG_ALIGN_LEFT     0u
/** @brief Right-align cell text within its column width. */
#define TPBLOG_ALIGN_RIGHT    1u

/**
 * @brief Options for tpblog_printf_ctab() fixed-width column layout.
 *
 * No borders or delimiter glyphs are drawn; columns are separated only by
 * @c gap spaces. Layout width comes from stdout ioctl, @c TPBLOG_TEST_WIDTH,
 * or the module default (85), then optionally clamped by
 * @c term_col_width_min / @c term_col_width_max.
 *
 * When proportional allocation yields any column width below 1, each cell is
 * printed on its own line (degenerate mode).
 */
typedef struct tpblog_ctab {
    const float *col_ratios;   /**< Width ratios; NULL selects equal ratios. */
    int ncol;                  /**< Column count in [1, TPBLOG_COLUMN_MAX]. */
    int gap;                   /**< Spaces between columns; must be >= 0. */
    const uint32_t *wrap;      /**< Per-column wrap; NULL => HYPHEN for all. */
    const uint32_t *align;     /**< Per-column align; NULL => LEFT for all. */
    int term_col_width_min;    /**< Floor for terminal width; 0 = disabled. */
    int term_col_width_max;    /**< Ceiling for terminal width; 0 = disabled. */
} tpblog_ctab_t;

/**
 * @brief Print one fixed-width table row to stdout and the active run log.
 *
 * @param opt Layout options; must be non-NULL with valid @c ncol and @c gap.
 * @param cells Array of @c opt->ncol NUL-terminated cell strings (NULL cell = "").
 *
 * Wrap prefers breaks at whitespace. Mid-word breaks use @c TPBLOG_WRAP_HYPHEN
 * (append '-') or @c TPBLOG_WRAP_NO_HYPHEN. Alignment applies when padding a
 * wrapped line fragment to the column width.
 */
void tpblog_printf_ctab(const tpblog_ctab_t *opt, const char *const *cells);

/**
 * @brief Print fixed-width columns (compat wrapper around tpblog_printf_ctab).
 *
 * Equivalent to tpblog_printf_ctab with wrap=NULL, align=NULL, and both
 * term_col_width_min/max set to 0 (no clamp).
 *
 * @param col_ratios Column width ratios; NULL selects equal ratios.
 * @param ncol Number of columns.
 * @param gap Spaces between columns; must be >= 0.
 * @param cells Array of ncol pre-formatted C strings.
 */
void tpblog_printf_c(const float *col_ratios, int ncol, int gap,
                     const char *const *cells);

/**
 * @brief Like tpblog_printf_c with per-column wrap behaviour control.
 * @param wrap_flags Per-column flags: TPBLOG_WRAP_HYPHEN or TPBLOG_WRAP_NO_HYPHEN.
 */
void tpblog_printf_c_flags(const float *col_ratios, int ncol, int gap,
                           const char *const *cells,
                           const uint32_t *wrap_flags);

/**
 * @brief Print a full terminal-width horizontal rule line.
 * @param fill Character repeated across the line (e.g. '=' or '-').
 */
void tpblog_print_hline(char fill);

/**
 * @brief Print one key = value row with fixed key column and no hyphenation.
 *
 * Uses a three-column layout (key, "=", value). Long keys or values wrap inside
 * their columns without trailing hyphens (TPBLOG_WRAP_NO_HYPHEN).
 *
 * @param key Field name; may wrap within key_width.
 * @param value Pre-formatted value string.
 * @param key_width Width of the key column in characters (caller clamps).
 */
void tpblog_print_kv_eq(const char *key, const char *value, int key_width);

/**
 * @brief Set output formatting arguments for CLI display.
 * @param unit_cast Enable unit casting (0 or 1).
 * @param sigbit_trim Significant bits for trimming (0 = no trim).
 */
void tpb_set_outargs(int unit_cast, int sigbit_trim);

/**
 * @brief Format a floating-point value with significant-digit trimming.
 * @param value Value to format.
 * @param buf Output buffer.
 * @param bufsize Capacity of @p buf.
 * @param sigbit Significant digits to retain when trimming.
 * @param intbit Integer digits before decimal (0 or negative = no limit).
 * @return Number of characters written.
 */
int tpb_format_value(double value, char *buf, size_t bufsize,
                     int sigbit, int intbit);

/* ===== Statistics (also used by tpbcli benchmark over rafdb payloads) ===== */

/**
 * @brief Element size in bytes for a TPB_DTYPE (TYPE_MASK bits).
 * @param dtype Data type flags.
 * @param out Receives size; must be non-NULL.
 * @return 0 on success, nonzero if unsupported.
 */
int tpb_dtype_elem_size(TPB_DTYPE dtype, size_t *out);

/**
 * @brief Mean of a typed 1-D array.
 */
int tpb_stat_mean(void *arr, size_t narr, TPB_DTYPE dtype, double *mean_out);

/**
 * @brief Minimum of a typed 1-D array.
 */
int tpb_stat_min(void *arr, size_t narr, TPB_DTYPE dtype, double *min_out);

/**
 * @brief Maximum of a typed 1-D array.
 */
int tpb_stat_max(void *arr, size_t narr, TPB_DTYPE dtype, double *max_out);

/**
 * @brief Quantiles of a typed 1-D array (@p qarr entries in [0,1]).
 */
int tpb_stat_qtile_1d(void *arr, size_t narr, TPB_DTYPE dtype,
                      double *qarr, size_t nq, double *qout);

/* ===== Installation Path API ===== */

/**
 * @brief Get the resolved TPBench installation root (TPB_HOME).
 * @return Installation root path, or NULL if unset.
 */
const char *tpb_dl_get_tpb_home(void);

/**
 * @brief Override the resolved TPBench installation root.
 * @param path Absolute or relative install root path.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_dl_force_tpb_home(const char *path);

/* ===== CLI Output Helpers ===== */

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

/* ===== Record Data Types (rafdb) ===== */

/** @brief Opaque reserve size in .tpbr meta and tail of .tpbe entry rows (bytes) */
#define TPB_RAF_RESERVE_SIZE 128

/** @brief 64-bit bit-packed datetime representation */
typedef uint64_t tpb_dtbits_t;

/**
 * @brief Datetime structure storing components from year to second.
 */
typedef struct tpb_datetime {
    uint16_t year;  /**< Full year, e.g. 2026 */
    uint8_t  month; /**< 1-12 */
    uint8_t  day;   /**< 1-31 */
    uint8_t  hour;  /**< 0-23 */
    uint8_t  min;   /**< 0-59 */
    uint8_t  sec;   /**< 0-59 */
} tpb_datetime_t;

/**
 * @brief Buffer for ISO 8601 formatted datetime strings.
 */
typedef struct tpb_datetime_str {
    char str[32]; /**< ISO 8601 formatted datetime string */
} tpb_datetime_str_t;

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
    char name[TPBM_NAME_STR_MAX_LEN];       /**< Header local name (256 bytes, NUL-terminated) */
    char tag[TPBM_NAME_STR_MAX_LEN];        /**< Normalized tags (256 bytes); empty if none */
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
    unsigned char derive_to[20];     /**< Derivation target (alias/group ID), or zero */
    unsigned char inherit_from[20];  /**< Lineage: source record ID, or zero */
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
    unsigned char inherit_from[20];   /**< Lineage: source TBatchID, or zero */
    tpb_dtbits_t start_utc_bits;  /**< Batch start datetime */
    uint64_t duration;            /**< Duration in nanoseconds */
    char hostname[64];            /**< Hostname */
    uint32_t nkernel;             /**< Number of kernels */
    uint32_t ntask;               /**< Number of tasks */
    uint32_t nscore;              /**< Number of scores */
    uint32_t batch_type;          /**< 0=run, 1=benchmark */
    unsigned char reserve[TPB_RAF_RESERVE_SIZE]; /**< Reserved tail */
} tbatch_entry_t;

/** @brief Kernel full attributes (.tpbr meta section) */
typedef struct kernel_attr {
    unsigned char kernel_id[20];  /**< KernelID (kernel .so SHA-1) */
    unsigned char derive_to[20];     /**< Derivation target, or zero */
    unsigned char inherit_from[20];   /**< Lineage: source KernelID, or zero */
    char kernel_name[256];        /**< Kernel name */
    char version[64];             /**< Version string */
    char description[2048];       /**< Description */
    uint32_t narg;                /**< Registered arguments */
    uint32_t nmetric;             /**< Registered output metrics */
    uint32_t kctrl;               /**< Kernel control bits */
    uint32_t nheader;             /**< Number of headers */
    uint32_t active;              /**< 1 if this KernelID is the loadable variant */
    tpb_dtbits_t utc_bits;        /**< Kernel build/registration datetime (UTC) */
    tpb_meta_header_t *headers;   /**< Header array */
} kernel_attr_t;

/** @brief Kernel entry (slim record in .tpbe) */
typedef struct kernel_entry {
    unsigned char kernel_id[20];  /**< KernelID (kernel .so SHA-1) */
    unsigned char inherit_from[20];   /**< Lineage: source KernelID, or zero */
    char kernel_name[64];         /**< Kernel name */
    uint32_t kctrl;               /**< Kernel control bits */
    uint32_t narg;                /**< Number of arguments */
    uint32_t nmetric;             /**< Number of metrics */
    uint32_t active;              /**< 1 if loadable, 0 if inactive/historical */
    tpb_dtbits_t utc_bits;        /**< Kernel build/registration datetime (UTC) */
    unsigned char reserve[TPB_RAF_RESERVE_SIZE + 8]; /**< Reserved */
} kernel_entry_t;

/** @brief Task full attributes (.tpbr meta section) */
typedef struct task_attr {
    unsigned char task_record_id[20]; /**< TaskRecordID (SHA-1) */
    unsigned char derive_to[20];         /**< Derivation target, or zero */
    unsigned char inherit_from[20];       /**< Lineage: source TaskRecordID, or zero */
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
    unsigned char inherit_from[20];       /**< Lineage: source TaskRecordID, or zero */
    unsigned char tbatch_id[20];      /**< TBatchID */
    unsigned char kernel_id[20];      /**< KernelID */
    tpb_dtbits_t utc_bits;            /**< Invocation datetime */
    uint64_t duration;                /**< Duration (ns) */
    uint32_t exit_code;               /**< Exit code */
    uint32_t handle_index;            /**< Handle index */
    unsigned char derive_to[20];         /**< Merge target: merged TaskRecordID, or zero */
    unsigned char reserve[TPB_RAF_RESERVE_SIZE - 20]; /**< Reserved tail */
} task_entry_t;

/* ===== rafdb Workspace API ===== */

/**
 * @brief Copy the workspace path set by tpb_corelib_init into out_path.
 *
 * Requires a prior successful tpb_corelib_init (or tpb_k_corelib_init) in this process.
 * Optionally verifies the path exists as a directory.
 *
 * @param out_path Buffer to receive resolved path
 * @param pathlen  Buffer size
 * @return TPBE_SUCCESS, TPBE_ILLEGAL_CALL if corelib is not initialized and
 *         TPB_WORKSPACE is unset, or TPBE_FILE_IO_FAIL if the path is missing
 */
int tpb_raf_resolve_workspace(char *out_path, size_t pathlen);

/**
 * @brief Initialize workspace directory structure.
 *
 * Creates etc/config.json and rafdb/{task_batch,kernel,task,log}
 * directories if they do not exist.
 *
 * @param workspace_path Root workspace directory
 * @return 0 on success, error code otherwise
 */
int tpb_raf_init_workspace(const char *workspace_path);

/* ===== rafdb Magic API ===== */

/** @brief Length of an 8-byte rafdb magic signature */
#define TPB_RAF_MAGIC_LEN     8
/** @brief Magic signature byte 0 (template E1 54 50 42 … 31 E0) */
#define TPB_RAF_MAGIC_B0      0xE1
/** @brief Magic signature byte 1 ('T') */
#define TPB_RAF_MAGIC_B1      0x54
/** @brief Magic signature byte 2 ('P') */
#define TPB_RAF_MAGIC_B2      0x50
/** @brief Magic signature byte 3 ('B') */
#define TPB_RAF_MAGIC_B3      0x42
/** @brief Magic signature byte 6 ('1') */
#define TPB_RAF_MAGIC_B6      0x31
/** @brief Magic signature byte 7 */
#define TPB_RAF_MAGIC_B7      0xE0
/** @brief Record start position marker (byte 5, 'S') */
#define TPB_RAF_POS_START     0x53
/** @brief Metadata / record-data split marker (byte 5, 'D') */
#define TPB_RAF_POS_SPLIT     0x44
/** @brief Record end marker (byte 5, 'E') */
#define TPB_RAF_POS_END       0x45

/** @brief Entry file (.tpbe) type nibble for magic byte 4 (high nibble) */
#define TPB_RAF_FTYPE_ENTRY   ((uint8_t)0xE0)
/** @brief Record file (.tpbr) type nibble for magic byte 4 (high nibble) */
#define TPB_RAF_FTYPE_RECORD  ((uint8_t)0xD0)
/** @brief Task batch domain (magic byte 4 low nibble) */
#define TPB_RAF_DOM_TBATCH    ((uint8_t)0x00)
/** @brief Kernel domain (magic byte 4 low nibble) */
#define TPB_RAF_DOM_KERNEL    ((uint8_t)0x01)
/** @brief Task domain (magic byte 4 low nibble) */
#define TPB_RAF_DOM_TASK      ((uint8_t)0x02)
/** @brief Runtime environment domain (magic byte 4 low nibble) */
#define TPB_RAF_DOM_RTENV     ((uint8_t)0x03)

/**
 * @brief Build an 8-byte magic signature.
 * @param ftype  File type (TPB_RAF_FTYPE_ENTRY or _RECORD)
 * @param domain Domain (TPB_RAF_DOM_TBATCH/KERNEL/TASK)
 * @param pos    Position (TPB_RAF_POS_START/SPLIT/END)
 * @param out    Output buffer, at least 8 bytes
 */
void tpb_raf_build_magic(uint8_t ftype, uint8_t domain,
                           uint8_t pos, unsigned char out[8]);

/**
 * @brief Validate an 8-byte magic signature.
 * @param magic  8-byte buffer to check
 * @param ftype  Expected file type
 * @param domain Expected domain
 * @param pos    Expected position
 * @return 1 if valid, 0 if invalid
 */
int tpb_raf_validate_magic(const unsigned char magic[8],
                             uint8_t ftype, uint8_t domain,
                             uint8_t pos);

/**
 * @brief Detect rafdb file type and domain from the first 8-byte magic.
 * @param filepath Path to a .tpbe or .tpbr file
 * @param ftype_out Receives TPB_RAF_FTYPE_ENTRY or TPB_RAF_FTYPE_RECORD
 * @param domain_out Receives TPB_RAF_DOM_TBATCH, _KERNEL, or _TASK
 * @return 0 on success, TPBE_FILE_IO_FAIL if unreadable or invalid magic
 */
int tpb_raf_detect_file(const char *filepath,
                          uint8_t *ftype_out,
                          uint8_t *domain_out);

/**
 * @brief Read START/SPLIT/END magic bytes from an on-disk .tpbr file.
 *
 * Opens the record read-only, copies the three 8-byte signatures from the file
 * (does not reconstruct them), and validates ftype/domain/position. Skips the
 * metadata and payload blobs using metasize/datasize fields only.
 *
 * @param workspace   Workspace root path.
 * @param domain      TPB_RAF_DOM_* selector.
 * @param id20        20-byte record id for tbatch/kernel/task; NULL for rtenv.
 * @param rtenv_id    Numeric id when domain is TPB_RAF_DOM_RTENV (ignored otherwise).
 * @param start_magic Output START signature (8 bytes).
 * @param split_magic Output SPLIT signature (8 bytes).
 * @param end_magic   Output END signature (8 bytes).
 * @return TPBE_SUCCESS or TPBE_FILE_IO_FAIL.
 */
int tpb_raf_record_peek_magics(const char *workspace,
                               uint8_t domain,
                               const unsigned char id20[20],
                               int32_t rtenv_id,
                               unsigned char start_magic[8],
                               unsigned char split_magic[8],
                               unsigned char end_magic[8]);

/* ===== rafdb Entry API (.tpbe) ===== */

/**
 * @brief Append a tbatch entry to the .tpbe file.
 * @param workspace Workspace root path
 * @param entry     Entry to append
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_append_tbatch(const char *workspace,
                                  const tbatch_entry_t *entry);

/**
 * @brief Append a kernel entry to the .tpbe file.
 * @param workspace Workspace root path
 * @param entry     Entry to append
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_append_kernel(const char *workspace,
                                  const kernel_entry_t *entry);

/**
 * @brief Append a task entry to the .tpbe file.
 * @param workspace Workspace root path
 * @param entry     Entry to append
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_append_task(const char *workspace,
                                const task_entry_t *entry);

/**
 * @brief List all tbatch entries from the .tpbe file.
 * @param workspace Workspace root path
 * @param entries   Output: allocated array (caller must free)
 * @param count     Output: number of entries
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_list_tbatch(const char *workspace,
                                tbatch_entry_t **entries,
                                int *count);

/**
 * @brief List all kernel entries from the .tpbe file.
 * @param workspace Workspace root path
 * @param entries   Output: allocated array (caller must free)
 * @param count     Output: number of entries
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_list_kernel(const char *workspace,
                                kernel_entry_t **entries,
                                int *count);

/**
 * @brief List all task entries from the .tpbe file.
 * @param workspace Workspace root path
 * @param entries   Output: allocated array (caller must free)
 * @param count     Output: number of entries
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_list_task(const char *workspace,
                              task_entry_t **entries,
                              int *count);

/* ===== rafdb Record API (.tpbr) ===== */

/**
 * @brief Write a tbatch .tpbr record file.
 * @param workspace Workspace root path
 * @param attr      TBatch attributes with headers populated
 * @param data      Record data buffer (may be NULL if datasize==0)
 * @param datasize  Size of record data in bytes
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_write_tbatch(const char *workspace,
                                  const tbatch_attr_t *attr,
                                  const void *data,
                                  uint64_t datasize);

/**
 * @brief Append a TaskRecordID to tbatch .tpbr header[0] (TPBLINK::TaskID).
 * @param workspace   Workspace root
 * @param tbatch_id   20-byte TBatchID
 * @param task_id     TaskRecordID to append (20 bytes)
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_append_tbatch(const char *workspace,
                                 const unsigned char tbatch_id[20],
                                 const unsigned char task_id[20]);

/**
 * @brief Patch duration, nkernel, and ntask in an existing tbatch .tpbr.
 * @param workspace   Workspace root
 * @param tbatch_id   20-byte TBatchID
 * @param duration    Batch duration (ns)
 * @param nkernel     Unique kernel count
 * @param ntask       Task entry-point count
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_patch_tbatch_counters(const char *workspace,
                                         const unsigned char tbatch_id[20],
                                         uint64_t duration,
                                         uint32_t nkernel,
                                         uint32_t ntask);

/**
 * @brief Read a tbatch .tpbr record file.
 * @param workspace Workspace root path
 * @param tbatch_id 20-byte TBatchID
 * @param attr      Output attributes (headers allocated; free with tpb_raf_free_headers)
 * @param data      Output data buffer (caller must free, may be NULL)
 * @param datasize  Output data size
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_read_tbatch(const char *workspace,
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
int tpb_raf_record_write_kernel(const char *workspace,
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
int tpb_raf_record_read_kernel(const char *workspace,
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
int tpb_raf_record_write_task(const char *workspace,
                                const task_attr_t *attr,
                                const void *data,
                                uint64_t datasize);

/**
 * @brief Write a new task capsule .tpbr (one header, first task ID in data).
 *
 * Expects attr->nheader==1, ninput==noutput==0, inherit_from all 0xFF, and
 * attr->task_record_id set to the capsule ID.
 *
 * @param workspace      Workspace root
 * @param attr           Task attributes for the capsule
 * @param first_task_id  First member TaskRecordID (20 bytes)
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_create_task_capsule(const char *workspace,
                                       const task_attr_t *attr,
                                       const unsigned char first_task_id[20]);

/**
 * @brief Append a TaskRecordID to a task capsule .tpbr under file lock.
 * @param workspace   Workspace root
 * @param capsule_id  TaskCapsuleRecordID
 * @param task_id     TaskRecordID to append
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_append_task_capsule(const char *workspace,
                                       const unsigned char capsule_id[20],
                                       const unsigned char task_id[20]);

/**
 * @brief Read a task .tpbr record file.
 * @param workspace Workspace root path
 * @param task_id   20-byte TaskRecordID
 * @param attr      Output attributes
 * @param data      Output data buffer (caller must free)
 * @param datasize  Output data size
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_read_task(const char *workspace,
                               const unsigned char task_id[20],
                               task_attr_t *attr,
                               void **data,
                               uint64_t *datasize);

/**
 * @brief Free header array allocated by record read functions.
 * @param headers Header array
 * @param nheader Number of headers
 */
void tpb_raf_free_headers(tpb_meta_header_t *headers,
                            uint32_t nheader);

/**
 * @brief Resolve a pointer into a concatenated record data blob for one header.
 *
 * Headers are laid out in order: offset of header @p idx is the sum of
 * @c data_size for headers [0 .. idx). Returns TPBE_LIST_NOT_FOUND if @p idx is
 * out of range, or TPBE_FILE_IO_FAIL if the span does not fit in @p datasize.
 *
 * @param headers Header array from a record read.
 * @param nheader Number of headers.
 * @param data Concatenated payload blob (may be NULL only when all sizes are 0).
 * @param datasize Size of @p data in bytes.
 * @param idx Header index in [0, nheader).
 * @param ptr_out Receives pointer to that header's bytes (may be data+off with size 0).
 * @param nbytes_out Receives headers[idx].data_size.
 * @return TPBE_SUCCESS or encoded error.
 */
int tpb_raf_header_data_ptr(const tpb_meta_header_t *headers,
                            uint32_t nheader,
                            const void *data,
                            uint64_t datasize,
                            uint32_t idx,
                            const void **ptr_out,
                            uint64_t *nbytes_out);

/* ===== Runtime Environment (rafdb) ===== */

#define TPB_RAF_RTENV_NAME_LEN     256
#define TPB_RAF_RTENV_HOST_LEN     256
#define TPB_RAF_RTENV_NOTE_LEN     256
#define TPB_RAF_RTENV_RESERVE_LEN  1024
#define TPB_RAF_RTENV_APP_STR_LEN  64
#define TPB_RTENV_MERGED_MAX_VAR   512

/** on_set: applied at rtenv load and for --kenvs when key is declared. */
#define TPB_RTENV_ON_SET_IGNORE    0u
#define TPB_RTENV_ON_SET_OVERWRITE 1u
#define TPB_RTENV_ON_SET_PREPEND   2u
#define TPB_RTENV_ON_SET_APPEND    3u

/** on_get: applied when scanning environ at tpb_k_corelib_init. */
#define TPB_RTENV_ON_GET_IGNORE    0u
#define TPB_RTENV_ON_GET_WARN      1u
#define TPB_RTENV_ON_GET_FAIL      2u
#define TPB_RTENV_ON_GET_OVERWRITE 3u

/** @brief One merged RTEnv variable (inherit chain resolved). */
typedef struct tpb_rtenv_merged_var {
    char     key[256];
    char     value[4096];
    uint32_t on_set;
    uint32_t on_get;
} tpb_rtenv_merged_var_t;

/** @brief Merged application + variable view for active RTEnv chain. */
typedef struct tpb_rtenv_merged {
    char     apps_name[32][64];
    char     apps_version[32][64];
    char     apps_note[32][64];
    int      napp;
    tpb_rtenv_merged_var_t vars[TPB_RTENV_MERGED_MAX_VAR];
    int      nenv;
} tpb_rtenv_merged_t;

/** @brief Runtime environment .tpbe row (no nheader field) */
typedef struct tpb_raf_rtenv_entry {
    int32_t      id;                                      /**< Domain-local ID */
    char         name[TPB_RAF_RTENV_NAME_LEN];            /**< Unique name */
    char         hostname[TPB_RAF_RTENV_HOST_LEN];        /**< Creation host */
    tpb_dtbits_t utc_bits;                                /**< Creation time */
    int32_t inherit_from;                            /**< Parent ID or 0 if root */
    int32_t      derive_to;                               /**< Latest child ID or -1 */
    uint32_t     ntask;                                   /**< Task count */
    uint32_t     ntbatch;                                 /**< TBatch count */
    char         note[TPB_RAF_RTENV_NOTE_LEN];            /**< Description */
    uint32_t     napp;                                    /**< Application count */
    uint32_t     nenv;                                    /**< Env-var count */
    unsigned char reserve[TPB_RAF_RTENV_RESERVE_LEN];     /**< Reserved */
} tpb_raf_rtenv_entry_t;

/** @brief Runtime environment .tpbr attributes (includes nheader) */
typedef struct tpb_raf_rtenv_attr {
    int32_t      id;                                      /**< Domain-local ID */
    char         name[TPB_RAF_RTENV_NAME_LEN];            /**< Unique name */
    char         hostname[TPB_RAF_RTENV_HOST_LEN];        /**< Creation host */
    tpb_dtbits_t utc_bits;                                /**< Creation time */
    int32_t inherit_from;                            /**< Parent ID or 0 if root */
    int32_t      derive_to;                               /**< Latest child ID or -1 */
    uint32_t     ntask;                                   /**< Task count */
    uint32_t     ntbatch;                                 /**< TBatch count */
    char         note[TPB_RAF_RTENV_NOTE_LEN];            /**< Description */
    uint32_t     napp;                                    /**< Application count */
    uint32_t     nenv;                                    /**< Env-var count */
    uint32_t     nheader;                                 /**< Meta header count */
    unsigned char reserve[TPB_RAF_RTENV_RESERVE_LEN];      /**< Reserved */
} tpb_raf_rtenv_attr_t;

/**
 * @brief Allocate the next domain-local RTEnv ID (max existing + 1, or 0).
 * @param workspace Workspace root path
 * @param id_out Receives next ID
 * @return 0 on success, error code otherwise
 */
int tpb_raf_rtenv_alloc_next_id(const char *workspace, int32_t *id_out);

/**
 * @brief Append a runtime environment entry to .tpbe (name must be unique).
 * @param workspace Workspace root path
 * @param entry Entry to append
 * @return 0 on success, TPBE_LIST_DUP if name exists
 */
int tpb_raf_entry_append_rtenv(const char *workspace,
                               const tpb_raf_rtenv_entry_t *entry);

/**
 * @brief List all runtime environment entries from .tpbe.
 * @param workspace Workspace root path
 * @param entries Output array (caller must free)
 * @param count Output entry count
 * @return 0 on success, error code otherwise
 */
int tpb_raf_entry_list_rtenv(const char *workspace,
                             tpb_raf_rtenv_entry_t **entries,
                             int *count);

/**
 * @brief Write a runtime environment .tpbr record file.
 * @param workspace Workspace root path
 * @param attr Fixed attributes including nheader
 * @param headers Meta headers (length attr->nheader)
 * @param data Record payload (may be NULL if datasize==0)
 * @param datasize Payload size in bytes
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_write_rtenv(const char *workspace,
                               const tpb_raf_rtenv_attr_t *attr,
                               const tpb_meta_header_t *headers,
                               const void *data,
                               uint64_t datasize);

/**
 * @brief Read a runtime environment .tpbr record by numeric ID.
 * @param workspace Workspace root path
 * @param id Domain-local RTEnv ID
 * @param attr Output fixed attributes
 * @param headers Output headers (caller frees with tpb_raf_free_headers)
 * @param data Output payload (caller must free, may be NULL)
 * @param datasize Output payload size
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_read_rtenv(const char *workspace,
                             int32_t id,
                             tpb_raf_rtenv_attr_t *attr,
                             tpb_meta_header_t **headers,
                             void **data,
                             uint64_t *datasize);

/**
 * @brief Patch ntask/ntbatch counters in RTEnv .tpbe row and .tpbr record.
 * @param workspace Workspace root path
 * @param id RTEnv domain-local ID
 * @param add_ntask Increment for ntask (may be 0)
 * @param add_ntbatch Increment for ntbatch (may be 0)
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_patch_rtenv_counters(const char *workspace,
                                        int32_t id,
                                        uint32_t add_ntask,
                                        uint32_t add_ntbatch);

/**
 * @brief Append a child RTEnv ID to parent's DeriveTo link and derive_to field.
 * @param workspace Workspace root path
 * @param parent_id Parent RTEnv ID
 * @param child_id Child RTEnv ID to append
 * @return 0 on success, error code otherwise
 */
int tpb_raf_record_append_rtenv_derive(const char *workspace,
                                       int32_t parent_id,
                                       int32_t child_id);

/**
 * @brief Read runtime_environment.base_id from etc/config.json.
 * @param workspace Workspace root path
 * @param base_id_out Receives base ID (1 if key missing)
 * @return 0 on success, error code otherwise
 */
int tpb_raf_config_get_base_id(const char *workspace, int32_t *base_id_out);

/**
 * @brief Write runtime_environment.base_id into etc/config.json.
 * @param workspace Workspace root path
 * @param base_id Base environment ID to store
 * @return 0 on success, error code otherwise
 */
int tpb_raf_config_set_base_id(const char *workspace, int32_t base_id);

/**
 * @brief Record build-time environment snapshot as a root RTEnv entry.
 * @param workspace Workspace root path
 * @param always_new Non-zero to always append a new root snapshot
 * @param id_out Receives created or existing base ID
 * @return 0 on success, error code otherwise
 */
int tpb_rtenv_snapshot_build_env(const char *workspace, int always_new,
                                 int32_t *id_out);

/**
 * @brief Ensure a base RTEnv exists (lazy snapshot when domain is empty).
 * @param workspace Workspace root path
 * @param id_out Receives existing or created base ID
 * @return 0 on success, error code otherwise
 */
int tpb_rtenv_ensure_base_env(const char *workspace, int32_t *id_out);

/**
 * @brief Resolve active RTEnv ID from $TPB_RTENV_ID, config base_id, or lazy base.
 * @param workspace Workspace root path
 * @param id_out Receives resolved active ID
 * @return 0 on success, error code otherwise
 */
int tpb_rtenv_resolve_active_id(const char *workspace, int32_t *id_out);

/**
 * @brief Merge RTEnv inherit chain for target id (root to leaf).
 */
int tpb_rtenv_merge_chain(const char *workspace, int32_t target_id,
                          tpb_rtenv_merged_t *out);

/**
 * @brief Find merged variable by key; returns NULL if absent.
 */
const tpb_rtenv_merged_var_t *
tpb_rtenv_merged_find(const tpb_rtenv_merged_t *merged, const char *key);

/**
 * @brief Apply on_set to combine record value with current process value.
 * @param key Env var name (for prepend/append reads getenv).
 * @param record_val Value from RTEnv record.
 * @param on_set One of TPB_RTENV_ON_SET_*.
 * @param out Output buffer for resulting value.
 * @param outlen Size of out.
 * @return TPBE_SUCCESS, or TPBE_CLI_FAIL if on_set is ignore (out unchanged).
 */
int tpb_rtenv_apply_onset(const char *key, const char *record_val,
                           uint32_t on_set, char *out, size_t outlen);

/**
 * @brief Scan process environ at kernel init; apply on_get vs active RTEnv.
 * Snapshot is stored until tpb_rtenv_clear_environ_snapshot().
 */
int tpb_rtenv_capture_environ_snapshot(const char *workspace);

/** @brief Release environ snapshot buffers (safe if none). */
void tpb_rtenv_clear_environ_snapshot(void);

/**
 * @brief Append TPB_TASK_ENV_SNAPSHOT_HDR_COUNT environment snapshot headers.
 *
 * Appends after any headers already in *headers (typically input/output).
 * Extra trailing headers may be added later without breaking readers.
 *
 * @param headers Reallocated header array (may be NULL if only env headers).
 * @param nheader In/out header count.
 * @param rec_data Reallocated payload.
 * @param rec_datasize In/out payload size.
 */
int tpb_rtenv_append_env_snapshot_headers(tpb_meta_header_t **headers,
                                          uint32_t *nheader,
                                          void **rec_data,
                                          uint64_t *rec_datasize);

/* ===== rafdb ID Generation ===== */

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
int tpb_raf_gen_tbatch_id(tpb_dtbits_t utc_bits,
                            uint64_t btime,
                            const char *hostname,
                            const char *username,
                            uint32_t front_pid,
                            unsigned char id_out[20]);

/**
 * @brief Copy kernel `.so` SHA-1 digest into KernelID output buffer.
 * @param tpbx_sha1 20-byte kernel module SHA-1
 * @param id_out    20-byte output buffer
 * @return 0 on success, error code otherwise
 */
int tpb_raf_gen_kernel_id(const unsigned char tpbx_sha1[20],
                            unsigned char id_out[20]);

/**
 * @brief Generate TaskRecordID via SHA1.
 * @param utc_bits   Invocation datetime
 * @param btime      Boot time in nanoseconds
 * @param hostname   Hostname string
 * @param username   Username string
 * @param tbatch_id  20-byte TBatchID
 * @param kernel_id  20-byte KernelID
 * @param hdl_id     Handle ID (0-based kernel index in batch)
 * @param pid        Writer process ID
 * @param tid        Writer thread ID
 * @param id_out     20-byte output buffer
 * @return 0 on success, error code otherwise
 */
int tpb_raf_gen_task_id(tpb_dtbits_t utc_bits,
                          uint64_t btime,
                          const char *hostname,
                          const char *username,
                          const unsigned char tbatch_id[20],
                          const unsigned char kernel_id[20],
                          uint32_t hdl_id,
                          uint32_t pid,
                          uint32_t tid,
                          unsigned char id_out[20]);

/**
 * @brief Generate TaskCapsuleRecordID (same inputs as task ID, different
 *        leading hash literal).
 */
int tpb_raf_gen_taskcapsule_id(tpb_dtbits_t utc_bits,
                                 uint64_t btime,
                                 const char *hostname,
                                 const char *username,
                                 const unsigned char tbatch_id[20],
                                 const unsigned char kernel_id[20],
                                 uint32_t hdl_id,
                                 uint32_t pid,
                                 uint32_t tid,
                                 unsigned char id_out[20]);

/**
 * @brief Convert 20-byte ID to 40-char hex string.
 * @param id  20-byte ID
 * @param hex Output buffer, at least 41 bytes (40 hex + NUL)
 */
void tpb_raf_id_to_hex(const unsigned char id[20],
                         char hex[41]);

/**
 * @brief Parse exactly 40 hex digits into a 20-byte ID.
 * @param hex 40 hex digits [0-9A-Fa-f], no prefix, no spaces
 * @param id  Output 20-byte buffer
 * @return 0 on success, TPBE_NULLPTR_ARG or TPBE_CLI_FAIL on error
 */
int tpb_raf_hex_to_id(const char *hex, unsigned char id[20]);

/**
 * @brief Find which domain directory contains a .tpbr for the given ID.
 * @param workspace Workspace root
 * @param id 20-byte record ID
 * @param domain_out Receives TPB_RAF_DOM_TBATCH, _KERNEL, or _TASK
 * @return 0 on success, TPBE_FILE_IO_FAIL if no matching file
 */
int tpb_raf_find_record(const char *workspace,
                          const unsigned char id[20],
                          uint8_t *domain_out);

/** @brief Domain filter: scan all rafdb record domains. */
#define TPB_RAF_DOM_ALL ((uint8_t)0xFF)

/** @brief One match from tpb_raf_scan_records_by_id_prefix(). */
typedef struct tpb_raf_id_match {
    char    id_hex[41]; /**< Lowercase 40-char hex ID plus NUL */
    uint8_t domain;     /**< TPB_RAF_DOM_TBATCH, _KERNEL, or _TASK */
} tpb_raf_id_match_t;

/**
 * @brief Resolve a user path to an existing rafdb .tpbr/.tpbe file.
 * @param workspace Workspace root path.
 * @param inpath User path or basename.
 * @param resolved Output buffer for absolute path.
 * @param resolved_cap Capacity of resolved buffer.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_resolve_record_file(const char *workspace, const char *inpath,
                                char *resolved, size_t resolved_cap);

/**
 * @brief Scan .tpbr files whose record ID hex starts with a prefix.
 * @param workspace Workspace root path.
 * @param id_hex_prefix Lowercase hex prefix.
 * @param prefix_len Prefix length in characters (4-40).
 * @param domain_filter TPB_RAF_DOM_* or TPB_RAF_DOM_ALL.
 * @param matches_out Output allocated array; free with tpb_raf_free_id_matches().
 * @param nmatch_out Output number of matches.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_scan_records_by_id_prefix(const char *workspace,
                                      const char *id_hex_prefix,
                                      size_t prefix_len,
                                      uint8_t domain_filter,
                                      tpb_raf_id_match_t **matches_out,
                                      int *nmatch_out);

/**
 * @brief Free array returned by tpb_raf_scan_records_by_id_prefix().
 * @param matches Array from scan API, may be NULL.
 */
void tpb_raf_free_id_matches(tpb_raf_id_match_t *matches);

/**
 * @brief SHA-1 hash the contents of a regular file.
 * @param filepath Path to readable file.
 * @param sha1_out Output 20-byte digest buffer.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_hash_file(const char *filepath, unsigned char sha1_out[20]);

/** Number of fixed metadata headers appended after parms/metrics in kernel .tpbr */
#define TPB_RAF_KERNEL_META_HDR_COUNT   3

/** Metadata header names in kernel .tpbr */
#define TPB_RAF_KERNEL_HDR_VARIATION    "variation"
#define TPB_RAF_KERNEL_HDR_COMPILATION  "compilation"
#define TPB_RAF_KERNEL_HDR_DEPENDENCY   "dependency"

/**
 * Number of fixed environment snapshot headers appended after input/output
 * headers in task .tpbr (see tpb_rtenv_append_env_snapshot_headers).
 */
#define TPB_TASK_ENV_SNAPSHOT_HDR_COUNT 3

/** Environment snapshot header names in task .tpbr */
#define TPB_TASK_HDR_ENV_KEY            "environment_variable_key"
#define TPB_TASK_HDR_ENV_COUNT          "environment_variable_count"
#define TPB_TASK_HDR_ENV_VALUE          "environment_variable_value"

/** Environment variable: allow overwriting an existing KernelID record */
#define TPB_K_OVERRIDE_ENV              "TPB_K_OVERRIDE"

/**
 * @brief Return nonzero if TPB_K_OVERRIDE allows overwriting KernelID records.
 * @return Nonzero when override is enabled.
 */
int tpb_raf_kernel_override_enabled(void);

/**
 * @brief Find a header index by exact name in a kernel attr.
 * @param attr Kernel attributes from tpb_raf_record_read_kernel().
 * @param name Header name to find.
 * @return Index >= 0, or -1 if not found.
 */
int tpb_raf_kernel_find_header(const kernel_attr_t *attr, const char *name);

/**
 * @brief Read a key from metadata kv payload.
 * @param payload Metadata kv payload string.
 * @param key Dotted key without section prefix.
 * @param value_out Output buffer for value.
 * @param value_len Capacity of value_out.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_kernel_meta_kv_get(const char *payload, const char *key,
                               char *value_out, size_t value_len);

/**
 * @brief Copy a kernel metadata string header payload by name.
 * @param attr Kernel attributes from tpb_raf_record_read_kernel().
 * @param data Record data buffer from read_kernel (may be NULL).
 * @param hdr_name Header name (e.g. TPB_RAF_KERNEL_HDR_VARIATION).
 * @param buf Output buffer for NUL-terminated payload.
 * @param bufsz Capacity of buf.
 * @return TPBE_SUCCESS, TPBE_LIST_NOT_FOUND, or TPBE_NULLPTR_ARG.
 */
int tpb_raf_kernel_get_header_payload(const kernel_attr_t *attr,
                                      const void *data,
                                      const char *hdr_name,
                                      char *buf, size_t bufsz);

/**
 * @brief Build a one-line variation summary from a metadata kv payload.
 *
 * Prefers the kernel.name key when present (formatted as kernel.name=<value>);
 * otherwise returns the first non-format/section line from the payload.
 *
 * @param payload Metadata kv payload string.
 * @param out Output buffer.
 * @param outlen Capacity of out.
 */
void tpb_raf_kernel_variation_summary(const char *payload, char *out,
                                      size_t outlen);

/**
 * @brief Patch active flag in kernel .tpbe entry for a KernelID.
 * @param workspace Workspace root path.
 * @param kernel_id 20-byte KernelID.
 * @param active Active flag (0 or 1).
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_entry_patch_kernel_active(const char *workspace,
                                      const unsigned char kernel_id[20],
                                      uint32_t active);

/**
 * @brief Patch build utc_bits in kernel .tpbe entry for a KernelID.
 * @param workspace Workspace root path.
 * @param kernel_id 20-byte KernelID.
 * @param utc_bits Kernel build/registration datetime (UTC).
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_entry_patch_kernel_utc(const char *workspace,
                                   const unsigned char kernel_id[20],
                                   tpb_dtbits_t utc_bits);

/**
 * @brief Patch active flag in kernel .tpbr fixed attributes.
 * @param workspace Workspace root path.
 * @param kernel_id 20-byte KernelID.
 * @param active Active flag (0 or 1).
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_record_patch_kernel_active(const char *workspace,
                                       const unsigned char kernel_id[20],
                                       uint32_t active);

/**
 * @brief Deactivate all kernel entries with the given name except skip_id.
 * @param workspace Workspace root path.
 * @param kernel_name Logical kernel name.
 * @param skip_id KernelID to keep active.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_kernel_deactivate_same_name(const char *workspace,
                                        const char *kernel_name,
                                        const unsigned char skip_id[20]);

/**
 * @brief Update one metadata key on an existing kernel .tpbr record.
 * @param workspace Workspace root path.
 * @param kernel_id 20-byte KernelID.
 * @param full_key Dotted key with section prefix.
 * @param value New value string.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_kernel_update_meta_key(const char *workspace,
                                   const unsigned char kernel_id[20],
                                   const char *full_key,
                                   const char *value);

/** @brief Mode flag: current UTC datetime for tpb_ts_get_datetime() */
#define TPBM_TS_UTC 0x01u

/**
 * @brief Get current UTC or local datetime components.
 * @param mode TPBM_TS_UTC or TPBM_TS_LOCAL
 * @param dt Output datetime structure
 * @return 0 on success, error code otherwise
 */
int tpb_ts_get_datetime(uint32_t mode, tpb_datetime_t *dt);

/**
 * @brief Pack datetime components into tpb_dtbits_t.
 * @param dt Input datetime
 * @param tz_bias_min Timezone bias in minutes (0 for UTC)
 * @param bits_out Output encoded bits
 * @return 0 on success, error code otherwise
 */
int tpb_ts_datetime_to_bits(const tpb_datetime_t *dt, int16_t tz_bias_min,
                            tpb_dtbits_t *bits_out);

/**
 * @brief Convert 64-bit datetime bits to ISO 8601 UTC string.
 * @param bits Encoded datetime bits.
 * @param str Output ISO string structure.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_ts_bits_to_isoutc(tpb_dtbits_t bits, tpb_datetime_str_t *str);

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

