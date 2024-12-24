/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
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
 * @file tpdata.c
 * @version 0.3
 * @brief factory of benchmarking data
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-27
 */
#include <stdlib.h>
#include "tpdata.h"
#include "tperror.h"
#include "tpio.h"
#include "stdio.h"


int qsort_ascend(const void * a, const void * b) {
    if( *(double *)a < *(double *)b ) {
        return -1;
    }
    return  *(double *)a > *(double *)b;
}


// calculate mean, min, max, quantile in a n-item 1d fp64 array
int
calc_quant(double *data, int nitem, __ovl_t *res) {
    int i05, i25, i50, i75, i95;
    size_t ndata;
    double sum = 0;
    double ovl_byte, mind, maxd, meand, d25, d50, d75;

    for(int i = 0; i < nitem; i ++) {
        sum += data[i];
        // printf("%f\n", data[i]);
    }
    qsort((void *)data, nitem, sizeof(double), qsort_ascend);

    i05 = 0.05 * nitem;
    i25 = 0.25 * nitem;
    i50 = 0.5 * nitem;
    i75 = 0.75 * nitem;
    i95 = 0.95 * nitem;
    res->meantp = sum / nitem;
    res->min = data[0];
    res->tp05 = data[i05];
    res->tp25 = data[i25];
    res->tp50 = data[i50];
    res->tp75 = data[i75];
    res->tp95 = data[i95];
    res->max = data[nitem - 1];

    return 0;
}

// calculate task volumn per time unit. (e.g. MB/s, Bytes/cy, etc.)
int
calc_rate_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res) {
    double rate[nitem];

    for(int i = 0; i < nitem; i ++) {
        rate[i] = s * volume / (double)raw[i]; // scale_factor * volumne / total_time
    }
    calc_quant(rate, nitem, res);

    return 0;
}

// calculate period per task volumne unit. (e.g. sec/iter, cy/iter, etc.)
int
calc_period_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res) {
    double period[nitem];

    for(int i = 0; i < nitem; i ++) {
        period[i] = s * (double)raw[i] / volume; // scale_factor * total_time / volumne
    }

    calc_quant(period, nitem, res);

    return 0;
}

int
dpipe_k0(uint64_t *ns, uint64_t *cy, int nskip, int ntest, int freq, size_t bpi, size_t niter) {
    __ovl_t res;

    tpprintf(0, 0, 0, OVL_QUANT_HEADER);
    // MB/s
    calc_rate_quant(&ns[nskip], ntest - nskip, niter * bpi, 1e3, &res);
    tpprintf(0, 0, 0, "MB/s    %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n", 
           res.meantp, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95);
    // Byte/cy
    calc_rate_quant(&cy[nskip], ntest - nskip, niter * bpi, 1, &res);
    tpprintf(0, 0, 0, "B/c     %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n", 
           res.meantp, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95);
    if(freq) {
        double *freqs =  (double *)malloc(sizeof(double) * ntest);
        for(int i = nskip; i < ntest; i ++) {
            freqs[i] = (double)cy[i] / (double)ns[i];
            // printf("%d, %f", i, freqs[i]);
        }
        calc_quant(&freqs[nskip], ntest - nskip, &res);
        tpprintf(0, 0, 0, "GHz     %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n", 
               res.meantp, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95);
        free(freqs);
    }

    return 0;
}

int
dpipe_g0(uint64_t **ns, uint64_t **cy, int eid, int nskip, int ntest, int freq, size_t bpi, size_t niter) {
    uint64_t kern_ns[ntest], kern_cy[ntest];
    for(int i = 0; i < ntest; i ++) {
        kern_ns[i] = ns[i][eid];
        kern_cy[i] = cy[i][eid];
        // printf("%llu, %llu\n", kern_ns[i], kern_cy[i]);
    }

    dpipe_k0(kern_ns, kern_cy, nskip, ntest, freq, bpi, niter);

    return 0;
}
