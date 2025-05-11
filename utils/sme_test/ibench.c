#include "time.h"
#include "stdio.h"

#ifdef SME
#include "arm_sme.h"
#endif
const int N = 102400000;
struct timespec start, stop;

void time_region_begin() {
    clock_gettime(CLOCK_MONOTONIC, &start);
}

long long time_region_end() {
    clock_gettime(CLOCK_MONOTONIC, &stop);
    return (stop.tv_sec - start.tv_sec) * 1e9 + (stop.tv_nsec - start.tv_nsec);

}

double scalar_latency(double a) {
    for (int i = 0; i < N; i++) {
        a = a * 1.01;
    }
    return a;
}

double scalar_throughput(double a) {
    for (int i = 0; i < N; i++) {
        double r = a * a;
        asm volatile (""::"m" (r):);
    }
    return a;
}

#ifdef SME
 __arm_new("za") __arm_streaming
double sme_latency(double a) {
    svfloat64_t vec = svdup_n_f64(a);
    svbool_t pred = svptrue_b64();
    svzero_za();
    for (int i = 0; i < N; i++) {
        svmopa_za64_f64_m(0, pred, pred, vec, vec);
    }
    double buf[16];
    svst1_hor_za64(0, 0, pred, buf);

    return buf[0];
}

 __arm_new("za") __arm_streaming
double sme_throughput(double a) {
    svfloat64_t vec = svdup_n_f64(a);
    svbool_t pred = svptrue_b64();
    svzero_za();
    for (int i = 0; i < N; i++) {
            svmopa_za64_f64_m(0, pred, pred, vec, vec);
            svmopa_za64_f64_m(1, pred, pred, vec, vec);
            svmopa_za64_f64_m(2, pred, pred, vec, vec);
            svmopa_za64_f64_m(3, pred, pred, vec, vec);
            svmopa_za64_f64_m(4, pred, pred, vec, vec);
            svmopa_za64_f64_m(5, pred, pred, vec, vec);
            svmopa_za64_f64_m(6, pred, pred, vec, vec);
            svmopa_za64_f64_m(7, pred, pred, vec, vec);
    }
    double buf[8][16];
    svst1_hor_za64(0, 0, pred, buf[t]);
    svst1_hor_za64(1, 0, pred, buf[t]);
    svst1_hor_za64(2, 0, pred, buf[t]);
    svst1_hor_za64(3, 0, pred, buf[t]);
    svst1_hor_za64(4, 0, pred, buf[t]);
    svst1_hor_za64(5, 0, pred, buf[t]);
    svst1_hor_za64(6, 0, pred, buf[t]);
    svst1_hor_za64(7, 0, pred, buf[t]);

    double rv = 0;
    for (int t = 0; t < 8; t++) {
        rv += buf[t][0];
    }
    return rv;
}
#endif


int main(int argc, char **argv) {
    double a = argc > 100 ? 1.23 : 1.24;
    double b = 0;
    long long duration;

    time_region_begin();
    b += scalar_latency(a);
    duration = time_region_end();
    printf("scalar mul latency %.2f\n", 1.0 * duration / N);

    time_region_begin();
    b += scalar_throughput(a);
    duration = time_region_end();
    printf("scalar mul throughput %.2f\n", 1.0 * duration / N);

#ifdef SME
    time_region_begin();
    b += sme_latency(a);
    duration = time_region_end();
    printf("sme mul latency %.2f\n", 1.0 * duration / N);

    time_region_begin();
    b += sme_throughput(a);
    duration = time_region_end();
    printf("sme mul throughput %.2f\n", 1.0 * duration / N);
#endif

    return b == 1.23456 ? 1 : 0;
}