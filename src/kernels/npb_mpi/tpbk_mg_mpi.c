/*
 * tpbk_mg_mpi.c
 * PLI shim for NAS NPB-MPI MG.
 */

#include "tpb_npb_pli.h"

void npb_mg_run(void);

static int
_sf_run_mg(char graph)
{
    (void)graph;
    npb_mg_run();
    return 0;
}


int tpbk_pli_register_mg_mpi(void);

int
tpbk_pli_register_mg_mpi(void)
{
    return tpb_npb_pli_register("mg_mpi",
        "NAS NPB-MPI Multi-Grid",
        "SWABCDEF", 0);
}

int
tpbk_mg_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "mg_mpi", "MG",
        tpbk_pli_register_mg_mpi, _sf_run_mg, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_mg_mpi_entry(argc, argv);
}
