/*
 * tpbk_lu_mpi.c
 * PLI shim for NAS NPB-MPI LU.
 */

#include "tpb_npb_pli.h"

void npb_lu_run(void);

static int
_sf_run_lu(char graph)
{
    (void)graph;
    npb_lu_run();
    return 0;
}


int tpbk_pli_register_lu_mpi(void);

int
tpbk_pli_register_lu_mpi(void)
{
    return tpb_npb_pli_register("lu_mpi",
        "NAS NPB-MPI Lower-Upper Gauss-Seidel",
        "SWABCDEF", 0);
}

int
tpbk_lu_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "lu_mpi", "LU",
        tpbk_pli_register_lu_mpi, _sf_run_lu, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_lu_mpi_entry(argc, argv);
}
