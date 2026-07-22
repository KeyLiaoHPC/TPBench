/*
 * tpbcli-task-export.c
 * `tpbcli task export` directory layout and meta/data CSV generation.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-data-fmt.h"
#include "tpbcli-task-csv.h"
#include "tpbcli-task-internal.h"

/* Local Function Prototypes */
static int _sf_dir_has_files(const char *path);
static int _sf_pick_work_dir(const char *outroot, const unsigned char root_id[20],
                             char *dir_out, size_t dir_len);
static int _sf_sanitize_hostname(const char *in, char *out, size_t outlen);
static int _sf_hostname_for_task(const char *workspace,
                                   const unsigned char tbatch_id[20],
                                   char *host_out, size_t host_len);
static int _sf_format_elem(const tpb_meta_header_t *h, const uint8_t *p,
                             size_t elem_size, char *buf, size_t buflen,
                             int *use_byte_out);
static int _sf_column_rows(const tpb_meta_header_t *h, size_t elem_size,
                           uint64_t *nrows_out);
static int _sf_write_meta(FILE *fp, const task_attr_t *attr, uint64_t datasize);
static int _sf_write_data(FILE *fp, const task_attr_t *attr, const void *data,
                          uint64_t datasize);
static int _sf_atomic_file(const char *final_path,
                           int (*writer)(FILE *fp, void *ctx), void *ctx);
static int _sf_export_pair(const char *meta_final, const char *data_final,
                           const task_attr_t *attr, const void *data,
                           uint64_t datasize, int *had_fail_out);
static int _sf_member_passes(const tpbcli_task_export_opts_t *opts,
                             int is_capsule, int subrank, uint32_t tid);
static int _sf_export_one_id(const char *workspace, const char *work_dir,
                             const unsigned char id[20], int is_capsule_name,
                             int *ok_out, int *fail_out);
static int _sf_export_work(const char *workspace, const char *outroot,
                           const tpbcli_task_export_work_t *work,
                           const tpbcli_task_export_opts_t *opts,
                           int *ok_out, int *fail_out);

static int
_sf_dir_has_files(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *ent;

    if (d == NULL) {
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        closedir(d);
        return 1;
    }
    closedir(d);
    return 0;
}

static int
_sf_pick_work_dir(const char *outroot, const unsigned char root_id[20],
                  char *dir_out, size_t dir_len)
{
    char hex[41];
    char base[PATH_MAX];
    int n;
    int suffix;

    tpb_raf_id_to_hex(root_id, hex);
    n = snprintf(base, sizeof(base), "%s/%s", outroot, hex);
    if (n < 0 || (size_t)n >= sizeof(base)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (mkdir(outroot, 0700) != 0 && errno != EEXIST) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    for (suffix = 0; suffix <= 3; suffix++) {
        char candidate[PATH_MAX];
        struct stat st;

        if (suffix == 0) {
            snprintf(candidate, sizeof(candidate), "%s", base);
        } else {
            snprintf(candidate, sizeof(candidate), "%s_%d", base, suffix);
        }
        if (stat(candidate, &st) != 0) {
            if (mkdir(candidate, 0700) != 0 && errno != EEXIST) {
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
            }
            snprintf(dir_out, dir_len, "%s", candidate);
            return TPBE_SUCCESS;
        }
        if (S_ISDIR(st.st_mode) && !_sf_dir_has_files(candidate)) {
            snprintf(dir_out, dir_len, "%s", candidate);
            return TPBE_SUCCESS;
        }
    }
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                    "error: cannot allocate export directory for task %s\n",
                    hex);
    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
}

static int
_sf_sanitize_hostname(const char *in, char *out, size_t outlen)
{
    size_t i;
    size_t o = 0;

    if (in == NULL || out == NULL || outlen == 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    for (i = 0; in[i] != '\0' && o + 1 < outlen; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            out[o++] = (char)c;
        } else {
            out[o++] = '_';
        }
    }
    out[o] = '\0';
    if (o == 0) {
        snprintf(out, outlen, "localhost");
    }
    return TPBE_SUCCESS;
}

static int
_sf_hostname_for_task(const char *workspace, const unsigned char tbatch_id[20],
                      char *host_out, size_t host_len)
{
    tbatch_attr_t tb;
    void *data = NULL;
    uint64_t ds = 0;
    int err;

    memset(&tb, 0, sizeof(tb));
    err = tpb_raf_record_read_tbatch(workspace, tbatch_id, &tb, &data, &ds);
    if (err != TPBE_SUCCESS || tb.hostname[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_DIRECT,
                        "warning: tbatch hostname unavailable; using "
                        "'localhost'\n");
        snprintf(host_out, host_len, "localhost");
        tpb_raf_free_headers(tb.headers, tb.nheader);
        free(data);
        return TPBE_SUCCESS;
    }
    err = _sf_sanitize_hostname(tb.hostname, host_out, host_len);
    tpb_raf_free_headers(tb.headers, tb.nheader);
    free(data);
    return err;
}

static int
_sf_is_taskid_link(const tpb_meta_header_t *h)
{
    return (h != NULL && strcmp(h->name, "TaskID") == 0 &&
            strcmp(h->tag, TPB_TAG_LINK) == 0);
}

static size_t
_sf_elem_size(const tpb_meta_header_t *h)
{
    TPB_DTYPE dt;
    size_t esz = 0;

    if (_sf_is_taskid_link(h)) {
        return 20;
    }
    dt = (TPB_DTYPE)(h->type_bits & TPB_PARM_TYPE_MASK);
    if (tpb_dtype_elem_size(dt, &esz) != TPBE_SUCCESS || esz == 0) {
        if (h->data_size > 0 && h->ndim == 0) {
            return (size_t)h->data_size;
        }
        return 1;
    }
    return esz;
}

static int
_sf_format_elem(const tpb_meta_header_t *h, const uint8_t *p, size_t elem_size,
                char *buf, size_t buflen, int *use_byte_out)
{
    TPB_DTYPE dt;
    float fv;
    double dv;

    *use_byte_out = 0;
    if (_sf_is_taskid_link(h)) {
        tpb_raf_id_to_hex(p, buf);
        return TPBE_SUCCESS;
    }
    dt = (TPB_DTYPE)(h->type_bits & TPB_PARM_TYPE_MASK);
    if (dt == TPB_CHAR_T && elem_size == 1 && *p == ',') {
        *use_byte_out = 1;
        return TPBE_SUCCESS;
    }
    switch (dt) {
    case TPB_FLOAT_T:
        fv = *(const float *)(const void *)p;
        if (isnan(fv)) {
            snprintf(buf, buflen, "nan");
        } else if (isinf(fv)) {
            snprintf(buf, buflen, fv < 0 ? "-inf" : "inf");
        } else {
            snprintf(buf, buflen, "%.7f", (double)fv);
        }
        return TPBE_SUCCESS;
    case TPB_DOUBLE_T:
        dv = *(const double *)(const void *)p;
        if (isnan(dv)) {
            snprintf(buf, buflen, "nan");
        } else if (isinf(dv)) {
            snprintf(buf, buflen, dv < 0 ? "-inf" : "inf");
        } else {
            snprintf(buf, buflen, "%.15f", dv);
        }
        return TPBE_SUCCESS;
    case TPB_UINT32_T:
    case TPB_UNSIGNED_T:
        snprintf(buf, buflen, "%" PRIu32,
                 *(const uint32_t *)(const void *)p);
        return TPBE_SUCCESS;
    case TPB_INT32_T:
    case TPB_INT_T:
        snprintf(buf, buflen, "%" PRId32, *(const int32_t *)(const void *)p);
        return TPBE_SUCCESS;
    case TPB_UINT64_T:
        snprintf(buf, buflen, "%" PRIu64,
                 *(const uint64_t *)(const void *)p);
        return TPBE_SUCCESS;
    case TPB_INT64_T:
        snprintf(buf, buflen, "%" PRId64, *(const int64_t *)(const void *)p);
        return TPBE_SUCCESS;
    case TPB_STRING_T:
        snprintf(buf, buflen, "%s", (const char *)p);
        return TPBE_SUCCESS;
    default:
        {
            size_t k;
            size_t pos = 0;
            pos = (size_t)snprintf(buf, buflen, "0x");
            for (k = 0; k < elem_size && pos + 3 < buflen; k++) {
                pos += (size_t)snprintf(buf + pos, buflen - pos, "%02x",
                                        (unsigned)p[k]);
            }
        }
        return TPBE_SUCCESS;
    }
}

static int
_sf_column_rows(const tpb_meta_header_t *h, size_t elem_size, uint64_t *nrows_out)
{
    uint32_t nd = h->ndim;
    uint64_t total = 1;
    uint32_t j;

    if ((h->type_bits & TPB_PARM_TYPE_MASK) == TPB_STRING_T && nd <= 1) {
        *nrows_out = 1;
        return TPBE_SUCCESS;
    }
    if (nd == 0) {
        *nrows_out = 1;
        return TPBE_SUCCESS;
    }
    if (nd > TPBM_DATA_NDIM_MAX) {
        nd = TPBM_DATA_NDIM_MAX;
    }
    for (j = 0; j < nd; j++) {
        if (h->dimsizes[j] == 0) {
            total = 0;
            break;
        }
        if (total > UINT64_MAX / h->dimsizes[j]) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        total *= h->dimsizes[j];
    }
    if (elem_size > 0 && total > 0 &&
        total > h->data_size / elem_size) {
        total = h->data_size / elem_size;
    }
    *nrows_out = total;
    return TPBE_SUCCESS;
}

static TPB_DTYPE
_sf_hdr_dtype(const tpb_meta_header_t *h)
{
    return (TPB_DTYPE)(h->type_bits & TPB_PARM_TYPE_MASK);
}

static int
_sf_env_detect_triple(const task_attr_t *attr, uint32_t *key_hi_out,
                      uint32_t *count_hi_out, uint32_t *value_hi_out)
{
    uint32_t i;

    if (attr == NULL || key_hi_out == NULL || count_hi_out == NULL ||
        value_hi_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    for (i = 0; i + 2 < attr->nheader; i++) {
        const tpb_meta_header_t *k = &attr->headers[i];
        const tpb_meta_header_t *c = &attr->headers[i + 1];
        const tpb_meta_header_t *v = &attr->headers[i + 2];

        if (strcmp(k->name, TPB_TASK_HDR_ENV_KEY) != 0 ||
            strcmp(c->name, TPB_TASK_HDR_ENV_COUNT) != 0 ||
            strcmp(v->name, TPB_TASK_HDR_ENV_VALUE) != 0) {
            continue;
        }
        if (_sf_hdr_dtype(k) != TPB_STRING_T ||
            _sf_hdr_dtype(c) != TPB_INT32_T ||
            _sf_hdr_dtype(v) != TPB_STRING_T) {
            continue;
        }
        if (k->data_size == 0 || c->data_size == 0 || v->data_size == 0) {
            continue;
        }
        *key_hi_out = i;
        *count_hi_out = i + 1;
        *value_hi_out = i + 2;
        return 1;
    }
    return 0;
}

static int
_sf_env_blob_nkeys(const char *kb, size_t *nkeys_out)
{
    size_t n = 0;

    if (nkeys_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (kb == NULL || kb[0] == '\0') {
        *nkeys_out = 0;
        return TPBE_SUCCESS;
    }
    n = 1;
    for (const char *p = kb; *p != '\0'; p++) {
        if (*p == ':') {
            n++;
        }
    }
    *nkeys_out = n;
    return TPBE_SUCCESS;
}

static int
_sf_env_blob_nsegs(const char *vb, size_t *nseg_out)
{
    return _sf_env_blob_nkeys(vb, nseg_out);
}

static int
_sf_env_key_to_buf(const char *kb, size_t idx, char *buf, size_t buflen)
{
    const char *p;
    const char *start;
    size_t cur = 0;

    if (buf == NULL || buflen == 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (kb == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    p = kb;
    while (1) {
        start = p;
        while (*p != '\0' && *p != ':') {
            p++;
        }
        if (cur == idx) {
            size_t len = (size_t)(p - start);
            if (len + 1 > buflen) {
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
            }
            memcpy(buf, start, len);
            buf[len] = '\0';
            return TPBE_SUCCESS;
        }
        if (*p == '\0') {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        p++;
        cur++;
    }
}

static int
_sf_env_value_to_buf(const int32_t *counts, size_t nkeys, const char *vb,
                     size_t key_idx, char *out, size_t outlen)
{
    size_t i;
    size_t seg_skip = 0;
    size_t seg_take;
    const char *p;
    const char *start;
    size_t nseg = 0;
    size_t outpos = 0;

    if (key_idx >= nkeys || counts == NULL || out == NULL || outlen == 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < key_idx; i++) {
        if (counts[i] < 0) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        seg_skip += (size_t)counts[i];
    }
    seg_take = (size_t)counts[key_idx];
    out[0] = '\0';
    if (seg_take == 0) {
        return TPBE_SUCCESS;
    }
    p = (vb != NULL) ? vb : "";
    while (nseg < seg_skip && *p != '\0') {
        while (*p != '\0' && *p != ':') {
            p++;
        }
        if (*p == ':') {
            p++;
        }
        nseg++;
    }
    start = p;
    for (i = 0; i < seg_take; i++) {
        if (*p == '\0' && p == start) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        while (*p != '\0' && *p != ':') {
            p++;
        }
        if (outpos > 0) {
            if (outpos + 1 >= outlen) {
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
            }
            out[outpos++] = ':';
        }
        if ((size_t)(p - start) >= outlen - outpos) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        memcpy(out + outpos, start, (size_t)(p - start));
        outpos += (size_t)(p - start);
        out[outpos] = '\0';
        if (*p == ':') {
            p++;
        }
        start = p;
    }
    return TPBE_SUCCESS;
}

static int
_sf_env_validate(const task_attr_t *attr, const void *data, uint64_t datasize,
                 uint32_t key_hi, uint32_t count_hi, uint32_t value_hi,
                 size_t *nkeys_out)
{
    const tpb_meta_header_t *hc = &attr->headers[count_hi];
    const uint8_t *base = (const uint8_t *)data;
    uint64_t off = 0;
    uint32_t i;
    const char *kb;
    const int32_t *counts;
    const char *vb;
    size_t nkeys_meta;
    size_t nkeys_blob;
    size_t nseg;
    size_t sum = 0;
    size_t expect_cb;

    for (i = 0; i < attr->nheader; i++) {
        if (i == key_hi) {
            break;
        }
        off += attr->headers[i].data_size;
    }
    if (off + attr->headers[key_hi].data_size + hc->data_size +
            attr->headers[value_hi].data_size >
        datasize) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    kb = (const char *)(base + off);
    counts = (const int32_t *)(const void *)(base + off +
        attr->headers[key_hi].data_size);
    vb = (const char *)(base + off + attr->headers[key_hi].data_size +
        hc->data_size);

    nkeys_meta = (size_t)hc->dimsizes[0];
    expect_cb = (nkeys_meta > 0) ? nkeys_meta : 1;
    if (hc->data_size != expect_cb * sizeof(int32_t)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (_sf_env_blob_nkeys(kb, &nkeys_blob) != TPBE_SUCCESS) {
        return TPBE_CLI_FAIL;
    }
    if (nkeys_blob != nkeys_meta) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < nkeys_meta; i++) {
        if (counts[i] < 0) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        sum += (size_t)counts[i];
    }
    if (_sf_env_blob_nsegs(vb, &nseg) != TPBE_SUCCESS) {
        return TPBE_CLI_FAIL;
    }
    if (nseg != sum) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *nkeys_out = nkeys_meta;
    return TPBE_SUCCESS;
}

static int
_sf_env_skip_col(int env_on, uint32_t hi, uint32_t count_hi, uint32_t value_hi)
{
    return env_on && (hi == count_hi || hi == value_hi);
}

typedef struct meta_wctx {
    const task_attr_t *attr;
    uint64_t           datasize;
} meta_wctx_t;

static void
_sf_meta_kv(tpbcli_task_csv_writer_t *w, const char *field, const char *val)
{
    (void)tpbcli_task_csv_row_begin(w);
    (void)tpbcli_task_csv_cell(w, field);
    (void)tpbcli_task_csv_cell(w, val);
    (void)tpbcli_task_csv_row_end(w);
}

static int
_sf_write_meta(FILE *fp, const task_attr_t *attr, uint64_t datasize)
{
    tpbcli_task_csv_writer_t csv;
    char hx[41];
    char tmp[768];
    char decode[512];
    tpb_datetime_str_t ts;
    uint32_t i;
    meta_wctx_t ctx;

    (void)ctx;
    tpbcli_task_csv_writer_init(&csv, fp);
    (void)tpbcli_task_csv_row_begin(&csv);
    (void)tpbcli_task_csv_cell(&csv, "field");
    (void)tpbcli_task_csv_cell(&csv, "value");
    (void)tpbcli_task_csv_row_end(&csv);

    tpb_raf_id_to_hex(attr->task_record_id, hx);
    _sf_meta_kv(&csv, "task_record_id", hx);
    tpb_raf_id_to_hex(attr->derive_to, hx);
    _sf_meta_kv(&csv, "derive_to", hx);
    tpb_raf_id_to_hex(attr->inherit_from, hx);
    _sf_meta_kv(&csv, "inherit_from", hx);
    tpb_raf_id_to_hex(attr->tbatch_id, hx);
    _sf_meta_kv(&csv, "tbatch_id", hx);
    tpb_raf_id_to_hex(attr->kernel_id, hx);
    _sf_meta_kv(&csv, "kernel_id", hx);
    snprintf(tmp, sizeof(tmp), "%" PRIu64, (uint64_t)attr->utc_bits);
    _sf_meta_kv(&csv, "utc_bits", tmp);
    if (attr->utc_bits != 0 &&
        tpb_ts_bits_to_isoutc(attr->utc_bits, &ts) == 0) {
        _sf_meta_kv(&csv, "start_utc", ts.str);
    } else {
        _sf_meta_kv(&csv, "start_utc", "");
    }
    snprintf(tmp, sizeof(tmp), "%" PRIu64, (uint64_t)attr->btime);
    _sf_meta_kv(&csv, "btime", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu64, (uint64_t)attr->duration);
    _sf_meta_kv(&csv, "duration", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->exit_code);
    _sf_meta_kv(&csv, "exit_code", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->handle_index);
    _sf_meta_kv(&csv, "handle_index", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->pid);
    _sf_meta_kv(&csv, "pid", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->tid);
    _sf_meta_kv(&csv, "tid", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->ninput);
    _sf_meta_kv(&csv, "ninput", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->noutput);
    _sf_meta_kv(&csv, "noutput", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->nheader);
    _sf_meta_kv(&csv, "nheader", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu32, attr->reserve);
    _sf_meta_kv(&csv, "reserve", tmp);
    snprintf(tmp, sizeof(tmp), "%" PRIu64, datasize);
    _sf_meta_kv(&csv, "record_datasize", tmp);

    for (i = 0; i < attr->nheader; i++) {
        const tpb_meta_header_t *h = &attr->headers[i];
        char key[128];

        snprintf(key, sizeof(key), "header[%" PRIu32 "].block_size", i);
        snprintf(tmp, sizeof(tmp), "%" PRIu32, h->block_size);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].ndim", i);
        snprintf(tmp, sizeof(tmp), "%" PRIu32, h->ndim);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].data_size", i);
        snprintf(tmp, sizeof(tmp), "%" PRIu64, h->data_size);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].type_bits", i);
        snprintf(tmp, sizeof(tmp), "0x%08" PRIX32, h->type_bits);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].type_bits.source", i);
        tpbcli_data_fmt_decode_type_bits(h->type_bits, decode, sizeof(decode));
        snprintf(tmp, sizeof(tmp), "0x%08" PRIX32,
                 h->type_bits & (uint32_t)TPB_PARM_SOURCE_MASK);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].type_bits.check", i);
        snprintf(tmp, sizeof(tmp), "0x%08" PRIX32,
                 h->type_bits & (uint32_t)TPB_PARM_CHECK_MASK);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].type_bits.type", i);
        snprintf(tmp, sizeof(tmp), "0x%08" PRIX32,
                 h->type_bits & (uint32_t)TPB_PARM_TYPE_MASK);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "]._reserve", i);
        snprintf(tmp, sizeof(tmp), "%" PRIu32, h->_reserve);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].uattr_bits", i);
        snprintf(tmp, sizeof(tmp), "0x%016" PRIX64, (uint64_t)h->uattr_bits);
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].uattr_bits.cast", i);
        snprintf(tmp, sizeof(tmp), "0x%016" PRIX64,
                 (uint64_t)(h->uattr_bits & TPB_UATTR_MASK));
        _sf_meta_kv(&csv, key, tmp);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].name", i);
        _sf_meta_kv(&csv, key, h->name);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].tag", i);
        _sf_meta_kv(&csv, key, h->tag);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].note", i);
        _sf_meta_kv(&csv, key, h->note);
        {
            char dsz[256];
            char dnm[512];
            uint32_t j;

            dsz[0] = '\0';
            for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
                char part[32];
                snprintf(part, sizeof(part), "%s%" PRIu64, j ? "," : "",
                         h->dimsizes[j]);
                if (strlen(dsz) + strlen(part) + 1 < sizeof(dsz)) {
                    strcat(dsz, part);
                }
            }
            snprintf(key, sizeof(key), "header[%" PRIu32 "].dimsizes", i);
            _sf_meta_kv(&csv, key, dsz);
            dnm[0] = '\0';
            for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
                char part[80];
                snprintf(part, sizeof(part), "%s%s", j ? "," : "", h->dimnames[j]);
                if (strlen(dnm) + strlen(part) + 1 < sizeof(dnm)) {
                    strcat(dnm, part);
                }
            }
            snprintf(key, sizeof(key), "header[%" PRIu32 "].dimnames", i);
            _sf_meta_kv(&csv, key, dnm);
        }
        (void)decode;
    }
    if (fflush(fp) != 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

typedef struct data_wctx {
    const task_attr_t *attr;
    const void        *data;
    uint64_t           datasize;
} data_wctx_t;

static int
_sf_write_data(FILE *fp, const task_attr_t *attr, const void *data,
               uint64_t datasize)
{
    tpbcli_task_csv_writer_t csv;
    uint32_t hi;
    uint32_t ncols = 0;
    uint64_t nrows = 0;
    uint64_t row;
    const uint8_t *base = (const uint8_t *)data;
    uint64_t off = 0;
    int env_on = 0;
    uint32_t env_key_hi = 0;
    uint32_t env_count_hi = 0;
    uint32_t env_value_hi = 0;
    size_t env_nkeys = 0;
    const char *env_kb = NULL;
    const int32_t *env_counts = NULL;
    const char *env_vb = NULL;
    uint64_t env_data_off = 0;

    env_on = _sf_env_detect_triple(attr, &env_key_hi, &env_count_hi,
                                   &env_value_hi);
    if (env_on) {
        int err;

        err = _sf_env_validate(attr, data, datasize, env_key_hi, env_count_hi,
                               env_value_hi, &env_nkeys);
        if (err != TPBE_SUCCESS) {
            return err;
        }
        off = 0;
        for (hi = 0; hi < env_key_hi; hi++) {
            env_data_off += attr->headers[hi].data_size;
        }
        env_kb = (const char *)(base + env_data_off);
        env_counts = (const int32_t *)(const void *)(base + env_data_off +
            attr->headers[env_key_hi].data_size);
        env_vb = (const char *)(base + env_data_off +
            attr->headers[env_key_hi].data_size +
            attr->headers[env_count_hi].data_size);
    }

    tpbcli_task_csv_writer_init(&csv, fp);

    for (hi = 0; hi < attr->nheader; hi++) {
        const tpb_meta_header_t *h = &attr->headers[hi];
        uint64_t nr;
        size_t esz;

        if (h->data_size == 0) {
            continue;
        }
        if (_sf_env_skip_col(env_on, hi, env_count_hi, env_value_hi)) {
            continue;
        }
        if (env_on && hi == env_key_hi) {
            ncols += 2;
            if (env_nkeys > nrows) {
                nrows = (uint64_t)env_nkeys;
            }
            continue;
        }
        esz = _sf_elem_size(h);
        if (_sf_column_rows(h, esz, &nr) != TPBE_SUCCESS) {
            return TPBE_CLI_FAIL;
        }
        if (nr > nrows) {
            nrows = nr;
        }
        ncols++;
    }

    (void)ncols;
    (void)tpbcli_task_csv_row_begin(&csv);
    for (hi = 0; hi < attr->nheader; hi++) {
        const tpb_meta_header_t *h = &attr->headers[hi];

        if (h->data_size == 0) {
            continue;
        }
        if (_sf_env_skip_col(env_on, hi, env_count_hi, env_value_hi)) {
            continue;
        }
        if (env_on && hi == env_key_hi) {
            (void)tpbcli_task_csv_cell(&csv, TPB_TASK_HDR_ENV_KEY);
            (void)tpbcli_task_csv_cell(&csv, TPB_TASK_HDR_ENV_VALUE);
            continue;
        }
        (void)tpbcli_task_csv_cell(&csv, h->name);
    }
    (void)tpbcli_task_csv_row_end(&csv);

    for (row = 0; row < nrows; row++) {
        (void)tpbcli_task_csv_row_begin(&csv);
        off = 0;
        for (hi = 0; hi < attr->nheader; hi++) {
            const tpb_meta_header_t *h = &attr->headers[hi];
            size_t esz;
            uint64_t nr;
            char cell[4096];
            int use_byte = 0;

            if (h->data_size == 0) {
                continue;
            }
            if (_sf_env_skip_col(env_on, hi, env_count_hi, env_value_hi)) {
                off += h->data_size;
                continue;
            }
            if (off + h->data_size > datasize) {
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
            }
            if (env_on && hi == env_key_hi) {
                if (row < (uint64_t)env_nkeys) {
                    char keycell[4096];

                    if (_sf_env_key_to_buf(env_kb, (size_t)row, keycell,
                                           sizeof(keycell)) != TPBE_SUCCESS ||
                        _sf_env_value_to_buf(env_counts, env_nkeys, env_vb,
                                             (size_t)row, cell, sizeof(cell)) != TPBE_SUCCESS) {
                        return TPBE_CLI_FAIL;
                    }
                    (void)tpbcli_task_csv_cell(&csv, keycell);
                    (void)tpbcli_task_csv_cell(&csv, cell);
                } else {
                    (void)tpbcli_task_csv_cell(&csv, "");
                    (void)tpbcli_task_csv_cell(&csv, "");
                }
                off += h->data_size;
                continue;
            }
            esz = _sf_elem_size(h);
            (void)_sf_column_rows(h, esz, &nr);
            if (row < nr) {
                const uint8_t *p = base + off + (size_t)(row * esz);
                if (_sf_format_elem(h, p, esz, cell, sizeof(cell),
                                    &use_byte) != TPBE_SUCCESS) {
                    return TPBE_CLI_FAIL;
                }
                if (use_byte) {
                    (void)tpbcli_task_csv_cell_byte(&csv, *p);
                } else {
                    (void)tpbcli_task_csv_cell(&csv, cell);
                }
            } else {
                (void)tpbcli_task_csv_cell(&csv, "");
            }
            off += h->data_size;
        }
        (void)tpbcli_task_csv_row_end(&csv);
    }

    if (fflush(fp) != 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_meta_writer(FILE *fp, void *ctx)
{
    meta_wctx_t *m = (meta_wctx_t *)ctx;
    return _sf_write_meta(fp, m->attr, m->datasize);
}

static int
_sf_data_writer(FILE *fp, void *ctx)
{
    data_wctx_t *d = (data_wctx_t *)ctx;
    return _sf_write_data(fp, d->attr, d->data, d->datasize);
}

static int
_sf_atomic_file(const char *final_path,
                int (*writer)(FILE *fp, void *ctx), void *ctx)
{
    char tmp[PATH_MAX];
    FILE *fp;
    int err;

    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", final_path, (int)getpid());
    fp = fopen(tmp, "w");
    if (fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    err = writer(fp, ctx);
    if (err != TPBE_SUCCESS) {
        fclose(fp);
        unlink(tmp);
        return err;
    }
    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
        fclose(fp);
        unlink(tmp);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    if (fclose(fp) != 0) {
        unlink(tmp);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    if (rename(tmp, final_path) != 0) {
        unlink(tmp);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_export_pair(const char *meta_final, const char *data_final,
                  const task_attr_t *attr, const void *data, uint64_t datasize,
                  int *had_fail_out)
{
    meta_wctx_t mw;
    data_wctx_t dw;
    int err;

    mw.attr = attr;
    mw.datasize = datasize;
    dw.attr = attr;
    dw.data = data;
    dw.datasize = datasize;

    err = _sf_atomic_file(meta_final, _sf_meta_writer, &mw);
    if (err != TPBE_SUCCESS) {
        *had_fail_out = 1;
        return err;
    }
    err = _sf_atomic_file(data_final, _sf_data_writer, &dw);
    if (err != TPBE_SUCCESS) {
        unlink(meta_final);
        *had_fail_out = 1;
        return err;
    }
    return TPBE_SUCCESS;
}

static int
_sf_member_passes(const tpbcli_task_export_opts_t *opts, int is_capsule,
                  int subrank, uint32_t tid)
{
    int f;
    char tidhx[9];

    if (!is_capsule || opts->nfilter == 0) {
        return 1;
    }
    snprintf(tidhx, sizeof(tidhx), "%08x", (unsigned)tid);
    for (f = 0; f < opts->nfilter; f++) {
        const tpbcli_task_export_filter_t *fl = &opts->filters[f];
        int i;
        int matched = 0;

        if (fl->key == TPBCLI_TASK_EXP_FKEY_SUBRANK) {
            for (i = 0; i < fl->nsubrank; i++) {
                if (fl->subrank[i] == subrank) {
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                return 0;
            }
        } else if (fl->key == TPBCLI_TASK_EXP_FKEY_SUBTID) {
            for (i = 0; i < fl->nsubtid; i++) {
                size_t n = strlen(fl->subtid[i]);
                if (strncmp(tidhx, fl->subtid[i], n) == 0) {
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                return 0;
            }
        }
    }
    return 1;
}

static int
_sf_export_one_id(const char *workspace, const char *work_dir,
                  const unsigned char id[20], int is_capsule_name,
                  int *ok_out, int *fail_out)
{
    task_attr_t attr;
    void *data = NULL;
    uint64_t ds = 0;
    char hx[41];
    char host[128];
    char meta_path[PATH_MAX];
    char data_path[PATH_MAX];
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_task(workspace, id, &attr, &data, &ds);
    if (err != TPBE_SUCCESS) {
        tpb_raf_id_to_hex(id, hx);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: export failed for task %s\n", hx);
        (*fail_out)++;
        return err;
    }
    tpb_raf_id_to_hex(id, hx);
    if (is_capsule_name) {
        snprintf(meta_path, sizeof(meta_path), "%s/capsule_%s_meta.csv",
                 work_dir, hx);
        snprintf(data_path, sizeof(data_path), "%s/capsule_%s_data.csv",
                 work_dir, hx);
    } else {
        (void)_sf_hostname_for_task(workspace, attr.tbatch_id, host,
                                    sizeof(host));
        snprintf(meta_path, sizeof(meta_path), "%s/%s_meta_%s.csv", work_dir,
                 host, hx);
        snprintf(data_path, sizeof(data_path), "%s/%s_data_%s.csv", work_dir,
                 host, hx);
    }
    err = _sf_export_pair(meta_path, data_path, &attr, data, ds, fail_out);
    if (err == TPBE_SUCCESS) {
        (*ok_out)++;
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "exported %s\n", hx);
    }
    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(data);
    return err;
}

static int
_sf_export_work(const char *workspace, const char *outroot,
                const tpbcli_task_export_work_t *work,
                const tpbcli_task_export_opts_t *opts, int *ok_out,
                int *fail_out)
{
    char work_dir[PATH_MAX];
    task_attr_t attr;
    void *data = NULL;
    uint64_t ds = 0;
    int err;
    int is_capsule = 0;

    if (opts->nfilter > 0 && work->keep_single_member) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: member filters cannot be used with a single "
                        "kept member export\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    err = _sf_pick_work_dir(outroot, work->root_id, work_dir, sizeof(work_dir));
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    if (work->keep_single_member) {
        return _sf_export_one_id(workspace, work_dir, work->single_member_id, 0,
                               ok_out, fail_out);
    }

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_task(workspace, work->root_id, &attr, &data, &ds);
    if (err != TPBE_SUCCESS) {
        (*fail_out)++;
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    is_capsule = tpbcli_task_confirm_capsule(&attr, data, ds);
    if (opts->nfilter > 0 && !is_capsule) {
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: member filters require a capsule work root\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = _sf_export_one_id(workspace, work_dir, work->root_id, is_capsule,
                            ok_out, fail_out);
    if (err != TPBE_SUCCESS) {
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return err;
    }

    if (is_capsule) {
        unsigned char (*ids)[20] = NULL;
        int nmem = 0;
        int i;
        unsigned char seen[32][20];
        int nseen = 0;

        err = tpbcli_task_read_capsule_members(&attr, data, ds, &ids, &nmem);
        if (err != TPBE_SUCCESS) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            (*fail_out)++;
            return err;
        }
        for (i = 0; i < nmem; i++) {
            task_attr_t ma;
            void *md = NULL;
            uint64_t mds = 0;
            int j;
            int dup = 0;

            memset(&ma, 0, sizeof(ma));
            err = tpb_raf_record_read_task(workspace, ids[i], &ma, &md, &mds);
            if (err != TPBE_SUCCESS) {
                free(ids);
                tpb_raf_free_headers(attr.headers, attr.nheader);
                free(data);
                (*fail_out)++;
                return err;
            }
            if (!_sf_member_passes(opts, 1, i, ma.tid)) {
                tpb_raf_free_headers(ma.headers, ma.nheader);
                free(md);
                continue;
            }
            for (j = 0; j < nseen; j++) {
                if (memcmp(seen[j], ids[i], 20) == 0) {
                    dup = 1;
                    tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                                    TPBLOG_FLAG_DIRECT,
                                    "warning: duplicate member ID at subrank "
                                    "%d; exporting once\n",
                                    i);
                    break;
                }
            }
            if (dup) {
                tpb_raf_free_headers(ma.headers, ma.nheader);
                free(md);
                continue;
            }
            if (nseen < 32) {
                memcpy(seen[nseen++], ids[i], 20);
            }
            err = _sf_export_one_id(workspace, work_dir, ids[i], 0, ok_out,
                                    fail_out);
            tpb_raf_free_headers(ma.headers, ma.nheader);
            free(md);
            if (err != TPBE_SUCCESS) {
                free(ids);
                tpb_raf_free_headers(attr.headers, attr.nheader);
                free(data);
                return err;
            }
        }
        free(ids);
    }

    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(data);
    return TPBE_SUCCESS;
}

/**
 * @brief Implement `tpbcli task export`.
 */
int
tpbcli_task_export(const char *workspace, const tpbcli_task_export_opts_t *opts)
{
    char outroot[PATH_MAX];
    char ridmap[PATH_MAX];
    tpbcli_task_export_work_t *works = NULL;
    int nworks = 0;
    int ok = 0;
    int fail = 0;
    int err;
    int i;
    int final_err = TPBE_SUCCESS;

    if (workspace == NULL || opts == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    err = tpbcli_task_ridmap_path(workspace, ridmap, sizeof(ridmap));
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "RIDMAP: %s\n", ridmap);

    if (opts->outdir != NULL && opts->outdir[0] != '\0') {
        snprintf(outroot, sizeof(outroot), "%s", opts->outdir);
    } else {
        snprintf(outroot, sizeof(outroot), "%s/csv", workspace);
    }

    err = tpbcli_task_export_build_works(workspace, opts, &works, &nworks);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    for (i = 0; i < nworks; i++) {
        err = _sf_export_work(workspace, outroot, &works[i], opts, &ok, &fail);
        if (err != TPBE_SUCCESS) {
            final_err = err;
        }
    }

    tpbcli_task_export_free_works(works, nworks);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "export complete: %d succeeded, %d failed, output %s\n", ok,
                    fail, outroot);
    if (fail > 0 || final_err != TPBE_SUCCESS) {
        if (final_err == TPBE_SUCCESS) {
            final_err = TPBE_CLI_FAIL;
        }
        return final_err;
    }
    return TPBE_SUCCESS;
}
