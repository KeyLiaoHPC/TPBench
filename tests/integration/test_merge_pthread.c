/*
 * test_merge_pthread.c
 * Test pack C2.1: Thread-level task record merging via pthreads.
 *
 * Spawns 4 pthreads, each running a STREAM triad and writing
 * its own task record + entry. The main thread merges them via
 * tpb_k_merge_record_thread and verifies the merged record.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <limits.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"
#include "corelib/strftime.h"

/* Test harness */
static char g_test_dir[512];
static int  g_pass = 0;
static int  g_fail = 0;

#define CHECK(msg, cond) do {                                  \
    if (cond) { g_pass++; }                                    \
    else {                                                     \
        g_fail++;                                              \
        fprintf(stderr, "  FAIL [%s]: %s\n", (msg), #cond);   \
    }                                                          \
} while (0)

#define CHECK_INT(msg, expected, actual) do {                  \
    if ((expected) == (actual)) { g_pass++; }                  \
    else {                                                     \
        g_fail++;                                              \
        fprintf(stderr, "  FAIL [%s]: expected %d, got %d\n", \
                (msg), (int)(expected), (int)(actual));        \
    }                                                          \
} while (0)

#define N_THREADS 4
#define ARRAY_N   512

typedef struct {
    int            thread_index;
    unsigned char  tbatch_id[20];
    unsigned char  kernel_id[20];
    unsigned char  task_id[20];   /* output */
    task_entry_t   entry;         /* output: filled by thread */
    const char    *workspace;
} thread_arg_t;

/* Local Function Prototypes */
static int  is_zero_id(const unsigned char id[20]);
static int  is_ff_id(const unsigned char id[20]);
static void setup_test_dir(void);
static void cleanup_test_dir(void);
static void *thread_worker(void *arg);
static int  test_merge_pthread(void);

static int
is_zero_id(const unsigned char id[20])
{
    for (int i = 0; i < 20; i++) {
        if (id[i] != 0) return 0;
    }
    return 1;
}

static int
is_ff_id(const unsigned char id[20])
{
    for (int i = 0; i < 20; i++) {
        if (id[i] != 0xFF) return 0;
    }
    return 1;
}

static void
setup_test_dir(void)
{
    snprintf(g_test_dir, sizeof(g_test_dir),
             "/tmp/tpb_merge_pthread_%d", (int)getpid());
    mkdir(g_test_dir, 0755);
}

static void
cleanup_test_dir(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    system(cmd);
}

static void *
thread_worker(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    struct timespec ts_start, ts_end;
    uint64_t btime_start, btime_end;
    double *a, *b, *c;
    double bw_value;
    int i, err;

    /* Boottime before computation */
    clock_gettime(CLOCK_BOOTTIME, &ts_start);
    btime_start = (uint64_t)ts_start.tv_sec * 1000000000ULL
                + (uint64_t)ts_start.tv_nsec;

    /* Allocate and run STREAM triad */
    a = (double *)malloc(ARRAY_N * sizeof(double));
    b = (double *)malloc(ARRAY_N * sizeof(double));
    c = (double *)malloc(ARRAY_N * sizeof(double));
    if (!a || !b || !c) {
        free(a); free(b); free(c);
        return NULL;
    }

    for (i = 0; i < ARRAY_N; i++) {
        b[i] = 1.0;
        c[i] = 2.0;
    }
    for (i = 0; i < ARRAY_N; i++) {
        a[i] = b[i] + 3.0 * c[i];
    }

    /* Boottime after computation */
    clock_gettime(CLOCK_BOOTTIME, &ts_end);
    btime_end = (uint64_t)ts_end.tv_sec * 1000000000ULL
              + (uint64_t)ts_end.tv_nsec;

    /* Compute a fake bandwidth value */
    bw_value = (double)(3 * ARRAY_N * sizeof(double))
             / ((double)(btime_end - btime_start) * 1e-9);

    /* Get datetime and encode to bits */
    tpb_datetime_t dt;
    tpb_dtbits_t utc_bits;
    tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    tpb_ts_datetime_to_bits(&dt, 0, &utc_bits);

    /* Hostname and username */
    char hostname[64];
    memset(hostname, 0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    struct passwd *pw = getpwuid(geteuid());
    const char *username = pw ? pw->pw_name : "unknown";

    /* Thread ID */
    uint32_t cur_tid;
#ifdef __linux__
    cur_tid = (uint32_t)syscall(SYS_gettid);
#else
    cur_tid = (uint32_t)getpid();
#endif

    /* Generate task record ID */
    unsigned char task_id[20];
    err = tpb_raf_gen_task_id(
        utc_bits, btime_start,
        hostname, username,
        ta->tbatch_id, ta->kernel_id,
        (uint32_t)ta->thread_index,
        (uint32_t)getpid(), cur_tid,
        task_id);
    if (err) {
        free(a); free(b); free(c);
        return NULL;
    }

    /* Build task_attr_t */
    task_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, task_id, 20);
    memset(attr.dup_to, 0, 20);
    memset(attr.dup_from, 0, 20);
    memcpy(attr.tbatch_id, ta->tbatch_id, 20);
    memcpy(attr.kernel_id, ta->kernel_id, 20);
    attr.utc_bits     = utc_bits;
    attr.btime        = btime_start;
    attr.duration     = btime_end - btime_start;
    attr.exit_code    = 0;
    attr.handle_index = (uint32_t)ta->thread_index;
    attr.pid          = (uint32_t)getpid();
    attr.tid          = cur_tid;
    attr.ninput       = 0;
    attr.noutput      = 1;
    attr.nheader      = 1;

    /* Build output header: triad_bw */
    tpb_meta_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.block_size = TPB_RAF_HDR_FIXED_SIZE;
    hdr.ndim       = 1;
    hdr.dimsizes[0] = 1;
    hdr.type_bits  = (uint32_t)(TPB_DOUBLE_T
                                & TPB_PARM_TYPE_MASK);
    hdr.data_size  = 8;
    strncpy(hdr.name, "triad_bw",
            sizeof(hdr.name) - 1);
    attr.headers = &hdr;

    /* Write .tpbr */
    err = tpb_raf_record_write_task(ta->workspace,
                                      &attr,
                                      &bw_value, 8);
    if (err) {
        fprintf(stderr,
                "  thread %d: record_write_task failed %d\n",
                ta->thread_index, err);
    }

    memcpy(ta->task_id, task_id, 20);

    /* Populate entry for main thread to write to .tpbe */
    memset(&ta->entry, 0, sizeof(ta->entry));
    memcpy(ta->entry.task_record_id, task_id, 20);
    memcpy(ta->entry.tbatch_id, ta->tbatch_id, 20);
    memcpy(ta->entry.kernel_id, ta->kernel_id, 20);
    ta->entry.utc_bits     = utc_bits;
    ta->entry.duration     = btime_end - btime_start;
    ta->entry.exit_code    = 0;
    ta->entry.handle_index = (uint32_t)ta->thread_index;

    free(a);
    free(b);
    free(c);
    return NULL;
}

static int
test_merge_pthread(void)
{
    int err, i;
    pthread_t threads[N_THREADS];
    thread_arg_t targs[N_THREADS];
    unsigned char tbatch_id[20];
    unsigned char kernel_id[20];
    unsigned char merged_id[20];
    unsigned char dummy_sha1[20];
    char hex[41];

    setup_test_dir();

    /* Initialize workspace */
    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("init_workspace", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    /* Set TPB_WORKSPACE so corelib_init picks it up */
    setenv("TPB_WORKSPACE", g_test_dir, 1);
    err = tpb_k_corelib_init(NULL);
    CHECK("corelib_init", err == 0 || err == TPBE_ILLEGAL_CALL);

    /* Generate a fake TBatchID */
    tpb_datetime_t dt;
    tpb_dtbits_t utc_bits;
    tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    tpb_ts_datetime_to_bits(&dt, 0, &utc_bits);

    struct timespec ts_now;
    clock_gettime(CLOCK_BOOTTIME, &ts_now);
    uint64_t btime_now = (uint64_t)ts_now.tv_sec * 1000000000ULL
                       + (uint64_t)ts_now.tv_nsec;

    char hostname[64];
    memset(hostname, 0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    struct passwd *pw = getpwuid(geteuid());
    const char *username = pw ? pw->pw_name : "unknown";

    err = tpb_raf_gen_tbatch_id(utc_bits, btime_now,
                                  hostname, username,
                                  (uint32_t)getpid(),
                                  tbatch_id);
    CHECK("gen_tbatch_id", err == 0);

    /* Generate a fake KernelID */
    memset(dummy_sha1, 0xAB, 20);
    err = tpb_raf_gen_kernel_id("test_stream",
                                  dummy_sha1, dummy_sha1,
                                  kernel_id);
    CHECK("gen_kernel_id", err == 0);

    /* Spawn threads */
    for (i = 0; i < N_THREADS; i++) {
        targs[i].thread_index = i;
        memcpy(targs[i].tbatch_id, tbatch_id, 20);
        memcpy(targs[i].kernel_id, kernel_id, 20);
        targs[i].workspace = g_test_dir;
        memset(targs[i].task_id, 0, 20);
    }

    for (i = 0; i < N_THREADS; i++) {
        err = pthread_create(&threads[i], NULL,
                             thread_worker, &targs[i]);
        CHECK("pthread_create", err == 0);
    }

    for (i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify each thread produced a non-zero task ID */
    unsigned char task_ids[N_THREADS][20];
    for (i = 0; i < N_THREADS; i++) {
        memcpy(task_ids[i], targs[i].task_id, 20);
        char tag[32];
        snprintf(tag, sizeof(tag), "thread%d task_id", i);
        CHECK(tag, !is_zero_id(task_ids[i]));
    }

    /* Main thread writes all .tpbe entries (single-writer) */
    for (i = 0; i < N_THREADS; i++) {
        err = tpb_raf_entry_append_task(g_test_dir,
                                          &targs[i].entry);
        CHECK("main entry_append", err == 0);
    }

    /* Merge */
    err = tpb_k_merge_record_thread(task_ids, N_THREADS,
                                    merged_id);
    CHECK("merge_record_thread", err == 0);

    tpb_raf_id_to_hex(merged_id, hex);
    printf("  merged_id: %s\n", hex);

    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    /* Read merged task record */
    task_attr_t merged;
    void *merged_data = NULL;
    uint64_t merged_datasize = 0;
    memset(&merged, 0, sizeof(merged));

    err = tpb_raf_record_read_task(g_test_dir,
                                     merged_id,
                                     &merged,
                                     &merged_data,
                                     &merged_datasize);
    CHECK("read_merged", err == 0);

    if (err != 0) {
        free(merged_data);
        tpb_raf_free_headers(merged.headers,
                               merged.nheader);
        cleanup_test_dir();
        return 1;
    }

    /* Verify merged.dup_from is 0xFF (merge sentinel) */
    CHECK("merged.dup_from=0xFF", is_ff_id(merged.dup_from));

    /* Verify merged.dup_to is all 0x00 */
    CHECK("merged.dup_to=0x00", is_zero_id(merged.dup_to));

    /* Verify tbatch_id and kernel_id */
    CHECK("merged.tbatch_id",
          memcmp(merged.tbatch_id, tbatch_id, 20) == 0);
    CHECK("merged.kernel_id",
          memcmp(merged.kernel_id, kernel_id, 20) == 0);

    /* Verify nheader >= 2 (SourceTaskIDs + ThreadIDs) */
    CHECK("merged.nheader >= 2",
          merged.nheader >= 2);

    /* Verify merged.duration > 0 */
    CHECK("merged.duration > 0", merged.duration > 0);

    /* Find SourceTaskIDs header */
    int found_src = 0, found_tid = 0;
    for (i = 0; i < (int)merged.nheader; i++) {
        if (strcmp(merged.headers[i].name,
                   "SourceTaskIDs") == 0) {
            found_src = 1;
            CHECK_INT("SourceTaskIDs.ndim", 1,
                      (int)merged.headers[i].ndim);
            CHECK_INT("SourceTaskIDs.dim[0]", N_THREADS,
                      (int)merged.headers[i].dimsizes[0]);
        }
        if (strcmp(merged.headers[i].name,
                   "ThreadIDs") == 0) {
            found_tid = 1;
            CHECK_INT("ThreadIDs.ndim", 1,
                      (int)merged.headers[i].ndim);
            CHECK_INT("ThreadIDs.dim[0]", N_THREADS,
                      (int)merged.headers[i].dimsizes[0]);
        }
    }
    CHECK("found SourceTaskIDs", found_src);
    CHECK("found ThreadIDs", found_tid);

    free(merged_data);
    tpb_raf_free_headers(merged.headers,
                           merged.nheader);

    /* Read source task entries, verify dup_to = merged_id */
    task_entry_t *entries = NULL;
    int entry_count = 0;
    err = tpb_raf_entry_list_task(g_test_dir,
                                    &entries, &entry_count);
    CHECK("list_task ok", err == 0);
    CHECK_INT("entry_count", N_THREADS + 1, entry_count);

    if (err == 0 && entries) {
        for (i = 0; i < entry_count; i++) {
            int is_merged = (memcmp(entries[i].task_record_id,
                                    merged_id, 20) == 0);
            if (is_merged) continue;

            /* Source entry: dup_to must be merged_id */
            char tag[64];
            tpb_raf_id_to_hex(
                entries[i].task_record_id, hex);
            snprintf(tag, sizeof(tag),
                     "src dup_to %.8s", hex);
            CHECK(tag,
                  memcmp(entries[i].dup_to,
                         merged_id, 20) == 0);
        }
    }

    free(entries);
    cleanup_test_dir();

    return (g_fail > 0) ? 1 : 0;
}

typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;

    test_case_t cases[] = {
        {"C2.1", "merge_pthread", test_merge_pthread},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int pass = 0, fail = 0;

    printf("Running test pack C2 (%d cases)\n", n);
    printf("----------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        if (filter && strcmp(filter, cases[i].id) != 0)
            continue;
        int res = cases[i].func();
        printf("[%s] %-36s %s\n", cases[i].id,
               cases[i].name,
               res == 0 ? "PASS" : "FAIL");
        if (res == 0) pass++;
        else fail++;
    }

    printf("----------------------------------------------\n");
    printf("Pack C2: %d passed, %d failed "
           "(%d checks: %d ok, %d fail)\n",
           pass, fail, g_pass + g_fail, g_pass, g_fail);
    return fail;
}
