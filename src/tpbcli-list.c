/*
 * tpbcli-list.c
 * List subcommand implementation.
 */

#include <stdlib.h>
#include "tpbcli-list.h"
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Local Function Prototypes */

/* Parse list-specific arguments */
static int parse_list(int argc, char **argv);

/* Local Function Implementations */

static int
parse_list(int argc, char **argv)
{
    /* Currently list has no specific options, but we keep the structure
     * for potential future expansion */

    if (argc > 2) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Warning: list command takes no arguments.\n");
    }

    return 0;
}

/* Public Function Implementations */

int
tpbcli_list(int argc, char **argv)
{
    int err;

    err = parse_list(argc, argv);
    if (err != 0) {
        return err;
    }

    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-list.c: tpb_register_kernel");

    if (err == 0) {
        tpb_list();
    }

    return err;
}
