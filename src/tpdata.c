/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
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
 * @file tpdata.c
 * @version 0.3
 * @brief factory of benchmarking data
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2020-05-27
 */
#include <stdlib.h>
#include "tpdata.h"

int qsort_ascend(const void * a, const void * b) {
   return ( *(double *)a - *(double *)b );
}


// calculate mean, min, max, quantile in a n-item 1d fp64 array
int
calc_quant(double *data, int nitem, __ovl_t *res) {
    int i25, i50, i75;
    size_t ndata;
    double sum = 0;
    double ovl_byte, mind, maxd, meand, d25, d50, d75;

    for(int i = 0; i < nitem; i ++) {
        sum += data[i];
    }
    qsort(data, nitem, sizeof(uint64_t), qsort_ascend);
    i25 = 0.25 * nitem;
    i50 = 0.50 * nitem;
    i75 = 0.75 * nitem;
    res->mintp = data[0];
    res->maxtp = data[nitem-1];
    res->meantp = sum / nitem;
    res->tp25 = data[i25];
    res->tp50 = data[i50];
    res->tp75 = data[i75];

    return 0;
}

// calculate task volumn per time unit. (e.g. MB/s, Bytes/cy, etc.)
int
calc_rate_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res) {
    double rate[nitem];

    for(int i = 0; i < nitem; i ++) {
        rate[i] = s * volume / (double)raw[i]; // scale_factor * volumne / total_time
        // printf("%d, %llu,%f\n",i, raw[i], rate[i]);
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