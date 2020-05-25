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
 * tpio.c
 * Description: some accessory functions. 
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include "tpb_core.h"

int
tpb_mkdir(char *path) {


    int err;
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

/**
 * @brief 
 * 
 * @param path 
 * @param data 
 * @param nrow 
 * @param ncol 
 * @param header 
 * @return int 
 */
int
tpb_writecsv(char *path, uint64_t **data, int nrow, int ncol, char *header) {
    int err, i, j;
    FILE *fp;    

    fp = fopen(path, "w");
    if(fp == NULL) {
        return FILE_OPEN_FAIL;
    }
    fprintf(fp, "%s\n", header);
    for(i = 0; i < nrow; i ++) {
        for(j = 0; j < ncol - 1; j ++) {
            fprintf(fp, "%llu,", data[j][i]);
        }
        fprintf(fp, "%llu\n", data[ncol-1][i]);
    }
    fflush(fp);
    fclose(fp);

    return NO_ERROR;
}

int
tpb_printf(int err, int ts_flag, int tag_flag, char *fmt, ...) {
    
    int err_op = 0;
    time_t t = time(0);
    struct tm* lt = localtime(&t);
    char tag[5];
    
    if(strcmp(fmt, HLINE) == 0 || strcmp(fmt, DHLINE) == 0) {
        if(myrank) {
            return err_op;
        }
        printf("%s", fmt);
        return err_op;
    }
    if(err == 0) {
        err_op = 0;
        if(myrank) {
            return err_op;
        }
        sprintf(tag, "NOTE");
    }
    else if(err > 100) {
        err_op = 0;
        if(myrank) {
            return err_op;
        }
        sprintf(tag, "WARN");
    } 
    else {
        err_op = 1;
        if(myrank) {
            return err_op;
        }
        sprintf(tag, "FAIL");
    }
    if(ts_flag) {
        printf("\n%04d-%02d-%02d %02d:%02d:%02d ", 
               lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, 
               lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
    if(tag_flag) {
        if(ts_flag) {
            printf("[%s] ", tag); 
        }
        else {
            printf("\n[%s] ", tag);
        }
        
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    return err_op;
}

void
tpb_list(){
    tpb_printf(0, 1, 1, "Listing supported kernel and groups.\n");
    tpb_printf(0, 0, 0, HLINE);
    tpb_printf(0, 0, 0, "Kernel          Routine         NOTE\n");
    tpb_printf(0, 0, 0, HLINE);
    for(int i = 0 ; i < nkrout; i ++) {
        tpb_printf(0, 0, 0, "%-12s    %-12s    %s\n", 
                   kern_info[i].kname, kern_info[i].rname, kern_info[i].note);
    }
    tpb_printf(0, 0, 0, HLINE);
    tpb_printf(0, 0, 0, "GROUP           Routine         NOTE\n");
    tpb_printf(0, 0, 0, HLINE);
    for(int i = 0 ; i < ngrout; i ++) {
        tpb_printf(0, 0, 0, "%-12s    %-12s    %s\n",
                   grp_info[i].gname, grp_info[i].rname, grp_info[i].note);
    }
    tpb_printf(0, 0, 0, DHLINE);
}
