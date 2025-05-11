#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "arm_sme.h"

#define A(i, p) (A[(i) * k + (p)])
#define B(p, j) (B[(p) * n + (j)])
#define C(i, j) (C[(i) * n + (j)])
#define ABS(a) ((a) > 0 ? (a) : (-(a)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

double relative_error(double a, double b) {
    if (a == b)
        return 0;
    return ABS(a - b) / MAX(ABS(b), ABS(a)); 
}

void gemm_basic(int m, int n, int k, double *A, double *B, double *C) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (int l = 0; l < k; ++l) {
                sum += A[i * k + l] * B[l * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

void gemm_sve(int m, int n, int k, double *A, double *B, double *C) {
    const int vec_len = svcntd();
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j+=vec_len) {
            svfloat64_t acc = svdup_n_f64(0.0);
            svbool_t pred = svwhilelt_b64_s32(j, n);
            for (int p = 0; p < k; p++) {
                svfloat64_t v_a = svdup_n_f64(A(i, p));
                svfloat64_t v_b = svld1_f64(pred, &B(p, j));
                acc = svmla_f64_x(pred, acc, v_a, v_b);
            }
            svst1_f64(pred, &C(i, j), acc);
        }
    }
}

__arm_new("za") __arm_streaming 
void gemm_sme(int m, int n, int k, double *A, double *B, double *C) {
    const int vec_len = svcntd();
    for (int i = 0; i < m; i+=vec_len) {
        svbool_t predi = svwhilelt_b64_s32(i, m);
        for (int j = 0; j < n; j+=vec_len) {
            svbool_t predj = svwhilelt_b64_s32(j, n);
            svzero_za();
            double tmp_a[16];

            for (int p = 0; p < k; p++) {
                for (int ii = 0; i + ii < MIN(m, i + vec_len); ii++) {
                    tmp_a[ii] = A(i + ii, p);
                }
                svfloat64_t v_a = svld1_f64(predi, tmp_a);
                svfloat64_t v_b = svld1_f64(predj, &B(p, j));
                svmopa_za64_f64_m(0, predi, predj, v_a, v_b);
            }

            for (int ii = 0; i + ii < MIN(m, i + vec_len); ii++) {
                svst1_hor_za64(0, ii, predj, &C(i + ii, j));
            }
        }
    }
}

// Function to verify the correctness of GEMM implementations
int verify_gemm(int m, int n, int k, double *C1, double *C2) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            if (relative_error(C1[i * n + j], C2[i * n + j]) > 1e-8) {
                printf("Verification failed at position (%d, %d)\n", i, j);
                return 0;
            }
        }
    }
    printf("Verification successful!\n");
    return 1;
}

int main() {
    // Matrix dimensions
    int m = 256;
    int n = 256;
    int k = 256;

    // Allocate memory for matrices
    double *A = (double *)malloc(m * k * sizeof(double));
    double *B = (double *)malloc(k * n * sizeof(double));
    double *C1 = (double *)malloc(m * n * sizeof(double));
    double *C2 = (double *)malloc(m * n * sizeof(double));
    double *C3 = (double *)malloc(m * n * sizeof(double));

    // Initialize matrices with random values
    srand(time(NULL));
    for (int i = 0; i < m * k; ++i) {
        A[i] = (double)rand() / RAND_MAX;
    }
    for (int i = 0; i < k * n; ++i) {
        B[i] = (double)rand() / RAND_MAX;
    }

    gemm_basic(m, n, k, A, B, C1);
    gemm_sve(m, n, k, A, B, C2);
    gemm_sme(m, n, k, A, B, C3);

    // Verify correctness
    printf("Verifiy SVE...\n")
    verify_gemm(m, n, k, C1, C2);
    printf("Verifiy SME...\n")
    verify_gemm(m, n, k, C1, C3);

    return 0;
}
