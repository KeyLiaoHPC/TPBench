/*
 * tpb-dynloader.c
 * Dynamic kernel loader for PLI (Process-Level Integration) mode.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <linux/limits.h>
#include "tpb-dynloader.h"
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Compile-time TPB_DIR from CMake, may be empty */
#ifndef TPB_DIR
#define TPB_DIR ""
#endif

/* Maximum number of dynamically loaded kernels */
#define MAX_DYN_KERNELS 64

/* Internal structure to track loaded kernel info */
typedef struct {
    char name[TPBM_NAME_STR_MAX_LEN];
    char exec_path[PATH_MAX];
    void *dl_handle;
    int complete;  /* 1 if both .so and .tpbx exist */
    TPB_K_CTRL ktype;
} dyn_kernel_entry_t;

/* Module-level state */
static char tpb_dir_resolved[PATH_MAX] = {0};
static dyn_kernel_entry_t dyn_kernels[MAX_DYN_KERNELS];
static int num_dyn_kernels = 0;
static int dynloader_initialized = 0;

/* Local Function Prototypes */
static int resolve_tpb_dir(void);
static int extract_kernel_name(const char *filename, const char *prefix,
                               const char *suffix, char *name, size_t name_len);
static int check_tpbx_exists(const char *kernel_name);
static int find_dyn_kernel(const char *kernel_name);

/* Local Function Implementations */

static int
resolve_tpb_dir(void)
{
    if (tpb_dir_resolved[0] != '\0') {
        return 0;  /* Already resolved */
    }

    /* Use compile-time TPB_DIR if set */
    if (strlen(TPB_DIR) > 0) {
        char *resolved = realpath(TPB_DIR, tpb_dir_resolved);
        if (resolved != NULL) {
            return 0;
        }
        /* If realpath fails, use the original path */
        snprintf(tpb_dir_resolved, PATH_MAX, "%s", TPB_DIR);
        return 0;
    }

    /* Fallback: try to get from /proc/self/exe (Linux-specific) */
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        /* Get parent directory of bin/ */
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';
            /* Go up one more level from bin/ */
            last_slash = strrchr(exe_path, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
                snprintf(tpb_dir_resolved, PATH_MAX, "%s", exe_path);
                return 0;
            }
        }
    }

    /* Last resort: use current directory */
    if (getcwd(tpb_dir_resolved, PATH_MAX) == NULL) {
        return TPBE_FILE_IO_FAIL;
    }

    return 0;
}

static int
extract_kernel_name(const char *filename, const char *prefix,
                    const char *suffix, char *name, size_t name_len)
{
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t filename_len = strlen(filename);

    /* Check if filename starts with prefix and ends with suffix */
    if (filename_len <= prefix_len + suffix_len) {
        return -1;
    }
    if (strncmp(filename, prefix, prefix_len) != 0) {
        return -1;
    }
    if (strcmp(filename + filename_len - suffix_len, suffix) != 0) {
        return -1;
    }

    /* Extract the kernel name */
    size_t name_length = filename_len - prefix_len - suffix_len;
    if (name_length >= name_len) {
        return -1;
    }
    strncpy(name, filename + prefix_len, name_length);
    name[name_length] = '\0';

    return 0;
}

static int
check_tpbx_exists(const char *kernel_name)
{
    char tpbx_path[PATH_MAX];
    snprintf(tpbx_path, PATH_MAX, "%s/bin/tpbk_%s.tpbx",
             tpb_dir_resolved, kernel_name);

    return (access(tpbx_path, X_OK) == 0);
}

static int
find_dyn_kernel(const char *kernel_name)
{
    for (int i = 0; i < num_dyn_kernels; i++) {
        if (strcmp(dyn_kernels[i].name, kernel_name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Public Function Implementations */

const char *
tpb_dl_get_tpb_dir(void)
{
    if (tpb_dir_resolved[0] == '\0') {
        resolve_tpb_dir();
    }
    return tpb_dir_resolved;
}

int
tpb_dl_scan(void)
{
    int err;
    DIR *dir;
    struct dirent *entry;
    char lib_path[PATH_MAX];
    char so_path[PATH_MAX];
    char kernel_name[TPBM_NAME_STR_MAX_LEN];
    char func_name[TPBM_NAME_STR_MAX_LEN + 32];

    if (dynloader_initialized) {
        return 0;  /* Already scanned */
    }

    err = resolve_tpb_dir();
    if (err != 0) {
        return err;
    }

    /* Construct lib directory path */
    snprintf(lib_path, PATH_MAX, "%s/lib", tpb_dir_resolved);

    dir = opendir(lib_path);
    if (dir == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "In tpb_dl_scan: Cannot open lib directory: %s\n", lib_path);
        return 0;  /* Not an error - just no kernels found */
    }

    while ((entry = readdir(dir)) != NULL) {
        /* Look for libtpbk_*.so files */
        if (extract_kernel_name(entry->d_name, "libtpbk_", ".so",
                                kernel_name, sizeof(kernel_name)) != 0) {
            continue;
        }

        /* Check if we have room for more kernels */
        if (num_dyn_kernels >= MAX_DYN_KERNELS) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "In tpb_dl_scan: Maximum number of dynamic kernels reached.\n");
            break;
        }

        /* Build full path to .so file */
        snprintf(so_path, PATH_MAX, "%s/%s", lib_path, entry->d_name);

        /* dlopen the library */
        void *handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "In tpb_dl_scan: Failed to load %s: %s\n", so_path, dlerror());
            continue;
        }

        /* Look for tpbk_pli_register_<name> function */
        snprintf(func_name, sizeof(func_name), "tpbk_pli_register_%s", kernel_name);
        typedef int (*register_func_t)(void);
        register_func_t reg_func = (register_func_t)dlsym(handle, func_name);

        if (reg_func == NULL) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "In tpb_dl_scan: No PLI registration function %s in %s\n", func_name, so_path);
            dlclose(handle);
            continue;
        }

        /* Call the registration function */
        err = reg_func();
        if (err != 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "In tpb_dl_scan: Failed to register kernel %s: error %d\n", kernel_name, err);
            dlclose(handle);
            continue;
        }

        /* Store kernel info */
        dyn_kernel_entry_t *k = &dyn_kernels[num_dyn_kernels];
        snprintf(k->name, TPBM_NAME_STR_MAX_LEN, "%s", kernel_name);
        snprintf(k->exec_path, PATH_MAX, "%s/bin/tpbk_%s.tpbx",
                 tpb_dir_resolved, kernel_name);
        k->dl_handle = handle;
        k->complete = check_tpbx_exists(kernel_name);
        k->ktype = TPB_KTYPE_PLI;

        if (!k->complete) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "In tpb_dl_scan: Kernel %s is incomplete: missing %s\n",
                       kernel_name, k->exec_path);
        }

        num_dyn_kernels++;
    }

    closedir(dir);
    dynloader_initialized = 1;

    return 0;
}

const char *
tpb_dl_get_exec_path(const char *kernel_name)
{
    if (kernel_name == NULL) {
        return NULL;
    }

    int idx = find_dyn_kernel(kernel_name);
    if (idx < 0) {
        return NULL;
    }

    if (!dyn_kernels[idx].complete) {
        return NULL;
    }

    return dyn_kernels[idx].exec_path;
}

int
tpb_dl_is_complete(const char *kernel_name)
{
    if (kernel_name == NULL) {
        return 0;
    }

    int idx = find_dyn_kernel(kernel_name);
    if (idx < 0) {
        return 0;
    }

    return dyn_kernels[idx].complete;
}

TPB_K_CTRL
tpb_dl_get_ktype(const char *kernel_name)
{
    if (kernel_name == NULL) {
        return 0;
    }

    int idx = find_dyn_kernel(kernel_name);
    if (idx < 0) {
        return 0;
    }

    return dyn_kernels[idx].ktype;
}
