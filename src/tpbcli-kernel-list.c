/*
 * tpbcli-kernel-list.c
 * Kernel list output for `tpbcli kernel list` / `tpbcli k ls`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/tpb-public.h"

#include "tpbcli-kernel-list.h"

static int
kernel_id_is_zero(const unsigned char id[20])
{
    static const unsigned char z[20] = {0};

    return memcmp(id, z, 20) == 0;
}

int
tpbcli_kernel_list(int argc, char **argv)
{
    int nkern;
    int i;

    (void)argc;
    (void)argv;

    nkern = tpb_query_kernel(0, NULL, NULL);

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Listing supported kernels.\n");

    for (i = 0; i < nkern; i++) {
        tpb_kernel_t *kernel = NULL;

        tpb_query_kernel(i, NULL, &kernel);
        if (kernel == NULL) {
            continue;
        }
        if (!kernel->info.kernel_record_ok) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "Kernel %s: workspace kernel record update failed.\n",
                       kernel->info.name);
        }
        tpb_free_kernel(kernel);
        free(kernel);
    }

    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Kernel          KernelID  Description\n");

    for (i = 0; i < nkern; i++) {
        tpb_kernel_t *kernel = NULL;
        char kid_str[16];

        tpb_query_kernel(i, NULL, &kernel);
        if (kernel == NULL) {
            continue;
        }

        if (!kernel->info.kernel_record_ok) {
            snprintf(kid_str, sizeof(kid_str), "[ERROR]");
        } else if (kernel_id_is_zero(kernel->info.kernel_id)) {
            snprintf(kid_str, sizeof(kid_str), "-");
        } else {
            char hex[41];

            tpb_rawdb_id_to_hex(kernel->info.kernel_id, hex);
            snprintf(kid_str, sizeof(kid_str), "%.6s*", hex);
        }

        tpb_printf(TPBM_PRTN_M_DIRECT, "%-15s %-9s %s\n",
                   kernel->info.name, kid_str, kernel->info.note);
        tpb_free_kernel(kernel);
        free(kernel);
    }

    return 0;
}
