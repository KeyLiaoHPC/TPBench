/*
 * test_tri_tests_record.c
 * Test pack C1: Integration test for auto-record across 3 invocation modes.
 *
 * Runs:
 *   1) tpbcli r --kernel stream --kargs stream_array_size=32
 *   2) tpbcli run --kernel stream --kargs ntest=100 --kargs-dim stream_array_size=[242144,524288,1048576]
 *   3) tpbcli-pli-launcher lib/libtpbk_stream.so clock_gettime 10 32 0  (direct invoke)
 *
 * Verifies:
 *   - 2 tbatch entries (batch 1: ntask=1, batch 2: ntask=3)
 *   - 5 task entries total
 *   - Tasks from batch 1/2 carry the correct tbatch_id
 *   - Task from direct invocation has all-zero tbatch_id
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"

static char g_test_dir[512];
static char g_bin_dir[512];
static int g_pass = 0;
static int g_fail = 0;

#define CHECK(msg, cond) do {                                  \
    if (cond) { g_pass++; }                                    \
    else {                                                     \
        g_fail++;                                              \
        fprintf(stderr, "  FAIL [%s]: %s\n", (msg), #cond);   \
    }                                                          \
} while (0)

#define CHECK_INT(msg, expected, actual) do {                              \
    if ((expected) == (actual)) { g_pass++; }                              \
    else {                                                                 \
        g_fail++;                                                          \
        fprintf(stderr, "  FAIL [%s]: expected %d, got %d\n",             \
                (msg), (int)(expected), (int)(actual));                    \
    }                                                                      \
} while (0)

static int
is_zero_id(const unsigned char id[20])
{
    for (int i = 0; i < 20; i++) {
        if (id[i] != 0) return 0;
    }
    return 1;
}

static void
setup_test_dir(void)
{
    snprintf(g_test_dir, sizeof(g_test_dir),
             "/tmp/tpb_tri_record_test_%d", (int)getpid());
    mkdir(g_test_dir, 0755);
}

static void
cleanup_test_dir(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    system(cmd);
}

static int
run_cmd(const char *cmd)
{
    printf("  CMD: %s\n", cmd);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "  Command failed with rc=%d\n", rc);
    }
    return rc;
}

static int
write_stream_bench_yaml_missing_metric(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp,
            "benchmark:\n"
            "  name: stream_bench_missing\n"
            "  batch:\n"
            "    - id: b1\n"
            "      kernel: stream\n"
            "      kargs: [[\"stream_array_size\", \"32\"], [\"ntest\", \"10\"]]\n"
            "      v:\n"
            "        - [\"NoSuchMetric\", \"mean\"]\n"
            "  score:\n"
            "    - id: s1\n"
            "      name: Missing\n"
            "      display: 1\n"
            "      modifier: raw\n"
            "      args:\n"
            "        - \"@batch[id=='b1'].v[0]\"\n");
    fclose(fp);
    return 0;
}

static int
write_stream_bench_yaml(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp,
            "benchmark:\n"
            "  name: stream_bench\n"
            "  batch:\n"
            "    - id: b1\n"
            "      kernel: stream\n"
            "      kargs: [[\"stream_array_size\", \"32\"], [\"ntest\", \"10\"]]\n"
            "      v:\n"
            "        - [\"FOM,BANDWIDTH::Triad\", \"mean\"]\n"
            "  score:\n"
            "    - id: s1\n"
            "      name: Triad\n"
            "      display: 1\n"
            "      modifier: raw\n"
            "      args:\n"
            "        - \"@batch[id=='b1'].v[0]\"\n");
    fclose(fp);
    return 0;
}

static int
make_task_batch_readonly(const char *workspace)
{
    char tbatch_dir[640];

    snprintf(tbatch_dir, sizeof(tbatch_dir), "%s/rafdb/task_batch", workspace);
    return chmod(tbatch_dir, 0555);
}

static int
restore_task_batch_writable(const char *workspace)
{
    char tbatch_dir[640];

    snprintf(tbatch_dir, sizeof(tbatch_dir), "%s/rafdb/task_batch", workspace);
    return chmod(tbatch_dir, 0755);
}

static int
test_tri_record(void)
{
    int err;
    char cmd[2048];

    setup_test_dir();

    /* Initialize workspace */
    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("init_workspace", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    /* --- Run 1: tpbcli r --kernel stream --kargs stream_array_size=32 --- */
    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli r --kernel stream "
             "--kargs stream_array_size=32",
             g_bin_dir, g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("run1 exit", err == 0);

    /* --- Run 2: tpbcli run --kernel stream --kargs ntest=100 --kargs-dim ... --- */
    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli run --kernel stream "
             "--kargs ntest=100 "
             "--kargs-dim stream_array_size=[242144,524288,1048576]",
             g_bin_dir, g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("run2 exit", err == 0);

    /* --- Run 3: Direct kernel invocation (no TPB_TBATCH_ID) --- */
    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli-pli-launcher "
             "%s/../lib/libtpbk_stream.so clock_gettime 10 32 0",
             g_bin_dir, g_test_dir, g_bin_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("run3 exit", err == 0);

    /* --- Verify tbatch entries --- */
    tbatch_entry_t *tb_entries = NULL;
    int tb_count = 0;
    err = tpb_raf_entry_list_tbatch(g_test_dir, &tb_entries, &tb_count);
    CHECK("list_tbatch ok", err == 0);
    CHECK_INT("tbatch count", 2, tb_count);

    if (err == 0 && tb_count == 2) {
        CHECK_INT("batch1 ntask", 1, tb_entries[0].ntask);
        CHECK_INT("batch2 ntask", 3, tb_entries[1].ntask);
        CHECK_INT("batch1 type", TPB_BATCH_TYPE_RUN, tb_entries[0].batch_type);
        CHECK_INT("batch2 type", TPB_BATCH_TYPE_RUN, tb_entries[1].batch_type);
        CHECK("batch1 id nonzero", !is_zero_id(tb_entries[0].tbatch_id));
        CHECK("batch2 id nonzero", !is_zero_id(tb_entries[1].tbatch_id));
        CHECK("batch ids differ",
              memcmp(tb_entries[0].tbatch_id, tb_entries[1].tbatch_id, 20) != 0);
    }

    /* --- Verify task entries --- */
    task_entry_t *tk_entries = NULL;
    int tk_count = 0;
    err = tpb_raf_entry_list_task(g_test_dir, &tk_entries, &tk_count);
    CHECK("list_task ok", err == 0);
    CHECK_INT("task count", 5, tk_count);

    if (err == 0 && tk_count == 5 && tb_count == 2) {
        /* Task 0: from batch 1 */
        CHECK("task0 tbatch_id matches batch1",
              memcmp(tk_entries[0].tbatch_id, tb_entries[0].tbatch_id, 20) == 0);

        /* Tasks 1,2,3: from batch 2 */
        for (int i = 1; i <= 3; i++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "task%d tbatch_id matches batch2", i);
            CHECK(msg,
                  memcmp(tk_entries[i].tbatch_id, tb_entries[1].tbatch_id, 20) == 0);
        }

        /* Task 4: direct invocation, tbatch_id should be all-zero */
        CHECK("task4 tbatch_id is zero", is_zero_id(tk_entries[4].tbatch_id));
    }

    /* Cleanup */
    free(tb_entries);
    free(tk_entries);
    cleanup_test_dir();

    return (g_fail > 0) ? 1 : 0;
}

static int
test_benchmark_tbatch_record(void)
{
    int err;
    char cmd[4096];
    char suite_path[640];
    tbatch_entry_t *tb_entries = NULL;
    task_entry_t *tk_entries = NULL;
    int tb_count = 0;
    int tk_count = 0;

    setup_test_dir();

    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("bench init_workspace", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(suite_path, sizeof(suite_path), "%s/bench_stream.yml", g_test_dir);
    err = write_stream_bench_yaml(suite_path);
    CHECK("bench write yaml", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli benchmark "
             "--suite %s",
             g_bin_dir, g_test_dir, g_bin_dir, suite_path);
    err = run_cmd(cmd);
    CHECK("benchmark exit", err == 0);

    err = tpb_raf_entry_list_tbatch(g_test_dir, &tb_entries, &tb_count);
    CHECK("bench list_tbatch ok", err == 0);
    CHECK_INT("bench tbatch count", 1, tb_count);
    if (err == 0 && tb_count == 1) {
        CHECK_INT("bench batch type", TPB_BATCH_TYPE_BENCHMARK,
                  tb_entries[0].batch_type);
        CHECK_INT("bench ntask", 1, tb_entries[0].ntask);
        CHECK("bench id nonzero", !is_zero_id(tb_entries[0].tbatch_id));
    }

    err = tpb_raf_entry_list_task(g_test_dir, &tk_entries, &tk_count);
    CHECK("bench list_task ok", err == 0);
    CHECK_INT("bench task count", 1, tk_count);
    if (err == 0 && tk_count == 1 && tb_count == 1) {
        CHECK("bench task tbatch_id matches",
              memcmp(tk_entries[0].tbatch_id, tb_entries[0].tbatch_id, 20) == 0);
    }

    free(tb_entries);
    free(tk_entries);
    cleanup_test_dir();
    return (g_fail > 0) ? 1 : 0;
}

static int
test_benchmark_begin_batch_fail(void)
{
    int err;
    char cmd[4096];
    char suite_path[640];
    char buf_path[640];
    FILE *fp;
    char line[512];
    int saw_begin_fail = 0;

    setup_test_dir();

    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("bench_fail init_workspace", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    err = make_task_batch_readonly(g_test_dir);
    CHECK("bench_fail chmod task_batch", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(suite_path, sizeof(suite_path), "%s/bench_stream.yml", g_test_dir);
    err = write_stream_bench_yaml(suite_path);
    CHECK("bench_fail write yaml", err == 0);
    if (err != 0) {
        (void)restore_task_batch_writable(g_test_dir);
        cleanup_test_dir();
        return 1;
    }

    snprintf(buf_path, sizeof(buf_path), "%s/bench_fail.out", g_test_dir);
    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli benchmark "
             "--suite %s > %s 2>&1",
             g_bin_dir, g_test_dir, g_bin_dir, suite_path, buf_path);
    err = run_cmd(cmd);
    CHECK("benchmark begin_batch fail exit", err != 0);

    fp = fopen(buf_path, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, "begin_batch failed") != NULL) {
                saw_begin_fail = 1;
                break;
            }
        }
        fclose(fp);
    }
    CHECK("benchmark begin_batch fail message", saw_begin_fail);

    (void)restore_task_batch_writable(g_test_dir);
    cleanup_test_dir();
    return (g_fail > 0) ? 1 : 0;
}

static int
test_benchmark_metric_missing_soft_fail(void)
{
    int err;
    char cmd[4096];
    char suite_path[640];
    char buf_path[640];
    FILE *fp;
    char line[512];
    int saw_metric_missing = 0;
    int saw_score_na = 0;

    setup_test_dir();

    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("bench_missing init_workspace", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(suite_path, sizeof(suite_path), "%s/bench_missing.yml", g_test_dir);
    err = write_stream_bench_yaml_missing_metric(suite_path);
    CHECK("bench_missing write yaml", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }

    snprintf(buf_path, sizeof(buf_path), "%s/bench_missing.out", g_test_dir);
    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli benchmark "
             "--suite %s > %s 2>&1",
             g_bin_dir, g_test_dir, g_bin_dir, suite_path, buf_path);
    err = run_cmd(cmd);
    CHECK("benchmark metric missing exit 0", err == 0);

    fp = fopen(buf_path, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, "Metric 'NoSuchMetric' not found") != NULL) {
                saw_metric_missing = 1;
            }
            if (strstr(line, "s1") != NULL && strstr(line, "N/A") != NULL) {
                saw_score_na = 1;
            }
        }
        fclose(fp);
    }
    CHECK("benchmark metric missing warn message", saw_metric_missing);
    CHECK("benchmark metric missing score N/A", saw_score_na);

    cleanup_test_dir();
    return (g_fail > 0) ? 1 : 0;
}

static int
test_rtenv_fk_and_counter(void)
{
    int err;
    char cmd[2048];
    tbatch_entry_t *tb = NULL;
    task_entry_t *tk = NULL;
    tpb_raf_rtenv_entry_t *re = NULL;
    int tb_n = 0;
    int tk_n = 0;
    int re_n = 0;

    setup_test_dir();
    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("rtenv_fk init_workspace", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }
    {
        int32_t base = -1;
        err = tpb_rtenv_ensure_base_env(g_test_dir, &base);
        CHECK("rtenv_fk ensure_base", err == 0 && base == 1);
    }

    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s TPB_RTENV_ID=1 %s/tpbcli run "
             "--kernel stream --kargs stream_array_size=32,ntest=10",
             g_bin_dir, g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("rtenv_fk run exit", err == 0);

    err = tpb_raf_entry_list_tbatch(g_test_dir, &tb, &tb_n);
    CHECK("rtenv_fk list tbatch", err == 0 && tb_n == 1);
    if (err == 0 && tb_n == 1) {
        CHECK_INT("tbatch rtenv fk", 1, tb[0].runtime_environment_id);
    }

    err = tpb_raf_entry_list_task(g_test_dir, &tk, &tk_n);
    CHECK("rtenv_fk list task", err == 0 && tk_n == 1);
    if (err == 0 && tk_n == 1) {
        CHECK_INT("task rtenv fk", 1, tk[0].runtime_environment_id);
    }

    err = tpb_raf_entry_list_rtenv(g_test_dir, &re, &re_n);
    CHECK("rtenv_fk list rtenv", err == 0 && re_n == 1);
    if (err == 0 && re_n == 1) {
        CHECK_INT("rtenv ntask", 1, (int)re[0].ntask);
        CHECK_INT("rtenv ntbatch", 1, (int)re[0].ntbatch);
    }

    free(tb);
    free(tk);
    free(re);
    cleanup_test_dir();
    return (g_fail > 0) ? 1 : 0;
}

static int
test_rtenv_fallback_warn(void)
{
    int err;
    char cmd[2048];
    tbatch_entry_t *tb = NULL;
    int tb_n = 0;

    setup_test_dir();
    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("rtenv_fallback init", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }
    {
        int32_t base_id = 0;

        (void)tpb_rtenv_ensure_base_env(g_test_dir, &base_id);
    }

    unsetenv("TPB_RTENV_ID");
    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s %s/tpbcli run --kernel stream "
             "--kargs stream_array_size=32,ntest=10",
             g_bin_dir, g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("rtenv_fallback run exit", err == 0);

    err = tpb_raf_entry_list_tbatch(g_test_dir, &tb, &tb_n);
    CHECK("rtenv_fallback list tbatch", err == 0 && tb_n == 1);
    if (err == 0 && tb_n == 1) {
        CHECK_INT("rtenv_fallback fk", 1, tb[0].runtime_environment_id);
    }

    free(tb);
    cleanup_test_dir();
    return (g_fail > 0) ? 1 : 0;
}

static int
test_rtenv_kenvs_input_param(void)
{
    int err;
    char cmd[2048];
    task_entry_t *tk = NULL;
    int tk_n = 0;
    int found = 0;

    setup_test_dir();
    err = tpb_raf_init_workspace(g_test_dir);
    CHECK("rtenv_kenvs init", err == 0);
    if (err != 0) {
        cleanup_test_dir();
        return 1;
    }
    {
        int32_t base_id = 0;

        (void)tpb_rtenv_ensure_base_env(g_test_dir, &base_id);
    }

    snprintf(cmd, sizeof(cmd),
             "TPB_HOME=%s/.. TPB_WORKSPACE=%s TPB_RTENV_ID=1 "
             "OMP_NUM_THREADS=4 %s/tpbcli run --kernel stream "
             "--kargs stream_array_size=32,ntest=10 "
             "--kenvs OMP_NUM_THREADS=4",
             g_bin_dir, g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("rtenv_kenvs run exit", err == 0);

    err = tpb_raf_entry_list_task(g_test_dir, &tk, &tk_n);
    CHECK("rtenv_kenvs list task", err == 0 && tk_n == 1);
    if (err == 0 && tk_n == 1) {
        task_attr_t attr;
        void *data = NULL;
        uint64_t dsize = 0;
        uint32_t i;

        err = tpb_raf_record_read_task(g_test_dir, tk[0].task_record_id,
                                       &attr, &data, &dsize);
        CHECK("rtenv_kenvs read task", err == 0);
        if (err == 0) {
            size_t off = 0;
            uint32_t i;

            for (i = 0; i < attr.nheader; i++) {
                if (strcmp(attr.headers[i].name, "OMP_NUM_THREADS") == 0) {
                    const char *val = (const char *)data + off;
                    if (strcmp(val, "4") == 0 &&
                        (attr.headers[i].type_bits & TPB_PARM_SOURCE_MASK)
                        == (TPB_PARM_ENV & TPB_PARM_SOURCE_MASK)) {
                        found = 1;
                    }
                    break;
                }
                off += (size_t)attr.headers[i].data_size;
            }
        }
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
    }
    CHECK("rtenv_kenvs header found", found);

    free(tk);
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

    /* Resolve bin directory from executable path */
    {
        /* The test binary lives in build/tests/bin/; tpbcli lives in build/bin/ */
        char self[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (len > 0) {
            self[len] = '\0';
            /* Strip filename */
            char *slash = strrchr(self, '/');
            if (slash) *slash = '\0';
            /* Go up from tests/bin/ to build root, then into bin/ */
            snprintf(g_bin_dir, sizeof(g_bin_dir), "%s/../../bin", self);
        } else {
            snprintf(g_bin_dir, sizeof(g_bin_dir), "./build/bin");
        }
    }

    test_case_t cases[] = {
        {"C1.1", "tri_tests_record", test_tri_record},
        {"C1.2", "benchmark_tbatch_record", test_benchmark_tbatch_record},
        {"C1.3", "benchmark_begin_batch_fail", test_benchmark_begin_batch_fail},
        {"C1.4", "benchmark_metric_missing_soft_fail",
         test_benchmark_metric_missing_soft_fail},
        {"C1.5", "rtenv_fk_and_counter", test_rtenv_fk_and_counter},
        {"C1.6", "rtenv_fallback_warn", test_rtenv_fallback_warn},
        {"C1.7", "rtenv_kenvs_input_param", test_rtenv_kenvs_input_param},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int pass = 0, fail = 0;

    printf("Running test pack C1 (%d cases)\n", n);
    printf("------------------------------------------------------\n");
    printf("  bin_dir: %s\n", g_bin_dir);

    for (int i = 0; i < n; i++) {
        if (filter && strcmp(filter, cases[i].id) != 0) continue;
        int res = cases[i].func();
        printf("[%s] %-36s %s\n", cases[i].id, cases[i].name,
               res == 0 ? "PASS" : "FAIL");
        if (res == 0) pass++;
        else fail++;
    }

    printf("------------------------------------------------------\n");
    printf("Pack C1: %d passed, %d failed (%d checks: %d ok, %d fail)\n",
           pass, fail, g_pass + g_fail, g_pass, g_fail);
    return fail;
}
