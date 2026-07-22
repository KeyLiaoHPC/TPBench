/*
 * test-cli-task-ls.c
 * Pack B7: `tpbcli task ls|list` behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "test-cli-task-fixture.h"
#include "tpb-public.h"

static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

static int
test_b7_1_missing_subcmd(void)
{
    char buf[4096];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code == 0) {
        FAIL("B7.1: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "ls") == NULL || strstr(buf, "list") == NULL) {
        FAIL("B7.1: expected ls|list hint");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_2_task_help(void)
{
    char buf[8192];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("-h", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.2: expected exit 0");
        return 1;
    }
    if (strstr(buf, "ls") == NULL || strstr(buf, "list") == NULL ||
        strstr(buf, "get-result") == NULL) {
        FAIL("B7.2: missing task subcommand overview");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_3_ls_help(void)
{
    char buf[4096];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("ls -h", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.3: expected exit 0");
        return 1;
    }
    if (strstr(buf, "Usage:") == NULL || strstr(buf, "-n") == NULL ||
        strstr(buf, "-N") == NULL || strstr(buf, "--filter") == NULL) {
        FAIL("B7.3: missing ls usage/options");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_4_list_alias(void)
{
    char buf[4096];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("list", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.4: expected exit 0 for list alias");
        fprintf(stderr, "    exit %d\n", code);
        return 1;
    }
    if (strstr(buf, "Start Time (Local)") == NULL) {
        FAIL("B7.4: missing table header from list alias");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_5_empty_workspace(void)
{
    char buf[4096];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("ls", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.5: expected exit 0 on empty workspace");
        return 1;
    }
    if (strstr(buf, "shown 0 results") == NULL) {
        FAIL("B7.5: expected zero shown results");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    if (strstr(buf, "RIDMAP not updated") == NULL) {
        FAIL("B7.5: expected RIDMAP not updated message");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_6_entrypoint_only(void)
{
    char buf[8192];
    unsigned char cap[20];
    int code;
    int k;

    tpb_test_task_fixture_setup();
    for (k = 0; k < 2; k++) {
        if (k > 0) {
            tpb_test_task_sleep_sep();
        }
        if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
            tpb_test_task_fixture_cleanup();
            FAIL("B7.6: seed standalone failed");
            return 1;
        }
    }
    if (tpb_test_task_seed_capsule(3, cap) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.6: seed capsule failed");
        return 1;
    }
    code = tpb_test_task_run_cmd("ls", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.6: ls failed");
        return 1;
    }
    if (strstr(buf, "Found 3 in 3 task records") == NULL) {
        FAIL("B7.6: expected 3 entrypoints (2 standalone + capsule)");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_7_count_limits(void)
{
    char buf[8192];
    int code;
    int k;

    tpb_test_task_fixture_setup();
    for (k = 0; k < 5; k++) {
        if (k > 0) {
            tpb_test_task_sleep_sep();
        }
        if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
            tpb_test_task_fixture_cleanup();
            FAIL("B7.7: seed failed");
            return 1;
        }
    }

    code = tpb_test_task_run_cmd("ls -n 2", buf, sizeof(buf));
    if (code != 0 || strstr(buf, "shown 2 results") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.7: -n 2 expected 2 shown");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    code = tpb_test_task_run_cmd("ls -N 2", buf, sizeof(buf));
    if (code != 0 || strstr(buf, "shown 2 results") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.7: -N 2 expected 2 shown");
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    code = tpb_test_task_run_cmd("ls -n 0", buf, sizeof(buf));
    if (code != 0 || strstr(buf, "shown 5 results") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.7: -n 0 expected all 5 shown");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }

    tpb_test_task_fixture_cleanup();
    PASS();
    return 0;
}

static int
test_b7_8_exit_code_filter(void)
{
    char buf[8192];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.8: seed failed");
        return 1;
    }
    tpb_test_task_sleep_sep();
    if (tpb_test_task_write_standalone(7, NULL) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.8: seed failed");
        return 1;
    }

    code = tpb_test_task_run_cmd("ls -f exit_code=7", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.8: ls with exit_code filter failed");
        return 1;
    }
    if (strstr(buf, "Found 1 in 2") == NULL ||
        strstr(buf, "shown 1 results") == NULL) {
        FAIL("B7.8: expected one matching exit_code=7");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    if (strstr(buf, " 7 ") == NULL && strstr(buf, "\t7\t") == NULL) {
        /* Exit column should show 7 somewhere in the data row */
        if (strstr(buf, "7") == NULL) {
            FAIL("B7.8: missing exit code 7 in output");
            return 1;
        }
    }
    PASS();
    return 0;
}

static int
test_b7_9_reject_bad_filters(void)
{
    char buf[4096];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("ls -f duration=1", buf, sizeof(buf));
    if (code == 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.9: duration filter should fail");
        return 1;
    }
    if (strstr(buf, "not valid for task ls") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.9: missing duration rejection message");
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    code = tpb_test_task_run_cmd("ls -f subrank=0", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code == 0) {
        FAIL("B7.9: subrank filter should fail");
        return 1;
    }
    if (strstr(buf, "not valid for task ls") == NULL) {
        FAIL("B7.9: missing subrank rejection message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_10_local_time_header(void)
{
    char buf[8192];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.10: seed failed");
        return 1;
    }
    code = tpb_test_task_run_cmd("ls", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.10: ls failed");
        return 1;
    }
    if (strstr(buf, "Start Time (Local)") == NULL) {
        FAIL("B7.10: missing Start Time (Local) header");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    if (strstr(buf, "Start Time (UTC)") != NULL) {
        FAIL("B7.10: should not use UTC header");
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_11_capsule_subproc(void)
{
    char buf[8192];
    unsigned char cap[20];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_seed_capsule(4, cap) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.11: seed capsule failed");
        return 1;
    }
    code = tpb_test_task_run_cmd("ls", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.11: ls failed");
        return 1;
    }
    if (strstr(buf, "Found 1 in 1") == NULL ||
        strstr(buf, "shown 1 results") == NULL) {
        FAIL("B7.11: expected single capsule entrypoint");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    if (strstr(buf, "Subp") == NULL) {
        FAIL("B7.11: missing Subproc column (may wrap as Subp-)");
        return 1;
    }
    {
        const char *summary = strstr(buf, "shown 1 results");
        const char *row = summary ? summary : buf;
        const char *p = buf;

        while ((p = strstr(p, "\n")) != NULL) {
            p++;
            if (p[0] >= '0' && p[0] <= '9' && p[1] == ' ') {
                row = p;
                break;
            }
        }
        if (strstr(row, " 4 ") == NULL && strstr(row, "  4 ") == NULL) {
            FAIL("B7.11: expected Subproc count 4 in data row");
            fprintf(stderr, "    row: %.200s\n", row);
            return 1;
        }
    }
    PASS();
    return 0;
}

static int
test_b7_12_ridmap_update(void)
{
    char buf[8192];
    unsigned char (*ids)[20] = NULL;
    int nids = 0;
    int code;
    int k;

    tpb_test_task_fixture_setup();
    for (k = 0; k < 3; k++) {
        if (k > 0) {
            tpb_test_task_sleep_sep();
        }
        if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
            tpb_test_task_fixture_cleanup();
            FAIL("B7.12: seed failed");
            return 1;
        }
    }
    code = tpb_test_task_run_cmd("ls -n 2", buf, sizeof(buf));
    if (code != 0 || strstr(buf, "RIDMAP updated") == NULL) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.12: expected RIDMAP updated");
        return 1;
    }
    if (tpb_test_task_ridmap_read(&ids, &nids) != 0 || nids != 2) {
        tpb_test_task_ridmap_free(ids);
        tpb_test_task_fixture_cleanup();
        FAIL("B7.12: RIDMAP should contain 2 ids");
        return 1;
    }
    tpb_test_task_ridmap_free(ids);
    tpb_test_task_fixture_cleanup();
    PASS();
    return 0;
}

static int
test_b7_13_ridmap_zero_preserve(void)
{
    char path[1024];
    char before[256];
    char after[256];
    unsigned char (*ids)[20] = NULL;
    int nids = 0;
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.13: seed failed");
        return 1;
    }
    code = tpb_test_task_run_cmd("ls", NULL, 0);
    if (code != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.13: initial ls failed");
        return 1;
    }
    if (tpb_test_task_ridmap_path(path, sizeof(path)) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.13: ridmap path failed");
        return 1;
    }
  {
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            tpb_test_task_fixture_cleanup();
            FAIL("B7.13: missing ridmap after ls");
            return 1;
        }
        if (fgets(before, sizeof(before), fp) == NULL) {
            fclose(fp);
            tpb_test_task_fixture_cleanup();
            FAIL("B7.13: empty ridmap after ls");
            return 1;
        }
        fclose(fp);
    }

    code = tpb_test_task_run_cmd("ls -f exit_code=99", NULL, 0);
    if (code != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.13: filtered ls should exit 0");
        return 1;
    }
    if (tpb_test_task_ridmap_read(&ids, &nids) != 0 || nids != 1) {
        tpb_test_task_ridmap_free(ids);
        tpb_test_task_fixture_cleanup();
        FAIL("B7.13: ridmap entry count should be preserved");
        return 1;
    }
    tpb_test_task_ridmap_free(ids);

  {
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            tpb_test_task_fixture_cleanup();
            FAIL("B7.13: ridmap missing after zero-result ls");
            return 1;
        }
        if (fgets(after, sizeof(after), fp) == NULL) {
            fclose(fp);
            tpb_test_task_fixture_cleanup();
            FAIL("B7.13: ridmap cleared after zero-result ls");
            return 1;
        }
        fclose(fp);
    }
    if (strcmp(before, after) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.13: ridmap content changed on zero results");
        return 1;
    }

    tpb_test_task_fixture_cleanup();
    PASS();
    return 0;
}

static int
test_b7_14_count_conflict(void)
{
    char buf[4096];
    int code;

    tpb_test_task_fixture_setup();
    code = tpb_test_task_run_cmd("ls -n 3 -N 3", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code == 0) {
        FAIL("B7.14: expected nonzero for -n/-N conflict");
        return 1;
    }
    if (strstr(buf, "conflict") == NULL) {
        FAIL("B7.14: missing conflict message");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b7_15_kernel_id_filter(void)
{
    char buf[8192];
    unsigned char kid[20];
    char prefix[12];
    int code;

    tpb_test_task_fixture_setup();
    if (tpb_raf_hex_to_id(
            "f1e2d3c4b5a6f1e2d3c4b5a6f1e2d3c4b5a6f1e2", kid) != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.15: kernel id parse failed");
        return 1;
    }
    if (tpb_test_task_seed_kernel_name("stream") != 0) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.15: kernel seed failed");
        return 1;
    }
    if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.15: task seed failed");
        return 1;
    }
    tpb_test_task_sleep_sep();
    if (tpb_test_task_write_standalone(0, NULL) != TPBE_SUCCESS) {
        tpb_test_task_fixture_cleanup();
        FAIL("B7.15: task seed failed");
        return 1;
    }
    snprintf(prefix, sizeof(prefix), "f1e2d3");
    code = tpb_test_task_run_cmd("ls -f kernel_id=f1e2d3", buf, sizeof(buf));
    tpb_test_task_fixture_cleanup();

    if (code != 0) {
        FAIL("B7.15: kernel_id filter failed");
        return 1;
    }
    if (strstr(buf, "Found 2 in 2") == NULL ||
        strstr(buf, "shown 2 results") == NULL) {
        FAIL("B7.15: expected both tasks to match kernel_id prefix");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    (void)prefix;
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *id = (argc >= 2) ? argv[1] : "";

    g_pass = 0;
    g_fail = 0;

    if (strcmp(id, "B7.1") == 0) {
        return test_b7_1_missing_subcmd();
    }
    if (strcmp(id, "B7.2") == 0) {
        return test_b7_2_task_help();
    }
    if (strcmp(id, "B7.3") == 0) {
        return test_b7_3_ls_help();
    }
    if (strcmp(id, "B7.4") == 0) {
        return test_b7_4_list_alias();
    }
    if (strcmp(id, "B7.5") == 0) {
        return test_b7_5_empty_workspace();
    }
    if (strcmp(id, "B7.6") == 0) {
        return test_b7_6_entrypoint_only();
    }
    if (strcmp(id, "B7.7") == 0) {
        return test_b7_7_count_limits();
    }
    if (strcmp(id, "B7.8") == 0) {
        return test_b7_8_exit_code_filter();
    }
    if (strcmp(id, "B7.9") == 0) {
        return test_b7_9_reject_bad_filters();
    }
    if (strcmp(id, "B7.10") == 0) {
        return test_b7_10_local_time_header();
    }
    if (strcmp(id, "B7.11") == 0) {
        return test_b7_11_capsule_subproc();
    }
    if (strcmp(id, "B7.12") == 0) {
        return test_b7_12_ridmap_update();
    }
    if (strcmp(id, "B7.13") == 0) {
        return test_b7_13_ridmap_zero_preserve();
    }
    if (strcmp(id, "B7.14") == 0) {
        return test_b7_14_count_conflict();
    }
    if (strcmp(id, "B7.15") == 0) {
        return test_b7_15_kernel_id_filter();
    }

    fprintf(stderr, "Unknown case id: %s\n", id);
    return 2;
}
