/**
 * TPBench - A throughputs benchmarking tool for high-performance computing
 * 
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later 
 * version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with 
 * this program. If not, see https://www.gnu.org/licenses/.
 * =================================================================================
 * @file tpb_io.h
 * @version 0.3
 * @brief Header for handling input/output of tpbench.
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */

#define _GNU_SOURCE

#include <stdint.h>
#include <limits.h>
#include "tpmpi.h"

/**
 * @brief 
 */
typedef struct __result {
    char header[1024];
    char fname[1024];
    char fdir[PATH_MAX];
    char fpath[PATH_MAX];
    uint64_t **data; //data[col][row], row for run id, col for different tests.
} __res_t;




/**
 * @brief 
 */
void tpb_list();

/**
 * @brief 
 * @param dirpath 
 * @return int 
 */
int tpb_mkdir(char *dirpath);

/**
 * @brief TPBench format stdout module. Return error level according to error number. 
 *        Print message if tpmpi_info.myrank == 0. Output syntax:
 *        YYYY-mm-dd HH:MM:SS [TAG ] *msg
 * @param err   Error number described in tperror.h
 * @param ts    Set to 0 to ignore timestamp.
 * @param tag   Set to 0 to ignore error tag.
 * @param msg   char *msg, same syntax as printf.
 * @param ...   varlist, same syntax as printf.
 * @return int  return 0 if error is NOTE or WARN, 1 if critical error.
 */
int tpprintf(int err, int ts_flag, int tag_flag, char *fmt, ...);

/**
 * @brief write 2d data with header into a csv file.
 * @param path 
 * @param data 
 * @param nrow 
 * @param ncol 
 * @param header 
 * @param tran_flag 
 * @return int 0 or 1, set 1 when writing order is different from [row][col]
 */
int tpb_writecsv(char *path, uint64_t **data, int nrow, int ncol, char *header, int tran_flag);