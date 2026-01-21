/**
 * @file tpb-types.h
 * @brief Type definitions for TPBench.
 */

#ifndef TPB_TYPES_H
#define TPB_TYPES_H
#define TPB_VERSION 0.8

#include <limits.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stddef.h>
#include "tpb-unitdefs.h"

/** @brief Maximum CLI string length */
#define TPBM_CLI_STR_MAX_LEN 4096

/** @brief Maximum name string length */
#define TPBM_NAME_STR_MAX_LEN 256

/** @brief Maximum note string length */
#define TPBM_NOTE_STR_MAX_LEN 2048

/** @brief Maximum number of kernels */
#define TPBM_CLI_K_MAX 128

/* Print modes */
#define TPBM_PRTN_M_DIRECT 0x00  // directly print, ignore headers
#define TPBM_PRTN_M_TS 0x01  // only print timestamp header
#define TPBM_PRTN_M_TAG 0x02  // only print tag header
#define TPBM_PRTN_M_TSTAG 0x03  // print both timestamp and tag headers

/* Error tags */
#define TPBE_NOTE   0x00  // tag: NOTE
#define TPBE_WARN   0x10  // tag: WARN
#define TPBE_FAIL   0x20  // tag: FAIL
#define TPBE_UNKN   0x30  // tag: UNKN

typedef uint64_t TPB_DTYPE;
typedef uint64_t TPB_MASK;
typedef uint32_t TPB_DTYPE_U32;

/*
 * Type definition for parameter data types
 * Format: 0xSSCCTTTT where:
 *   SS   = Parameter Source (bits 24-31)
 *   CC   = Check Mode (bits 16-23)
 *   TTTT = Type Code (bits 0-15)
 */

/* Parameter source flags (bits 24-31) */
#define TPB_PARM_SOURCE_MASK    ((TPB_MASK)0xFF000000)
#define TPB_PARM_CLI            ((TPB_DTYPE)0x01000000)  // parameter from CLI
#define TPB_PARM_MACRO          ((TPB_DTYPE)0x02000000)  // parameter from macro
#define TPB_PARM_FILE           ((TPB_DTYPE)0x04000000)  // parameter from config file
#define TPB_PARM_ENV            ((TPB_DTYPE)0x08000000)  // parameter from env var

/* Parameter validation/check mode flags (bits 16-23) */
#define TPB_PARM_CHECK_MASK ((TPB_MASK)0x00FF0000)
#define TPB_PARM_NOCHECK    ((TPB_DTYPE)0x00000000)  // no check
#define TPB_PARM_RANGE      ((TPB_DTYPE)0x00010000)  // check range [lo, hi]
#define TPB_PARM_LIST       ((TPB_DTYPE)0x00020000)  // check against list
#define TPB_PARM_CUSTOM     ((TPB_DTYPE)0x00040000)  // custom check pfunc

/* Parameter type flags (bits 0-15) - Aligned with MPI_Datatype encoding */
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

/** Kernel integration types */
typedef uint32_t TPB_K_CTRL;
#define TPB_KTYPE_MASK      ((TPB_K_CTRL)0x0000000F)
#define TPB_KTYPE_FLI       ((TPB_K_CTRL)(1 << 0))
#define TPB_KTYPE_PLI       ((TPB_K_CTRL)(1 << 1))
#define TPB_KTYPE_ALI       ((TPB_K_CTRL)(1 << 2))

/** Error codes */
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
    TPBE_KERNEL_NE_FAIL,    // Kernel not exist
    TPBE_KARG_NE_FAIL       // Kernel argument not exist
};
typedef enum _tpb_errno tpb_errno_t;

typedef struct _tpb_error {
    tpb_errno_t err_code;
    unsigned err_type;
    char err_msg[256];
} tpb_error_type;

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

/** @brief TPBench run-time parameters */
typedef struct tpb_args {
    tpb_timer_t timer;
    int nkern;
} tpb_args_t;

/** @brief Metric-unit pair for performance reporting */
typedef struct tpb_k_metric {
    char *metric;
    char *unit;
} tpb_k_metric_t;

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
    TPB_DTYPE ctrlbits;  // TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK
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
    TPB_K_CTRL kctrl;                    /**< Kernel integration type: DLI, SPI, SLI */
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

/** @brief Runtime handle for kernel execution */
typedef struct tpb_k_rthdl {
    tpb_argpack_t argpack;
    tpb_respack_t respack;
    tpb_kernel_t kernel;
} tpb_k_rthdl_t;

/** @brief TPBench result data file struct */
typedef struct tpb_res {
    char header[1024];
    char fname[1024];
    char fdir[PATH_MAX];
    char fpath[PATH_MAX];
    int64_t **data;
} tpb_res_t;

/** @brief CLI output format configuration structure */
typedef struct tpb_cliout_format {
    int max_col;
    size_t nq;
    double *qtiles;
    int sigbit, intbit;
    int initialized;
} tpb_cliout_format_t;

#endif /* TPB_TYPES_H */
