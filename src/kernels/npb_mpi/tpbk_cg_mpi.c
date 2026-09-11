/*
 * tpbk_cg_mpi.c
 * PLI shim for NAS NPB-MPI CG.
 */

#include "tpb_npb_pli.h"

void npb_cg_run(void);

static int
_sf_run_cg(char graph)
{
    (void)graph;
    npb_cg_run();
    return 0;
}


int tpbk_pli_register_cg_mpi(void);

int
tpbk_pli_register_cg_mpi(void)
{
    return tpb_npb_pli_register("cg_mpi",
        "NAS NPB-MPI Conjugate Gradient",
        "SWABCDEF", 0);
}

int
tpbk_cg_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "cg_mpi", "CG",
        tpbk_pli_register_cg_mpi, _sf_run_cg, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_cg_mpi_entry(argc, argv);
}
