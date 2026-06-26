/*
 * tpb-dynloader.c
 * Dynamic kernel loader for PLI (Process-Level Integration) mode.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include "tpb-dynloader.h"
#include "tpb-driver.h"
#include "tpb-public.h"
#include "rafdb/tpb-raf-types.h"
#include "rafdb/tpb-sha1.h"
#include "rafdb/tpb-raf-kernel-meta.h"

/* Compile-time TPB_HOME from CMake, may be empty */
#ifndef TPB_HOME
#define TPB_HOME ""
#endif

/* Maximum number of dynamically loaded kernels */
#define MAX_DYN_KERNELS 64

typedef int (*tpb_pli_register_fn_t)(void);

/* Internal structure to track loaded kernel info */
typedef struct {
    char name[TPBM_NAME_STR_MAX_LEN];
    char exec_path[PATH_MAX];
    void *dl_handle;
    int complete;  /* 1 if .so exists and registration succeeded */
    TPB_K_CTRL ktype;
} dyn_kernel_entry_t;

/* Module-level state */
static char tpb_home_resolved[PATH_MAX] = {0};
static dyn_kernel_entry_t dyn_kernels[MAX_DYN_KERNELS];
static int num_dyn_kernels = 0;
static int dynloader_full_scan_done = 0;

/* Local Function Prototypes */

static int _sf_build_kernel_paths(const char *kernel_name, char *so_path,
                                  size_t so_len);
static int _sf_build_pli_launch_path(char *launch_path, size_t launch_len);
static int _sf_check_so_exists(const char *kernel_name);
static int _sf_extract_kernel_name(const char *filename, const char *prefix,
                                   const char *suffix, char *name,
                                   size_t name_len);
static int _sf_finalize_kernel_scan(const char *kernel_name,
                                    void *dl_handle, const char *so_path,
                                    int require_success);
static int _sf_find_dyn_kernel(const char *kernel_name);
static int _sf_follow_kernel_dup_chain(const char *workspace,
                                       const unsigned char start_id[20],
                                       unsigned char final_id[20]);
static int _sf_hash_file_sha1(const char *path, unsigned char out[20]);
static int _sf_is_zero_id(const unsigned char id[20]);
static int _sf_load_register_fn(void *handle, const char *kernel_name,
                                const char *path_label,
                                tpb_pli_register_fn_t *reg_out);
static int _sf_resolve_kernel_id_for_workspace(const char *workspace,
                                               const char *kernel_name,
                                               const unsigned char so_sha1[20],
                                               const tpb_kernel_t *registered,
                                               unsigned char out_final_id[20],
                                               int *is_new_kernel);
static int _sf_resolve_tpb_home(void);
static int _sf_scan_kernel_internal(const char *kernel_name,
                                    int require_success);
static int _sf_try_load_from_path(const char *kernel_name, const char *path,
                                  const char *path_label, void **handle_out,
                                  tpb_pli_register_fn_t *reg_out);

/* Local Function Implementations */

static int
_sf_resolve_tpb_home(void)
{
    const char *env_home;
    const char *home;
    char *resolved;

    if (tpb_home_resolved[0] != '\0') {
        return 0;
    }

    env_home = getenv("TPB_HOME");
    if (env_home != NULL && env_home[0] != '\0') {
        resolved = realpath(env_home, tpb_home_resolved);
        if (resolved != NULL) {
            return 0;
        }
        snprintf(tpb_home_resolved, PATH_MAX, "%s", env_home);
        return 0;
    }

    home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        if (snprintf(tpb_home_resolved, PATH_MAX, "%s/%s", home,
                     TPB_RAF_DEFAULT_DIR) >= PATH_MAX) {
            return TPBE_FILE_IO_FAIL;
        }
        return 0;
    }

    if (strlen(TPB_HOME) > 0) {
        resolved = realpath(TPB_HOME, tpb_home_resolved);
        if (resolved != NULL) {
            return 0;
        }
        snprintf(tpb_home_resolved, PATH_MAX, "%s", TPB_HOME);
        return 0;
    }

    return TPBE_FILE_IO_FAIL;
}

static int
_sf_build_kernel_paths(const char *kernel_name, char *so_path,
                       size_t so_len)
{
    if (kernel_name == NULL || so_path == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    snprintf(so_path, so_len, "%s/lib/libtpbk_%s.so",
             tpb_home_resolved, kernel_name);
    return TPBE_SUCCESS;
}

static int
_sf_build_pli_launch_path(char *launch_path, size_t launch_len)
{
    if (launch_path == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    snprintf(launch_path, launch_len, "%s/bin/tpbcli-pli-launcher",
             tpb_home_resolved);
    return TPBE_SUCCESS;
}

static int
_sf_extract_kernel_name(const char *filename, const char *prefix,
                        const char *suffix, char *name, size_t name_len)
{
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t filename_len = strlen(filename);

    if (filename_len <= prefix_len + suffix_len) {
        return -1;
    }
    if (strncmp(filename, prefix, prefix_len) != 0) {
        return -1;
    }
    if (strcmp(filename + filename_len - suffix_len, suffix) != 0) {
        return -1;
    }

    size_t name_length = filename_len - prefix_len - suffix_len;
    if (name_length >= name_len) {
        return -1;
    }
    strncpy(name, filename + prefix_len, name_length);
    name[name_length] = '\0';

    return 0;
}

static int
_sf_check_so_exists(const char *kernel_name)
{
    char so_path[PATH_MAX];

    snprintf(so_path, PATH_MAX, "%s/lib/libtpbk_%s.so",
             tpb_home_resolved, kernel_name);
    return (access(so_path, R_OK) == 0);
}

static int
_sf_find_dyn_kernel(const char *kernel_name)
{
    for (int i = 0; i < num_dyn_kernels; i++) {
        if (strcmp(dyn_kernels[i].name, kernel_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int
_sf_hash_file_sha1(const char *path, unsigned char out[20])
{
    FILE *fp;
    size_t nread;
    unsigned char buf[4096];
    tpb_sha1_ctx_t sha1_ctx;

    if (path == NULL || out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }

    tpb_sha1_init(&sha1_ctx);
    while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
        tpb_sha1_update(&sha1_ctx, buf, nread);
    }
    tpb_sha1_final(&sha1_ctx, out);
    fclose(fp);
    return TPBE_SUCCESS;
}

static int
_sf_is_zero_id(const unsigned char id[20])
{
    unsigned char zero[20] = {0};

    if (id == NULL) {
        return 1;
    }

    return (memcmp(id, zero, 20) == 0);
}

static int
_sf_follow_kernel_dup_chain(const char *workspace,
                            const unsigned char start_id[20],
                            unsigned char final_id[20])
{
    const int max_hops = 64;
    unsigned char cur_id[20];
    int err;

    if (workspace == NULL || start_id == NULL || final_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    memcpy(cur_id, start_id, 20);
    for (int hop = 0; hop < max_hops; hop++) {
        kernel_attr_t attr;
        void *rec_data = NULL;
        uint64_t rec_datasize = 0;

        memset(&attr, 0, sizeof(attr));
        err = tpb_raf_record_read_kernel(workspace, cur_id,
                                         &attr, &rec_data, &rec_datasize);
        if (attr.headers != NULL) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
        }
        if (rec_data != NULL) {
            free(rec_data);
        }
        if (err != TPBE_SUCCESS) {
            memcpy(final_id, cur_id, 20);
            return TPBE_SUCCESS;
        }

        if (_sf_is_zero_id(attr.derive_to) ||
            memcmp(attr.derive_to, cur_id, 20) == 0) {
            memcpy(final_id, cur_id, 20);
            return TPBE_SUCCESS;
        }

        memcpy(cur_id, attr.derive_to, 20);
    }

    memcpy(final_id, cur_id, 20);
    return TPBE_SUCCESS;
}

static int
_sf_resolve_kernel_id_for_workspace(const char *workspace,
                                    const char *kernel_name,
                                    const unsigned char so_sha1[20],
                                    const tpb_kernel_t *registered,
                                    unsigned char out_final_id[20],
                                    int *is_new_kernel)
{
    int err;
    int found = 0;
    kernel_entry_t *entries = NULL;
    int nentries = 0;
    unsigned char computed_id[20];
    char kid_hex[41];

    if (workspace == NULL || kernel_name == NULL || so_sha1 == NULL ||
        registered == NULL || out_final_id == NULL || is_new_kernel == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    *is_new_kernel = 0;

    err = tpb_raf_gen_kernel_id(so_sha1, computed_id);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpb_raf_entry_list_kernel(workspace, &entries, &nentries);
    if (err != TPBE_SUCCESS) {
        nentries = 0;
        entries = NULL;
    }

    for (int i = 0; i < nentries; i++) {
        if (memcmp(entries[i].kernel_id, computed_id, 20) == 0) {
            found = 1;
            break;
        }
    }

    if (found) {
        if (!tpb_raf_kernel_override_enabled()) {
            tpb_raf_id_to_hex(computed_id, kid_hex);
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "KernelID %s already recorded; skip update "
                       "(set %s=1 to override).\n",
                       kid_hex, TPB_K_OVERRIDE_ENV);
        } else {
            kernel_attr_t attr;
            void *data = NULL;
            uint64_t datasize = 0;

            err = tpb_raf_kernel_build_registered_attr(registered, computed_id,
                                                       &attr, &data, &datasize);
            if (err == TPBE_SUCCESS) {
                err = tpb_raf_record_write_kernel(workspace, &attr, data,
                                                  datasize);
            }
            tpb_raf_kernel_free_built_attr(&attr, data);
            if (err == TPBE_SUCCESS) {
                (void)tpb_raf_kernel_deactivate_same_name(workspace, kernel_name,
                                                          computed_id);
                (void)tpb_raf_entry_patch_kernel_active(workspace, computed_id, 1);
                (void)tpb_raf_record_patch_kernel_active(workspace, computed_id, 1);
            }
        }
        err = _sf_follow_kernel_dup_chain(workspace, computed_id, out_final_id);
        free(entries);
        return err;
    }

    {
        kernel_attr_t attr;
        kernel_entry_t ent;
        void *data = NULL;
        uint64_t datasize = 0;

        err = tpb_raf_kernel_build_registered_attr(registered, computed_id,
                                                   &attr, &data, &datasize);
        if (err != TPBE_SUCCESS) {
            free(entries);
            return err;
        }

        err = tpb_raf_record_write_kernel(workspace, &attr, data, datasize);
        tpb_raf_kernel_free_built_attr(&attr, data);
        if (err != TPBE_SUCCESS) {
            free(entries);
            return err;
        }

        memset(&ent, 0, sizeof(ent));
        memcpy(ent.kernel_id, computed_id, 20);
        snprintf(ent.kernel_name, sizeof(ent.kernel_name), "%s", kernel_name);
        ent.kctrl = (uint32_t)registered->info.kctrl;
        ent.nparm = (uint32_t)registered->info.nparms;
        ent.nmetric = (uint32_t)registered->info.nouts;
        ent.active = 1;

        err = tpb_raf_entry_append_kernel(workspace, &ent);
        if (err != TPBE_SUCCESS) {
            free(entries);
            return err;
        }

        (void)tpb_raf_kernel_deactivate_same_name(workspace, kernel_name,
                                                  computed_id);
        *is_new_kernel = 1;
    }

    memcpy(out_final_id, computed_id, 20);
    free(entries);
    return TPBE_SUCCESS;
}

static int
_sf_load_register_fn(void *handle, const char *kernel_name,
                     const char *path_label, tpb_pli_register_fn_t *reg_out)
{
    char func_name[TPBM_NAME_STR_MAX_LEN + 32];

    if (handle == NULL || kernel_name == NULL || reg_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    snprintf(func_name, sizeof(func_name), "tpbk_pli_register_%s", kernel_name);
    *reg_out = (tpb_pli_register_fn_t)dlsym(handle, func_name);
    if (*reg_out == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "In tpb_dl_scan: No PLI registration function %s in %s\n",
                   func_name, path_label);
        return TPBE_KERNEL_NE_FAIL;
    }

    return TPBE_SUCCESS;
}

static int
_sf_try_load_from_path(const char *kernel_name, const char *path,
                       const char *path_label, void **handle_out,
                       tpb_pli_register_fn_t *reg_out)
{
    void *handle;
    int err;

    if (kernel_name == NULL || path == NULL || handle_out == NULL ||
        reg_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (access(path, R_OK) != 0) {
        return TPBE_KERNEL_NE_FAIL;
    }

    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "In tpb_dl_scan: Failed to load %s: %s\n",
                   path, dlerror());
        return TPBE_KERNEL_NE_FAIL;
    }

    err = _sf_load_register_fn(handle, kernel_name, path_label, reg_out);
    if (err != TPBE_SUCCESS) {
        dlclose(handle);
        return err;
    }

    *handle_out = handle;
    return TPBE_SUCCESS;
}

static int
_sf_finalize_kernel_scan(const char *kernel_name, void *dl_handle,
                         const char *so_path, int require_success)
{
    unsigned char so_sha1[20] = {0};
    unsigned char kernel_id[20] = {0};
    char kernel_id_hex[41];
    char workspace[PATH_MAX];
    int is_new_kernel = 0;
    int ws_err = TPBE_SUCCESS;
    int record_ok = 0;
    int err;
    dyn_kernel_entry_t *k;
    tpb_kernel_t *registered = NULL;

    err = _sf_hash_file_sha1(so_path, so_sha1);
    if (err == TPBE_SUCCESS) {
        err = tpb_raf_gen_kernel_id(so_sha1, kernel_id);
    }
    if (err == TPBE_SUCCESS) {
        ws_err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
        if (ws_err == TPBE_SUCCESS) {
            (void)tpb_query_kernel(-1, kernel_name, &registered);
            if (registered == NULL) {
                ws_err = TPBE_KERNEL_NE_FAIL;
            } else {
                ws_err = _sf_resolve_kernel_id_for_workspace(workspace,
                                                             kernel_name,
                                                             so_sha1,
                                                             registered,
                                                             kernel_id,
                                                             &is_new_kernel);
                tpb_free_kernel(registered);
                free(registered);
            }
        }
    } else {
        memset(kernel_id, 0, 20);
    }

    record_ok = (err == TPBE_SUCCESS && ws_err == TPBE_SUCCESS) ? 1 : 0;

    tpb_raf_id_to_hex(kernel_id, kernel_id_hex);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               " KernelID=%s\n", kernel_id_hex);
    if (is_new_kernel) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "New kernel found, add to kernel records.\n");
    }
    tpb_driver_set_kernel_id(kernel_name, kernel_id);
    tpb_driver_set_kernel_record_ok(kernel_name, record_ok);

    k = &dyn_kernels[num_dyn_kernels];
    snprintf(k->name, TPBM_NAME_STR_MAX_LEN, "%s", kernel_name);
    snprintf(k->exec_path, PATH_MAX, "%s", so_path);
    k->dl_handle = dl_handle;
    k->complete = _sf_check_so_exists(kernel_name);
    k->ktype = TPB_KTYPE_PLI;

    if (!k->complete) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "In tpb_dl_scan: Kernel %s is incomplete: missing %s\n",
                   kernel_name, k->exec_path);
        if (require_success) {
            dlclose(dl_handle);
            return TPBE_KERNEL_INCOMPLETE;
        }
    }

    num_dyn_kernels++;
    return TPBE_SUCCESS;
}

static int
_sf_scan_kernel_internal(const char *kernel_name, int require_success)
{
    char so_path[PATH_MAX];
    void *handle = NULL;
    tpb_pli_register_fn_t reg_func = NULL;
    int err;

    if (kernel_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (_sf_find_dyn_kernel(kernel_name) >= 0) {
        return TPBE_SUCCESS;
    }

    if (num_dyn_kernels >= MAX_DYN_KERNELS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "In tpb_dl_scan: Maximum number of dynamic kernels reached.\n");
        return TPBE_KERNEL_NE_FAIL;
    }

    err = _sf_resolve_tpb_home();
    if (err != 0) {
        return err;
    }

    _sf_build_kernel_paths(kernel_name, so_path, sizeof(so_path));

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Parsing kernel %s\n", kernel_name);

    if (_sf_try_load_from_path(kernel_name, so_path, so_path,
                               &handle, &reg_func) != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | (require_success ? TPBE_FAIL : TPBE_WARN),
                   "In tpb_dl_scan: Failed to scan kernel %s "
                   "(so dlopen failed).\n", kernel_name);
        return require_success ? TPBE_KERNEL_NE_FAIL : TPBE_SUCCESS;
    }

    err = reg_func();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | (require_success ? TPBE_FAIL : TPBE_WARN),
                   "In tpb_dl_scan: Failed to register kernel %s: error %d\n",
                   kernel_name, err);
        dlclose(handle);
        return require_success ? TPBE_KERNEL_NE_FAIL : TPBE_SUCCESS;
    }

    err = _sf_finalize_kernel_scan(kernel_name, handle, so_path,
                                   require_success);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    return TPBE_SUCCESS;
}

/* Public Function Implementations */

const char *
tpb_dl_get_tpb_home(void)
{
    if (tpb_home_resolved[0] == '\0') {
        _sf_resolve_tpb_home();
    }
    return tpb_home_resolved;
}

int
tpb_dl_force_tpb_home(const char *path)
{
    char *resolved;

    if (path == NULL || path[0] == '\0') {
        return TPBE_NULLPTR_ARG;
    }

    tpb_home_resolved[0] = '\0';
    resolved = realpath(path, tpb_home_resolved);
    if (resolved != NULL) {
        return TPBE_SUCCESS;
    }
    if (snprintf(tpb_home_resolved, PATH_MAX, "%s", path) >= PATH_MAX) {
        tpb_home_resolved[0] = '\0';
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;
}

int
tpb_dl_scan_kernel(const char *kernel_name)
{
    return _sf_scan_kernel_internal(kernel_name, 1);
}

int
tpb_dl_scan(void)
{
    int err;
    DIR *dir;
    struct dirent *entry;
    char lib_path[PATH_MAX];
    char kernel_name[TPBM_NAME_STR_MAX_LEN];

    if (dynloader_full_scan_done) {
        return 0;
    }

    err = _sf_resolve_tpb_home();
    if (err != 0) {
        return err;
    }

    snprintf(lib_path, PATH_MAX, "%s/lib", tpb_home_resolved);
    dir = opendir(lib_path);
    if (dir == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "In tpb_dl_scan: Cannot open lib directory: %s\n",
                   lib_path);
        dynloader_full_scan_done = 1;
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (_sf_extract_kernel_name(entry->d_name, "libtpbk_", ".so",
                                    kernel_name, sizeof(kernel_name)) != 0) {
            continue;
        }

        (void)_sf_scan_kernel_internal(kernel_name, 0);
    }

    closedir(dir);
    dynloader_full_scan_done = 1;
    return 0;
}

const char *
tpb_dl_get_pli_launch_path(void)
{
    static char launch_path[PATH_MAX] = {0};

    if (launch_path[0] == '\0') {
        if (_sf_resolve_tpb_home() != 0) {
            return NULL;
        }
        _sf_build_pli_launch_path(launch_path, sizeof(launch_path));
    }

    if (access(launch_path, X_OK) != 0) {
        return NULL;
    }

    return launch_path;
}

const char *
tpb_dl_get_exec_path(const char *kernel_name)
{
    if (kernel_name == NULL) {
        return NULL;
    }

    int idx = _sf_find_dyn_kernel(kernel_name);
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

    int idx = _sf_find_dyn_kernel(kernel_name);
    if (idx < 0) {
        return 0;
    }

    return dyn_kernels[idx].complete;
}
