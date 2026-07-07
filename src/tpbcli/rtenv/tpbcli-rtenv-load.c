/*
 * tpbcli-rtenv-load.c
 * `tpbcli rtenv load` — emit shell fragment for eval/source.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-rtenv-template.h"

/* Local Function Prototypes */

static int _sf_parse_selector(int argc, char **argv, const char *workspace,
                              int32_t *id_out, char *name_out, size_t nlen);
static int _sf_valid_env_key(const char *key);
static void _sf_shell_quote(const char *in, char *out, size_t outlen);
static void _sf_emit_export(const char *key, const char *value);
static void _sf_emit_prepend(const char *key, const char *value);
static void _sf_emit_append(const char *key, const char *value);

static int
_sf_parse_selector(int argc, char **argv, const char *workspace,
                   int32_t *id_out, char *name_out, size_t nlen)
{
    int i;
    int have_id = 0;
    int32_t id = 0;

    if (name_out != NULL && nlen > 0) {
        name_out[0] = '\0';
    }

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--id") == 0) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "missing -i value");
            }
            id = (int32_t)strtol(argv[++i], NULL, 10);
            have_id = 1;
            continue;
        }
        if (strcmp(argv[i], "--name") == 0) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "missing --name");
            }
            if (tpbcli_rtenv_find_id_by_name(workspace, argv[++i], &id) !=
                TPBE_SUCCESS) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_LIST_NOT_FOUND, "RTEnv name");
            }
            have_id = 1;
        }
    }

    if (have_id) {
        if (!tpbcli_rtenv_id_exists(workspace, id)) {
            TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_LIST_NOT_FOUND, "RTEnv id");
        }
        *id_out = id;
    } else {
        if (tpbcli_rtenv_resolve_active_id_cli(workspace, &id) !=
                TPBE_SUCCESS ||
            !tpbcli_rtenv_id_exists(workspace, id)) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                       TPBLOG_FLAG_DIRECT,
                       "error: no runtime environment selected\n");
            TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
        }
        *id_out = id;
    }

    {
        tpb_raf_rtenv_entry_t *ents = NULL;
        int n = 0;
        int j;

        if (tpb_raf_entry_list_rtenv(workspace, &ents, &n) == TPBE_SUCCESS) {
            for (j = 0; j < n; j++) {
                if (ents[j].id == *id_out && name_out != NULL && nlen > 0) {
                    snprintf(name_out, nlen, "%s", ents[j].name);
                    break;
                }
            }
        }
        free(ents);
    }
    return TPBE_SUCCESS;
}

static int
_sf_valid_env_key(const char *key)
{
    size_t i;

    if (key == NULL || key[0] == '\0') {
        return 0;
    }
    if (!(isalpha((unsigned char)key[0]) || key[0] == '_')) {
        return 0;
    }
    for (i = 1; key[i] != '\0'; i++) {
        if (!(isalnum((unsigned char)key[i]) || key[i] == '_')) {
            return 0;
        }
    }
    return 1;
}

static void
_sf_shell_quote(const char *in, char *out, size_t outlen)
{
    size_t o = 0;
    size_t i;

    if (outlen == 0) {
        return;
    }
    out[o++] = '\'';
    for (i = 0; in != NULL && in[i] != '\0' && o + 2 < outlen; i++) {
        if (in[i] == '\'') {
            if (o + 4 >= outlen) {
                break;
            }
            out[o++] = '\'';
            out[o++] = '\\';
            out[o++] = '\'';
            out[o++] = '\'';
        } else {
            out[o++] = in[i];
        }
    }
    if (o < outlen) {
        out[o++] = '\'';
        out[o] = '\0';
    }
}

static void
_sf_emit_export(const char *key, const char *value)
{
    char qv[8192];

    _sf_shell_quote(value, qv, sizeof(qv));
    printf("export %s=%s\n", key, qv);
}

static void
_sf_emit_prepend(const char *key, const char *value)
{
    char qv[8192];

    _sf_shell_quote(value, qv, sizeof(qv));
    printf("if [ -n \"${%s:-}\" ]; then export %s=%s:\"$%s\"; "
           "else export %s=%s; fi\n",
           key, key, qv, key, key, qv);
}

static void
_sf_emit_append(const char *key, const char *value)
{
    char qv[8192];

    _sf_shell_quote(value, qv, sizeof(qv));
    printf("if [ -n \"${%s:-}\" ]; then export %s=\"$%s\":%s; "
           "else export %s=%s; fi\n",
           key, key, key, qv, key, qv);
}

int
tpbcli_rtenv_load(int argc, char **argv, const char *workspace)
{
    int32_t id;
    char env_name[256];
    tpbcli_rtenv_merged_t merged;
    char id_text[32];
    char qid[64];
    char qname[512];
    int i;
    int err;

    err = _sf_parse_selector(argc, argv, workspace, &id, env_name,
                           sizeof(env_name));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpbcli_rtenv_merge_chain(workspace, id, &merged);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "merge chain");
    }

    snprintf(id_text, sizeof(id_text), "%d", (int)id);
    _sf_shell_quote(id_text, qid, sizeof(qid));
    printf("export TPB_RTENV_ID=%s\n", qid);
    _sf_shell_quote(env_name, qname, sizeof(qname));
    printf("export TPB_RTENV_NAME=%s\n", qname);

    for (i = 0; i < merged.nenv; i++) {
        if (!_sf_valid_env_key(merged.vars[i].key)) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                       TPBLOG_FLAG_DIRECT,
                       "error: invalid environment variable key '%s'\n",
                       merged.vars[i].key);
            TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
        }
        if (merged.vars[i].mode == 3) {
            printf("unset %s\n", merged.vars[i].key);
        } else if (merged.vars[i].mode == 1) {
            _sf_emit_prepend(merged.vars[i].key, merged.vars[i].value);
        } else if (merged.vars[i].mode == 2) {
            _sf_emit_append(merged.vars[i].key, merged.vars[i].value);
        } else {
            _sf_emit_export(merged.vars[i].key, merged.vars[i].value);
        }
    }
    return TPBE_SUCCESS;
}
