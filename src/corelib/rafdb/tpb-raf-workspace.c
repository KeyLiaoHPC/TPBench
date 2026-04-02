/*
 * tpb-raf-workspace.c
 * Workspace resolution, initialization, and config.json management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "../tpb-types.h"
#include "../tpb_corelib_state.h"
#include "tpb-raf-types.h"

/* Local Function Prototypes */
static int _sf_dir_exists(const char *path);
static int _sf_file_exists(const char *path);
static int _sf_mkdir_recursive(const char *path);
static int _sf_write_default_config(const char *config_path);

static int
_sf_dir_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static int
_sf_file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/*
 * Create directory and all parents. Returns 0 on success.
 */
static int
_sf_mkdir_recursive(const char *path)
{
    char tmp[TPB_RAF_PATH_MAX];
    char *p;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!_sf_dir_exists(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (!_sf_dir_exists(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

/*
 * Write a minimal default config.json: {"name": "default"}
 */
static int
_sf_write_default_config(const char *config_path)
{
    FILE *fp = fopen(config_path, "w");
    if (!fp) return -1;
    fprintf(fp, "{\n    \"name\": \"default\"\n}\n");
    fclose(fp);
    return 0;
}

/**
 * @brief Copy the workspace path set by tpb_corelib_init into out_path.
 */
int
tpb_raf_resolve_workspace(char *out_path, size_t pathlen)
{
    const char *ws;
    struct stat st;

    if (!out_path || pathlen == 0) {
        return TPBE_NULLPTR_ARG;
    }

    if (tpb_corelib_workspace_ready() != 0) {
        ws = _tpb_workspace_path_get();
    } else {
        /*
         * Unit tests and minimal tools may set TPB_WORKSPACE without
         * tpb_corelib_init.
         */
        ws = getenv("TPB_WORKSPACE");
    }

    if (ws == NULL || ws[0] == '\0') {
        if (tpb_corelib_workspace_ready() != 0) {
            return TPBE_FILE_IO_FAIL;
        }
        return TPBE_ILLEGAL_CALL;
    }

    if (strlen(ws) >= pathlen) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(out_path, pathlen, "%s", ws);

    if (stat(out_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return TPBE_FILE_IO_FAIL;
    }

    return TPBE_SUCCESS;
}

/**
 * @brief Initialize workspace directory structure.
 */
int
tpb_raf_init_workspace(const char *workspace_path)
{
    char path[TPB_RAF_PATH_MAX];

    if (!workspace_path) {
        return TPBE_NULLPTR_ARG;
    }

    /* Create etc/ directory and config.json */
    snprintf(path, sizeof(path), "%s/etc", workspace_path);
    if (_sf_mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAF_CONFIG_REL);
    if (!_sf_file_exists(path)) {
        if (_sf_write_default_config(path) != 0) {
            return TPBE_FILE_IO_FAIL;
        }
    }

    /* Create rafdb domain directories */
    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAF_TBATCH_DIR);
    if (_sf_mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAF_KERNEL_DIR);
    if (_sf_mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAF_TASK_DIR);
    if (_sf_mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAF_LOG_REL);
    if (_sf_mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    return TPBE_SUCCESS;
}
