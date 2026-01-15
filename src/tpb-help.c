/*
 * tpb-help.c
 * Help subcommand.
 */

#include "tpb-launch.h"
#include "tpb-io.h"
#include "tpb-types.h"

int
tpb_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tpb_print_help_total();
    return TPBE_EXIT_ON_HELP;
}
