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

/* Local Function Prototypes */

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
static int test_kargs_dim_bad_parm(void);
static int test_help_kernel_specific(void);

/* Local Function Implementations */

static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

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
    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kargs ntest=10 --kernel stream",
             TPB_TEST_TPBCLI_STR);
    return expect_fail_with_hint("B2.1 kargs_before_kernel", cmd);
}

static int
test_kargs_dim_before_kernel(void)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kargs-dim 'ntest=[10,20]' --kernel stream",
             TPB_TEST_TPBCLI_STR);
    return expect_fail_with_hint("B2.2 kargs_dim_before_kernel", cmd);
}

static int
test_kenvs_before_kernel(void)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kenvs OMP_NUM_THREADS=4 --kernel stream",
             TPB_TEST_TPBCLI_STR);
    return expect_fail_with_hint("B2.3 kenvs_before_kernel", cmd);
}

static int
test_kmpiargs_before_kernel(void)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kmpiargs '-np 2' --kernel stream",
             TPB_TEST_TPBCLI_STR);
    return expect_fail_with_hint("B2.4 kmpiargs_before_kernel", cmd);
}

static int
test_kenvs_dim_before_kernel(void)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kenvs-dim 'OMP_NUM_THREADS=[1,2]' --kernel stream",
             TPB_TEST_TPBCLI_STR);
    return expect_fail_with_hint("B2.6 kenvs_dim_before_kernel", cmd);
}

static int
test_kmpiargs_dim_before_kernel(void)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kmpiargs-dim 'np=[1,2]' --kernel stream",
             TPB_TEST_TPBCLI_STR);
    return expect_fail_with_hint("B2.7 kmpiargs_dim_before_kernel", cmd);
}

static int
test_normal_with_kernel(void)
{
    char cmd[4096];
    int code;

    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kernel stream --kargs stream_array_size=524288,ntest=5",
             TPB_TEST_TPBCLI_STR);
    code = run_cmd_capture(cmd, NULL, 0);
    if (code != 0) {
        FAIL("B2.5 normal_with_kernel");
        fprintf(stderr, "    expected exit 0, got %d\n", code);
        return 1;
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

    snprintf(cmd, sizeof(cmd),
             "\"%s\" run -d --kernel stream "
             "--kargs stream_array_size=1000,ntest=5",
             TPB_TEST_TPBCLI_STR);
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

    snprintf(cmd, sizeof(cmd), "\"%s\" run --help", TPB_TEST_TPBCLI_STR);
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

    snprintf(cmd, sizeof(cmd), "\"%s\" run --kernel -h", TPB_TEST_TPBCLI_STR);
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

    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kernel nonexistent_kern",
             TPB_TEST_TPBCLI_STR);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("B2.11: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "requires a legal kernel name") == NULL) {
        FAIL("B2.11: missing hint");
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

    snprintf(cmd, sizeof(cmd),
             "\"%s\" run -d --kernel stream "
             "--kargs-dim 'stream_array_size=[1000,2000]' "
             "--kargs-dim 'ntest=[10,20]'",
             TPB_TEST_TPBCLI_STR);
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
test_kargs_dim_bad_parm(void)
{
    char cmd[4096];
    char buf[8192];
    int code;

    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kernel stream --kargs-dim 'array_size=[524288]'",
             TPB_TEST_TPBCLI_STR);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("B2.14 kargs_dim_bad_parm: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "Dimension expansion failed") == NULL) {
        FAIL("B2.14 kargs_dim_bad_parm: missing expansion failure line");
        fprintf(stderr, "    output (truncated): %.800s\n", buf);
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

    snprintf(cmd, sizeof(cmd),
             "\"%s\" run --kernel stream --help",
             TPB_TEST_TPBCLI_STR);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B2.13: expected exit 0 (help is success)");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Parameters:") == NULL
        && strstr(buf, "Outputs:") == NULL) {
        FAIL("B2.13: missing kernel info");
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
    if (strcmp(id, "B2.14") == 0) {
        return test_kargs_dim_bad_parm();
    }

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
