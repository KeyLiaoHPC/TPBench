/*
 * test_merge_hybrid.c
 * Pack C2.3: Hybrid multi-process + multi-thread merge test.
 *
 * Fork 2 processes, each spawns 2 threads running STREAM triad,
 * per-process thread merge, then cross-process merge in parent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"
#include "corelib/strftime.h"

#define N_PROCS            2
#define N_THREADS_PER_PROC 2
#define ARRAY_N            512

typedef struct {
    int thread_index;
    unsigned char tbatch_id[20];
    unsigned char kernel_id[20];
    unsigned char task_id[20];
    task_entry_t entry;          /* output: filled by thread */
    const char *workspace;
} thread_arg_t;

static char g_workspace[512];
static int g_pass = 0;
static int g_fail = 0;

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

/* Local Function Prototypes */
static int is_all_ff(const unsigned char id[20]);
static uint32_t get_cur_tid(void);
static void *thread_worker(void *arg);
static void run_child(int proc_index, int write_fd,
                      const unsigned char tbatch_id[20],
                      const unsigned char kernel_id[20],
                      const char *workspace);
static int find_header(const task_attr_t *attr,
                       const char *name,
                       tpb_meta_header_t *out);
static int test_merge_hybrid(void);

static int
is_all_ff(const unsigned char id[20])
{
    for (int i = 0; i < 20; i++) {
        if (id[i] != 0xFF) return 0;
    }
    return 1;
}

static uint32_t
get_cur_tid(void)
{
#ifdef __linux__
    return (uint32_t)syscall(SYS_gettid);
#else
    return (uint32_t)getpid();
#endif
}

/*
 * Thread worker: run STREAM triad on ARRAY_N doubles,
 * write .tpbr record; populate entry for main thread.
 */
static void *
thread_worker(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    double a[ARRAY_N], b[ARRAY_N], c[ARRAY_N];
    double s = 0.42;
    int i, err;

    for (i = 0; i < ARRAY_N; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    struct timespec ts0, ts1;
    clock_gettime(CLOCK_BOOTTIME, &ts0);
    for (int rep = 0; rep < 10; rep++) {
        for (i = 0; i < ARRAY_N; i++)
            a[i] = b[i] + s * c[i];
    }
    clock_gettime(CLOCK_BOOTTIME, &ts1);

    uint64_t btime_ns = (uint64_t)ts0.tv_sec * 1000000000ULL
                       + (uint64_t)ts0.tv_nsec;
    uint64_t end_ns = (uint64_t)ts1.tv_sec * 1000000000ULL
                    + (uint64_t)ts1.tv_nsec;
    uint64_t duration_ns = end_ns - btime_ns;

    tpb_datetime_t dt;
    tpb_dtbits_t utc_bits;
    tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    tpb_ts_datetime_to_bits(&dt, 0, &utc_bits);

    char hostname[64];
    memset(hostname, 0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    struct passwd *pw = getpwuid(geteuid());
    const char *username = pw ? pw->pw_name : "unknown";

    uint32_t pid = (uint32_t)getpid();
    uint32_t tid = get_cur_tid();

    unsigned char task_id[20];
    tpb_raf_gen_task_id(utc_bits, btime_ns,
                          hostname, username,
                          ta->tbatch_id, ta->kernel_id,
                          (uint32_t)ta->thread_index,
                          pid, tid, task_id);

    /* Build task record with one output header */
    task_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, task_id, 20);
    memcpy(attr.tbatch_id, ta->tbatch_id, 20);
    memcpy(attr.kernel_id, ta->kernel_id, 20);
    attr.utc_bits = utc_bits;
    attr.btime = btime_ns;
    attr.duration = duration_ns;
    attr.exit_code = 0;
    attr.handle_index = (uint32_t)ta->thread_index;
    attr.pid = pid;
    attr.tid = tid;
    attr.ninput = 0;
    attr.noutput = 1;

    tpb_meta_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.block_size = TPB_RAF_HDR_FIXED_SIZE;
    hdr.ndim = 1;
    hdr.dimsizes[0] = 1;
    hdr.type_bits = (uint32_t)(TPB_DOUBLE_T
                                & TPB_PARM_TYPE_MASK);
    hdr.data_size = sizeof(double);
    strncpy(hdr.name, "triad_time",
            sizeof(hdr.name) - 1);

    attr.nheader = 1;
    attr.headers = &hdr;

    double triad_time = (double)duration_ns;

    err = tpb_raf_record_write_task(ta->workspace,
                                      &attr, &triad_time,
                                      sizeof(double));
    if (err) {
        fprintf(stderr, "  thread %d: record_write_task "
                "failed: %d\n", ta->thread_index, err);
    }

    memcpy(ta->task_id, task_id, 20);

    /* Populate entry for the child's main thread to write */
    memset(&ta->entry, 0, sizeof(ta->entry));
    memcpy(ta->entry.task_record_id, task_id, 20);
    memcpy(ta->entry.tbatch_id, ta->tbatch_id, 20);
    memcpy(ta->entry.kernel_id, ta->kernel_id, 20);
    ta->entry.utc_bits     = utc_bits;
    ta->entry.duration     = duration_ns;
    ta->entry.exit_code    = 0;
    ta->entry.handle_index = (uint32_t)ta->thread_index;

    return NULL;
}

/*
 * Child process: spawn N_THREADS_PER_PROC threads,
 * thread-merge, write merged ID to pipe.
 */
static void
run_child(int proc_index, int write_fd,
          const unsigned char tbatch_id[20],
          const unsigned char kernel_id[20],
          const char *workspace)
{
    pthread_t threads[N_THREADS_PER_PROC];
    thread_arg_t targs[N_THREADS_PER_PROC];
    int i, err;

    for (i = 0; i < N_THREADS_PER_PROC; i++) {
        targs[i].thread_index =
            proc_index * N_THREADS_PER_PROC + i;
        memcpy(targs[i].tbatch_id, tbatch_id, 20);
        memcpy(targs[i].kernel_id, kernel_id, 20);
        targs[i].workspace = workspace;
        memset(targs[i].task_id, 0, 20);
    }

    for (i = 0; i < N_THREADS_PER_PROC; i++) {
        pthread_create(&threads[i], NULL,
                       thread_worker, &targs[i]);
    }
    for (i = 0; i < N_THREADS_PER_PROC; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Child main thread writes .tpbe entries (single-writer) */
    for (i = 0; i < N_THREADS_PER_PROC; i++) {
        err = tpb_raf_entry_append_task(workspace,
                                          &targs[i].entry);
        if (err) {
            fprintf(stderr, "  child %d: entry_append "
                    "failed: %d\n", proc_index, err);
        }
    }

    unsigned char task_ids[N_THREADS_PER_PROC][20];
    for (i = 0; i < N_THREADS_PER_PROC; i++)
        memcpy(task_ids[i], targs[i].task_id, 20);

    unsigned char child_merged_id[20];
    err = tpb_k_merge_record_thread(
        task_ids, N_THREADS_PER_PROC, child_merged_id);
    if (err) {
        fprintf(stderr,
                "  child %d: merge_record_thread "
                "failed: %d\n", proc_index, err);
        memset(child_merged_id, 0, 20);
    }

    write(write_fd, child_merged_id, 20);
    close(write_fd);
}

static int
find_header(const task_attr_t *attr, const char *name,
            tpb_meta_header_t *out)
{
    for (uint32_t i = 0; i < attr->nheader; i++) {
        if (strcmp(attr->headers[i].name, name) == 0) {
            if (out) *out = attr->headers[i];
            return 1;
        }
    }
    return 0;
}

static int
test_merge_hybrid(void)
{
    int err, i;
    char hex[41];

    /* 1. Create temp workspace */
    snprintf(g_workspace, sizeof(g_workspace),
             "/tmp/tpb_merge_hybrid_%d", (int)getpid());
    mkdir(g_workspace, 0755);

    err = tpb_raf_init_workspace(g_workspace);
    CHECK("init_workspace", err == 0);
    if (err) return 1;

    /* 2. Set env */
    setenv("TPB_WORKSPACE", g_workspace, 1);

    /* 3. Init corelib */
    err = tpb_k_corelib_init(NULL);
    CHECK("corelib_init", err == 0);
    if (err) return 1;

    /* 4. Generate shared TBatchID and KernelID */
    tpb_datetime_t dt;
    tpb_dtbits_t utc_bits;
    tpb_btime_t btime;
    tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    tpb_ts_datetime_to_bits(&dt, 0, &utc_bits);
    tpb_ts_get_btime(&btime);
    uint64_t btime_ns = btime.sec * 1000000000ULL
                      + btime.nsec;

    char hostname[64];
    memset(hostname, 0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    struct passwd *pw = getpwuid(geteuid());
    const char *username =
        pw ? pw->pw_name : "unknown";

    unsigned char tbatch_id[20];
    tpb_raf_gen_tbatch_id(utc_bits, btime_ns,
                            hostname, username,
                            (uint32_t)getpid(),
                            tbatch_id);

    unsigned char so_sha1[20], bin_sha1[20];
    memset(so_sha1, 0xAA, 20);
    memset(bin_sha1, 0xBB, 20);
    unsigned char kernel_id[20];
    tpb_raf_gen_kernel_id("stream",
                            so_sha1, bin_sha1,
                            kernel_id);

    /* Write kernel record + entry */
    kernel_attr_t kattr;
    memset(&kattr, 0, sizeof(kattr));
    memcpy(kattr.kernel_id, kernel_id, 20);
    strncpy(kattr.kernel_name, "stream",
            sizeof(kattr.kernel_name) - 1);
    kattr.kctrl = (uint32_t)TPB_KTYPE_PLI;

    err = tpb_raf_record_write_kernel(g_workspace,
                                        &kattr,
                                        NULL, 0);
    CHECK("write kernel record", err == 0);

    kernel_entry_t kentry;
    memset(&kentry, 0, sizeof(kentry));
    memcpy(kentry.kernel_id, kernel_id, 20);
    strncpy(kentry.kernel_name, "stream",
            sizeof(kentry.kernel_name) - 1);
    kentry.kctrl = (uint32_t)TPB_KTYPE_PLI;

    err = tpb_raf_entry_append_kernel(g_workspace,
                                        &kentry);
    CHECK("append kernel entry", err == 0);

    /* Write TBatch record + entry */
    tbatch_attr_t battr;
    memset(&battr, 0, sizeof(battr));
    memcpy(battr.tbatch_id, tbatch_id, 20);
    battr.utc_bits = utc_bits;
    battr.btime = btime_ns;
    battr.nkernel = 1;
    battr.ntask = N_PROCS * N_THREADS_PER_PROC;
    strncpy(battr.hostname, hostname,
            sizeof(battr.hostname) - 1);
    strncpy(battr.username, username,
            sizeof(battr.username) - 1);
    battr.front_pid = (uint32_t)getpid();

    err = tpb_raf_record_write_tbatch(g_workspace,
                                        &battr,
                                        NULL, 0);
    CHECK("write tbatch record", err == 0);

    tbatch_entry_t bentry;
    memset(&bentry, 0, sizeof(bentry));
    memcpy(bentry.tbatch_id, tbatch_id, 20);
    bentry.start_utc_bits = utc_bits;
    bentry.nkernel = 1;
    bentry.ntask = N_PROCS * N_THREADS_PER_PROC;
    strncpy(bentry.hostname, hostname,
            sizeof(bentry.hostname) - 1);

    err = tpb_raf_entry_append_tbatch(g_workspace,
                                        &bentry);
    CHECK("append tbatch entry", err == 0);

    /* 5. Fork 2 child processes */
    int pipes[N_PROCS][2];
    pid_t child_pids[N_PROCS];
    unsigned char proc_merged_ids[N_PROCS][20];

    /* Run children sequentially: each child writes .tpbe
     * entries + thread-merge (which also touches .tpbe),
     * so only one writer at a time. */
    for (i = 0; i < N_PROCS; i++) {
        if (pipe(pipes[i]) != 0) {
            perror("pipe");
            return 1;
        }
        child_pids[i] = fork();
        if (child_pids[i] < 0) {
            perror("fork");
            return 1;
        }
        if (child_pids[i] == 0) {
            close(pipes[i][0]);
            run_child(i, pipes[i][1],
                      tbatch_id, kernel_id,
                      g_workspace);
            _exit(0);
        }
        close(pipes[i][1]);

        int status;
        waitpid(child_pids[i], &status, 0);
        CHECK("child exited ok",
              WIFEXITED(status)
              && WEXITSTATUS(status) == 0);

        ssize_t rd = read(pipes[i][0],
                          proc_merged_ids[i], 20);
        CHECK("read merged id", rd == 20);
        close(pipes[i][0]);

        tpb_raf_id_to_hex(proc_merged_ids[i], hex);
        printf("  proc %d merged: %s\n", i, hex);
    }

    /* 7. Process-merge the 2 intermediate records */
    unsigned char final_merged_id[20];
    err = tpb_k_merge_record_process(proc_merged_ids,
                                     N_PROCS,
                                     final_merged_id);
    CHECK("merge_record_process", err == 0);

    tpb_raf_id_to_hex(final_merged_id, hex);
    printf("  final merged: %s\n", hex);

    /* 8. Verify final merged record */
    if (err == 0) {
        task_attr_t final_attr;
        void *final_data = NULL;
        uint64_t final_datasize = 0;
        memset(&final_attr, 0, sizeof(final_attr));

        err = tpb_raf_record_read_task(
            g_workspace, final_merged_id,
            &final_attr, &final_data,
            &final_datasize);
        CHECK("read final record", err == 0);

        if (err == 0) {
            /* 8a: inherit_from is all 0xFF */
            CHECK("inherit_from all 0xFF",
                  is_all_ff(final_attr.inherit_from));

            /* 8b: SourceTaskIDs dimsizes[0] = 2 */
            tpb_meta_header_t hdr;
            int found;

            found = find_header(&final_attr,
                                "SourceTaskIDs", &hdr);
            CHECK("has SourceTaskIDs", found);
            if (found) {
                CHECK_INT("SourceTaskIDs dim",
                          N_PROCS,
                          (int)hdr.dimsizes[0]);
            }

            /* 8c: ThreadIDs exists, dimsizes[0] = 2 */
            found = find_header(&final_attr,
                                "ThreadIDs", &hdr);
            CHECK("has ThreadIDs", found);
            if (found) {
                CHECK_INT("ThreadIDs dim",
                          N_PROCS,
                          (int)hdr.dimsizes[0]);
            }

            /* 8d: ProcessIDs dimsizes[0] = 2 */
            found = find_header(&final_attr,
                                "ProcessIDs", &hdr);
            CHECK("has ProcessIDs", found);
            if (found) {
                CHECK_INT("ProcessIDs dim",
                          N_PROCS,
                          (int)hdr.dimsizes[0]);
            }

            /* 8e: Hosts dimsizes[0] = 2 */
            found = find_header(&final_attr,
                                "Hosts", &hdr);
            CHECK("has Hosts", found);
            if (found) {
                CHECK_INT("Hosts dim",
                          N_PROCS,
                          (int)hdr.dimsizes[0]);
            }

            /* 8f: duration > 0 */
            CHECK("duration > 0",
                  final_attr.duration > 0);

            /* 8f2: verify triad_time data values are
               valid (not garbage from misaligned blob) */
            if (final_data && final_datasize > 0) {
                const uint8_t *dptr =
                    (const uint8_t *)final_data;
                uint64_t doff = 0;
                int tt_found = 0;
                for (uint32_t hi = 0;
                     hi < final_attr.nheader; hi++) {
                    const tpb_meta_header_t *fh =
                        &final_attr.headers[hi];
                    if (strcmp(fh->name,
                              "triad_time") == 0
                        && fh->data_size == sizeof(double)
                        && doff + sizeof(double)
                           <= final_datasize) {
                        double val;
                        memcpy(&val, dptr + doff,
                               sizeof(double));
                        char msg[64];
                        snprintf(msg, sizeof(msg),
                            "triad_time[%d] > 0",
                            tt_found);
                        CHECK(msg, val > 0.0);
                        snprintf(msg, sizeof(msg),
                            "triad_time[%d] < 1e15",
                            tt_found);
                        CHECK(msg, val < 1.0e15);
                        tt_found++;
                    }
                    doff += fh->data_size;
                }
                CHECK("triad_time count >= 2",
                      tt_found >= 2);
            }

            tpb_raf_free_headers(final_attr.headers,
                                   final_attr.nheader);
            free(final_data);
        }

        /* 8g: Total task entries = 7 */
        task_entry_t *all_entries = NULL;
        int entry_count = 0;
        err = tpb_raf_entry_list_task(g_workspace,
                                        &all_entries,
                                        &entry_count);
        CHECK("list_task ok", err == 0);
        CHECK_INT("total task entries", 7, entry_count);

        /* 8h: Source (per-process merged) entries
         *     should have derive_to = final_merged_id */
        if (err == 0 && all_entries) {
            for (i = 0; i < N_PROCS; i++) {
                int j, found_src = 0;
                for (j = 0; j < entry_count; j++) {
                    if (memcmp(
                            all_entries[j].task_record_id,
                            proc_merged_ids[i],
                            20) == 0) {
                        found_src = 1;
                        char msg[64];
                        snprintf(msg, sizeof(msg),
                                 "src %d derive_to", i);
                        CHECK(msg,
                              memcmp(
                                  all_entries[j].derive_to,
                                  final_merged_id,
                                  20) == 0);
                        break;
                    }
                }
                if (!found_src) {
                    char msg[64];
                    snprintf(msg, sizeof(msg),
                             "find src entry %d", i);
                    CHECK(msg, 0);
                }
            }
        }

        free(all_entries);
    }

    /* 9. Cleanup */
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_workspace);
    system(cmd);

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
        {"C2.3", "merge_hybrid", test_merge_hybrid},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int pass = 0, fail = 0;

    printf("Running test pack C2.3 (%d cases)\n", n);
    printf("------------------------------------------"
           "----\n");

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

    printf("------------------------------------------"
           "----\n");
    printf("Pack C2.3: %d passed, %d failed "
           "(%d checks: %d ok, %d fail)\n",
           pass, fail, g_pass + g_fail,
           g_pass, g_fail);
    return fail;
}
