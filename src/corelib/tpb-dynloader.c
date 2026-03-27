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
#include "raw_db/tpb-sha1.h"

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
/*
 * Compute SHA-1 hash for a file content.
 * Used to derive stable KernelID from .so/.tpbx binaries.
 */
static int hash_file_sha1(const char *path, unsigned char out[20]);
/* Return 1 when a 20-byte ID is all zeros, otherwise return 0. */
static int is_zero_id(const unsigned char id[20]);
/*
 * Follow dup_to chain in kernel records until terminal record.
 * If any record in chain is missing, keep current ID as terminal.
 */
static int follow_kernel_dup_chain(const char *workspace,
                                   const unsigned char start_id[20],
                                   unsigned char final_id[20]);
/*
 * Resolve final KernelID against current workspace:
 * - compute candidate KernelID
 * - follow dup_to chain when existing
 * - create kernel record/entry when missing
 */
static int resolve_kernel_id_for_workspace(const char *workspace,
                                           const char *kernel_name,
                                           const unsigned char so_sha1[20],
                                           const unsigned char bin_sha1[20],
                                           unsigned char out_final_id[20],
                                           int *is_new_kernel);

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

static int
hash_file_sha1(const char *path, unsigned char out[20])
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
is_zero_id(const unsigned char id[20])
{
    unsigned char zero[20] = {0};

    if (id == NULL) {
        return 1;
    }

    return (memcmp(id, zero, 20) == 0);
}

static int
follow_kernel_dup_chain(const char *workspace,
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
        err = tpb_rawdb_record_read_kernel(workspace, cur_id,
                                           &attr, &rec_data, &rec_datasize);
        if (attr.headers != NULL) {
            tpb_rawdb_free_headers(attr.headers, attr.nheader);
        }
        if (rec_data != NULL) {
            free(rec_data);
        }
        if (err != TPBE_SUCCESS) {
            memcpy(final_id, cur_id, 20);
            return TPBE_SUCCESS;
        }

        if (is_zero_id(attr.dup_to) || memcmp(attr.dup_to, cur_id, 20) == 0) {
            memcpy(final_id, cur_id, 20);
            return TPBE_SUCCESS;
        }

        memcpy(cur_id, attr.dup_to, 20);
    }

    memcpy(final_id, cur_id, 20);
    return TPBE_SUCCESS;
}

static int
resolve_kernel_id_for_workspace(const char *workspace,
                                const char *kernel_name,
                                const unsigned char so_sha1[20],
                                const unsigned char bin_sha1[20],
                                unsigned char out_final_id[20],
                                int *is_new_kernel)
{
    int err;
    int found = 0;
    kernel_entry_t *entries = NULL;
    int nentries = 0;
    unsigned char computed_id[20];

    if (workspace == NULL || kernel_name == NULL || so_sha1 == NULL ||
        bin_sha1 == NULL || out_final_id == NULL || is_new_kernel == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    *is_new_kernel = 0;

    err = tpb_rawdb_gen_kernel_id(kernel_name, so_sha1, bin_sha1, computed_id);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpb_rawdb_entry_list_kernel(workspace, &entries, &nentries);
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
        err = follow_kernel_dup_chain(workspace, computed_id, out_final_id);
        free(entries);
        return err;
    }

    {
        kernel_attr_t attr;
        kernel_entry_t ent;

        memset(&attr, 0, sizeof(attr));
        memcpy(attr.kernel_id, computed_id, 20);
        memcpy(attr.so_sha1, so_sha1, 20);
        memcpy(attr.bin_sha1, bin_sha1, 20);
        snprintf(attr.kernel_name, sizeof(attr.kernel_name), "%s", kernel_name);
        snprintf(attr.version, sizeof(attr.version), "%s", "unknown");
        snprintf(attr.description, sizeof(attr.description),
                 "%s", "Auto-generated kernel record");
        attr.nparm = 0;
        attr.nmetric = 0;
        attr.kctrl = 0;
        attr.nheader = 0;

        err = tpb_rawdb_record_write_kernel(workspace, &attr, NULL, 0);
        if (err != TPBE_SUCCESS) {
            free(entries);
            return err;
        }

        memset(&ent, 0, sizeof(ent));
        memcpy(ent.kernel_id, computed_id, 20);
        snprintf(ent.kernel_name, sizeof(ent.kernel_name), "%s", kernel_name);
        memcpy(ent.so_sha1, so_sha1, 20);
        ent.kctrl = 0;
        ent.nparm = 0;
        ent.nmetric = 0;

        err = tpb_rawdb_entry_append_kernel(workspace, &ent);
        if (err != TPBE_SUCCESS) {
            free(entries);
            return err;
        }
        *is_new_kernel = 1;
    }

    memcpy(out_final_id, computed_id, 20);
    free(entries);
    return TPBE_SUCCESS;
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
    char tpbx_path[PATH_MAX];
    char kernel_name[TPBM_NAME_STR_MAX_LEN];
    char func_name[TPBM_NAME_STR_MAX_LEN + 32];
    char workspace[PATH_MAX];

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
        unsigned char so_sha1[20] = {0};
        unsigned char bin_sha1[20] = {0};
        unsigned char kernel_id[20] = {0};
        char kernel_id_hex[41];
        int is_new_kernel = 0;
        int ws_err = TPBE_SUCCESS;
        int record_ok = 0;

        /* Look for libtpbk_*.so files */
        if (extract_kernel_name(entry->d_name, "libtpbk_", ".so",
                                kernel_name, sizeof(kernel_name)) != 0) {
            continue;
        }

        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "Parsing kernel %s\n", kernel_name);

        /* Check if we have room for more kernels */
        if (num_dyn_kernels >= MAX_DYN_KERNELS) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "In tpb_dl_scan: Maximum number of dynamic kernels reached.\n");
            break;
        }

        /* Build full path to .so file */
        snprintf(so_path, PATH_MAX, "%s/%s", lib_path, entry->d_name);
        snprintf(tpbx_path, PATH_MAX, "%s/bin/tpbk_%s.tpbx",
                 tpb_dir_resolved, kernel_name);

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

        /* Build KernelID and resolve workspace duplicate chain. */
        err = hash_file_sha1(so_path, so_sha1);
        if (err == TPBE_SUCCESS) {
            err = hash_file_sha1(tpbx_path, bin_sha1);
        }
        if (err == TPBE_SUCCESS) {
            err = tpb_rawdb_gen_kernel_id(kernel_name, so_sha1, bin_sha1,
                                          kernel_id);
        }
        if (err == TPBE_SUCCESS) {
            ws_err = tpb_rawdb_resolve_workspace(workspace, sizeof(workspace));
            if (ws_err == TPBE_SUCCESS) {
                ws_err = resolve_kernel_id_for_workspace(workspace, kernel_name,
                                                         so_sha1, bin_sha1,
                                                         kernel_id,
                                                         &is_new_kernel);
            }
        } else {
            memset(kernel_id, 0, 20);
        }

        record_ok = (err == TPBE_SUCCESS && ws_err == TPBE_SUCCESS) ? 1 : 0;

        tpb_rawdb_id_to_hex(kernel_id, kernel_id_hex);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   " KernelID=%s\n", kernel_id_hex);
        if (is_new_kernel) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                       "New kernel found, add to kernel records.\n");
        }
        tpb_driver_set_kernel_id(kernel_name, kernel_id);
        tpb_driver_set_kernel_record_ok(kernel_name, record_ok);

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
