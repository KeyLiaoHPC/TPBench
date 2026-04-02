/*
 * tpb-stat.c
 * Statistics and data processing functions for TPBench.
 */

#include <stdlib.h>
#include <stdio.h>
#include "tpb-stat.h"
#include "tpb-io.h"

/* Local Function Prototypes */

static int _sf_check_dtype_support(TPB_DTYPE dtype);
static int _sf_compare_ascend(const void *a, const void *b);
static double _sf_get_element(void *arr, size_t idx, TPB_DTYPE dtype);

/* Local Function Implementations */

static int
_sf_compare_ascend(const void *a, const void *b)
{
    if (*(double *)a < *(double *)b) {
        return -1;
    }
    return *(double *)a > *(double *)b;
}

static int
_sf_check_dtype_support(TPB_DTYPE dtype)
{
    TPB_DTYPE type_only = dtype & TPB_PARM_TYPE_MASK;
    switch (type_only) {
    case TPB_INT_T:
    case TPB_INT8_T:
    case TPB_INT16_T:
    case TPB_INT32_T:
    case TPB_INT64_T:
    case TPB_UINT8_T:
    case TPB_UINT16_T:
    case TPB_UINT32_T:
    case TPB_UINT64_T:
    case TPB_FLOAT_T:
    case TPB_DOUBLE_T:
        return 1;
    default:
        return 0;
    }
}

static double
_sf_get_element(void *arr, size_t idx, TPB_DTYPE dtype)
{
    TPB_DTYPE type_only = dtype & TPB_PARM_TYPE_MASK;
    switch (type_only) {
    case TPB_INT_T:
        return (double)((int *)arr)[idx];
    case TPB_INT8_T:
        return (double)((int8_t *)arr)[idx];
    case TPB_INT16_T:
        return (double)((int16_t *)arr)[idx];
    case TPB_INT32_T:
        return (double)((int32_t *)arr)[idx];
    case TPB_INT64_T:
        return (double)((int64_t *)arr)[idx];
    case TPB_UINT8_T:
        return (double)((uint8_t *)arr)[idx];
    case TPB_UINT16_T:
        return (double)((uint16_t *)arr)[idx];
    case TPB_UINT32_T:
        return (double)((uint32_t *)arr)[idx];
    case TPB_UINT64_T:
        return (double)((uint64_t *)arr)[idx];
    case TPB_FLOAT_T:
        return (double)((float *)arr)[idx];
    case TPB_DOUBLE_T:
        return ((double *)arr)[idx];
    default:
        return 0.0;
    }
}

/* Public Function Implementations */

int
tpb_stat_qtile_1d(void *arr, size_t narr, TPB_DTYPE dtype,
                  double *qarr, size_t nq, double *qout)
{
    if (arr == NULL || qarr == NULL || qout == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (!_sf_check_dtype_support(dtype)) {
        return TPBE_DTYPE_NOT_SUPPORTED;
    }

    /* Allocate temporary double array for sorting */
    double *tmp = (double *)malloc(narr * sizeof(double));
    if (tmp == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    /* Copy and cast elements to double */
    for (size_t i = 0; i < narr; i++) {
        tmp[i] = _sf_get_element(arr, i, dtype);
    }

    /* Sort the temporary array */
    qsort(tmp, narr, sizeof(double), _sf_compare_ascend);

    /* Calculate quantile values */
    for (size_t i = 0; i < nq; i++) {
        /* Round down index: idx = (int)(q * narr) */
        size_t idx = (size_t)(qarr[i] * (double)narr);
        /* Clamp to valid range [0, narr-1] */
        if (idx >= narr) {
            idx = narr - 1;
        }
        qout[i] = tmp[idx];
    }

    free(tmp);
    return TPBE_SUCCESS;
}

int
tpb_stat_mean(void *arr, size_t narr, TPB_DTYPE dtype, double *mean_out)
{
    if (arr == NULL || mean_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (!_sf_check_dtype_support(dtype)) {
        return TPBE_DTYPE_NOT_SUPPORTED;
    }

    double sum = 0.0;
    for (size_t i = 0; i < narr; i++) {
        sum += _sf_get_element(arr, i, dtype);
    }

    *mean_out = sum / (double)narr;
    return TPBE_SUCCESS;
}

int
tpb_stat_max(void *arr, size_t narr, TPB_DTYPE dtype, double *max_out)
{
    if (arr == NULL || max_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (!_sf_check_dtype_support(dtype)) {
        return TPBE_DTYPE_NOT_SUPPORTED;
    }

    double max_val = _sf_get_element(arr, 0, dtype);
    for (size_t i = 1; i < narr; i++) {
        double val = _sf_get_element(arr, i, dtype);
        if (val > max_val) {
            max_val = val;
        }
    }

    *max_out = max_val;
    return TPBE_SUCCESS;
}

int
tpb_stat_min(void *arr, size_t narr, TPB_DTYPE dtype, double *min_out)
{
    if (arr == NULL || min_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (!_sf_check_dtype_support(dtype)) {
        return TPBE_DTYPE_NOT_SUPPORTED;
    }

    double min_val = _sf_get_element(arr, 0, dtype);
    for (size_t i = 1; i < narr; i++) {
        double val = _sf_get_element(arr, i, dtype);
        if (val < min_val) {
            min_val = val;
        }
    }

    *min_out = min_val;
    return TPBE_SUCCESS;
}

/* Legacy functions (kept for backward compatibility) */

int
calc_quant(double *data, int nitem, __ovl_t *res)
{
    int i05, i25, i50, i75, i95;
    double sum = 0;

    for (int i = 0; i < nitem; i++) {
        sum += data[i];
    }
    qsort((void *)data, nitem, sizeof(double), _sf_compare_ascend);

    i05 = (int)(0.05 * nitem);
    i25 = (int)(0.25 * nitem);
    i50 = (int)(0.5 * nitem);
    i75 = (int)(0.75 * nitem);
    i95 = (int)(0.95 * nitem);
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

int
calc_rate_quant(int64_t *raw, int nitem, double volume, double s, __ovl_t *res)
{
    double *rate = (double *)malloc(nitem * sizeof(double));
    if (rate == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    for (int i = 0; i < nitem; i++) {
        rate[i] = s * volume / (double)raw[i];
    }
    calc_quant(rate, nitem, res);

    free(rate);
    return 0;
}

int
calc_period_quant(uint64_t *raw, int nitem, double volume, double s, __ovl_t *res)
{
    double *period = (double *)malloc(nitem * sizeof(double));
    if (period == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    for (int i = 0; i < nitem; i++) {
        period[i] = s * (double)raw[i] / volume;
    }

    calc_quant(period, nitem, res);

    free(period);
    return 0;
}

int
dpipe_k0(int64_t *time_arr, int nskip, int ntest, int freq, size_t bpi, size_t niter)
{
    __ovl_t res;

    tpb_printf(TPBM_PRTN_M_DIRECT, OVL_QUANT_HEADER "\n");
    /* MB/s */
    calc_rate_quant(&time_arr[nskip], ntest - nskip, niter * bpi, 1e3, &res);
    tpb_printf(TPBM_PRTN_M_DIRECT,
               "MB/s    %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
               res.meantp, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95);

    return 0;
}
