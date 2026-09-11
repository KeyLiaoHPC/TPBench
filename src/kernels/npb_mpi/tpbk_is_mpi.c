/*
 * tpbk_is_mpi.c
 * PLI shim for NAS NPB-MPI IS.
 */

#include "tpb_npb_pli.h"

int npb_is_run(void);

static int
_sf_run_is(char graph)
{
    (void)graph;
    return npb_is_run();
}


int tpbk_pli_register_is_mpi(void);

int
tpbk_pli_register_is_mpi(void)
{
    return tpb_npb_pli_register("is_mpi",
        "NAS NPB-MPI Integer Sort",
        "SWABCDE", 0);
}

int
tpbk_is_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "is_mpi", "IS",
        tpbk_pli_register_is_mpi, _sf_run_is, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_is_mpi_entry(argc, argv);
}
