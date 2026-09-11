/*
 * tpbk_ep_mpi.c
 * PLI shim for NAS NPB-MPI EP.
 */

#include "tpb_npb_pli.h"

void npb_ep_run(void);

static int
_sf_run_ep(char graph)
{
    (void)graph;
    npb_ep_run();
    return 0;
}


int tpbk_pli_register_ep_mpi(void);

int
tpbk_pli_register_ep_mpi(void)
{
    return tpb_npb_pli_register("ep_mpi",
        "NAS NPB-MPI Embarrassingly Parallel",
        "SWABCDEF", 0);
}

int
tpbk_ep_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "ep_mpi", "EP",
        tpbk_pli_register_ep_mpi, _sf_run_ep, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_ep_mpi_entry(argc, argv);
}
