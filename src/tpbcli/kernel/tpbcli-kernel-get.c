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

static void
_sf_print_meta_payload(const char *label, const kernel_attr_t *attr,
                       const void *data, const char *hdr_name)
{
    int idx;
    char val[4096];
    uint64_t off = 0;
    uint32_t j;

    idx = tpb_raf_kernel_find_header(attr, hdr_name);
    if (idx < 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s: (missing)\n", label);
        return;
    }

    for (j = 0; j < (uint32_t)idx; j++) {
        off += attr->headers[j].data_size;
    }

    if (data != NULL && attr->headers[idx].data_size > 0) {
        const char *payload = (const char *)((const uint8_t *)data + off);
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s:\n%s\n", label, payload);
        (void)tpb_raf_kernel_meta_kv_get(payload, "active", val, sizeof(val));
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s: (empty)\n", label);
    }
}

static void
_sf_print_kernel_detail(const char *workspace,
                        const kernel_entry_t *ent)
{
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    char kid_hex[41];
    uint32_t i;
    int err;

    tpb_raf_id_to_hex(ent->kernel_id, kid_hex);
    tpb_printf(TPBM_PRTN_M_DIRECT,
               "KernelID: %s\nName: %s\nActive: %u\nnparm: %u nmetric: %u\n",
               kid_hex, ent->kernel_name, ent->active,
               ent->nparm, ent->nmetric);

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, ent->kernel_id,
                                     &attr, &data, &datasize);
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "(record read failed: %d)\n", err);
        return;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Headers: %u\n", attr.nheader);
    for (i = 0; i < attr.nheader; i++) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "  [%u] %s\n", i, attr.headers[i].name);
    }

    _sf_print_meta_payload("variation", &attr, data,
                           TPB_RAF_KERNEL_HDR_VARIATION);
    _sf_print_meta_payload("compilation", &attr, data,
                           TPB_RAF_KERNEL_HDR_COMPILATION);
    _sf_print_meta_payload("dependency", &attr, data,
                           TPB_RAF_KERNEL_HDR_DEPENDENCY);

    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(data);
    tpb_printf(TPBM_PRTN_M_DIRECT, "----\n");
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

int
tpbcli_kernel_get(int argc, char **argv)
{
    const char *kernel_name = NULL;
    char workspace[PATH_MAX];
    kernel_entry_t *entries = NULL;
    int n = 0;
    int verbose = 0;
    int err;
    int i;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            kernel_name = argv[++i];
        } else {
            fprintf(stderr,
                    "Usage: tpbcli kernel get [--verbose|-v] --kernel <name>\n");
            return TPBE_CLI_FAIL;
        }
    }

    if (kernel_name == NULL) {
        fprintf(stderr,
                "Usage: tpbcli kernel get [--verbose|-v] --kernel <name>\n");
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

    if (verbose) {
        int shown = 0;

        for (i = n - 1; i >= 0; i--) {
            if (strcmp(entries[i].kernel_name, kernel_name) != 0) {
                continue;
            }
            _sf_print_kernel_detail(workspace, &entries[i]);
            shown++;
        }
        if (shown == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "No kernel records for '%s'.\n", kernel_name);
            free(entries);
            return TPBE_LIST_NOT_FOUND;
        }
    } else {
        int idx;

        err = _sf_pick_latest_entry(entries, n, kernel_name, &idx);
        if (err != TPBE_SUCCESS) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "No kernel records for '%s'.\n", kernel_name);
            free(entries);
            return err;
        }
        _sf_print_kernel_detail(workspace, &entries[idx]);
    }

    free(entries);
    return TPBE_SUCCESS;
}
