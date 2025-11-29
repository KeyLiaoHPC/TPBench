/** 
 * @file tpb-types.h
 * @brief Type definitions for TPBench.
 */
#ifndef TPB_TYPES_H
#define TPB_TYPES_H

#include <limits.h>
#include <stdint.h>

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
    int64_t (*tick)(void);
    int64_t (*tock)(void);
    int64_t (*get_stamp)(void);
} tpb_timer_t; 

#endif