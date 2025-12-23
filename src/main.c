/**
  * @file main.c
  * @brief   main entry for tpbench
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
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-stat.h"
#include "tpmpi.h"

#ifdef USE_MPI
#include "mpi.h"
#endif

struct tpmpi_info_t tpmpi_info;

int init_res(char *prefix, char *posfix, char *host_dir, tpb_args_t *args, tpb_res_t *res);

// init result data structure
int
init_res(char *prefix, char *posfix, char *hostname, tpb_args_t *args, tpb_res_t *res) {
    res->header[0] = '\0';

    if(strcmp(prefix, "kernels") == 0) {
        // matched, header for kernel benchmark
        for(int i = 0; i < args->nkern - 1; i ++){
            sprintf(res->header, "%s,", strcat(res->header, kernel_all[args->klist[i]].info.kname));
        }
        int i = args->nkern - 1;
        sprintf(res->header, "%s", strcat(res->header, kernel_all[args->klist[i]].info.kname));
    }
    // print fname
    sprintf(res->fname, "%s-r%d_c%d-%s.csv", 
                  prefix, tpmpi_info.myrank, tpmpi_info.pcpu, posfix);
    // print fdir
    sprintf(res->fdir, "%s/%s", args->data_dir, hostname);
    // print fpath
    sprintf(res->fpath, "%s/%s", res->fdir, res->fname);
    
    return 0;
}


/**
 * @brief main entry of tpbench.x
 */
int
main(int argc, char **argv) {
    // process info
    int err;
    tpb_args_t tpb_args;
    tpb_kargs_common_t tpb_kargs_common;
    tpb_timer_t timer;
    char filename[1024], mydir[PATH_MAX], hostname[128]; 
    tpb_res_t time_arr, kib;

    // Init process info
    err = tpmpi_init();
    __tpbm_exit_on_error(err, "At main.c: tpmpi_init");
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE);
#ifdef USE_MPI
    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench-MPI v" VER "\n");
#else
    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench v" VER "\n");
#endif
    // init kernel, init tpbench arguments
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At main.c: tpb_register_kernel");
    
    // tpb_printf(0, 1, 1, "nkrout = %d, ngrout = %d", nkrout, ngrout);
    err = tpb_parse_args(argc, argv, &tpb_args, &tpb_kargs_common, &timer);
    if (err == TPBE_EXIT_ON_HELP) {
        goto MAIN_EXIT;
    } else {
        __tpbm_exit_on_error(err, "At main.c: tpb_parse_args");
    }

    // List only.
    if(tpb_args.list_only_flag){
        tpb_list();
        tpmpi_exit();
        exit(0);
    }

    // print kernel list
    if(tpb_args.nkern) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel list:\n");
        for(int i = 0; i < tpb_args.nkern; i ++) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "%s, \n", kernel_all[tpb_args.klist[i]].info.kname);
        }
    }
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Each routine will be tested %d times.\n", tpb_kargs_common.ntest);


    // create host dir
    gethostname(hostname, 128);
    sprintf(mydir, "%s/%s", tpb_args.data_dir, hostname);
    err = tpb_mkdir(mydir);
    __tpbm_exit_on_error(err, "At main.c: tpb_mkdir");
    {
        unsigned err_type = tpb_get_err_exit_flag(err);
        tpb_printf(TPBM_PRTN_M_TSTAG | err_type, "Host dir %s\n", mydir);
    }

    // kernel benchmark
    if(tpb_args.nkern) {
        // Struct initialization
        err = init_res("kernels", "ns", hostname, &tpb_args, &time_arr);
        __tpbm_exit_on_error(err, "At main.c: init_res");

        // Find maximum ntest among all kernels (for array allocation)
        int max_ntest = tpb_kargs_common.ntest;
        if(tpb_args.kargs_kernel.nkern > 0) {
            int token_start = 0;
            for(int i = 0; i < tpb_args.kargs_kernel.nkern; i++) {
                for(int j = 0; j < tpb_args.kargs_kernel.ntoken[i]; j++) {
                    char *tok = tpb_args.kargs_kernel.token[token_start + j];
                    if(strncmp(tok, "ntest=", 6) == 0) {
                        int ntest_val = atoi(tok + 6);
                        if(ntest_val > max_ntest) max_ntest = ntest_val;
                    }
                }
                token_start += tpb_args.kargs_kernel.ntoken[i];
            }
        }
        
        // allocate space for data using max_ntest
        time_arr.data = (int64_t **)malloc(tpb_args.nkern * sizeof(int64_t *));
        for(int i = 0; i < tpb_args.nkern; i ++) {
            time_arr.data[i] = (int64_t *)malloc(max_ntest * sizeof(int64_t));
        }
        
        // Run kernels.
        for(int i = 0; i < tpb_args.nkern; i ++) {
            int kid = tpb_args.klist[i];
            
            // Extract ntest and memsize from pre-configured runtime parameters
            int ntest = tpb_kargs_common.ntest;
            uint64_t memsize = tpb_kargs_common.memsize;
            
            for(int j = 0; j < tpb_args.kernel_nparms[i]; j++) {
                if(strcmp(tpb_args.kernel_rt_parms[i][j].name, "ntest") == 0) {
                    ntest = (int)tpb_args.kernel_rt_parms[i][j].value.i64;
                } else if(strcmp(tpb_args.kernel_rt_parms[i][j].name, "memsize") == 0) {
                    memsize = tpb_args.kernel_rt_parms[i][j].value.u64;
                }
            }
            
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started.\n" HLINE, kernel_all[kid].info.kname);
            err = tpb_run_kernel(kid, 
                                 &timer,
                                 ntest, 
                                 time_arr.data[i],
                                 memsize,
                                 tpb_args.kernel_rt_parms[i],
                                 tpb_args.kernel_nparms[i]);
            __tpbm_exit_on_error(err, "At main.c: tpb_run_kernel");
            
            // Process and display statistics
            int nskip = 1, freq = 1;
            size_t nsize = memsize * 1024 / sizeof(double);
            dpipe_k0(time_arr.data[i], nskip, ntest, freq, 
                     kernel_all[kid].info.nbyte, nsize);
            
            {
                unsigned err_type = tpb_get_err_exit_flag(err);
                tpb_printf(TPBM_PRTN_M_TSTAG | err_type, "Finished.\n");
            }
        }

        // Write raw data to csv files.
        err = tpb_writecsv(time_arr.fpath, time_arr.data, tpb_kargs_common.ntest, tpb_args.nkern, time_arr.header);
        __tpbm_exit_on_error(err, "At main.c: tpb_writecsv");
        // Clean up
        for(int i = 0; i < tpb_args.nkern; i ++) {
            free(time_arr.data[i]);
        }
        free(time_arr.data);
    }
    
    // end of benchmark
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "\nTPBench finished.\n" DHLINE);

MAIN_EXIT:
    tpmpi_exit();

    return 0;
}
