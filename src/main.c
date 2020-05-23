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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <string.h>
#include "tpbench.h"

#ifdef USE_MPI
#include "mpi.h"
#endif

// =============================================================================
// utilities
// =============================================================================

// process synchronization
void
mpi_sync(){
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    return;
}

// mpi exit
void 
mpi_exit(){
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif   
    return;
}



/**
 * @brief main entry of tpbench.x
 */
int
main(int argc, char **argv) {
    // process info
    int err;
    // iter variables
    __tp_args_t tp_args;
    //char fname[1024], dirname[1024]; // Cycle filename.
    //TYPE s, eps[5] = {0,0,0,0,0}; // s

    // nkerns: # of standalone kernels.
    // ngrps: # of group only for consecutive run
    // benchmark groups and kernels
    int *grps, *kerns;
    char header[256], filename[1024], mydir[1024], full_path[1024], hostname[128]; // headers for csv file
    char prefix[32], posfix[32], gname[32];
    // double *a, *b, *c, *d; 
#ifdef USE_MPI
    double **partps; // cpu cys, parallel cys
#endif
    // arr[NRUN][NKERN] - data of kernel at run.
    uint64_t **grp_ns, **grp_cy, **kern_ns, **kern_cy;
    FILE *fp;

// =============================================================================
// Starting....
// =============================================================================
    // Init process info
    nrank = 1;
    myrank = 0;
    mycpu = sched_getcpu();
    // init mpi rank info
#ifdef USE_MPI
    err = MPI_Init(NULL, NULL);
    if(err != MPI_SUCCESS) {
       printf("EXIT: MPI Initialization failed. [errno: %d]\n", err);
       exit(1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
#endif //#ifdef USE_MPI

    if(myrank == 0){
        printf(DHLINE "TPBench v" VER "\n");
    }
    gethostname(hostname, 128);

    // finished getting process info
    // init kernel, init tpbench arguments

    err = tpb_init();
    if(myrank == 0) {
        printf("nkrout = %d, ngrout = %d\n", nkrout, ngrout);
    }
    err = parse_args(argc, argv, &tp_args);
    if(err) {
        printf("ERROR: %d", err);
        exit(0);
    }

    // print args for debugging
    if(myrank == 0) {
        printf("ntest = %d, nkib = %d\n # of kern = %d, # of ngrp = %d\n",
               tp_args.ntest, tp_args.nkib, tp_args.nkern, tp_args.ngrp);
        for(int i = 0; i < tp_args.nkern; i ++) {
            printf("%d, ", tp_args.klist[i]);
        }
        printf("\n");
        for(int i = 0; i < tp_args.ngrp; i ++) {
            printf("%d, ", tp_args.glist[i]);
        }
    }
    // end args debugging
//     // List only.
//     if(tp_args.list_only_flag){
//         if(myid == 0){
//             // TODO: list_kern() is still hard coded.
//             tpb_list();
//         }
//         mpi_exit();
//         exit(0);
//     }
//     // Check mandatory options
//     if(tp_args.kernels == NULL  && tp_args.groups == NULL) {
//         if(myid == 0) {
//             printf(HLINE "EXIT: Invalid kernel/group list. See tpbench.x --help for help.\n" DHLINE);
//         }
//         mpi_exit();
//         exit(0);
//     }
//     if(tp_args.ntest == 0) {
//         if(myid == 0) {
//             printf(HLINE "EXIT: Invalid number of tests. See tpbench.x --help for help.\n" DHLINE);
//         }
//         mpi_exit();
//         exit(0);
//     }
//     if(tp_args.kib_size < 1) {
//         if(myid == 0) {
//             printf(HLINE "EXIT: Invalid array size. See tpbench.x --help for help.\n" DHLINE);
//         }
//         mpi_exit();
//         exit(0);
//     }
//     // Print arguments
//     if(myid == 0) {
//         printf(HLINE "Array size is set to %llu KiB.\nEach kernel will run for %llu times.\n",
//                tp_args.kib_size, tp_args.ntest);
//     }
//     if(tp_args.data_dir == NULL) {
//         tp_args.data_dir = (char *)malloc(sizeof(char) * 256);
//         err = sprintf(tp_args.data_dir, "./data");
//         if(err < 0) {
//             if(myid == 0){
//                 printf("EXIT: Failed at setting output data directory.\n" DHLINE);
//             }
//             mpi_exit();
//             exit(0);
//         }
        
//     }
//     if(myid == 0) {
//         printf("All csv files will be saved in %s.\n", tp_args.data_dir);
//     }
//     sprintf(mydir, "%s/%s", tp_args.data_dir, hostname);
//     if(myid == 0) {
//         err = make_dir(mydir);
//         if(err) {
//             printf("EXIT: Creating data directory failed.\n");
//             mpi_exit();
//             exit(0);
//         }
//     }

// // =============================================================================
// // STAGE 1 - Initialization
// // =============================================================================
//     // Prepare kernels according to input arguments.
//     // Using kern_list to run kernels one by one.
//     grps = NULL;
//     kerns = NULL;
//     n_tot_kern = sizeof(kern_info) / sizeof(Kern_Info_t);
//     n_tot_grp = grp_end;
//     grps = (int *)malloc(n_tot_grp * sizeof(int));
//     kerns = (int *)malloc(n_tot_kern * sizeof(int));
//     // allocate space for results
//     // Get the array of gid and uid
//     err = init_kern(tp_args.kernels, tp_args.groups, 
//                     kerns, grps, &nkern, &ngrp);
//     if(err){
//         printf("%s\n%s\n", tp_args.groups, tp_args.kernels);
//         printf("EXIT: Fail at init_kern.[%d]\nTPBench Exit\n" DHLINE, err);
//         return 0;
//     }

//     // Print kernel list.
//     if(myid == 0) {
//         printf(HLINE "Running %d groups and %d kernels.\n"
//                "Group list: ", ngrp, nkern);
//         for(i = 0; i < ngrp; i ++)
//             printf("%s, ", g_names[grps[i]]);
//         printf("\nKernel list: ");
//         for(i = 0; i < nkern; i ++) {
//             printf("%s, ", k_names[kerns[i]]);
//         }
//         fflush(stdout);
//     }

// // =============================================================================
// // STAGE 2 - Benchmarking
// // =============================================================================
//     // group benchmark
//     // write results of group benchmark for each group
//     // kern1,kern2,kern3,...,kernN
//     grp_ns = (uint64_t **)malloc(sizeof(uint64_t *) * tp_args.ntest);
//     grp_cy = (uint64_t **)malloc(sizeof(uint64_t *) * tp_args.ntest);
//     for(i = 0; i < ngrp; i ++){
//         gid = grps[i];
//         for(j = 0; j < n_tot_grp; j ++) {
//             if(grp_info[j].gid == gid) {
//                 ngrp_kern = grp_info[j].nkern;
//                 sprintf(gname, "%s", grp_info[j].name);
//                 if(myid == 0) {
//                     printf("Running group %s\n", grp_info[j].name);
//                 }
//                 break;
//             }
//         }
//         // info process
//         header_len = 0;
//         for(j = 0; j < n_tot_kern; j ++) {
//             if(kern_info[j].gid == gid){
//                 sprintf(&header[header_len], "%s,", kern_info[j].name);
//                 header_len = header_len + strlen(kern_info[j].name) + 1;
//             }
//         }
//         header[header_len] = '\0';
//         printf("%s\n", header);
//         mpi_sync();
        
//         for(j = 0; j < tp_args.ntest; j ++) {
//             grp_ns[j] = (uint64_t *)malloc(sizeof(uint64_t) * ngrp_kern);
//             grp_cy[j] = (uint64_t *)malloc(sizeof(uint64_t) * ngrp_kern);
//         }
//         // run benchmarks
//         run_group(gid, tp_args.ntest, tp_args.nloop, tp_args.kib_size, grp_ns, grp_cy);
//         // write results
//         sprintf(full_path, "%s/%s_r%d_c%d_%s.csv", mydir, g_names[gid], myid, mycpu, "ns");
//         write_csv(full_path, grp_ns, grp_info[i].nkern, ngrp_kern, header);
//         sprintf(full_path, "%s/%s_r%d_c%d_%s.csv", mydir, g_names[gid], myid, mycpu, "cy");
//         write_csv(full_path, grp_cy, grp_info[i].nkern, ngrp_kern, header);
//         if(i != ngrp - 1) {
//             for(j = 0; j < tp_args.ntest; j ++) {
//                 grp_ns[j] = (uint64_t *)realloc(grp_ns[j], sizeof(uint64_t) * ngrp_kern);
//                 grp_cy[j] = (uint64_t *)realloc(grp_cy[j], sizeof(uint64_t) * ngrp_kern);
//             }
//         }
//         else {
//             for(j = 0; j < tp_args.ntest; j ++) {
//                 free(grp_ns[j]);
//                 free(grp_cy[j]);
//             }
//             free(grp_ns);
//             free(grp_cy);
//         }
//     }

//     // kernel benchmark
//     // write all results to single csv file after running all kernels
//     kern_ns = (uint64_t **)malloc(sizeof(uint64_t *) * tp_args.ntest);
//     kern_cy = (uint64_t **)malloc(sizeof(uint64_t *) * tp_args.ntest);
//     for(j = 0; j < tp_args.ntest; j ++) {
//         kern_ns[j] = (uint64_t *)malloc(sizeof(uint64_t) * nkern);
//         kern_cy[j] = (uint64_t *)malloc(sizeof(uint64_t) * nkern);
//     }
//     for(i = 0; i < nkern; i ++){
//         uid = kerns[i];
//         if(myid == 0){
//             printf("Running kernel: %s\n", k_names[uid]);
//         }
//         // run benchmarks
//         mpi_sync();
//         run_kernel(uid, i, tp_args.ntest, tp_args.nloop, tp_args.kib_size, kern_ns, kern_cy);
//         // put single result into output array.
    
//     }

//     sprintf(full_path, "%s/kerns_r%d_c%d_%s.csv", mydir, myid, mycpu, "ns");
//     write_csv(full_path, kern_ns, tp_args.ntest, ngrp_kern, header);
//     sprintf(full_path, "%s/kerns_r%d_c%d_%s.csv", mydir, myid, mycpu, "cy");
//     write_csv(full_path, kern_cy, tp_args.ntest, ngrp_kern, header);
//     for(i = 0; i < tp_args.ntest; i ++) {
//         free(kern_ns[i]);
//         free(kern_cy[i]);
//     }
//     free(kern_ns);
//     free(kern_cy);
// // =============================================================================
// // STAGE 3 - Data process and output.
// // =============================================================================

//     mpi_exit();
    printf(HLINE "TPBench finished.\n" DHLINE);
    return 0;
}
