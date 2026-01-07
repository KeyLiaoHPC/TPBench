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
 * tpio.c
 * Description: some accessory functions. 
 * Author: Key Liao
 * Modified: May. 9th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>
#include "tpb-io.h"
#include "tpb-stat.h"
#include "tpb-driver.h"

/* === CLI Output Format Controller (file-scoped) === */

/**
 * @brief CLI output format configuration structure.
 *        Controls terminal output formatting for kernel results.
 */
typedef struct tpb_cliout_format {
    int max_col;          /* Maximum terminal columns before wrapping */
    double *qtiles;       /* Array of quantile positions */
    size_t nq;            /* Number of quantiles */
    int initialized;      /* Initialization flag */
} tpb_cliout_format_t;

/* Default quantile positions */
static double tpb_default_qtiles[] = {0.05, 0.25, 0.50, 0.75, 0.95};

/* Static format controller instance */
static tpb_cliout_format_t tpb_cliout_fmt = {
    .max_col = 85,
    .qtiles = tpb_default_qtiles,
    .nq = 5,
    .initialized = 0
};

/**
 * @brief Initialize the CLI output format controller.
 *        Gets terminal width via ioctl if available.
 */
static void
tpb_cliout_init(void)
{
    if (tpb_cliout_fmt.initialized) {
        return;
    }

    /* Try to get actual terminal width */
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        tpb_cliout_fmt.max_col = ws.ws_col;
    }
    /* Keep default (85) if ioctl fails */

    tpb_cliout_fmt.initialized = 1;
}

/**
 * @brief Convert TPB_UNIT_T to human-readable string.
 * @param unit The unit code.
 * @return Pointer to static string representation.
 */
static const char *
tpb_unit_to_string(TPB_UNIT_T unit)
{
    /* Extract unit kind directly from unit */
    TPB_UNIT_T ukind = unit & TPB_UKIND_MASK;

    /* Time units */
    if (ukind == TPB_UKIND_TIME) {
        if (unit == TPB_UNIT_NS) return "ns";
        if (unit == TPB_UNIT_US) return "us";
        if (unit == TPB_UNIT_MS) return "ms";
        if (unit == TPB_UNIT_SS) return "s";
        if (unit == TPB_UNIT_PS) return "ps";
        if (unit == TPB_UNIT_FS) return "fs";
        if (unit == TPB_UNIT_CY) return "cycles";
        if (unit == TPB_UNIT_TIMER) return "timer_unit";
        return "time";
    }

    /* Data size units */
    if (ukind == TPB_UKIND_VOL) {
        if (unit == TPB_UNIT_BYTE) return "B";
        if (unit == TPB_UNIT_KIB) return "KiB";
        if (unit == TPB_UNIT_MIB) return "MiB";
        if (unit == TPB_UNIT_GIB) return "GiB";
        if (unit == TPB_UNIT_TIB) return "TiB";
        if (unit == TPB_UNIT_KB) return "KB";
        if (unit == TPB_UNIT_MB) return "MB";
        if (unit == TPB_UNIT_GB) return "GB";
        if (unit == TPB_UNIT_TB) return "TB";
        if (unit == TPB_UNIT_BIT) return "bit";
        return "data_size";
    }

    /* FLOPS units */
    if (ukind == TPB_UKIND_OPS) {
        if (unit == TPB_UNIT_FLOPS) return "FLOP/s";
        if (unit == TPB_UNIT_KFLOPS) return "KFLOP/s";
        if (unit == TPB_UNIT_MFLOPS) return "MFLOP/s";
        if (unit == TPB_UNIT_GFLOPS) return "GFLOP/s";
        if (unit == TPB_UNIT_TFLOPS) return "TFLOP/s";
        if (unit == TPB_UNIT_PFLOPS) return "PFLOP/s";
        return "ops";
    }

    return "unknown";
}

/**
 * @brief Print a dynamic-width double horizontal line.
 * @param width The target width in characters.
 */
static void
tpb_print_dhline(int width)
{
    for (int i = 0; i < width; i++) {
        putchar('=');
    }
    putchar('\n');
}

/**
 * @brief Format and print a parameter value based on its dtype.
 * @param parm Pointer to the runtime parameter.
 * @param buf Output buffer.
 * @param bufsize Buffer size.
 * @return Number of characters written.
 */
static int
tpb_format_parm_value(const tpb_rt_parm_t *parm, char *buf, size_t bufsize)
{
    TPB_DTYPE type_only = parm->dtype & TPB_PARM_TYPE_MASK;

    switch (type_only) {
    case TPB_INT_T:
    case TPB_INT32_T:
        return snprintf(buf, bufsize, "%s=%d", parm->name, (int)parm->value.i64);
    case TPB_INT8_T:
    case TPB_INT16_T:
    case TPB_INT64_T:
        return snprintf(buf, bufsize, "%s=%ld", parm->name, (long)parm->value.i64);
    case TPB_UINT8_T:
    case TPB_UINT16_T:
    case TPB_UINT32_T:
    case TPB_UINT64_T:
        return snprintf(buf, bufsize, "%s=%lu", parm->name, (unsigned long)parm->value.u64);
    case TPB_FLOAT_T:
        return snprintf(buf, bufsize, "%s=%.6g", parm->name, (double)parm->value.f32);
    case TPB_DOUBLE_T:
        return snprintf(buf, bufsize, "%s=%.6g", parm->name, parm->value.f64);
    default:
        return snprintf(buf, bufsize, "%s=?", parm->name);
    }
}

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
tpb_writecsv(char *path, int64_t **data, int nrow, int ncol, char *header) {
#ifdef TPM_NO_RAW_DATA
    return 0;
#else
    int err, i, j;
    FILE *fp;    

    fp = fopen(path, "w");
    if(fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    if (header != NULL && strlen(header) > 0) {
        fprintf(fp, "%s\n", header);
    }

    // data[col][row], for kernel benchmark
    for(i = 0; i < nrow; i ++) {
        for(j = 0; j < ncol - 1; j ++) {
            fprintf(fp, "%"PRId64",", data[j][i]);
        }
        fprintf(fp, "%"PRId64"\n", data[ncol-1][i]);
    }
    fflush(fp);
    fclose(fp);

    return 0;
#endif
}

// tpbench printf wrapper. 
void
tpb_printf(uint64_t mode_bit, char *fmt, ...) {
    uint64_t print_mode = mode_bit & 0x0F;
    uint64_t tag_mode = mode_bit & 0xF0;
    const char *tag = "NOTE";

    if(tag_mode == TPBE_WARN) {
        tag = "WARN";
    } else if(tag_mode == TPBE_FAIL) {
        tag = "FAIL";
    } else if(tag_mode == TPBE_UNKN) {
        tag = "UNKN";
    }

    // print splitter directly.
    if(print_mode == TPBM_PRTN_M_DIRECT &&
       (strcmp(fmt, HLINE) == 0 || strcmp(fmt, DHLINE) == 0)) {
        if(tpmpi_info.myrank == 0) {
            printf("%s", fmt);
        }
        return;
    }

    va_list args;
    va_start(args, fmt);

    if(print_mode == TPBM_PRTN_M_DIRECT) {
        vprintf(fmt, args);
        va_end(args);
        return;
    }
    if(print_mode & TPBM_PRTN_M_TS) {
        time_t t = time(0);
        struct tm* lt = localtime(&t);
        printf("%04d-%02d-%02d %02d:%02d:%02d ",
               lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
               lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
    if(print_mode & TPBM_PRTN_M_TAG) {
        printf("[%s] ", tag);
    }
    vprintf(fmt, args);
    va_end(args);
}

void
tpb_print_help_total(void)
{
    printf(TPBM_HELP_DOC_TOTAL);
}

void
tpb_list(){
    int nkern = tpb_get_kernel_count();
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Listing supported kernels.\n");
    tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel          Description\n");
    tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    for(int i = 0 ; i < nkern; i ++) {
        tpb_kernel_t *kernel = NULL;
        if(tpb_get_kernel_by_index(i, &kernel) != 0) {
            continue;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "%-15s %s\n", 
                   kernel->info.name, kernel->info.note);
    }
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE);
}

static void transpose(uint64_t *out, uint64_t **in, int m, int n) {
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < m; i++) {
            out[j * m + i] = in[i][j];
        }
    }
}

int report_performance(uint64_t **ns, uint64_t **cy, uint64_t total_wall_time, int nskip, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm) {
    __ovl_t res;

    uint64_t* nst = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));
    uint64_t* cyt = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));
    transpose(nst, ns, ntest, nepoch);
    transpose(cyt, cy, ntest, nepoch);

    const int BUFLEN = 10240;
    char buf[10240];
    int offset = 0;
    offset += sprintf(&buf[offset], HLINE);
    offset += sprintf(&buf[offset], "rank %3d ", tpmpi_info.myrank);
    offset += sprintf(&buf[offset], OVL_QUANT_HEADER_EXT);

    if (!skip_comp) {
        calc_rate_quant(&nst[0 * ntest + nskip], ntest - nskip, 2.0*N*N*N, 1, &res);
        offset += sprintf(&buf[offset], "GEMM(GFLOPS)   %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp, res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);

        calc_period_quant(&nst[0 * ntest + nskip], ntest - nskip, 1, 1e-6, &res);
        offset += sprintf(&buf[offset], "GEMM(ms)       %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp, res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);
    }

    if (!skip_comm){
        calc_period_quant(&nst[1 * ntest + nskip], ntest - nskip, 1, 1e-3, &res);
        offset += sprintf(&buf[offset], "comm(us)       %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
                res.meantp, res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);
    }
    // gather output to rank 0 to avoid message interleaving among ranks
    char *gather_buf;
    if (tpmpi_info.myrank == 0) {
        gather_buf = (char *) malloc(BUFLEN * tpmpi_info.nrank);
    }
#ifdef USE_MPI
    MPI_Gather(buf, BUFLEN, MPI_CHAR, gather_buf, BUFLEN, MPI_CHAR, 0, MPI_COMM_WORLD);
#endif
    if (tpmpi_info.myrank == 0) {
        for (int i = 0; i < tpmpi_info.nrank; i++) {
                printf(gather_buf + i * BUFLEN);
        }
        double total_time = (double) total_wall_time * 1e-3;
        printf("\nTotal Wall Time(us): %-12.3f", total_time);
        free(gather_buf);
    }
    free(nst);
    free(cyt);
    return 0;
}


/***
 * Log every step's performance data into a csv file
 * the csv file will be named as "np${rank_size}_kernelname_ntest_N{N}.csv"
 * the csv headers are "rank0" ~ "rank${rank_size-1}"
 * each row is the performance data of a step
 * @param ns: the time data of each step
 * @param cy: the cycle data of each step
 * @param kernel_name: the name of the kernel
 * @param ntest: the number of steps
 * @param nepoch: the number of epochs
 * @param N: the matrix size
 * @param Nr: the number of rows to allreduce
 * @param skip_comp: whether to skip the computation
 * @return void
 */
int log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm) {
    int err = 0;
    const int BUFLEN = (ntest + 1) * 20;
    char *headers = malloc(BUFLEN);
    char filename[4][100];
    char filedir[16] = "./result/log/";
    char filepath[4][120];
    uint64_t* nst = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));
    uint64_t* cyt = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));

    char *tpbench_run_mode = getenv("TPBENCH_RUN_MODE");
    char run_mode[24];
    if (tpbench_run_mode != NULL) {
        strcpy(run_mode, tpbench_run_mode);
    } else {
        if(skip_comp) {
            strcpy(run_mode, "commonly");
        } else {   
            strcpy(run_mode, "compcomm");
        } 
    }

    sprintf(filename[0], "np%d-%s-%s-GEMM(ns)-ntest%d-N%d-Nr%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N, Nr);
    sprintf(filename[1], "np%d-%s-%s-comm(ns)-ntest%d-N%d-Nr%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N, Nr);
    sprintf(filename[2], "np%d-%s-%s-GEMM(cy)-ntest%d-N%d-Nr%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N, Nr);

    strcpy(filepath[0], filedir);
    strcpy(filepath[1], filedir);
    strcpy(filepath[2], filedir);


    strcat(filepath[0], filename[0]);
    strcat(filepath[1], filename[1]);
    strcat(filepath[2], filename[2]);

    tpb_mkdir(filedir);

    sprintf(headers , "rank, step0");
    #ifdef USE_MPI
    for (int i = 1; i < ntest; i++) {
        sprintf(headers + strlen(headers), ", step%d", i);
    }
    #endif

    transpose(nst, ns, ntest, nepoch);
    transpose(cyt, cy, ntest, nepoch);

    if (skip_comp == 0){
        err = tpmpi_writecsv(filepath[0], nst, ntest, headers);
        if (err) return err;
        err = tpmpi_writecsv(filepath[2], cyt, ntest, headers);
        if (err) return err;
    }
    if (skip_comm == 0) {
        err = tpmpi_writecsv(filepath[1], &nst[ntest], ntest, headers);
        if (err) return err;
    }
    
    free(nst);
    free(cyt);
    free(headers);

    return 0;
}

int
tpb_report_args_cli(tpb_k_rthdl_t *handle)
{
    if (handle == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Initialize format controller on first call */
    tpb_cliout_init();

    int max_col = tpb_cliout_fmt.max_col;

    /* Print header line */
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");

    /* Kernel Name - do not wrap even if over max_col */
    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel Name: %s\n", handle->kernel.info.name);

    /* Run-time parameter settings - wrap at max_col */
    if (handle->argpack.n > 0 && handle->argpack.args != NULL) {
        const char *prefix = "Run-time parameter settings: ";
        int prefix_len = (int)strlen(prefix);
        int cur_col = 0;

        tpb_printf(TPBM_PRTN_M_DIRECT, "%s", prefix);
        cur_col = prefix_len;

        for (int i = 0; i < handle->argpack.n; i++) {
            char parm_buf[256];
            int parm_len = tpb_format_parm_value(&handle->argpack.args[i], parm_buf, sizeof(parm_buf));

            /* Add separator */
            const char *sep = (i < handle->argpack.n - 1) ? ", " : "";
            int sep_len = (int)strlen(sep);
            int total_len = parm_len + sep_len;

            /* Check if we need to wrap */
            if (cur_col + total_len > max_col && cur_col > prefix_len) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
                /* Print continuation indent */
                char indent[256];
                memset(indent, ' ', prefix_len);
                indent[prefix_len] = '\0';
                tpb_printf(TPBM_PRTN_M_DIRECT, "%s", indent);
                cur_col = prefix_len;
            }

            tpb_printf(TPBM_PRTN_M_DIRECT, "%s%s", parm_buf, sep);
            cur_col += total_len;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
    }
    tpb_printf(TPBM_PRTN_M_DIRECT, HLINE"\n");

    return TPBE_SUCCESS;
}

int
tpb_report_result_cli(tpb_k_rthdl_t *handle)
{
    if (handle == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Initialize format controller on first call */
    tpb_cliout_init();

    double *qtiles = tpb_cliout_fmt.qtiles;
    size_t nq = tpb_cliout_fmt.nq;

    /* Test results section */
    tpb_printf(TPBM_PRTN_M_DIRECT, "Test results:\n");

    /* Allocate quantile output array */
    double *qout = (double *)malloc(nq * sizeof(double));
    if (qout == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    /* Process each output */
    for (int i = 0; i < handle->respack.n; i++) {
        tpb_k_output_t *out = &handle->respack.outputs[i];

        /* Skip if output data is NULL */
        if (out->p == NULL) {
            continue;
        }

        /* Print metrics name */
        tpb_printf(TPBM_PRTN_M_DIRECT, "Metrics: %s\n", out->name);

        /* Print unit */
        tpb_printf(TPBM_PRTN_M_DIRECT, "Units: %s\n", tpb_unit_to_string(out->unit));

        /* Calculate and print mean */
        double mean_val = 0.0;
        int err = tpb_stat_mean(out->p, (size_t)out->n, out->dtype, &mean_val);
        if (err == TPBE_SUCCESS) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Results mean: %.6g\n", mean_val);
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Results mean: N/A\n");
        }

        /* Calculate and print quantiles - do not wrap */
        err = tpb_stat_qtile_1d(out->p, (size_t)out->n, out->dtype, qtiles, nq, qout);
        if (err == TPBE_SUCCESS) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Results Quantile: ");
            for (size_t q = 0; q < nq; q++) {
                if (q > 0) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, ", ");
                }
                tpb_printf(TPBM_PRTN_M_DIRECT, "Q%.2f=%.6g", qtiles[q], qout[q]);
            }
            tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Results Quantile: N/A\n");
        }
    }

    free(qout);

    /* Print footer line */
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");

    return TPBE_SUCCESS;
}
