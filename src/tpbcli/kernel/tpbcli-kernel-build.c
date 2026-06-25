/*
 * tpbcli-kernel-build.c
 * `tpbcli kernel build` — configure/build out-of-tree kernel via CMake.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-kernel-backup.h"
#include "tpbcli-kernel-home.h"
#include "tpbcli-kernel-set.h"
#include "tpbcli-kernel-build.h"

#define TPBCLI_KERNEL_BUILD_MAX_CMAKE_DEFS  32
#define TPBCLI_KERNEL_BUILD_CMD_MAX         8192

typedef struct {
    const char *cc;
    const char *cflags;
    const char *cxx;
    const char *cxxflags;
    const char *fc;
    const char *fcflags;
    const char *cmake_defs[TPBCLI_KERNEL_BUILD_MAX_CMAKE_DEFS];
    int ncmake_defs;
} tpbcli_kernel_build_opts_t;

/* Local Function Prototypes */
static void _sf_print_build_usage(void);
static int _sf_run_shell(const char *cmd);
static int _sf_path_is_dir(const char *path);
static int _sf_ensure_dir(const char *path);
static int _sf_copy_file(const char *src, const char *dst);
static int _sf_activate_installed_kernel(const char *kernel_name,
                                         const char *so_path);
static int _sf_register_compile_meta(const char *kernel_name,
                                     const char *tgt_file,
                                     const char *src_dir,
                                     const char *build_dir,
                                     const tpbcli_kernel_build_opts_t *opts);
static int _sf_build_cmake_cmd(char *cmd, size_t cmdlen,
                               const char *source_dir,
                               const char *build_dir,
                               const char *tpb_home,
                               const char *kernel_name,
                               const tpbcli_kernel_build_opts_t *opts,
                               int configure);

static void
_sf_print_build_usage(void)
{
    fprintf(stderr,
            "Usage: tpbcli kernel build --dir <path> --kernel <kernel_name> "
            "[--tpb-home <path>] [-D<var>=<value> ...] "
            "[--cc <compiler>] [--cflags <flags>] "
            "[--cxx <compiler>] [--cxxflags <flags>] "
            "[--fc <compiler>] [--fcflags <flags>]\n");
}

static int
_sf_run_shell(const char *cmd)
{
    int st;

    st = system(cmd);
    if (st == -1) {
        return TPBE_FILE_IO_FAIL;
    }
    if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
        return TPBE_SUCCESS;
    }
    return TPBE_CLI_FAIL;
}

static int
_sf_path_is_dir(const char *path)
{
    struct stat st;

    if (path == NULL || stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int
_sf_ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || access(path, F_OK) == 0) {
        return TPBE_SUCCESS;
    }
    return TPBE_FILE_IO_FAIL;
}

static int
_sf_copy_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buf[4096];
    size_t n;

    in = fopen(src, "rb");
    if (in == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return TPBE_FILE_IO_FAIL;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return TPBE_FILE_IO_FAIL;
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;
}

static int
_sf_build_cmake_cmd(char *cmd, size_t cmdlen,
                    const char *source_dir,
                    const char *build_dir,
                    const char *tpb_home,
                    const char *kernel_name,
                    const tpbcli_kernel_build_opts_t *opts,
                    int configure)
{
    char tpbench_dir[PATH_MAX];
    char target[128];
    int n;
    int i;

    if (configure) {
        if (snprintf(tpbench_dir, sizeof(tpbench_dir), "%s/lib/cmake/TPBench",
                     tpb_home) >= (int)sizeof(tpbench_dir)) {
            return TPBE_FILE_IO_FAIL;
        }
        n = snprintf(cmd, cmdlen,
                     "cmake -S \"%s\" -B \"%s\" -DTPBench_DIR=\"%s\"",
                     source_dir, build_dir, tpbench_dir);
        if (n < 0 || (size_t)n >= cmdlen) {
            return TPBE_FILE_IO_FAIL;
        }
        if (opts->cc != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DCMAKE_C_COMPILER=\"%s\"", opts->cc);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        if (opts->cxx != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DCMAKE_CXX_COMPILER=\"%s\"", opts->cxx);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        if (opts->fc != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DCMAKE_Fortran_COMPILER=\"%s\"", opts->fc);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        if (opts->cflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_CFLAGS=\"%s\"", opts->cflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        if (opts->cxxflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_CXXFLAGS=\"%s\"", opts->cxxflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        if (opts->fcflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_FFLAGS=\"%s\"", opts->fcflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        for (i = 0; i < opts->ncmake_defs; i++) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -D%s", opts->cmake_defs[i]);
            if (n < 0 || (size_t)n >= cmdlen) {
                return TPBE_FILE_IO_FAIL;
            }
        }
        return TPBE_SUCCESS;
    }

    if (snprintf(target, sizeof(target), "tpbk_%s", kernel_name) >=
        (int)sizeof(target)) {
        return TPBE_FILE_IO_FAIL;
    }
    n = snprintf(cmd, cmdlen, "cmake --build \"%s\" --target %s",
                 build_dir, target);
    if (n < 0 || (size_t)n >= cmdlen) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;
}

static int
_sf_activate_installed_kernel(const char *kernel_name, const char *so_path)
{
    char workspace[PATH_MAX];
    unsigned char sha1[20];
    unsigned char kernel_id[20];
    int err;

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpb_raf_hash_file(so_path, sha1);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    err = tpb_raf_gen_kernel_id(sha1, kernel_id);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    (void)tpb_raf_kernel_deactivate_same_name(workspace, kernel_name, kernel_id);
    (void)tpb_raf_entry_patch_kernel_active(workspace, kernel_id, 1);
    (void)tpb_raf_record_patch_kernel_active(workspace, kernel_id, 1);
    return TPBE_SUCCESS;
}

static int
_sf_register_compile_meta(const char *kernel_name,
                          const char *tgt_file,
                          const char *src_dir,
                          const char *build_dir,
                          const tpbcli_kernel_build_opts_t *opts)
{
    char *argv[32];
    int argc;
    int err;
    const char *cflags = (opts->cflags != NULL) ? opts->cflags : "";
    const char *cc = (opts->cc != NULL) ? opts->cc : "";

    argc = 0;
    argv[argc++] = "tpbcli";
    argv[argc++] = "kernel";
    argv[argc++] = "set";
    argv[argc++] = "--kernel";
    argv[argc++] = (char *)kernel_name;
    argv[argc++] = "--key";
    argv[argc++] = "variation.target";
    argv[argc++] = (char *)tgt_file;
    argv[argc++] = "--key";
    argv[argc++] = "variation.source_dir";
    argv[argc++] = (char *)src_dir;
    argv[argc++] = "--key";
    argv[argc++] = "variation.build_dir";
    argv[argc++] = (char *)build_dir;
    argv[argc++] = "--key";
    argv[argc++] = "compilation.compiler.path";
    argv[argc++] = (char *)cc;
    argv[argc++] = "--key";
    argv[argc++] = "compilation.kernel_cflags";
    argv[argc++] = (char *)cflags;
    argv[argc++] = "--key";
    argv[argc++] = "dependency.tpbench";
    argv[argc++] = "libtpbench.so";
    argv[argc] = NULL;

    err = tpbcli_kernel_set(argc, argv);
    return err;
}

int
tpbcli_kernel_build(int argc, char **argv)
{
    const char *dir_path = NULL;
    const char *kernel_name = NULL;
    const char *tpb_home_opt = NULL;
    tpbcli_kernel_build_opts_t opts;
    char tpb_home[PATH_MAX];
    char build_dir[PATH_MAX];
    char tpbench_pkg[PATH_MAX];
    char source_file[PATH_MAX];
    char built_so[PATH_MAX];
    char dest_so[PATH_MAX];
    char lib_dir[PATH_MAX];
    char cmd[TPBCLI_KERNEL_BUILD_CMD_MAX];
    char *backup_argv[8];
    int err;
    int i;

    memset(&opts, 0, sizeof(opts));

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            dir_path = argv[++i];
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            kernel_name = argv[++i];
        } else if (strcmp(argv[i], "--tpb-home") == 0 && i + 1 < argc) {
            tpb_home_opt = argv[++i];
        } else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc) {
            opts.cc = argv[++i];
        } else if (strcmp(argv[i], "--cflags") == 0 && i + 1 < argc) {
            opts.cflags = argv[++i];
        } else if (strcmp(argv[i], "--cxx") == 0 && i + 1 < argc) {
            opts.cxx = argv[++i];
        } else if (strcmp(argv[i], "--cxxflags") == 0 && i + 1 < argc) {
            opts.cxxflags = argv[++i];
        } else if (strcmp(argv[i], "--fc") == 0 && i + 1 < argc) {
            opts.fc = argv[++i];
        } else if (strcmp(argv[i], "--fcflags") == 0 && i + 1 < argc) {
            opts.fcflags = argv[++i];
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            if (opts.ncmake_defs >= TPBCLI_KERNEL_BUILD_MAX_CMAKE_DEFS) {
                fprintf(stderr, "kernel build: too many -D options.\n");
                return TPBE_CLI_FAIL;
            }
            opts.cmake_defs[opts.ncmake_defs++] = argv[i] + 2;
        } else {
            _sf_print_build_usage();
            return TPBE_CLI_FAIL;
        }
    }

    if (dir_path == NULL || kernel_name == NULL) {
        _sf_print_build_usage();
        return TPBE_CLI_FAIL;
    }
    if (!tpbcli_kernel_name_valid(kernel_name)) {
        fprintf(stderr, "kernel build: invalid kernel name '%s'.\n",
                kernel_name);
        return TPBE_CLI_FAIL;
    }
    if (!_sf_path_is_dir(dir_path)) {
        fprintf(stderr, "kernel build: directory '%s' not found.\n", dir_path);
        return TPBE_CLI_FAIL;
    }

    err = tpbcli_kernel_resolve_home(tpb_home_opt, tpb_home, sizeof(tpb_home));
    if (err != TPBE_SUCCESS) {
        fprintf(stderr, "kernel build: failed to resolve TPB_HOME.\n");
        return err;
    }

    if (snprintf(tpbench_pkg, sizeof(tpbench_pkg), "%s/lib/cmake/TPBench",
                 tpb_home) >= (int)sizeof(tpbench_pkg)) {
        return TPBE_FILE_IO_FAIL;
    }
    if (access(tpbench_pkg, F_OK) != 0) {
        fprintf(stderr,
                "kernel build: TPBench package not found at '%s'.\n",
                tpbench_pkg);
        return TPBE_CLI_FAIL;
    }

    if (snprintf(source_file, sizeof(source_file), "%s/tpbk_%s.c",
                 dir_path, kernel_name) >= (int)sizeof(source_file)) {
        return TPBE_FILE_IO_FAIL;
    }
    if (access(source_file, R_OK) != 0) {
        fprintf(stderr, "kernel build: source '%s' not found.\n", source_file);
        return TPBE_CLI_FAIL;
    }

    if (snprintf(build_dir, sizeof(build_dir), "%s/build", dir_path) >=
        (int)sizeof(build_dir)) {
        return TPBE_FILE_IO_FAIL;
    }
    err = _sf_ensure_dir(build_dir);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = _sf_build_cmake_cmd(cmd, sizeof(cmd), dir_path, build_dir, tpb_home,
                              kernel_name, &opts, 1);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "kernel build: %s\n", cmd);
    err = _sf_run_shell(cmd);
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "kernel build: cmake configure failed.\n");
        return err;
    }

    err = _sf_build_cmake_cmd(cmd, sizeof(cmd), dir_path, build_dir, tpb_home,
                              kernel_name, &opts, 0);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "kernel build: %s\n", cmd);
    err = _sf_run_shell(cmd);
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "kernel build: cmake build failed.\n");
        return err;
    }

    if (snprintf(built_so, sizeof(built_so), "%s/lib/libtpbk_%s.so",
                 build_dir, kernel_name) >= (int)sizeof(built_so)) {
        return TPBE_FILE_IO_FAIL;
    }
    if (access(built_so, R_OK) != 0) {
        fprintf(stderr, "kernel build: built library '%s' not found.\n",
                built_so);
        return TPBE_CLI_FAIL;
    }

    err = tpb_dl_force_tpb_home(tpb_home);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (snprintf(lib_dir, sizeof(lib_dir), "%s/lib", tpb_home) >=
        (int)sizeof(lib_dir)) {
        return TPBE_FILE_IO_FAIL;
    }
    err = _sf_ensure_dir(lib_dir);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    backup_argv[0] = "tpbcli";
    backup_argv[1] = "kernel";
    backup_argv[2] = "backup-inactive";
    backup_argv[3] = "--kernel";
    backup_argv[4] = (char *)kernel_name;
    backup_argv[5] = NULL;
    (void)tpbcli_kernel_backup_inactive(5, backup_argv);

    if (snprintf(dest_so, sizeof(dest_so), "%s/libtpbk_%s.so",
                 lib_dir, kernel_name) >= (int)sizeof(dest_so)) {
        return TPBE_FILE_IO_FAIL;
    }
    err = _sf_copy_file(built_so, dest_so);
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "kernel build: failed to install '%s'.\n", dest_so);
        return err;
    }

    err = _sf_activate_installed_kernel(kernel_name, dest_so);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = _sf_register_compile_meta(kernel_name, dest_so, dir_path, build_dir,
                                      &opts);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "kernel build: installed %s\n", dest_so);
    return TPBE_SUCCESS;
}
