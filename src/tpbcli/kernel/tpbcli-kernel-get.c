/*
 * tpbcli-kernel-get.c
 * `tpbcli kernel get` — read-only kernel record query.
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
#include "tpbcli-kernel-get.h"
#include "tpbcli-print-kernel-help.h"

/* Local Function Prototypes */

static void _sf_print_kernel_versions(FILE *out, const char *workspace,
                                      const kernel_entry_t *entries, int n,
                                      const char *kernel_name);
static int _sf_pick_latest_entry(const kernel_entry_t *entries, int n,
                                 const char *kernel_name, int *out_idx);
static int _sf_find_entry_by_id(const kernel_entry_t *entries, int n,
                                const unsigned char kernel_id[20],
                                int *out_idx);
static void _sf_print_get_usage(void);

/* Local Function Implementations */

static void
_sf_print_get_usage(void)
{
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "Usage: tpbcli kernel get [-v] (--kernel <name>|--id <hex>)\n");
}

static void
_sf_print_kernel_versions(FILE *out, const char *workspace,
                          const kernel_entry_t *entries, int n,
                          const char *kernel_name)
{
    char kid_hex[41];
    char payload[4096];
    char variation[512];
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    int err;
    int i;

    fprintf(out, "Kernel Versions:\n");
    fprintf(out, "%-40s %s\n", "ID", "Variation");

    for (i = n - 1; i >= 0; i--) {
        if (strcmp(entries[i].kernel_name, kernel_name) != 0) {
            continue;
        }
        tpb_raf_id_to_hex(entries[i].kernel_id, kid_hex);
        variation[0] = '\0';
        memset(&attr, 0, sizeof(attr));
        err = tpb_raf_record_read_kernel(workspace, entries[i].kernel_id,
                                         &attr, &data, &datasize);
        if (err == TPBE_SUCCESS) {
            if (tpb_raf_kernel_get_header_payload(&attr, data,
                    TPB_RAF_KERNEL_HDR_VARIATION,
                    payload, sizeof(payload)) == TPBE_SUCCESS) {
                tpb_raf_kernel_variation_summary(payload, variation,
                                                 sizeof(variation));
            }
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            data = NULL;
        }
        if (variation[0] == '\0') {
            snprintf(variation, sizeof(variation), "-");
        }
        fprintf(out, "%-40s %s\n", kid_hex, variation);
    }
}

static int
_sf_pick_latest_entry(const kernel_entry_t *entries, int n,
                      const char *kernel_name, int *out_idx)
{
    int i;
    int pick = -1;

    for (i = n - 1; i >= 0; i--) {
        if (strcmp(entries[i].kernel_name, kernel_name) != 0) {
            continue;
        }
        if (entries[i].active != 0) {
            *out_idx = i;
            return 0;
        }
        if (pick < 0) {
            pick = i;
        }
    }
    if (pick >= 0) {
        *out_idx = pick;
        return 0;
    }
    return TPBE_LIST_NOT_FOUND;
}

static int
_sf_find_entry_by_id(const kernel_entry_t *entries, int n,
                     const unsigned char kernel_id[20], int *out_idx)
{
    int i;

    for (i = 0; i < n; i++) {
        if (memcmp(entries[i].kernel_id, kernel_id, 20) == 0) {
            *out_idx = i;
            return TPBE_SUCCESS;
        }
    }
    return TPBE_LIST_NOT_FOUND;
}

int
tpbcli_kernel_get(int argc, char **argv)
{
    const char *kernel_name = NULL;
    const char *kernel_id_hex = NULL;
    char workspace[PATH_MAX];
    kernel_entry_t *entries = NULL;
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    unsigned char kernel_id[20];
    int n = 0;
    int verbose = 0;
    int err;
    int i;
    int idx;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            kernel_name = argv[++i];
        } else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            kernel_id_hex = argv[++i];
        } else {
            _sf_print_get_usage();
            return TPBE_CLI_FAIL;
        }
    }

    if ((kernel_name == NULL && kernel_id_hex == NULL) ||
        (kernel_name != NULL && kernel_id_hex != NULL)) {
        _sf_print_get_usage();
        return TPBE_CLI_FAIL;
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpb_raf_entry_list_kernel(workspace, &entries, &n);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (kernel_id_hex != NULL) {
        err = tpb_raf_hex_to_id(kernel_id_hex, kernel_id);
        if (err != TPBE_SUCCESS) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                       "Invalid kernel id '%s'.\n", kernel_id_hex);
            free(entries);
            return err;
        }
        err = _sf_find_entry_by_id(entries, n, kernel_id, &idx);
        if (err != TPBE_SUCCESS) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                       "No kernel record for id '%s'.\n", kernel_id_hex);
            free(entries);
            return err;
        }
        kernel_name = entries[idx].kernel_name;
    } else {
        err = _sf_pick_latest_entry(entries, n, kernel_name, &idx);
        if (err != TPBE_SUCCESS) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                       "No kernel records for '%s'.\n", kernel_name);
            free(entries);
            return err;
        }
    }

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, entries[idx].kernel_id,
                                     &attr, &data, &datasize);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                   "Failed to read kernel record (%d).\n", err);
        free(entries);
        return err;
    }

    if (verbose) {
        tpbcli_print_kernel_help_from_attr(stdout, &attr, data, datasize);
        _sf_print_kernel_versions(stdout, workspace, entries, n, kernel_name);
    } else {
        tpbcli_print_kernel_names_from_attr(&attr);
    }

    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(data);
    free(entries);
    return TPBE_SUCCESS;
}
