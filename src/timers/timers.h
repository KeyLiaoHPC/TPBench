 #ifndef TIMERS_H
 #define TIMERS_H
 
 #include <stddef.h>
 #include <stdint.h>
 
 enum timer_name {
    TIMER_CLOCK_GETTIME = 0,
 #ifdef __x86_64__
    TIMER_TSC_ASYM,
 #endif
    TPB_TIMER_COUNT
 };
 
 int init_timer_clock_gettime(void);
 int tick_clock_gettime(int64_t *ts);
 int tock_clock_gettime(int64_t *ts);
 void get_time_clock_gettime(int64_t *ts);
 
 #ifdef __x86_64__
 int init_timer_tsc_asym(void);
 int tick_tsc_asym(int64_t *ts);
 int tock_tsc_asym(int64_t *ts);
 void get_time_tsc_asym(int64_t *ts);
 #endif 
 
 #endif
