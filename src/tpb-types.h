/** 
 * @file tpb-types.h
 * @brief Type definitions for TPBench.
 */
#ifndef TPB_TYPES_H
#define TPB_TYPES_H

#include <limits.h>
#include <stdint.h>

/**
 * @brief Error codes for tpbench. Error has three levels:[NOTE], [WARNING], [FAIL].
 */
enum _tpb_errno {
    TPBE_SUCCESS = 0,
    TPBE_WARNING,
    VERIFY_FAIL,
    OVER_OPTMIZE,
    DEFAULT_DIR,
    CORE_NOT_BIND,
    TPBE_FAIL,
    TPBE_EXIT_ON_HELP,
    GRP_ARG_ERROR,
    KERN_ARG_ERROR,
    KERN_NE,
    GRP_NE,
    TPBE_CLI_SYNTAX_FAIL,
    FILE_OPEN_FAIL,
    MALLOC_FAIL,
    ARGS_MISS,
    MKDIR_ERROR,
    RES_INIT_FAIL,
    TPBE_MPI_INIT_FAIL,
};

typedef enum _tpb_errno tpb_errno_t;
typedef struct _tpb_error {
    tpb_errno_t err_code;
    int err_type;
    char err_msg[256];
} tpb_error_type;

/**
 * @brief tpbench argument struct
 */
typedef struct tpb_args {
    int mode; // [Optional] The flag for socre benchmarking.
    uint64_t ntest; // [Mandatory] Number of tests per job.
    uint64_t nkib; // [Mandatory] Number of ngrps and nkerns
    char kstr[1024], data_dir[1024]; // [Mandatory] group and kernels name
    char timer[128];
    int *klist; 
    int nkern;
    int list_only_flag; // [Optinal] flags for list mode and consecutive run
} tpb_args_t;

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