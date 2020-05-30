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

int init_res(char *prefix, char *posfix, char *host_dir, __tp_args_t *args, __res_t *res);

// =============================================================================
// utilities
// =============================================================================

// init result data structure
int
init_res(char *prefix, char *posfix, char *hostname, __tp_args_t *args, __res_t *res) {
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
    err = parse_args(argc, argv, &tp_args);
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
    // print group list
    if(tp_args.ngrp) {
        tpprintf(0, 1, 1, "Group routine: ");
        for(int i = 0; i < tp_args.ngrp; i ++) {
            tpprintf(0, 0, 0, "%s, ", kern_info[tp_args.glist[i]].rname);
        }
    }
    tpprintf(0, 1, 1, "Each routine will be tested %d times.", tp_args.ntest);
    
    
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
    __error_fun(err, "Cannot create data directory.");
    tpprintf(err, 1, 1, "Host dir %s", mydir);

    // kernel benchmark
    if(tp_args.nkern) {
        // Struct initialization
        err = init_res("kernels", "ns", hostname, &tp_args, &kern_ns);
        __error_fun(err, "Kernel data space init failed.");
        err = init_res("kernels", "cy", hostname, &tp_args, &kern_cy);
        __error_fun(err, "Kernel data space init failed.");

        // allocate space for data
        kern_ns.data = (uint64_t **)malloc(tp_args.nkern * sizeof(uint64_t *));
        kern_cy.data = (uint64_t **)malloc(tp_args.nkern * sizeof(uint64_t *));
        for(int i = 0; i < tp_args.nkern; i ++) {
            kern_ns.data[i] = (uint64_t *)malloc(tp_args.ntest * sizeof(uint64_t));
            kern_cy.data[i] = (uint64_t *)malloc(tp_args.ntest * sizeof(uint64_t));
        }
        
        // Run kernels.
        for(int i = 0; i < tp_args.nkern; i ++) {
            tpprintf(err, 1, 1, "Kernel routine %s started.\n" HLINE, kern_info[tp_args.klist[i]].rname);
            err = tpb_run_kernel(tp_args.klist[i], 
                                 tp_args.ntest, 
                                 kern_ns.data[i], 
                                 kern_cy.data[i], 
                                 tp_args.nkib);
            __error_fun(err, "Benchmark failed.");
            tpprintf(err, 1, 1, "Finished.");
        }

        // Write raw data to csv files.
        err = tpb_writecsv(kern_ns.fpath, kern_ns.data, tp_args.ntest, tp_args.nkern, kern_ns.header, 1);
        __error_fun(err, "Writing ns csv failed.");
        err = tpb_writecsv(kern_cy.fpath, kern_cy.data, tp_args.ntest, tp_args.nkern, kern_cy.header, 1);
        __error_fun(err, "Writing cycle csv failed.");

        // Clean up
        for(int i = 0; i < tp_args.nkern; i ++) {
            free(kern_ns.data[i]);
            free(kern_cy.data[i]);
        }
        free(kern_ns.data);
        free(kern_cy.data);
    }
    
    // group benchmark
    for(int i = 0; i < tp_args.ngrp; i ++) {
        int nepoch = grp_info[tp_args.glist[i]].nepoch;
        tpprintf(0, 1, 1, "Group routine %s initializing.", grp_info[tp_args.glist[i]].rname);
        // Struct initialization
        err = init_res(grp_info[tp_args.glist[i]].rname, "ns", hostname, &tp_args, &grp_ns);
        __error_fun(err, "Data space init failed.");
        err = init_res(grp_info[tp_args.glist[i]].rname, "cy", hostname, &tp_args, &grp_cy);   
        __error_fun(err, "Data space init failed.");
        // prepare header for output. [group routine name],[epoch1],[epoch2],...
        sprintf(grp_ns.header, "%s,%s", grp_info[i].rname, grp_info[i].header);
        sprintf(grp_cy.header, "%s,%s", grp_info[i].rname, grp_info[i].header);

        // allocate space for data
        // data dimension in group test is reverse against to the kernel test since group test will
        // execute its epochs consecutively
        grp_ns.data = (uint64_t **)malloc(tp_args.ntest * sizeof(uint64_t *));
        grp_cy.data = (uint64_t **)malloc(tp_args.ntest * sizeof(uint64_t *));
        if(grp_ns.data == NULL || grp_cy.data == NULL) {
            err = MALLOC_FAIL;
        }
        __error_fun(err, "Data space init failed.");
        for(int i = 0; i < tp_args.ntest; i ++) {
            // one more slot for overall time
            grp_ns.data[i] = (uint64_t *)malloc((nepoch + 1) * sizeof(uint64_t));
            grp_cy.data[i] = (uint64_t *)malloc((nepoch + 1) * sizeof(uint64_t));
        }
        
        // Run group
        tpprintf(0, 1, 1, "Start running.\n", grp_info[tp_args.glist[i]].rname);
        err = tpb_run_group(tp_args.glist[i],
                            tp_args.ntest,
                            grp_ns.data,
                            grp_cy.data,
                            tp_args.nkib);
        __error_fun(err, "Banchmark failed.");
        tpprintf(err, 1, 1, "Finish running.");
        
        // Write to csv files.
        err = tpb_writecsv(grp_ns.fpath, grp_ns.data, tp_args.ntest, grp_info[i].nepoch+1, grp_ns.header, 0);
        __error_fun(err, "Writing ns csv failed.");
        err = tpb_writecsv(grp_cy.fpath, grp_cy.data, tp_args.ntest, grp_info[i].nepoch+1, grp_cy.header, 0);
        __error_fun(err, "Writing ns cy failed.");

        for(int i = 0; i < tp_args.ntest; i ++) {
            free(grp_ns.data[i]);
            free(grp_cy.data[i]);
        }
        free(grp_ns.data);
        free(grp_cy.data);
    }

    // end of benchmark
    tpprintf(0, 1, 1, "\nTPBench finished.\n" DHLINE);
    tpmpi_exit();

    return 0;
}
