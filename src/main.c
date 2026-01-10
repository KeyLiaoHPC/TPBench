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
#include "tpmpi.h"

#ifdef USE_MPI
#include "mpi.h"
#endif

struct tpmpi_info_t tpmpi_info;

int init_res(char *prefix, char *posfix, char *host_dir, const char *data_dir,
             tpb_k_rthdl_t *handles, int nkern, tpb_res_t *res);
static int tpb_find_i64(const tpb_rt_parm_t *parms, int nparms, const char *name, int64_t *out);
static int tpb_find_u64(const tpb_rt_parm_t *parms, int nparms, const char *name, uint64_t *out);

// init result data structure
int
init_res(char *prefix, char *posfix, char *hostname, const char *data_dir,
         tpb_k_rthdl_t *handles, int nkern, tpb_res_t *res) {
    res->header[0] = '\0';

    if(strcmp(prefix, "kernels") == 0) {
        // matched, header for kernel benchmark
        for(int i = 0; i < nkern - 1; i ++){
            sprintf(res->header, "%s,", strcat(res->header, handles[i].kernel.info.name));
        }
        int i = nkern - 1;
        sprintf(res->header, "%s", strcat(res->header, handles[i].kernel.info.name));
    }
    // print fname
    sprintf(res->fname, "%s-r%d_c%d-%s.csv", 
                  prefix, tpmpi_info.myrank, tpmpi_info.pcpu, posfix);
    // print fdir
    sprintf(res->fdir, "%s/%s", data_dir, hostname);
    // print fpath
    sprintf(res->fpath, "%s/%s", res->fdir, res->fname);
    
    return 0;
}

static int
tpb_find_i64(const tpb_rt_parm_t *parms, int nparms, const char *name, int64_t *out)
{
    if(parms == NULL || name == NULL || out == NULL) {
        return 0;
    }

    for(int i = 0; i < nparms; i++) {
        if(strcmp(parms[i].name, name) == 0) {
            *out = parms[i].value.i64;
            return 1;
        }
    }

    return 0;
}

static int
tpb_find_u64(const tpb_rt_parm_t *parms, int nparms, const char *name, uint64_t *out)
{
    if(parms == NULL || name == NULL || out == NULL) {
        return 0;
    }

    for(int i = 0; i < nparms; i++) {
        if(strcmp(parms[i].name, name) == 0) {
            *out = parms[i].value.u64;
            return 1;
        }
    }

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
    tpb_k_rthdl_t *kernel_handles = NULL;
    char filename[1024], mydir[PATH_MAX], hostname[128]; 

    // Init process info
    err = tpmpi_init();
    __tpbm_exit_on_error(err, "At main.c: tpmpi_init");
#ifdef USE_MPI
    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench-MPI v" VER "\n");
#else
    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench v" VER "\n");
#endif
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE"\n");
    // init kernel and parse arguments
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At main.c: tpb_register_kernel");
    err = tpb_parse_args(argc, argv, &tpb_args, &kernel_handles);
    if (err == TPBE_EXIT_ON_HELP) {
        goto MAIN_EXIT;
    } else {
        __tpbm_exit_on_error(err, "At main.c: tpb_parse_args");
    }
    if (tpb_args.nkern) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernels to run: "); 
        for (int i = 0; i < tpb_args.nkern - 1; i ++) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "%s, ", kernel_handles[i].kernel.info.name);
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s\n", kernel_handles[tpb_args.nkern-1].kernel.info.name);
    }
    tpb_driver_set_timer(tpb_args.timer);

    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE"\n");
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Start driver runner.\n");
    for (int i = 0; i < tpb_args.nkern; i++) {
        tpb_k_rthdl_t *handle = &kernel_handles[i];

        /* Run kernel (includes reporting) */
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started.\n", handle->kernel.info.name);
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE"\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "# Test %d\n", i);
        err = tpb_run_kernel(handle);
        __tpbm_exit_on_error(err, "At main.c: tpb_run_kernel");

        /* Clean up */
        tpb_clean_output(handle);
    }
    
    // end of benchmark
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "TPBench exit.\n");

MAIN_EXIT:
    tpmpi_exit();

    return 0;
}
