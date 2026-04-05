/*
 * test-cli-run-nokernel.c
 * Test pack B2: `tpbcli run` rejects kargs/kenvs/kmpiargs (and dim) before --kernel.
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
static int test_normal_with_kernel(void);

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
    if (strstr(buf, "preceding --kernel") == NULL) {
        FAIL(case_name);
        fprintf(stderr, "    stderr missing hint \"preceding --kernel\"\n");
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

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
