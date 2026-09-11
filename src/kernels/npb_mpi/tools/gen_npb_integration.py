#!/usr/bin/env python3
"""Generate NPB-MPI PLI shims, npbparams, and apply NASA source patches."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor"
INCLUDE = ROOT / "include"

KERNELS = [
    ("bt_mpi", "BT", "NAS NPB-MPI Block Tridiagonal", "SWABCDEF", 0),
    ("sp_mpi", "SP", "NAS NPB-MPI Scalar Pentadiagonal", "SWABCDEF", 0),
    ("lu_mpi", "LU", "NAS NPB-MPI Lower-Upper Gauss-Seidel", "SWABCDEF", 0),
    ("cg_mpi", "CG", "NAS NPB-MPI Conjugate Gradient", "SWABCDEF", 0),
    ("mg_mpi", "MG", "NAS NPB-MPI Multi-Grid", "SWABCDEF", 0),
    ("ft_mpi", "FT", "NAS NPB-MPI Fourier Transform", "SWABCDEF", 0),
    ("ep_mpi", "EP", "NAS NPB-MPI Embarrassingly Parallel", "SWABCDEF", 0),
    ("is_mpi", "IS", "NAS NPB-MPI Integer Sort", "SWABCDE", 0),
    ("dt_mpi", "DT", "NAS NPB-MPI Data Traffic", "SWABCD", 1),
]

COMPILE_INFO = """
      logical convertdouble
      parameter (convertdouble = .false.)
      character*11 compiletime
      parameter (compiletime='TPBench')
      character*5 npbversion
      parameter (npbversion='3.4.4')
      character*8 cs1
      parameter (cs1='tpbench')
      character*6 cs2
      parameter (cs2='(none)')
      character*6 cs3
      parameter (cs3='(none)')
      character*6 cs4
      parameter (cs4='(none)')
      character*6 cs5
      parameter (cs5='(none)')
      character*6 cs6
      parameter (cs6='(none)')
      character*6 cs7
      parameter (cs7='randi8')
"""

C_COMPILE_INFO = """
#define NPBVERSION "3.4.4"
#define COMPILETIME "TPBench"
#define MPICC "(none)"
#define CLINK "(none)"
#define CMPI_LIB "(none)"
#define CMPI_INC "(none)"
#define CFLAGS "(none)"
#define CLINKFLAGS "(none)"
"""

NPBPARAMS = {
    "bt_mpi": """
      character class
      integer problem_size, niter_default
      double precision dt_default
      integer wr_default
      parameter (wr_default=5)
      integer iotype
      parameter (iotype=0)
""" + COMPILE_INFO,
    "sp_mpi": """
      character class
      integer problem_size, niter_default
      double precision dt_default
""" + COMPILE_INFO,
    "lu_mpi": """
      character class
      integer isiz01, isiz02, isiz03
      integer itmax_default, inorm_default
      double precision dt_default
""" + COMPILE_INFO,
    "cg_mpi": """
      character class
      integer na, nonzer, niter
      double precision shift, rcond
""" + COMPILE_INFO,
    "mg_mpi": """
      integer nx_default, ny_default, nz_default
      integer nit_default, lt_default
      integer debug_default
      parameter (debug_default=0)
""" + COMPILE_INFO,
    "ft_mpi": """
      character class
      integer nx, ny, nz, maxdim, niter_default
""" + COMPILE_INFO,
    "ep_mpi": """
      character class
      integer m
""" + COMPILE_INFO,
    "is_mpi": """
#include "tpb_npb_class.h"
#define CLASS tpb_npb_c_class
""" + C_COMPILE_INFO,
    "dt_mpi": """
#include "tpb_npb_class.h"
#define CLASS tpb_npb_c_class
#define NUM_SAMPLES tpb_npb_dt_num_samples
#define STD_DEVIATION tpb_npb_dt_std_deviation
#define NUM_SOURCES tpb_npb_dt_num_sources
""" + C_COMPILE_INFO,
}

TPBK_C = """/*
 * tpbk_{kname}.c
 * PLI shim for NAS NPB-MPI {bench}.
 */

#include "tpb_npb_pli.h"

{run_decl}

int tpbk_pli_register_{kname}(void);

int
tpbk_pli_register_{kname}(void)
{{
    return tpb_npb_pli_register("{kname}",
        "{note}",
        "{clist}", {has_graph});
}}

int
tpbk_{kname}_entry(int argc, char **argv)
{{
    return tpb_npb_pli_entry(argc, argv, "{kname}", "{bench}",
        tpbk_pli_register_{kname}, {run_wrap}, {has_graph});
}}

int
main(int argc, char **argv)
{{
    return tpbk_{kname}_entry(argc, argv);
}}
"""

F_WRAP = """static int
_sf_run_{bench_l}(char graph)
{{
    (void)graph;
    npb_{bench_l}_run();
    return 0;
}}
"""


def write_tpbk():
    for kname, bench, note, clist, has_graph in KERNELS:
        bench_l = bench.lower()
        if kname == "is_mpi":
            run_decl = "int npb_is_run(void);\n\nstatic int\n_sf_run_is(char graph)\n{\n    (void)graph;\n    return npb_is_run();\n}\n"
            run_wrap = "_sf_run_is"
        elif kname == "dt_mpi":
            run_decl = "int npb_dt_run(char graph);\n\nstatic int\n_sf_run_dt(char graph)\n{\n    return npb_dt_run(graph);\n}\n"
            run_wrap = "_sf_run_dt"
        else:
            run_decl = f"void npb_{bench_l}_run(void);\n\n" + F_WRAP.format(bench_l=bench_l)
            run_wrap = f"_sf_run_{bench_l}"
        text = TPBK_C.format(
            kname=kname, bench=bench, note=note, clist=clist,
            has_graph=has_graph, run_decl=run_decl, run_wrap=run_wrap)
        path = ROOT / f"tpbk_{kname}.c"
        path.write_text(text)
        print("wrote", path)


def write_npbparams():
    for kname, body in NPBPARAMS.items():
        d = INCLUDE / kname
        d.mkdir(parents=True, exist_ok=True)
        path = d / "npbparams.h"
        path.write_text(body.lstrip("\n") if not body.startswith("\n#include") else body.lstrip("\n"))
        # keep leading include form
        path.write_text(body.lstrip("\n"))
        print("wrote", path)


def repl(path: Path, old: str, new: str, required=True):
    text = path.read_text()
    if old not in text:
        if required:
            raise SystemExit(f"missing patch text in {path}:\n{old[:80]}")
        return False
    path.write_text(text.replace(old, new, 1))
    print("patched", path, "->", old.splitlines()[0][:60])
    return True


def patch_fortran_data():
    # EP
    p = VENDOR / "EP" / "ep_data.f90"
    repl(p,
         "      integer    mk, mm, nn, nk, nq\n"
         "      parameter (mk = 16, mm = m - mk, nn = 2 ** mm,  &\n"
         "     &           nk = 2 ** mk, nq = 10)\n",
         "      integer    mk, mm, nn, nk, nq\n"
         "      parameter (mk = 16, nk = 2 ** mk, nq = 10)\n")
    p.write_text(p.read_text() + """
      subroutine tpb_npb_ep_set_class(cls, mval) bind(C, name="tpb_npb_ep_set_class")
      use iso_c_binding
      use ep_data
      implicit none
      character(c_char), value :: cls
      integer(c_int), value :: mval
      class = cls
      m = mval
      mm = m - mk
      nn = 2 ** mm
      return
      end
""")

    # BT sum array
    p = VENDOR / "BT" / "bt_data.f90"
    repl(p,
         "      double precision sum(niter_default), xce_sub(5)\n",
         "      integer niter_max\n"
         "      parameter (niter_max=500)\n"
         "      double precision sum(niter_max), xce_sub(5)\n")
    p.write_text(p.read_text() + """
      subroutine tpb_npb_bt_set_class(psize, niter, dtval) bind(C, name="tpb_npb_bt_set_class")
      use iso_c_binding
      use bt_data
      implicit none
      integer(c_int), value :: psize, niter
      real(c_double), value :: dtval
      problem_size = psize
      niter_default = niter
      dt_default = dtval
      class = 'S'
      return
      end
""")

    # SP
    p = VENDOR / "SP" / "sp_data.f90"
    p.write_text(p.read_text() + """
      subroutine tpb_npb_sp_set_class(psize, niter, dtval) bind(C, name="tpb_npb_sp_set_class")
      use iso_c_binding
      use sp_data
      implicit none
      integer(c_int), value :: psize, niter
      real(c_double), value :: dtval
      problem_size = psize
      niter_default = niter
      dt_default = dtval
      class = 'S'
      return
      end
""")

    # LU
    p = VENDOR / "LU" / "lu_data.f90"
    p.write_text(p.read_text() + """
      subroutine tpb_npb_lu_set_class(psize, itmax, dtval) bind(C, name="tpb_npb_lu_set_class")
      use iso_c_binding
      use lu_data
      implicit none
      integer(c_int), value :: psize, itmax
      real(c_double), value :: dtval
      isiz01 = psize
      isiz02 = psize
      isiz03 = psize
      itmax_default = itmax
      inorm_default = itmax
      dt_default = dtval
      class = 'S'
      return
      end
""")

    # CG
    p = VENDOR / "CG" / "cg_data.f90"
    # insert setter after module end of cg_data (before timing module)
    text = p.read_text()
    marker = "      end module cg_data\n"
    setter = """      end module cg_data

      subroutine tpb_npb_cg_set_class(naval, nonz, nit, shft) bind(C, name="tpb_npb_cg_set_class")
      use iso_c_binding
      use cg_data
      implicit none
      integer(c_int), value :: naval, nonz, nit
      real(c_double), value :: shft
      na = naval
      nonzer = nonz
      niter = nit
      shift = shft
      rcond = 1.0d-1
      class = 'S'
      return
      end
"""
    if marker not in text:
        raise SystemExit("cg_data end module missing")
    p.write_text(text.replace(marker, setter, 1))

    # FT
    p = VENDOR / "FT" / "ft_data.f90"
    repl(p,
         "      double precision ntotal_f, nx_f\n"
         "      parameter (nx_f=dble(nx))\n"
         "      parameter (ntotal_f=nx_f*ny*nz)\n",
         "      double precision ntotal_f, nx_f\n")
    repl(p,
         "      double complex sums(0:niter_default)\n",
         "      double complex, allocatable :: sums(:)\n")
    p.write_text(p.read_text() + """
      subroutine tpb_npb_ft_set_class(nxv, nyv, nzv, nit) bind(C, name="tpb_npb_ft_set_class")
      use iso_c_binding
      use ft_data
      implicit none
      integer(c_int), value :: nxv, nyv, nzv, nit
      nx = nxv
      ny = nyv
      nz = nzv
      niter_default = nit
      maxdim = max(nx, max(ny, nz))
      nx_f = dble(nx)
      ntotal_f = nx_f * dble(ny) * dble(nz)
      if (allocated(sums)) deallocate(sums)
      allocate(sums(0:niter_default))
      return
      end
""")

    # MG
    p = VENDOR / "MG" / "mg_data.f90"
    repl(p,
         "      parameter (maxlevel = lt_default+1)\n",
         "      parameter (maxlevel = 16)\n")
    text = p.read_text()
    # Append setter after the first module; keep mg_fields module intact.
    insert_at = text.find("      end module mg_data")
    if insert_at < 0:
        raise SystemExit("mg_data end module missing")
    insert_at = text.find("\n", insert_at) + 1
    setter = """
      subroutine tpb_npb_mg_set_class(nxv, nit, ltv) bind(C, name="tpb_npb_mg_set_class")
      use iso_c_binding
      use mg_data
      implicit none
      integer(c_int), value :: nxv, nit, ltv
      nx_default = nxv
      ny_default = nxv
      nz_default = nxv
      nit_default = nit
      lt_default = ltv
      return
      end
"""
    p.write_text(text[:insert_at] + setter + text[insert_at:])


def patch_programs():
    # EP
    p = VENDOR / "EP" / "ep.f90"
    repl(p, "      program EMBAR\n",
         "      subroutine npb_ep_run() bind(C, name=\"npb_ep_run\")\n")
    repl(p, "      call mpi_init(ierr)\n", "")
    repl(p,
         "         call print_results('EP', class, m+1, 0, 0, nit, no_nodes,  &\n"
         "     &                      no_nodes, tm, Mops,  &\n"
         "     &                      'Random numbers generated',  &\n"
         "     &                      verified, npbversion, compiletime, cs1,  &\n"
         "     &                      cs2, cs3, cs4, cs5, cs6, cs7)\n",
         "         call print_results('EP', class, m+1, 0, 0, nit, no_nodes,  &\n"
         "     &                      no_nodes, tm, Mops,  &\n"
         "     &                      'Random numbers generated',  &\n"
         "     &                      verified, npbversion, compiletime, cs1,  &\n"
         "     &                      cs2, cs3, cs4, cs5, cs6, cs7)\n"
         "         call tpb_npb_set_result_f(tm, Mops, verified, nit,  &\n"
         "     &        m+1, 0, 0, class)\n")
    repl(p, "      call mpi_finalize(ierr)\n\n      end\n",
         "      end subroutine npb_ep_run\n")

    # CG
    p = VENDOR / "CG" / "cg.f90"
    repl(p, "      program cg\n",
         "      subroutine npb_cg_run() bind(C, name=\"npb_cg_run\")\n")
    repl(p, "      call mpi_init( ierr )\n", "")
    repl(p,
         "         call print_results('CG', class, na, 0, 0,  &\n"
         "     &                      niter, nprocs, total_nodes, tmax,  &\n"
         "     &                      mflops, '          floating point',  &\n"
         "     &                      verified, npbversion, compiletime,  &\n"
         "     &                      cs1, cs2, cs3, cs4, cs5, cs6, cs7)\n",
         "         call print_results('CG', class, na, 0, 0,  &\n"
         "     &                      niter, nprocs, total_nodes, tmax,  &\n"
         "     &                      mflops, '          floating point',  &\n"
         "     &                      verified, npbversion, compiletime,  &\n"
         "     &                      cs1, cs2, cs3, cs4, cs5, cs6, cs7)\n"
         "         call tpb_npb_set_result_f(tmax, mflops, verified, niter,  &\n"
         "     &        na, 0, 0, class)\n")
    repl(p, "      call mpi_finalize(ierr)\n\n\n\n      end                              ! end main\n",
         "      end subroutine npb_cg_run\n")

    # BT
    p = VENDOR / "BT" / "bt.f90"
    repl(p, "       program MPBT\n",
         "       subroutine npb_bt_run() bind(C, name=\"npb_bt_run\")\n")
    repl(p,
         "         call print_results('BT', class, grid_points(1),  &\n"
         "     &     grid_points(2), grid_points(3), niter, no_nodes,  &\n"
         "     &     total_nodes, tmax, mflops, '          floating point',  &\n"
         "     &     verified, npbversion,compiletime, cs1, cs2, cs3, cs4, cs5,  &\n"
         "     &     cs6, '(none)')\n",
         "         call print_results('BT', class, grid_points(1),  &\n"
         "     &     grid_points(2), grid_points(3), niter, no_nodes,  &\n"
         "     &     total_nodes, tmax, mflops, '          floating point',  &\n"
         "     &     verified, npbversion,compiletime, cs1, cs2, cs3, cs4, cs5,  &\n"
         "     &     cs6, '(none)')\n"
         "         call tpb_npb_set_result_f(tmax, mflops, verified, niter,  &\n"
         "     &        grid_points(1), grid_points(2), grid_points(3), class)\n")
    repl(p, "       call mpi_finalize(error)\n\n       end\n",
         "       end subroutine npb_bt_run\n")
    repl(VENDOR / "BT" / "setup_mpi.f90", "      call mpi_init(error)\n", "")

    # SP
    p = VENDOR / "SP" / "sp.f90"
    repl(p, "       program MPSP\n",
         "       subroutine npb_sp_run() bind(C, name=\"npb_sp_run\")\n")
    repl(p,
         "         call print_results('SP', class, grid_points(1),  &\n"
         "     &     grid_points(2), grid_points(3), niter, no_nodes,  &\n"
         "     &     total_nodes, tmax, mflops, '          floating point',  &\n"
         "     &     verified, npbversion,compiletime, cs1, cs2, cs3, cs4, cs5,  &\n"
         "     &     cs6, '(none)')\n",
         "         call print_results('SP', class, grid_points(1),  &\n"
         "     &     grid_points(2), grid_points(3), niter, no_nodes,  &\n"
         "     &     total_nodes, tmax, mflops, '          floating point',  &\n"
         "     &     verified, npbversion,compiletime, cs1, cs2, cs3, cs4, cs5,  &\n"
         "     &     cs6, '(none)')\n"
         "         call tpb_npb_set_result_f(tmax, mflops, verified, niter,  &\n"
         "     &        grid_points(1), grid_points(2), grid_points(3), class)\n")
    repl(p, "       call mpi_finalize(error)\n\n       end\n",
         "       end subroutine npb_sp_run\n")
    repl(VENDOR / "SP" / "setup_mpi.f90", "      call mpi_init(error)\n", "")

    # LU
    p = VENDOR / "LU" / "lu.f90"
    repl(p, "      program applu\n",
         "      subroutine npb_lu_run() bind(C, name=\"npb_lu_run\")\n")
    repl(p,
         "         call print_results('LU', class, nx0,  &\n"
         "     &     ny0, nz0, itmax, no_nodes, total_nodes,  &\n"
         "     &     maxtime, mflops, '          floating point', verified,  &\n"
         "     &     npbversion, compiletime, cs1, cs2, cs3, cs4, cs5, cs6,  &\n"
         "     &     '(none)')\n",
         "         call print_results('LU', class, nx0,  &\n"
         "     &     ny0, nz0, itmax, no_nodes, total_nodes,  &\n"
         "     &     maxtime, mflops, '          floating point', verified,  &\n"
         "     &     npbversion, compiletime, cs1, cs2, cs3, cs4, cs5, cs6,  &\n"
         "     &     '(none)')\n"
         "         call tpb_npb_set_result_f(maxtime, mflops, verified, itmax,  &\n"
         "     &        nx0, ny0, nz0, class)\n")
    repl(p, "      call mpi_finalize(ierr)\n      end\n",
         "      end subroutine npb_lu_run\n")
    repl(VENDOR / "LU" / "init_comm.f90", "      call MPI_INIT( IERROR )\n", "")

    # MG
    p = VENDOR / "MG" / "mg.f90"
    repl(p, "      program mg_mpi\n",
         "      subroutine npb_mg_run() bind(C, name=\"npb_mg_run\")\n")
    repl(p, "      call mpi_init(ierr)\n", "")
    repl(p,
         "         call print_results('MG', class, nx(lt), ny(lt), nz(lt),  &\n"
         "     &                      nit, nprocs, nprocs_total, t,  &\n"
         "     &                      mflops, '          floating point',  &\n"
         "     &                      verified, npbversion, compiletime,  &\n"
         "     &                      cs1, cs2, cs3, cs4, cs5, cs6, cs7)\n",
         "         call print_results('MG', class, nx(lt), ny(lt), nz(lt),  &\n"
         "     &                      nit, nprocs, nprocs_total, t,  &\n"
         "     &                      mflops, '          floating point',  &\n"
         "     &                      verified, npbversion, compiletime,  &\n"
         "     &                      cs1, cs2, cs3, cs4, cs5, cs6, cs7)\n"
         "         call tpb_npb_set_result_f(t, mflops, verified, nit,  &\n"
         "     &        nx(lt), ny(lt), nz(lt), class)\n")
    repl(p, "      call mpi_finalize(ierr)\n      end\n",
         "      end subroutine npb_mg_run\n")

    # FT
    p = VENDOR / "FT" / "ft.f90"
    repl(p, "      program ft\n",
         "      subroutine npb_ft_run() bind(C, name=\"npb_ft_run\")\n")
    repl(p, "      call MPI_Init(ierr)\n", "")
    repl(p,
         "         call print_results('FT', class, nx, ny, nz, niter, np_min, np,  &\n"
         "     &     total_time, mflops, '          floating point', verified,  &\n"
         "     &     npbversion, compiletime, cs1, cs2, cs3, cs4, cs5, cs6, cs7)\n",
         "         call print_results('FT', class, nx, ny, nz, niter, np_min, np,  &\n"
         "     &     total_time, mflops, '          floating point', verified,  &\n"
         "     &     npbversion, compiletime, cs1, cs2, cs3, cs4, cs5, cs6, cs7)\n"
         "         call tpb_npb_set_result_f(total_time, mflops, verified, niter,  &\n"
         "     &        nx, ny, nz, class)\n")
    repl(p, "      call MPI_Finalize(ierr)\n      end\n",
         "      end subroutine npb_ft_run\n")


def patch_is():
    p = VENDOR / "IS" / "is.c"
    text = p.read_text()
    old = """#ifndef CLASS
#define CLASS 'S'
#define NUM_PROCS            1                 
#endif
#define MIN_PROCS            1
#define ONE                  1


/*************/
/*  CLASS S  */
/*************/
#if CLASS == 'S'
#define  TOTAL_KEYS_LOG_2    16
#define  MAX_KEY_LOG_2       11
#define  NUM_BUCKETS_LOG_2   9
#endif


/*************/
/*  CLASS W  */
/*************/
#if CLASS == 'W'
#define  TOTAL_KEYS_LOG_2    20
#define  MAX_KEY_LOG_2       16
#define  NUM_BUCKETS_LOG_2   10
#endif

/*************/
/*  CLASS A  */
/*************/
#if CLASS == 'A'
#define  TOTAL_KEYS_LOG_2    23
#define  MAX_KEY_LOG_2       19
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS B  */
/*************/
#if CLASS == 'B'
#define  TOTAL_KEYS_LOG_2    25
#define  MAX_KEY_LOG_2       21
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS C  */
/*************/
#if CLASS == 'C'
#define  TOTAL_KEYS_LOG_2    27
#define  MAX_KEY_LOG_2       23
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS D  */
/*************/
#if CLASS == 'D'
#define  TOTAL_KEYS_LOG_2    29     /* 2^31 */
#define  MAX_KEY_LOG_2       27
#define  NUM_BUCKETS_LOG_2   10
#undef   MIN_PROCS
#define  MIN_PROCS           4
#endif


/*************/
/*  CLASS E  */
/*************/
#if CLASS == 'E'
#define  TOTAL_KEYS_LOG_2    29     /* 2^35 */
#define  MAX_KEY_LOG_2       31
#define  NUM_BUCKETS_LOG_2   10
#undef   MIN_PROCS
#define  MIN_PROCS           64
#undef   ONE
#define  ONE                 1L
#endif


/*******************************************************************
 * Defining MIN_PROCS is to avoid integer overflow for large problem 
 * sizes without using a larger integer type, such as long int.
 * The actual total keys = TOTAL_KEYS * MIN_PROCS
 *******************************************************************/
#define  TOTAL_KEYS          (1 << TOTAL_KEYS_LOG_2)

#define  MAX_KEY             (ONE << MAX_KEY_LOG_2)
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
"""
    new = """#include "tpb_npb_class.h"
#include "tpb_npb_result.h"
#define MIN_PROCS            tpb_npb_is_min_procs
#define ONE                  1L
#define  TOTAL_KEYS_LOG_2    tpb_npb_is_total_keys_log2
#define  MAX_KEY_LOG_2       tpb_npb_is_max_key_log2
#define  NUM_BUCKETS_LOG_2   tpb_npb_is_num_buckets_log2
#define  TOTAL_KEYS          (1L << TOTAL_KEYS_LOG_2)
#define  MAX_KEY             (ONE << MAX_KEY_LOG_2)
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
#define  NUM_BUCKETS_MAX     1024
"""
    if old not in text:
        raise SystemExit("IS class macros not found")
    text = text.replace(old, new, 1)
    text = text.replace(
        """#if CLASS == 'S'
#define  MAX_PROCS           128
#else
#define  MAX_PROCS           1024
#endif
""",
        "#define  MAX_PROCS           tpb_npb_is_max_procs\n")
    text = text.replace(
        """#if CLASS == 'D' || CLASS == 'E'
typedef  long KEY_TYPE;
#else
typedef  int  KEY_TYPE;
#endif
""",
        "typedef  long KEY_TYPE;\n")
    text = text.replace(
        "         bucket_size[NUM_BUCKETS+TEST_ARRAY_SIZE],     /* Top 5 elements for */\n"
        "         bucket_size_totals[NUM_BUCKETS+TEST_ARRAY_SIZE], /* part. ver. vals */\n"
        "         bucket_ptrs[NUM_BUCKETS],\n"
        "         process_bucket_distrib_ptr1[NUM_BUCKETS+TEST_ARRAY_SIZE],   \n"
        "         process_bucket_distrib_ptr2[NUM_BUCKETS+TEST_ARRAY_SIZE];   \n",
        "         bucket_size[NUM_BUCKETS_MAX+TEST_ARRAY_SIZE],\n"
        "         bucket_size_totals[NUM_BUCKETS_MAX+TEST_ARRAY_SIZE],\n"
        "         bucket_ptrs[NUM_BUCKETS_MAX],\n"
        "         process_bucket_distrib_ptr1[NUM_BUCKETS_MAX+TEST_ARRAY_SIZE],\n"
        "         process_bucket_distrib_ptr2[NUM_BUCKETS_MAX+TEST_ARRAY_SIZE];\n")
    text = text.replace("int main( int argc, char **argv )",
                        "int npb_is_run( void )")
    text = text.replace("    MPI_Init( &argc, &argv );\n", "")
    text = text.replace("       MPI_Finalize();\n       exit( 1 );",
                        "       return 1;")
    text = text.replace("            MPI_Abort(MPI_COMM_WORLD, MPI_ERR_OTHER);\n"
                        "            exit( 1 );",
                        "            return 1;")
    text = text.replace("        MPI_Finalize();\n        exit( 0 );",
                        "        return 0;")
    text = text.replace(
        "                         CLINKFLAGS );\n    }\n",
        "                         CLINKFLAGS );\n"
        "        tpb_npb_set_result(maxtime,\n"
        "            ((double) (MAX_ITERATIONS)*TOTAL_KEYS*MIN_PROCS)\n"
        "                /maxtime/1000000.,\n"
        "            passed_verification ? 1 : 0,\n"
        "            MAX_ITERATIONS, (int)TOTAL_KEYS, MIN_PROCS, 0, CLASS);\n"
        "    }\n")
    text = text.replace("    MPI_Finalize();\n\n\n    return 0;",
                        "    return 0;")
    p.write_text(text)
    print("patched", p)


def patch_dt():
    p = VENDOR / "DT" / "dt.c"
    text = p.read_text()
    text = text.replace("#ifndef CLASS\n#define CLASS 'S'\n#endif\n", "")
    text = text.replace('#include "../common/c_timers.h"',
                        '#include "tpb_npb_result.h"\n#include "../common/c_timers.h"')
    text = text.replace("int main(int argc,char **argv ){",
                        "int npb_dt_run(char graph){")
    text = text.replace("    MPI_Init( &argc, &argv );\n", "")
    old_usage = """     if(argc!=2||
                (  strncmp(argv[1],"BH",2)!=0
                 &&strncmp(argv[1],"WH",2)!=0
                 &&strncmp(argv[1],"SH",2)!=0
                )
      ){
      if(my_rank==0){
        fprintf(stderr,"** Usage: mpirun -np N ../bin/dt.S GraphName\\n");
        fprintf(stderr,"** Where \\n   - N is integer number of MPI processes\\n");
        fprintf(stderr,"   - S is the class S, W, or A \\n");
        fprintf(stderr,"   - GraphName is the communication graph name BH, WH, or SH.\\n");
        fprintf(stderr,"   - the number of MPI processes N should not be be less than \\n");
        fprintf(stderr,"     the number of nodes in the graph\\n");
      }
      MPI_Finalize();
      exit(1);
    } 
   if(strncmp(argv[1],"BH",2)==0){
      dg=buildBH(CLASS);
    }else if(strncmp(argv[1],"WH",2)==0){
      dg=buildWH(CLASS);
    }else if(strncmp(argv[1],"SH",2)==0){
      dg=buildSH(CLASS);
    }
"""
    new_usage = """    if(graph=='B'){
      dg=buildBH(CLASS);
    }else if(graph=='W'){
      dg=buildWH(CLASS);
    }else if(graph=='S'){
      dg=buildSH(CLASS);
    }else{
      if(my_rank==0){
        fprintf(stderr,"** DT graph must be B, W, or S\\n");
      }
      return 1;
    }
"""
    if old_usage not in text:
        raise SystemExit("DT usage block not found")
    text = text.replace(old_usage, new_usage, 1)
    text = text.replace("      MPI_Finalize();\n      exit(1);\n    }",
                        "      return 1;\n    }")
    text = text.replace(
        "        	       CLINKFLAGS );\n    }          \n    MPI_Finalize();\n  return 0;\n}",
        "        	       CLINKFLAGS );\n"
        "      tpb_npb_set_result(tot_time, bytes_sent/tot_time,\n"
        "          verified ? 1 : 0, dg->numNodes, featnum, 0, 0, CLASS);\n"
        "    }\n  return 0;\n}")
    p.write_text(text)
    print("patched", p)


def main():
    write_tpbk()
    write_npbparams()
    patch_fortran_data()
    patch_programs()
    patch_is()
    patch_dt()
    print("done")


if __name__ == "__main__":
    main()
