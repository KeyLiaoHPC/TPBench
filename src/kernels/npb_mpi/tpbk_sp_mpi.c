/*
 * tpbk_sp_mpi.c
 * PLI shim for NAS NPB-MPI SP.
 */

#include "tpb_npb_pli.h"

void npb_sp_run(void);

static int
_sf_run_sp(char graph)
{
    (void)graph;
    npb_sp_run();
    return 0;
}


int tpbk_pli_register_sp_mpi(void);

int
tpbk_pli_register_sp_mpi(void)
{
    return tpb_npb_pli_register("sp_mpi",
        "NAS NPB-MPI Scalar Pentadiagonal",
        "SWABCDEF", 0);
}

int
tpbk_sp_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "sp_mpi", "SP",
        tpbk_pli_register_sp_mpi, _sf_run_sp, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_sp_mpi_entry(argc, argv);
}
