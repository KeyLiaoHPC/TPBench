/*
 * test-cli-rtenv.c
 * Pack B6: `tpbcli rtenv` behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "tpb-public.h"

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

static int g_pass;
static int g_fail;
static char g_ws[512];

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

static int
run_cmd_capture(const char *cmd, char *outbuf, size_t outbuf_sz)
{
    char full[16384];
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

static void
setup_workspace(void)
{
    int32_t id;

    snprintf(g_ws, sizeof(g_ws), "/tmp/tpb_rtenv_cli_%d", (int)getpid());
    tpb_raf_init_workspace(g_ws);
    (void)tpb_rtenv_ensure_base_env(g_ws, &id);
}

static void
cleanup_workspace(void)
{
    char cmd[640];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_ws);
    (void)system(cmd);
}

static int
count_rtenv_entries(void)
{
    tpb_raf_rtenv_entry_t *ents = NULL;
    int n = 0;

    if (tpb_raf_entry_list_rtenv(g_ws, &ents, &n) != TPBE_SUCCESS) {
        return -1;
    }
    free(ents);
    return n;
}

static int
test_b6_1_missing_subcmd(void)
{
    char buf[4096];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" rtenv", buf, sizeof(buf));
    if (code == 0) {
        FAIL("B6.1: expected nonzero exit");
        return 1;
    }
    if (strstr(buf, "new") == NULL || strstr(buf, "list") == NULL ||
        strstr(buf, "show") == NULL || strstr(buf, "load") == NULL) {
        FAIL("B6.1: expected subcommand hint");
        fprintf(stderr, "    output: %.400s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b6_2_rtenv_help(void)
{
    char buf[8192];
    int code = run_cmd_capture("\"" TPB_TEST_TPBCLI_STR "\" rtenv -h", buf,
                               sizeof(buf));

    if (code != 0) {
        FAIL("B6.2: expected exit 0");
        return 1;
    }
    if (strstr(buf, "new") == NULL || strstr(buf, "list") == NULL ||
        strstr(buf, "show") == NULL || strstr(buf, "load") == NULL) {
        FAIL("B6.2: missing subcommands");
        fprintf(stderr, "    output: %.500s\n", buf);
        return 1;
    }
    PASS();
    return 0;
}

static int
test_b6_3_new_output_template(void)
{
    char buf[4096];
    char tmpl[640];
    char cmd[2048];
    int before;
    int after;
    int code;

    setup_workspace();
    before = count_rtenv_entries();
    snprintf(tmpl, sizeof(tmpl), "%s/rtenv_template.txt", g_ws);
    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" rtenv new -o '%s' "
             "--name tmpl-env --note t",
             g_ws, tmpl);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    after = count_rtenv_entries();
    if (code != 0) {
        FAIL("B6.3: expected exit 0");
        fprintf(stderr, "    output: %.400s\n", buf);
        cleanup_workspace();
        return 1;
    }
    if (before != after) {
        FAIL("B6.3: -o should not create rafdb entry");
        cleanup_workspace();
        return 1;
    }
    {
        FILE *fp = fopen(tmpl, "r");
        if (fp == NULL) {
            FAIL("B6.3: template file missing");
            cleanup_workspace();
            return 1;
        }
        fclose(fp);
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_4_new_from_file(void)
{
    char buf[4096];
    char tmpl[640];
    char cmd[4096];
    FILE *fp;
    int before;
    int after;
    int code;

    setup_workspace();
    before = count_rtenv_entries();
    snprintf(tmpl, sizeof(tmpl), "%s/rtenv_new.txt", g_ws);
    fp = fopen(tmpl, "w");
    if (fp == NULL) {
        FAIL("B6.4: cannot write template");
        cleanup_workspace();
        return 1;
    }
    fprintf(fp,
            "--name 'from-file-env'\n"
            "--note 'nf'\n"
            "--add-application --name='gcc' --version='13.2' --note='cc'\n");
    fclose(fp);

    snprintf(cmd, sizeof(cmd),
             "TPB_RTENV_ID=0 TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" "
             "rtenv new -f '%s'",
             g_ws, tmpl);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    after = count_rtenv_entries();
    if (code != 0 || after != before + 1) {
        FAIL("B6.4: expected one new entry");
        fprintf(stderr, "    code=%d before=%d after=%d output: %.400s\n",
                code, before, after, buf);
        cleanup_workspace();
        return 1;
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_5_new_inline(void)
{
    char buf[4096];
    char cmd[4096];
    int before;
    int after;
    int code;

    setup_workspace();
    before = count_rtenv_entries();
    snprintf(cmd, sizeof(cmd),
             "TPB_RTENV_ID=0 TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" "
             "rtenv new --name inline-env --note inline "
             "--add-application --name gcc --version 13.2 --note cc "
             "--append-variable --name LD_LIBRARY_PATH --value /opt/lib",
             g_ws);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    after = count_rtenv_entries();
    if (code != 0 || after != before + 1) {
        FAIL("B6.5: inline new failed");
        fprintf(stderr, "    code=%d before=%d after=%d output: %.500s\n",
                code, before, after, buf);
        cleanup_workspace();
        return 1;
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_6_new_bad_template(void)
{
    char buf[4096];
    char tmpl[640];
    char cmd[4096];
    FILE *fp;
    int before;
    int after;
    int code;

    setup_workspace();
    before = count_rtenv_entries();
    snprintf(tmpl, sizeof(tmpl), "%s/rtenv_bad.txt", g_ws);
    fp = fopen(tmpl, "w");
    fprintf(fp, "--name 'bad-env'\nTHIS_IS_NOT_A_VALID_LINE\n");
    fclose(fp);
    snprintf(cmd, sizeof(cmd),
             "TPB_RTENV_ID=0 TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" "
             "rtenv new -f '%s'",
             g_ws, tmpl);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    after = count_rtenv_entries();
    if (code == 0) {
        FAIL("B6.6: expected failure");
        cleanup_workspace();
        return 1;
    }
    if (before != after) {
        FAIL("B6.6: bad template must not write rafdb");
        cleanup_workspace();
        return 1;
    }
    if (strstr(buf, "line 2") == NULL && strstr(buf, "line") == NULL) {
        FAIL("B6.6: expected line number in error");
        fprintf(stderr, "    output: %.400s\n", buf);
        cleanup_workspace();
        return 1;
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_7_list_activated(void)
{
    char buf[8192];
    char cmd[1024];
    int code;

    setup_workspace();
    unsetenv("TPB_RTENV_ID");
    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" rtenv list",
             g_ws);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B6.7: list failed");
        cleanup_workspace();
        return 1;
    }
    if (strstr(buf, "Activated runtime environment ID: 0") == NULL) {
        FAIL("B6.7: expected activated id 0 from base_id");
        fprintf(stderr, "    output: %.400s\n", buf);
        cleanup_workspace();
        return 1;
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_8_show_tables(void)
{
    char buf[8192];
    char cmd[2048];
    int code;

    setup_workspace();
    snprintf(cmd, sizeof(cmd),
             "TPB_RTENV_ID=0 TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" "
             "rtenv new --name show-me --note sn "
             "--add-application --name app1 --version 1.0 --note an "
             "--variable --name FOO --value bar",
             g_ws);
    (void)run_cmd_capture(cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" rtenv show -i 1",
             g_ws);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B6.8: show failed");
        cleanup_workspace();
        return 1;
    }
    if (strstr(buf, "Applications (merged):") == NULL ||
        strstr(buf, "Environment variables (merged):") == NULL ||
        strstr(buf, "app1") == NULL || strstr(buf, "FOO") == NULL) {
        FAIL("B6.8: missing merged tables");
        fprintf(stderr, "    output: %.600s\n", buf);
        cleanup_workspace();
        return 1;
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_9_load_shell_fragment(void)
{
    char buf[8192];
    char tmpl[640];
    char cmd[4096];
    FILE *fp;
    int code;

    setup_workspace();
    snprintf(tmpl, sizeof(tmpl), "%s/rtenv_load.txt", g_ws);
    fp = fopen(tmpl, "w");
    fprintf(fp,
            "--name 'load-env'\n"
            "--note 'ld'\n"
            "--variable --name='GOOD_VAR' --value='1'\n"
            "--unset-variable --name='OLD_VAR'\n"
            "--variable --name='9BAD' --value='x'\n");
    fclose(fp);

    snprintf(cmd, sizeof(cmd),
             "TPB_RTENV_ID=0 TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" "
             "rtenv new -f '%s'",
             g_ws, tmpl);
    (void)run_cmd_capture(cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" rtenv load -i 1",
             g_ws);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("B6.9: invalid key should fail");
        cleanup_workspace();
        return 1;
    }

    snprintf(tmpl, sizeof(tmpl), "%s/rtenv_load_ok.txt", g_ws);
    fp = fopen(tmpl, "w");
    fprintf(fp,
            "--name 'load-ok'\n"
            "--unset-variable --name='OLD_VAR'\n"
            "--variable --name='GOOD_VAR' --value='1'\n");
    fclose(fp);
    snprintf(cmd, sizeof(cmd),
             "TPB_RTENV_ID=0 TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" "
             "rtenv new -f '%s'",
             g_ws, tmpl);
    (void)run_cmd_capture(cmd, buf, sizeof(buf));

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" rtenv load -i 2",
             g_ws);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        FAIL("B6.9: load failed");
        fprintf(stderr, "    output: %.500s\n", buf);
        cleanup_workspace();
        return 1;
    }
    if (strstr(buf, "export TPB_RTENV_ID=") == NULL ||
        strstr(buf, "unset OLD_VAR") == NULL) {
        FAIL("B6.9: missing export/unset fragment");
        fprintf(stderr, "    output: %.500s\n", buf);
        cleanup_workspace();
        return 1;
    }
    PASS();
    cleanup_workspace();
    return 0;
}

static int
test_b6_10_new_requires_active(void)
{
    char buf[4096];
    char ws_empty[512];
    char cmd[2048];
    int code;

    snprintf(ws_empty, sizeof(ws_empty), "/tmp/tpb_rtenv_empty_%d", (int)getpid());
    tpb_raf_init_workspace(ws_empty);

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR "\" rtenv new "
             "--name orphan --note x "
             "--variable --name A --value B",
             ws_empty);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code == 0) {
        FAIL("B6.10: expected failure without active/inherit");
        cleanup_workspace();
        return 1;
    }
    if (strstr(buf, "inherit") == NULL && strstr(buf, "active") == NULL) {
        FAIL("B6.10: expected active/inherit error");
        fprintf(stderr, "    output: %.400s\n", buf);
    }
    {
        char rm[640];
        snprintf(rm, sizeof(rm), "rm -rf '%s'", ws_empty);
        (void)system(rm);
    }
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    int fail = 0;

    if (filter == NULL || strcmp(filter, "B6.1") == 0) {
        fail += test_b6_1_missing_subcmd();
    }
    if (filter == NULL || strcmp(filter, "B6.2") == 0) {
        fail += test_b6_2_rtenv_help();
    }
    if (filter == NULL || strcmp(filter, "B6.3") == 0) {
        fail += test_b6_3_new_output_template();
    }
    if (filter == NULL || strcmp(filter, "B6.4") == 0) {
        fail += test_b6_4_new_from_file();
    }
    if (filter == NULL || strcmp(filter, "B6.5") == 0) {
        fail += test_b6_5_new_inline();
    }
    if (filter == NULL || strcmp(filter, "B6.6") == 0) {
        fail += test_b6_6_new_bad_template();
    }
    if (filter == NULL || strcmp(filter, "B6.7") == 0) {
        fail += test_b6_7_list_activated();
    }
    if (filter == NULL || strcmp(filter, "B6.8") == 0) {
        fail += test_b6_8_show_tables();
    }
    if (filter == NULL || strcmp(filter, "B6.9") == 0) {
        fail += test_b6_9_load_shell_fragment();
    }
    if (filter == NULL || strcmp(filter, "B6.10") == 0) {
        fail += test_b6_10_new_requires_active();
    }

    printf("B6 pack: %d passed, %d failed\n", g_pass, g_fail);
    return (fail > 0) ? 1 : 0;
}
