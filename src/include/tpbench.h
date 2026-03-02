/**
 * @file tpbench.h
 * @brief Public API header for TPBench kernel development.
 *
 * This is the single source of truth for all public types, macros, and
 * function declarations used by kernel code.  Internal corelib headers
 * include this file and add internal-only definitions on top.
 */

#ifndef TPBENCH_H
#define TPBENCH_H

#include <stdint.h>
#include <stddef.h>
#include "tpb-unitdefs.h"

typedef uint64_t TPB_DTYPE;
typedef uint64_t TPB_MASK;
typedef uint32_t TPB_DTYPE_U32;

/* Limits */
#define TPBM_CLI_STR_MAX_LEN 4096
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
#define TPB_PARM_SOURCE_MASK    ((TPB_MASK)0xFF000000)
#define TPB_PARM_CLI            ((TPB_DTYPE)0x01000000)
#define TPB_PARM_MACRO          ((TPB_DTYPE)0x02000000)
#define TPB_PARM_FILE           ((TPB_DTYPE)0x04000000)
#define TPB_PARM_ENV            ((TPB_DTYPE)0x08000000)

/* Parameter validation/check mode flags (bits 16-23) */
#define TPB_PARM_CHECK_MASK ((TPB_MASK)0x00FF0000)
#define TPB_PARM_NOCHECK    ((TPB_DTYPE)0x00000000)
#define TPB_PARM_RANGE      ((TPB_DTYPE)0x00010000)
#define TPB_PARM_LIST       ((TPB_DTYPE)0x00020000)
#define TPB_PARM_CUSTOM     ((TPB_DTYPE)0x00040000)

/* Parameter type flags (bits 0-15) */
#define TPB_PARM_TYPE_MASK  ((TPB_MASK)0x0000FFFF)
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

/* Kernel integration types */
typedef uint32_t TPB_K_CTRL;
#define TPB_KTYPE_MASK      ((TPB_K_CTRL)0x0000000F)
#define TPB_KTYPE_FLI       ((TPB_K_CTRL)(1 << 0))
#define TPB_KTYPE_PLI       ((TPB_K_CTRL)(1 << 1))
#define TPB_KTYPE_ALI       ((TPB_K_CTRL)(1 << 2))

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
    TPBE_DLOPEN_FAIL
};
typedef enum _tpb_errno tpb_errno_t;

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
    tpb_parm_value_t default_value;
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
    TPB_K_CTRL kctrl;
    int nparms, nouts;
    tpb_rt_parm_t *parms;
    tpb_k_output_t *outs;
} tpb_k_static_info_t;

/** @brief Kernel function pointers */
typedef struct tpb_k_func {
    int (*k_run)(void);
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
 * @param default_value String representation of default value
 * @param dtype Combined data type: source | check | type
 * @param ... Variable arguments based on validation mode:
 *            - TPB_PARM_RANGE: (lo, hi) range bounds
 *            - TPB_PARM_LIST: (n, plist) valid values
 *            - TPB_PARM_NOCHECK: no additional arguments
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_parm(const char *name, const char *note,
                   const char *default_value, TPB_DTYPE dtype, ...);

/**
 * @brief Set the runner function for the current kernel.
 * @param runner Function pointer to kernel runner
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_runner(int (*runner)(void));

/**
 * @brief Register a new output data definition for the current kernel.
 *
 * Must be called during kernel registration (after tpb_k_register,
 * before tpb_k_add_runner) to define output metrics.
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

/* ===== PLI (Process-Level Integration) API ===== */

/**
 * @brief Finalize PLI kernel registration.
 *
 * For PLI kernels, this replaces tpb_k_add_runner(). It increments the kernel
 * count without setting a runner function (PLI kernels use exec instead).
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

#endif /* TPBENCH_H */
