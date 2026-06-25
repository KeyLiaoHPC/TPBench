/*
 * tpbcli-kernel-set.c
 * `tpbcli kernel set` — register one kernel and patch metadata kv.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-kernel-set.h"

#define TPBCLI_KERNEL_SET_MAX_KV  32

typedef struct {
    const char *key;
    const char *value;
} tpbcli_kernel_kv_t;

/* Local Function Prototypes */

static int _sf_hash_current_kernel_so(const char *kernel_name,
                                      unsigned char kernel_id[20]);
static int _sf_kernel_id_exists(const char *workspace,
                                const unsigned char kernel_id[20]);
static void _sf_print_set_usage(void);

static int
_sf_kernel_id_exists(const char *workspace, const unsigned char kernel_id[20])
{
    kernel_entry_t *entries = NULL;
    int n;
    int i;
    int found = 0;

    if (tpb_raf_entry_list_kernel(workspace, &entries, &n) != TPBE_SUCCESS) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (memcmp(entries[i].kernel_id, kernel_id, 20) == 0) {
            found = 1;
            break;
        }
    }
    free(entries);
    return found;
}

static int
_sf_hash_current_kernel_so(const char *kernel_name, unsigned char kernel_id[20])
{
    char so_path[PATH_MAX];
    unsigned char sha1[20];
    const char *tpb_home;
    int err;

    if (kernel_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_home = tpb_dl_get_tpb_home();
    if (tpb_home == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    snprintf(so_path, sizeof(so_path), "%s/lib/libtpbk_%s.so",
             tpb_home, kernel_name);

    err = tpb_raf_hash_file(so_path, sha1);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    return tpb_raf_gen_kernel_id(sha1, kernel_id);
}

static void
_sf_print_set_usage(void)
{
    fprintf(stderr,
            "Usage: tpbcli kernel set --kernel <name> "
            "--key <section.subkey> '<value>' "
            "[--key <section.subkey> '<value>' ...]\n");
}

int
tpbcli_kernel_set(int argc, char **argv)
{
    const char *kernel_name = NULL;
    tpbcli_kernel_kv_t pairs[TPBCLI_KERNEL_SET_MAX_KV];
    char workspace[PATH_MAX];
    unsigned char kernel_id[20];
    char kid_hex[41];
    int exists_before;
    int err;
    int i;
    int npairs;

    npairs = 0;
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--kernel") == 0) {
            if (i + 1 >= argc) {
                _sf_print_set_usage();
                return TPBE_CLI_FAIL;
            }
            kernel_name = argv[++i];
        } else if (strcmp(argv[i], "--key") == 0) {
            if (i + 2 >= argc) {
                _sf_print_set_usage();
                return TPBE_CLI_FAIL;
            }
            if (npairs >= TPBCLI_KERNEL_SET_MAX_KV) {
                fprintf(stderr, "kernel set: too many --key pairs.\n");
                return TPBE_CLI_FAIL;
            }
            pairs[npairs].key = argv[++i];
            pairs[npairs].value = argv[++i];
            npairs++;
        } else {
            _sf_print_set_usage();
            return TPBE_CLI_FAIL;
        }
    }

    if (kernel_name == NULL || npairs == 0) {
        _sf_print_set_usage();
        return TPBE_CLI_FAIL;
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = _sf_hash_current_kernel_so(kernel_name, kernel_id);
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "kernel set: cannot hash libtpbk_%s.so (%d).\n",
                   kernel_name, err);
        return err;
    }

    exists_before = _sf_kernel_id_exists(workspace, kernel_id);

    err = tpb_register_kernels(1, &kernel_name);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (exists_before && !tpb_raf_kernel_override_enabled()) {
        tpb_raf_id_to_hex(kernel_id, kid_hex);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "KernelID %s already recorded; skip metadata update "
                   "(set %s=1 to override).\n",
                   kid_hex, TPB_K_OVERRIDE_ENV);
        return TPBE_SUCCESS;
    }

    for (i = 0; i < npairs; i++) {
        err = tpb_raf_kernel_update_meta_key(workspace, kernel_id,
                                             pairs[i].key, pairs[i].value);
        if (err != TPBE_SUCCESS) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                       "kernel set: failed to update '%s' (%d).\n",
                       pairs[i].key, err);
            return err;
        }
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "kernel set: updated %s for kernel %s.\n",
                   pairs[i].key, kernel_name);
    }

    return TPBE_SUCCESS;
}
