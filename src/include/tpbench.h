#ifndef TPBENCH_H
#define TPBENCH_H

#include <stdint.h>
#include <stddef.h>

/* Minimal, flattened TPBench API for kernel development. */

typedef uint64_t TPB_UNIT_T;
typedef uint64_t TPB_DTYPE;
typedef uint64_t TPB_MASK;
typedef uint32_t TPB_DTYPE_U32;

/* Unit attribution and shape */
#define TPB_UATTR_MASK               ((TPB_UNIT_T)0xFFFF000000000000)
#define TPB_UATTR_CAST_MASK          ((TPB_UNIT_T)(1ULL << 48))
#define TPB_UATTR_CAST_Y             ((TPB_UNIT_T)(1ULL << 48))
#define TPB_UATTR_CAST_N             ((TPB_UNIT_T)(0ULL << 48))
#define TPB_UATTR_SHAPE_MASK         ((TPB_UNIT_T)(7ULL << 49))
#define TPB_UATTR_SHAPE_POINT        ((TPB_UNIT_T)(0ULL << 49))
#define TPB_UATTR_SHAPE_1D           ((TPB_UNIT_T)(1ULL << 49))
#define TPB_UATTR_TRIM_MASK          ((TPB_UNIT_T)(1ULL << 52))
#define TPB_UATTR_TRIM_N             ((TPB_UNIT_T)(1ULL << 52))
#define TPB_UATTR_TRIM_Y             ((TPB_UNIT_T)(0ULL << 52))

/* Unit kind/name encoding */
#define TPB_UKIND_MASK          ((TPB_UNIT_T)0x0000F00000000000)
#define TPB_UKIND_UNDEF         ((TPB_UNIT_T)0x0000000000000000)
#define TPB_UKIND_TIME          ((TPB_UNIT_T)0x0000100000000000)
#define TPB_UKIND_VOL           ((TPB_UNIT_T)0x0000200000000000)
#define TPB_UKIND_VOLPTIME      ((TPB_UNIT_T)0x0000300000000000)
#define TPB_UNAME_MASK          ((TPB_UNIT_T)0x0000FFF000000000)
#define TPB_UNAME_UNDEF         ((TPB_UNIT_T)0x0000000000000000)
#define TPB_UNAME_WALLTIME      (((TPB_UNIT_T)0x0000001000000000) | TPB_UKIND_TIME)
#define TPB_UNAME_PHYSTIME      (((TPB_UNIT_T)0x0000002000000000) | TPB_UKIND_TIME)
#define TPB_UNAME_TIMERTIME     (((TPB_UNIT_T)0x0000005000000000) | TPB_UKIND_TIME)
#define TPB_UNAME_DATASIZE      (((TPB_UNIT_T)0x0000001000000000) | TPB_UKIND_VOL)
#define TPB_UNAME_DATAPS        (((TPB_UNIT_T)0x0000006000000000) | TPB_UKIND_VOLPTIME)
#define TPB_UNAME_BITPCY        (((TPB_UNIT_T)0x0000007000000000) | TPB_UKIND_VOLPTIME)

/* Unit base encoding */
#define TPB_UBASE_MASK          ((TPB_UNIT_T)0x0000000F00000000)
#define TPB_UBASE_UNDEF         ((TPB_UNIT_T)0x0000000000000000)
#define TPB_UBASE_BASE          ((TPB_UNIT_T)0x0000000100000000)
#define TPB_UBASE_DEC_EXP_P     ((TPB_UNIT_T)0x0000000700000000)

/* Units used by current kernels */
#define TPB_UNIT_B         (((TPB_UNIT_T)0)  | TPB_UNAME_DATASIZE | TPB_UBASE_BASE)
#define TPB_UNIT_MBPS      (((TPB_UNIT_T)6)  | TPB_UNAME_DATAPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_BYTEPCY   (((TPB_UNIT_T)0)  | TPB_UNAME_BITPCY | TPB_UBASE_BASE)
#define TPB_UNIT_TIMER     (((TPB_UNIT_T)0)  | TPB_UNAME_TIMERTIME | TPB_UBASE_BASE)

/* FLOPS units */
#define TPB_UNAME_FLOPS    (((TPB_UNIT_T)0x0000002000000000) | TPB_UKIND_VOLPTIME)
#define TPB_UNIT_FLOPS     (((TPB_UNIT_T)0)  | TPB_UNAME_FLOPS | TPB_UBASE_BASE)
#define TPB_UNIT_GFLOPS    (((TPB_UNIT_T)9)  | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)
#define TPB_UNIT_TFLOPS    (((TPB_UNIT_T)12) | TPB_UNAME_FLOPS | TPB_UBASE_DEC_EXP_P)

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

/* Kernel registration and runtime APIs used by kernels */
int tpb_k_register(const char *name, const char *note, TPB_K_CTRL kctrl);
int tpb_k_add_parm(const char *name, const char *note,
                   const char *default_value, TPB_DTYPE dtype, ...);
int tpb_k_add_runner(int (*runner)(void));
int tpb_k_add_output(const char *name, const char *note, TPB_DTYPE dtype, TPB_UNIT_T unit);
int tpb_k_get_arg(const char *name, TPB_DTYPE dtype, void *argptr);
int tpb_k_get_timer(tpb_timer_t *timer);
int tpb_k_alloc_output(const char *name, uint64_t n, void *ptr);
int tpb_k_finalize_pli(void);
int tpb_k_pli_set_timer(const char *timer_name);
int tpb_k_pli_build_handle(tpb_k_rthdl_t *handle, int argc, char **argv);
int tpb_driver_clean_handle(tpb_k_rthdl_t *handle);

/* CLI output helpers */
void tpb_printf(uint64_t mode_bit, char *fmt, ...);
int tpb_cliout_args(tpb_k_rthdl_t *handle);
int tpb_cliout_results(tpb_k_rthdl_t *handle);

#endif /* TPBENCH_H */
