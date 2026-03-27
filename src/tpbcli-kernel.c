/*
 * tpbcli-kernel.c
 * `tpbcli kernel` / `tpbcli k` — kernel management subcommands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpbcli-kernel.h"
#include "tpbcli-list.h"
#include "corelib/tpb-driver.h"
#include "corelib/tpb-impl.h"

int
tpbcli_kernel(int argc, char **argv)
{
    int err;

    if (argc < 3) {
        fprintf(stderr, "Usage: tpbcli kernel <list|ls>\n");
        return TPBE_CLI_FAIL;
    }

    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-kernel.c: tpb_register_kernel");
    if (err != 0) {
        return err;
    }

    if (strcmp(argv[2], "list") == 0 || strcmp(argv[2], "ls") == 0) {
        return tpbcli_kernel_list(argc, argv);
    }

    fprintf(stderr, "Usage: tpbcli kernel <list|ls>\n");
    return TPBE_CLI_FAIL;
}
