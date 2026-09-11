/**
 * @file tpb_npb_timers.h
 * @brief NPB timer ABI implemented with the TPBench timer.
 */

#ifndef TPB_NPB_TIMERS_H
#define TPB_NPB_TIMERS_H

#include "tpbench.h"

int tpb_npb_timers_bind(void);
void tpb_npb_timer_clear(int n);
void tpb_npb_timer_start(int n);
void tpb_npb_timer_stop(int n);
double tpb_npb_timer_read(int n);
int tpb_npb_check_timer_flag(void);

void timer_clear(int n);
void timer_start(int n);
void timer_stop(int n);
double timer_read(int n);
int check_timer_flag(void);

#endif
