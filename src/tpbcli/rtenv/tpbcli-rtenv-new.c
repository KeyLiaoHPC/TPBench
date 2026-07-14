/*
 * tpbcli-rtenv-new.c
 * `tpbcli rtenv new|create` — create runtime environment records.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tpb-public.h"
#include "tpbcli-rtenv-template.h"

/* Local Function Prototypes */

static int _sf_match_arg(const char *arg, const char *short_name,
                         const char *long_name);
static int _sf_is_new_block_start(const char *arg);
static int _sf_parse_new_argv(int argc, char **argv, tpbcli_rtenv_spec_t *spec,
                              const char **out_file, const char **in_file,
                              int *output_only);
static int _sf_resolve_inherit(const char *workspace, tpbcli_rtenv_spec_t *spec,
                               int32_t *inherit_out);
static int _sf_commit_new(const char *workspace, tpbcli_rtenv_spec_t *spec,
                          int32_t inherit_from);

static int
_sf_match_arg(const char *arg, const char *short_name, const char *long_name)
{
    if (arg == NULL) {
        return 0;
    }
    if (short_name != NULL && strcmp(arg, short_name) == 0) {
        return 1;
    }
    if (long_name != NULL && strcmp(arg, long_name) == 0) {
        return 1;
    }
    return 0;
}

static int
_sf_is_new_block_start(const char *arg)
{
    return _sf_match_arg(arg, "-app", "--application") ||
           _sf_match_arg(arg, "-evo", "--env-var-overwrite") ||
           _sf_match_arg(arg, "-evp", "--env-var-prepend") ||
           _sf_match_arg(arg, "-eva", "--env-var-append") ||
           _sf_match_arg(arg, "-o", "--output-file") ||
           _sf_match_arg(arg, "-f", "--input-file") ||
           strcmp(arg, "--inherit-from") == 0;
}

static int
_sf_parse_new_argv(int argc, char **argv, tpbcli_rtenv_spec_t *spec,
                   const char **out_file, const char **in_file,
                   int *output_only)
{
    int i;

    if (spec == NULL || out_file == NULL || in_file == NULL ||
        output_only == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    *out_file = NULL;
    *in_file = NULL;
    *output_only = 0;
    memset(spec, 0, sizeof(*spec));
    spec->inherit_from = -1;

    for (i = 3; i < argc; i++) {
        if (_sf_match_arg(argv[i], "-o", "--output-file")) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                         "missing argument for -o");
            }
            *out_file = argv[++i];
            *output_only = 1;
            continue;
        }
        if (_sf_match_arg(argv[i], "-f", "--input-file")) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                         "missing argument for -f");
            }
            *in_file = argv[++i];
            continue;
        }
        if (_sf_match_arg(argv[i], "-n", "--name")) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "missing -n value");
            }
            snprintf(spec->env_name, sizeof(spec->env_name), "%s", argv[++i]);
            continue;
        }
        if (_sf_match_arg(argv[i], "-N", "--note")) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "missing -N value");
            }
            snprintf(spec->note, sizeof(spec->note), "%s", argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--inherit-from") == 0) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                         "missing --inherit-from value");
            }
            snprintf(spec->inherit_from_text, sizeof(spec->inherit_from_text),
                     "%s", argv[++i]);
            spec->inherit_from = (int32_t)strtol(spec->inherit_from_text, NULL, 10);
            spec->has_inherit = 1;
            continue;
        }
        if (_sf_match_arg(argv[i], "-app", "--application")) {
            char aname[64] = "";
            char aver[64] = "";
            char anote[64] = "";

            while (i + 1 < argc && !_sf_is_new_block_start(argv[i + 1])) {
                const char *opt = argv[++i];

                if (_sf_match_arg(opt, "-n", "--name")) {
                    if (i + 1 >= argc) {
                        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                                 "missing application -n value");
                    }
                    snprintf(aname, sizeof(aname), "%s", argv[++i]);
                } else if (_sf_match_arg(opt, "-v", "--value")) {
                    if (i + 1 >= argc) {
                        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                                 "missing application -v value");
                    }
                    snprintf(aver, sizeof(aver), "%s", argv[++i]);
                } else if (_sf_match_arg(opt, "-N", "--note")) {
                    if (i + 1 >= argc) {
                        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                                 "missing application -N value");
                    }
                    snprintf(anote, sizeof(anote), "%s", argv[++i]);
                } else {
                    TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                             "unknown option after -app");
                }
            }
            if (aname[0] == '\0') {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                         "application -n required");
            }
            if (spec->napp >= TPBCLI_RTENV_MAX_APP) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "too many applications");
            }
            snprintf(spec->apps[spec->napp].name, 64, "%s", aname);
            snprintf(spec->apps[spec->napp].version, 64, "%s", aver);
            snprintf(spec->apps[spec->napp].note, 64, "%s", anote);
            spec->napp++;
            continue;
        }
        if (_sf_match_arg(argv[i], "-evo", "--env-var-overwrite") ||
            _sf_match_arg(argv[i], "-evp", "--env-var-prepend") ||
            _sf_match_arg(argv[i], "-eva", "--env-var-append")) {
            uint32_t on_set = TPB_RTENV_ON_SET_OVERWRITE;
            uint32_t on_get = TPB_RTENV_ON_GET_OVERWRITE;
            char vname[256] = "";
            char vval[4096] = "";

            if (_sf_match_arg(argv[i], "-evp", "--env-var-prepend")) {
                on_set = TPB_RTENV_ON_SET_PREPEND;
            } else if (_sf_match_arg(argv[i], "-eva", "--env-var-append")) {
                on_set = TPB_RTENV_ON_SET_APPEND;
            }
            while (i + 1 < argc && !_sf_is_new_block_start(argv[i + 1])) {
                const char *opt = argv[++i];

                if (_sf_match_arg(opt, "-k", "--key")) {
                    if (i + 1 >= argc) {
                        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                                 "missing -k value");
                    }
                    snprintf(vname, sizeof(vname), "%s", argv[++i]);
                } else if (_sf_match_arg(opt, "-v", "--value")) {
                    if (i + 1 >= argc) {
                        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                                 "missing -v value");
                    }
                    snprintf(vval, sizeof(vval), "%s", argv[++i]);
                } else {
                    TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                             "unknown option after env-var flag");
                }
            }
            if (vname[0] == '\0') {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                         "env-var -k required");
            }
            if (spec->nenv >= TPBCLI_RTENV_MAX_VAR) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL,
                         "too many variables");
            }
            snprintf(spec->vars[spec->nenv].key,
                     sizeof(spec->vars[0].key), "%s", vname);
            snprintf(spec->vars[spec->nenv].value,
                     sizeof(spec->vars[0].value), "%s", vval);
            spec->vars[spec->nenv].on_set = on_set;
            spec->vars[spec->nenv].on_get = on_get;
            spec->nenv++;
            continue;
        }
    }
    return TPBE_SUCCESS;
}

static int
_sf_resolve_inherit(const char *workspace, tpbcli_rtenv_spec_t *spec,
                    int32_t *inherit_out)
{
    int32_t id;
    char *end;
    long v;

    if (spec->has_inherit) {
        v = strtol(spec->inherit_from_text, &end, 10);
        if (end != spec->inherit_from_text && *end == '\0' && v == 0) {
            *inherit_out = 0;
            return TPBE_SUCCESS;
        }
        if (end != spec->inherit_from_text && *end == '\0' &&
            tpbcli_rtenv_id_exists(workspace, (int32_t)v)) {
            *inherit_out = (int32_t)v;
            return TPBE_SUCCESS;
        }
        if (tpbcli_rtenv_find_id_by_name(workspace, spec->inherit_from_text,
                                         &id) != TPBE_SUCCESS) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                       TPBLOG_FLAG_DIRECT,
                       "error: inherit-from RTEnv not found\n");
            TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_LIST_NOT_FOUND, NULL);
        }
        *inherit_out = id;
        return TPBE_SUCCESS;
    }

    if (tpbcli_rtenv_resolve_active_id_cli(workspace, &id) != TPBE_SUCCESS) {
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "resolve active RTEnv");
    }
    if (!tpbcli_rtenv_id_exists(workspace, id)) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                   TPBLOG_FLAG_DIRECT,
                   "error: no active runtime environment; set TPB_RTENV_ID or "
                   "use --inherit-from\n");
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
    }
    *inherit_out = id;
    return TPBE_SUCCESS;
}

static int
_sf_commit_new(const char *workspace, tpbcli_rtenv_spec_t *spec,
               int32_t inherit_from)
{
    int32_t id;
    int err;

    if (spec->env_name[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                   TPBLOG_FLAG_DIRECT, "error: environment -n required\n");
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
    }

    err = tpb_raf_rtenv_alloc_next_id(workspace, &id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "tpb_raf_rtenv_alloc_next_id");
    }

    err = tpbcli_rtenv_write_record(workspace, id, spec->env_name, spec->note,
                                    inherit_from, spec);
    if (err == TPBE_LIST_DUP) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                   TPBLOG_FLAG_DIRECT,
                   "error: runtime environment name already exists\n");
    }
    TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "tpbcli_rtenv_write_record");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Created runtime environment id=%d name=%s inherit_from=%d\n",
               (int)id, spec->env_name, (int)inherit_from);
    return TPBE_SUCCESS;
}

int
tpbcli_rtenv_new(int argc, char **argv, const char *workspace)
{
    tpbcli_rtenv_spec_t spec;
    const char *out_file = NULL;
    const char *in_file = NULL;
    int output_only = 0;
    int32_t inherit_from = -1;
    int err;
    int error_line = 0;

    err = _sf_parse_new_argv(argc, argv, &spec, &out_file, &in_file,
                             &output_only);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (out_file != NULL && in_file != NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                   TPBLOG_FLAG_DIRECT,
                   "error: -o and -f are mutually exclusive\n");
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
    }

    if (output_only) {
        FILE *fp = fopen(out_file, "w");
        if (fp == NULL) {
            TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_FILE_IO_FAIL, out_file);
        }
        if (spec.napp == 0 && spec.nenv == 0 && spec.env_name[0] == '\0') {
            err = tpbcli_rtenv_write_active_template(workspace, fp);
        } else {
            err = tpbcli_rtenv_write_template(&spec, fp);
        }
        fclose(fp);
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "tpbcli_rtenv_write_template");
        return TPBE_SUCCESS;
    }

    if (in_file != NULL) {
        err = tpbcli_rtenv_parse_template_file(in_file, &spec, &error_line);
        if (err != TPBE_SUCCESS) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                       TPBLOG_FLAG_DIRECT,
                       "error: failed to parse template %s", in_file);
            if (error_line > 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                           TPBLOG_FLAG_DIRECT, " at line %d", error_line);
            }
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                       TPBLOG_FLAG_DIRECT, "\n");
            TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "parse template");
        }
    } else if (spec.napp == 0 && spec.nenv == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                   TPBLOG_FLAG_DIRECT,
                   "error: no inline application/variable options; use -f or "
                   "provide -app / -evo|-evp|-eva\n");
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
    }

    err = _sf_resolve_inherit(workspace, &spec, &inherit_from);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    return _sf_commit_new(workspace, &spec, inherit_from);
}
