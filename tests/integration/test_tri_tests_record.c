/*
 * test_tri_tests_record.c
 * Test pack C1: Integration test for auto-record across 3 invocation modes.
 *
 * Runs:
 *   1) tpbcli r --kernel stream --kargs stream_array_size=32
 *   2) tpbcli run --kernel stream --kargs ntest=100 --kargs-dim stream_array_size=[242144,524288,1048576]
 *   3) tpbk_stream.tpbx clock_gettime 10 32 0  (direct invoke)
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
             "TPB_WORKSPACE=%s %s/tpbcli r --kernel stream --kargs stream_array_size=32",
             g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("run1 exit", err == 0);

    /* --- Run 2: tpbcli run --kernel stream --kargs ntest=100 --kargs-dim stream_array_size=[242144,524288,1048576] --- */
    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=%s %s/tpbcli run --kernel stream --kargs ntest=100 "
             "--kargs-dim stream_array_size=[242144,524288,1048576]",
             g_test_dir, g_bin_dir);
    err = run_cmd(cmd);
    CHECK("run2 exit", err == 0);

    /* --- Run 3: Direct kernel invocation (no TPB_TBATCH_ID) --- */
    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=%s %s/tpbk_stream.tpbx clock_gettime 10 32 0",
             g_test_dir, g_bin_dir);
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
