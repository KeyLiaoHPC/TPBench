/*
 * tpbk_dt_mpi.c
 * PLI shim for NAS NPB-MPI DT.
 */

#include "tpb_npb_pli.h"

int npb_dt_run(char graph);

static int
_sf_run_dt(char graph)
{
    return npb_dt_run(graph);
}


int tpbk_pli_register_dt_mpi(void);

int
tpbk_pli_register_dt_mpi(void)
{
    return tpb_npb_pli_register("dt_mpi",
        "NAS NPB-MPI Data Traffic",
        "SWABCD", 1);
}

int
tpbk_dt_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "dt_mpi", "DT",
        tpbk_pli_register_dt_mpi, _sf_run_dt, 1);
}

int
main(int argc, char **argv)
{
    return tpbk_dt_mpi_entry(argc, argv);
}
