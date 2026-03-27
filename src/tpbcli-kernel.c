/*
 * tpbcli-kernel.c
 * `tpbcli kernel` / `tpbcli k` — kernel management subcommands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/tpb-public.h"

#include "tpbcli-kernel.h"
#include "tpbcli-kernel-list.h"

int
tpbcli_kernel(int argc, char **argv)
{
    int err;

    if (argc < 3) {
        fprintf(stderr, "Usage: tpbcli kernel <list|ls>\n");
        return TPBE_CLI_FAIL;
    }

    err = tpb_register_kernel();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "tpbcli kernel: tpb_register_kernel failed (%d).\n", err);
        return err;
    }

    if (strcmp(argv[2], "list") == 0 || strcmp(argv[2], "ls") == 0) {
        return tpbcli_kernel_list(argc, argv);
    }

    fprintf(stderr, "Usage: tpbcli kernel <list|ls>\n");
    return TPBE_CLI_FAIL;
}
