/*
 * tpb-cli.c
 * TPBench CLI entry point.
 */

#include "tpb-launch.h"
#include "tpb-types.h"

int
main(int argc, char **argv)
{
    int rc = tpb_launch(argc, argv);
    if (rc == TPBE_EXIT_ON_HELP) {
        return 0;
    }
    return rc;
}
