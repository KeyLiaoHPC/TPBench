/*
 * test-cli-run-nokernel.c
 * Test pack B2: `tpbcli run` rejects kargs/kenvs/kmpiargs and *-dim variants before --kernel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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

/* Pack B2 sub-case runners (argv[1] id B2.1 .. B2.5) */
static int test_kargs_before_kernel(void);
static int test_kargs_dim_before_kernel(void);
static int test_kenvs_before_kernel(void);
static int test_kmpiargs_before_kernel(void);
static int test_kenvs_dim_before_kernel(void);
static int test_kmpiargs_dim_before_kernel(void);
static int test_normal_with_kernel(void);
static int test_dry_run_basic(void);
static int test_help_run_level(void);
static int test_help_kernel_no_name(void);
static int test_bad_kernel_name(void);
static int test_dry_run_cartesian(void);
static int test_help_kernel_specific(void);

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
    char cmd[4096];
    build_tpbcli_cmd(cmd, sizeof(cmd), "run --kargs ntest=10 --kernel stream");
    return expect_fail_with_hint("B2.1 kargs_before_kernel", cmd);
}

static int
test_kargs_dim_before_kernel(void)
{
    char cmd[4096];
    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run --kargs-dim 'ntest=[10,20]' --kernel stream");
    return expect_fail_with_hint("B2.2 kargs_dim_before_kernel", cmd);
}

static int
test_kenvs_before_kernel(void)
{
    char cmd[4096];
    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run --kenvs OMP_NUM_THREADS=4 --kernel stream");
    return expect_fail_with_hint("B2.3 kenvs_before_kernel", cmd);
}

static int
test_kmpiargs_before_kernel(void)
{
    char cmd[4096];
    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run --kmpiargs '-np 2' --kernel stream");
    return expect_fail_with_hint("B2.4 kmpiargs_before_kernel", cmd);
}

static int
test_kenvs_dim_before_kernel(void)
{
    char cmd[4096];
    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run --kenvs-dim 'OMP_NUM_THREADS=[1,2]' --kernel stream");
    return expect_fail_with_hint("B2.6 kenvs_dim_before_kernel", cmd);
}

static int
test_kmpiargs_dim_before_kernel(void)
{
    char cmd[4096];
    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run --kmpiargs-dim 'np=[1,2]' --kernel stream");
    return expect_fail_with_hint("B2.7 kmpiargs_dim_before_kernel", cmd);
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
    if (strstr(buf, "FOM,BANDWIDTH::Triad") == NULL &&
        strstr(buf, "Triad:") == NULL) {
        FAIL("B2.5 normal_with_kernel: missing Triad metric");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "MB/s") == NULL) {
        FAIL("B2.5 normal_with_kernel: missing MB/s units");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    triad_line = strstr(buf, "Triad:");
    if (triad_line != NULL) {
        if (sscanf(triad_line, "Triad: %lf", &triad_bw) != 1 ||
            triad_bw <= 0.0) {
            FAIL("B2.5 normal_with_kernel: invalid Triad bandwidth");
            fprintf(stderr, "    line: %.80s\n", triad_line);
            return 1;
        }
    }
    PASS();
    return 0;
}

static int
test_dry_run_basic(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run -d --kernel stream "
                     "--kargs stream_array_size=1000,ntest=5");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.8 dry_run_basic: nonzero exit");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "[DRY-RUN]") == NULL) {
        FAIL("B2.8 dry_run_basic: missing DRY-RUN");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Exec:") == NULL) {
        FAIL("B2.8 dry_run_basic: missing Exec");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_help_run_level(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

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
    PASS();
    return 0;
}

static int
test_help_kernel_no_name(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

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
    PASS();
    return 0;
}

static int
test_bad_kernel_name(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd), "run --kernel nonexistent_kern");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("B2.11: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "requires a legal kernel name") == NULL &&
        strstr(buf, "Kernel nonexistent_kern not found") == NULL &&
        strstr(buf, "Failed to scan kernel nonexistent_kern") == NULL) {
        FAIL("B2.11: missing hint");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Available kernels:") == NULL) {
        FAIL("B2.11: missing available kernel list");
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
        strstr(buf, "Metrics") == NULL) {
        FAIL("B2.13: missing kernel info");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Type/Description") == NULL ||
        strstr(buf, "Tags/Unit/Description") == NULL) {
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
    if (strstr(buf, "INPARM") == NULL ||
        strstr(buf, "Data size (e.g. B, MB, GB)") == NULL) {
        FAIL("B2.13: missing metric tag or unit category");
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
    if (strcmp(id, "B2.2") == 0) {
        return test_kargs_dim_before_kernel();
    }
    if (strcmp(id, "B2.3") == 0) {
        return test_kenvs_before_kernel();
    }
    if (strcmp(id, "B2.4") == 0) {
        return test_kmpiargs_before_kernel();
    }
    if (strcmp(id, "B2.5") == 0) {
        return test_normal_with_kernel();
    }
    if (strcmp(id, "B2.6") == 0) {
        return test_kenvs_dim_before_kernel();
    }
    if (strcmp(id, "B2.7") == 0) {
        return test_kmpiargs_dim_before_kernel();
    }
    if (strcmp(id, "B2.8") == 0) {
        return test_dry_run_basic();
    }
    if (strcmp(id, "B2.9") == 0) {
        return test_help_run_level();
    }
    if (strcmp(id, "B2.10") == 0) {
        return test_help_kernel_no_name();
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

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
