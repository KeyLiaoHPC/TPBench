/**
  * =================================================================================
  * TPBench - A high-precision throughputs benchmarking tool for HPC
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
  * 
  * @file main.c
  * @version 0.71
  * @brief   main entry for tpbench
  * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
  * @date 2024-01-22
  */


#define _GNU_SOURCE
#define VER "0.71"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <string.h>
#include <limits.h>
#include "tpb-types.h"
#include "tpb-cli.h"
#include "tpb-core.h"
#include "tpb-io.h"
#include "tpmpi.h"

#ifdef USE_MPI
#include "mpi.h"
#endif

struct tpmpi_info_t tpmpi_info;

int init_res(char *prefix, char *posfix, char *host_dir, tpb_args_t *args, tpb_res_t *res);

// =============================================================================
// utilities
// =============================================================================

// init result data structure
int
init_res(char *prefix, char *posfix, char *hostname, tpb_args_t *args, tpb_res_t *res) {
    int err;
    // print headers
    res->header[0] = '\0';

    if(strcmp(prefix, "kernels") == 0) {
        // matched, header for kernel benchmark
        for(int i = 0; i < args->nkern - 1; i ++){
            err = sprintf(res->header, "%s,", strcat(res->header, kern_info[args->klist[i]].rname));
            __error_lt(err, 0, RES_INIT_FAIL);
        }
        int i = args->nkern - 1;
        err = sprintf(res->header, "%s", strcat(res->header, kern_info[args->klist[i]].rname));
        __error_lt(err, 0, RES_INIT_FAIL);
    }
    // print fname
    err = sprintf(res->fname, "%s-r%d_c%d-%s.csv", 
                  prefix, tpmpi_info.myrank, tpmpi_info.pcpu, posfix);
    __error_lt(err, 0, RES_INIT_FAIL);
    // print fdir
    err = sprintf(res->fdir, "%s/%s", args->data_dir, hostname);
    __error_lt(err, 0, RES_INIT_FAIL);
    // print fpath
    err = sprintf(res->fpath, "%s/%s", res->fdir, res->fname);
    __error_lt(err, 0, RES_INIT_FAIL);
    
    return 0;
}


/**
 * @brief main entry of tpbench.x
 */
int
main(int argc, char **argv) {
    // process info
    int err;
    tpb_args_t tp_args;
    tpb_timer_t timer;
    char filename[1024], mydir[PATH_MAX], full_path[PATH_MAX], hostname[128]; 
    tpb_res_t time_arr, kib;

// =============================================================================
// Initialization
// =============================================================================
    // Init process info
    tpmpi_init();
    tpprintf(0, 0, 0, DHLINE);
#ifdef USE_MPI
    tpprintf(0, 0, 0, "TPBench-MPI v" VER "\n");
#else
    tpprintf(0, 0, 0, "TPBench v" VER "\n");
#endif
    // init kernel, init tpbench arguments
    tpprintf(0, 1, 1, "Initializing TPBench kernels.");
    err = tpb_init();
    if(err) {
        err = tpprintf(err, 1, 1, "");
        if(err) exit(1);
    }
    
    // tpprintf(0, 1, 1, "nkrout = %d, ngrout = %d", nkrout, ngrout);
    err = tpb_parse_args(argc, argv, &tp_args, &timer);
    if(err) {
        err = tpprintf(err, 1, 1, "In arg parsing.", err);
        __error_exit(err);
    }

    // List only.
    if(tp_args.list_only_flag){
        tpb_list();
        tpmpi_exit();
        exit(0);
    }

    // print kernel list
    if(tp_args.nkern) {
        tpprintf(0, 1, 1, "Kernel routine: ");
        for(int i = 0; i < tp_args.nkern; i ++) {
            tpprintf(0, 0, 0, "%s, ", kern_info[tp_args.klist[i]].rname);
        }
    }
    
    tpprintf(0, 1, 1, "Each routine will be tested %d times.", tp_args.ntest);


    // create host dir
    gethostname(hostname, 128);
    sprintf(mydir, "%s/%s", tp_args.data_dir, hostname);
    err = tpb_mkdir(mydir);
    __error_fun(err, "Cannot create data directory.");
    tpprintf(err, 1, 1, "Host dir %s", mydir);

    // kernel benchmark
    if(tp_args.nkern) {
        // Struct initialization
        err = init_res("kernels", "ns", hostname, &tp_args, &time_arr);
        __error_fun(err, "Kernel data space init failed.");

        // allocate space for data
        time_arr.data = (int64_t **)malloc(tp_args.nkern * sizeof(int64_t *));
        for(int i = 0; i < tp_args.nkern; i ++) {
            time_arr.data[i] = (int64_t *)malloc(tp_args.ntest * sizeof(int64_t));
        }
        
        // Run kernels.
        for(int i = 0; i < tp_args.nkern; i ++) {
            tpprintf(err, 1, 1, "Kernel routine %s started.\n" HLINE, kern_info[tp_args.klist[i]].rname);
            err = tpb_run_kernel(tp_args.klist[i], 
                                 &timer,
                                 tp_args.ntest, 
                                 time_arr.data[i],
                                 tp_args.nkib);
            __error_fun(err, "Benchmark failed.");
            tpprintf(err, 1, 1, "Finished.");
        }

        // Write raw data to csv files.
        err = tpb_writecsv(time_arr.fpath, time_arr.data, tp_args.ntest, tp_args.nkern, time_arr.header);
        __error_fun(err, "Writing ns csv failed.");
        // Clean up
        for(int i = 0; i < tp_args.nkern; i ++) {
            free(time_arr.data[i]);
        }
        free(time_arr.data);
    }
    
    // end of benchmark
    tpprintf(0, 1, 1, "\nTPBench finished.\n" DHLINE);
    tpmpi_exit();

    return 0;
}
