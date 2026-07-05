/*
 * rafdb-l3-kernel-kvmeta.c
 * Kernel metadata kv helpers and header payload access.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"
#include "tpb-raf-kernel-meta.h"

/**
 * @brief Return nonzero if TPB_K_OVERRIDE is set to a truthy value.
 */
int
tpb_raf_kernel_override_enabled(void)
{
    const char *v;

    v = getenv(TPB_K_OVERRIDE_ENV);
    if (v == NULL || v[0] == '\0') {
        return 0;
    }
    if (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0 ||
        strcasecmp(v, "no") == 0) {
        return 0;
    }
    return 1;
}

/**
 * @brief Find a header index by exact name in a kernel attr.
 */
int
tpb_raf_kernel_find_header(const kernel_attr_t *attr, const char *name)
{
    if (attr == NULL || name == NULL) {
        return -1;
    }
    return _tpb_raf_l1_find_header_by_name(attr->headers, attr->nheader,
                                           name);
}

/**
 * @brief Read a key from metadata kv payload; returns empty string if missing.
 */
int
tpb_raf_kernel_meta_kv_get(const char *payload, const char *key,
                           char *value_out, size_t value_len)
{
    const char *line;
    const char *end;
    size_t klen;
    char search[256];

    if (value_out == NULL || value_len == 0) {
        return TPBE_NULLPTR_ARG;
    }
    value_out[0] = '\0';
    if (payload == NULL || key == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    klen = strlen(key);
    if (klen + 2 >= sizeof(search)) {
        return TPBE_KERN_ARG_FAIL;
    }
    snprintf(search, sizeof(search), "%s=", key);

    line = payload;
    while (line != NULL && *line != '\0') {
        end = strchr(line, '\n');
        if (strncmp(line, search, strlen(search)) == 0) {
            const char *val = line + strlen(search);
            size_t vlen;

            if (end != NULL) {
                vlen = (size_t)(end - val);
            } else {
                vlen = strlen(val);
            }
            if (vlen >= value_len) {
                vlen = value_len - 1;
            }
            memcpy(value_out, val, vlen);
            value_out[vlen] = '\0';
            return TPBE_SUCCESS;
        }
        if (end == NULL) {
            break;
        }
        line = end + 1;
    }
    return TPBE_LIST_NOT_FOUND;
}

/**
 * @brief Copy a kernel metadata string header payload by name.
 */
int
tpb_raf_kernel_get_header_payload(const kernel_attr_t *attr, const void *data,
                                  const char *hdr_name, char *buf, size_t bufsz)
{
    int idx;
    uint64_t off = 0;
    uint32_t j;
    const char *payload;

    if (buf == NULL || bufsz == 0) {
        return TPBE_NULLPTR_ARG;
    }
    buf[0] = '\0';
    if (attr == NULL || hdr_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    idx = tpb_raf_kernel_find_header(attr, hdr_name);
    if (idx < 0) {
        return TPBE_LIST_NOT_FOUND;
    }
    for (j = 0; j < (uint32_t)idx; j++) {
        off += attr->headers[j].data_size;
    }
    if (data == NULL || attr->headers[idx].data_size == 0) {
        return TPBE_SUCCESS;
    }
    payload = (const char *)((const uint8_t *)data + off);
    snprintf(buf, bufsz, "%s", payload);
    return TPBE_SUCCESS;
}

/**
 * @brief Build a one-line variation summary from a metadata kv payload.
 */
void
tpb_raf_kernel_variation_summary(const char *payload, char *out, size_t outlen)
{
    char kv_val[512];
    const char *line;
    const char *p;

    if (out == NULL || outlen == 0) {
        return;
    }
    out[0] = '\0';
    if (payload == NULL || payload[0] == '\0') {
        return;
    }

    if (tpb_raf_kernel_meta_kv_get(payload, "kernel.name", kv_val,
                                   sizeof(kv_val)) == TPBE_SUCCESS &&
        kv_val[0] != '\0') {
        snprintf(out, outlen, "kernel.name=%s", kv_val);
        return;
    }

    line = payload;
    while (*line != '\0') {
        while (*line == '\n' || *line == '\r') {
            line++;
        }
        if (line[0] == '\0') {
            break;
        }
        if (strncmp(line, "format=", 7) == 0 ||
            strncmp(line, "section=", 8) == 0) {
            p = strchr(line, '\n');
            line = (p != NULL) ? p + 1 : line + strlen(line);
            continue;
        }
        p = strchr(line, '\n');
        if (p != NULL) {
            size_t n = (size_t)(p - line);
            if (n >= outlen) {
                n = outlen - 1;
            }
            memcpy(out, line, n);
            out[n] = '\0';
        } else {
            snprintf(out, outlen, "%s", line);
        }
        return;
    }
}

/**
 * @brief Set or update key=value in a metadata string header payload.
 */
int
tpb_raf_kernel_meta_kv_set(char **payload_buf, size_t *payload_cap,
                           const char *key, const char *value)
{
    char *old;
    char *out;
    size_t out_len;
    size_t out_cap;
    const char *line;
    const char *end;
    char search[256];
    int replaced;

    if (payload_buf == NULL || payload_cap == NULL ||
        key == NULL || value == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    old = (*payload_buf != NULL) ? *payload_buf : "";
    snprintf(search, sizeof(search), "%s=", key);

    out_cap = strlen(old) + strlen(key) + strlen(value) + 32;
    out = (char *)calloc(1, out_cap);
    if (out == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    replaced = 0;
    line = old;
    while (line != NULL && *line != '\0') {
        end = strchr(line, '\n');
        if (strncmp(line, search, strlen(search)) == 0) {
            if (!replaced) {
                out_len = strlen(out);
                snprintf(out + out_len, out_cap - out_len, "%s%s\n",
                         search, value);
                replaced = 1;
            }
        } else {
            out_len = strlen(out);
            if (end != NULL) {
                size_t ll = (size_t)(end - line + 1);
                if (out_len + ll + 1 > out_cap) {
                    free(out);
                    return TPBE_MALLOC_FAIL;
                }
                memcpy(out + out_len, line, ll);
                out[out_len + ll] = '\0';
            } else {
                snprintf(out + out_len, out_cap - out_len, "%s\n", line);
            }
        }
        if (end == NULL) {
            break;
        }
        line = end + 1;
    }

    if (!replaced) {
        out_len = strlen(out);
        snprintf(out + out_len, out_cap - out_len, "%s%s\n", search, value);
    }

    free(*payload_buf);
    *payload_buf = out;
    *payload_cap = out_cap;
    return TPBE_SUCCESS;
}
