/*
 * tpbk_ft_mpi.c
 * PLI shim for NAS NPB-MPI FT.
 */

#include "tpb_npb_pli.h"

void npb_ft_run(void);

static int
_sf_run_ft(char graph)
{
    (void)graph;
    npb_ft_run();
    return 0;
}


int tpbk_pli_register_ft_mpi(void);

int
tpbk_pli_register_ft_mpi(void)
{
    return tpb_npb_pli_register("ft_mpi",
        "NAS NPB-MPI Fourier Transform",
        "SWABCDEF", 0);
}

int
tpbk_ft_mpi_entry(int argc, char **argv)
{
    return tpb_npb_pli_entry(argc, argv, "ft_mpi", "FT",
        tpbk_pli_register_ft_mpi, _sf_run_ft, 0);
}

int
main(int argc, char **argv)
{
    return tpbk_ft_mpi_entry(argc, argv);
}
