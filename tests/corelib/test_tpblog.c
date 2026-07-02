/**
 * @file test_tpblog.c
 * @brief Test pack A8: tpblog module unit tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "include/tpb-public.h"
#include "corelib/tpblog/tpb-printf.h"

typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

static char g_ws[512];
static char g_log_path[512];

static int
read_file_contents(const char *path, char *buf, size_t bufsz)
{
    FILE *fp;

    if (path == NULL || buf == NULL || bufsz == 0) {
        return 1;
    }
    buf[0] = '\0';
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }
    if (fread(buf, 1, bufsz - 1, fp) == 0 && ferror(fp)) {
        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}

static int
capture_stdout_call(void (*fn)(void), char *buf, size_t bufsz)
{
    int pipefd[2];
    int saved_stdout;

    if (fn == NULL || buf == NULL || bufsz == 0) {
        return 1;
    }
    buf[0] = '\0';
    if (pipe(pipefd) != 0) {
        return 1;
    }
    saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    fn();
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    {
        size_t total = 0;
        ssize_t nread;

        while (total + 1 < bufsz) {
            nread = read(pipefd[0], buf + total, bufsz - 1 - total);
            if (nread <= 0) {
                break;
            }
            total += (size_t)nread;
        }
        buf[total] = '\0';
    }
    close(pipefd[0]);
    return 0;
}

static void
fn_init_session(void)
{
    (void)tpblog_init();
}

static void
fn_print_info(void)
{
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "tpblog test line\n");
}

static void
fn_print_warn_tagged(void)
{
    tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                    TPBLOG_FLAG_TAG | TPBLOG_FLAG_TS,
                    "warn message\n");
}

static void
fn_print_column(void)
{
    const char *cells[] = { "hello", "I", "know" };
    float ratios[] = { 1.0f, 2.0f, 3.0f };

    setenv("TPBLOG_TEST_WIDTH", "80", 1);
    tpblog_printf_c(ratios, 3, 2, cells);
}

static void
fn_print_degenerate(void)
{
    const char *cells[] = { "a", "b", "c", "d", "e", "f", "g", "h" };
    float ratios[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    setenv("TPBLOG_TEST_WIDTH", "10", 1);
    tpblog_printf_c(ratios, 8, 1, cells);
}

static int
setup_workspace(void)
{
    snprintf(g_ws, sizeof(g_ws), "/tmp/tpblog_test_%d", (int)getpid());
    setenv("TPB_WORKSPACE", g_ws, 1);
    return tpb_corelib_init(g_ws);
}

static int
test_a8_1_init_session_header(void)
{
    char logbuf[4096];

    if (setup_workspace() != TPBE_SUCCESS) {
        return 1;
    }
    if (tpblog_get_filepath() == NULL) {
        return 1;
    }
    if (read_file_contents(tpblog_get_filepath(), logbuf, sizeof(logbuf)) != 0) {
        return 1;
    }
    if (strstr(logbuf, "TPBench Run Log") == NULL) {
        return 1;
    }
    if (strstr(logbuf, "Session Started:") == NULL) {
        return 1;
    }
    tpblog_cleanup();
    return 0;
}

static int
test_a8_2_log_file_append(void)
{
    char logbuf[4096];

    snprintf(g_ws, sizeof(g_ws), "/tmp/tpblog_test_%d", (int)getpid());
    mkdir(g_ws, 0700);
    snprintf(g_log_path, sizeof(g_log_path), "%s/manual_run.log", g_ws);
    setenv("TPB_LOG_FILE", g_log_path, 1);
    if (tpblog_init() != TPBE_SUCCESS) {
        return 1;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "append line\n");
    tpblog_cleanup();

    setenv("TPB_LOG_FILE", g_log_path, 1);
    if (tpblog_init() != TPBE_SUCCESS) {
        return 1;
    }
    if (read_file_contents(g_log_path, logbuf, sizeof(logbuf))) {
        return 1;
    }
    if (strstr(logbuf, "TPBench Run Log") != NULL) {
        return 1;
    }
    if (strstr(logbuf, "append line") == NULL) {
        return 1;
    }
    tpblog_cleanup();
    return 0;
}

static int
test_a8_3_dual_write(void)
{
    char outbuf[4096];
    char logbuf[4096];

    if (setup_workspace() != TPBE_SUCCESS) {
        return 1;
    }
    if (capture_stdout_call(fn_print_info, outbuf, sizeof(outbuf)) != 0) {
        return 1;
    }
    if (read_file_contents(tpblog_get_filepath(), logbuf, sizeof(logbuf)) != 0) {
        return 1;
    }
    if (strstr(outbuf, "tpblog test line") == NULL) {
        return 1;
    }
    if (strstr(logbuf, "tpblog test line") == NULL) {
        return 1;
    }
    tpblog_cleanup();
    return 0;
}

static int
test_a8_4_tag_labels(void)
{
    char outbuf[4096];

    if (setup_workspace() != TPBE_SUCCESS) {
        return 1;
    }
    if (capture_stdout_call(fn_print_warn_tagged, outbuf, sizeof(outbuf)) != 0) {
        return 1;
    }
    if (strstr(outbuf, "[WARN]") == NULL) {
        return 1;
    }
    if (strstr(outbuf, "[NOTE]") != NULL) {
        return 1;
    }
    tpblog_cleanup();
    return 0;
}

static int
test_a8_5_column_widths(void)
{
    int widths[3];

    setenv("TPBLOG_TEST_WIDTH", "24", 1);
    if (_tpblog_compute_column_widths(24, (const float[]){1, 2, 3}, 3, 2,
                                      widths) != 0) {
        return 1;
    }
    if (widths[0] != 2 || widths[1] != 5 || widths[2] != 9) {
        return 1;
    }
    return 0;
}

static int
test_a8_6_column_output(void)
{
    char outbuf[4096];

    if (setup_workspace() != TPBE_SUCCESS) {
        return 1;
    }
    if (capture_stdout_call(fn_print_column, outbuf, sizeof(outbuf)) != 0) {
        return 1;
    }
    if (strstr(outbuf, "hello") == NULL || strstr(outbuf, "know") == NULL) {
        return 1;
    }
    tpblog_cleanup();
    return 0;
}

static int
test_a8_7_degenerate_output(void)
{
    char outbuf[4096];

    if (setup_workspace() != TPBE_SUCCESS) {
        return 1;
    }
    if (capture_stdout_call(fn_print_degenerate, outbuf, sizeof(outbuf)) != 0) {
        return 1;
    }
    if (strstr(outbuf, "a") == NULL || strstr(outbuf, "h") == NULL) {
        return 1;
    }
    tpblog_cleanup();
    return 0;
}

static int
test_a8_8_snprintf_macro(void)
{
    char buf[32];

    tpblog_snprintf(buf, sizeof(buf), "value=%d", 42);
    if (strcmp(buf, "value=42") != 0) {
        return 1;
    }
    return 0;
}

static test_case_t cases[] = {
    { "A8.1", "init_session_header", test_a8_1_init_session_header },
    { "A8.2", "log_file_append", test_a8_2_log_file_append },
    { "A8.3", "dual_write", test_a8_3_dual_write },
    { "A8.4", "tag_labels", test_a8_4_tag_labels },
    { "A8.5", "column_widths", test_a8_5_column_widths },
    { "A8.6", "column_output", test_a8_6_column_output },
    { "A8.7", "degenerate_output", test_a8_7_degenerate_output },
    { "A8.8", "snprintf_macro", test_a8_8_snprintf_macro },
};

static int
run_pack_local(const char *pack, test_case_t *list, int n, const char *filter)
{
    int failures = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (filter != NULL && strcmp(filter, list[i].id) != 0) {
            continue;
        }
        if (list[i].func() != 0) {
            printf("[%s] %s FAIL\n", list[i].id, list[i].name);
            failures++;
        } else {
            printf("[%s] %s PASS\n", list[i].id, list[i].name);
        }
    }
    (void)pack;
    return failures;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    int failures = run_pack_local("A8", cases,
                                  (int)(sizeof(cases) / sizeof(cases[0])),
                                  filter);
    return failures;
}
