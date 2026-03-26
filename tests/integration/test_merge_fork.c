/*
 * test_merge_fork.c
 * Test pack C2.2: process-level task record merging via fork.
 *
 * Forks N_PROCS children, each writing a task record with its own
 * pid.  Parent merges them with tpb_k_merge_record_process and
 * verifies the merged record contains SourceTaskIDs, ThreadIDs,
 * ProcessIDs, and Hosts headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include "include/tpb-public.h"
#include "corelib/raw_db/tpb-rawdb-types.h"
#include "corelib/strftime.h"

#define N_PROCS 4
#define ARRAY_N 512

static char g_workspace[512];
static int g_pass = 0;
static int g_fail = 0;

#define CHECK(msg, cond) do {                                \
    if (cond) { g_pass++; }                                  \
    else {                                                   \
        g_fail++;                                            \
        fprintf(stderr, "  FAIL [%s]: %s\n",                 \
                (msg), #cond);                               \
    }                                                        \
} while (0)

#define CHECK_INT(msg, expected, actual) do {                \
    if ((expected) == (actual)) { g_pass++; }                \
    else {                                                   \
        g_fail++;                                            \
        fprintf(stderr,                                      \
                "  FAIL [%s]: expected %d, got %d\n",        \
                (msg), (int)(expected), (int)(actual));       \
    }                                                        \
} while (0)

/* Local Function Prototypes */
static int is_all_ff(const unsigned char id[20]);
static int is_zero_id(const unsigned char id[20]);
static uint32_t get_cur_tid(void);
static void cleanup_workspace(void);
static int find_header(const task_attr_t *attr,
                       const char *name);
static int test_merge_fork(void);

static int
is_all_ff(const unsigned char id[20])
{
    int i;
    for (i = 0; i < 20; i++) {
        if (id[i] != 0xFF) return 0;
    }
    return 1;
}

static int
is_zero_id(const unsigned char id[20])
{
    int i;
    for (i = 0; i < 20; i++) {
        if (id[i] != 0) return 0;
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

static void
cleanup_workspace(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_workspace);
    system(cmd);
}

static int
find_header(const task_attr_t *attr, const char *name)
{
    uint32_t i;
    for (i = 0; i < attr->nheader; i++) {
        if (strcmp(attr->headers[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

static int
test_merge_fork(void)
{
    int err, i;
    unsigned char tbatch_id[20], kernel_id[20];
    unsigned char task_ids[N_PROCS][20];
    unsigned char merged_id[20];
    int pipes[N_PROCS][2];
    tpb_datetime_t dt;
    tpb_dtbits_t utc_bits;
    char hostname[64];
    const char *username;
    struct passwd *pw;
    struct timespec ts_boot;
    uint64_t btime_base;
    unsigned char dummy_sha[20];

    /* Setup workspace */
    snprintf(g_workspace, sizeof(g_workspace),
             "/tmp/tpb_merge_fork_%d", (int)getpid());
    mkdir(g_workspace, 0755);

    err = tpb_rawdb_init_workspace(g_workspace);
    CHECK("init_workspace", err == 0);
    if (err != 0) {
        cleanup_workspace();
        return 1;
    }

    setenv("TPB_WORKSPACE", g_workspace, 1);
    err = tpb_k_corelib_init(NULL);
    CHECK("corelib_init", err == 0);
    if (err != 0) {
        cleanup_workspace();
        return 1;
    }

    /* Generate shared tbatch_id and kernel_id */
    memset(hostname, 0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    pw = getpwuid(geteuid());
    username = pw ? pw->pw_name : "unknown";

    tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    tpb_ts_datetime_to_bits(&dt, 0, &utc_bits);

    clock_gettime(CLOCK_BOOTTIME, &ts_boot);
    btime_base = (uint64_t)ts_boot.tv_sec * 1000000000ULL
               + (uint64_t)ts_boot.tv_nsec;

    err = tpb_rawdb_gen_tbatch_id(utc_bits, btime_base,
                                   hostname, username,
                                   (uint32_t)getpid(),
                                   tbatch_id);
    CHECK("gen_tbatch_id", err == 0);

    memset(dummy_sha, 0xAB, 20);
    err = tpb_rawdb_gen_kernel_id("test_fork_kernel",
                                   dummy_sha, dummy_sha,
                                   kernel_id);
    CHECK("gen_kernel_id", err == 0);

    /* Create pipes */
    for (i = 0; i < N_PROCS; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            cleanup_workspace();
            return 1;
        }
    }

    /* Fork all children: each writes .tpbr only.
     * Parent writes all .tpbe entries after wait. */
    for (i = 0; i < N_PROCS; i++) {
        pid_t cpid = fork();
        if (cpid < 0) {
            perror("fork");
            cleanup_workspace();
            return 1;
        }

        if (cpid == 0) {
            int ci = i, j;
            struct timespec ts_s, ts_e;
            uint64_t bt_s, bt_e, dur;
            double a[ARRAY_N], b[ARRAY_N], c[ARRAY_N];
            tpb_datetime_t cdt;
            tpb_dtbits_t cutc;
            unsigned char child_id[20];
            task_attr_t tattr;
            task_entry_t tent;

            close(pipes[ci][0]);

            tpb_k_corelib_init(g_workspace);

            clock_gettime(CLOCK_BOOTTIME, &ts_s);
            bt_s = (uint64_t)ts_s.tv_sec * 1000000000ULL
                 + (uint64_t)ts_s.tv_nsec;

            for (j = 0; j < ARRAY_N; j++) {
                b[j] = 1.0 + (double)j;
                c[j] = 2.0 + (double)j;
            }
            for (j = 0; j < ARRAY_N; j++)
                a[j] = b[j] + 3.0 * c[j];
            (void)a[0];

            clock_gettime(CLOCK_BOOTTIME, &ts_e);
            bt_e = (uint64_t)ts_e.tv_sec * 1000000000ULL
                 + (uint64_t)ts_e.tv_nsec;
            dur = bt_e - bt_s;

            tpb_ts_get_datetime(TPBM_TS_UTC, &cdt);
            tpb_ts_datetime_to_bits(&cdt, 0, &cutc);

            err = tpb_rawdb_gen_task_id(
                cutc, bt_s, hostname, username,
                tbatch_id, kernel_id,
                (uint32_t)ci,
                (uint32_t)getpid(),
                get_cur_tid(),
                child_id);
            if (err) _exit(1);

            memset(&tattr, 0, sizeof(tattr));
            memcpy(tattr.task_record_id, child_id, 20);
            memcpy(tattr.tbatch_id, tbatch_id, 20);
            memcpy(tattr.kernel_id, kernel_id, 20);
            tattr.utc_bits      = cutc;
            tattr.btime         = bt_s;
            tattr.duration      = dur;
            tattr.exit_code     = 0;
            tattr.handle_index  = (uint32_t)ci;
            tattr.pid           = (uint32_t)getpid();
            tattr.tid           = get_cur_tid();
            tattr.nheader       = 0;
            tattr.headers       = NULL;

            err = tpb_rawdb_record_write_task(
                g_workspace, &tattr, NULL, 0);
            if (err) _exit(2);

            memset(&tent, 0, sizeof(tent));
            memcpy(tent.task_record_id, child_id, 20);
            memcpy(tent.tbatch_id, tbatch_id, 20);
            memcpy(tent.kernel_id, kernel_id, 20);
            tent.utc_bits      = cutc;
            tent.duration      = dur;
            tent.exit_code     = 0;
            tent.handle_index  = (uint32_t)ci;

            /* Send full entry to parent (parent writes .tpbe) */
            write(pipes[ci][1], &tent, sizeof(tent));
            close(pipes[ci][1]);
            _exit(0);
        }
    }

    /* Parent: close write ends, wait for all, read IDs */
    for (i = 0; i < N_PROCS; i++)
        close(pipes[i][1]);

    {
        int all_ok = 1;
        for (i = 0; i < N_PROCS; i++) {
            int status;
            wait(&status);
            if (!WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {
                all_ok = 0;
            }
        }
        CHECK("all children exited ok", all_ok);
    }

    {
        task_entry_t child_entries[N_PROCS];
        for (i = 0; i < N_PROCS; i++) {
            ssize_t nr = read(pipes[i][0],
                              &child_entries[i],
                              sizeof(task_entry_t));
            close(pipes[i][0]);
            CHECK("pipe read entry",
                  nr == (ssize_t)sizeof(task_entry_t));
            memcpy(task_ids[i],
                   child_entries[i].task_record_id, 20);
        }
        /* Parent writes all .tpbe entries (single-writer) */
        for (i = 0; i < N_PROCS; i++) {
            err = tpb_rawdb_entry_append_task(
                g_workspace, &child_entries[i]);
            CHECK("parent entry_append", err == 0);
        }
    }

    /* Merge via process-level merge */
    err = tpb_k_merge_record_process(
        (const unsigned char (*)[20])task_ids,
        N_PROCS, merged_id);
    CHECK_INT("merge_record_process", 0, err);
    if (err != 0) {
        cleanup_workspace();
        return 1;
    }

    /* Verify merged record */
    {
        task_attr_t merged;
        void *mdata = NULL;
        uint64_t mdatasize = 0;
        int hi;

        memset(&merged, 0, sizeof(merged));
        err = tpb_rawdb_record_read_task(
            g_workspace, merged_id,
            &merged, &mdata, &mdatasize);
        CHECK("read merged record", err == 0);

        if (err == 0) {
            CHECK("dup_from is 0xFF",
                  is_all_ff(merged.dup_from));
            CHECK("tbatch_id matches",
                  memcmp(merged.tbatch_id,
                         tbatch_id, 20) == 0);
            CHECK("kernel_id matches",
                  memcmp(merged.kernel_id,
                         kernel_id, 20) == 0);

            hi = find_header(&merged, "SourceTaskIDs");
            CHECK("has SourceTaskIDs", hi >= 0);
            if (hi >= 0)
                CHECK_INT("SourceTaskIDs dim0",
                          N_PROCS,
                          (int)merged.headers[hi]
                              .dimsizes[0]);

            hi = find_header(&merged, "ThreadIDs");
            CHECK("has ThreadIDs", hi >= 0);
            if (hi >= 0)
                CHECK_INT("ThreadIDs dim0",
                          N_PROCS,
                          (int)merged.headers[hi]
                              .dimsizes[0]);

            hi = find_header(&merged, "ProcessIDs");
            CHECK("has ProcessIDs", hi >= 0);
            if (hi >= 0)
                CHECK_INT("ProcessIDs dim0",
                          N_PROCS,
                          (int)merged.headers[hi]
                              .dimsizes[0]);

            hi = find_header(&merged, "Hosts");
            CHECK("has Hosts", hi >= 0);
            if (hi >= 0)
                CHECK_INT("Hosts dim0",
                          N_PROCS,
                          (int)merged.headers[hi]
                              .dimsizes[0]);

            CHECK("duration > 0", merged.duration > 0);
        }

        /* Verify source entries have dup_to = merged_id */
        {
            task_entry_t *entries = NULL;
            int ecount = 0;

            err = tpb_rawdb_entry_list_task(
                g_workspace, &entries, &ecount);
            CHECK("list_task ok", err == 0);
            CHECK_INT("total entries",
                      N_PROCS + 1, ecount);

            if (err == 0 && entries) {
                int dup_ok = 1;
                for (i = 0; i < N_PROCS; i++) {
                    int found = 0, j;
                    for (j = 0; j < ecount; j++) {
                        if (memcmp(entries[j]
                                       .task_record_id,
                                   task_ids[i],
                                   20) == 0) {
                            found = 1;
                            if (memcmp(entries[j].dup_to,
                                       merged_id,
                                       20) != 0)
                                dup_ok = 0;
                            break;
                        }
                    }
                    if (!found) dup_ok = 0;
                }
                CHECK("source dup_to = merged_id",
                      dup_ok);
                free(entries);
            }
        }

        if (mdata) free(mdata);
        tpb_rawdb_free_headers(merged.headers,
                               merged.nheader);
    }

    cleanup_workspace();
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
        {"C2.2", "merge_fork", test_merge_fork},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int pass = 0, fail = 0;

    printf("Running test pack C2.2 (%d cases)\n", n);
    printf("----------------------------------------------"
           "----\n");

    for (int i = 0; i < n; i++) {
        if (filter &&
            strcmp(filter, cases[i].id) != 0)
            continue;
        int res = cases[i].func();
        printf("[%s] %-36s %s\n",
               cases[i].id, cases[i].name,
               res == 0 ? "PASS" : "FAIL");
        if (res == 0) pass++;
        else fail++;
    }

    printf("----------------------------------------------"
           "----\n");
    printf("Pack C2.2: %d passed, %d failed "
           "(%d checks: %d ok, %d fail)\n",
           pass, fail,
           g_pass + g_fail, g_pass, g_fail);
    return fail;
}
