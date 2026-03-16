/*
 * tpb-rawdb-workspace.c
 * Workspace resolution, initialization, and config.json management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "../tpb-types.h"
#include "tpb-rawdb-types.h"

/* Local Function Prototypes */
static int dir_exists(const char *path);
static int file_exists(const char *path);
static int mkdir_recursive(const char *path);
static int write_default_config(const char *config_path);
static int read_config_name(const char *config_path,
                            char *name, size_t namelen);

static int
dir_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static int
file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/*
 * Create directory and all parents. Returns 0 on success.
 */
static int
mkdir_recursive(const char *path)
{
    char tmp[TPB_RAWDB_PATH_MAX];
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
            if (!dir_exists(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (!dir_exists(tmp)) {
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
write_default_config(const char *config_path)
{
    FILE *fp = fopen(config_path, "w");
    if (!fp) return -1;
    fprintf(fp, "{\n    \"name\": \"default\"\n}\n");
    fclose(fp);
    return 0;
}

/*
 * Read the "name" field from a minimal JSON config.
 * Simple parser: looks for "name" : "value".
 */
static int
read_config_name(const char *config_path, char *name,
                 size_t namelen)
{
    FILE *fp;
    char line[1024];
    char *key_pos, *val_start, *val_end;

    fp = fopen(config_path, "r");
    if (!fp) return -1;

    while (fgets(line, sizeof(line), fp)) {
        key_pos = strstr(line, "\"name\"");
        if (!key_pos) continue;

        val_start = strchr(key_pos + 6, '\"');
        if (!val_start) continue;
        val_start++;

        val_end = strchr(val_start, '\"');
        if (!val_end) continue;

        size_t vlen = (size_t)(val_end - val_start);
        if (vlen >= namelen) vlen = namelen - 1;
        memcpy(name, val_start, vlen);
        name[vlen] = '\0';
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return -1;
}

int
tpb_rawdb_resolve_workspace(char *out_path, size_t pathlen)
{
    const char *env_ws;
    const char *home;
    char candidate[TPB_RAWDB_PATH_MAX];
    char config_path[TPB_RAWDB_PATH_MAX];
    char name[256];

    if (!out_path || pathlen == 0) {
        return TPBE_NULLPTR_ARG;
    }

    /* Step 1: check $TPB_WORKSPACE */
    env_ws = getenv("TPB_WORKSPACE");
    if (env_ws && env_ws[0] != '\0') {
        snprintf(out_path, pathlen, "%s", env_ws);
        return TPBE_SUCCESS;
    }

    /* Step 2: check $HOME/.tpbench/etc/config.json */
    home = getenv("HOME");
    if (!home || home[0] == '\0') {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(candidate, sizeof(candidate), "%s/%s",
             home, TPB_RAWDB_DEFAULT_DIR);
    snprintf(config_path, sizeof(config_path), "%s/%s",
             candidate, TPB_RAWDB_CONFIG_REL);

    if (file_exists(config_path)) {
        if (read_config_name(config_path, name,
                             sizeof(name)) == 0) {
            snprintf(out_path, pathlen, "%s", candidate);
            return TPBE_SUCCESS;
        }
    }

    /* Step 3: create default workspace */
    snprintf(out_path, pathlen, "%s", candidate);
    return tpb_rawdb_init_workspace(out_path);
}

int
tpb_rawdb_init_workspace(const char *workspace_path)
{
    char path[TPB_RAWDB_PATH_MAX];

    if (!workspace_path) {
        return TPBE_NULLPTR_ARG;
    }

    /* Create etc/ directory and config.json */
    snprintf(path, sizeof(path), "%s/etc", workspace_path);
    if (mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAWDB_CONFIG_REL);
    if (!file_exists(path)) {
        if (write_default_config(path) != 0) {
            return TPBE_FILE_IO_FAIL;
        }
    }

    /* Create rawdb domain directories */
    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAWDB_TBATCH_DIR);
    if (mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAWDB_KERNEL_DIR);
    if (mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(path, sizeof(path), "%s/%s",
             workspace_path, TPB_RAWDB_TASK_DIR);
    if (mkdir_recursive(path) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    return TPBE_SUCCESS;
}
