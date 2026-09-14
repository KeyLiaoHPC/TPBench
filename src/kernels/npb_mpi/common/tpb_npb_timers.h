/**
 * @file tpb_npb_timers.h
 * @brief NPB timer ABI implemented with the TPBench timer.
 */

#ifndef TPB_NPB_TIMERS_H
#define TPB_NPB_TIMERS_H

#include <stddef.h>
#include <stdint.h>
#include "tpbench.h"

#ifndef NPB_TIMER_FLAG
#define NPB_TIMER_FLAG 0
#endif

int tpb_npb_timers_bind(void);
void tpb_npb_timer_clear(int n);
void tpb_npb_timer_start(int n);
void tpb_npb_timer_stop(int n);
double tpb_npb_timer_read(int n);
int tpb_npb_check_timer_flag(void);
void tpb_npb_timers_set_sample_cap(size_t n);
void tpb_npb_timers_free(void);
size_t tpb_npb_timer_nsample(int n);
const double *tpb_npb_timer_samples(int n);
uint64_t tpb_npb_timer_ncall(int n);
uint64_t tpb_npb_timer_ndropped(int n);

void timer_clear(int n);
void timer_start(int n);
void timer_stop(int n);
double timer_read(int n);
int check_timer_flag(void);

#endif /* TPB_NPB_TIMERS_H */
