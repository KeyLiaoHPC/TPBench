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
#include "corelib/tpb-types.h"
#include "kernels/kernels.h"

/* Local Function Prototypes */

/* Parse list-specific arguments */
static int parse_list(int argc, char **argv);

/* Local Function Implementations */

static int
parse_list(int argc, char **argv)
{
    /* Default is PLI mode */
    int mode = TPB_INTEG_MODE_PLI;

    /* Scan for -P or -F switches */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) {
            mode = TPB_INTEG_MODE_PLI;
        } else if (strcmp(argv[i], "-F") == 0) {
            mode = TPB_INTEG_MODE_FLI;
        }
    }

    tpb_driver_set_integ_mode(mode);
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

    /* For FLI mode, register the statically linked kernels */
    int mode = tpb_driver_get_integ_mode();
    if (mode == TPB_INTEG_MODE_FLI) {
        tpb_driver_enable_kernel_reg();
#ifdef TPB_HAS_KERNEL_TRIAD
        err = register_triad();
        __tpbm_exit_on_error(err, "At tpbcli-list.c: register_triad");
#endif
#ifdef TPB_HAS_KERNEL_STREAM
        err = register_stream();
        __tpbm_exit_on_error(err, "At tpbcli-list.c: register_stream");
#endif
        tpb_driver_disable_kernel_reg();
    }

    if (err == 0) {
        tpb_list();
    }

    return err;
}
