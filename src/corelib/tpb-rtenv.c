/*
 * tpb-rtenv.c
 * Runtime environment activation helpers (resolve active ID, ensure base env).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/tpb-public.h"
#include "tpb-types.h"

#define TPB_RTENV_ID_ENV  "TPB_RTENV_ID"

/* Local Function Prototypes */
static int _sf_rtenv_id_exists(const char *workspace, int32_t id);

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

/**
 * @brief Create minimal base RTEnv (id=0) when the domain is empty.
 */
int
tpb_rtenv_ensure_base_env(const char *workspace, int32_t *id_out)
{
    tpb_raf_rtenv_entry_t *entries = NULL;
    tpb_raf_rtenv_entry_t ent;
    tpb_raf_rtenv_attr_t attr;
    int count = 0;
    int err;

    if (!workspace || !id_out) {
        TPB_FAIL(TPB_MOD_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }
    if (count > 0) {
        free(entries);
        *id_out = 0;
        return TPBE_SUCCESS;
    }
    free(entries);

    memset(&ent, 0, sizeof(ent));
    ent.id = 0;
    snprintf(ent.name, sizeof(ent.name), "base");
    snprintf(ent.hostname, sizeof(ent.hostname), "localhost");
    ent.inherit_from = -1;
    ent.derive_to = -1;
    snprintf(ent.note, sizeof(ent.note), "auto base env");

    err = tpb_raf_entry_append_rtenv(workspace, &ent);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    memset(&attr, 0, sizeof(attr));
    attr.id = 0;
    snprintf(attr.name, sizeof(attr.name), "base");
    snprintf(attr.hostname, sizeof(attr.hostname), "localhost");
    attr.inherit_from = -1;
    attr.derive_to = -1;
    snprintf(attr.note, sizeof(attr.note), "auto base env");
    attr.nheader = 0;

    err = tpb_raf_record_write_rtenv(workspace, &attr, NULL, NULL, 0);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_MISC, err, NULL);
    }

    (void)tpb_raf_config_set_base_id(workspace, 0);
    *id_out = 0;
    return TPBE_SUCCESS;
}

/**
 * @brief Resolve active RTEnv ID from $TPB_RTENV_ID, config base_id, or lazy base.
 */
int
tpb_rtenv_resolve_active_id(const char *workspace, int32_t *id_out)
{
    const char *env_id;
    int32_t base_id = 0;
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
