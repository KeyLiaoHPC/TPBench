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
#include "pchase.h"

#define MAX_LEVELS 10
#define MAX_SIZES 256
#define MAX_THREADS 256

/* Macro expansion to avoid compiler optimizations */
#define LOOP1(x) x
#define LOOP2(x) x x
#define LOOP3(x) x x x
#define LOOP4(x) x x x x
#define LOOP5(x) x x x x x
#define LOOP10(x) LOOP5(x) LOOP5(x)
#define LOOP100(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x) LOOP10(x)

#define LOOP1000(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x) LOOP100(x)

#define LOOP10000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x) LOOP1000(x)

#define LOOP100000(x) LOOP50000(x) LOOP50000(x)

/* Macro to repeat pointer chasing for better timing accuracy */
#define REPEAT_POINTER_CHASE_10K(p) LOOP10000(p = *(void **)p;)

/* ------------------------- data structures ------------------------- */

typedef pchase_cache_info_t cache_info_t;

typedef struct {
    size_t *seq;       /* array of sizes to test */
    size_t count;     /* number of elements */
} size_seq_t;

typedef struct {
    size_seq_t single_core;  /* sizes for single-core tests */
    size_seq_t multi_core;   /* sizes for multi-core tests */
} test_sizes_t;

/* ------------------------- helpers ------------------------- */

static uint64_t nsec_diff(struct timespec a, struct timespec b)
{
    return (uint64_t)(b.tv_sec - a.tv_sec) * 1000000000ULL + (b.tv_nsec - a.tv_nsec);
}

/* read a numeric value from a sysfs file */
static long read_long(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    long v = -1;
    if (fscanf(f, "%ld", &v) != 1)
        v = -1;
    fclose(f);
    return v;
}

/* read cache size with proper unit handling (K, M, G suffixes) */
static size_t read_cache_size(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    
    char buf[64];
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    
    /* Remove newline */
    buf[strcspn(buf, "\n")] = 0;
    
    size_t size = 0;
    char unit = 0;
    if (sscanf(buf, "%zu%c", &size, &unit) >= 1) {
        switch (unit) {
            case 'K': size *= 1024; break;
            case 'M': size *= 1024 * 1024; break;
            case 'G': size *= 1024 * 1024 * 1024; break;
            default: break; /* assume bytes if no unit */
        }
    }
    return size;
}

/* ------------------------- S0 / cache descriptor ------------------------- */

/*
 * fill cache_info[ level ] with size/ share info obtained from
 * /sys/devices/system/cpu/cpu0/cache/indexX/{size,shared_cpu_list}.
 * returns number of cache levels discovered.
 */
static int detect_cache_hierarchy(cache_info_t *caches)
{
    char path[256];
    int levels = 0;

    for (int idx = 0; idx < MAX_LEVELS; ++idx) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
        if (access(path, R_OK) != 0)
            break; /* no more indices */

        long lv = read_long(path);
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/size", idx);
        size_t bytes = read_cache_size(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/shared_cpu_list", idx);
        FILE *f = fopen(path, "r");
        int shared = 1;
        if (f) {
            /* parse list like "0‑7,16‑23" → count logical CPUs */
            char buf[128];
            if (fgets(buf, sizeof(buf), f)) {
                /* Remove newline if present */
                buf[strcspn(buf, "\n")] = 0;
                strncpy(caches[lv - 1].shared_cpu_list, buf, sizeof(caches[lv - 1].shared_cpu_list) - 1);
                caches[lv - 1].shared_cpu_list[sizeof(caches[lv - 1].shared_cpu_list) - 1] = '\0';
                
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
        } else {
            caches[lv - 1].shared_cpu_list[0] = '\0';
        }
        
        if (bytes == 0)
            continue;
        caches[lv - 1].size_bytes = bytes;
        caches[lv - 1].shared_cnt = shared;
        if (lv > levels)
            levels = lv;
    }
    return levels;
}

/* Helper function to remove duplicates and sort a size sequence */
static void deduplicate_and_sort(size_seq_t *seq)
{
    if (seq->count <= 1)
        return;
    
    /* Simple bubble sort and deduplication */
    for (size_t i = 0; i < seq->count - 1; ++i) {
        for (size_t j = 0; j < seq->count - i - 1; ++j) {
            if (seq->seq[j] > seq->seq[j + 1]) {
                size_t temp = seq->seq[j];
                seq->seq[j] = seq->seq[j + 1];
                seq->seq[j + 1] = temp;
            }
        }
    }
    
    /* Remove duplicates */
    size_t write_idx = 0;
    for (size_t read_idx = 1; read_idx < seq->count; ++read_idx) {
        if (seq->seq[read_idx] != seq->seq[write_idx]) {
            write_idx++;
            seq->seq[write_idx] = seq->seq[read_idx];
        }
    }
    seq->count = write_idx + 1;
}

/* ------------------------- S2 build size sequences ------------------------- */
static int build_test_sizes(const cache_info_t *caches, int levels, test_sizes_t *sizes, bool is_single_core)
{
    /* Initialize sequences */
    if (is_single_core) {
        sizes->single_core.seq = calloc(MAX_SIZES, sizeof(size_t));
        if (!sizes->single_core.seq)
            return -1;
        sizes->single_core.count = 0;
    } else {
        sizes->multi_core.seq = calloc(MAX_SIZES, sizeof(size_t));
        if (!sizes->multi_core.seq)
            return -1;
        sizes->multi_core.count = 0;
    }

    /* Auto-detect rule-based approach */
    for (int level = 0; level < levels; ++level) {
        size_t detected_size = caches[level].size_bytes;
        size_t per_core_size = detected_size / caches[level].shared_cnt;
        
        if (is_single_core) {
            /* Single-core sizes */
            if (level == levels - 1) {  /* L3 */
                size_t l3_sizes[] = {
                    per_core_size / 2,      /* 0.5 * per_core_size */
                    per_core_size,          /* per_core_size */
                    detected_size,
                    detected_size * 2,  /* per_core_size * shared_core_count */
                    detected_size * 3,      /* 3 * detected_size */
                    detected_size * 4       /* 4 * detected_size */
                };
                
                for (size_t i = 0; i < sizeof(l3_sizes)/sizeof(l3_sizes[0]) && 
                     sizes->single_core.count < MAX_SIZES; ++i) {
                    sizes->single_core.seq[sizes->single_core.count++] = l3_sizes[i];
                }
            } else {  /* L1 & L2 */
                size_t l12_sizes[] = {
                    per_core_size / 2,      /* 0.5 * per_core_size */
                    per_core_size,          /* per_core_size */
                    per_core_size * caches[level].shared_cnt   /* per_core_size * shared_core_count */
                };
                
                for (size_t i = 0; i < sizeof(l12_sizes)/sizeof(l12_sizes[0]) && 
                     sizes->single_core.count < MAX_SIZES; ++i) {
                    if (l12_sizes[i] <= detected_size) {
                        sizes->single_core.seq[sizes->single_core.count++] = l12_sizes[i];
                    }
                }
            }
        } else {
            /* Multi-core sizes */
            if (level == levels - 1) {  /* L3 */
                size_t l3_sizes[] = {
                    per_core_size / 2,      /* 0.5 * per_core_size */
                    per_core_size,          /* per_core_size */
                    per_core_size * 2,      /* 2 * detected_size */
                    per_core_size * 3,      /* 3 * detected_size */
                    per_core_size * 4       /* 4 * detected_size */
                };
                
                for (size_t i = 0; i < sizeof(l3_sizes)/sizeof(l3_sizes[0]) && 
                     sizes->multi_core.count < MAX_SIZES; ++i) {
                    sizes->multi_core.seq[sizes->multi_core.count++] = l3_sizes[i];
                }
            } else {  /* L1 & L2 */
                size_t l12_sizes[] = {
                    per_core_size / 2,      /* 0.5 * per_core_size */
                    per_core_size           /* per_core_size */
                };
                
                for (size_t i = 0; i < sizeof(l12_sizes)/sizeof(l12_sizes[0]) && 
                     sizes->multi_core.count < MAX_SIZES; ++i) {
                    if (l12_sizes[i] <= detected_size) {
                        sizes->multi_core.seq[sizes->multi_core.count++] = l12_sizes[i];
                    }
                }
            }
        }
    }
    
    /* Remove duplicates and sort */
    if (is_single_core) {
        deduplicate_and_sort(&sizes->single_core);
    } else {
        deduplicate_and_sort(&sizes->multi_core);
    }
    
    return 0;
}

/* ------------------------- pointer‑chasing buffer ------------------------- */

/* allocate and shuffle an array of ptr sized entries to form a random chain */
static void **build_chain(size_t bytes)
{
    /* align to cache line and huge pages if possible */
    size_t elems = bytes / sizeof(void *);
    size_t alloc_bytes = ((bytes + (1 << 21) - 1) / (1 << 21)) * (1 << 21); /* round 2M */

    void **buf = mmap(NULL, alloc_bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (buf == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    /* simple Fisher‑Yates shuffle of indices */
    size_t *idx = malloc(elems * sizeof(size_t));
    if (!idx) {
        munmap(buf, alloc_bytes);
        return NULL;
    }
    for (size_t i = 0; i < elems; ++i)
        idx[i] = i;

    for (size_t i = elems - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        size_t tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
    }

    /* Build random chain instead of ring */
    for (size_t i = 0; i < elems - 1; ++i) {
        buf[idx[i]] = &buf[idx[i + 1]];
    }
    /* Last element points to a random location to break predictability */
    buf[idx[elems - 1]] = &buf[idx[rand() % elems]];
    
    free(idx);
    return buf;
}

/* ------------------------- S3 single‑core measurement ------------------------- */

/* Helper function to get number of cores per socket */
static int get_cores_per_socket(void)
{
    /* Get number of CPU sockets by reading unique physical IDs */
    int nsockets = 0;
    int *physical_ids = calloc(MAX_THREADS, sizeof(int));
    if (!physical_ids)
        return 1; /* fallback to 1 */

    /* Read physical package ID for each logical CPU */
    for (int i = 0; i < MAX_THREADS; i++) {
        char path[256];
        snprintf(path, sizeof(path), 
                "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
        if (access(path, R_OK) != 0)
            break;
        
        int phys_id = read_long(path);
        if (phys_id >= 0) {
            /* Check if we've seen this physical ID before */
            bool found = false;
            for (int j = 0; j < nsockets; j++) {
                if (physical_ids[j] == phys_id) {
                    found = true;
                    break;
                }
            }
            if (!found)
                physical_ids[nsockets++] = phys_id;
        }
    }
    free(physical_ids);

    /* Calculate cores per socket */
    int total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int cores_per_socket = total_cpus / (nsockets > 0 ? nsockets : 1);
    
    return cores_per_socket;
}

static double measure_latency(void **chain, int ntest, int nwarm)
{
    struct timespec t0, t1;
    void **p = chain;
    volatile uint64_t fake_ns;
    uint64_t total_ns = 0;
    int total_measurements = ntest + nwarm;

    // Run ntest + nwarm measurements
    for (int m = 0; m < total_measurements; ++m) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
        fake_ns = t0.tv_nsec >= 0? 0: 1;
        p = (uint64_t)p > fake_ns ? p : (void **)fake_ns;
        
        REPEAT_POINTER_CHASE_10K(p);
        
        t1.tv_nsec = (uint64_t)p > fake_ns ? t1.tv_nsec : 0;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
        fake_ns = t1.tv_nsec >= 0? 0: 1;
        p = (uint64_t)p > fake_ns ? p : (void **)fake_ns;
        
        uint64_t ns = nsec_diff(t0, t1);
        
        // Only accumulate the last ntest measurements
        if (m >= nwarm) {
            total_ns += ns;
        }
    }

    return (double)total_ns / (ntest * 10000);
}

static int run_single_core(size_t bytes, double *lat_ns)
{
    /* Pin to second core of CPU0 if socket has more than 1 core */
    int cores_per_socket = get_cores_per_socket();
    if (cores_per_socket > 1) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(1, &set); /* Pin to CPU 1 (second core of CPU0) */
        if (sched_setaffinity(0, sizeof(set), &set) != 0) {
            perror("sched_setaffinity");
            return -1;
        }
    }

    void **chain = build_chain(bytes);
    if (!chain)
        return -1;

    *lat_ns = measure_latency(chain, 10, 2);
    munmap(chain, bytes);
    return 0;
}

/* ------------------------- S4 multi‑core measurement ------------------------- */

typedef struct {
    size_t bytes;
    double lat_ns;
    int cpu_id;
} thread_arg_t;

static void *thread_worker(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(a->cpu_id, &set);
    sched_setaffinity(0, sizeof(set), &set);

    void **chain = build_chain(a->bytes);
    if (!chain) {
        a->lat_ns = -1.0;
        pthread_exit(NULL);
    }
    a->lat_ns = measure_latency(chain, 10, 2);
    munmap(chain, a->bytes);
    return NULL;
}

static int run_all_cores(size_t bytes, double *lat_ns)
{
    /* Get number of cores per socket using the helper function */
    int cores_per_socket = get_cores_per_socket();
    int total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int nthreads = cores_per_socket;
    if (nthreads > MAX_THREADS)
        nthreads = MAX_THREADS;

    pthread_t th[MAX_THREADS];
    thread_arg_t args[MAX_THREADS];

    for (int i = 0; i < nthreads; ++i) {
        args[i].bytes = bytes;
        args[i].cpu_id = i;
        if (pthread_create(&th[i], NULL, thread_worker, &args[i]) != 0) {
            return -1;
        }
    }
    
    double sum = 0.0;
    int valid_results = 0;
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(th[i], NULL);
        if (args[i].lat_ns >= 0) {
            sum += args[i].lat_ns;
            valid_results++;
        }
    }
    
    if (valid_results == 0) {
        return -1;
    }
    
    *lat_ns = sum / valid_results;
    return 0;
}

/* ------------------------- Public API ------------------------- */

int mem_pchase_run(size_t *ptsize, int nsize, int ntest, int nwarm, uint64_t *pres)
{
    if (!pres) {
        return -1;
    }
    
    /* Initialize random seed */
    srand(time(NULL));
    
    /* Auto-detect mode */
    if (!ptsize || nsize <= 0) {
        cache_info_t caches[MAX_LEVELS] = {0};
        int levels = detect_cache_hierarchy(caches);
        if (levels <= 0) {
            return -1;
        }
        
        test_sizes_t sizes = {0};
        if (build_test_sizes(caches, levels, &sizes, true) != 0) {
            return -1;
        }
        
        /* Use single-core sizes for auto-detect */
        nsize = sizes.single_core.count;
        ptsize = sizes.single_core.seq;
        
        /* Cleanup will be done by caller */
    }
    
    /* Run tests for each size */
    for (int i = 0; i < nsize; ++i) {
        double lat_ns;
        if (run_single_core(ptsize[i], &lat_ns) != 0) {
            return -1;
        }
        pres[i] = (uint64_t)(lat_ns * 1000); /* Convert to picoseconds for precision */
    }
    
    return 0;
}

int mem_pchase_run_allcore(size_t *ptsize, int nsize, int ntest, int nwarm, uint64_t *pres)
{
    if (!pres) {
        return -1;
    }
    
    /* Initialize random seed */
    srand(time(NULL));
    
    /* Auto-detect mode */
    if (!ptsize || nsize <= 0) {
        cache_info_t caches[MAX_LEVELS] = {0};
        int levels = detect_cache_hierarchy(caches);
        if (levels <= 0) {
            return -1;
        }
        
        test_sizes_t sizes = {0};
        if (build_test_sizes(caches, levels, &sizes, false) != 0) {
            return -1;
        }
        
        /* Use multi-core sizes for auto-detect */
        nsize = sizes.multi_core.count;
        ptsize = sizes.multi_core.seq;
        
        /* Cleanup will be done by caller */
    }
    
    /* Run tests for each size */
    for (int i = 0; i < nsize; ++i) {
        double lat_ns;
        if (run_all_cores(ptsize[i], &lat_ns) != 0) {
            return -1;
        }
        pres[i] = (uint64_t)(lat_ns * 1000); /* Convert to picoseconds for precision */
    }
    
    return 0;
}

int mem_pchase_get_cache_info(cache_info_t *caches, int *levels)
{
    if (!caches || !levels) {
        return -1;
    }
    
    *levels = detect_cache_hierarchy(caches);
    return (*levels > 0) ? 0 : -1;
}

int mem_pchase_build_sizes(size_t **sizes, int *count, bool single_core)
{
    if (!sizes || !count) {
        return -1;
    }
    
    cache_info_t caches[MAX_LEVELS] = {0};
    int levels = detect_cache_hierarchy(caches);
    if (levels <= 0) {
        return -1;
    }
    
    test_sizes_t test_sizes = {0};
    if (build_test_sizes(caches, levels, &test_sizes, single_core) != 0) {
        return -1;
    }
    
    if (single_core) {
        *sizes = test_sizes.single_core.seq;
        *count = test_sizes.single_core.count;
    } else {
        *sizes = test_sizes.multi_core.seq;
        *count = test_sizes.multi_core.count;
    }
    
    return 0;
} 