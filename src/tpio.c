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
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include "tpio.h"
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


int
tpb_writecsv(char *path, uint64_t **data, int nrow, int ncol, char *header, int tran_flag) {
    int err, i, j;
    FILE *fp;    

    fp = fopen(path, "w");
    if(fp == NULL) {
        return FILE_OPEN_FAIL;
    }
    fprintf(fp, "%s\n", header);

    // data[col][row], for kernel benchmark
    if(tran_flag) {
        for(i = 0; i < nrow; i ++) {
            for(j = 0; j < ncol - 1; j ++) {
                fprintf(fp, "%llu,", data[j][i]);
            }
            fprintf(fp, "%llu\n", data[ncol-1][i]);
        }
    }
    // data[row][col], for group benchmark
    else {
        for(i = 0; i < nrow; i ++) {
            for(j = 0; j < ncol - 1; j ++) {
                fprintf(fp, "%llu,", data[i][j]);
            }
            fprintf(fp, "%llu\n", data[i][ncol-1]);
        }
    }
    fflush(fp);
    fclose(fp);

    return NO_ERROR;
}

// tpbench printf wrapper. 
int
tpprintf(int err, int ts_flag, int tag_flag, char *fmt, ...) {
    
    int err_op = 0;
    time_t t = time(0);
    struct tm* lt = localtime(&t);
    char tag[5], err_msg[32];
    
    // print splitter directly.
    if(strcmp(fmt, HLINE) == 0 || strcmp(fmt, DHLINE) == 0) {
        // only allow rank 0 to print
        if(tpmpi_info.myrank) {
            return err_op;
        }
        printf("\n%s", fmt);
        return err_op;
    }
    
    if(err == 0) {
        // no error
        err_op = 0;
        if(tpmpi_info.myrank) {
            return err_op;
        }
        sprintf(tag, "NOTE");
    }
    else if(err > 100) {
        // warning
        err_op = 0;
        if(tpmpi_info.myrank) {
            return err_op;
        }
        sprintf(tag, "WARN");
    } 
    else {
        // error
        err_op = 1;
        if(tpmpi_info.myrank) {
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

    // transport va_arg list to vprintf
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    // on error exit, print error message.
    if(err != 0 &&  err < 100) {
        printf("\n%04d-%02d-%02d %02d:%02d:%02d [EXIT] ", 
               lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, 
               lt->tm_hour, lt->tm_min, lt->tm_sec);
        printf("TPBench exit with error %s.\n" DHLINE, tpb_geterr(err, err_msg));
    }
    fflush(stdout);

    return err_op;
}

void
tpb_list(){
    tpprintf(0, 1, 1, "Listing supported kernel and groups.\n");
    tpprintf(0, 0, 0, HLINE);
    tpprintf(0, 0, 0, "Kernel          Routine         NOTE\n");
    tpprintf(0, 0, 0, HLINE);
    for(int i = 0 ; i < nkrout; i ++) {
        tpprintf(0, 0, 0, "%-12s    %-12s    %s\n", 
                   kern_info[i].kname, kern_info[i].rname, kern_info[i].note);
    }
    tpprintf(0, 0, 0, HLINE);
    tpprintf(0, 0, 0, "GROUP           Routine         NOTE\n");
    tpprintf(0, 0, 0, HLINE);
    for(int i = 0 ; i < ngrout; i ++) {
        tpprintf(0, 0, 0, "%-12s    %-12s    %s\n",
                   grp_info[i].gname, grp_info[i].rname, grp_info[i].note);
    }
    tpprintf(0, 0, 0, DHLINE);
}

char *tpb_geterr(const int err, char *buf) {
    switch (err) {
        case NO_ERROR:
            sprintf(buf, "NO_ERROR");
            break;
        case GRP_ARG_ERROR:
            sprintf(buf, "GRP_ARG_ERROR");
            break;
        case KERN_ARG_ERROR:
            sprintf(buf, "KERN_ARG_ERROR");
            break;
        case KERN_NE:
            sprintf(buf, "KERN_NE");
            break;
        case GRP_NE:
            sprintf(buf, "GRP_NE");
            break;
        case SYNTAX_ERROR:
            sprintf(buf, "SYNTAX_ERROR");
            break;
        case FILE_OPEN_FAIL:
            sprintf(buf, "FILE_OPEN_FAIL");
            break;
        case MALLOC_FAIL:
            sprintf(buf, "MALLOC_FAIL");
            break;
        case ARGS_MISS:
            sprintf(buf, "ARGS_MISS");
            break;
        case MKDIR_ERROR:
            sprintf(buf, "MKDIR_ERROR");
            break;
        case RES_INIT_FAIL:
            sprintf(buf, "RES_INIT_FAIL");
            break;
        case MPI_INIT_FAIL:
            sprintf(buf, "MPI_INIT_FAIL");
            break;
        case VERIFY_FAIL:
            sprintf(buf, "VERIFY_FAIL");
            break;
        case OVER_OPTMIZE:
            sprintf(buf, "OVER_OPTIMIZE");
            break;
        case DEFAULT_DIR:
            sprintf(buf, "DEFAULT_DIR");
            break;
        default:
            break;
    }
    return buf;
}