/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
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
 * 
 * =================================================================================
 * test.c
 * Description: Main entry of TPBench. 
 * Author: Key Liao
 * Modified: May. 9th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#define _GNU_SOURCE
#define VER "0.3"

#include <stdio.h>
#include <stdlib.h>
#include "tpbench.h"

// int
// test_run_group(int gid) {
//     int i, j, err;
//     uint64_t **res_ns, **res_cy;

//     res_ns = (uint64_t **)malloc(sizeof(uint64_t) * 10);
//     res_cy = (uint64_t **)malloc(sizeof(uint64_t) * 10);
//     for(i = 0; i < 10; i ++) {
//         res_ns[i] = (uint64_t *)malloc(sizeof(uint64_t) * 8);
//         res_cy[i] = (uint64_t *)malloc(sizeof(uint64_t) * 8);
//     }
//     err = run_group(0, 10, 1, 2048, res_ns, res_cy);
//     for(i = 0; i < 10; i ++) {
//         for(j = 0; j < 8; j ++) {
//             printf("%llu, ", res_ns[i][j]) ;
//         }
//         printf("\n");
//         for(j = 0; j < 8; j ++) {
//             printf("%llu, ", res_cy[i][j]) ;
//         }
//         printf("\n");
//     }
//     for(i = 0; i < 10; i ++) {
//         free(res_ns[i]);
//         free(res_cy[i]);
//     }
//     free(res_ns);
//     free(res_cy);
//     return err;
// }

void
test_parse_args(int argc, char **argv) {
    //int argc;
    //char **argv;
    __tp_args_t tp_args;
    //argc = 11;
    //argv = (char **)malloc(sizeof(char *) * 11);

    //argv[0] = "./tpbench.x";
    //argv[1] = "-n";
    //argv[2] = "100";
    //argv[3] = "-s";
    //argv[4] = "4096";
    //argv[5] = "-g";
    //argv[6] = "stream";
    //argv[7] = "-k";
    //argv[8] = "d_copy,d_scale";
    //argv[9] = "-d";
    //argv[10] = "./data";
    if(parse_args(argc, argv, &tp_args)){
        printf("FAILED.\n");
    } 
    else {
        printf("%llu, %llu\n%s\n", tp_args.ntest, tp_args.nkib, tp_args.data_dir);
        for(int i = 0; i < tp_args.nkern; i ++) {
            printf("%d: %s\n", tp_args.klist[i], kern_info[i].rname);
        }
        for(int i = 0; i < tp_args.ngrp; i ++) {
            printf("%d: %s\n", tp_args.glist[i], grp_info[i].rname);
        }
    }
    
    //free(argv);
}

int
test_run_kernel(int kid) {
    int i, j, k;
    uint64_t min_cy; // least cycle for a kernel
    uint64_t ns[20], cy[20];
    double freq;
    int nkernel;


    nkernel = sizeof(kern_info) / sizeof(Kern_Info_t);
    for(i = 0; i < nkernel; i ++) {
        printf("TEST: %s ------ ", kern_info[i].rname);
        min_cy = 4096 * 1024 / 8;
        kern_info[i].pfun(20, ns, cy, 4096);
        for(j = 1; j < 20; j ++) {
            freq = (double)cy[j] / (double)ns[j];
            if(freq < 0.5 || freq > 5) {
                printf("FAILED. Frequency = %f\n", freq);
                break;
            }
            if(cy[j] <= min_cy) {
                // kernel seems to be wiped out in optimization
                printf("FAILED. Cycle = %llu < minimum cycle %llu\n", min_cy);
                break;
            }
        }
        if(j == 20) {
            printf("PASS. Frequency = %f\n", freq);
        } 
    }
    return 0;
}

int
main(int argc, char **argv) {
    int err;
    // printf("TEST run_group\n");
    
    // if(err = test_run_group()) {
    //     printf("FAIL run_group [%d]\n", err);
    // }
    printf("TEST parse_args\n");
    test_parse_args(argc, argv);
    printf("TEST run_kernel\n");
    test_run_kernel(0);

    return 0;
}