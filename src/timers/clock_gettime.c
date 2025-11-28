/**
 * @file clock_gettime.c
 * @brief: Implementation of clock_gettime timer.
 */

 #define _XOPEN_SOURCE 700
 #define _POSIX_C_SOURCE 200809L
 #define _GNU_SOURCE
 #include <time.h>
 #include <stdint.h>
 #include <unistd.h>
 #include "timers.h"
 
 int init_timer_clock_gettime(void) {
     // No initialization needed for clock_gettime
     return 0;
 }
 
 int64_t tick_clock_gettime(void) {
     struct timespec _tv;
     clock_gettime(CLOCK_MONOTONIC, &_tv);
 
     return (int64_t)_tv.tv_sec * 1000000000LL + (int64_t)_tv.tv_nsec;
 }
 
 int64_t tock_clock_gettime(void) {
     struct timespec _tv;
     clock_gettime(CLOCK_MONOTONIC, &_tv);
 
     return (int64_t)_tv.tv_sec * 1000000000LL + (int64_t)_tv.tv_nsec;
 }
 
 int64_t get_stamp_clock_gettime(void) {
     struct timespec _tv;
     clock_gettime(CLOCK_MONOTONIC, &_tv);
 
     return (int64_t)_tv.tv_sec * 1000000000LL + (int64_t)_tv.tv_nsec;
 }
 