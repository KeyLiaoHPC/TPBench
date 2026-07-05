/*
 * tpb-raf-kernel-meta.c
 * Kernel Domain metadata: registered parm/metric headers and variation/compilation/dependency kv.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "tpb-raf-kernel-meta.h"
#include "tpb-raf-types.h"
#include "../tpb-types.h"

/* kernel_entry_t.active on-disk offset from entry start */
#define TPB_KERNEL_ENTRY_ACTIVE_OFF  116

static const char *g_meta_hdr_names[TPB_RAF_KERNEL_META_HDR_COUNT] = {
    TPB_RAF_KERNEL_HDR_VARIATION,
    TPB_RAF_KERNEL_HDR_COMPILATION,
    TPB_RAF_KERNEL_HDR_DEPENDENCY
};

static void
_sf_fill_parm_header(tpb_meta_header_t *h, const tpb_rt_parm_t *parm)
{
    uint32_t type_code;
    uint32_t elem_size;

    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAF_HDR_FIXED_SIZE;
    h->ndim = 1;
    h->dimsizes[0] = 1;
    type_code = (uint32_t)(parm->ctrlbits & TPB_PARM_TYPE_MASK);
    elem_size = (type_code >> 8) & 0xFF;
    if (elem_size == 0) {
        elem_size = 8;
    }
    h->data_size = elem_size;
    h->type_bits = (uint32_t)(parm->ctrlbits &
        (TPB_PARM_SOURCE_MASK | TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK));
    snprintf(h->name, sizeof(h->name), "%s", parm->name);
    snprintf(h->note, sizeof(h->note), "%s", parm->note);
}

static void
_sf_fill_metric_header(tpb_meta_header_t *h, const tpb_k_output_t *out)
{
    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAF_HDR_FIXED_SIZE;
    h->ndim = 1;
    h->dimsizes[0] = 1;
    h->data_size = 0;
    if ((uint32_t)(out->dtype & TPB_PARM_TYPE_MASK) ==
        (TPB_DTYPE_TIMER_T & TPB_PARM_TYPE_MASK)) {
        h->type_bits = (uint32_t)(TPB_INT64_T & TPB_PARM_TYPE_MASK);
    } else {
        h->type_bits = (uint32_t)(out->dtype & TPB_PARM_TYPE_MASK);
    }
    h->uattr_bits = (uint64_t)out->unit;
    snprintf(h->name, sizeof(h->name), "%s", out->name);
    snprintf(h->note, sizeof(h->note), "%s", out->note);
}

static int
_sf_append_parm_value(uint8_t **ptr, const tpb_rt_parm_t *parm, uint32_t elem_size)
{
    if (*ptr == NULL || parm == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    memcpy(*ptr, &parm->value, elem_size);
    *ptr += elem_size;
    return TPBE_SUCCESS;
}

static int
_sf_init_meta_payload(char **payload_buf, size_t *payload_cap,
                      const char *section, const char *initial_kv)
{
    size_t need;

    if (payload_buf == NULL || payload_cap == NULL || section == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    need = 64;
    if (initial_kv != NULL) {
        need += strlen(initial_kv);
    }
    *payload_buf = (char *)calloc(1, need);
    if (*payload_buf == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    *payload_cap = need;
    snprintf(*payload_buf, need, "format=tpbench.kernel_meta.v1\nsection=%s\n",
             section);
    if (initial_kv != NULL && initial_kv[0] != '\0') {
        strncat(*payload_buf, initial_kv, need - strlen(*payload_buf) - 1);
    }
    return TPBE_SUCCESS;
}

static void
_sf_fill_string_header(tpb_meta_header_t *h, const char *name,
                       const char *payload)
{
    size_t plen;

    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAF_HDR_FIXED_SIZE;
    h->ndim = 1;
    plen = (payload != NULL) ? strlen(payload) : 0;
    h->dimsizes[0] = (uint64_t)(plen + 1);
    h->data_size = (uint64_t)(plen + 1);
    h->type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    snprintf(h->name, sizeof(h->name), "%s", name);
    snprintf(h->note, sizeof(h->note), "Kernel metadata: %s", name);
}

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

int
tpb_raf_kernel_find_header(const kernel_attr_t *attr, const char *name)
{
    uint32_t i;

    if (attr == NULL || name == NULL) {
        return -1;
    }
    for (i = 0; i < attr->nheader; i++) {
        if (strcmp(attr->headers[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

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

int
tpb_raf_kernel_meta_kv_set(char **payload_buf, size_t *payload_cap,
                           const char *key, const char *value)
{
    char *old;
    char *newline;
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

int
tpb_raf_kernel_build_registered_attr(const tpb_kernel_t *kernel,
                                     const unsigned char kernel_id[20],
                                     kernel_attr_t *attr_out,
                                     void **data_out,
                                     uint64_t *datasize_out)
{
    uint32_t nparm;
    uint32_t nmetric;
    uint32_t nheader;
    uint32_t hi;
    tpb_meta_header_t *hdrs;
    void *data;
    uint64_t datasize;
    uint8_t *ptr;
    char kid_hex[41];
    char var_init[512];
    char *meta_payloads[TPB_RAF_KERNEL_META_HDR_COUNT];
    size_t meta_caps[TPB_RAF_KERNEL_META_HDR_COUNT];
    uint32_t mi;

    if (kernel == NULL || kernel_id == NULL || attr_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    nparm = (uint32_t)kernel->info.nparms;
    nmetric = (uint32_t)kernel->info.nouts;
    nheader = nparm + nmetric + TPB_RAF_KERNEL_META_HDR_COUNT;

    hdrs = (tpb_meta_header_t *)calloc(nheader, sizeof(*hdrs));
    if (hdrs == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    datasize = 0;
    for (uint32_t i = 0; i < nparm; i++) {
        uint32_t type_code;
        uint32_t elem_size;

        _sf_fill_parm_header(&hdrs[i], &kernel->info.parms[i]);
        type_code = (uint32_t)(kernel->info.parms[i].ctrlbits &
                               TPB_PARM_TYPE_MASK);
        elem_size = (type_code >> 8) & 0xFF;
        if (elem_size == 0) {
            elem_size = 8;
        }
        datasize += elem_size;
    }

    for (uint32_t j = 0; j < nmetric; j++) {
        _sf_fill_metric_header(&hdrs[nparm + j], &kernel->info.outs[j]);
    }

    tpb_raf_id_to_hex(kernel_id, kid_hex);
    snprintf(var_init, sizeof(var_init),
             "kernel.name=%s\nkernel.id=%s\nactive=1\n",
             kernel->info.name, kid_hex);

    memset(meta_payloads, 0, sizeof(meta_payloads));
    memset(meta_caps, 0, sizeof(meta_caps));
    for (mi = 0; mi < TPB_RAF_KERNEL_META_HDR_COUNT; mi++) {
        const char *init = (mi == 0) ? var_init : NULL;

        if (_sf_init_meta_payload(&meta_payloads[mi], &meta_caps[mi],
                                  g_meta_hdr_names[mi], init) != TPBE_SUCCESS) {
            for (uint32_t k = 0; k < mi; k++) {
                free(meta_payloads[k]);
            }
            free(hdrs);
            return TPBE_MALLOC_FAIL;
        }
        _sf_fill_string_header(&hdrs[nparm + nmetric + mi],
                               g_meta_hdr_names[mi], meta_payloads[mi]);
        datasize += hdrs[nparm + nmetric + mi].data_size;
    }

    data = (datasize > 0) ? calloc(1, (size_t)datasize) : NULL;
    if (datasize > 0 && data == NULL) {
        for (mi = 0; mi < TPB_RAF_KERNEL_META_HDR_COUNT; mi++) {
            free(meta_payloads[mi]);
        }
        free(hdrs);
        return TPBE_MALLOC_FAIL;
    }

    ptr = (uint8_t *)data;
    for (uint32_t i = 0; i < nparm; i++) {
        uint32_t type_code;
        uint32_t elem_size;

        type_code = (uint32_t)(kernel->info.parms[i].ctrlbits &
                               TPB_PARM_TYPE_MASK);
        elem_size = (type_code >> 8) & 0xFF;
        if (elem_size == 0) {
            elem_size = 8;
        }
        _sf_append_parm_value(&ptr, &kernel->info.parms[i], elem_size);
    }

    hi = nparm + nmetric;
    for (mi = 0; mi < TPB_RAF_KERNEL_META_HDR_COUNT; mi++) {
        size_t plen = strlen(meta_payloads[mi]) + 1;
        memcpy(ptr, meta_payloads[mi], plen);
        ptr += plen;
        free(meta_payloads[mi]);
    }

    memset(attr_out, 0, sizeof(*attr_out));
    memcpy(attr_out->kernel_id, kernel_id, 20);
    snprintf(attr_out->kernel_name, sizeof(attr_out->kernel_name), "%s",
             kernel->info.name);
    snprintf(attr_out->version, sizeof(attr_out->version), "%.1f", TPB_VERSION);
    snprintf(attr_out->description, sizeof(attr_out->description), "%s",
             kernel->info.note);
    attr_out->nparm = nparm;
    attr_out->nmetric = nmetric;
    attr_out->kctrl = (uint32_t)kernel->info.kctrl;
    attr_out->nheader = nheader;
    attr_out->active = 1;
    attr_out->headers = hdrs;

    if (data_out != NULL) {
        *data_out = data;
    } else {
        free(data);
    }
    if (datasize_out != NULL) {
        *datasize_out = datasize;
    }
    return TPBE_SUCCESS;
}

void
tpb_raf_kernel_free_built_attr(kernel_attr_t *attr, void *data)
{
    if (attr != NULL && attr->headers != NULL) {
        tpb_raf_free_headers(attr->headers, attr->nheader);
        attr->headers = NULL;
    }
    free(data);
}

int
tpb_raf_entry_patch_kernel_active(const char *workspace,
                                  const unsigned char kernel_id[20],
                                  uint32_t active)
{
    char fpath[TPB_RAF_PATH_MAX];
    int fd;
    FILE *fp;
    struct stat st;
    long fsize;
    long off;
    int i;
    int n;
    kernel_entry_t *entries;

    if (workspace == NULL || kernel_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (tpb_raf_entry_list_kernel(workspace, &entries, &n) != TPBE_SUCCESS) {
        return TPBE_FILE_IO_FAIL;
    }

    off = -1;
    for (i = 0; i < n; i++) {
        if (memcmp(entries[i].kernel_id, kernel_id, 20) == 0) {
            off = (long)i;
            break;
        }
    }
    free(entries);
    if (off < 0) {
        return TPBE_LIST_NOT_FOUND;
    }

    snprintf(fpath, sizeof(fpath), "%s/%s/%s",
             workspace, TPB_RAF_KERNEL_DIR, TPB_RAF_KERNEL_ENTRY);

    fd = open(fpath, O_RDWR);
    if (fd < 0) {
        return TPBE_FILE_IO_FAIL;
    }
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return TPBE_FILE_IO_FAIL;
    }

    fp = fdopen(fd, "r+b");
    if (fp == NULL) {
        close(fd);
        return TPBE_FILE_IO_FAIL;
    }

    if (fstat(fd, &st) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAF_MAGIC_LEN)) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    {
        long entry_off = (long)TPB_RAF_MAGIC_LEN +
            off * (long)sizeof(kernel_entry_t) +
            (long)TPB_KERNEL_ENTRY_ACTIVE_OFF;

        if (fseek(fp, entry_off, SEEK_SET) != 0) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        if (fwrite(&active, sizeof(active), 1, fp) != 1) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
    }

    fclose(fp);
    return TPBE_SUCCESS;
}

int
tpb_raf_record_patch_kernel_active(const char *workspace,
                                   const unsigned char kernel_id[20],
                                   uint32_t active)
{
    char fpath[TPB_RAF_PATH_MAX];
    char hex[41];
    FILE *fp;
    long off;
    /* magic(8)+metasize(8)+datasize(8)+ids(60)+name(256)+version(64)+desc(2048)
       + nparm(4)+nmetric(4)+kctrl(4)+nheader(4)+active(4) */
    const long active_off = 8 + 8 + 8 + 60 + 256 + 64 + 2048 + 4 + 4 + 4 + 4;

    if (workspace == NULL || kernel_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_raf_id_to_hex(kernel_id, hex);
    snprintf(fpath, sizeof(fpath), "%s/%s/%s.tpbr",
             workspace, TPB_RAF_KERNEL_DIR, hex);

    fp = fopen(fpath, "r+b");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }

    off = active_off;
    if (fseek(fp, off, SEEK_SET) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (fwrite(&active, sizeof(active), 1, fp) != 1) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    fclose(fp);
    return TPBE_SUCCESS;
}

int
tpb_raf_kernel_deactivate_same_name(const char *workspace,
                                    const char *kernel_name,
                                    const unsigned char skip_id[20])
{
    kernel_entry_t *entries;
    int n;
    int i;
    int err;

    if (workspace == NULL || kernel_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    err = tpb_raf_entry_list_kernel(workspace, &entries, &n);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    for (i = 0; i < n; i++) {
        if (strcmp(entries[i].kernel_name, kernel_name) != 0) {
            continue;
        }
        if (skip_id != NULL &&
            memcmp(entries[i].kernel_id, skip_id, 20) == 0) {
            continue;
        }
        (void)tpb_raf_entry_patch_kernel_active(workspace,
                                                entries[i].kernel_id, 0);
        (void)tpb_raf_record_patch_kernel_active(workspace,
                                                 entries[i].kernel_id, 0);
    }

    free(entries);
    return TPBE_SUCCESS;
}

static int
_sf_split_meta_key(const char *full_key, char *section_out, size_t section_len,
                   char *subkey_out, size_t subkey_len)
{
    const char *dot;

    if (full_key == NULL || section_out == NULL || subkey_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    dot = strchr(full_key, '.');
    if (dot == NULL || dot == full_key) {
        return TPBE_KERN_ARG_FAIL;
    }

    if ((size_t)(dot - full_key) >= section_len) {
        return TPBE_KERN_ARG_FAIL;
    }
    memcpy(section_out, full_key, (size_t)(dot - full_key));
    section_out[dot - full_key] = '\0';
    snprintf(subkey_out, subkey_len, "%s", dot + 1);
    return TPBE_SUCCESS;
}

int
tpb_raf_kernel_update_meta_key(const char *workspace,
                               const unsigned char kernel_id[20],
                               const char *full_key,
                               const char *value)
{
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    char section[64];
    char subkey[256];
    int hdr_idx;
    char *payload = NULL;
    size_t payload_cap = 0;
    int err;
    uint32_t i;
    uint64_t new_datasize;
    void *new_data;
    uint8_t *ptr;

    if (workspace == NULL || kernel_id == NULL ||
        full_key == NULL || value == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    err = _sf_split_meta_key(full_key, section, sizeof(section),
                             subkey, sizeof(subkey));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, kernel_id, &attr,
                                     &data, &datasize);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    hdr_idx = tpb_raf_kernel_find_header(&attr, section);
    if (hdr_idx < 0) {
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return TPBE_LIST_NOT_FOUND;
    }

    {
        uint64_t off = 0;
        uint32_t j;

        for (j = 0; j < (uint32_t)hdr_idx; j++) {
            off += attr.headers[j].data_size;
        }
        if (data == NULL || off + attr.headers[hdr_idx].data_size > datasize) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            return TPBE_FILE_IO_FAIL;
        }
        payload = strdup((const char *)((const uint8_t *)data + off));
        if (payload == NULL) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            return TPBE_MALLOC_FAIL;
        }
        payload_cap = strlen(payload) + 1;
    }

    err = tpb_raf_kernel_meta_kv_set(&payload, &payload_cap, subkey, value);
    if (err != TPBE_SUCCESS) {
        free(payload);
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return err;
    }

    attr.headers[hdr_idx].dimsizes[0] = strlen(payload) + 1;
    attr.headers[hdr_idx].data_size = strlen(payload) + 1;

    new_datasize = 0;
    for (i = 0; i < attr.nheader; i++) {
        new_datasize += attr.headers[i].data_size;
    }

    new_data = calloc(1, (size_t)new_datasize);
    if (new_data == NULL) {
        free(payload);
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return TPBE_MALLOC_FAIL;
    }

    ptr = (uint8_t *)new_data;
    for (i = 0; i < attr.nheader; i++) {
        if (i == (uint32_t)hdr_idx) {
            memcpy(ptr, payload, (size_t)attr.headers[i].data_size);
        } else {
            uint64_t off = 0;
            uint32_t j;

            for (j = 0; j < i; j++) {
                off += attr.headers[j].data_size;
            }
            if (data != NULL && attr.headers[i].data_size > 0) {
                memcpy(ptr, (const uint8_t *)data + off,
                       (size_t)attr.headers[i].data_size);
            }
        }
        ptr += attr.headers[i].data_size;
    }

    free(payload);
    free(data);

    err = tpb_raf_record_write_kernel(workspace, &attr, new_data, new_datasize);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(new_data);
    return err;
}
