/*
 * tpb_npb_result.c
 * Snapshot of official NPB Time / Mop/s / verified for the PLI record.
 */

#include "tpb_npb_result.h"

static double s_time;
static double s_mops;
static int s_verified;
static int s_niter;
static int s_n1;
static int s_n2;
static int s_n3;
static char s_class;

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
