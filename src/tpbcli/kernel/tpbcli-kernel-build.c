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
#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-kernel-backup.h"
#include "tpbcli-kernel-home.h"
#include "tpbcli-kernel-registry.h"
#include "tpbcli-kernel-set.h"
#include "tpbcli-kernel-build.h"

#define TPBCLI_KERNEL_BUILD_MAX_CMAKE_DEFS  32
#define TPBCLI_KERNEL_BUILD_MAX_KERNELS     64
#define TPBCLI_KERNEL_BUILD_CMD_MAX         8192

typedef struct {
    const char *cc;
    const char *cflags;
    const char *cxx;
    const char *cxxflags;
    const char *fc;
    const char *fcflags;
    const char *ldflags;
    const char *cmake_defs[TPBCLI_KERNEL_BUILD_MAX_CMAKE_DEFS];
    int ncmake_defs;
} tpbcli_kernel_build_opts_t;

typedef struct {
    const char *dir_path;
    int dir_explicit;
    const char *kernel_csv;
    const char *tag_csv;
    const char *tpb_home_opt;
    tpbcli_kernel_build_opts_t opts;
} tpbcli_kernel_build_args_t;

/* Local Function Prototypes */
static void _sf_print_build_usage(void);
static int _sf_run_shell(const char *cmd);
static int _sf_path_is_dir(const char *path);
static int _sf_ensure_dir(const char *path);
/*
 * Copy a file byte-for-byte (used to stage registry CMakeLists.txt).
 */
static int _sf_copy_file(const char *src, const char *dst);
static int _sf_parse_build_args(int argc, char **argv,
                                tpbcli_kernel_build_args_t *args);
static int _sf_resolve_kernel_names(const tpbcli_kernel_build_args_t *args,
                                    const char *tpb_home,
                                    const char **names_out,
                                    int names_max,
                                    tpbcli_kernel_reg_list_t *reg_out);
static int _sf_resolve_kernel_dir(const char *tpb_home,
                                  const char *dir_path,
                                  int dir_explicit,
                                  const char *kernel_name,
                                  const tpbcli_kernel_reg_list_t *reg,
                                  char *out, size_t outlen);
static int _sf_canonicalize_path(char *path, size_t pathlen);
/*
 * Ensure kernel source dir has CMakeLists.txt for out-of-tree registry build.
 * Copies $TPB_HOME/etc/cmake/kernel/CMakeLists.registry.txt when missing.
 */
static int _sf_ensure_registry_cmake(const char *source_dir,
                                      const char *tpb_home);
static int _sf_build_one_kernel(const char *kernel_name,
                                const char *source_dir,
                                const char *tpb_home,
                                const tpbcli_kernel_build_opts_t *opts);
static int _sf_activate_installed_kernel(const char *kernel_name,
                                         const char *so_path);
static int _sf_register_compile_meta(const char *kernel_name,
                                     const char *tgt_file,
                                     const char *src_dir,
                                     const char *build_dir,
                                     const tpbcli_kernel_build_opts_t *opts);
/*
 * Build cmake configure or build command for one out-of-tree kernel.
 * link_libs holds registry extras (e.g. MPI::MPI_C) passed as a -D cache var.
 */
static int _sf_build_cmake_cmd(char *cmd, size_t cmdlen,
                               const char *source_dir,
                               const char *build_dir,
                               const char *tpb_home,
                               const char *kernel_name,
                               const char *link_libs,
                               const tpbcli_kernel_build_opts_t *opts,
                               int configure);

static void
_sf_print_build_usage(void)
{
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                    "Usage: tpbcli kernel build [--dir <path>] "
                    "(--kernel <names> | --kernel-tag <tags>) "
                    "[--tpb-home <path>] [--ldflags <flags>] [-D<var>=<value> ...] "
                    "[--cc <compiler>] [--cflags <flags>] "
                    "[--cxx <compiler>] [--cxxflags <flags>] "
                    "[--fc <compiler>] [--fcflags <flags>]\n"
                    "\n"
                    "  --dir defaults to TPB_HOME; with the default, kernel source "
                    "dirs are resolved from\n"
                    "  $TPB_HOME/src/kernels/kernel_list.cmake.in.\n"
                    "  --kernel and --kernel-tag are mutually exclusive; each accepts "
                    "comma-separated values\n"
                    "  optionally wrapped in single or double quotes.\n");
}

static int
_sf_run_shell(const char *cmd)
{
    int st;

    st = system(cmd);
    if (st == -1) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
        return TPBE_SUCCESS;
    }
    TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
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
    TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_parse_build_args(int argc, char **argv,
                     tpbcli_kernel_build_args_t *args)
{
    int i;

    memset(args, 0, sizeof(*args));
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            args->dir_path = argv[++i];
            args->dir_explicit = 1;
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            args->kernel_csv = argv[++i];
        } else if (strcmp(argv[i], "--kernel-tag") == 0 && i + 1 < argc) {
            args->tag_csv = argv[++i];
        } else if (strcmp(argv[i], "--tpb-home") == 0 && i + 1 < argc) {
            args->tpb_home_opt = argv[++i];
        } else if (strcmp(argv[i], "--ldflags") == 0 && i + 1 < argc) {
            args->opts.ldflags = argv[++i];
        } else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc) {
            args->opts.cc = argv[++i];
        } else if (strcmp(argv[i], "--cflags") == 0 && i + 1 < argc) {
            args->opts.cflags = argv[++i];
        } else if (strcmp(argv[i], "--cxx") == 0 && i + 1 < argc) {
            args->opts.cxx = argv[++i];
        } else if (strcmp(argv[i], "--cxxflags") == 0 && i + 1 < argc) {
            args->opts.cxxflags = argv[++i];
        } else if (strcmp(argv[i], "--fc") == 0 && i + 1 < argc) {
            args->opts.fc = argv[++i];
        } else if (strcmp(argv[i], "--fcflags") == 0 && i + 1 < argc) {
            args->opts.fcflags = argv[++i];
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            if (args->opts.ncmake_defs >= TPBCLI_KERNEL_BUILD_MAX_CMAKE_DEFS) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel build: too many -D options.\n");
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
            }
            args->opts.cmake_defs[args->opts.ncmake_defs++] = argv[i] + 2;
        } else {
            _sf_print_build_usage();
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
        }
    }

    if (args->kernel_csv != NULL && args->tag_csv != NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: --kernel and --kernel-tag are mutually "
                        "exclusive; specify exactly one.\n");
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }
    if (args->kernel_csv == NULL && args->tag_csv == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: specify exactly one of --kernel or "
                        "--kernel-tag.\n");
        _sf_print_build_usage();
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_resolve_kernel_names(const tpbcli_kernel_build_args_t *args,
                         const char *tpb_home,
                         const char **names_out,
                         int names_max,
                         tpbcli_kernel_reg_list_t *reg_out)
{
    tpbcli_kernel_reg_list_t reg;
    char scratch[512];
    char req_scratch[512];
    char *tokens[TPBCLI_KERNEL_BUILD_MAX_KERNELS];
    int n;
    int i;
    int err;

    err = tpbcli_kernel_reg_load(tpb_home, &reg);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    if (reg_out != NULL) {
        *reg_out = reg;
    }

    if (args->tag_csv != NULL) {
        n = tpbcli_kernel_reg_expand_tags(&reg, args->tag_csv,
                                          names_out, names_max);
        if (n <= 0) {
            char all_tags[512];

            tpbcli_kernel_reg_all_tags(&reg, all_tags, sizeof(all_tags));
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                            "kernel build: no kernels matched --kernel-tag '%s'.\n",
                            args->tag_csv);
            if (all_tags[0] != '\0') {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel build: known tags: %s\n", all_tags);
            }
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
        }
        return n;
    }

    snprintf(scratch, sizeof(scratch), "%s", args->kernel_csv);
    n = tpbcli_kernel_reg_split_csv(scratch, tokens, names_max,
                                    req_scratch, sizeof(req_scratch));
    if (n <= 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel build: --kernel list is empty or invalid.\n");
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < n; i++) {
        if (!tpbcli_kernel_name_valid(tokens[i])) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                            "kernel build: invalid kernel name '%s'.\n",
                            tokens[i]);
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
        }
        names_out[i] = tokens[i];
    }
    return n;
}

static int
_sf_resolve_kernel_dir(const char *tpb_home,
                       const char *dir_path,
                       int dir_explicit,
                       const char *kernel_name,
                       const tpbcli_kernel_reg_list_t *reg,
                       char *out, size_t outlen)
{
    const tpbcli_kernel_reg_entry_t *ent;

    if (dir_explicit) {
        if (snprintf(out, outlen, "%s", dir_path) >= (int)outlen) {
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
        }
        return TPBE_SUCCESS;
    }

    ent = tpbcli_kernel_reg_find(reg, kernel_name);
    if (ent == NULL || ent->path[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: kernel '%s' not found in "
                        "%s/src/kernels/kernel_list.cmake.in.\n"
                        "kernel build: use --dir <path> for out-of-tree kernels "
                        "not listed in the registry.\n",
                        kernel_name, tpb_home);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }
    if (snprintf(out, outlen, "%s/src/kernels/%s",
                 tpb_home, ent->path) >= (int)outlen) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_build_cmake_cmd(char *cmd, size_t cmdlen,
                    const char *source_dir,
                    const char *build_dir,
                    const char *tpb_home,
                    const char *kernel_name,
                    const char *link_libs,
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
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
        }
        n = snprintf(cmd, cmdlen,
                     "cmake -S \"%s\" -B \"%s\" -DTPBench_DIR=\"%s\" "
                     "-DTPB_KERNEL_NAME=\"%s\"",
                     source_dir, build_dir, tpbench_dir, kernel_name);
        if (n < 0 || (size_t)n >= cmdlen) {
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
        }
        if (opts->cc != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DCMAKE_C_COMPILER=\"%s\"", opts->cc);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (opts->cxx != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DCMAKE_CXX_COMPILER=\"%s\"", opts->cxx);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (opts->fc != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DCMAKE_Fortran_COMPILER=\"%s\"", opts->fc);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (opts->cflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_CFLAGS=\"%s\"", opts->cflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (opts->cxxflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_CXXFLAGS=\"%s\"", opts->cxxflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (opts->fcflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_FFLAGS=\"%s\"", opts->fcflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (opts->ldflags != NULL) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_LDFLAGS=\"%s\"", opts->ldflags);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        if (link_libs != NULL && link_libs[0] != '\0') {
            /* Registry link extras (MPI, etc.) for CMakeLists.registry.txt */
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -DTPB_KERNEL_LINK_LIBS=\"%s\"", link_libs);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        for (i = 0; i < opts->ncmake_defs; i++) {
            n += snprintf(cmd + n, cmdlen - (size_t)n,
                          " -D%s", opts->cmake_defs[i]);
            if (n < 0 || (size_t)n >= cmdlen) {
                TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
            }
        }
        return TPBE_SUCCESS;
    }

    if (snprintf(target, sizeof(target), "tpbk_%s", kernel_name) >=
        (int)sizeof(target)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    n = snprintf(cmd, cmdlen, "cmake --build \"%s\" --target %s",
                 build_dir, target);
    if (n < 0 || (size_t)n >= cmdlen) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
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
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    err = tpb_raf_hash_file(so_path, sha1);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    err = tpb_raf_gen_kernel_id(sha1, kernel_id);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

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
    char *argv[36];
    int argc;
    int err;
    const char *cflags = (opts->cflags != NULL) ? opts->cflags : "";
    const char *cc = (opts->cc != NULL) ? opts->cc : "";
    const char *ldflags = (opts->ldflags != NULL) ? opts->ldflags : "";

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
    argv[argc++] = "compilation.kernel_ldflags";
    argv[argc++] = (char *)ldflags;
    argv[argc++] = "--key";
    argv[argc++] = "dependency.tpbench";
    argv[argc++] = "libtpbench" TPB_SHLIB_EXT;
    argv[argc] = NULL;

    err = tpbcli_kernel_set(argc, argv);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    return TPBE_SUCCESS;
}

static int
_sf_canonicalize_path(char *path, size_t pathlen)
{
    char resolved[PATH_MAX];

    if (path == NULL || pathlen == 0) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }
    if (realpath(path, resolved) == NULL) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (strlen(resolved) >= pathlen) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    snprintf(path, pathlen, "%s", resolved);
    return TPBE_SUCCESS;
}

static int
_sf_ensure_registry_cmake(const char *source_dir, const char *tpb_home)
{
    char cmake_path[PATH_MAX];
    char tmpl_path[PATH_MAX];

    if (snprintf(cmake_path, sizeof(cmake_path), "%s/CMakeLists.txt",
                 source_dir) >= (int)sizeof(cmake_path)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (access(cmake_path, R_OK) == 0) {
        return TPBE_SUCCESS;  /* Installed or previously staged */
    }

    /* Fallback: copy shipped template into the kernel source tree */
    if (snprintf(tmpl_path, sizeof(tmpl_path), "%s/etc/cmake/kernel/CMakeLists.registry.txt",
                 tpb_home) >= (int)sizeof(tmpl_path)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (access(tmpl_path, R_OK) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: missing '%s' and template '%s'.\n"
                        "kernel build: reinstall TPBench or rebuild with a current "
                        "version that stages registry CMakeLists.txt files.\n",
                        cmake_path, tmpl_path);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    if (_sf_copy_file(tmpl_path, cmake_path) != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: failed to install registry CMakeLists.txt "
                        "into '%s'.\n", source_dir);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_build_one_kernel(const char *kernel_name,
                     const char *source_dir,
                     const char *tpb_home,
                     const tpbcli_kernel_build_opts_t *opts)
{
    char build_dir[PATH_MAX];
    char tpbench_pkg[PATH_MAX];
    char source_file[PATH_MAX];
    char built_so[PATH_MAX];
    char dest_so[PATH_MAX];
    char lib_dir[PATH_MAX];
    char link_libs[TPBCLI_KERNEL_REG_LINK_MAX];
    char cmd[TPBCLI_KERNEL_BUILD_CMD_MAX];
    char *backup_argv[8];
    int err;

    if (!_sf_path_is_dir(source_dir)) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: directory '%s' not found.\n",
                        source_dir);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    err = _sf_ensure_registry_cmake(source_dir, tpb_home);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    /* Optional per-kernel link libraries from kernel_link_defs.txt */
    link_libs[0] = '\0';
    (void)tpbcli_kernel_reg_link_libs(tpb_home, kernel_name,
                                      link_libs, sizeof(link_libs));

    if (snprintf(tpbench_pkg, sizeof(tpbench_pkg), "%s/lib/cmake/TPBench",
                 tpb_home) >= (int)sizeof(tpbench_pkg)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (access(tpbench_pkg, F_OK) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: TPBench package not found at '%s'.\n",
                        tpbench_pkg);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    if (snprintf(source_file, sizeof(source_file), "%s/tpbk_%s.c",
                 source_dir, kernel_name) >= (int)sizeof(source_file)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (access(source_file, R_OK) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: source '%s' not found.\n",
                        source_file);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    if (snprintf(build_dir, sizeof(build_dir), "%s/build_%s",
                 source_dir, kernel_name) >= (int)sizeof(build_dir)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    err = _sf_ensure_dir(build_dir);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    err = _sf_build_cmake_cmd(cmd, sizeof(cmd), source_dir, build_dir,
                              tpb_home, kernel_name, link_libs, opts, 1);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "kernel build: %s\n", cmd);
    err = _sf_run_shell(cmd);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "kernel build: cmake configure failed for '%s'.\n",
                   kernel_name);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_run_shell");
    }

    err = _sf_build_cmake_cmd(cmd, sizeof(cmd), source_dir, build_dir,
                              tpb_home, kernel_name, link_libs, opts, 0);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "kernel build: %s\n", cmd);
    err = _sf_run_shell(cmd);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "kernel build: cmake build failed for '%s'.\n",
                   kernel_name);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_run_shell");
    }

    if (snprintf(built_so, sizeof(built_so), "%s/lib/libtpbk_%s" TPB_SHLIB_EXT,
                 build_dir, kernel_name) >= (int)sizeof(built_so)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (access(built_so, R_OK) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel build: built library '%s' not found.\n",
                        built_so);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    err = tpb_dl_force_tpb_home(tpb_home);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    if (snprintf(lib_dir, sizeof(lib_dir), "%s/lib", tpb_home) >=
        (int)sizeof(lib_dir)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    err = _sf_ensure_dir(lib_dir);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    backup_argv[0] = "tpbcli";
    backup_argv[1] = "kernel";
    backup_argv[2] = "backup-inactive";
    backup_argv[3] = "--kernel";
    backup_argv[4] = (char *)kernel_name;
    backup_argv[5] = NULL;
    (void)tpbcli_kernel_backup_inactive(5, backup_argv);

    if (snprintf(dest_so, sizeof(dest_so), "%s/libtpbk_%s" TPB_SHLIB_EXT,
                 lib_dir, kernel_name) >= (int)sizeof(dest_so)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    err = _sf_copy_file(built_so, dest_so);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "kernel build: failed to install '%s'.\n", dest_so);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_copy_file");
    }

    err = _sf_activate_installed_kernel(kernel_name, dest_so);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    err = _sf_register_compile_meta(kernel_name, dest_so, source_dir,
                                    build_dir, opts);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "kernel build: installed %s\n", dest_so);
    return TPBE_SUCCESS;
}

int
tpbcli_kernel_build(int argc, char **argv)
{
    tpbcli_kernel_build_args_t args;
    tpbcli_kernel_reg_list_t reg;
    char tpb_home[PATH_MAX];
    const char *names[TPBCLI_KERNEL_BUILD_MAX_KERNELS];
    char source_dir[PATH_MAX];
    char name_storage[TPBCLI_KERNEL_BUILD_MAX_KERNELS]
                     [TPBCLI_KERNEL_REG_NAME_MAX];
    int nkern;
    int err;
    int i;
    int nfail = 0;
    int npass = 0;

    err = _sf_parse_build_args(argc, argv, &args);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);

    err = tpbcli_kernel_resolve_home(args.tpb_home_opt, tpb_home,
                                     sizeof(tpb_home));
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel build: failed to resolve TPB_HOME.\n");
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "tpbcli_kernel_resolve_home");
    }
    (void)_sf_canonicalize_path(tpb_home, sizeof(tpb_home));

    if (args.dir_path == NULL) {
        args.dir_path = tpb_home;
    }

    nkern = _sf_resolve_kernel_names(&args, tpb_home, names,
                                     TPBCLI_KERNEL_BUILD_MAX_KERNELS, &reg);
    if (nkern < 0) {
        return nkern;
    }

    for (i = 0; i < nkern; i++) {
        if (args.kernel_csv != NULL) {
            snprintf(name_storage[i], sizeof(name_storage[i]), "%s",
                     names[i]);
            names[i] = name_storage[i];
        }
    }

    for (i = 0; i < nkern; i++) {
        err = _sf_resolve_kernel_dir(tpb_home, args.dir_path,
                                     args.dir_explicit, names[i], &reg,
                                     source_dir, sizeof(source_dir));
        if (err != TPBE_SUCCESS) {
            nfail++;
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                       "kernel build: %s FAIL (resolve dir)\n", names[i]);
            continue;
        }
        (void)_sf_canonicalize_path(source_dir, sizeof(source_dir));

        err = _sf_build_one_kernel(names[i], source_dir, tpb_home, &args.opts);
        if (err != TPBE_SUCCESS) {
            nfail++;
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                       "kernel build: %s FAIL\n", names[i]);
            continue;
        }
        npass++;
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
                   "kernel build: %s PASS\n", names[i]);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "kernel build: summary %d passed, %d failed (of %d)\n",
               npass, nfail, nkern);
    if (nfail > 0) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}
