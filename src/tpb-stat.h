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
 * @file tpb-stat.h
 * @version 0.4
 * @brief Header for tpbench statistics and data processing functions.
 *        Provides generic statistics functions supporting multiple data types
 *        via TPB_DTYPE, including quantile, mean, min, and max calculations.
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-26
 */

#ifndef TPB_STAT_H
#define TPB_STAT_H

#include <stddef.h>
#include <stdint.h>
#include "tpb-types.h"

/**
 * @brief [DEPRECATED] Legacy statistics result structure.
 *        Use tpb_stat_qtile_1d, tpb_stat_mean, tpb_stat_min, tpb_stat_max instead.
 */
typedef struct __stat_result {
    double meantp, min, tp05, tp25, tp50, tp75, tp95, max;
} __ovl_t;

#define OVL_QUANT_HEADER_EXT "        MEAN        MIN        0.05        0.25         0.50         0.75         0.95        MAX\n"
#define OVL_QUANT_HEADER "        MEAN        0.05        0.25         0.50         0.75         0.95\n"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Calculate quantiles from a 1D array of generic data type.
 *
 * This function calculates the specified quantile positions from the input array.
 * It supports various integer and floating-point types defined by TPB_DTYPE.
 * The input array is copied internally for sorting, so the original data is preserved.
 *
 * @param arr       Pointer to input 1D array of data. Must not be NULL.
 * @param narr      Number of elements in the input array. Must be >= 1.
 * @param dtype     Data type of the input array elements. Supported types:
 *                  TPB_INT_T, TPB_INT8_T, TPB_INT16_T, TPB_INT32_T, TPB_INT64_T,
 *                  TPB_UINT8_T, TPB_UINT16_T, TPB_UINT32_T, TPB_UINT64_T,
 *                  TPB_FLOAT_T, TPB_DOUBLE_T.
 * @param qarr      Pointer to array of quantile positions (values in [0.0, 1.0]).
 *                  E.g., {0.05, 0.25, 0.50, 0.75, 0.95}. Must not be NULL.
 * @param nq        Number of quantile positions in qarr. Must be >= 1.
 * @param qout      Pointer to output array where calculated quantile values will be
 *                  stored. Must have at least nq elements. Must not be NULL.
 * @return          TPBE_SUCCESS (0) on success.
 *                  TPBE_NULLPTR_ARG if arr, qarr, or qout is NULL.
 *                  TPBE_DTYPE_NOT_SUPPORTED if dtype is not a supported type.
 *                  TPBE_MALLOC_FAIL if memory allocation fails.
 *
 * @note When narr is small (down to 1), quantile indices are computed by rounding
 *       down: idx = (int)(q * narr). This allows the function to work even with
 *       single-element arrays.
 */
int tpb_stat_qtile_1d(void *arr, size_t narr, TPB_DTYPE dtype,
                      double *qarr, size_t nq, double *qout);

/**
 * @brief Calculate the arithmetic mean of a 1D array of generic data type.
 *
 * @param arr       Pointer to input 1D array of data. Must not be NULL.
 * @param narr      Number of elements in the input array. Must be >= 1.
 * @param dtype     Data type of the input array elements. Supported types:
 *                  TPB_INT_T, TPB_INT8_T, TPB_INT16_T, TPB_INT32_T, TPB_INT64_T,
 *                  TPB_UINT8_T, TPB_UINT16_T, TPB_UINT32_T, TPB_UINT64_T,
 *                  TPB_FLOAT_T, TPB_DOUBLE_T.
 * @param mean_out  Pointer to output double where the mean will be stored.
 *                  Must not be NULL.
 * @return          TPBE_SUCCESS (0) on success.
 *                  TPBE_NULLPTR_ARG if arr or mean_out is NULL.
 *                  TPBE_DTYPE_NOT_SUPPORTED if dtype is not a supported type.
 */
int tpb_stat_mean(void *arr, size_t narr, TPB_DTYPE dtype, double *mean_out);

/**
 * @brief Find the maximum value in a 1D array of generic data type.
 *
 * @param arr       Pointer to input 1D array of data. Must not be NULL.
 * @param narr      Number of elements in the input array. Must be >= 1.
 * @param dtype     Data type of the input array elements. Supported types:
 *                  TPB_INT_T, TPB_INT8_T, TPB_INT16_T, TPB_INT32_T, TPB_INT64_T,
 *                  TPB_UINT8_T, TPB_UINT16_T, TPB_UINT32_T, TPB_UINT64_T,
 *                  TPB_FLOAT_T, TPB_DOUBLE_T.
 * @param max_out   Pointer to output double where the maximum will be stored.
 *                  Must not be NULL.
 * @return          TPBE_SUCCESS (0) on success.
 *                  TPBE_NULLPTR_ARG if arr or max_out is NULL.
 *                  TPBE_DTYPE_NOT_SUPPORTED if dtype is not a supported type.
 */
int tpb_stat_max(void *arr, size_t narr, TPB_DTYPE dtype, double *max_out);

/**
 * @brief Find the minimum value in a 1D array of generic data type.
 *
 * @param arr       Pointer to input 1D array of data. Must not be NULL.
 * @param narr      Number of elements in the input array. Must be >= 1.
 * @param dtype     Data type of the input array elements. Supported types:
 *                  TPB_INT_T, TPB_INT8_T, TPB_INT16_T, TPB_INT32_T, TPB_INT64_T,
 *                  TPB_UINT8_T, TPB_UINT16_T, TPB_UINT32_T, TPB_UINT64_T,
 *                  TPB_FLOAT_T, TPB_DOUBLE_T.
 * @param min_out   Pointer to output double where the minimum will be stored.
 *                  Must not be NULL.
 * @return          TPBE_SUCCESS (0) on success.
 *                  TPBE_NULLPTR_ARG if arr or min_out is NULL.
 *                  TPBE_DTYPE_NOT_SUPPORTED if dtype is not a supported type.
 */
int tpb_stat_min(void *arr, size_t narr, TPB_DTYPE dtype, double *min_out);

/* === Legacy functions (kept for backward compatibility) === */

/**
 * @brief [DEPRECATED] Calculate rate-based quantiles from raw timing data.
 * @param raw       Raw timing data array
 * @param nitem     Number of items
 * @param volume    Volume/size of work
 * @param s         Scale factor
 * @param res       Pointer to result structure
 * @return          0 on success
 */
int calc_rate_quant(int64_t *raw, int nitem, double volume, double s, __ovl_t *res);

/**
 * @brief [DEPRECATED] Calculate period-based quantiles from raw timing data.
 * @param raw       Raw timing data array
 * @param nitem     Number of items
 * @param volume    Volume/size of work
 * @param s         Scale factor
 * @param res       Pointer to result structure
 * @return          0 on success
 */
int calc_period_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res);

/**
 * @brief [DEPRECATED] Calculate quantiles from double array.
 *        Use tpb_stat_qtile_1d with TPB_DOUBLE_T instead.
 * @param data      Input double array
 * @param nitem     Number of items
 * @param res       Pointer to result structure
 * @return          0 on success
 */
int calc_quant(double *data, int nitem, __ovl_t *res);

/**
 * @brief [DEPRECATED] Legacy data pipeline function.
 * @param time_arr  1D array of measured runtime
 * @param nskip     Number of initial iterations to skip
 * @param ntest     Total number of test iterations
 * @param freq      Timer frequency
 * @param bpi       Bytes per iteration
 * @param nsize     Size of work
 * @return          0 on success
 */
int dpipe_k0(int64_t *time_arr, int nskip, int ntest, int freq, size_t bpi, size_t nsize);

#endif /* TPB_STAT_H */
