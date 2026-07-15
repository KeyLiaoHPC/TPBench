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

#ifndef TPB_TEST_WORKSPACE
#define TPB_TEST_WORKSPACE "."
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
        while (fgetc(fp) != EOF) {
        }
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
    if (strstr(buf, "-dT") == NULL || strstr(buf, "-i") == NULL ||
        strstr(buf, "-e") == NULL) {
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
    if (strstr(buf, "-dT") == NULL || strstr(buf, "-dr") == NULL ||
        strstr(buf, "--domain") == NULL || strstr(buf, "-i") == NULL ||
        strstr(buf, "-e") == NULL ||
        strstr(buf, "-n") == NULL || strstr(buf, "-N") == NULL) {
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
    if (strstr(buf, "domain") == NULL) {
        FAIL("B4.5: missing domain error");
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
        "\" database dump -dT -i deadbeef -e",
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

static int
test_b4_17_dump_domain_id_prefix(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dT -i deadbeef",
        buf, sizeof(buf));

    if (strstr(buf, "unknown argument") != NULL) {
        FAIL("B4.17: unexpected unknown argument");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    (void)code;
    PASS();
    return 0;
}

static int
test_b4_18_dump_missing_domain(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR
        "\" database dump -i abcdef0123456789abcdef0123456789abcdef",
        buf, sizeof(buf));

    if (code == 0) {
        FAIL("B4.18: expected nonzero without domain");
        return 1;
    }
    if (strstr(buf, "domain") == NULL) {
        FAIL("B4.18: missing domain error");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_19_dump_missing_mode(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dT", buf, sizeof(buf));

    if (code == 0) {
        FAIL("B4.19: expected nonzero without -i/-e");
        return 1;
    }
    if (strstr(buf, "-i") == NULL && strstr(buf, "-e") == NULL) {
        FAIL("B4.19: missing -i/-e hint");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_20_dump_id_entry_conflict(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR
        "\" database dump -dT -i deadbeef -e",
        buf, sizeof(buf));

    if (code == 0) {
        FAIL("B4.20: expected nonzero for -i/-e conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.20: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_21_dump_domain_conflict(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dT -dk", buf,
        sizeof(buf));

    if (code == 0) {
        FAIL("B4.21: expected nonzero for domain conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.21: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_22_dump_rtenv_entry(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "TPB_WORKSPACE=\"" TPB_TEST_WORKSPACE "\" "
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dr -e", buf, sizeof(buf));

    if (code != 0) {
        FAIL("B4.22: expected exit 0 for -dr -e");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "runtime_environment.tpbe") == NULL &&
        strstr(buf, "Entry File") == NULL) {
        FAIL("B4.22: missing rtenv entry dump header");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    if (strstr(buf, "entry[0].id,") == NULL) {
        FAIL("B4.22: missing rtenv entry rows (default -n window)");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b4_23_dump_rtenv_record(void)
{
    char buf[8192];
    const char *nenv_line;
    int nenv_val;
    int code = run_cmd_capture(
        "TPB_WORKSPACE=\"" TPB_TEST_WORKSPACE "\" "
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dr -i 1", buf,
        sizeof(buf));

    if (code != 0) {
        FAIL("B4.23: expected exit 0 for -dr -i 1");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Section: Metadata") == NULL ||
        strstr(buf, "id           = 1") == NULL) {
        FAIL("B4.23: missing rtenv .tpbr metadata");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "0x E1 54 50 42") == NULL) {
        FAIL("B4.23: missing tpbr START magic line");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    code = run_cmd_capture(
        "TPB_WORKSPACE=\"" TPB_TEST_WORKSPACE "\" "
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dr -i 1 2>&1 "
        "| tail -5",
        buf, sizeof(buf));
    if (code != 0 || strstr(buf, "END OF FILE") == NULL) {
        FAIL("B4.23: missing tpbr END OF FILE trailer");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    code = run_cmd_capture(
        "TPB_WORKSPACE=\"" TPB_TEST_WORKSPACE "\" "
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dr -i 1 2>&1 "
        "| grep -F 'nenv'",
        buf, sizeof(buf));
    if (code != 0) {
        FAIL("B4.23: missing nenv in metadata");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    nenv_line = strstr(buf, "nenv         = ");
    if (nenv_line == NULL) {
        nenv_line = strstr(buf, "nenv = ");
    }
    if (nenv_line == NULL) {
        FAIL("B4.23: missing nenv in metadata");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    nenv_val = atoi(nenv_line + strcspn(nenv_line, "=") + 1);
    if (nenv_val <= 0) {
        FAIL("B4.23: expected nenv > 0 from post-build snapshot");
        fprintf(stderr, "    nenv=%d\n", nenv_val);
        return 1;
    }

    code = run_cmd_capture(
        "TPB_WORKSPACE=\"" TPB_TEST_WORKSPACE "\" "
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dr -i 1 2>&1 "
        "| grep -F '[]: SHELL'",
        buf, sizeof(buf));
    if (code != 0 || strstr(buf, "[]: SHELL") == NULL) {
        FAIL("B4.23: missing key[0] STRING payload in Record Data");
        fprintf(stderr, "    exit %d output: %.400s\n", code, buf);
        return 1;
    }
    if (strstr(buf, "0x") != NULL) {
        FAIL("B4.23: key[0] must be human-readable STRING, not hex bytes");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    code = run_cmd_capture(
        "TPB_WORKSPACE=\"" TPB_TEST_WORKSPACE "\" "
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dr -i 1 2>&1 "
        "| grep -F 'application ['",
        buf, sizeof(buf));
    if (code == 0) {
        FAIL("B4.23: zero-length application header must not appear in Record Data");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }

    PASS();
    return 0;
}

static int
test_b4_24_dump_count_conflict(void)
{
    char buf[4096];
    int code = run_cmd_capture(
        "\"" TPB_TEST_TPBCLI_STR "\" database dump -dk -e -n 3 -N 3",
        buf, sizeof(buf));

    if (code == 0) {
        FAIL("B4.24: expected nonzero for -n/-N conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B4.24: missing conflict message");
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
    if (strcmp(id, "B4.17") == 0) {
        return test_b4_17_dump_domain_id_prefix();
    }
    if (strcmp(id, "B4.18") == 0) {
        return test_b4_18_dump_missing_domain();
    }
    if (strcmp(id, "B4.19") == 0) {
        return test_b4_19_dump_missing_mode();
    }
    if (strcmp(id, "B4.20") == 0) {
        return test_b4_20_dump_id_entry_conflict();
    }
    if (strcmp(id, "B4.21") == 0) {
        return test_b4_21_dump_domain_conflict();
    }
    if (strcmp(id, "B4.22") == 0) {
        return test_b4_22_dump_rtenv_entry();
    }
    if (strcmp(id, "B4.23") == 0) {
        return test_b4_23_dump_rtenv_record();
    }
    if (strcmp(id, "B4.24") == 0) {
        return test_b4_24_dump_count_conflict();
    }

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
