/*
 * tpb-benchmark.c
 * Benchmark subcommand skeleton.
 */

#include "tpb-launch.h"
#include "tpb-io.h"
#include "tpb-types.h"

int
tpb_benchmark(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Benchmark action is not implemented yet.\n");
    return TPBE_ILLEGAL_CALL;
}
