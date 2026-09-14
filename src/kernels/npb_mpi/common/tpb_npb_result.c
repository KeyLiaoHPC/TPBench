/*
 * tpb_npb_result.c
 * Snapshot of official NPB Time / Mop/s / verified for the PLI record.
 */

#include <string.h>
#include "tpb_npb_result.h"

#define TPB_NPB_TVEC_MAX 64

static double _sf_tvec_at(const double *a, int i);

static double s_time;
static double s_mops;
static int s_verified;
static int s_niter;
static int s_n1;
static int s_n2;
static int s_n3;
static char s_class;
static int s_tvec_has;
static int s_tvec_nslot;
static int s_tvec_nprocs;
static double s_tvec_local[TPB_NPB_TVEC_MAX];
static double s_tvec_min[TPB_NPB_TVEC_MAX];
static double s_tvec_max[TPB_NPB_TVEC_MAX];
static double s_tvec_sum[TPB_NPB_TVEC_MAX];

void
tpb_npb_result_clear(void)
{
    s_time = 0.0;
    s_mops = 0.0;
    s_verified = 0;
    s_niter = 0;
    s_n1 = 0;
    s_n2 = 0;
    s_n3 = 0;
    s_class = 'S';
    s_tvec_has = 0;
    s_tvec_nslot = 0;
    s_tvec_nprocs = 0;
    memset(s_tvec_local, 0, sizeof(s_tvec_local));
    memset(s_tvec_min, 0, sizeof(s_tvec_min));
    memset(s_tvec_max, 0, sizeof(s_tvec_max));
    memset(s_tvec_sum, 0, sizeof(s_tvec_sum));
}

void
tpb_npb_set_result(double time_sec, double mops, int verified,
                   int niter, int n1, int n2, int n3, char cls)
{
    s_time = time_sec;
    s_mops = mops;
    s_verified = verified ? 1 : 0;
    s_niter = niter;
    s_n1 = n1;
    s_n2 = n2;
    s_n3 = n3;
    s_class = cls;
}

double
tpb_npb_result_time(void)
{
    return s_time;
}

double
tpb_npb_result_mops(void)
{
    return s_mops;
}

int
tpb_npb_result_verified(void)
{
    return s_verified;
}

int
tpb_npb_result_niter(void)
{
    return s_niter;
}

int
tpb_npb_result_n1(void)
{
    return s_n1;
}

int
tpb_npb_result_n2(void)
{
    return s_n2;
}

int
tpb_npb_result_n3(void)
{
    return s_n3;
}

char
tpb_npb_result_class(void)
{
    return s_class;
}

void
tpb_npb_set_timer_vectors(int nslot, const double *t1,
                          const double *tmin, const double *tmax,
                          const double *tsum, int nprocs_div)
{
    int n;
    int i;

    if (t1 == NULL || tmin == NULL || tmax == NULL || tsum == NULL) {
        return;
    }
    n = nslot;
    if (n < 0) {
        n = 0;
    }
    if (n > TPB_NPB_TVEC_MAX) {
        n = TPB_NPB_TVEC_MAX;
    }
    s_tvec_nslot = n;
    s_tvec_nprocs = (nprocs_div > 0) ? nprocs_div : 1;
    for (i = 0; i < n; i++) {
        s_tvec_local[i] = t1[i];
        s_tvec_min[i] = tmin[i];
        s_tvec_max[i] = tmax[i];
        s_tvec_sum[i] = tsum[i];
    }
    s_tvec_has = 1;
}

int
tpb_npb_timer_vec_has(void)
{
    return s_tvec_has;
}

int
tpb_npb_timer_vec_nslot(void)
{
    return s_tvec_nslot;
}

int
tpb_npb_timer_vec_nprocs(void)
{
    return s_tvec_nprocs;
}

static double
_sf_tvec_at(const double *a, int i)
{
    if (!s_tvec_has || i < 0 || i >= s_tvec_nslot) {
        return 0.0;
    }
    return a[i];
}

double
tpb_npb_timer_vec_local(int i)
{
    return _sf_tvec_at(s_tvec_local, i);
}

double
tpb_npb_timer_vec_min(int i)
{
    return _sf_tvec_at(s_tvec_min, i);
}

double
tpb_npb_timer_vec_max(int i)
{
    return _sf_tvec_at(s_tvec_max, i);
}

double
tpb_npb_timer_vec_avg(int i)
{
    if (!s_tvec_has || i < 0 || i >= s_tvec_nslot || s_tvec_nprocs <= 0) {
        return 0.0;
    }
    return s_tvec_sum[i] / (double)s_tvec_nprocs;
}
