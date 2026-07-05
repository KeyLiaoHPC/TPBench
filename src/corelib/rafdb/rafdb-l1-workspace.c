/*
 * rafdb-l1-workspace.c
 * L1 workspace resolution and initialization (domain dirs via registry).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "../../include/tpb-public.h"
#include "../tpb-types.h"
#include "../tpb_corelib_state.h"
#include "rafdb-l1-domain-reg.h"
#include "rafdb-l1-types.h"

/* Local Function Prototypes */
static int _sf_dir_exists(const char *path);
static int _sf_file_exists(const char *path);
static int _sf_mkdir_recursive(const char *path);
static int _sf_write_default_config(const char *config_path);

static int
_sf_dir_exists(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int
_sf_file_exists(const char *path)
{
    struct stat st;

    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

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

static int
_sf_write_default_config(const char *config_path)
{
    FILE *fp = fopen(config_path, "w");

    if (!fp) {
        return -1;
    }
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
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_NULLPTR_ARG, NULL);
    }

    if (tpb_corelib_workspace_ready() != 0) {
        ws = _tpb_workspace_path_get();
    } else {
        ws = getenv("TPB_WORKSPACE");
    }

    if (ws == NULL || ws[0] == '\0') {
        if (tpb_corelib_workspace_ready() != 0) {
            TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
        }
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_ILLEGAL_CALL, NULL);
    }

    if (strlen(ws) >= pathlen) {
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
    }

    snprintf(out_path, pathlen, "%s", ws);

    if (stat(out_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
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
    int i;
    int nd;

    if (!workspace_path) {
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_NULLPTR_ARG, NULL);
    }

    snprintf(path, sizeof(path), "%s/etc", workspace_path);
    if (_sf_mkdir_recursive(path) != 0) {
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
    }

    snprintf(path, sizeof(path), "%s/%s", workspace_path, TPB_RAF_CONFIG_REL);
    if (!_sf_file_exists(path)) {
        if (_sf_write_default_config(path) != 0) {
            TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
        }
    }

    nd = _tpb_raf_l1_domain_count();
    for (i = 0; i < nd; i++) {
        const tpb_raf_domain_desc_t *desc = _tpb_raf_l1_domain_by_index(i);

        if (desc == NULL) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", workspace_path, desc->subdir);
        if (_sf_mkdir_recursive(path) != 0) {
            TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
        }
    }

    snprintf(path, sizeof(path), "%s/%s", workspace_path, TPB_RAF_LOG_REL);
    if (_sf_mkdir_recursive(path) != 0) {
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
    }

    return TPBE_SUCCESS;
}
