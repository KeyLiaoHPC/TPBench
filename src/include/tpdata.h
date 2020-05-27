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
 * @file tpdata.h
 * @version 0.3
 * @brief Header for tpbench data processor 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2020-05-26
 */

#define  OVL_QUANT_HEADER "        MEAN        MIN         1/4         1/2         3/4         MAX\n"

#include <stdint.h>

typedef struct __stat_result {
    double mintp, maxtp, meantp, tp25, tp50, tp75;
} __ovl_t;


int calc_rate_quant(uint64_t * raw, int nitem, double tpt, double s, __ovl_t *res);

int calc_period_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res);

int calc_quant(double *data, int nitem, __ovl_t *res);


int dpipe_k0(uint64_t *ns, uint64_t *cy, int nskip, int ntest, int freq, size_t bpi, size_t nsize);