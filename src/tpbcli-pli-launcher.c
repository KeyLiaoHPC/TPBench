/*
 * tpbcli-pli-launcher.c
 * Launch a PLI kernel .so by dlopen and calling tpbk_<kernel>_entry.
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/tpb-public.h"
#include "include/tpbench.h"

static int
_sf_extract_kernel_name(const char *so_path, char *kernel_name, size_t name_len)
{
    char *copy;
    char *base;
    const char *prefix = "libtpbk_";
    size_t prefix_len = strlen(prefix);
    size_t base_len;
    size_t kernel_len;

    if (so_path == NULL || kernel_name == NULL || name_len == 0) {
        return -1;
    }

    copy = strdup(so_path);
    if (copy == NULL) {
        return -1;
    }

    base = basename(copy);
    base_len = strlen(base);
    if (base_len <= prefix_len + 3 ||
        strncmp(base, prefix, prefix_len) != 0 ||
        strcmp(base + base_len - 3, ".so") != 0) {
        free(copy);
        return -1;
    }

    kernel_len = base_len - prefix_len - 3;
    if (kernel_len >= name_len) {
        free(copy);
        return -1;
    }

    memcpy(kernel_name, base + prefix_len, kernel_len);
    kernel_name[kernel_len] = '\0';
    free(copy);
    return 0;
}

int
main(int argc, char **argv)
{
    void *handle;
    int (*kernel_entry)(int, char **);
    int (*kernel_main)(int, char **);
    char kernel_name[TPBM_NAME_STR_MAX_LEN];
    char entry_name[TPBM_NAME_STR_MAX_LEN + 16];
    char *so_copy;
    char **kargv;
    int rc;

    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <kernel.so> <timer_name> [kernel_args...]\n",
                argv[0]);
        return TPBE_CLI_FAIL;
    }

    handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "tpbcli-pli-launcher: dlopen(%s): %s\n",
                argv[1], dlerror());
        return TPBE_KERNEL_NE_FAIL;
    }

    if (_sf_extract_kernel_name(argv[1], kernel_name,
                                sizeof(kernel_name)) == 0) {
        snprintf(entry_name, sizeof(entry_name), "tpbk_%s_entry", kernel_name);
        kernel_entry = (int (*)(int, char **))dlsym(handle, entry_name);
        if (kernel_entry != NULL) {
            so_copy = strdup(argv[1]);
            if (so_copy == NULL) {
                dlclose(handle);
                return TPBE_MALLOC_FAIL;
            }

            kargv = (char **)malloc(sizeof(char *) * (size_t)(argc - 1));
            if (kargv == NULL) {
                free(so_copy);
                dlclose(handle);
                return TPBE_MALLOC_FAIL;
            }

            kargv[0] = basename(so_copy);
            for (int i = 2; i < argc; i++) {
                kargv[i - 1] = argv[i];
            }

            rc = kernel_entry(argc - 1, kargv);

            free(kargv);
            free(so_copy);
            dlclose(handle);
            return rc;
        }
    }

    kernel_main = (int (*)(int, char **))dlsym(handle, "main");
    if (kernel_main == NULL) {
        fprintf(stderr,
                "tpbcli-pli-launcher: dlsym(tpbk_*_entry/main): %s\n",
                dlerror());
        dlclose(handle);
        return TPBE_KERNEL_NE_FAIL;
    }

    so_copy = strdup(argv[1]);
    if (so_copy == NULL) {
        dlclose(handle);
        return TPBE_MALLOC_FAIL;
    }

    kargv = (char **)malloc(sizeof(char *) * (size_t)(argc - 1));
    if (kargv == NULL) {
        free(so_copy);
        dlclose(handle);
        return TPBE_MALLOC_FAIL;
    }

    kargv[0] = basename(so_copy);
    for (int i = 2; i < argc; i++) {
        kargv[i - 1] = argv[i];
    }

    rc = kernel_main(argc - 1, kargv);

    free(kargv);
    free(so_copy);
    dlclose(handle);
    return rc;
}
