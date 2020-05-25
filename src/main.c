/**
  * =================================================================================
  * TPBench - A high-precision throughputs benchmarking tool for HPC
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
  * =================================================================================
  * 
  * @file main.c
  * @version 0.3
  * @brief   main entry for tpbench
  * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
  * @date 2020-05-22
  */


#define _GNU_SOURCE
#define VER "0.3"

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include "tpbench.h"

#ifdef USE_MPI
#include "mpi.h"
#endif

void mpi_sync();
void mpi_exit();
int init_res(char *prefix, char *posfix, char *host_dir, __tp_args_t *args, __res_t *res);

// =============================================================================
// utilities
// =============================================================================

// process synchronization
void
mpi_sync() {
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    return;
}

// mpi exit
void 
mpi_exit() {
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif   
    return;
}

// init result data structure
int
init_res(char *prefix, char *posfix, char *hostname, __tp_args_t *args, __res_t *res) {
    int err;
    // print headers
    res->header[0] = '\0';
    if(strcmp(prefix, "kernels")) {
        for(int i = 0; i < args->nkern; i ++){
            err = sprintf(res->header, "%s", strcat(res->header, kern_info[i].rname));
            __error_lt(err, 0, RES_INIT_FAIL);
        }
    } else{
        for(int i = 0; i < args->nkern; i ++){
            err = sprintf(res->header, "%s", strcat(res->header, grp_info[i].rname));
            __error_lt(err, 0, RES_INIT_FAIL);
        }
    }
    // print fname
    err = sprintf(res->fname, "%s-r%d_c%d-%s.csv", prefix, myrank, mycpu, posfix);
    __error_lt(err, 0, RES_INIT_FAIL);
    // print fdir
    err = sprintf(res->fdir, "%s/%s", args->data_dir, hostname);
    __error_lt(err, 0, RES_INIT_FAIL);
    // print fpath
    err = sprintf(res->fpath, "%s/%s", res->fdir, res->fname);
    __error_lt(err, 0, RES_INIT_FAIL);
    // allocate space for data
    res->data = (uint64_t **)malloc(args->nkern * sizeof(uint64_t *));
    if(res->data == NULL) {
            return MALLOC_FAIL;
        }
    for(int i = 0; i < args->ntest; i ++) {
        res->data[i] = (uint64_t *)malloc(args->ngrp* sizeof(uint64_t));
        if(res->data[i] == NULL) {
            return MALLOC_FAIL;
        }
    }

    return NO_ERROR;
}


/**
 * @brief main entry of tpbench.x
 */
int
main(int argc, char **argv) {
    // process info
    int err;
    __tp_args_t tp_args;
#ifdef USE_MPI
    double **partps; // cpu cys, parallel cys
#endif

// =============================================================================
// Initialization
// =============================================================================
    // Init process info
    nrank = 1;
    myrank = 0;
    mycpu = sched_getcpu();
    tpb_printf(0, 0, 0, DHLINE);
    tpb_printf(0, 0, 0, "TPBench v" VER "\n");

    // init mpi rank info
#ifdef USE_MPI
    if(myrank == 0) {
        tpb_printf(0, 1, 1, "Initializing MPI.");
    }
    err = MPI_Init(NULL, NULL);
    if(err != MPI_SUCCESS) {
        err = tpb_printf(err, "MPI Initialization failed.");
        __error_exit(err);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
#endif //#ifdef USE_MPI

    // finished getting process info
    // init kernel, init tpbench arguments
    tpb_printf(0, 1, 1, "Initializing TPBench kernels.");
    err = tpb_init();
    if(err) {
        err = tpb_printf(err, 1, 1, "");
        if(err) exit(1);
    }
    tpb_printf(0, 1, 1, "nkrout = %d, ngrout = %d", nkrout, ngrout);
    err = parse_args(argc, argv, &tp_args);
    if(err) {
        err = tpb_printf(err, 1, 1, "ERROR: %d\n" DHLINE, err);
        if(err) exit(1);
    }

    // print args for debugging
    tpb_printf(0, 1, 1, "ntest = %d, nkib = %d", tp_args.ntest, tp_args.nkib);
    for(int i = 0; i < tp_args.nkern; i ++) {
        tpb_printf(0, 1, 1, "%d, ", tp_args.klist[i]);
    }
    for(int i = 0; i < tp_args.ngrp; i ++) {
        tpb_printf(0, 1, 1, "%d, ", tp_args.glist[i]);
    }
    
    // end args debugging
    // List only.
    if(tp_args.list_only_flag){
        tpb_list();
        mpi_exit();
        exit(0);
    }
// =============================================================================
// Benchmarking 
// =============================================================================
    // headers, output csv name, host data dir, full path for csvs, hostname
    char filename[1024], mydir[PATH_MAX], full_path[PATH_MAX], hostname[128]; 
    __res_t grp_ns, grp_cy, kern_ns, kern_cy;
    // create host dir
    gethostname(hostname, 128);
    sprintf(mydir, "%s/%s", tp_args.data_dir, hostname);
    err = tpb_mkdir(mydir);
    err = tpb_printf(err, 1, 1, "Host dir %s", mydir);
    if(err) {
        exit(1);
    }
    // kernel benchmark
    if(nkern) {
        // Struct initialization
        err = init_res("kernels", "ns", hostname, &tp_args, &kern_ns);
        if(err) {
            err = tpb_printf(err, 1, 1, "In kernel nanosec space init.");
            __error_exit(err);
        }
        err = init_res("kernels", "cy", hostname, &tp_args, &kern_cy);
        if(err) {
            err = tpb_printf(err, 1, 1, "In kernel cycle space init.");
            __error_exit(err);
        }
        // Run kernels.
        for(int i = 0; i < tp_args.nkern; i ++) {
            err = tpb_run_kernel(tp_args.klist[i], 
                                 tp_args.ntest, 
                                 kern_ns.data[i], 
                                 kern_cy.data[i], 
                                 tp_args.nkern);
            if(err) {
               err = tpb_printf(err, 1, 1, "In kernel benchmarking.");
               __error_exit(err);
            }
            else {
                tpb_printf(err, 1, 1, "Finish kernel routine %s.", kern_info[tp_args.klist[i]].rname);
            }
        }
        // Write to csv files.
        err = tpb_writecsv(kern_ns.fpath, kern_ns.data, tp_args.ntest, tp_args.nkern, kern_ns.header);
        if(err) {
            err = tpb_printf(err, 1, 1, "In kernel nanosec writing.");
            __error_exit(err);
        }
        err = tpb_writecsv(kern_cy.fpath, kern_cy.data, tp_args.ntest, tp_args.nkern, kern_cy.header);
        if(err) {
            err = tpb_printf(err, 1, 1, "In kernel nanosec writing.");
            __error_exit(err);
        }
    }
    // group benchmark
    if(ngrp) {
        for(int i = 0; i < tp_args.ngrp; i ++) {
            // Struct initialization
            err = init_res(grp_info[tp_args.glist[i]].rname, "ns", hostname, &tp_args, &grp_ns);
            if(err) {
                err = tpb_printf(err, 1, 1, "In group nanosec init.");
                __error_exit(err);
            }
            err = init_res(grp_info[tp_args.glist[i]].rname, "cy", hostname, &tp_args, &grp_cy);   
            if(err) {
                err = tpb_printf(err, 1, 1, "In group cycle init.");
                __error_exit(err);
            }
            err = tpb_run_group(tp_args.glist[i],
                                tp_args.ntest,
                                grp_ns.data,
                                grp_cy.data,
                                tp_args.nkib);
            if(err) {
                err = tpb_printf(err, 1, 1, "In group benchmarking.");
                __error_exit(err);
            }
            else {
                tpb_printf(err, 1, 1, "Finish group routine %s.", grp_info[tp_args.glist[i]].rname);
            }
            // Write to csv files.
            err = tpb_writecsv(grp_ns.fpath, grp_ns.data, tp_args.ntest, tp_args.ngrp, grp_ns.header);
            if(err) {
                err = tpb_printf(err, 1, 1, "In group nanosec writing.");
                __error_exit(err);
            }
            err = tpb_writecsv(grp_cy.fpath, grp_cy.data, tp_args.ntest, tp_args.ngrp, grp_cy.header);
            if(err) {
                err = tpb_printf(err, 1, 1, "In group nanosec writing.");
                __error_exit(err);
            }
        }
    }

    mpi_exit();
    tpb_printf(0, 1, 1, HLINE "TPBench finished.\n" DHLINE);
    return 0;
}
