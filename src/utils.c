/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
 * 
 * Copyright (C) 2020 Key Liao (Liao Qiucheng)
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
 * 
 * =================================================================================
 * utils.c
 * Description: some accessory functions. 
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>     /* PATH_MAX */
#include <sys/stat.h>   /* mkdir(2) */
#include <errno.h>
#include "tpbench.h"


int
make_dir(const char *path) {
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p; 

    errno = 0;

    // Copy string so its mutable
    if(len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1; 
    }   
    strcpy(_path, path);

    // Iterate the string
    for(p = _path + 1; *p; p++) {
        if (*p == '/') {
            // Temporarily truncate 
            *p = '\0';

            if(mkdir(_path, S_IRWXU) != 0) {
                if(errno != EEXIST)
                    return -1; 
            }

            *p = '/';
        }
    }   

    if(mkdir(_path, S_IRWXU) != 0){
        if(errno != EEXIST)
            return -1; 
    }   
    return 0;
}

/*
 * function: write_res
 * description: write a 2d data result to prefix_r?_c?_pofix.csv
 * ret: 
 * int, error code
 * args:
 *     char *dir [in]: data directory 
 *     uint64_t **data [in]: pointer to 2d array
 *     int ndim1 [in]: number of dim1 elements. (data[dim1][dim2])
 *     int ndim2 [in]: number of dim2 elements. (data[dim1][dim2])
 *     char *prefix [in]: prefix of filename. 
 *     char *posfix [in]: postfix of filename.
 */ 
int
write_csv(char *path, uint64_t **data, int ndim1, int ndim2, char *headers) {
    int err, i, j;
    FILE *fp;    

    fp = fopen(path, "w");
    if(fp == NULL) {
        return FILE_OPEN_FAIL;
    }
    fprintf(fp, "%s\n", headers);
    for(i = 0; i < ndim1; i ++) {
        for(j = 0; j < ndim2 - 1; j ++) {
            fprintf(fp, "%llu,", data[i][j]);
        }
        fprintf(fp, "%llu\n", data[i][ndim2-1]);
    }
    fflush(fp);
    fclose(fp);

    return NO_ERROR;
}