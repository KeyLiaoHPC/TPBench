/*
 * tpbcli-list.c
 * List subcommand implementation.
 */

#include <stdlib.h>
#include <string.h>
#include "tpbcli-list.h"
#include "corelib/tpb-driver.h"
#include "corelib/tpb-impl.h"
#include "corelib/tpb-io.h"

/* Public Function Implementations */

int
tpbcli_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int err;

    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-list.c: tpb_register_kernel");

    if (err == 0) {
        tpb_list();
    }

    return err;
}
