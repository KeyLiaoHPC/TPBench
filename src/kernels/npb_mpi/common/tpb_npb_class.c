/*
 * tpb_npb_class.c
 * Runtime CLASS tables copied from NPB 3.4.4 sys/setparams.c.
 */

#include <ctype.h>
#include <string.h>
#include "tpb_npb_class.h"
#include "tpbench.h"

#if defined(TPB_NPB_KERNEL_EP)
void tpb_npb_ep_set_class(char cls, int m);
#endif
#if defined(TPB_NPB_KERNEL_CG)
void tpb_npb_cg_set_class(int na, int nonzer, int niter, double shift);
#endif
#if defined(TPB_NPB_KERNEL_FT)
void tpb_npb_ft_set_class(int nx, int ny, int nz, int niter);
#endif
#if defined(TPB_NPB_KERNEL_MG)
void tpb_npb_mg_set_class(int nx, int nit, int lt);
#endif
#if defined(TPB_NPB_KERNEL_BT)
void tpb_npb_bt_set_class(int problem_size, int niter, double dt);
#endif
#if defined(TPB_NPB_KERNEL_SP)
void tpb_npb_sp_set_class(int problem_size, int niter, double dt);
#endif
#if defined(TPB_NPB_KERNEL_LU)
void tpb_npb_lu_set_class(int problem_size, int itmax, double dt);
#endif

char tpb_npb_c_class = 'S';
int tpb_npb_is_total_keys_log2 = 16;
int tpb_npb_is_max_key_log2 = 11;
int tpb_npb_is_num_buckets_log2 = 9;
int tpb_npb_is_min_procs = 1;
int tpb_npb_is_max_procs = 128;
int tpb_npb_dt_num_samples = 1728;
int tpb_npb_dt_std_deviation = 128;
int tpb_npb_dt_num_sources = 4;

static int
_sf_has_char(const char *list, char cls)
{
    size_t i;

    for (i = 0; list[i] != '\0'; i++) {
        if (list[i] == cls) {
            return 1;
        }
    }
    return 0;
}

int
tpb_npb_class_allowed(const char *bench, char cls)
{
    if (bench == NULL) {
        return 0;
    }
    if (strcmp(bench, "IS") == 0) {
        return _sf_has_char("SWABCDE", cls);
    }
    if (strcmp(bench, "DT") == 0) {
        return _sf_has_char("SWABCD", cls);
    }
    return _sf_has_char("SWABCDEF", cls);
}

int
tpb_npb_apply_class(const char *bench, char cls)
{
    char u;

    if (bench == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    u = (char)toupper((unsigned char)cls);
    if (!tpb_npb_class_allowed(bench, u)) {
        return TPBE_KERN_ARG_FAIL;
    }
    tpb_npb_c_class = u;

    if (strcmp(bench, "EP") == 0) {
        int m = 24;

        if (u == 'S') m = 24;
        else if (u == 'W') m = 25;
        else if (u == 'A') m = 28;
        else if (u == 'B') m = 30;
        else if (u == 'C') m = 32;
        else if (u == 'D') m = 36;
        else if (u == 'E') m = 40;
        else if (u == 'F') m = 44;
#if defined(TPB_NPB_KERNEL_EP)
        tpb_npb_ep_set_class(u, m);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "CG") == 0) {
        int na = 1400, nonzer = 7, niter = 15;
        double shift = 10.0;

        if (u == 'S') { na = 1400; nonzer = 7; niter = 15; shift = 10.0; }
        else if (u == 'W') { na = 7000; nonzer = 8; niter = 15; shift = 12.0; }
        else if (u == 'A') { na = 14000; nonzer = 11; niter = 15; shift = 20.0; }
        else if (u == 'B') { na = 75000; nonzer = 13; niter = 75; shift = 60.0; }
        else if (u == 'C') { na = 150000; nonzer = 15; niter = 75; shift = 110.0; }
        else if (u == 'D') { na = 1500000; nonzer = 21; niter = 100; shift = 500.0; }
        else if (u == 'E') { na = 9000000; nonzer = 26; niter = 100; shift = 1500.0; }
        else if (u == 'F') { na = 54000000; nonzer = 31; niter = 100; shift = 5000.0; }
#if defined(TPB_NPB_KERNEL_CG)
        tpb_npb_cg_set_class(na, nonzer, niter, shift);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "FT") == 0) {
        int nx = 64, ny = 64, nz = 64, niter = 6;

        if (u == 'S') { nx = 64; ny = 64; nz = 64; niter = 6; }
        else if (u == 'W') { nx = 128; ny = 128; nz = 32; niter = 6; }
        else if (u == 'A') { nx = 256; ny = 256; nz = 128; niter = 6; }
        else if (u == 'B') { nx = 512; ny = 256; nz = 256; niter = 20; }
        else if (u == 'C') { nx = 512; ny = 512; nz = 512; niter = 20; }
        else if (u == 'D') { nx = 2048; ny = 1024; nz = 1024; niter = 25; }
        else if (u == 'E') { nx = 4096; ny = 2048; nz = 2048; niter = 25; }
        else if (u == 'F') { nx = 8192; ny = 4096; nz = 4096; niter = 25; }
#if defined(TPB_NPB_KERNEL_FT)
        tpb_npb_ft_set_class(nx, ny, nz, niter);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "MG") == 0) {
        int nx = 32, nit = 4, lt;

        if (u == 'S') { nx = 32; nit = 4; }
        else if (u == 'W') { nx = 128; nit = 4; }
        else if (u == 'A') { nx = 256; nit = 4; }
        else if (u == 'B') { nx = 256; nit = 20; }
        else if (u == 'C') { nx = 512; nit = 20; }
        else if (u == 'D') { nx = 1024; nit = 50; }
        else if (u == 'E') { nx = 2048; nit = 50; }
        else if (u == 'F') { nx = 4096; nit = 50; }
        lt = 0;
        while ((1 << lt) < nx) {
            lt++;
        }
#if defined(TPB_NPB_KERNEL_MG)
        tpb_npb_mg_set_class(nx, nit, lt);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "BT") == 0) {
        int problem_size = 12, niter = 60;
        double dt = 0.010;

        if (u == 'S') { problem_size = 12; dt = 0.010; niter = 60; }
        else if (u == 'W') { problem_size = 24; dt = 0.0008; niter = 200; }
        else if (u == 'A') { problem_size = 64; dt = 0.0008; niter = 200; }
        else if (u == 'B') { problem_size = 102; dt = 0.0003; niter = 200; }
        else if (u == 'C') { problem_size = 162; dt = 0.0001; niter = 200; }
        else if (u == 'D') { problem_size = 408; dt = 0.00002; niter = 250; }
        else if (u == 'E') { problem_size = 1020; dt = 0.000004; niter = 250; }
        else if (u == 'F') { problem_size = 2560; dt = 0.0000006; niter = 250; }
#if defined(TPB_NPB_KERNEL_BT)
        tpb_npb_bt_set_class(problem_size, niter, dt);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "SP") == 0) {
        int problem_size = 12, niter = 100;
        double dt = 0.015;

        if (u == 'S') { problem_size = 12; dt = 0.015; niter = 100; }
        else if (u == 'W') { problem_size = 36; dt = 0.0015; niter = 400; }
        else if (u == 'A') { problem_size = 64; dt = 0.0015; niter = 400; }
        else if (u == 'B') { problem_size = 102; dt = 0.001; niter = 400; }
        else if (u == 'C') { problem_size = 162; dt = 0.00067; niter = 400; }
        else if (u == 'D') { problem_size = 408; dt = 0.00030; niter = 500; }
        else if (u == 'E') { problem_size = 1020; dt = 0.0001; niter = 500; }
        else if (u == 'F') { problem_size = 2560; dt = 0.000015; niter = 500; }
#if defined(TPB_NPB_KERNEL_SP)
        tpb_npb_sp_set_class(problem_size, niter, dt);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "LU") == 0) {
        int problem_size = 12, itmax = 50;
        double dt = 0.5;

        if (u == 'S') { problem_size = 12; dt = 0.5; itmax = 50; }
        else if (u == 'W') { problem_size = 33; dt = 1.5e-3; itmax = 300; }
        else if (u == 'A') { problem_size = 64; dt = 2.0; itmax = 250; }
        else if (u == 'B') { problem_size = 102; dt = 2.0; itmax = 250; }
        else if (u == 'C') { problem_size = 162; dt = 2.0; itmax = 250; }
        else if (u == 'D') { problem_size = 408; dt = 1.0; itmax = 300; }
        else if (u == 'E') { problem_size = 1020; dt = 0.5; itmax = 300; }
        else if (u == 'F') { problem_size = 2560; dt = 0.2; itmax = 300; }
#if defined(TPB_NPB_KERNEL_LU)
        tpb_npb_lu_set_class(problem_size, itmax, dt);
#endif
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "IS") == 0) {
        tpb_npb_is_min_procs = 1;
        tpb_npb_is_max_procs = 1024;
        if (u == 'S') {
            tpb_npb_is_total_keys_log2 = 16;
            tpb_npb_is_max_key_log2 = 11;
            tpb_npb_is_num_buckets_log2 = 9;
            tpb_npb_is_max_procs = 128;
        } else if (u == 'W') {
            tpb_npb_is_total_keys_log2 = 20;
            tpb_npb_is_max_key_log2 = 16;
            tpb_npb_is_num_buckets_log2 = 10;
        } else if (u == 'A') {
            tpb_npb_is_total_keys_log2 = 23;
            tpb_npb_is_max_key_log2 = 19;
            tpb_npb_is_num_buckets_log2 = 10;
        } else if (u == 'B') {
            tpb_npb_is_total_keys_log2 = 25;
            tpb_npb_is_max_key_log2 = 21;
            tpb_npb_is_num_buckets_log2 = 10;
        } else if (u == 'C') {
            tpb_npb_is_total_keys_log2 = 27;
            tpb_npb_is_max_key_log2 = 23;
            tpb_npb_is_num_buckets_log2 = 10;
        } else if (u == 'D') {
            tpb_npb_is_total_keys_log2 = 29;
            tpb_npb_is_max_key_log2 = 27;
            tpb_npb_is_num_buckets_log2 = 10;
            tpb_npb_is_min_procs = 4;
        } else if (u == 'E') {
            tpb_npb_is_total_keys_log2 = 29;
            tpb_npb_is_max_key_log2 = 31;
            tpb_npb_is_num_buckets_log2 = 10;
            tpb_npb_is_min_procs = 64;
        }
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "DT") == 0) {
        if (u == 'S') {
            tpb_npb_dt_num_samples = 1728;
            tpb_npb_dt_std_deviation = 128;
            tpb_npb_dt_num_sources = 4;
        } else if (u == 'W') {
            tpb_npb_dt_num_samples = 1728 * 8;
            tpb_npb_dt_std_deviation = 128 * 2;
            tpb_npb_dt_num_sources = 8;
        } else if (u == 'A') {
            tpb_npb_dt_num_samples = 1728 * 64;
            tpb_npb_dt_std_deviation = 128 * 4;
            tpb_npb_dt_num_sources = 16;
        } else if (u == 'B') {
            tpb_npb_dt_num_samples = 1728 * 512;
            tpb_npb_dt_std_deviation = 128 * 8;
            tpb_npb_dt_num_sources = 32;
        } else if (u == 'C') {
            tpb_npb_dt_num_samples = 1728 * 4096;
            tpb_npb_dt_std_deviation = 128 * 16;
            tpb_npb_dt_num_sources = 64;
        } else if (u == 'D') {
            tpb_npb_dt_num_samples = 1728 * 4096 * 8;
            tpb_npb_dt_std_deviation = 128 * 32;
            tpb_npb_dt_num_sources = 128;
        }
        return TPBE_SUCCESS;
    }
    return TPBE_KERN_ARG_FAIL;
}

static int
_sf_is_pow2(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static int
_sf_is_square(int n)
{
    int r;

    if (n <= 0) {
        return 0;
    }
    for (r = 1; r * r <= n; r++) {
        if (r * r == n) {
            return 1;
        }
    }
    return 0;
}

static int
_sf_lu_proc_ok(int n)
{
    int x;
    int y;

    if (n <= 0) {
        return 0;
    }
    for (x = n; x >= 1; x--) {
        if (n % x != 0) {
            continue;
        }
        y = n / x;
        if (x / 2 <= y && y <= x) {
            return 1;
        }
    }
    return 0;
}

/*
 * DT node counts from vendor/DT/README (BH/WH share a count; SH is larger).
 */
static int
_sf_dt_min_nprocs(char graph, char cls)
{
    int bhwh;
    int sh;

    bhwh = 1;
    sh = 1;
    if (cls == 'S') {
        bhwh = 5;
        sh = 12;
    } else if (cls == 'W') {
        bhwh = 11;
        sh = 32;
    } else if (cls == 'A') {
        bhwh = 21;
        sh = 80;
    } else if (cls == 'B') {
        bhwh = 43;
        sh = 192;
    } else if (cls == 'C') {
        bhwh = 85;
        sh = 448;
    } else if (cls == 'D') {
        bhwh = 171;
        sh = 1024;
    }
    if (graph == 'S') {
        return sh;
    }
    return bhwh;
}

int
tpb_npb_check_nprocs(const char *bench, int nprocs, char dt_graph)
{
    if (bench == NULL || nprocs <= 0) {
        return TPBE_KERN_ARG_FAIL;
    }
    if (strcmp(bench, "EP") == 0) {
        return TPBE_SUCCESS;
    }
    if (strcmp(bench, "BT") == 0 || strcmp(bench, "SP") == 0) {
        return _sf_is_square(nprocs) ? TPBE_SUCCESS : TPBE_KERN_ARG_FAIL;
    }
    if (strcmp(bench, "LU") == 0) {
        return _sf_lu_proc_ok(nprocs) ? TPBE_SUCCESS : TPBE_KERN_ARG_FAIL;
    }
    if (strcmp(bench, "CG") == 0 || strcmp(bench, "FT") == 0 ||
        strcmp(bench, "IS") == 0 || strcmp(bench, "MG") == 0) {
        return _sf_is_pow2(nprocs) ? TPBE_SUCCESS : TPBE_KERN_ARG_FAIL;
    }
    if (strcmp(bench, "DT") == 0) {
        if (nprocs < _sf_dt_min_nprocs(dt_graph, tpb_npb_c_class)) {
            return TPBE_KERN_ARG_FAIL;
        }
        return TPBE_SUCCESS;
    }
    return TPBE_KERN_ARG_FAIL;
}
