/*
 * tpbcli-rtenv-template.c
 * RTEnv template I/O, merge, and record creation helpers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tpb-public.h"
#include "tpbcli-rtenv-template.h"

/* Local Function Prototypes */
static int _sf_unquote(const char *in, char *out, size_t outlen);
static int _sf_parse_on_set(const char *s, uint32_t *out);
static int _sf_parse_on_get(const char *s, uint32_t *out);
static const char *_sf_on_set_name(uint32_t v);
static const char *_sf_on_get_name(uint32_t v);
static int _sf_split_colon_quoted(const char *in, char **parts, int max_parts);
static void _sf_free_parts(char **parts, int n);
static int _sf_add_app(tpbcli_rtenv_spec_t *spec, const char *name,
                       const char *version, const char *note);
static int _sf_add_var(tpbcli_rtenv_spec_t *spec, const char *key,
                       const char *value, uint32_t on_set, uint32_t on_get);
static int _sf_merge_add_app(tpbcli_rtenv_merged_t *out, const char *name,
                             const char *version, const char *note);
static int _sf_merge_add_var(tpbcli_rtenv_merged_t *out, const char *key,
                             const char *value, uint32_t on_set,
                             uint32_t on_get);
static size_t _sf_hdr_payload_off(const tpb_meta_header_t *hdrs, int n, int idx);
static int _sf_merge_record_delta(const char *workspace, int32_t id,
                                  tpbcli_rtenv_merged_t *out);
static int _sf_rtenv_id_exists(const char *workspace, int32_t id);
static int _sf_join_colon_parts(char **parts, int from, int n, char *out,
                                size_t outlen);
static void _sf_rtrim_inplace(char *s);

static void
_sf_rtrim_inplace(char *s)
{
    size_t n;

    if (s == NULL) {
        return;
    }
    n = strlen(s);
    while (n > 0 &&
           (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' ||
            s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

static int
_sf_unquote(const char *in, char *out, size_t outlen)
{
    size_t n;

    if (in == NULL || out == NULL || outlen == 0) {
        return -1;
    }
    if (in[0] == '\'' || in[0] == '"') {
        char q = in[0];
        const char *end = strrchr(in + 1, q);
        if (end == NULL) {
            return -1;
        }
        n = (size_t)(end - (in + 1));
        if (n >= outlen) {
            n = outlen - 1;
        }
        memcpy(out, in + 1, n);
        out[n] = '\0';
        return 0;
    }
    if (in != out) {
        snprintf(out, outlen, "%s", in);
    }
    return 0;
}

static int
_sf_parse_on_set(const char *s, uint32_t *out)
{
    if (s == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(s, "ignore") == 0) {
        *out = TPB_RTENV_ON_SET_IGNORE;
    } else if (strcmp(s, "overwrite") == 0) {
        *out = TPB_RTENV_ON_SET_OVERWRITE;
    } else if (strcmp(s, "prepend") == 0) {
        *out = TPB_RTENV_ON_SET_PREPEND;
    } else if (strcmp(s, "append") == 0) {
        *out = TPB_RTENV_ON_SET_APPEND;
    } else {
        return -1;
    }
    return 0;
}

static int
_sf_parse_on_get(const char *s, uint32_t *out)
{
    if (s == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(s, "ignore") == 0) {
        *out = TPB_RTENV_ON_GET_IGNORE;
    } else if (strcmp(s, "warn") == 0) {
        *out = TPB_RTENV_ON_GET_WARN;
    } else if (strcmp(s, "fail") == 0) {
        *out = TPB_RTENV_ON_GET_FAIL;
    } else if (strcmp(s, "overwrite") == 0) {
        *out = TPB_RTENV_ON_GET_OVERWRITE;
    } else {
        return -1;
    }
    return 0;
}

static const char *
_sf_on_set_name(uint32_t v)
{
    switch (v) {
    case TPB_RTENV_ON_SET_IGNORE: return "ignore";
    case TPB_RTENV_ON_SET_OVERWRITE: return "overwrite";
    case TPB_RTENV_ON_SET_PREPEND: return "prepend";
    case TPB_RTENV_ON_SET_APPEND: return "append";
    default: return "?";
    }
}

static const char *
_sf_on_get_name(uint32_t v)
{
    switch (v) {
    case TPB_RTENV_ON_GET_IGNORE: return "ignore";
    case TPB_RTENV_ON_GET_WARN: return "warn";
    case TPB_RTENV_ON_GET_FAIL: return "fail";
    case TPB_RTENV_ON_GET_OVERWRITE: return "overwrite";
    default: return "?";
    }
}

static int
_sf_split_colon_quoted(const char *in, char **parts, int max_parts)
{
    const char *p;
    int n = 0;
    char q = 0;

    if (in == NULL || parts == NULL || max_parts <= 0) {
        return -1;
    }
    p = in;
    while (*p != '\0' && n < max_parts) {
        const char *start;
        size_t len;

        if (*p == ':') {
            p++;
            continue;
        }
        start = p;
        if (*p == '\'' || *p == '"') {
            q = *p++;
            while (*p != '\0' && *p != q) {
                p++;
            }
            if (*p == q) {
                p++;
            }
        } else {
            while (*p != '\0' && *p != ':') {
                p++;
            }
        }
        len = (size_t)(p - start);
        parts[n] = (char *)malloc(len + 1);
        if (parts[n] == NULL) {
            return -1;
        }
        memcpy(parts[n], start, len);
        parts[n][len] = '\0';
        _sf_unquote(parts[n], parts[n], len + 1);
        n++;
    }
    return n;
}

static void
_sf_free_parts(char **parts, int n)
{
    int i;

    if (parts == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        free(parts[i]);
        parts[i] = NULL;
    }
}

static int
_sf_join_colon_parts(char **parts, int from, int n, char *out, size_t outlen)
{
    size_t pos = 0;
    int i;

    if (out == NULL || outlen == 0) {
        return -1;
    }
    out[0] = '\0';
    for (i = from; i < n; i++) {
        size_t plen = strlen(parts[i]);
        if (i > from) {
            if (pos + 1 >= outlen) {
                return -1;
            }
            out[pos++] = ':';
        }
        if (pos + plen >= outlen) {
            return -1;
        }
        memcpy(out + pos, parts[i], plen);
        pos += plen;
        out[pos] = '\0';
    }
    return 0;
}

static int
_sf_add_app(tpbcli_rtenv_spec_t *spec, const char *name, const char *version,
            const char *note)
{
    if (spec->napp >= TPBCLI_RTENV_MAX_APP) {
        return -1;
    }
    snprintf(spec->apps[spec->napp].name, sizeof(spec->apps[0].name),
             "%s", name);
    snprintf(spec->apps[spec->napp].version, sizeof(spec->apps[0].version),
             "%s", version);
    snprintf(spec->apps[spec->napp].note, sizeof(spec->apps[0].note),
             "%s", note);
    spec->napp++;
    return 0;
}

static int
_sf_add_var(tpbcli_rtenv_spec_t *spec, const char *key, const char *value,
            uint32_t on_set, uint32_t on_get)
{
    if (spec->nenv >= TPBCLI_RTENV_MAX_VAR) {
        return -1;
    }
    snprintf(spec->vars[spec->nenv].key, sizeof(spec->vars[0].key), "%s", key);
    snprintf(spec->vars[spec->nenv].value, sizeof(spec->vars[0].value),
             "%s", value ? value : "");
    spec->vars[spec->nenv].on_set = on_set;
    spec->vars[spec->nenv].on_get = on_get;
    spec->nenv++;
    return 0;
}

static int
_sf_merge_add_app(tpbcli_rtenv_merged_t *out, const char *name,
                  const char *version, const char *note)
{
    int i;

    for (i = 0; i < out->napp; i++) {
        if (strcmp(out->apps[i].name, name) == 0) {
            snprintf(out->apps[i].version, sizeof(out->apps[i].version),
                     "%s", version);
            snprintf(out->apps[i].note, sizeof(out->apps[i].note), "%s", note);
            return 0;
        }
    }
    if (out->napp >= TPBCLI_RTENV_MAX_APP) {
        return -1;
    }
    snprintf(out->apps[out->napp].name, sizeof(out->apps[0].name), "%s", name);
    snprintf(out->apps[out->napp].version, sizeof(out->apps[0].version),
             "%s", version);
    snprintf(out->apps[out->napp].note, sizeof(out->apps[0].note), "%s", note);
    out->napp++;
    return 0;
}

static int
_sf_merge_add_var(tpbcli_rtenv_merged_t *out, const char *key,
                  const char *value, uint32_t on_set, uint32_t on_get)
{
    int i;

    for (i = 0; i < out->nenv; i++) {
        if (strcmp(out->vars[i].key, key) == 0) {
            snprintf(out->vars[i].value, sizeof(out->vars[0].value),
                     "%s", value ? value : "");
            out->vars[i].on_set = on_set;
            out->vars[i].on_get = on_get;
            return 0;
        }
    }
    if (out->nenv >= TPBCLI_RTENV_MAX_VAR) {
        return -1;
    }
    snprintf(out->vars[out->nenv].key, sizeof(out->vars[0].key), "%s", key);
    snprintf(out->vars[out->nenv].value, sizeof(out->vars[0].value),
             "%s", value ? value : "");
    out->vars[out->nenv].on_set = on_set;
    out->vars[out->nenv].on_get = on_get;
    out->nenv++;
    return 0;
}

static size_t
_sf_hdr_payload_off(const tpb_meta_header_t *hdrs, int n, int idx)
{
    size_t off = 0;
    int i;

    for (i = 0; i < idx && i < n; i++) {
        off += (size_t)hdrs[i].data_size;
    }
    return off;
}

static int
_sf_rtenv_id_exists(const char *workspace, int32_t id)
{
    tpb_raf_rtenv_entry_t *ents = NULL;
    int n = 0;
    int i;
    int found = 0;

    if (tpb_raf_entry_list_rtenv(workspace, &ents, &n) != TPBE_SUCCESS) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (ents[i].id == id) {
            found = 1;
            break;
        }
    }
    free(ents);
    return found;
}

int
tpbcli_rtenv_write_template(const tpbcli_rtenv_spec_t *spec, FILE *fp)
{
    int i;
    char *parts[64];

    if (spec == NULL || fp == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    fprintf(fp,
            "# Set this ID to inherit settings from an existing "
            "runtime_environment tpbr, use `tpbcli rtenv ls` and "
            "`tpbcli rtenv show` to view existing records.\n");
    if (spec->has_inherit) {
        fprintf(fp, "inherit_from=%s\n", spec->inherit_from_text);
    } else {
        fprintf(fp, "inherit_from=\n");
    }
    if (spec->env_name[0] != '\0') {
        fprintf(fp, "name=%s\n", spec->env_name);
    }
    if (spec->note[0] != '\0') {
        fprintf(fp, "note=%s\n", spec->note);
    }
    fprintf(fp, "# Set applications used in the run-time environment.\n");
    for (i = 0; i < spec->napp; i++) {
        fprintf(fp, "application=%s:%s:%s\n", spec->apps[i].name,
                spec->apps[i].version, spec->apps[i].note);
    }
    fprintf(fp, "# Set environment variables and modes\n");
    for (i = 0; i < spec->nenv; i++) {
        int np;
        char val_joined[4096];
        int j;

        np = _sf_split_colon_quoted(spec->vars[i].value, parts, 64);
        if (np < 0) {
            return TPBE_CLI_FAIL;
        }
        fprintf(fp, "var=%s:%s:%s", _sf_on_get_name(spec->vars[i].on_get),
                _sf_on_set_name(spec->vars[i].on_set), spec->vars[i].key);
        if (np > 0) {
            if (_sf_join_colon_parts(parts, 0, np, val_joined,
                                     sizeof(val_joined)) != 0) {
                _sf_free_parts(parts, np);
                return TPBE_CLI_FAIL;
            }
            np = _sf_split_colon_quoted(val_joined, parts, 64);
        }
        for (j = 0; j < np; j++) {
            if (strchr(parts[j], ':') != NULL || strchr(parts[j], ' ') != NULL) {
                fprintf(fp, ":'%s'", parts[j]);
            } else {
                fprintf(fp, ":%s", parts[j]);
            }
        }
        fprintf(fp, "\n");
        _sf_free_parts(parts, np);
    }
    return TPBE_SUCCESS;
}

int
tpbcli_rtenv_merged_to_spec(const tpbcli_rtenv_merged_t *merged,
                            tpbcli_rtenv_spec_t *spec)
{
    int i;

    if (merged == NULL || spec == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    spec->napp = merged->napp;
    spec->nenv = merged->nenv;
    for (i = 0; i < merged->napp; i++) {
        spec->apps[i] = merged->apps[i];
    }
    for (i = 0; i < merged->nenv; i++) {
        spec->vars[i] = merged->vars[i];
    }
    return TPBE_SUCCESS;
}

int
tpbcli_rtenv_write_active_template(const char *workspace, FILE *fp)
{
    int32_t id;
    tpbcli_rtenv_merged_t merged;
    tpbcli_rtenv_spec_t spec;
    int err;

    if (workspace == NULL || fp == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    memset(&spec, 0, sizeof(spec));
    err = tpbcli_rtenv_resolve_active_id_cli(workspace, &id);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    err = tpbcli_rtenv_merge_chain(workspace, id, &merged);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    err = tpbcli_rtenv_merged_to_spec(&merged, &spec);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    snprintf(spec.inherit_from_text, sizeof(spec.inherit_from_text), "%d",
             (int)id);
    spec.has_inherit = 1;
    return tpbcli_rtenv_write_template(&spec, fp);
}

int
tpbcli_rtenv_parse_template_file(const char *path, tpbcli_rtenv_spec_t *spec,
                                 int *error_line)
{
    FILE *fp;
    char line[4096];
    int ln = 0;

    if (path == NULL || spec == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        char *eq;
        char *rhs;
        char *parts[64];
        int np;

        ln++;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '#' || *p == '\0' || *p == '\n') {
            continue;
        }
        eq = strchr(p, '=');
        if (eq == NULL) {
            fclose(fp);
            if (error_line) {
                *error_line = ln;
            }
            return TPBE_CLI_FAIL;
        }
        *eq = '\0';
        _sf_rtrim_inplace(p);
        rhs = eq + 1;
        _sf_rtrim_inplace(rhs);
        if (strcmp(p, "name") == 0) {
            snprintf(spec->env_name, sizeof(spec->env_name), "%s", rhs);
            continue;
        }
        if (strcmp(p, "note") == 0) {
            snprintf(spec->note, sizeof(spec->note), "%s", rhs);
            continue;
        }
        if (strcmp(p, "inherit_from") == 0) {
            if (rhs[0] != '\0') {
                snprintf(spec->inherit_from_text,
                         sizeof(spec->inherit_from_text), "%s", rhs);
                spec->inherit_from =
                    (int32_t)strtol(spec->inherit_from_text, NULL, 10);
                spec->has_inherit = 1;
            }
            continue;
        }
        np = _sf_split_colon_quoted(rhs, parts, 64);
        if (np < 0) {
            fclose(fp);
            if (error_line) {
                *error_line = ln;
            }
            return TPBE_CLI_FAIL;
        }
        if (strcmp(p, "application") == 0) {
            if (np < 1 || _sf_add_app(spec, parts[0],
                                      np >= 2 ? parts[1] : "",
                                      np >= 3 ? parts[2] : "") != 0) {
                _sf_free_parts(parts, np);
                fclose(fp);
                if (error_line) {
                    *error_line = ln;
                }
                return TPBE_CLI_FAIL;
            }
        } else if (strcmp(p, "var") == 0) {
            uint32_t on_get = TPB_RTENV_ON_GET_IGNORE;
            uint32_t on_set = TPB_RTENV_ON_SET_IGNORE;
            char valbuf[4096];

            if (np < 3 || _sf_parse_on_get(parts[0], &on_get) != 0 ||
                _sf_parse_on_set(parts[1], &on_set) != 0) {
                _sf_free_parts(parts, np);
                fclose(fp);
                if (error_line) {
                    *error_line = ln;
                }
                return TPBE_CLI_FAIL;
            }
            valbuf[0] = '\0';
            if (np > 3 &&
                _sf_join_colon_parts(parts, 3, np, valbuf, sizeof(valbuf)) != 0) {
                _sf_free_parts(parts, np);
                fclose(fp);
                if (error_line) {
                    *error_line = ln;
                }
                return TPBE_CLI_FAIL;
            }
            if (_sf_add_var(spec, parts[2], valbuf, on_set, on_get) != 0) {
                _sf_free_parts(parts, np);
                fclose(fp);
                if (error_line) {
                    *error_line = ln;
                }
                return TPBE_CLI_FAIL;
            }
        } else {
            _sf_free_parts(parts, np);
            fclose(fp);
            if (error_line) {
                *error_line = ln;
            }
            return TPBE_CLI_FAIL;
        }
        _sf_free_parts(parts, np);
    }
    fclose(fp);
    return TPBE_SUCCESS;
}

int
tpbcli_rtenv_resolve_active_id_cli(const char *workspace, int32_t *id_out)
{
    const char *env_id;
    int32_t base = 1;
    int err;

    if (workspace == NULL || id_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    env_id = getenv("TPB_RTENV_ID");
    if (env_id != NULL && env_id[0] != '\0') {
        *id_out = (int32_t)strtol(env_id, NULL, 10);
        return TPBE_SUCCESS;
    }
    err = tpb_raf_config_get_base_id(workspace, &base);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    *id_out = base;
    return TPBE_SUCCESS;
}

static int
_sf_merge_record_delta(const char *workspace, int32_t id,
                       tpbcli_rtenv_merged_t *out)
{
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *hdrs = NULL;
    void *data = NULL;
    uint64_t dsize = 0;
    int err;
    int i;

    err = tpb_raf_record_read_rtenv(workspace, id, &attr, &hdrs, &data, &dsize);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    for (i = 0; i < (int)attr.nheader; i++) {
        if (strcmp(hdrs[i].name, "application") == 0 && data != NULL &&
            attr.napp > 0) {
            const char *base = (const char *)data +
                               _sf_hdr_payload_off(hdrs, (int)attr.nheader, i);
            int a;

            for (a = 0; a < (int)attr.napp; a++) {
                if (_sf_merge_add_app(out, base + a * 3 * 64,
                                      base + a * 3 * 64 + 64,
                                      base + a * 3 * 64 + 128) != 0) {
                    err = TPBE_CLI_FAIL;
                    goto done;
                }
            }
        }
    }
    for (i = 0; i < (int)attr.nheader; i++) {
        if (strncmp(hdrs[i].name, "key[", 4) == 0 && data != NULL) {
            int idx = atoi(hdrs[i].name + 4);
            char vname[32];
            char osname[32];
            char ogname[32];
            const char *k;
            const char *val = "";
            uint32_t on_set = TPB_RTENV_ON_SET_IGNORE;
            uint32_t on_get = TPB_RTENV_ON_GET_IGNORE;
            int j;

            snprintf(vname, sizeof(vname), "value[%d]", idx);
            snprintf(osname, sizeof(osname), "on_set[%d]", idx);
            snprintf(ogname, sizeof(ogname), "on_get[%d]", idx);
            k = (const char *)data +
                _sf_hdr_payload_off(hdrs, (int)attr.nheader, i);
            for (j = 0; j < (int)attr.nheader; j++) {
                if (strcmp(hdrs[j].name, vname) == 0) {
                    val = (const char *)data +
                          _sf_hdr_payload_off(hdrs, (int)attr.nheader, j);
                } else if (strcmp(hdrs[j].name, osname) == 0 &&
                           hdrs[j].data_size >= sizeof(uint32_t)) {
                    on_set = *(const uint32_t *)((const char *)data +
                        _sf_hdr_payload_off(hdrs, (int)attr.nheader, j));
                } else if (strcmp(hdrs[j].name, ogname) == 0 &&
                           hdrs[j].data_size >= sizeof(uint32_t)) {
                    on_get = *(const uint32_t *)((const char *)data +
                        _sf_hdr_payload_off(hdrs, (int)attr.nheader, j));
                }
            }
            if (_sf_merge_add_var(out, k, val, on_set, on_get) != 0) {
                err = TPBE_CLI_FAIL;
                goto done;
            }
        }
    }
done:
    tpb_raf_free_headers(hdrs, attr.nheader);
    free(data);
    return err;
}

int
tpbcli_rtenv_merge_chain(const char *workspace, int32_t target_id,
                         tpbcli_rtenv_merged_t *out)
{
    int32_t chain[64];
    int depth = 0;
    int32_t cur = target_id;
    int i;

    if (workspace == NULL || out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    memset(out, 0, sizeof(*out));
    while (depth < 64) {
        tpb_raf_rtenv_entry_t *ents = NULL;
        int n = 0;
        int j;
        int32_t parent = -1;

        chain[depth++] = cur;
        if (tpb_raf_entry_list_rtenv(workspace, &ents, &n) != TPBE_SUCCESS) {
            return TPBE_FILE_IO_FAIL;
        }
        for (j = 0; j < n; j++) {
            if (ents[j].id == cur) {
                parent = ents[j].inherit_from;
                break;
            }
        }
        free(ents);
        if (parent <= 0) {
            break;
        }
        for (j = 0; j < depth - 1; j++) {
            if (chain[j] == parent) {
                return TPBE_CLI_FAIL;
            }
        }
        cur = parent;
    }
    for (i = 0; i < depth; i++) {
        int err = _sf_merge_record_delta(workspace, chain[depth - 1 - i], out);
        if (err != TPBE_SUCCESS) {
            return err;
        }
    }
    return TPBE_SUCCESS;
}

int
tpbcli_rtenv_write_record(const char *workspace, int32_t id,
                          const char *name, const char *note,
                          int32_t inherit_from,
                          const tpbcli_rtenv_spec_t *spec)
{
    tpb_raf_rtenv_entry_t ent;
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *hdrs = NULL;
    unsigned char payload[65536];
    size_t off = 0;
    int i;
    int err;

    memset(&ent, 0, sizeof(ent));
    ent.id = id;
    snprintf(ent.name, sizeof(ent.name), "%s", name);
    {
        tpb_datetime_t dt;

        if (gethostname(ent.hostname, sizeof(ent.hostname)) != 0) {
            snprintf(ent.hostname, sizeof(ent.hostname), "localhost");
        }
        if (tpb_ts_get_datetime(TPBM_TS_UTC, &dt) != TPBE_SUCCESS ||
            tpb_ts_datetime_to_bits(&dt, 0, &ent.utc_bits) != TPBE_SUCCESS) {
            ent.utc_bits = 0;
        }
    }
    ent.inherit_from = inherit_from;
    ent.derive_to = -1;
    snprintf(ent.note, sizeof(ent.note), "%s", note);
    ent.napp = (uint32_t)spec->napp;
    ent.nenv = (uint32_t)spec->nenv;

    err = tpb_raf_entry_append_rtenv(workspace, &ent);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    memset(&attr, 0, sizeof(attr));
    attr.id = id;
    snprintf(attr.name, sizeof(attr.name), "%s", name);
    snprintf(attr.hostname, sizeof(attr.hostname), "%s", ent.hostname);
    attr.utc_bits = ent.utc_bits;
    attr.inherit_from = inherit_from;
    attr.napp = ent.napp;
    attr.nenv = ent.nenv;
    attr.nheader = (uint32_t)(1 + spec->nenv * 4);

    hdrs = (tpb_meta_header_t *)calloc(attr.nheader, sizeof(*hdrs));
    if (hdrs == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    memset(hdrs, 0, attr.nheader * sizeof(*hdrs));
    hdrs[0].ndim = 3;
    hdrs[0].dimsizes[0] = 64;
    hdrs[0].dimsizes[1] = 3;
    hdrs[0].dimsizes[2] = (uint64_t)spec->napp;
    hdrs[0].data_size = (uint64_t)(spec->napp * 3 * 64);
    hdrs[0].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    hdrs[0].block_size = (uint32_t)sizeof(tpb_meta_header_t);
    snprintf(hdrs[0].name, TPBM_NAME_STR_MAX_LEN, "application");
    memset(payload + off, 0, (size_t)hdrs[0].data_size);
    for (i = 0; i < spec->napp; i++) {
        snprintf((char *)payload + off + (size_t)(i * 3 * 64), 64, "%s",
                 spec->apps[i].name);
        snprintf((char *)payload + off + (size_t)(i * 3 * 64 + 64), 64, "%s",
                 spec->apps[i].version);
        snprintf((char *)payload + off + (size_t)(i * 3 * 64 + 128), 64, "%s",
                 spec->apps[i].note);
    }
    off += (size_t)hdrs[0].data_size;

    for (i = 0; i < spec->nenv; i++) {
        char kname[32];
        char vname[32];
        char osname[32];
        char ogname[32];
        size_t klen = strlen(spec->vars[i].key) + 1;
        size_t vlen = strlen(spec->vars[i].value) + 1;
        int base = 1 + i * 4;

        snprintf(kname, sizeof(kname), "key[%d]", i);
        snprintf(vname, sizeof(vname), "value[%d]", i);
        snprintf(osname, sizeof(osname), "on_set[%d]", i);
        snprintf(ogname, sizeof(ogname), "on_get[%d]", i);
        hdrs[base].ndim = 1;
        hdrs[base].dimsizes[0] = klen;
        hdrs[base].data_size = klen;
        hdrs[base].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
        hdrs[base].block_size = (uint32_t)sizeof(tpb_meta_header_t);
        snprintf(hdrs[base].name, TPBM_NAME_STR_MAX_LEN, "%s", kname);
        memcpy(payload + off, spec->vars[i].key, klen);
        off += klen;

        hdrs[base + 1].ndim = 1;
        hdrs[base + 1].dimsizes[0] = vlen;
        hdrs[base + 1].data_size = vlen;
        hdrs[base + 1].type_bits =
            (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
        hdrs[base + 1].block_size = (uint32_t)sizeof(tpb_meta_header_t);
        snprintf(hdrs[base + 1].name, TPBM_NAME_STR_MAX_LEN, "%s", vname);
        memcpy(payload + off, spec->vars[i].value, vlen);
        off += vlen;

        hdrs[base + 2].ndim = 1;
        hdrs[base + 2].dimsizes[0] = sizeof(uint32_t);
        hdrs[base + 2].data_size = sizeof(uint32_t);
        hdrs[base + 2].type_bits =
            (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
        hdrs[base + 2].block_size = (uint32_t)sizeof(tpb_meta_header_t);
        snprintf(hdrs[base + 2].name, TPBM_NAME_STR_MAX_LEN, "%s", osname);
        memcpy(payload + off, &spec->vars[i].on_set, sizeof(uint32_t));
        off += sizeof(uint32_t);

        hdrs[base + 3].ndim = 1;
        hdrs[base + 3].dimsizes[0] = sizeof(uint32_t);
        hdrs[base + 3].data_size = sizeof(uint32_t);
        hdrs[base + 3].type_bits =
            (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
        hdrs[base + 3].block_size = (uint32_t)sizeof(tpb_meta_header_t);
        snprintf(hdrs[base + 3].name, TPBM_NAME_STR_MAX_LEN, "%s", ogname);
        memcpy(payload + off, &spec->vars[i].on_get, sizeof(uint32_t));
        off += sizeof(uint32_t);
    }

    err = tpb_raf_record_write_rtenv(workspace, &attr, hdrs, payload,
                                     (uint64_t)off);
    free(hdrs);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    if (inherit_from > 0) {
        (void)tpb_raf_record_append_rtenv_derive(workspace, inherit_from, id);
    }
    return TPBE_SUCCESS;
}

int
tpbcli_rtenv_id_exists(const char *workspace, int32_t id)
{
    return _sf_rtenv_id_exists(workspace, id);
}

int
tpbcli_rtenv_find_id_by_name(const char *workspace, const char *name,
                             int32_t *id_out)
{
    tpb_raf_rtenv_entry_t *ents = NULL;
    int n = 0;
    int i;

    if (workspace == NULL || name == NULL || id_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    if (tpb_raf_entry_list_rtenv(workspace, &ents, &n) != TPBE_SUCCESS) {
        return TPBE_FILE_IO_FAIL;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(ents[i].name, name) == 0) {
            *id_out = ents[i].id;
            free(ents);
            return TPBE_SUCCESS;
        }
    }
    free(ents);
    return TPBE_LIST_NOT_FOUND;
}

int
tpbcli_rtenv_resolve_list_active(const char *workspace, int32_t *id_out,
                                 int *found_out)
{
    const char *env_id;
    int32_t base = 1;

    if (workspace == NULL || found_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    *found_out = 0;
    if (id_out != NULL) {
        *id_out = -1;
    }
    env_id = getenv("TPB_RTENV_ID");
    if (env_id != NULL && env_id[0] != '\0') {
        int32_t id = (int32_t)strtol(env_id, NULL, 10);
        if (_sf_rtenv_id_exists(workspace, id)) {
            if (id_out != NULL) {
                *id_out = id;
            }
            *found_out = 1;
        }
        return TPBE_SUCCESS;
    }
    if (tpb_raf_config_get_base_id(workspace, &base) != TPBE_SUCCESS) {
        base = 1;
    }
    if (_sf_rtenv_id_exists(workspace, base)) {
        if (id_out != NULL) {
            *id_out = base;
        }
        *found_out = 1;
    }
    return TPBE_SUCCESS;
}
