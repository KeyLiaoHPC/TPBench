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
        payload += sizeof(uint32_t);
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
    attr.nheader = (uint32_t)(1 + nenv * 3);

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
            char mname[32];
            size_t klen;
            size_t vlen;
            uint32_t mode = 0;
            int base;

            if (eq == NULL || eq == *e) {
                continue;
            }
            klen = (size_t)(eq - *e) + 1;
            vlen = strlen(eq + 1) + 1;
            base = 1 + i * 3;

            snprintf(kname, sizeof(kname), "key[%d]", i);
            snprintf(vname, sizeof(vname), "value[%d]", i);
            snprintf(mname, sizeof(mname), "mode[%d]", i);

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
            snprintf(hdrs[base + 2].name, TPBM_NAME_STR_MAX_LEN, "%s", mname);
            memcpy(payload + off, &mode, sizeof(uint32_t));
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
