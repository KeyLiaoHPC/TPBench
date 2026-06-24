/*
 * test-cli-kernel.c
 * Pack B5: `tpbcli kernel set/get` behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

#define TPB_RAF_KERNEL_DIR_LOCAL     "rafdb/kernel"
#define TPB_RAF_KERNEL_ENTRY_LOCAL   "kernel.tpbe"
#define TPB_RAF_MAGIC_LEN_LOCAL      8
#define TPB_RAF_ENTRY_SIZE_LOCAL     264

static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

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
        }
    }
    {
        int st = pclose(fp);
        if (st == -1) {
            return -1;
        }
        return WEXITSTATUS(st);
    }
}

static int
count_kernel_tpbe_entries(const char *workspace)
{
    char path[1024];
    struct stat st;
    long data;

    snprintf(path, sizeof(path), "%s/%s/%s",
             workspace, TPB_RAF_KERNEL_DIR_LOCAL, TPB_RAF_KERNEL_ENTRY_LOCAL);
    if (stat(path, &st) != 0) {
        return 0;
    }
    data = (long)st.st_size - 2 * TPB_RAF_MAGIC_LEN_LOCAL;
    if (data <= 0) {
        return 0;
    }
    return (int)(data / TPB_RAF_ENTRY_SIZE_LOCAL);
}

static int
ensure_stream_registered(const char *workspace)
{
    char cmd[4096];

    snprintf(cmd, sizeof(cmd),
             "rm -rf \"%s\" && mkdir -p \"%s\" && "
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" kernel list",
             workspace, workspace, workspace);
    return run_cmd_capture(cmd, NULL, 0);
}

static int
test_b5_1_set_missing_args(void)
{
    char buf[4096];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" kernel set", buf,
                               sizeof(buf));

    if (code == 0) {
        FAIL("B5.1 expected nonzero");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b5_2_get_no_register_side_effect(void)
{
#ifndef TPB_TEST_KERNEL_WORKSPACE
    PASS();
    return 0;
#else
    char cmd[4096];
    char buf[8192];
    int before;
    int after;
    int code;

    if (ensure_stream_registered(TPB_TEST_KERNEL_WORKSPACE) != 0) {
        FAIL("B5.2 setup list failed");
        return 1;
    }
    before = count_kernel_tpbe_entries(TPB_TEST_KERNEL_WORKSPACE);

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR
             "\" kernel get --kernel stream", TPB_TEST_KERNEL_WORKSPACE);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    after = count_kernel_tpbe_entries(TPB_TEST_KERNEL_WORKSPACE);

    if (code != 0) {
        FAIL("B5.2 get failed");
        return 1;
    }
    if (before != after) {
        FAIL("B5.2 get changed kernel.tpbe entry count");
        return 1;
    }
    if (strstr(buf, "KernelID:") == NULL) {
        FAIL("B5.2 missing KernelID output");
        return 1;
    }
    PASS();
    return 0;
#endif
}

static int
test_b5_3_set_and_get_metadata(void)
{
#ifndef TPB_TEST_KERNEL_WORKSPACE
    PASS();
    return 0;
#else
    char cmd[4096];
    char buf[8192];
    int code;

    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\"",
             TPB_TEST_KERNEL_WORKSPACE, TPB_TEST_KERNEL_WORKSPACE);
    if (run_cmd_capture(cmd, NULL, 0) != 0) {
        FAIL("B5.3 workspace setup failed");
        return 1;
    }

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR
             "\" kernel set --kernel stream "
             "--key compilation.kernel_cflags -O2",
             TPB_TEST_KERNEL_WORKSPACE);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B5.3 set failed");
        return 1;
    }

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR
             "\" kernel get --kernel stream",
             TPB_TEST_KERNEL_WORKSPACE);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0 || strstr(buf, "kernel_cflags=-O2") == NULL) {
        FAIL("B5.3 get missing metadata");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
#endif
}

int
main(int argc, char **argv)
{
    const char *id = (argc >= 2) ? argv[1] : "";

    g_pass = 0;
    g_fail = 0;

    if (strcmp(id, "B5.1") == 0) {
        return test_b5_1_set_missing_args();
    }
    if (strcmp(id, "B5.2") == 0) {
        return test_b5_2_get_no_register_side_effect();
    }
    if (strcmp(id, "B5.3") == 0) {
        return test_b5_3_set_and_get_metadata();
    }

    test_b5_1_set_missing_args();
    test_b5_2_get_no_register_side_effect();
    test_b5_3_set_and_get_metadata();
    return (g_fail > 0) ? 1 : 0;
}
