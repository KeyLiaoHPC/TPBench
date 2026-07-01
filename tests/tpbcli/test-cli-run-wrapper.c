/*
 * test-cli-run-wrapper.c
 * Dry-run tests for generic PLI wrapper chain command generation.
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
run_cmd_capture(const char *cmd, char *outbuf, size_t outbuf_sz)
{
    char full[8192];
    FILE *fp;

    if (outbuf != NULL && outbuf_sz > 0) {
        outbuf[0] = '\0';
    }

    snprintf(full, sizeof(full), "%s 2>&1", cmd);
    fp = popen(full, "r");
    if (fp == NULL) {
        return -1;
    }
    if (outbuf != NULL && outbuf_sz > 0) {
        size_t n = fread(outbuf, 1, outbuf_sz - 1, fp);
        outbuf[n] = '\0';
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

static const char *
find_exec_line(const char *buf)
{
    return strstr(buf, "Exec:");
}

static int
exec_line_contains(const char *exec, const char *needle)
{
    const char *end = strchr(exec, '\n');
    size_t span;
    size_t nlen;
    size_t i;

    if (exec == NULL || needle == NULL) {
        return 0;
    }

    span = (end != NULL) ? (size_t)(end - exec) : strlen(exec);
    nlen = strlen(needle);
    if (nlen == 0 || nlen > span) {
        return 0;
    }

    for (i = 0; i + nlen <= span; i++) {
        if (memcmp(exec + i, needle, nlen) == 0) {
            return 1;
        }
    }
    return 0;
}

static int
expect_dry_run_contains(const char *case_name, const char *args,
                        const char *must_contain, const char *must_not_contain)
{
    char cmd[4096];
    char buf[16384];
    const char *exec_line;
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd), args);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL(case_name);
        fprintf(stderr, "    expected exit 0, got %d\n", code);
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "[DRY-RUN]") == NULL) {
        FAIL(case_name);
        fprintf(stderr, "    missing [DRY-RUN]\n");
        return 1;
    }
    exec_line = find_exec_line(buf);
    if (exec_line == NULL) {
        FAIL(case_name);
        fprintf(stderr, "    missing Exec:\n");
        return 1;
    }
    if (must_contain != NULL && !exec_line_contains(exec_line, must_contain)) {
        FAIL(case_name);
        fprintf(stderr, "    Exec missing \"%s\"\n", must_contain);
        fprintf(stderr, "    line: %.300s\n", exec_line);
        return 1;
    }
    if (must_not_contain != NULL &&
        exec_line_contains(exec_line, must_not_contain)) {
        FAIL(case_name);
        fprintf(stderr, "    Exec must not contain \"%s\"\n",
                must_not_contain);
        fprintf(stderr, "    line: %.300s\n", exec_line);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_no_wrapper(void)
{
    return expect_dry_run_contains(
        "W1.1 no_wrapper",
        "run -d --kernel stream --kargs stream_array_size=1000,ntest=2",
        "tpbcli-pli-launcher", NULL);
}

static int
test_global_wrapper_only(void)
{
    return expect_dry_run_contains(
        "W1.2 global_wrapper_only",
        "run -d --wrapper /usr/bin/fake-numactl --kernel stream "
        "--kargs stream_array_size=1000,ntest=2",
        "/usr/bin/fake-numactl", "fake-mpirun");
}

static int
test_global_local_chain(void)
{
    char cmd[4096];
    char buf[16384];
    const char *exec_line;
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run -d --wrapper /usr/bin/fake-numactl --kernel stream "
                     "--kargs stream_array_size=1000,ntest=2 "
                     "--wrapper /usr/bin/fake-mpirun --wrapper-args '-np 20'");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("W1.3 global_local_chain");
        fprintf(stderr, "    exit %d\n    output: %.500s\n", code, buf);
        return 1;
    }
    exec_line = find_exec_line(buf);
    if (exec_line == NULL ||
        !exec_line_contains(exec_line, "/usr/bin/fake-numactl") ||
        !exec_line_contains(exec_line, "/usr/bin/fake-mpirun") ||
        !exec_line_contains(exec_line, "-np 20") ||
        !exec_line_contains(exec_line, "tpbcli-pli-launcher")) {
        FAIL("W1.3 global_local_chain");
        fprintf(stderr, "    line: %.300s\n", exec_line ? exec_line : "(null)");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_override_global(void)
{
    char cmd[4096];
    char buf[16384];
    const char *exec_line;
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run -d --wrapper /usr/bin/fake-numactl --kernel stream "
                     "-og --kargs stream_array_size=1000,ntest=2 "
                     "--wrapper /usr/bin/fake-mpirun --wrapper-args '-np 20'");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("W1.4 override_global");
        fprintf(stderr, "    exit %d\n    output: %.500s\n", code, buf);
        return 1;
    }
    exec_line = find_exec_line(buf);
    if (exec_line == NULL ||
        !exec_line_contains(exec_line, "/usr/bin/fake-mpirun") ||
        !exec_line_contains(exec_line, "-np 20") ||
        exec_line_contains(exec_line, "/usr/bin/fake-numactl")) {
        FAIL("W1.4 override_global");
        fprintf(stderr, "    line: %.300s\n", exec_line ? exec_line : "(null)");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_multiple_kernels(void)
{
    char cmd[4096];
    char buf[32768];
    const char *exec1;
    const char *exec2;
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run -d --wrapper /usr/bin/fake-numactl "
                     "--kernel stream --kargs stream_array_size=1000,ntest=2 "
                     "--kernel stream --kargs stream_array_size=2000,ntest=2 "
                     "--wrapper /usr/bin/fake-mpirun --wrapper-args '-np 4'");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("W1.5 multiple_kernels");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    exec1 = find_exec_line(buf);
    if (exec1 == NULL) {
        FAIL("W1.5 multiple_kernels: missing first Exec");
        return 1;
    }
    exec2 = find_exec_line(exec1 + strlen("Exec:"));
    if (exec2 == NULL) {
        FAIL("W1.5 multiple_kernels: missing second Exec");
        return 1;
    }
    if (exec2 == exec1) {
        FAIL("W1.5 multiple_kernels: duplicate Exec pointer");
        return 1;
    }
    if (!exec_line_contains(exec1, "/usr/bin/fake-numactl") ||
        exec_line_contains(exec1, "/usr/bin/fake-mpirun")) {
        FAIL("W1.5 multiple_kernels: first kernel wrapper chain");
        fprintf(stderr, "    line: %.300s\n", exec1);
        return 1;
    }
    if (!exec_line_contains(exec2, "/usr/bin/fake-numactl") ||
        !exec_line_contains(exec2, "/usr/bin/fake-mpirun") ||
        !exec_line_contains(exec2, "-np 4")) {
        FAIL("W1.5 multiple_kernels: second kernel wrapper chain");
        fprintf(stderr, "    line: %.300s\n", exec2);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_wrapper_args_without_wrapper(void)
{
    char cmd[4096];
    char buf[4096];
    int code;

    build_tpbcli_cmd(cmd, sizeof(cmd),
                     "run -d --wrapper-args '-np 2' --kernel stream");
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("W1.6 wrapper_args_without_wrapper");
        fprintf(stderr, "    expected nonzero exit\n");
        return 1;
    }
    if (strstr(buf, "wrapper-args requires a preceding --wrapper") == NULL &&
        strstr(buf, "unknown argument") == NULL) {
        FAIL("W1.6 wrapper_args_without_wrapper: missing error hint");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    g_pass = 0;
    g_fail = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <subcase>\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "W1.1") == 0) {
        return test_no_wrapper();
    }
    if (strcmp(argv[1], "W1.2") == 0) {
        return test_global_wrapper_only();
    }
    if (strcmp(argv[1], "W1.3") == 0) {
        return test_global_local_chain();
    }
    if (strcmp(argv[1], "W1.4") == 0) {
        return test_override_global();
    }
    if (strcmp(argv[1], "W1.5") == 0) {
        return test_multiple_kernels();
    }
    if (strcmp(argv[1], "W1.6") == 0) {
        return test_wrapper_args_without_wrapper();
    }

    fprintf(stderr, "Unknown subcase: %s\n", argv[1]);
    return 2;
}
