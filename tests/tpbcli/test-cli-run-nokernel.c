/*
 * test-cli-run-nokernel.c
 * Test pack B2: `tpbcli run` rejects kargs/kenvs before --kernel; wrapper-args needs --wrapper.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

#ifndef TPB_TEST_TPB_HOME
#define TPB_TEST_TPB_HOME ""
#endif

/* Local Function Prototypes */

/*
 * Prefix shell command with TPB_HOME/TPB_WORKSPACE when TPB_TEST_TPB_HOME set.
 */
static void build_tpbcli_cmd(char *cmd, size_t cmd_sz, const char *args);

/*
 * Expect nonzero exit and stderr containing the preceding --kernel hint.
 */
static int expect_fail_with_hint(const char *case_name, const char *cmd);

/*
 * Run shell command; merge stderr into captured buffer if non-NULL.
 */
static int run_cmd_capture(const char *cmd, char *errbuf, size_t errbuf_sz);

/* Pack B2 sub-case runners */
static int test_kargs_before_kernel(void);
static int test_normal_with_kernel(void);
static int test_bad_kernel_name(void);
static int test_dry_run_cartesian(void);
static int test_help_kernel_specific(void);
static int test_run_begin_batch_fail(void);

/* Local Function Implementations */

static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

static void
build_tpbcli_cmd(char *cmd, size_t cmd_sz, const char *args)
{
    if (TPB_TEST_TPB_HOME[0] != '\0') {
        snprintf(cmd, cmd_sz,
                 "TPB_HOME=\"%s\" TPB_WORKSPACE=\"%s\" \"%s\" %s",
                 TPB_TEST_TPB_HOME, TPB_TEST_TPB_HOME,
                 TPB_TEST_TPBCLI_STR, args);
    } else {
        snprintf(cmd, cmd_sz, "\"%s\" %s", TPB_TEST_TPBCLI_STR, args);
    }
}

static int
run_cmd_capture(const char *cmd, char *errbuf, size_t errbuf_sz)
{
    char full[8192];
    FILE *fp;

    if (errbuf != NULL && errbuf_sz > 0) {
        errbuf[0] = '\0';
    }

    snprintf(full, sizeof(full), "%s 2>&1", cmd);
    fp = popen(full, "r");
    if (fp == NULL) {
        return -1;
    }
    if (errbuf != NULL && errbuf_sz > 0) {
        size_t n = fread(errbuf, 1, errbuf_sz - 1, fp);
        errbuf[n] = '\0';
    } else {
        while (fgetc(fp) != EOF) {
            /* drain */
        }
    }
    int st = pclose(fp);
    if (st == -1) {
        return -1;
    }
    return WEXITSTATUS(st);
}

static int
expect_fail_with_hint(const char *case_name, const char *cmd)
{
    char buf[4096];
    int code = run_cmd_capture(cmd, buf, sizeof(buf));

    if (code == 0) {
        FAIL(case_name);
        fprintf(stderr, "    expected nonzero exit, got 0\n");
        return 1;
    }
    if (strstr(buf, "unknown argument") == NULL) {
        FAIL(case_name);
        fprintf(stderr, "    stderr missing \"unknown argument\"\n");
        fprintf(stderr, "    output (truncated): %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_kargs_before_kernel(void)
{
    static const struct {
        const char *case_name;
        const char *args;
    } cases[] = {
        { "B2.1 kargs_before_kernel",
          "run --kargs ntest=10 --kernel stream" },
        /* absorbed B2.2/B2.3/B2.6: k* options before --kernel */
        { "B2.2 kargs_dim_before_kernel",
          "run --kargs-dim 'ntest=[10,20]' --kernel stream" },
        { "B2.3 kenvs_before_kernel",
          "run --kenvs OMP_NUM_THREADS=4 --kernel stream" },
        { "B2.6 kenvs_dim_before_kernel",
          "run --kenvs-dim 'OMP_NUM_THREADS=[1,2]' --kernel stream" },
    };
    char cmd[4096];
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        build_tpbcli_cmd(cmd, sizeof(cmd), cases[i].args);
        if (expect_fail_with_hint(cases[i].case_name, cmd) != 0) {
            return 1;
        }
    }
    return 0;
}

static int
test_normal_with_kernel(void)
{
    char cmd[4096];
    char buf[32768];
    const char *triad_line;
    double triad_bw;
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run --kernel stream "
                     "--kargs stream_array_size=524288,ntest=5");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.5 normal_with_kernel");
        fprintf(stderr, "    expected exit 0, got %d\n", code);
        return 1;
    }
    if (strstr(buf, "Solution Validates") == NULL) {
        FAIL("B2.5 normal_with_kernel: missing validation");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Triad bandwidth") == NULL) {
        FAIL("B2.5 normal_with_kernel: missing Triad metric");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "mean") == NULL || strstr(buf, "p50") == NULL) {
        FAIL("B2.5 normal_with_kernel: missing result table headers");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "MB/s") == NULL) {
        FAIL("B2.5 normal_with_kernel: missing MB/s units");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }

    /* absorbed B2.14 run_kernel_found_id: second run, KernelID / no skip-update */
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.14: second run expected exit 0");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "already recorded; skip update") != NULL) {
        FAIL("B2.14: unexpected already-recorded warning");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Kernel stream found, KernelID:") == NULL) {
        FAIL("B2.14: missing kernel found message");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }

    triad_line = strstr(buf, "Triad bandwidth");
    if (triad_line != NULL) {
        (void)triad_bw;
    }
    PASS();
    return 0;
}

static int
test_bad_kernel_name(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

    /* absorbed B2.9 help_run_level */
    build_tpbcli_cmd(cmd, sizeof(cmd), "run --help");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.9 help_run_level: expected exit 0 (help is success)");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Usage:") == NULL || strstr(buf, "--kernel") == NULL) {
        FAIL("B2.9 help_run_level: missing expected help text");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }

    /* absorbed B2.10 help_kernel_no_name */
    build_tpbcli_cmd(cmd, sizeof(cmd), "run --kernel -h");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.10: expected exit 0 (help is success)");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "requires a legal kernel name") == NULL) {
        FAIL("B2.10: missing hint");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "tpbcli kernel list") == NULL) {
        FAIL("B2.10: missing kernel list hint");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }

    build_tpbcli_cmd(cmd, sizeof(cmd), "run --kernel nonexistent_kern");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("B2.11: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "Kernel nonexistent_kern not found. Use `tpbcli kernel list`")
        == NULL) {
        FAIL("B2.11: missing not-found hint");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Failed to scan kernel nonexistent_kern") != NULL) {
        FAIL("B2.11: unexpected dynloader scan error");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Available kernels:") != NULL) {
        FAIL("B2.11: unexpected inline kernel list");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_dry_run_cartesian(void)
{
    char cmd[4096];
    char buf[16384];
    int code;
    int exec_count = 0;
    const char *p;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run -d --kernel stream "
                     "--kargs-dim 'stream_array_size=[1000,2000]' "
                     "--kargs-dim 'ntest=[10,20]'");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.12 dry_run_cartesian: nonzero exit");
        return 1;
    }
    p = buf;
    while ((p = strstr(p, "Exec:")) != NULL) {
        exec_count++;
        p += 5;
    }
    if (exec_count != 4) {
        FAIL("B2.12: expected 4 Exec lines");
        fprintf(stderr, "    got %d\n", exec_count);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_help_kernel_specific(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd), "run --kernel stream --help");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.13: expected exit 0 (help is success)");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Kernel: stream") == NULL ||
        strstr(buf, "Parameters::CLI") == NULL ||
        strstr(buf, "Data Records") == NULL) {
        FAIL("B2.13: missing kernel info");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "\nMetrics\n") != NULL ||
        strstr(buf, "Metrics\n---") != NULL) {
        FAIL("B2.13: old Metrics section still present");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Type/Description") == NULL ||
        strstr(buf, "Tags/Type/Unit/Description") == NULL) {
        FAIL("B2.13: missing column headers");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "(/type:") != NULL) {
        FAIL("B2.13: old parameter type wrapper still present");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "long int") == NULL ||
        strstr(buf, "unsigned int") == NULL) {
        FAIL("B2.13: missing parameter type strings");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Allocated memory size") == NULL ||
        strstr(buf, "INPARM::Allocated memory size") != NULL) {
        FAIL("B2.13: metric name/tag split incorrect");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "TPBINPUT") == NULL ||
        strstr(buf, "TPBOUTPUT") == NULL ||
        strstr(buf, "TPBFOM") == NULL ||
        strstr(buf, "Data size (e.g. B, MB, GB)") == NULL) {
        FAIL("B2.13: missing preset tags or unit category");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "INPARM") != NULL ||
        strstr(buf, "TPBARG") != NULL ||
        strstr(buf, "ntest") == NULL ||
        strstr(buf, "Triad bandwidth") == NULL) {
        if (strstr(buf, "INPARM") != NULL || strstr(buf, "TPBARG") != NULL) {
            FAIL("B2.13: obsolete tags still present");
            fprintf(stderr, "    output: %.500s\n", buf);
            return 1;
        }
        if (strstr(buf, "ntest") == NULL ||
            strstr(buf, "Triad bandwidth") == NULL) {
            FAIL("B2.13: Data Records missing input/output names");
            fprintf(stderr, "    output: %.500s\n", buf);
            return 1;
        }
    }
    PASS();
    return 0;
}

static int
test_run_begin_batch_fail(void)
{
    char cmd[4096];
    char buf[32768];
    char ws[512];
    char tbatch_dir[640];
    int code;

    snprintf(ws, sizeof(ws), "/tmp/tpb_run_tbatch_fail_%d", (int)getpid());
    snprintf(tbatch_dir, sizeof(tbatch_dir), "%s/rafdb/task_batch", ws);

    if (mkdir(ws, 0755) != 0 && errno != EEXIST) {
        FAIL("B2.15 setup mkdir workspace");
        return 1;
    }
    {
        char rafdb_dir[640];
        snprintf(rafdb_dir, sizeof(rafdb_dir), "%s/rafdb", ws);
        if (mkdir(rafdb_dir, 0755) != 0 && errno != EEXIST) {
            FAIL("B2.15 setup mkdir rafdb");
            return 1;
        }
    }
    if (mkdir(tbatch_dir, 0755) != 0 && errno != EEXIST) {
        FAIL("B2.15 setup mkdir task_batch");
        return 1;
    }
    if (chmod(tbatch_dir, 0555) != 0) {
        FAIL("B2.15 setup chmod task_batch");
        return 1;
    }

    if (TPB_TEST_TPB_HOME[0] != '\0') {
        snprintf(cmd, sizeof(cmd),
                 "TPB_HOME=\"%s\" TPB_WORKSPACE=\"%s\" \"%s\" "
                 "run --kernel stream --kargs stream_array_size=32,ntest=5",
                 TPB_TEST_TPB_HOME, ws, TPB_TEST_TPBCLI_STR);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "TPB_WORKSPACE=\"%s\" \"%s\" "
                 "run --kernel stream --kargs stream_array_size=32,ntest=5",
                 ws, TPB_TEST_TPBCLI_STR);
    }

    code = run_cmd_capture(cmd, buf, sizeof(buf));
    (void)chmod(tbatch_dir, 0755);
    {
        char rm_cmd[640];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", ws);
        (void)system(rm_cmd);
    }

    if (code == 0) {
        FAIL("B2.15 run_begin_batch_fail: expected nonzero exit");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "begin_batch failed") == NULL) {
        FAIL("B2.15 run_begin_batch_fail: missing begin_batch failed message");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *id = (argc >= 2) ? argv[1] : "";

    g_pass = 0;
    g_fail = 0;

    if (strcmp(id, "B2.1") == 0) {
        return test_kargs_before_kernel();
    }
    if (strcmp(id, "B2.5") == 0) {
        return test_normal_with_kernel();
    }
    if (strcmp(id, "B2.11") == 0) {
        return test_bad_kernel_name();
    }
    if (strcmp(id, "B2.12") == 0) {
        return test_dry_run_cartesian();
    }
    if (strcmp(id, "B2.13") == 0) {
        return test_help_kernel_specific();
    }
    if (strcmp(id, "B2.15") == 0) {
        return test_run_begin_batch_fail();
    }

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
