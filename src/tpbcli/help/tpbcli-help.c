/*
 * tpbcli-help.c
 * Help subcommand implementation.
 */

#include "tpb-public.h"
#include "tpbcli-help.h"
#include "tpbcli-help-doc.h"

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
    if (err != 0 && TPBE_CAUSE(err) != TPBE_EXIT_ON_HELP) {
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }

    tpbcli_print_help_total();
    return TPBE_EXIT_ON_HELP;
}
