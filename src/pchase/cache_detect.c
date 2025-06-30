#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sched.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>

#define MAX_LEVELS 10
#define MAX_SIZES 256
#define MAX_THREADS 256

/* ------------------------- data structures ------------------------- */

typedef struct {
    size_t size_bytes;   /* total reported size of this cache index */
    int    shared_cnt;   /* how many logical CPUs share it */
} cache_info_t;

typedef struct {
    size_t   *seq;       /* array of sizes to test */
    size_t    count;     /* number of elements */
} size_seq_t;

typedef struct {
    size_t  test_size;   /* bytes */
    double  lat_sc_ns;   /* single‑core avg latency */
    double  lat_ac_ns;   /* all‑core avg latency */
} result_t;

/* ------------------------- helpers ------------------------- */

static uint64_t nsec_diff(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
}

/* read a numeric value from a sysfs file */
static long read_long(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = -1;
    if (fscanf(f, "%ld", &v) != 1) v = -1;
    fclose(f);
    return v;
}

/* ------------------------- S0 / cache descriptor ------------------------- */

/*
 * fill cache_info[ level ] with size/ share info obtained from
 * /sys/devices/system/cpu/cpu0/cache/indexX/{size,shared_cpu_list}.
 * returns number of cache levels discovered.
 */
static int detect_cache_hierarchy(cache_info_t *caches) {
    char path[256];
    int levels = 0;

    for (int idx = 0; idx < MAX_LEVELS; ++idx) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
        if (access(path, R_OK) != 0) break; /* no more indices */

        long lv = read_long(path);
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/size", idx);
        long bytes = read_long(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/shared_cpu_list", idx);
        FILE *f = fopen(path, "r");
        int shared = 1;
        if (f) {
            /* parse list like "0‑7,16‑23" → count logical CPUs */
            char buf[128];
            if (fgets(buf, sizeof(buf), f)) {
                shared = 0;
                char *tok = strtok(buf, ",");
                while (tok) {
                    int lo, hi;
                    if (sscanf(tok, "%d-%d", &lo, &hi) == 2)
                        shared += hi - lo + 1;
                    else if (sscanf(tok, "%d", &lo) == 1)
                        shared += 1;
                    tok = strtok(NULL, ",");
                }
            }
            fclose(f);
        }
        size_t sz = (size_t)bytes;
        if (sz == 0) continue;
        caches[lv - 1].size_bytes = sz;
        caches[lv - 1].shared_cnt = shared;
        if (lv > levels) levels = lv;
    }
    return levels;
}

/* ------------------------- command line ------------------------- */
typedef struct {
    size_t min_kb, max_kb, step_kb;
} cli_opts_t;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [--min KB] [--max KB] [--step KB]\n", prog);
}

static void parse_args(int argc, char **argv, cli_opts_t *opt) {
    opt->min_kb = opt->max_kb = opt->step_kb = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--min") && i + 1 < argc) {
            opt->min_kb = strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "--max") && i + 1 < argc) {
            opt->max_kb = strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "--step") && i + 1 < argc) {
            opt->step_kb = strtoul(argv[++i], NULL, 0);
        } else {
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

/* ------------------------- S1B2 generate defaults ------------------------- */
static void derive_defaults(const cache_info_t *caches, int levels, cli_opts_t *opt) {
    if (opt->min_kb == 0 && levels > 0) {
        /* half of L1D per‑core */
        size_t l1 = caches[0].size_bytes / caches[0].shared_cnt / 2;
        opt->min_kb = l1 >> 10;
    }
    if (opt->max_kb == 0 && levels > 0) {
        /* largest cache (LLC) total */
        size_t llc = caches[levels - 1].size_bytes;
        opt->max_kb = llc >> 10;
    }
    if (opt->step_kb == 0) {
        size_t l1 = caches[0].size_bytes / caches[0].shared_cnt;
        opt->step_kb = l1 >> 10;
    }
}

/* ------------------------- S2 build size sequence ------------------------- */
static void build_sequence(const cli_opts_t *opt, size_seq_t *seq) {
    seq->seq   = calloc(MAX_SIZES, sizeof(size_t));
    seq->count = 0;
    size_t min = opt->min_kb << 10;
    size_t max = opt->max_kb << 10;
    size_t step = opt->step_kb << 10;
    for (size_t s = min; s <= max && seq->count < MAX_SIZES; s += step) {
        seq->seq[seq->count++] = s;
        if (min == max) break; /* user requested single size */
    }
}

/* ------------------------- pointer‑chasing buffer ------------------------- */

/* allocate and shuffle an array of ptr sized entries to form a self‑referential ring */
static void **build_ring(size_t bytes) {
    /* align to cache line and huge pages if possible */
    size_t elems = bytes / sizeof(void*);
    size_t alloc_bytes = ((bytes + (1 << 21) - 1) / (1 << 21)) * (1 << 21); /* round 2M */

    void **buf = mmap(NULL, alloc_bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (buf == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    /* simple Fisher‑Yates shuffle of indices */
    size_t *idx = malloc(elems * sizeof(size_t));
    if (!idx) { munmap(buf, alloc_bytes); return NULL; }
    for (size_t i = 0; i < elems; ++i) idx[i] = i;

    for (size_t i = elems - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        size_t tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
    }

    for (size_t i = 0; i < elems; ++i) {
        buf[idx[i]] = &buf[idx[(i + 1) % elems]];
    }
    free(idx);
    return buf;
}

/* ------------------------- S3 single‑core measurement ------------------------- */
static double measure_latency(void **ring, size_t iters) {
    struct timespec t0, t1;
    void **p = ring;

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (size_t i = 0; i < iters; ++i) {
        p = *p;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    uint64_t ns = nsec_diff(t0, t1);
    return (double)ns / iters;
}

static double run_single_core(size_t bytes) {
    void **ring = build_ring(bytes);
    if (!ring) return -1.0;

    /* warmup */
    for (volatile int i = 0; i < 10000; ++i) ring = *ring;

    double lat = measure_latency(ring, 2 * 1024 * 1024);
    munmap(ring, bytes);
    return lat;
}

/* ------------------------- S4 multi‑core measurement ------------------------- */

typedef struct {
    size_t bytes;
    double lat_ns;
} thread_arg_t;

static void *thread_worker(void *arg) {
    thread_arg_t *a = (thread_arg_t *)arg;
    cpu_set_t set; CPU_ZERO(&set);
    long tid = (long)a->lat_ns; /* abuse field to carry cpu id for pinning */
    CPU_SET(tid, &set);
    sched_setaffinity(0, sizeof(set), &set);

    void **ring = build_ring(a->bytes);
    if (!ring) pthread_exit(NULL);
    for (volatile int i = 0; i < 10000; ++i) ring = *ring;
    a->lat_ns = measure_latency(ring, 512 * 1024);
    munmap(ring, a->bytes);
    return NULL;
}

static double run_all_cores(size_t bytes) {
    int nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    if (nthreads > MAX_THREADS) nthreads = MAX_THREADS;

    pthread_t th[MAX_THREADS];
    thread_arg_t args[MAX_THREADS];

    for (int i = 0; i < nthreads; ++i) {
        args[i].bytes = bytes;
        args[i].lat_ns = (double)i; /* store id */
        pthread_create(&th[i], NULL, thread_worker, &args[i]);
    }
    double sum = 0.0;
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(th[i], NULL);
        sum += args[i].lat_ns;
    }
    return sum / nthreads;
}

/* ------------------------- main orchestrator ------------------------- */
int main(int argc, char **argv) {
    srand(time(NULL));

    cache_info_t caches[MAX_LEVELS] = {0};
    int levels = detect_cache_hierarchy(caches);

    cli_opts_t opt = {0};
    parse_args(argc, argv, &opt);
    derive_defaults(caches, levels, &opt);

    size_seq_t seq; build_sequence(&opt, &seq);

    printf("=== Cache & Memory Latency Profiler ===\n");
    printf("SMT active: %s | logical CPUs: %ld\n",
           access("/sys/devices/system/cpu/smt/active", R_OK) ? "?" : (read_long("/sys/devices/system/cpu/smt/active") ? "on" : "off"),
           sysconf(_SC_NPROCESSORS_ONLN));
    printf("Test sizes: %zu entries, %zu KB .. %zu KB\n\n", seq.count, opt.min_kb, opt.max_kb);

    result_t *res = calloc(seq.count, sizeof(result_t));
    for (size_t i = 0; i < seq.count; ++i) {
        res[i].test_size = seq.seq[i];
        res[i].lat_sc_ns = run_single_core(seq.seq[i]);
        res[i].lat_ac_ns = run_all_cores(seq.seq[i]);
        printf("%8zu KB  %.2f ns  %.2f ns\n", seq.seq[i] >> 10, res[i].lat_sc_ns, res[i].lat_ac_ns);
    }

    free(seq.seq);
    free(res);
    return 0;
}
