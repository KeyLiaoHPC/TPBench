/*
 * tpb_npb_timer_meta.c
 * NPB timing-point names, slots, and notes for rafdb headers.
 */

#include <string.h>
#include "tpb_npb_timer_meta.h"

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const tpb_npb_timer_meta_t s_cg[] = {
    {1, 0, "t_total",
     "Official timed CG region (timer 1). invert_power_method loop in "
     "npb_cg_run (CG/cg.f90). NPB reduces t_last+2 derived values with "
     "MPI_Reduce SUM/MIN/MAX to rank 0 and prints min, max, SUM/nprocs."},
    {2, 0, "t_conjg",
     "conj_grad() body minus rcomm (CG/cg.f90). After timer_read, NPB "
     "sets t_conjg -= t_rcomm, then reduces SUM/MIN/MAX and prints "
     "SUM/nprocs as average."},
    {3, 0, "t_rcomm",
     "Reduction communication inside conj_grad() (CG/cg.f90). Reduced "
     "with MPI_Reduce SUM/MIN/MAX to rank 0; average is SUM/nprocs."},
    {4, 0, "t_ncomm",
     "Neighbor exchange communication in conj_grad() (CG/cg.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average is SUM/nprocs."},
    {5, 1, "t_totcomp",
     "Derived totcomp = t_total - (t_rcomm + t_ncomm) in npb_cg_run "
     "(CG/cg.f90). Reduced as part of the t_last+2 vector; average "
     "SUM/nprocs."},
    {6, 1, "t_totcomm",
     "Derived totcomm = t_rcomm + t_ncomm in npb_cg_run (CG/cg.f90). "
     "Reduced as part of the t_last+2 vector; average SUM/nprocs."},
};

static const tpb_npb_timer_meta_t s_ep[] = {
    {1, 0, "t_total",
     "Official timed EP region (timer 1) in npb_ep_run (EP/ep.f90). "
     "NPB reduces t_last+2 values with MPI_Reduce SUM/MIN/MAX to rank 0 "
     "and prints min, max, SUM/no_nodes."},
    {2, 0, "t_gpairs",
     "Gaussian-pair generation loop in npb_ep_run (EP/ep.f90). Reduced "
     "with MPI_Reduce SUM/MIN/MAX; average is SUM/no_nodes."},
    {3, 0, "t_randn",
     "Random-number generation in npb_ep_run (EP/ep.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average is SUM/no_nodes."},
    {4, 0, "t_rcomm",
     "Allreduce communication in npb_ep_run (EP/ep.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average is SUM/no_nodes."},
    {5, 1, "t_totcomp",
     "Derived totcomp = t_total - t_rcomm in npb_ep_run (EP/ep.f90). "
     "Reduced in the t_last+2 vector; average SUM/no_nodes."},
    {6, 1, "t_totcomm",
     "Derived totcomm = t_rcomm in npb_ep_run (EP/ep.f90). Reduced in "
     "the t_last+2 vector; average SUM/no_nodes."},
};

static const tpb_npb_timer_meta_t s_bt[] = {
    {1, 0, "t_total",
     "Official timed BT ADI region (timer 1) in npb_bt_run (BT/bt.f90). "
     "NPB reduces t_last values with MPI_Reduce SUM/MIN/MAX to rank 0 "
     "and prints min, max, SUM/no_nodes."},
    {2, 0, "t_io",
     "Optional BT I/O (BT/btio.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/no_nodes."},
    {3, 0, "t_rhs",
     "compute_rhs() residual formation (BT/rhs.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {4, 0, "t_xsolve",
     "x_solve() minus xcomm after timer_read (BT/x_solve.f90, BT/bt.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {5, 0, "t_ysolve",
     "y_solve() minus ycomm after timer_read (BT/y_solve.f90, BT/bt.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {6, 0, "t_zsolve",
     "z_solve() minus zcomm after timer_read (BT/z_solve.f90, BT/bt.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {7, 0, "t_bpack",
     "Block packing around face exchanges (BT/copy_faces.f90). Reduced "
     "with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {8, 0, "t_exch",
     "Face-exchange communication (BT/copy_faces.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {9, 0, "t_xcomm",
     "X-direction solver communication (BT/x_solve.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {10, 0, "t_ycomm",
     "Y-direction solver communication (BT/y_solve.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {11, 0, "t_zcomm",
     "Z-direction solver communication (BT/z_solve.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {12, 1, "t_comp",
     "Derived totcomp = t_total - t_comm in npb_bt_run (BT/bt.f90). Slot "
     "aliases t_enorm for I/O variants (bt_data.f90). Reduced over "
     "t_last; average SUM/no_nodes."},
    {13, 1, "t_comm",
     "Derived totcomm = xcomm+ycomm+zcomm+exch in npb_bt_run "
     "(BT/bt.f90). Slot aliases t_iov for I/O variants. Reduced over "
     "t_last; average SUM/no_nodes."},
};

static const tpb_npb_timer_meta_t s_sp[] = {
    {1, 0, "t_total",
     "Official timed SP ADI region (timer 1) in npb_sp_run (SP/sp.f90). "
     "NPB reduces t_last+2 values with MPI_Reduce SUM/MIN/MAX to rank 0 "
     "and prints min, max, SUM/no_nodes."},
    {2, 0, "t_rhs",
     "compute_rhs() residual formation (SP/rhs.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {3, 0, "t_xsolve",
     "x_solve() minus xcomm after timer_read (SP/x_solve.f90, SP/sp.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {4, 0, "t_ysolve",
     "y_solve() minus ycomm after timer_read (SP/y_solve.f90, SP/sp.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {5, 0, "t_zsolve",
     "z_solve() minus zcomm after timer_read (SP/z_solve.f90, SP/sp.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {6, 0, "t_bpack",
     "Block packing around face exchanges (SP/copy_faces.f90). Reduced "
     "with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {7, 0, "t_exch",
     "Face-exchange communication (SP/copy_faces.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {8, 0, "t_xcomm",
     "X-direction solver communication (SP/x_solve.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {9, 0, "t_ycomm",
     "Y-direction solver communication (SP/y_solve.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {10, 0, "t_zcomm",
     "Z-direction solver communication (SP/z_solve.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {11, 1, "t_totcomp",
     "Derived totcomp = t_total - totcomm in npb_sp_run (SP/sp.f90). "
     "Reduced in the t_last+2 vector; average SUM/no_nodes."},
    {12, 1, "t_totcomm",
     "Derived totcomm = xcomm+ycomm+zcomm+exch in npb_sp_run "
     "(SP/sp.f90). Reduced in the t_last+2 vector; average SUM/no_nodes."},
};

static const tpb_npb_timer_meta_t s_lu[] = {
    {1, 0, "t_total",
     "Official timed LU SSOR region (timer 1) in ssor() (LU/ssor.f90). "
     "NPB reduces t_last+2 values with MPI_Reduce SUM/MIN/MAX to rank 0 "
     "and prints min, max, SUM/no_nodes."},
    {2, 0, "t_rhs",
     "rhs() residual formation minus exch after timer_read "
     "(LU/rhs.f90, LU/lu.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/no_nodes."},
    {3, 0, "t_blts",
     "blts() lower-triangular solve, once per k-plane inside the do k "
     "loop of ssor() (LU/ssor.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/no_nodes."},
    {4, 0, "t_buts",
     "buts() upper-triangular solve, once per k-plane inside the do k "
     "loop of ssor() (LU/ssor.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/no_nodes."},
    {5, 0, "t_jacld",
     "jacld() lower Jacobian (LU/jacld.f90). NPB print label is #jacld "
     "and is omitted from the SECTION table; recorded here. Reduced with "
     "the t_last+2 vector; average SUM/no_nodes."},
    {6, 0, "t_jacu",
     "jacu() upper Jacobian (LU/jacu.f90). NPB print label is #jacu and "
     "is omitted from the SECTION table; recorded here. Reduced with "
     "the t_last+2 vector; average SUM/no_nodes."},
    {7, 0, "t_exch",
     "Halo exchange around rhs() (LU/exchange_*.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {8, 0, "t_lcomm",
     "Lower-solve plane communication inside ssor() do k (LU/ssor.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {9, 0, "t_ucomm",
     "Upper-solve plane communication inside ssor() do k (LU/ssor.f90). "
     "Reduced with MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {10, 0, "t_rcomm",
     "rhs() residual communication (LU/rhs.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/no_nodes."},
    {11, 1, "t_totcomp",
     "Derived totcomp = t_total - totcomm in npb_lu_run (LU/lu.f90). "
     "Reduced in the t_last+2 vector; average SUM/no_nodes."},
    {12, 1, "t_totcomm",
     "Derived totcomm = lcomm+ucomm+rcomm+exch in npb_lu_run "
     "(LU/lu.f90). Reduced in the t_last+2 vector; average SUM/no_nodes."},
};

static const tpb_npb_timer_meta_t s_mg[] = {
    {1, 0, "t_bench",
     "Official timed MG V-cycle region (timer 1, printed as total) in "
     "npb_mg_run (MG/mg.f90). NPB reduces t_last+2 values with "
     "MPI_Reduce SUM/MIN/MAX and prints min, max, SUM/nprocs."},
    {2, 0, "t_init",
     "zero3/setup initialization (MG/mg.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/nprocs."},
    {3, 0, "t_psinv",
     "psinv() smoother (MG/mg.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/nprocs."},
    {4, 0, "t_resid",
     "resid() residual (MG/mg.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/nprocs."},
    {5, 0, "t_rprj3",
     "rprj3() restriction (MG/mg.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/nprocs."},
    {6, 0, "t_interp",
     "interp() prolongation (MG/mg.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/nprocs."},
    {7, 0, "t_norm2u3",
     "norm2u3() residual norm (MG/mg.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/nprocs."},
    {8, 0, "t_comm3",
     "comm3() halo exchange (MG/mg.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/nprocs."},
    {9, 0, "t_rcomm",
     "Reduction communication around norm2u3 (MG/mg.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/nprocs."},
    {10, 1, "t_totcomp",
     "Derived totcomp = t_bench - (t_rcomm + t_comm3) in npb_mg_run "
     "(MG/mg.f90). Reduced in the t_last+2 vector; average SUM/nprocs."},
    {11, 1, "t_totcomm",
     "Derived totcomm = t_rcomm + t_comm3 in npb_mg_run (MG/mg.f90). "
     "Reduced in the t_last+2 vector; average SUM/nprocs."},
};

static const tpb_npb_timer_meta_t s_ft[] = {
    {1, 0, "T_total",
     "Official timed FT region (timer 1) in npb_ft_run (FT/ft.f90). "
     "print_timers reduces T_max+2 values with MPI_Reduce SUM/MIN/MAX "
     "and prints min, max, SUM/np_min (zero-sum rows skipped)."},
    {2, 0, "T_setup",
     "setup() index maps (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {3, 0, "T_fft",
     "fft() 3-D FFT (FT/ft.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/np_min."},
    {4, 0, "T_evolve",
     "evolve() time-step multiply (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {5, 0, "T_checksum",
     "checksum() (FT/ft.f90). Reduced with MPI_Reduce SUM/MIN/MAX; "
     "average SUM/np_min."},
    {6, 0, "T_fftlow",
     "Low-level 1-D FFT (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {7, 0, "T_fftcopy",
     "FFT buffer copies (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {8, 0, "T_transpose",
     "Outer transpose wrapper (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {9, 0, "T_transxzloc",
     "XZ transpose local phase (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {10, 0, "T_transxzglo",
     "XZ transpose all-to-all (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {11, 0, "T_transxzfin",
     "XZ transpose finish (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {12, 0, "T_transxyloc",
     "XY transpose local phase (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {13, 0, "T_transxyglo",
     "XY transpose all-to-all (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {14, 0, "T_transxyfin",
     "XY transpose finish (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {15, 0, "T_synch",
     "synchup() barrier around checksum (FT/ft.f90). Reduced with "
     "MPI_Reduce SUM/MIN/MAX; average SUM/np_min."},
    {16, 0, "T_init",
     "compute_initial_conditions (FT/ft.f90). Reduced with MPI_Reduce "
     "SUM/MIN/MAX; average SUM/np_min."},
    {17, 1, "T_totcomp",
     "Derived totcomp = T_total - totcomm in print_timers (FT/ft.f90). "
     "Reduced in the T_max+2 vector; average SUM/np_min."},
    {18, 1, "T_totcomm",
     "Derived totcomm = T_transxzglo + T_transxyglo + T_synch in "
     "print_timers (FT/ft.f90). Reduced in the T_max+2 vector; average "
     "SUM/np_min."},
};

static const tpb_npb_timer_meta_t s_is[] = {
    {0, 0, "T_TOTAL",
     "Official timed IS region (timer 0) after warmup rank(1) (IS/is.c). "
     "NPB reduces T_LAST+1 values with MPI_Reduce MIN/SUM/MAX to rank 0 "
     "and prints min, max, SUM/comm_size (printed index is i+1)."},
    {1, 0, "T_RANK",
     "rank() integer-sort compute, printed as rcomp (IS/is.c). Reduced "
     "with MPI_Reduce MIN/SUM/MAX; average SUM/comm_size."},
    {2, 0, "T_RCOMM",
     "rank() communication, printed as rcomm (IS/is.c). Reduced with "
     "MPI_Reduce MIN/SUM/MAX; average SUM/comm_size."},
    {3, 0, "T_VERIFY",
     "full_verify() (IS/is.c). Reduced with MPI_Reduce MIN/SUM/MAX; "
     "average SUM/comm_size."},
};

typedef struct tpb_npb_bench_meta {
    const char *bench;
    int ntimer;
    int total_slot;
    int slot_base;
    const tpb_npb_timer_meta_t *rows;
} tpb_npb_bench_meta_t;

static const tpb_npb_bench_meta_t s_benches[] = {
    {"CG", N(s_cg), 1, 1, s_cg},
    {"EP", N(s_ep), 1, 1, s_ep},
    {"BT", N(s_bt), 1, 1, s_bt},
    {"SP", N(s_sp), 1, 1, s_sp},
    {"LU", N(s_lu), 1, 1, s_lu},
    {"MG", N(s_mg), 1, 1, s_mg},
    {"FT", N(s_ft), 1, 1, s_ft},
    {"IS", N(s_is), 0, 0, s_is},
    {"DT", 0, 0, 0, NULL},
};

static const tpb_npb_bench_meta_t *_sf_find_bench(const char *bench);

static const tpb_npb_bench_meta_t *
_sf_find_bench(const char *bench)
{
    int i;

    if (bench == NULL) {
        return NULL;
    }
    for (i = 0; i < N(s_benches); i++) {
        if (strcmp(s_benches[i].bench, bench) == 0) {
            return &s_benches[i];
        }
    }
    return NULL;
}

const tpb_npb_timer_meta_t *
tpb_npb_timer_meta(const char *bench, int *count)
{
    const tpb_npb_bench_meta_t *b;

    b = _sf_find_bench(bench);
    if (count != NULL) {
        *count = (b != NULL) ? b->ntimer : 0;
    }
    if (b == NULL) {
        return NULL;
    }
    return b->rows;
}

int
tpb_npb_timer_total_slot(const char *bench)
{
    const tpb_npb_bench_meta_t *b;

    b = _sf_find_bench(bench);
    if (b == NULL) {
        return 1;
    }
    return b->total_slot;
}

int
tpb_npb_timer_slot_base(const char *bench)
{
    const tpb_npb_bench_meta_t *b;

    b = _sf_find_bench(bench);
    if (b == NULL) {
        return 1;
    }
    return b->slot_base;
}
