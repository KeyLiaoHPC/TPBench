/*
 * tpbcli-kernel-init.c
 * `tpbcli kernel init` — copy CMake/C skeleton from ${TPB_HOME}/etc/cmake/kernel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-kernel-home.h"
#include "tpbcli-kernel-init.h"

#define TPBCLI_KERNEL_INIT_TEMPLATE_DIR  "etc/cmake/kernel"
#define TPBCLI_KERNEL_INIT_CMAKE_TMPL   "CMakeLists.txt.in"
#define TPBCLI_KERNEL_INIT_SOURCE_TMPL  "tpbk_kernel.c.in"

/* Local Function Prototypes */
static void _sf_print_init_usage(void);
static int _sf_read_template(const char *path, char **out, size_t *outlen);
static int _sf_subst_kernel_name(const char *tmpl, size_t tmpl_len,
                                 const char *kernel_name,
                                 char **out, size_t *outlen);
static int _sf_write_new_file(const char *path, const char *content);

static void
_sf_print_init_usage(void)
{
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "Usage: tpbcli kernel init --dir <path> --kernel <kernel_name>\n");
}

static int
_sf_read_template(const char *path, char **out, size_t *outlen)
{
    FILE *fp;
    long sz;
    char *buf;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    buf = (char *)malloc((size_t)sz + 1U);
    if (buf == NULL) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_MALLOC_FAIL, NULL);
    }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    buf[sz] = '\0';
    fclose(fp);
    *out = buf;
    *outlen = (size_t)sz;
    return TPBE_SUCCESS;
}

static int
_sf_subst_kernel_name(const char *tmpl, size_t tmpl_len,
                      const char *kernel_name, char **out, size_t *outlen)
{
    static const char *needle = "@KERNEL_NAME@";
    size_t needle_len = strlen(needle);
    size_t klen = strlen(kernel_name);
    size_t count = 0;
    size_t i;
    size_t pos = 0;
    char *buf;
    const char *p;

    for (p = tmpl; (p = strstr(p, needle)) != NULL; p += needle_len) {
        count++;
    }
    buf = (char *)malloc(tmpl_len + count * (klen - needle_len) + 1U);
    if (buf == NULL) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_MALLOC_FAIL, NULL);
    }

    i = 0;
    while (i < tmpl_len) {
        if (strncmp(tmpl + i, needle, needle_len) == 0) {
            memcpy(buf + pos, kernel_name, klen);
            pos += klen;
            i += needle_len;
        } else {
            buf[pos++] = tmpl[i++];
        }
    }
    buf[pos] = '\0';
    *out = buf;
    *outlen = pos;
    return TPBE_SUCCESS;
}

static int
_sf_write_new_file(const char *path, const char *content)
{
    FILE *fp;

    if (access(path, F_OK) == 0) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (fputs(content, fp) == EOF) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    fclose(fp);
    return TPBE_SUCCESS;
}

int
tpbcli_kernel_init(int argc, char **argv)
{
    const char *dir_path = NULL;
    const char *kernel_name = NULL;
    const char *tpb_home;
    char tmpl_dir[PATH_MAX];
    char cmake_tmpl[PATH_MAX];
    char source_tmpl[PATH_MAX];
    char cmake_out[PATH_MAX];
    char source_out[PATH_MAX];
    char *raw = NULL;
    char *rendered = NULL;
    size_t raw_len;
    size_t rendered_len;
    int err;
    int i;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            dir_path = argv[++i];
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            kernel_name = argv[++i];
        } else {
            _sf_print_init_usage();
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
        }
    }

    if (dir_path == NULL || kernel_name == NULL) {
        _sf_print_init_usage();
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }
    if (!tpbcli_kernel_name_valid(kernel_name)) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel init: invalid kernel name '%s'.\n",
                        kernel_name);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    tpb_home = tpb_dl_get_tpb_home();
    if (tpb_home == NULL || tpb_home[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel init: TPB_HOME not resolved.\n");
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    if (snprintf(tmpl_dir, sizeof(tmpl_dir), "%s/%s",
                 tpb_home, TPBCLI_KERNEL_INIT_TEMPLATE_DIR) >= (int)sizeof(tmpl_dir)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (snprintf(cmake_tmpl, sizeof(cmake_tmpl), "%s/%s",
                 tmpl_dir, TPBCLI_KERNEL_INIT_CMAKE_TMPL) >= (int)sizeof(cmake_tmpl)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (snprintf(source_tmpl, sizeof(source_tmpl), "%s/%s",
                 tmpl_dir, TPBCLI_KERNEL_INIT_SOURCE_TMPL) >= (int)sizeof(source_tmpl)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    if (mkdir(dir_path, 0755) != 0 && access(dir_path, F_OK) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel init: cannot create directory '%s'.\n",
                        dir_path);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    if (snprintf(cmake_out, sizeof(cmake_out), "%s/CMakeLists.txt",
                 dir_path) >= (int)sizeof(cmake_out)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (snprintf(source_out, sizeof(source_out), "%s/tpbk_%s.c",
                 dir_path, kernel_name) >= (int)sizeof(source_out)) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    err = _sf_read_template(cmake_tmpl, &raw, &raw_len);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel init: missing template '%s'.\n", cmake_tmpl);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_read_template");
    }
    err = _sf_subst_kernel_name(raw, raw_len, kernel_name,
                               &rendered, &rendered_len);
    free(raw);
    raw = NULL;
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    err = _sf_write_new_file(cmake_out, rendered);
    free(rendered);
    rendered = NULL;
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel init: '%s' already exists or write failed.\n",
                        cmake_out);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_write_new_file");
    }

    err = _sf_read_template(source_tmpl, &raw, &raw_len);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "kernel init: missing template '%s'.\n", source_tmpl);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_read_template");
    }
    err = _sf_subst_kernel_name(raw, raw_len, kernel_name,
                               &rendered, &rendered_len);
    free(raw);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    err = _sf_write_new_file(source_out, rendered);
    free(rendered);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "kernel init: '%s' already exists or write failed.\n",
                        source_out);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "_sf_write_new_file");
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "kernel init: created %s and %s\n", cmake_out, source_out);
    return TPBE_SUCCESS;
}
