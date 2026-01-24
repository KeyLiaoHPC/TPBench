/*
 * tpbcli-benchmark.c
 * Benchmark subcommand implementation.
 */

#include "tpbcli-benchmark.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Local Function Prototypes */

/* Parse benchmark-specific arguments */
static int parse_benchmark(int argc, char **argv, tpb_args_t *tpb_args);

/* Local Function Implementations */

static int
parse_benchmark(int argc, char **argv, tpb_args_t *tpb_args)
{
    /* TODO: Implement benchmark command parsing
     * This function should parse benchmark-specific options like:
     * --suite <PATH>
     * --workdir <PATH>
     * --outdir <PATH>
     */

    (void)argc;
    (void)argv;
    (void)tpb_args;

    return 0;
}

/* Public Function Implementations */

int
tpbcli_benchmark(int argc, char **argv)
{
    int err = 0;
    tpb_args_t tpb_args;

    /* Currently benchmark is not implemented */
    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Benchmark action is not implemented yet.\n");
    return TPBE_ILLEGAL_CALL;
}
