/** 
 * @file tpb-types.h
 * @brief Type definitions for TPBench.
 */
#ifndef TPB_TYPES_H
#define TPB_TYPES_H

#include <limits.h>
#include <stdint.h>

typedef struct tpb_res {
    char header[1024];
    char fname[1024];
    char fdir[PATH_MAX];
    char fpath[PATH_MAX];
    int64_t **data; //data[col][row], row for run id, col for different tests.
} tpb_res_t;

typedef struct {
    int (*init)(void);
    int64_t (*tick)(void);
    int64_t (*tock)(void);
    int64_t (*get_stamp)(void);
} tpb_timer_t; 

#endif