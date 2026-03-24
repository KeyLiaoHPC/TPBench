/*
 * tpb_run_pli.c
 * Test pack A2: Unit tests for tpb_run_pli (PLI kernel runner).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mock_kernel.h"
#include "mock_dynloader.h"

#define MOCK_SCRIPT_DIR "/tmp/tpbench_test_pli"

static char path_success[512];
static char path_fail[512];
static char path_signal[512];
static char path_capture_kid[512];
static char path_kernel_id_out[512];

static int g_setup_done = 0;
static char g_pli_workspace[512];

static int
setup_pli_workspace(void)
{
    snprintf(g_pli_workspace, sizeof(g_pli_workspace),
             "/tmp/tpbench_pli_ws_%d", (int)getpid());
    if (mkdir(g_pli_workspace, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (setenv("TPB_WORKSPACE", g_pli_workspace, 1) != 0) {
        return -1;
    }
    return 0;
}

static int
write_script(const char *path, const char *body)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "#!/bin/sh\n%s\n", body);
    fclose(f);
    chmod(path, 0755);
    return 0;
}

static int
create_mock_scripts(void)
{
    snprintf(path_success, sizeof(path_success),
             "%s/mock_pli_success.sh", MOCK_SCRIPT_DIR);
    snprintf(path_fail, sizeof(path_fail),
             "%s/mock_pli_fail.sh", MOCK_SCRIPT_DIR);
    snprintf(path_signal, sizeof(path_signal),
             "%s/mock_pli_signal.sh", MOCK_SCRIPT_DIR);
    snprintf(path_capture_kid, sizeof(path_capture_kid),
             "%s/mock_pli_capture_kid.sh", MOCK_SCRIPT_DIR);
    snprintf(path_kernel_id_out, sizeof(path_kernel_id_out),
             "%s/kernel_id.out", MOCK_SCRIPT_DIR);

    mkdir(MOCK_SCRIPT_DIR, 0755);

    if (write_script(path_success, "exit 0")) return -1;
    if (write_script(path_fail, "exit 42")) return -1;
    if (write_script(path_signal, "kill -9 $$")) return -1;
    if (write_script(path_capture_kid,
                     "printf \"%s\" \"$TPB_KERNEL_ID\" > /tmp/tpbench_test_pli/kernel_id.out\nexit 0")) {
        return -1;
    }
    return 0;
}

static void
cleanup_mock_scripts(void)
{
    char cmd[640];

    unlink(path_success);
    unlink(path_fail);
    unlink(path_signal);
    unlink(path_capture_kid);
    unlink(path_kernel_id_out);
    rmdir(MOCK_SCRIPT_DIR);
    if (g_pli_workspace[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "rm -rf %s", g_pli_workspace);
        (void)system(cmd);
        g_pli_workspace[0] = '\0';
    }
}

static int
ensure_setup(void)
{
    if (!g_setup_done) {
        int err;

        g_pli_workspace[0] = '\0';
        if (setup_pli_workspace() != 0) {
            fprintf(stderr, "FATAL: setup_pli_workspace failed\n");
            return -1;
        }

        err = mock_setup_driver();
        if (err) {
            fprintf(stderr, "FATAL: mock_setup_driver failed with %d\n", err);
            return err;
        }
        err = create_mock_scripts();
        if (err) {
            fprintf(stderr, "FATAL: failed to create mock PLI scripts\n");
            return err;
        }
        g_setup_done = 1;
    }
    return 0;
}

/* A2.1: NULL handle returns TPBE_NULLPTR_ARG */
static int
test_null_handle(void)
{
    int err = tpb_run_pli(NULL);
    return (err == TPBE_NULLPTR_ARG) ? 0 : 1;
}

/* A2.2: Missing executable returns TPBE_KERNEL_INCOMPLETE */
static int
test_missing_exec(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_pli_test", &hdl);
    if (err) return 1;

    mock_dl_set_exec_path(NULL);
    mock_dl_set_complete(0);

    err = tpb_run_pli(&hdl);
    free(hdl.argpack.args);
    tpb_free_kernel(&hdl.kernel);
    return (err == TPBE_KERNEL_INCOMPLETE) ? 0 : 1;
}

/* A2.3: Child exits 0, function returns success */
static int
test_basic_success(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_pli_test", &hdl);
    if (err) return 1;

    mock_dl_set_exec_path(path_success);
    mock_dl_set_complete(1);

    err = tpb_run_pli(&hdl);
    free(hdl.argpack.args);
    tpb_free_kernel(&hdl.kernel);
    return (err == 0) ? 0 : 1;
}

/* A2.4: Child exits with non-zero code, function returns that code */
static int
test_child_nonzero_exit(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_pli_test", &hdl);
    if (err) return 1;

    mock_dl_set_exec_path(path_fail);
    mock_dl_set_complete(1);

    err = tpb_run_pli(&hdl);
    free(hdl.argpack.args);
    tpb_free_kernel(&hdl.kernel);
    return (err == 42) ? 0 : 1;
}

/* A2.5: Child killed by signal, function returns TPBE_KERN_ARG_FAIL */
static int
test_child_signaled(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_pli_test", &hdl);
    if (err) return 1;

    mock_dl_set_exec_path(path_signal);
    mock_dl_set_complete(1);

    err = tpb_run_pli(&hdl);
    free(hdl.argpack.args);
    tpb_free_kernel(&hdl.kernel);
    /* Signal death: either TPBE_KERN_ARG_FAIL or a non-zero exit code
       (shell may translate SIGKILL to exit 137) */
    return (err != 0) ? 0 : 1;
}

/* A2.6: TPB_KERNEL_ID is injected into child process env */
static int
test_kernel_id_env(void)
{
    FILE *fp;
    char got_hex[128];
    char exp_hex[41];
    unsigned char kernel_id[20];
    size_t nread;
    int err;
    tpb_k_rthdl_t hdl;

    if (ensure_setup()) return 1;
    err = mock_build_handle("mock_pli_test", &hdl);
    if (err) return 1;

    for (int i = 0; i < 20; i++) {
        kernel_id[i] = (unsigned char)(i + 1);
    }
    memcpy(hdl.kernel.info.kernel_id, kernel_id, 20);
    tpb_rawdb_id_to_hex(kernel_id, exp_hex);

    unlink(path_kernel_id_out);
    mock_dl_set_exec_path(path_capture_kid);
    mock_dl_set_complete(1);
    err = tpb_run_pli(&hdl);
    free(hdl.argpack.args);
    tpb_free_kernel(&hdl.kernel);
    if (err != 0) {
        return 1;
    }

    fp = fopen(path_kernel_id_out, "r");
    if (fp == NULL) {
        return 1;
    }
    nread = fread(got_hex, 1, sizeof(got_hex) - 1, fp);
    fclose(fp);
    got_hex[nread] = '\0';

    return (strcmp(got_hex, exp_hex) == 0) ? 0 : 1;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    atexit(cleanup_mock_scripts);

    test_case_t cases[] = {
        { "A2.1", "null_handle",        test_null_handle        },
        { "A2.2", "missing_exec",       test_missing_exec       },
        { "A2.3", "basic_success",      test_basic_success      },
        { "A2.4", "child_nonzero_exit", test_child_nonzero_exit },
        { "A2.5", "child_signaled",     test_child_signaled     },
        { "A2.6", "kernel_id_env",      test_kernel_id_env      },
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int fail = run_pack("A2", cases, n, filter);
    return (fail > 0) ? 1 : 0;
}
