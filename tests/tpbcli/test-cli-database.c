/*
 * test-cli-database.c
 * Pack B4: `tpbcli database` argp behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

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
test_b4_1_missing_subcmd(void)
{
    char buf[4096];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" database", buf,
                               sizeof(buf));

    if (code == 0) {
        FAIL("B4.1: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "list") == NULL || strstr(buf, "dump") == NULL) {
        FAIL("B4.1: expected list/dump hint");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_2_database_help_overview(void)
{
    char buf[8192];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" database -h", buf,
                               sizeof(buf));

    if (code != 0) {
        FAIL("B4.2: expected exit 0");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "list") == NULL || strstr(buf, "dump") == NULL) {
        FAIL("B4.2: missing list/dump");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "--id") == NULL || strstr(buf, "--file") == NULL) {
        FAIL("B4.2: missing brief dump options");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_3_list_help(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list -h", buf, sizeof(buf));

    if (code != 0) {
        FAIL("B4.3: expected exit 0");
        return 1;
    }
    if (strstr(buf, "Usage:") == NULL) {
        FAIL("B4.3: missing Usage");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_4_dump_help(void)
{
    char buf[8192];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -h", buf, sizeof(buf));

    if (code != 0) {
        FAIL("B4.4: expected exit 0");
        return 1;
    }
    if (strstr(buf, "--id") == NULL || strstr(buf, "--file") == NULL) {
        FAIL("B4.4: missing dump flags");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_5_dump_no_selector(void)
{
    char buf[4096];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" database dump", buf,
                               sizeof(buf));

    if (code == 0) {
        FAIL("B4.5: expected nonzero");
        return 1;
    }
    if (strstr(buf, "Usage:") == NULL) {
        FAIL("B4.5: missing usage");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_6_dump_conflict(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR
        "\" database dump --id deadbeef --file /tmp/x",
        buf, sizeof(buf));

    if (code == 0) {
        FAIL("B4.6: expected nonzero");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.6: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_7_unknown_action(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database nosuchcmd", buf, sizeof(buf));

    if (code == 0) {
        FAIL("B4.7: expected nonzero");
        return 1;
    }
    if (strstr(buf, "unknown argument") == NULL) {
        FAIL("B4.7: missing unknown argument");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_8_dump_unknown_flag(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database dump --notaflag", buf,
        sizeof(buf));

    if (code == 0) {
        FAIL("B4.8: expected nonzero");
        return 1;
    }
    if (strstr(buf, "unknown argument") == NULL) {
        FAIL("B4.8: missing unknown argument");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_9_ls_alias(void)
{
    char buf[4096];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" database ls", buf,
                               sizeof(buf));

    if (code != 0) {
        FAIL("B4.9: expected exit 0 for ls");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    (void)buf;
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *id = (argc >= 2) ? argv[1] : "";

    g_pass = 0;
    g_fail = 0;

    if (strcmp(id, "B4.1") == 0) {
        return test_b4_1_missing_subcmd();
    }
    if (strcmp(id, "B4.2") == 0) {
        return test_b4_2_database_help_overview();
    }
    if (strcmp(id, "B4.3") == 0) {
        return test_b4_3_list_help();
    }
    if (strcmp(id, "B4.4") == 0) {
        return test_b4_4_dump_help();
    }
    if (strcmp(id, "B4.5") == 0) {
        return test_b4_5_dump_no_selector();
    }
    if (strcmp(id, "B4.6") == 0) {
        return test_b4_6_dump_conflict();
    }
    if (strcmp(id, "B4.7") == 0) {
        return test_b4_7_unknown_action();
    }
    if (strcmp(id, "B4.8") == 0) {
        return test_b4_8_dump_unknown_flag();
    }
    if (strcmp(id, "B4.9") == 0) {
        return test_b4_9_ls_alias();
    }

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
