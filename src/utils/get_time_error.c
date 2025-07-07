#define _GNU_SOURCE
#define _ISOC11_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <math.h>
#include <string.h>
#include "mpi.h"

#if defined(USE_PAPI) || defined(USE_PAPIX6)
#include "papi.h"

#elif defined(USE_LIKWID)
#include "likwid-marker.h"
#define NEV 20

#endif

// Warmup for 1000ms.
#ifndef NWARM
#define NWARM 1000
#endif

// Ignoring some tests at the beginning
#ifndef NPASS
#define NPASS 2
#endif

// Number of tests for each interval
#ifndef NTEST
#define NTEST 10
#endif

#ifndef TBASE
#define TBASE 100
#endif

// Uniform: V1: Number of DSub between two bins; V2: Number of bins.
// Normal: V1: sigma, standard error
// Pareto: V1: alpha, Pareto exponent 
#ifndef V1
#define V1 100
#endif

// Number of intervals (>=1)
#ifndef V2
#define V2 20
#endif

// Memory footprint for the sflush kernel
#ifndef SFSIZE
#define SFSIZE 0
#endif

// Memory footprint for the rflush kernel
#ifndef RFSIZE
#define RFSIZE 0
#endif

#ifndef LCUT
#define LCUT 1-0x7fffffff
#endif

#ifndef HCUT
#define HCUT 0x7fffffff-1
#endif

// Prefetching the benchmarking instructions.
#ifndef NPRECALC
#define NPRECALC 1
#endif



// Timing macros for Armv8
#ifdef __aarch64__
#ifndef TICK_NS
#define TICK_NS 10
#endif
#define _read_ns(_ns) \
    asm volatile("mrs %0, cntvct_el0"      "\n\t": "=r" (_ns)::); \
    _ns = _ns * TICK_NS;

// Timing macros for x86
#else
#define _read_ns(_ns) \
    do {                                                \
        register uint64_t ns;                           \
        asm volatile(                                   \
            "\n\tRDTSCP"                                \
            "\n\tshl $32, %%rdx"                        \
            "\n\tor  %%rax, %%rdx"                      \
            "\n\tmov %%rdx, %0"                         \
            "\n\t"                                      \
            :"=r" (ns)                                  \
            :                                           \
            : "memory", "%rax", "%rdx");                \
        _ns = ns;                                       \
    } while(0);

#define _mfence asm volatile("lfence"   "\n\t":::);
#endif

#ifdef USE_SER_TSC
void
tsc_start(uint64_t *cycle) {
    unsigned ch, cl;

    asm volatile (  "CPUID" "\n\t"
                    "RDTSC" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    : "=r" (ch), "=r" (cl)
                    :
                    : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ( ((uint64_t)ch << 32) | cl );
}

void
tsc_stop(uint64_t *cycle) {
    unsigned ch, cl;

    asm volatile (  "RDTSCP" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    "CPUID" "\n\t"
                    : "=r" (ch), "=r" (cl)
                    :
                    : "%rax", "%rbx", "%rcx", "%rdx");

    *cycle = ( ((uint64_t)ch << 32) | cl );
}
#endif

/**
 * @brief A desginated one-line computing kernel.
 */
void
rflush(double *a, double *b, double *c, uint64_t npf) {
#ifdef RF_INIT
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = i;
    }

#elif defined(RF_TRIAD)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = 0.42 * b[i] + c[i];
    }

#elif defined(RF_SCALE)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = 1.0001 * b[i];
        b[i] = 1.0001 * a[i];
    }

#elif defined(RF_COPY)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = b[i];
    }

#elif defined(RF_ADD)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = b[i] + c[i];
    }

#elif defined(RF_POW)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = pow(b[i], 1.0001);
    }
#elif defined(RF_DGEMM)
    // get square of npf
    uint64_t sq_npf = (uint64_t)sqrt(npf); 
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, sq_npf, sq_npf, sq_npf, 1.0, a, sq_npf, b, sq_npf, 0.0, c, sq_npf);

#elif defined(RF_BCAST)
    // Broadcast ps_a to all ranks
    MPI_Bcast(a, npf, MPI_DOUBLE, 0, MPI_COMM_WORLD);

#endif
    return;
}

void
sflush(double *a, double *b, double *c, uint64_t npf) {
#ifdef SF_INIT
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = i;
    }

#elif defined(SF_TRIAD) 
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = 0.42 * b[i] + c[i];
    }

#elif defined(SF_SCALE)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = 1.0001 * b[i];
        b[i] = 1.0001 * a[i];
    }

#elif defined(SF_COPY)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = b[i];
    }

#elif defined(SF_ADD)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = b[i] + c[i];
    }

#elif defined(SF_POW)
    for (uint64_t i = 0; i < npf; i ++) {
        a[i] = pow(b[i], 1.0001);
    }
#elif defined(SF_DGEMM)
    // get square of npf
    uint64_t sq_npf = (uint64_t)sqrt(npf); 
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, sq_npf, sq_npf, sq_npf, 1.0, a, sq_npf, b, sq_npf, 0.0, c, sq_npf);

#elif defined(SF_BCAST)
    // Broadcast ps_a to all ranks
    MPI_Bcast(a, npf, MPI_DOUBLE, 0, MPI_COMM_WORLD);

#endif
    return;
}

/**
 * @brief Reading urandom file for random seeds.
 */
int 
get_urandom(uint64_t *x) {
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp == NULL) {
        printf("Failed to open random file.\n");
        return 1;
    } 

    fread(x, sizeof(uint64_t), 1, fp); 

    fclose(fp);

    return 0;
}

/**
 * @brief Generate a shuffled list to loop through all ADD array's lengths.
 */
void
gen_walklist(uint64_t *len_list) {
#ifndef WALK_FILE
#ifdef UNIFORM
    for (int it = 0; it < NTEST; it ++) {
        for (int i = 0; i < V2; i ++) {
            len_list[NPASS+it*V2+i] = TBASE + i * V1;
        }
    }
    // Shuffle
    // Random seeds using nanosec timestamp
    struct timespec tv;
    uint64_t sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    sec = tv.tv_sec;
    nsec = tv.tv_nsec;
    nsec = sec * 1e9 + nsec + NWARM * 1e6;
    srand(nsec); 
    for (int i = NPASS; i < NPASS + NTEST * V2; i ++){
        int r = (int) (((float)rand() / (float)RAND_MAX) * (float)(NTEST * V2));
        uint64_t temp = len_list[i];
        // Randomly swapping two vkern loop length
        len_list[i] = len_list[NPASS+r];
        len_list[NPASS+r] = temp;
    }
    for (int i = 0; i < NPASS; i ++) {
        len_list[i] = TBASE + V1 * V2;
    }
#else
    double v1 = V1;
    uint64_t seed;
    get_urandom(&seed);
    srand(seed);
    for (int i = 0; i < NPASS+NTEST; i++) {
        double x = 0;
#ifdef NORMAL
        do {
            x = 1.0 + gaussian_random(v1);
        } while (x < LCUT || x > HCUT);
#elif defined(PARETO)
        do {
            x = pareto_random(v1, 1.0);
        } while (x > HCUT);
#endif
        len_list[i] = (int)(x*TBASE);
    }
#endif
#else
    // Reading walk list from file
    FILE *fp = fopen("walk.csv", "r");
    for (int it = 0; it < NTEST + NPASS; it ++) {
        fscanf(fp, "%llu\n", len_list+it);
    }
    fclose(fp);
#endif
}

void
print_kernel_name(char *ffkern, char *rfkern) {
#ifdef SF_INIT
    sprintf(ffkern, "INIT");
#elif defined(SF_TRIAD)
    sprintf(ffkern, "TRIAD");
#elif defined(SF_SCALE)
    sprintf(ffkern, "SCALE");
#elif defined(SF_COPY)
    sprintf(ffkern, "COPY");
#elif defined(SF_ADD)
    sprintf(ffkern, "ADD");
#elif defined(SF_POW)
    sprintf(ffkern, "POW");
#elif defined(SF_DGEMM)
    sprintf(ffkern, "DGEMM");
#elif defined(SF_BCAST)
    sprintf(ffkern, "BCAST");
#else
    sprintf(ffkern, "NONE");
#endif

#ifdef RF_INIT
    sprintf(rfkern, "INIT");
#elif defined(RF_TRIAD)
    sprintf(rfkern, "TRIAD");
#elif defined(RF_SCALE)
    sprintf(rfkern, "SCALE");
#elif defined(RF_COPY)
    sprintf(rfkern, "COPY");
#elif defined(RF_ADD)
    sprintf(rfkern, "ADD");
#elif defined(RF_POW)
    sprintf(rfkern, "POW");
#elif defined(RF_DGEMM)
    sprintf(rfkern, "DGEMM");
#elif defined(RF_BCAST)
    sprintf(rfkern, "BCAST");
#else
    sprintf(rfkern, "NONE");
#endif
}

/**
 * @brief Generate a random number from standard normal distribution using Box-Muller transform
 */
double
gaussian_random(double sigma) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    
    // Box-Muller transform
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    
    return z0 * sigma;
}

/**
 * @brief Generate a random number from Pareto distribution using inverse transform
 */
double
pareto_random(double alpha, double xm) {
    double u = (double)rand() / RAND_MAX;
    
    // Inverse transform method for Pareto distribution
    return xm / pow(u, 1.0 / alpha);
}

int
main(int argc, char **argv) {
    int ntest;
    register uint64_t a, b;
    uint64_t *p_len, *p_ns;
    uint64_t ns0, ns1, tstamp_0, tstamp_1, t_total;
    uint64_t tbase=TBASE;
    uint64_t sfsize = SFSIZE, rfsize = RFSIZE, snpf = 0, rnpf = 0; // npf: flush arr length
    double *ps_a, *ps_b, *ps_c, *pr_a, *pr_b, *pr_c;
    int myrank = 0, nrank = 1, errid = 0;
    struct timespec tv;
    FILE *fp;
    char fname[4096], myhost[1024], sfkern[32], rfkern[32];

    gethostname(myhost, 1024);
    print_kernel_name(sfkern, rfkern);

    // Get TSC frequency from command line, if not provided, exit(1)
    if (argc < 2) {
        printf("Please provide TSC frequency.\n");
        exit(1);
    }
    double tsc_freq = atof(argv[1]);

    clock_gettime(CLOCK_MONOTONIC, &tv);
    tstamp_0 = tv.tv_sec * 1e9 + tv.tv_nsec;

#ifdef UNIFORM
    uint64_t v1 = V1, v2 = V2;
    char distro[32] = "uniform";

#elif defined(NORMAL)
    double v1 = V1, v2 = 0;
    char distro[32] = "normal";

#elif defined(PARETO)
    double v1 = V1, v2 = 0;
    char distro[32] = "pareto";
#else
    char distro[32] = "file";
#endif

    errid = MPI_Init(NULL, NULL);
    if (errid != MPI_SUCCESS) {
        printf("Faild to init MPI.\n");
        exit(1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);


#ifdef UNIFORM
    ntest = NPASS + NTEST * V2;
#else
    ntest = NTEST + NPASS;
#endif

#ifdef USE_PAPI
    // Init PAPI
    int eventset = PAPI_NULL;
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&eventset);
    PAPI_start(eventset);
    
#elif defined(USE_PAPIX6)
    // Init PAPI
    int eventset = PAPI_NULL;
    int nev = 6;
    long long int ev_vals_0[6]={0}, ev_vals_1[6]={0};
    int64_t *p_ev;
    p_ev = (int64_t *)malloc(ntest * nev * sizeof(int64_t));
    for (int iev = 0; iev < nev; iev ++) {
        ev_vals_0[iev] = 0;
        ev_vals_1[iev] = 0;
    }
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&eventset);
    PAPI_add_named_event(eventset, "cpu-cycles");
    PAPI_add_named_event(eventset, "instructions");
    PAPI_add_named_event(eventset, "cache-references");
    PAPI_add_named_event(eventset, "cache-misses");
    PAPI_add_named_event(eventset, "branches");
    PAPI_add_named_event(eventset, "branch-misses");
    PAPI_start(eventset);

#elif defined(USE_LIKWID)
    // Init LIKWID
    double ev_vals_0[NEV]={0}, ev_vals_1[NEV]={0}, time;
    int nev = NEV, count;
    int64_t *p_ev;
    LIKWID_MARKER_INIT;
    LIKWID_MARKER_THREADINIT;
    LIKWID_MARKER_REGISTER("vkern"); 
    LIKWID_MARKER_REGISTER("nev_count"); 
    LIKWID_MARKER_START("nev_count");
    LIKWID_MARKER_STOP("nev_count");
    LIKWID_MARKER_GET("nev_count", &nev, (double *)ev_vals_1, &time, &count);
    if (myrank == 0) {
        printf("LIKWID event count = %d\n", nev);
    }
    p_ev = (int64_t *)malloc(ntest * nev * sizeof(int64_t));
    for (int iev = 0; iev < nev; iev ++) {
        ev_vals_0[iev] = 0;
        ev_vals_1[iev] = 0;
    }

    

#endif

    p_len = (uint64_t *)malloc(ntest * sizeof(uint64_t));
#ifndef TIMING_OFF
    p_ns = (uint64_t *)malloc(ntest * sizeof(uint64_t));
#endif
    if (SFSIZE) {
#if defined(SF_INIT) || defined(SF_BCAST)
        snpf  = (uint64_t)(SFSIZE / sizeof(double));
        sfsize = snpf * sizeof(double);
        ps_a = (double *)malloc(snpf * sizeof(double));
        for (uint64_t i = 0; i < snpf; i ++) {
            ps_a[i] = 1.01;
        }
#elif defined(SF_SCALE) || defined(SF_COPY) || defined(SF_POW)
        snpf = (uint64_t)(SFSIZE / sizeof(double) / 2);
        sfsize = snpf * sizeof(double) * 2;
        ps_a = (double *)malloc(snpf * sizeof(double));
        ps_b = (double *)malloc(snpf * sizeof(double));
        for (uint64_t i = 0; i < snpf; i ++) {
            ps_a[i] = 1.01;
            ps_b[i] = 1.01;
        }
#elif defined(SF_TRIAD) || defined(SF_ADD)
        snpf = (uint64_t)(SFSIZE / sizeof(double) / 3);
        sfsize = snpf * sizeof(double) * 3;
        ps_a = (double *)malloc(snpf * sizeof(double));
        ps_b = (double *)malloc(snpf * sizeof(double));
        ps_c = (double *)malloc(snpf * sizeof(double));
        for (uint64_t i = 0; i < snpf; i ++) {
            ps_a[i] = 1.01;
            ps_b[i] = 1.01;
            ps_c[i] = 1.01;
        }
#elif defined(SF_DGEMM)
        snpf = (uint64_t)sqrt(SFSIZE / sizeof(double) / 3);
        sfsize = 3 * snpf * snpf * sizeof(double);
        ps_a = (double *)malloc(snpf * sizeof(double));
        ps_b = (double *)malloc(snpf * sizeof(double));
        ps_c = (double *)malloc(snpf * sizeof(double));
        for (uint64_t i = 0; i < snpf; i ++) {
            ps_a[i] = 1.01;
            ps_b[i] = 1.01;
            ps_c[i] = 1.01;
        }
#endif
        if (myrank == 0) {
            printf("sfkernel: %s, sfsize = %lu Bytes, snpf = %lu\n", sfkern, sfsize, snpf);
        }
    }
    if (RFSIZE) {
#if defined(RF_INIT) || defined(RF_BCAST)
        rnpf  = (uint64_t)(RFSIZE / sizeof(double));
        rfsize = rnpf * sizeof(double);
        pr_a = (double *)malloc(rnpf * sizeof(double));
        for (uint64_t i = 0; i < rnpf; i ++) {
            pr_a[i] = 1.01;
        }
#elif defined(RF_SCALE) || defined(RF_COPY) || defined(RF_POW)
        rnpf = (uint64_t)(RFSIZE / sizeof(double) / 2);
        rfsize = rnpf * sizeof(double) * 2;
        pr_a = (double *)malloc(rnpf * sizeof(double));
        pr_b = (double *)malloc(rnpf * sizeof(double));
        for (uint64_t i = 0; i < rnpf; i ++) {
            pr_a[i] = 1.01;
            pr_b[i] = 1.01;
        }
#elif defined(RF_TRIAD) || defined(RF_ADD)
        rnpf = (uint64_t)(RFSIZE / sizeof(double) / 3);
        rfsize = rnpf * sizeof(double) * 3;
        pr_a = (double *)malloc(rnpf * sizeof(double));
        pr_b = (double *)malloc(rnpf * sizeof(double));
        pr_c = (double *)malloc(rnpf * sizeof(double));
        for (uint64_t i = 0; i < rnpf; i ++) {
            pr_a[i] = 1.01;
            pr_b[i] = 1.01;
            pr_c[i] = 1.01;
        }
#elif defined(RF_DGEMM)
        rnpf = (uint64_t)sqrt(RFSIZE / sizeof(double) / 3);
        rfsize = rnpf * rnpf * sizeof(double);
        pr_a = (double *)malloc(rnpf * sizeof(double));
        pr_b = (double *)malloc(rnpf * sizeof(double));
        pr_c = (double *)malloc(rnpf * sizeof(double));
        for (uint64_t i = 0; i < rnpf; i ++) {
            pr_a[i] = 1.01;
            pr_b[i] = 1.01;
            pr_c[i] = 1.01;
        }
#endif  
        if (myrank == 0) {
            printf("rfkernel: %s, rfsize = %lu Bytes, rnpf = %lu\n", rfkern, rfsize, rnpf);
        }
    }

    if (myrank == 0) {
#ifdef UNIFORM
        printf("Generating uniform ditribution. Tbase=%lu, interval=%lu, nint=%lu, Ntest=%u.\n",
            tbase, v1, v2, NTEST);

#elif defined(NORMAL)
        printf("Generating normal ditribution. Tbase=%lu, sigma=%.4f, Ntest=%lu.\n",
            tbase, v1, NTEST);

#elif defined(PARETO)
        printf("Generating Pareto ditribution. Tbase=%lu, alpha=%.4f, Ntest=%lu.\n",
            tbase, v1, NTEST);
#else
        printf("Reading vkern walk list from walk.csv.\n");

#endif
        fflush(stdout);
        gen_walklist(p_len);
    }

    MPI_Bcast(p_len, ntest, MPI_UINT64_T, 0, MPI_COMM_WORLD);


    a = 0;
    if (argc < 10) {
        b = 1;
    } else {
        b = atoi(argv[1]);
    }
    // Warm up
    do {
        if (myrank == 0) {
            printf("Warming up for %d ms.\n", NWARM);
        }
        uint64_t volatile nsec; // For warmup
        clock_gettime(CLOCK_MONOTONIC, &tv);
        nsec = tv.tv_sec * 1e9 + tv.tv_nsec + NWARM * 1e6;

        while (tv.tv_sec * 1e9 + tv.tv_nsec < nsec) {
            a = b + 1;
            clock_gettime(CLOCK_MONOTONIC, &tv);
        }
    } while (0);

    if (myrank == 0) {
        printf("Start random walking.\n");
    }
    if (myrank == 0) {
        printf("Start testing.\n");
    }
    MPI_Barrier(MPI_COMM_WORLD);
    ns1 = 0;
    ns0 = 0;

    MPI_Barrier(MPI_COMM_WORLD);
    for (int iwalk = 0; iwalk < ntest; iwalk ++) {
        register uint64_t n = p_len[iwalk];
        register uint64_t npre = NPRECALC;
        register uint64_t ra, rb;
        // struct timespec tv;

        // Flushing
        if (snpf) {
            sflush(ps_a, ps_b, ps_c, snpf);
        }

        // Instruction preload.
        ra = n + npre;
        rb = b;
        while (ra != n) {
            ra = ra - rb;
        }

        // Timing.
#ifndef TIMING_OFF
#ifdef USE_PAPI
        ns0 = PAPI_get_real_nsec();

#elif defined(USE_PAPIX6)
        ns0 = PAPI_get_real_nsec();
        PAPI_read(eventset, ev_vals_0);

#elif defined(USE_CGT)
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns0 = tv.tv_sec * 1e9 + tv.tv_nsec;

#elif defined(USE_WTIME)
        ns0 = (uint64_t)(MPI_Wtime() * 1e9);
#elif defined(USE_LIKWID)
        ns0 = ns1;
        LIKWID_MARKER_START("vkern"); 
#elif defined(USE_ASM)
        _read_ns (ns0);
#elif defined(USE_SER_TSC)
        tsc_start(&ns0);
#elif defined(USE_FENCE_TSC)
        _read_ns (ns0);
        _mfence;

#endif

#endif // END: TIMING_OFF

        // ===========DSub Kernel===========
        while (ra!= 0) {
            ra = ra - rb;
        }
        // ===========End of DSub Kernel===========

#ifndef TIMING_OFF
#ifdef USE_PAPI
        ns1 = PAPI_get_real_nsec();

#elif defined(USE_PAPIX6)
        ns1 = PAPI_get_real_nsec();
        PAPI_read(eventset, ev_vals_1);
        for (int iev = 0; iev < nev; iev ++) {
            p_ev[iwalk * nev + iev] = (int64_t)(ev_vals_1[iev] - ev_vals_0[iev]);
        }

#elif defined(USE_CGT)
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns1 = tv.tv_sec * 1e9 + tv.tv_nsec;

#elif defined(USE_WTIME)
        ns1 = (uint64_t)(MPI_Wtime() * 1e9);

#elif defined(USE_LIKWID)
        LIKWID_MARKER_STOP("vkern"); 
        LIKWID_MARKER_GET("vkern", &nev, (double*)ev_vals_1, &time, &count);
        for (int iev = 0; iev < nev; iev ++) {
            p_ev[iwalk * nev + iev] = (int64_t)ev_vals_1[iev] - (int64_t)ev_vals_0[iev];
            ev_vals_0[iev] = ev_vals_1[iev];
        }
        ns1 = (uint64_t)(1e9*time);
#elif defined(USE_ASM)
        _read_ns (ns1);
#elif defined(USE_SER_TSC)
        tsc_stop(&ns1);
        ns1 = (uint64_t)((double)(ns1 - ns0) / tsc_freq);
        ns0 = 0;
#elif defined(USE_FENCE_TSC)
        _read_ns (ns1);
        _mfence;
#endif
        p_ns[iwalk] = ns1 - ns0;

#endif // END: TIMING_OFF
        // Flushing
        if (rnpf) {
            rflush(pr_a, pr_b, pr_c, rnpf);
        }

        a += ra;
        MPI_Barrier(MPI_COMM_WORLD);
    }

#ifdef USE_PAPI
    PAPI_shutdown();

#elif defined(USE_PAPIX6)
    PAPI_shutdown();

#elif defined(USE_LIKWID)
    // Finalize LIKWID
    LIKWID_MARKER_CLOSE;    
#endif // END: USE_PAPI

    // Each rank writes its own file.
#ifdef TIMING_OFF
    char tmethod[32] = "off";
#elif defined(USE_PAPI)
    sprintf(fname, "papi_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "papi";
#elif defined(USE_CGT)
    sprintf(fname, "cgt_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "cgt";
#elif defined(USE_WTIME)
    sprintf(fname, "wtime_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "wtime";
#elif defined(USE_PAPIX6)
    sprintf(fname, "papix6_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "papix6";
#elif defined(USE_LIKWID)
    sprintf(fname, "likwid_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "likwid";
#elif defined(USE_ASM)
    sprintf(fname, "asm_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "asm";
#elif defined(USE_SER_TSC)
    sprintf(fname, "ser_tsc_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "ser_tsc";
#elif defined(USE_FENCE_TSC)
    sprintf(fname, "fence_tsc_time_%d_%s.csv", myrank, myhost);
    fp = fopen(fname, "w");
    char tmethod[32] = "fence_tsc";
#endif // END: TIMING_OFF


#ifndef TIMING_OFF
    for (int iwalk = NPASS; iwalk < ntest; iwalk ++) {
        fprintf(fp, "%d,%lu,%lu", myrank, p_len[iwalk], p_ns[iwalk]);
#if defined(USE_LIKWID) || defined(USE_PAPIX6)
        for (int iev = 0; iev < nev; iev ++) {
            fprintf(fp, ",%ld", p_ev[iwalk*nev+iev]);
        }
#endif // END: USE_LIKWID or USE_PAPIX6
        fprintf(fp, "\n");
    }
    fclose(fp);
#endif // END: TIMING_OFF

#if defined(USE_LIKWID) || defined(USE_PAPIX6)
    free(p_ev);
#endif // END: USE_LIKWID or USE_PAPIX6

    if (myrank == 0) {
        printf("Done. %lu\n", a);
    }

    if (myrank == 0) {
        clock_gettime(CLOCK_MONOTONIC, &tv);
        tstamp_1 = tv.tv_sec * 1e9 + tv.tv_nsec;
        tstamp_1 = tstamp_1 - tstamp_0;
        printf("Total time = %lu ns.\n", tstamp_1);
        // Write run-time parameters to <YYYYMMDDThhmmss>.log
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        char logfile[4096];
        sprintf(logfile, "%04d%02d%02dT%02d%02d%02d.log", 
                tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, 
                tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
        printf("Log file: %s\n", logfile);
        printf("t_total = %lu ns\n", tstamp_1);
        fflush(stdout);
        printf("distribution = %s\n", distro);
        fflush(stdout);
        printf("t_base = %lu\n", tbase);
        fflush(stdout);
        printf("v1 = %f\n", (double)v1);
        fflush(stdout);
        printf("v2 = %f\n", (double)v2);
        fflush(stdout);
        printf("ntest = %u\n", ntest);  
        fflush(stdout);
        printf("timing_method = %s\n", tmethod);
        fflush(stdout);
        printf("sfkern = %s\n", sfkern);
        fflush(stdout);
        printf("sfsize = %lu\n", sfsize);
        fflush(stdout);
        printf("rfkern = %s\n", rfkern);
        fflush(stdout);
        printf("rfsize = %lu\n", rfsize);
        fflush(stdout);
        printf("GHz = %.2f\n", (double)tsc_freq);
        fflush(stdout);

        FILE *flog = fopen(logfile, "w");
        fprintf(flog, "t_total, %lu\n", tstamp_1);
        fprintf(flog, "distribution, %s\n", distro);
        fprintf(flog, "t_base, %lu\n", tbase);
        fprintf(flog, "v1, %f\n", (double)v1);
        fprintf(flog, "v2, %f\n", (double)v2);
        fprintf(flog, "ntest, %u\n", ntest);  
        fprintf(flog, "timing_method, %s\n", tmethod);
        fprintf(flog, "sfkern, %s\n", sfkern);
        fprintf(flog, "sfsize, %lu\n", sfsize);
        fprintf(flog, "rfkern, %s\n", rfkern);
        fprintf(flog, "rfsize, %lu\n", rfsize);
        fprintf(flog, "GHz, %.2f\n", (double)tsc_freq);
        fclose(flog);
    }

    MPI_Finalize();

    return 0;
}
