/**
 * @file tpb_npb_result.h
 * @brief Official NPB FOM / verify snapshot for rafdb recording.
 */

#ifndef TPB_NPB_RESULT_H
#define TPB_NPB_RESULT_H

void tpb_npb_result_clear(void);
void tpb_npb_set_result(double time_sec, double mops, int verified,
                        int niter, int n1, int n2, int n3, char cls);
void tpb_npb_set_timer_vectors(int nslot, const double *t1,
                               const double *tmin, const double *tmax,
                               const double *tsum, int nprocs_div);

double tpb_npb_result_time(void);
double tpb_npb_result_mops(void);
int tpb_npb_result_verified(void);
int tpb_npb_result_niter(void);
int tpb_npb_result_n1(void);
int tpb_npb_result_n2(void);
int tpb_npb_result_n3(void);
char tpb_npb_result_class(void);
int tpb_npb_timer_vec_has(void);
int tpb_npb_timer_vec_nslot(void);
int tpb_npb_timer_vec_nprocs(void);
double tpb_npb_timer_vec_local(int i);
double tpb_npb_timer_vec_min(int i);
double tpb_npb_timer_vec_max(int i);
double tpb_npb_timer_vec_avg(int i);

#endif
