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
void tick_clock_gettime(int64_t *ts);
void tock_clock_gettime(int64_t *ts);
void get_time_clock_gettime(int64_t *ts);

#if defined(__x86_64__) || defined(_M_X64)
int init_timer_tsc_asym(void);
void tick_tsc_asym(int64_t *ts);
void tock_tsc_asym(int64_t *ts);
void get_time_tsc_asym(int64_t *ts);
#endif 
 
 #endif
