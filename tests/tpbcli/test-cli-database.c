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
    if (strstr(buf, "-dT") == NULL || strstr(buf, "-dt") == NULL ||
        strstr(buf, "-dk") == NULL || strstr(buf, "-dr") == NULL ||
        strstr(buf, "--domain") == NULL ||
        strstr(buf, "-n") == NULL || strstr(buf, "-N") == NULL) {
        FAIL("B4.3: missing list domain/count options");
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

static int
test_b4_10_list_domain_task(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list -dt", buf, sizeof(buf));

    if (code != 0) {
        FAIL("B4.10: expected exit 0 for -dt");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Start Time (UTC)") == NULL ||
        strstr(buf, "Task ID") == NULL) {
        FAIL("B4.10: missing task list headers");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_11_list_domain_kernel(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list --domain kernel",
        buf, sizeof(buf));

    if (code != 0) {
        FAIL("B4.11: expected exit 0 for --domain kernel");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Kernel Name") == NULL ||
        strstr(buf, "Kernel ID") == NULL) {
        FAIL("B4.11: missing kernel list headers");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_12_list_count_conflict(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list -n 3 -N 3", buf,
        sizeof(buf));

    if (code == 0) {
        FAIL("B4.12: expected nonzero for -n/-N conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.12: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_13_list_domain_conflict(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list -dT -dk", buf,
        sizeof(buf));

    if (code == 0) {
        FAIL("B4.13: expected nonzero for domain conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.13: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_14_list_domain_invalid(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list --domain bogus", buf,
        sizeof(buf));

    if (code == 0) {
        FAIL("B4.14: expected nonzero for bogus domain");
        return 1;
    }
    if (strstr(buf, "unknown domain") == NULL) {
        FAIL("B4.14: missing unknown domain message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_15_list_domain_rtenv(void)
{
    char buf[4096];
    int code;

    code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list -dr", buf, sizeof(buf));
    if (code != 0) {
        FAIL("B4.15: expected exit 0 for -dr");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Created UTC") == NULL ||
        strstr(buf, "Hostname") == NULL ||
        strstr(buf, "runtime_environment record") == NULL) {
        FAIL("B4.15: missing runtime_environment list headers (-dr)");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }

    code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list --domain runtime_environment",
        buf, sizeof(buf));
    if (code != 0) {
        FAIL("B4.15: expected exit 0 for --domain runtime_environment");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Created UTC") == NULL ||
        strstr(buf, "Hostname") == NULL ||
        strstr(buf, "runtime_environment record") == NULL) {
        FAIL("B4.15: missing runtime_environment list headers (--domain)");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_16_list_domain_conflict_dr(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database list -dT -dr", buf,
        sizeof(buf));

    if (code == 0) {
        FAIL("B4.16: expected nonzero for -dT/-dr conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.16: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
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
    if (strcmp(id, "B4.10") == 0) {
        return test_b4_10_list_domain_task();
    }
    if (strcmp(id, "B4.11") == 0) {
        return test_b4_11_list_domain_kernel();
    }
    if (strcmp(id, "B4.12") == 0) {
        return test_b4_12_list_count_conflict();
    }
    if (strcmp(id, "B4.13") == 0) {
        return test_b4_13_list_domain_conflict();
    }
    if (strcmp(id, "B4.14") == 0) {
        return test_b4_14_list_domain_invalid();
    }
    if (strcmp(id, "B4.15") == 0) {
        return test_b4_15_list_domain_rtenv();
    }
    if (strcmp(id, "B4.16") == 0) {
        return test_b4_16_list_domain_conflict_dr();
    }

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
