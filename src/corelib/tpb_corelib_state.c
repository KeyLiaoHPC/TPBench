/*
 * tpb_corelib_state.c
 * Mandatory corelib initialization and process-wide workspace state.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tpb-types.h"
#include "tpb-io.h"
#include "rafdb/tpb-raf-types.h"
#include "tpb_corelib_state.h"

/* Local Function Prototypes */
static int resolve_workspace_root(const char *override, char *out, size_t outlen);

static int _s_corelib_initialized;
static int _s_caller;
static char _s_workspace_path[PATH_MAX];

int
tpb_corelib_workspace_ready(void)
{
    return _s_corelib_initialized;
}

void
_tpb_workspace_path_set(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        _s_workspace_path[0] = '\0';
        return;
    }
    snprintf(_s_workspace_path, sizeof(_s_workspace_path), "%s", path);
}

const char *
_tpb_workspace_path_get(void)
{
    return _s_workspace_path;
}

void
_tpb_caller_set(int caller)
{
    _s_caller = caller;
}

int
_tpb_caller_get(void)
{
    return _s_caller;
}

/*
 * Resolve workspace directory: override, then $TPB_WORKSPACE, then $HOME/.tpbench.
 */
static int
resolve_workspace_root(const char *override, char *out, size_t outlen)
{
    const char *env_ws;
    const char *home;

    if (out == NULL || outlen == 0) {
        return TPBE_NULLPTR_ARG;
    }

    if (override != NULL && override[0] != '\0') {
        if (strlen(override) >= outlen) {
            return TPBE_FILE_IO_FAIL;
        }
        snprintf(out, outlen, "%s", override);
        return TPBE_SUCCESS;
    }

    env_ws = getenv("TPB_WORKSPACE");
    if (env_ws != NULL && env_ws[0] != '\0') {
        if (strlen(env_ws) >= outlen) {
            return TPBE_FILE_IO_FAIL;
        }
        snprintf(out, outlen, "%s", env_ws);
        return TPBE_SUCCESS;
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return TPBE_FILE_IO_FAIL;
    }
    if (snprintf(out, outlen, "%s/%s", home, TPB_RAF_DEFAULT_DIR) >= (int)outlen) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;
}

/*
 * Shared corelib startup: resolve workspace, rafdb layout, run log, caller tag.
 * For tpbcli only, clears TPB_LOG_FILE so a new timestamped log is always created.
 * MPI sub rank skips version/workspace/caller console lines but still initializes
 * workspace state.
 */
int
_tpb_init_corelib(const char *tpb_workspace_path, int caller_after)
{
    char resolved[PATH_MAX];
    int err;
    int silent;

    if (_s_corelib_initialized) {
        return TPBE_ILLEGAL_CALL;
    }

    silent = (caller_after == TPB_CORELIB_CTX_CALLER_KERNEL_MPI_SUB_RANK);

    if (caller_after == TPB_CORELIB_CTX_CALLER_TPBCLI) {
        unsetenv(TPB_LOG_FILE_ENV);
    }

    err = resolve_workspace_root(tpb_workspace_path, resolved, sizeof(resolved));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (!silent) {
        const char *who;

        tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench v%g\n", TPB_VERSION);
        tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench workspace: %s\n", resolved);
        if (caller_after == TPB_CORELIB_CTX_CALLER_TPBCLI) {
            who = "tpbcli";
        } else if (caller_after == TPB_CORELIB_CTX_CALLER_KERNEL) {
            who = "kernel";
        } else if (caller_after == TPB_CORELIB_CTX_CALLER_KERNEL_MPI_MAIN_RANK) {
            who = "MPI kernel";
        } else {
            who = "unknown";
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench is called by %s\n", who);
    }

    _tpb_workspace_path_set(resolved);

    err = tpb_raf_init_workspace(resolved);
    if (err != TPBE_SUCCESS) {
        _tpb_workspace_path_set("");
        return err;
    }

    err = tpb_log_init();
    if (err != TPBE_SUCCESS) {
        _tpb_workspace_path_set("");
        return err;
    }

    _tpb_caller_set(caller_after);
    _s_corelib_initialized = 1;
    return TPBE_SUCCESS;
}

int
tpb_corelib_init(const char *tpb_workspace_path)
{
    return _tpb_init_corelib(tpb_workspace_path,
        TPB_CORELIB_CTX_CALLER_TPBCLI);
}

int
tpb_k_corelib_init(const char *tpb_workspace_path)
{
    return _tpb_init_corelib(tpb_workspace_path,
        TPB_CORELIB_CTX_CALLER_KERNEL);
}
