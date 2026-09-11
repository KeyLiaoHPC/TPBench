/*
 * tpbk_bt_mpi.c
 * PLI shim for NAS NPB-MPI BT.
 */

#include "tpb_npb_pli.h"

void npb_bt_run(void);

static int
_sf_run_bt(char graph)
{
    (void)graph;
    npb_bt_run();
    return 0;
}


int tpbk_pli_register_bt_mpi(void);

int
tpbk_pli_register_bt_mpi(void)
{
    return tpb_npb_pli_register("bt_mpi",
        "NAS NPB-MPI Block Tridiagonal",
        "SWABCDEF", 0);
}

int
tpbk_bt_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "bt_mpi", "BT",
        tpbk_pli_register_bt_mpi, _sf_run_bt, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_bt_mpi_entry(argc, argv);
}
