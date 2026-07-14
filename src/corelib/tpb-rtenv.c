/*
 * tpb-rtenv.c
 * Runtime environment activation helpers (resolve active ID, build snapshots).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/tpb-public.h"
#include "tpb-types.h"
#include "tpblog/tpb-log.h"
#include "rafdb/rafdb-l1-internal.h"

#define TPB_RTENV_ID_ENV       "TPB_RTENV_ID"
#define TPB_RTENV_ROOT_INHERIT 0
#define TPB_RTENV_BUILD_NOTE   "build-time environment snapshot"

extern char **environ;

/* Local Function Prototypes */
static int _sf_count_environ(int *nenv_out, size_t *payload_out);
static int _sf_fill_utc_bits(tpb_dtbits_t *utc_out);
static int _sf_name_exists(const char *workspace, const char *name);
static int _sf_pick_build_env_name(const char *workspace, char *name_out,
                                   size_t name_len);
static int _sf_rtenv_id_exists(const char *workspace, int32_t id);
static int _sf_write_env_snapshot_record(const char *workspace, int32_t id,
                                         const char *name,
                                         const char *hostname,
                                         tpb_dtbits_t utc_bits);

static int
_sf_rtenv_id_exists(const char *workspace, int32_t id)
{
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int i;
    int err;

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (entries[i].id == id) {
            free(entries);
            return 1;
        }
    }
    free(entries);
    return 0;
}

static int
_sf_name_exists(const char *workspace, const char *name)
{
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int i;
    int found = 0;
    int err;

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            found = 1;
            break;
        }
    }
    free(entries);
    return found;
}

static int
_sf_count_environ(int *nenv_out, size_t *payload_out)
{
    int nenv = 0;
    size_t payload = 0;
    char **e;

    if (nenv_out == NULL || payload_out == NULL) {
        return -1;
    }
    if (environ == NULL) {
        *nenv_out = 0;
        *payload_out = 0;
        return 0;
    }
    for (e = environ; *e != NULL; e++) {
        const char *eq = strchr(*e, '=');

        if (eq == NULL || eq == *e) {
            continue;
        }
        nenv++;
        payload += (size_t)(eq - *e) + 1;
        payload += strlen(eq + 1) + 1;
        payload += sizeof(uint32_t) * 2;
    }
    *nenv_out = nenv;
    *payload_out = payload;
    return 0;
}

static int
_sf_fill_utc_bits(tpb_dtbits_t *utc_out)
{
    tpb_datetime_t dt;
    int err;

    if (utc_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    err = tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    return tpb_ts_datetime_to_bits(&dt, 0, utc_out);
}

static int
_sf_pick_build_env_name(const char *workspace, char *name_out, size_t name_len)
{
    int n;

    if (workspace == NULL || name_out == NULL || name_len == 0) {
        return TPBE_NULLPTR_ARG;
    }
    if (!_sf_name_exists(workspace, "base")) {
        snprintf(name_out, name_len, "base");
        return TPBE_SUCCESS;
    }
    for (n = 1; n < 100000; n++) {
        char candidate[256];

        snprintf(candidate, sizeof(candidate), "base_%d", n);
        if (!_sf_name_exists(workspace, candidate)) {
            snprintf(name_out, name_len, "%s", candidate);
            return TPBE_SUCCESS;
        }
    }
    TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL, "no unique build env name");
}

static int
_sf_write_env_snapshot_record(const char *workspace, int32_t id,
                              const char *name, const char *hostname,
                              tpb_dtbits_t utc_bits)
{
    tpb_raf_rtenv_entry_t ent;
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *hdrs = NULL;
    unsigned char *payload = NULL;
    size_t off = 0;
    int nenv = 0;
    size_t payload_cap = 0;
    int i;
    int err;
    char **e;

    if (_sf_count_environ(&nenv, &payload_cap) != 0) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL, NULL);
    }

    memset(&ent, 0, sizeof(ent));
    ent.id = id;
    snprintf(ent.name, sizeof(ent.name), "%s", name);
    snprintf(ent.hostname, sizeof(ent.hostname), "%s", hostname);
    ent.utc_bits = utc_bits;
    ent.inherit_from = TPB_RTENV_ROOT_INHERIT;
    ent.derive_to = -1;
    snprintf(ent.note, sizeof(ent.note), "%s", TPB_RTENV_BUILD_NOTE);
    ent.napp = 0;
    ent.nenv = (uint32_t)nenv;

    err = tpb_raf_entry_append_rtenv(workspace, &ent);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    memset(&attr, 0, sizeof(attr));
    attr.id = id;
    snprintf(attr.name, sizeof(attr.name), "%s", name);
    snprintf(attr.hostname, sizeof(attr.hostname), "%s", hostname);
    attr.utc_bits = utc_bits;
    attr.inherit_from = TPB_RTENV_ROOT_INHERIT;
    attr.derive_to = -1;
    snprintf(attr.note, sizeof(attr.note), "%s", TPB_RTENV_BUILD_NOTE);
    attr.napp = 0;
    attr.nenv = (uint32_t)nenv;
    attr.nheader = (uint32_t)(1 + nenv * 4);

    hdrs = (tpb_meta_header_t *)calloc(attr.nheader, sizeof(*hdrs));
    if (hdrs == NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    if (payload_cap > 0) {
        payload = (unsigned char *)malloc(payload_cap);
        if (payload == NULL) {
            free(hdrs);
            TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
        }
    }

    memset(hdrs, 0, attr.nheader * sizeof(*hdrs));
    hdrs[0].ndim = 3;
    hdrs[0].dimsizes[0] = 64;
    hdrs[0].dimsizes[1] = 3;
    hdrs[0].dimsizes[2] = 0;
    hdrs[0].data_size = 0;
    hdrs[0].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    hdrs[0].block_size = (uint32_t)sizeof(tpb_meta_header_t);
    snprintf(hdrs[0].name, TPBM_NAME_STR_MAX_LEN, "application");

    i = 0;
    if (environ != NULL) {
        for (e = environ; *e != NULL; e++) {
            const char *eq = strchr(*e, '=');
            char kname[32];
            char vname[32];
            char osname[32];
            char ogname[32];
            size_t klen;
            size_t vlen;
            uint32_t on_set = TPB_RTENV_ON_SET_IGNORE;
            uint32_t on_get = TPB_RTENV_ON_GET_IGNORE;
            int base;

            if (eq == NULL || eq == *e) {
                continue;
            }
            klen = (size_t)(eq - *e) + 1;
            vlen = strlen(eq + 1) + 1;
            base = 1 + i * 4;

            snprintf(kname, sizeof(kname), "key[%d]", i);
            snprintf(vname, sizeof(vname), "value[%d]", i);
            snprintf(osname, sizeof(osname), "on_set[%d]", i);
            snprintf(ogname, sizeof(ogname), "on_get[%d]", i);

            hdrs[base].ndim = 1;
            hdrs[base].dimsizes[0] = klen;
            hdrs[base].data_size = klen;
            hdrs[base].type_bits =
                (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
            hdrs[base].block_size = (uint32_t)sizeof(tpb_meta_header_t);
            snprintf(hdrs[base].name, TPBM_NAME_STR_MAX_LEN, "%s", kname);
            memcpy(payload + off, *e, klen - 1);
            payload[off + klen - 1] = '\0';
            off += klen;

            hdrs[base + 1].ndim = 1;
            hdrs[base + 1].dimsizes[0] = vlen;
            hdrs[base + 1].data_size = vlen;
            hdrs[base + 1].type_bits =
                (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
            hdrs[base + 1].block_size = (uint32_t)sizeof(tpb_meta_header_t);
            snprintf(hdrs[base + 1].name, TPBM_NAME_STR_MAX_LEN, "%s", vname);
            memcpy(payload + off, eq + 1, vlen - 1);
            payload[off + vlen - 1] = '\0';
            off += vlen;

            hdrs[base + 2].ndim = 1;
            hdrs[base + 2].dimsizes[0] = sizeof(uint32_t);
            hdrs[base + 2].data_size = sizeof(uint32_t);
            hdrs[base + 2].type_bits =
                (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
            hdrs[base + 2].block_size = (uint32_t)sizeof(tpb_meta_header_t);
            snprintf(hdrs[base + 2].name, TPBM_NAME_STR_MAX_LEN, "%s", osname);
            memcpy(payload + off, &on_set, sizeof(uint32_t));
            off += sizeof(uint32_t);

            hdrs[base + 3].ndim = 1;
            hdrs[base + 3].dimsizes[0] = sizeof(uint32_t);
            hdrs[base + 3].data_size = sizeof(uint32_t);
            hdrs[base + 3].type_bits =
                (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
            hdrs[base + 3].block_size = (uint32_t)sizeof(tpb_meta_header_t);
            snprintf(hdrs[base + 3].name, TPBM_NAME_STR_MAX_LEN, "%s", ogname);
            memcpy(payload + off, &on_get, sizeof(uint32_t));
            off += sizeof(uint32_t);
            i++;
        }
    }

    err = tpb_raf_record_write_rtenv(workspace, &attr, hdrs, payload,
                                     (uint64_t)off);
    free(hdrs);
    free(payload);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }
    return TPBE_SUCCESS;
}

/**
 * @brief Record build-time environment snapshot as a root RTEnv entry.
 */
int
tpb_rtenv_snapshot_build_env(const char *workspace, int always_new,
                             int32_t *id_out)
{
    tpb_raf_rtenv_entry_t *entries = NULL;
    char name[TPB_RAF_RTENV_NAME_LEN];
    char hostname[TPB_RAF_RTENV_HOST_LEN];
    tpb_dtbits_t utc_bits = 0;
    int32_t id = 0;
    int32_t base_id = 1;
    int count = 0;
    int err;

    if (!workspace || !id_out) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }
    free(entries);

    if (!always_new && count > 0) {
        err = tpb_raf_config_get_base_id(workspace, &base_id);
        if (err != TPBE_SUCCESS) {
            TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
        }
        if (_sf_rtenv_id_exists(workspace, base_id)) {
            *id_out = base_id;
            return TPBE_SUCCESS;
        }
        TPB_FAIL(TPB_MOD_MISC, TPBE_LIST_NOT_FOUND, "base_id RTEnv missing");
    }

    err = _sf_pick_build_env_name(workspace, name, sizeof(name));
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    err = tpb_raf_rtenv_alloc_next_id(workspace, &id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "localhost");
    }

    err = _sf_fill_utc_bits(&utc_bits);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    err = _sf_write_env_snapshot_record(workspace, id, name, hostname,
                                        utc_bits);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    err = tpb_raf_config_set_base_id(workspace, id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    *id_out = id;
    return TPBE_SUCCESS;
}

/**
 * @brief Ensure a base RTEnv exists (lazy snapshot when domain is empty).
 */
int
tpb_rtenv_ensure_base_env(const char *workspace, int32_t *id_out)
{
    if (!workspace || !id_out) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    return tpb_rtenv_snapshot_build_env(workspace, 0, id_out);
}

/**
 * @brief Resolve active RTEnv ID from $TPB_RTENV_ID, config base_id, or lazy base.
 */
int
tpb_rtenv_resolve_active_id(const char *workspace, int32_t *id_out)
{
    const char *env_id;
    int32_t base_id = 1;
    int32_t parsed = 0;
    int err;

    if (!workspace || !id_out) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    env_id = getenv(TPB_RTENV_ID_ENV);
    if (env_id != NULL && env_id[0] != '\0') {
        parsed = (int32_t)strtol(env_id, NULL, 10);
        if (_sf_rtenv_id_exists(workspace, parsed)) {
            *id_out = parsed;
            return TPBE_SUCCESS;
        }
    }

    err = tpb_raf_config_get_base_id(workspace, &base_id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }
    if (_sf_rtenv_id_exists(workspace, base_id)) {
        *id_out = base_id;
        return TPBE_SUCCESS;
    }

    err = tpb_rtenv_ensure_base_env(workspace, id_out);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }
    return TPBE_SUCCESS;
}

/* ===== Merged chain, on_set/on_get, task environ snapshot ===== */

typedef struct tpb_rtenv_env_snapshot {
    char    *key_blob;
    int32_t *counts;
    char    *value_blob;
    int      nkeys;
    size_t   key_cap;
    size_t   key_len;
    size_t   count_cap;
    size_t   value_cap;
    size_t   value_len;
} tpb_rtenv_env_snapshot_t;

static tpb_rtenv_env_snapshot_t s_env_snap;

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
_sf_merge_add_app(tpb_rtenv_merged_t *out, const char *name,
                  const char *version, const char *note)
{
    int i;

    for (i = 0; i < out->napp; i++) {
        if (strcmp(out->apps_name[i], name) == 0) {
            snprintf(out->apps_version[i], sizeof(out->apps_version[0]),
                     "%s", version);
            snprintf(out->apps_note[i], sizeof(out->apps_note[0]), "%s", note);
            return TPBE_SUCCESS;
        }
    }
    if (out->napp >= 32) {
        return TPBE_CLI_FAIL;
    }
    snprintf(out->apps_name[out->napp], sizeof(out->apps_name[0]), "%s", name);
    snprintf(out->apps_version[out->napp], sizeof(out->apps_version[0]),
             "%s", version);
    snprintf(out->apps_note[out->napp], sizeof(out->apps_note[0]), "%s", note);
    out->napp++;
    return TPBE_SUCCESS;
}

static int
_sf_merge_add_var(tpb_rtenv_merged_t *out, const char *key,
                  const char *value, uint32_t on_set, uint32_t on_get)
{
    int i;

    for (i = 0; i < out->nenv; i++) {
        if (strcmp(out->vars[i].key, key) == 0) {
            snprintf(out->vars[i].value, sizeof(out->vars[0].value), "%s",
                     value ? value : "");
            out->vars[i].on_set = on_set;
            out->vars[i].on_get = on_get;
            return TPBE_SUCCESS;
        }
    }
    if (out->nenv >= TPB_RTENV_MERGED_MAX_VAR) {
        return TPBE_CLI_FAIL;
    }
    snprintf(out->vars[out->nenv].key, sizeof(out->vars[0].key), "%s", key);
    snprintf(out->vars[out->nenv].value, sizeof(out->vars[0].value), "%s",
             value ? value : "");
    out->vars[out->nenv].on_set = on_set;
    out->vars[out->nenv].on_get = on_get;
    out->nenv++;
    return TPBE_SUCCESS;
}

static int
_sf_merge_record_delta(const char *workspace, int32_t id,
                       tpb_rtenv_merged_t *out)
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
                err = _sf_merge_add_app(out, base + a * 3 * 64,
                                        base + a * 3 * 64 + 64,
                                        base + a * 3 * 64 + 128);
                if (err != TPBE_SUCCESS) {
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
            err = _sf_merge_add_var(out, k, val, on_set, on_get);
            if (err != TPBE_SUCCESS) {
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
tpb_rtenv_merge_chain(const char *workspace, int32_t target_id,
                      tpb_rtenv_merged_t *out)
{
    int32_t chain[64];
    int depth = 0;
    int32_t cur = target_id;
    int i;

    if (workspace == NULL || out == NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    memset(out, 0, sizeof(*out));
    while (depth < 64) {
        tpb_raf_rtenv_entry_t *ents = NULL;
        int n = 0;
        int j;
        int32_t parent = -1;

        chain[depth++] = cur;
        if (tpb_raf_entry_list_rtenv(workspace, &ents, &n) != TPBE_SUCCESS) {
            TPB_FAIL(TPB_MOD_MISC, TPBE_FILE_IO_FAIL, NULL);
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
                TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL, "RTEnv inherit cycle");
            }
        }
        cur = parent;
    }
    for (i = 0; i < depth; i++) {
        int err = _sf_merge_record_delta(workspace, chain[depth - 1 - i], out);
        if (err != TPBE_SUCCESS) {
            TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
        }
    }
    return TPBE_SUCCESS;
}

const tpb_rtenv_merged_var_t *
tpb_rtenv_merged_find(const tpb_rtenv_merged_t *merged, const char *key)
{
    int i;

    if (merged == NULL || key == NULL) {
        return NULL;
    }
    for (i = 0; i < merged->nenv; i++) {
        if (strcmp(merged->vars[i].key, key) == 0) {
            return &merged->vars[i];
        }
    }
    return NULL;
}

int
tpb_rtenv_apply_onset(const char *key, const char *record_val,
                      uint32_t on_set, char *out, size_t outlen)
{
    const char *cur;
    size_t n;

    if (key == NULL || out == NULL || outlen == 0) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (record_val == NULL) {
        record_val = "";
    }
    switch (on_set) {
    case TPB_RTENV_ON_SET_IGNORE:
        return TPBE_CLI_FAIL;
    case TPB_RTENV_ON_SET_OVERWRITE:
        snprintf(out, outlen, "%s", record_val);
        return TPBE_SUCCESS;
    case TPB_RTENV_ON_SET_PREPEND:
        cur = getenv(key);
        if (cur != NULL && cur[0] != '\0') {
            snprintf(out, outlen, "%s:%s", record_val, cur);
        } else {
            snprintf(out, outlen, "%s", record_val);
        }
        return TPBE_SUCCESS;
    case TPB_RTENV_ON_SET_APPEND:
        cur = getenv(key);
        if (cur != NULL && cur[0] != '\0') {
            n = strlen(cur);
            if (n > 0 && cur[n - 1] == ':') {
                snprintf(out, outlen, "%s%s", cur, record_val);
            } else {
                snprintf(out, outlen, "%s:%s", cur, record_val);
            }
        } else {
            snprintf(out, outlen, "%s", record_val);
        }
        return TPBE_SUCCESS;
    default:
        TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL, "invalid on_set");
    }
}

static int
_sf_count_colon_segments(const char *val)
{
    const char *p;
    int nseg = 0;
    int in_seg = 0;

    if (val == NULL || val[0] == '\0') {
        return 0;
    }
    for (p = val; *p != '\0'; p++) {
        if (*p == ':') {
            if (in_seg) {
                nseg++;
                in_seg = 0;
            }
        } else {
            in_seg = 1;
        }
    }
    if (in_seg) {
        nseg++;
    }
    return nseg;
}

static int
_sf_append_segment_to_blob(char **blob, size_t *cap, size_t *len,
                           const char *seg, size_t seglen)
{
    char *nb;
    size_t need;
    size_t add = seglen;

    if (seg == NULL) {
        return TPBE_SUCCESS;
    }
    if (memchr(seg, ':', seglen) != NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL,
                 "environment value segment must not contain ':'");
    }
    need = *len + add + (*len > 0 ? 1 : 0) + 1;
    if (need > *cap) {
        size_t nc = (*cap == 0) ? 256 : *cap * 2;
        while (nc < need) {
            nc *= 2;
        }
        nb = (char *)realloc(*blob, nc);
        if (nb == NULL) {
            TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        *blob = nb;
        *cap = nc;
    }
    if (*len > 0) {
        (*blob)[(*len)++] = ':';
    }
    memcpy(*blob + *len, seg, seglen);
    *len += seglen;
    (*blob)[*len] = '\0';
    return TPBE_SUCCESS;
}

static int
_sf_snapshot_append_key(const char *key, const char *val)
{
    int nseg;
    const char *p;
    const char *start;
    int err;
    int32_t *nc;
    size_t kadd;

    if (strchr(key, ':') != NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL,
                 "environment variable key must not contain ':'");
    }
    kadd = strlen(key) + (s_env_snap.key_len > 0 ? 1 : 0) + 1;
    if (s_env_snap.key_len + kadd > s_env_snap.key_cap) {
        size_t nc_cap = (s_env_snap.key_cap == 0) ? 256 : s_env_snap.key_cap * 2;
        char *nb;

        while (nc_cap < s_env_snap.key_len + kadd) {
            nc_cap *= 2;
        }
        nb = (char *)realloc(s_env_snap.key_blob, nc_cap);
        if (nb == NULL) {
            TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        s_env_snap.key_blob = nb;
        s_env_snap.key_cap = nc_cap;
    }
    if (s_env_snap.key_len > 0) {
        s_env_snap.key_blob[s_env_snap.key_len++] = ':';
    }
    memcpy(s_env_snap.key_blob + s_env_snap.key_len, key, strlen(key));
    s_env_snap.key_len += strlen(key);
    s_env_snap.key_blob[s_env_snap.key_len] = '\0';

    nseg = _sf_count_colon_segments(val);
    if ((size_t)(s_env_snap.nkeys + 1) > s_env_snap.count_cap) {
        size_t nc_cap = (s_env_snap.count_cap == 0) ? 32 : s_env_snap.count_cap * 2;
        nc = (int32_t *)realloc(s_env_snap.counts, nc_cap * sizeof(int32_t));
        if (nc == NULL) {
            TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        s_env_snap.counts = nc;
        s_env_snap.count_cap = nc_cap;
    }
    s_env_snap.counts[s_env_snap.nkeys] = (int32_t)nseg;
    s_env_snap.nkeys++;

    if (val == NULL || val[0] == '\0') {
        return TPBE_SUCCESS;
    }
    start = val;
    for (p = val; ; p++) {
        if (*p == ':' || *p == '\0') {
            if (p > start) {
                err = _sf_append_segment_to_blob(&s_env_snap.value_blob,
                                                 &s_env_snap.value_cap,
                                                 &s_env_snap.value_len,
                                                 start, (size_t)(p - start));
                if (err != TPBE_SUCCESS) {
                    return err;
                }
            }
            if (*p == '\0') {
                break;
            }
            start = p + 1;
        }
    }
    return TPBE_SUCCESS;
}

void
tpb_rtenv_clear_environ_snapshot(void)
{
    free(s_env_snap.key_blob);
    free(s_env_snap.counts);
    free(s_env_snap.value_blob);
    memset(&s_env_snap, 0, sizeof(s_env_snap));
}

int
tpb_rtenv_capture_environ_snapshot(const char *workspace)
{
    tpb_rtenv_merged_t merged;
    char **e;
    char valbuf[65536];
    int32_t active_id;
    int err;

    if (workspace == NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    tpb_rtenv_clear_environ_snapshot();

    err = tpb_rtenv_resolve_active_id(workspace, &active_id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }
    err = tpb_rtenv_merge_chain(workspace, active_id, &merged);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    if (environ == NULL) {
        return TPBE_SUCCESS;
    }
    for (e = environ; *e != NULL; e++) {
        const char *eq = strchr(*e, '=');
        const char *actual;
        const tpb_rtenv_merged_var_t *decl;
        char key[256];
        size_t klen;

        if (eq == NULL || eq == *e) {
            continue;
        }
        klen = (size_t)(eq - *e);
        if (klen >= sizeof(key)) {
            klen = sizeof(key) - 1;
        }
        memcpy(key, *e, klen);
        key[klen] = '\0';

        actual = eq + 1;
        snprintf(valbuf, sizeof(valbuf), "%s", actual);

        decl = tpb_rtenv_merged_find(&merged, key);
        if (decl != NULL) {
            switch (decl->on_get) {
            case TPB_RTENV_ON_GET_IGNORE:
                continue;
            case TPB_RTENV_ON_GET_WARN:
                if (strcmp(actual, decl->value) != 0) {
                    tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                               TPBLOG_FLAG_TSTAG,
                               "RTEnv: '%s' differs from declared value\n",
                               key);
                }
                break;
            case TPB_RTENV_ON_GET_FAIL:
                if (strcmp(actual, decl->value) != 0) {
                    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                               TPBLOG_FLAG_DIRECT,
                               "Unmapped environment variables detected.\n");
                    TPB_FAIL(TPB_MOD_MISC, TPBE_CLI_FAIL, key);
                }
                break;
            case TPB_RTENV_ON_GET_OVERWRITE:
                break;
            default:
                break;
            }
        }

        err = _sf_snapshot_append_key(key, valbuf);
        if (err != TPBE_SUCCESS) {
            tpb_rtenv_clear_environ_snapshot();
            TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
        }
    }
    return TPBE_SUCCESS;
}

int
tpb_rtenv_append_env_snapshot_headers(tpb_meta_header_t **headers,
                                      uint32_t *nheader,
                                      void **rec_data,
                                      uint64_t *rec_datasize)
{
    tpb_meta_header_t *hdrs;
    void *data;
    uint64_t dsize;
    uint32_t base;
    size_t kb_len;
    size_t vb_len;
    size_t cb_len;

    if (headers == NULL || nheader == NULL || rec_data == NULL ||
        rec_datasize == NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    kb_len = (s_env_snap.key_blob != NULL) ? strlen(s_env_snap.key_blob) + 1 : 1;
    vb_len = (s_env_snap.value_blob != NULL) ? strlen(s_env_snap.value_blob) + 1 : 1;
    cb_len = (size_t)((s_env_snap.nkeys > 0) ? (size_t)s_env_snap.nkeys : 1)
             * sizeof(int32_t);

    hdrs = (tpb_meta_header_t *)realloc(
        *headers, (size_t)(*nheader + 3) * sizeof(tpb_meta_header_t));
    if (hdrs == NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    *headers = hdrs;
    base = *nheader;
    dsize = *rec_datasize;
    data = realloc(*rec_data, (size_t)(dsize + kb_len + vb_len + cb_len));
    if (data == NULL) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    *rec_data = data;

    memset(&hdrs[base], 0, 3 * sizeof(tpb_meta_header_t));
    hdrs[base].block_size = TPB_RAF_HDR_FIXED_SIZE;
    hdrs[base].ndim = 1;
    hdrs[base].dimsizes[0] = kb_len;
    hdrs[base].data_size = kb_len;
    hdrs[base].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    snprintf(hdrs[base].name, sizeof(hdrs[base].name),
             "environment_variable_key");
    if (s_env_snap.key_blob != NULL && s_env_snap.key_len > 0) {
        memcpy((uint8_t *)data + dsize, s_env_snap.key_blob, kb_len - 1);
    }
    ((char *)data)[dsize + kb_len - 1] = '\0';
    dsize += kb_len;

    hdrs[base + 1].block_size = TPB_RAF_HDR_FIXED_SIZE;
    hdrs[base + 1].ndim = 1;
    hdrs[base + 1].dimsizes[0] = (uint64_t)s_env_snap.nkeys;
    hdrs[base + 1].data_size = cb_len;
    hdrs[base + 1].type_bits = (uint32_t)(TPB_INT32_T & TPB_PARM_TYPE_MASK);
    snprintf(hdrs[base + 1].name, sizeof(hdrs[base + 1].name),
             "environment_variable_count");
    if (s_env_snap.nkeys > 0 && s_env_snap.counts != NULL) {
        memcpy((uint8_t *)data + dsize, s_env_snap.counts, cb_len);
    } else {
        memset((uint8_t *)data + dsize, 0, cb_len);
    }
    dsize += cb_len;

    hdrs[base + 2].block_size = TPB_RAF_HDR_FIXED_SIZE;
    hdrs[base + 2].ndim = 1;
    hdrs[base + 2].dimsizes[0] = vb_len;
    hdrs[base + 2].data_size = vb_len;
    hdrs[base + 2].type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    snprintf(hdrs[base + 2].name, sizeof(hdrs[base + 2].name),
             "environment_variable_value");
    if (s_env_snap.value_blob != NULL && s_env_snap.value_len > 0) {
        memcpy((uint8_t *)data + dsize, s_env_snap.value_blob, vb_len - 1);
    }
    ((char *)data)[dsize + vb_len - 1] = '\0';
    dsize += vb_len;

    *nheader = base + 3;
    *rec_datasize = dsize;
    return TPBE_SUCCESS;
}
