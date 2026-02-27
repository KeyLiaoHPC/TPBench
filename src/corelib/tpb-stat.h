/**
 * @file tpb-stat.h
 * @brief Statistics and data processing functions for TPBench.
 */

#ifndef TPB_STAT_H
#define TPB_STAT_H

#include <stddef.h>
#include <stdint.h>
#include "tpb-types.h"

/** @brief Legacy statistics result structure (deprecated) */
typedef struct __stat_result {
    double meantp, min, tp05, tp25, tp50, tp75, tp95, max;
} __ovl_t;

#define OVL_QUANT_HEADER_EXT "        MEAN        MIN        0.05        0.25         0.50         0.75         0.95        MAX\n"
#define OVL_QUANT_HEADER "        MEAN        0.05        0.25         0.50         0.75         0.95\n"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Calculate quantiles from a 1D array of generic data type.
 *
 * @param arr Pointer to input 1D array of data.
 * @param narr Number of elements in the input array.
 * @param dtype Data type of the input array elements.
 * @param qarr Pointer to array of quantile positions (values in [0.0, 1.0]).
 * @param nq Number of quantile positions in qarr.
 * @param qout Pointer to output array for calculated quantile values.
 * @return TPBE_SUCCESS on success, error code otherwise.
 */
int tpb_stat_qtile_1d(void *arr, size_t narr, TPB_DTYPE dtype,
                      double *qarr, size_t nq, double *qout);

/**
 * @brief Calculate the arithmetic mean of a 1D array.
 *
 * @param arr Pointer to input 1D array of data.
 * @param narr Number of elements in the input array.
 * @param dtype Data type of the input array elements.
 * @param mean_out Pointer to output double for the mean.
 * @return TPBE_SUCCESS on success, error code otherwise.
 */
int tpb_stat_mean(void *arr, size_t narr, TPB_DTYPE dtype, double *mean_out);

/**
 * @brief Find the maximum value in a 1D array.
 *
 * @param arr Pointer to input 1D array of data.
 * @param narr Number of elements in the input array.
 * @param dtype Data type of the input array elements.
 * @param max_out Pointer to output double for the maximum.
 * @return TPBE_SUCCESS on success, error code otherwise.
 */
int tpb_stat_max(void *arr, size_t narr, TPB_DTYPE dtype, double *max_out);

/**
 * @brief Find the minimum value in a 1D array.
 *
 * @param arr Pointer to input 1D array of data.
 * @param narr Number of elements in the input array.
 * @param dtype Data type of the input array elements.
 * @param min_out Pointer to output double for the minimum.
 * @return TPBE_SUCCESS on success, error code otherwise.
 */
int tpb_stat_min(void *arr, size_t narr, TPB_DTYPE dtype, double *min_out);

/* Legacy functions (kept for backward compatibility) */

/** @brief Calculate rate-based quantiles from raw timing data (deprecated) */
int calc_rate_quant(int64_t *raw, int nitem, double volume, double s, __ovl_t *res);

/** @brief Calculate period-based quantiles from raw timing data (deprecated) */
int calc_period_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res);

/** @brief Calculate quantiles from double array (deprecated) */
int calc_quant(double *data, int nitem, __ovl_t *res);

/** @brief Legacy data pipeline function (deprecated) */
int dpipe_k0(int64_t *time_arr, int nskip, int ntest, int freq, size_t bpi, size_t nsize);

#endif /* TPB_STAT_H */
