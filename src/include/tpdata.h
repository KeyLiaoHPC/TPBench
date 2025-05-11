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
 * @file tpdata.h
 * @version 0.3
 * @brief Header for tpbench data processor 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-26
 */


#include <stdint.h>

typedef struct __stat_result {
    double meantp, min, tp05, tp25, tp50, tp75, tp95, max;
} __ovl_t;

#define  OVL_QUANT_HEADER_EXT "        MEAN        MIN        0.05        0.25         0.50         0.75         0.95        MAX\n"
#define  OVL_QUANT_HEADER "        MEAN        0.05        0.25         0.50         0.75         0.95\n"

#define  MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief 
 * @param raw 
 * @param nitem 
 * @param tpt 
 * @param s 
 * @param res 
 * @return int 
 */
int calc_rate_quant(uint64_t * raw, int nitem, double tpt, double s, __ovl_t *res);

/**
 * @brief 
 * @param raw 
 * @param nitem 
 * @param volume 
 * @param s 
 * @param res 
 * @return int 
 */
int calc_period_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res);

/**
 * @brief 
 * @param data 
 * @param nitem 
 * @param res 
 * @return int 
 */
int calc_quant(double *data, int nitem, __ovl_t *res);

/**
 * @brief 
 * @param ns 
 * @param cy 
 * @param nskip 
 * @param ntest 
 * @param freq 
 * @param bpi 
 * @param nsize 
 * @return int 
 */
int dpipe_k0(uint64_t *ns, uint64_t *cy, int nskip, int ntest, int freq, size_t bpi, size_t nsize);

/**
 * @brief 
 * @param ns 
 * @param cy 
 * @param eid 
 * @param nskip 
 * @param ntest 
 * @param freq 
 * @param bpi 
 * @param nsize 
 * @return int 
 */
int dpipe_g0(uint64_t **ns, uint64_t **cy, int eid, int nskip, int ntest, int freq, size_t bpi, size_t niter);