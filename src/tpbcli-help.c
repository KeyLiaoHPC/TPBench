/*
 * tpbcli-help.c
 * Help subcommand implementation.
 */

#include "tpbcli-help.h"
#include "corelib/tpb-io.h"
#include "corelib/tpb-types.h"

/* Local Function Prototypes */

/* Parse help-specific arguments */
static int parse_help(int argc, char **argv);

/* Local Function Implementations */

static int
parse_help(int argc, char **argv)
{
    /* TODO: Implement detailed help parsing
     * This function could handle cases like:
     * tpbcli help <subcommand>
     * tpbcli help <kernel>
     */

    (void)argc;
    (void)argv;

    return 0;
}

/* Public Function Implementations */

int
tpbcli_help(int argc, char **argv)
{
    int err;

    err = parse_help(argc, argv);
    if (err != 0 && err != TPBE_EXIT_ON_HELP) {
        return err;
    }

    tpb_print_help_total();
    return TPBE_EXIT_ON_HELP;
}
