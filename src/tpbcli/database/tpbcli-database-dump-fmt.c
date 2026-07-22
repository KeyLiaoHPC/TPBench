/*
 * tpbcli-database-dump-fmt.c
 * Human-readable .tpbr dump layout for `tpbcli database dump -i`.
 *
 * Formats Domain/ID banners, on-disk magic lines, equal-width metadata KV rows,
 * decoded type_bits/uattr_bits, and record-data axis slices. Does not alter
 * rafdb files or corelib record I/O.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpbcli-database-dump-fmt.h"
#include "tpbcli-data-fmt.h"
#include "tpb-public.h"

/* Local Function Prototypes */

static int _sf_clamp_key_width(int w);
static int _sf_max_key_width(const tpbcli_dump_kv_row_t *rows, int nrows);
static void _sf_append_decode(char *out, size_t outsz, const char *tok,
                              int *first);
static void _sf_kv_copy_key(tpbcli_dump_kv_row_t *row, const char *key);
static size_t _sf_dtype_elem_size(uint32_t type_bits);
static int _sf_is_string_dtype(uint32_t type_bits);
static void _sf_print_elem(const uint8_t *p, uint32_t type_bits,
                           size_t elem_size);
static void _sf_print_string_cell(const char *s, size_t maxlen);
static void _sf_print_record_slice(const tpb_meta_header_t *h,
                                   const uint8_t *blob, uint32_t nd,
                                   uint64_t mult[TPBM_DATA_NDIM_MAX],
                                   uint64_t idx[TPBM_DATA_NDIM_MAX],
                                   int lev, size_t elem_size,
                                   int *first_line);
static void _sf_print_header_axis_title(const tpb_meta_header_t *h);

static int
_sf_clamp_key_width(int w)
{
    if (w < 8) {
        w = 8;
    }
    if (w > 24) {
        w = 24;
    }
    return w;
}

static int
_sf_max_key_width(const tpbcli_dump_kv_row_t *rows, int nrows)
{
    int maxw = 0;
    int i;

    for (i = 0; i < nrows; i++) {
        int klen = (int)strlen(rows[i].key);
        if (klen > maxw) {
            maxw = klen;
        }
    }
    return _sf_clamp_key_width(maxw);
}

static void
_sf_kv_copy_key(tpbcli_dump_kv_row_t *row, const char *key)
{
    if (row == NULL) {
        return;
    }
    snprintf(row->key, sizeof(row->key), "%s", key ? key : "");
}

static void
_sf_append_decode(char *out, size_t outsz, const char *tok, int *first)
{
    size_t len = strlen(out);

    if (tok == NULL || tok[0] == '\0') {
        return;
    }
    if (len >= outsz - 1) {
        return;
    }
    if (!(*first)) {
        if (len + 3 < outsz) {
            strcat(out, " | ");
        }
    } else {
        *first = 0;
    }
    if (len + strlen(tok) + 1 < outsz) {
        strcat(out, tok);
    }
}

const char *
tpbcli_dump_fmt_domain_name(uint8_t domain)
{
    switch (domain) {
    case TPB_RAF_DOM_TBATCH:
        return "tbatch";
    case TPB_RAF_DOM_KERNEL:
        return "kernel";
    case TPB_RAF_DOM_TASK:
        return "task";
    case TPB_RAF_DOM_RTENV:
        return "runtime_environment";
    default:
        return "unknown";
    }
}

void
tpbcli_dump_fmt_print_domain_banner(uint8_t domain, const char *id_display)
{
    tpblog_print_hline('=');
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Domain: %s\n", tpbcli_dump_fmt_domain_name(domain));
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "ID: %s\n", id_display ? id_display : "");
    tpblog_print_hline('=');
}

void
tpbcli_dump_fmt_print_magic_line(const unsigned char magic[8])
{
    int i;

    if (magic == NULL) {
        return;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "0x");
    for (i = 0; i < TPB_RAF_MAGIC_LEN; i++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   " %02X", (unsigned)magic[i]);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "\n");
}

void
tpbcli_dump_fmt_print_section_banner(const char *section_title)
{
    tpblog_print_hline('=');
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Section: %s\n", section_title ? section_title : "");
    tpblog_print_hline('=');
}

void
tpbcli_dump_fmt_print_subsection(const char *title)
{
    tpblog_print_hline('-');
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s\n", title ? title : "");
    tpblog_print_hline('-');
}

void
tpbcli_dump_fmt_print_kv_block(const tpbcli_dump_kv_row_t *rows, int nrows)
{
    int kw;
    int i;

    if (rows == NULL || nrows <= 0) {
        return;
    }
    kw = _sf_max_key_width(rows, nrows);
    for (i = 0; i < nrows; i++) {
        tpblog_print_kv_eq(rows[i].key, rows[i].value, kw);
    }
}

void
tpbcli_dump_fmt_decode_type_bits(uint32_t type_bits, char *out, size_t outsz)
{
    tpbcli_data_fmt_decode_type_bits(type_bits, out, outsz);
}

void
tpbcli_dump_fmt_decode_uattr_bits(uint64_t uattr_bits, char *out, size_t outsz)
{
    tpbcli_data_fmt_decode_uattr_bits(uattr_bits, out, outsz);
}

void
tpbcli_dump_fmt_kv_set_u64(tpbcli_dump_kv_row_t *row, const char *key,
                           uint64_t v)
{
    _sf_kv_copy_key(row, key);
    snprintf(row->value, sizeof(row->value), "%" PRIu64, v);
}

void
tpbcli_dump_fmt_kv_set_u32(tpbcli_dump_kv_row_t *row, const char *key,
                           uint32_t v)
{
    _sf_kv_copy_key(row, key);
    snprintf(row->value, sizeof(row->value), "%" PRIu32, v);
}

void
tpbcli_dump_fmt_kv_set_i32(tpbcli_dump_kv_row_t *row, const char *key,
                           int32_t v)
{
    _sf_kv_copy_key(row, key);
    snprintf(row->value, sizeof(row->value), "%" PRId32, v);
}

void
tpbcli_dump_fmt_kv_set_str(tpbcli_dump_kv_row_t *row, const char *key,
                           const char *val)
{
    _sf_kv_copy_key(row, key);
    snprintf(row->value, sizeof(row->value), "%s", val ? val : "");
}

void
tpbcli_dump_fmt_kv_set_hex20(tpbcli_dump_kv_row_t *row, const char *key,
                             const unsigned char id[20])
{
    char hex[41];

    _sf_kv_copy_key(row, key);
    tpb_raf_id_to_hex(id, hex);
    snprintf(row->value, sizeof(row->value), "%s", hex);
}

void
tpbcli_dump_fmt_kv_set_build_datetime(tpbcli_dump_kv_row_t *row,
                                      const char *key,
                                      tpb_dtbits_t utc_bits)
{
    tpb_datetime_str_t ts;

    _sf_kv_copy_key(row, key);
    if (utc_bits == 0) {
        snprintf(row->value, sizeof(row->value), "N/A");
        return;
    }
    if (tpb_ts_bits_to_isoutc(utc_bits, &ts) == 0) {
        snprintf(row->value, sizeof(row->value), "%s", ts.str);
    } else {
        snprintf(row->value, sizeof(row->value), "%" PRIu64,
                 (uint64_t)utc_bits);
    }
}

void
tpbcli_dump_fmt_kv_set_type_bits(tpbcli_dump_kv_row_t *row, const char *key,
                                 uint32_t type_bits)
{
    char decode[512];

    _sf_kv_copy_key(row, key);
    tpbcli_dump_fmt_decode_type_bits(type_bits, decode, sizeof(decode));
    snprintf(row->value, sizeof(row->value), "0x%08" PRIX32 " (%s)",
             type_bits, decode);
}

void
tpbcli_dump_fmt_kv_set_uattr_bits(tpbcli_dump_kv_row_t *row, const char *key,
                                  uint64_t uattr_bits)
{
    char decode[768];

    _sf_kv_copy_key(row, key);
    tpbcli_dump_fmt_decode_uattr_bits(uattr_bits, decode, sizeof(decode));
    snprintf(row->value, sizeof(row->value), "0x%016" PRIX64 " (%s)",
             (uint64_t)uattr_bits, decode);
}

void
tpbcli_dump_fmt_print_headers(const tpb_meta_header_t *hdrs, uint32_t n)
{
    uint32_t i;

    if (n == 0 || hdrs == NULL) {
        return;
    }

    tpbcli_dump_fmt_print_subsection("Headers");

    for (i = 0; i < n; i++) {
        const tpb_meta_header_t *h = &hdrs[i];
        tpbcli_dump_kv_row_t rows[12];
        char key[320];
        char dsz_line[512];
        char dnm_line[768];
        uint32_t j;
        int nr = 0;

        snprintf(key, sizeof(key), "header[%" PRIu32 "]", i);
        tpbcli_dump_fmt_print_subsection(key);

        snprintf(key, sizeof(key), "block_size");
        tpbcli_dump_fmt_kv_set_u32(&rows[nr++], key, h->block_size);
        snprintf(key, sizeof(key), "ndim");
        tpbcli_dump_fmt_kv_set_u32(&rows[nr++], key, h->ndim);
        snprintf(key, sizeof(key), "data_size");
        tpbcli_dump_fmt_kv_set_u64(&rows[nr++], key, h->data_size);
        snprintf(key, sizeof(key), "type_bits");
        tpbcli_dump_fmt_kv_set_type_bits(&rows[nr++], key, h->type_bits);
        snprintf(key, sizeof(key), "_reserve");
        tpbcli_dump_fmt_kv_set_u32(&rows[nr++], key, h->_reserve);
        snprintf(key, sizeof(key), "uattr_bits");
        tpbcli_dump_fmt_kv_set_uattr_bits(&rows[nr++], key, h->uattr_bits);
        snprintf(key, sizeof(key), "name");
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], key, h->name);
        snprintf(key, sizeof(key), "tag");
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], key, h->tag);
        snprintf(key, sizeof(key), "note");
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], key, h->note);

        dsz_line[0] = '\0';
        for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
            char part[32];
            snprintf(part, sizeof(part), "%s%" PRIu64,
                     j ? ", " : "", h->dimsizes[j]);
            if (strlen(dsz_line) + strlen(part) + 1 < sizeof(dsz_line)) {
                strcat(dsz_line, part);
            }
        }
        snprintf(key, sizeof(key), "dimsizes[0: %" PRIu32 "]",
                 (uint32_t)(TPBM_DATA_NDIM_MAX - 1));
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], key, dsz_line);

        dnm_line[0] = '\0';
        for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
            char part[80];
            snprintf(part, sizeof(part), "%s'%s'",
                     j ? ", " : "", h->dimnames[j]);
            if (strlen(dnm_line) + strlen(part) + 1 < sizeof(dnm_line)) {
                strcat(dnm_line, part);
            }
        }
        snprintf(key, sizeof(key), "dimnames[0: %" PRIu32 "]",
                 (uint32_t)(TPBM_DATA_NDIM_MAX - 1));
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], key, dnm_line);

        tpbcli_dump_fmt_print_kv_block(rows, nr);
    }
}

static size_t
_sf_dtype_elem_size(uint32_t type_bits)
{
    TPB_DTYPE dt = (TPB_DTYPE)(type_bits & TPB_PARM_TYPE_MASK);
    uint32_t code = (uint32_t)(dt & 0xFFFFu);
    uint32_t nbytes = (code >> 8) & 0xFFu;

    if (nbytes == 0) {
        nbytes = 8;
    }
    return (size_t)nbytes;
}

static int
_sf_is_string_dtype(uint32_t type_bits)
{
    return ((type_bits & TPB_PARM_TYPE_MASK) == TPB_STRING_T);
}

static void
_sf_print_string_cell(const char *s, size_t maxlen)
{
    size_t len = 0;

    if (s == NULL || maxlen == 0) {
        return;
    }
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%.*s", (int)len, s);
}

static void
_sf_print_elem(const uint8_t *p, uint32_t type_bits, size_t elem_size)
{
    TPB_DTYPE dt = (TPB_DTYPE)(type_bits & TPB_PARM_TYPE_MASK);

    if (elem_size == 20u) {
        char hx[41];
        tpb_raf_id_to_hex(p, hx);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%s", hx);
        return;
    }

    switch (dt) {
    case TPB_INT8_T:
    case TPB_UINT8_T:
    case TPB_CHAR_T:
    case TPB_UNSIGNED_CHAR_T:
    case TPB_SIGNED_CHAR_T:
    case TPB_BYTE_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%d", (int)*p);
        break;
    case TPB_INT16_T:
    case TPB_UINT16_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%d", (int)*(const int16_t *)(const void *)p);
        break;
    case TPB_INT32_T:
    case TPB_UINT32_T:
    case TPB_INT_T:
    case TPB_UNSIGNED_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%d", (int)*(const int32_t *)(const void *)p);
        break;
    case TPB_INT64_T:
    case TPB_UINT64_T:
    case TPB_LONG_T:
    case TPB_UNSIGNED_LONG_T:
    case TPB_LONG_LONG_T:
    case TPB_UNSIGNED_LONG_LONG_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%" PRId64, (int64_t)*(const int64_t *)(const void *)p);
        break;
    case TPB_FLOAT_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%.8g", (double)*(const float *)(const void *)p);
        break;
    case TPB_DOUBLE_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%.16g", *(const double *)(const void *)p);
        break;
    case TPB_LONG_DOUBLE_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%.16Lg", (long double)*(const long double *)(const void *)p);
        break;
    default:
        if (elem_size > 0) {
            size_t k;
            for (k = 0; k < elem_size; k++) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT, "%s%02x",
                           k ? "" : "0x", (unsigned)p[k]);
            }
        } else {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                       TPBLOG_FLAG_DIRECT, "0");
        }
        break;
    }
}

static void
_sf_print_header_axis_title(const tpb_meta_header_t *h)
{
    uint32_t nd;
    uint32_t k;
    int any = 0;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s", h->name);
    nd = h->ndim;
    if (nd > TPBM_DATA_NDIM_MAX) {
        nd = TPBM_DATA_NDIM_MAX;
    }
    for (k = nd; k-- > 0u; ) {
        if (h->dimsizes[k] == 0) {
            continue;
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   " [%" PRIu64 "]", h->dimsizes[k]);
        any = 1;
    }
    if (!any && nd > 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   " [%" PRIu64 "]", h->dimsizes[0]);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "\n");
}

static void
_sf_print_record_slice(const tpb_meta_header_t *h, const uint8_t *blob,
                       uint32_t nd, uint64_t mult[TPBM_DATA_NDIM_MAX],
                       uint64_t idx[TPBM_DATA_NDIM_MAX], int lev,
                       size_t elem_size, int *first_line)
{
    if (lev == 0) {
        uint64_t i0;
        uint64_t fix = 0;
        uint32_t k;

        if (!(*first_line)) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                       TPBLOG_FLAG_DIRECT, "\n");
        }
        *first_line = 0;

        for (k = 1; k < nd; k++) {
            fix += idx[k] * mult[k];
        }
        for (k = nd; k-- > 1u; ) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                       TPBLOG_FLAG_DIRECT, "[%" PRIu64 "]", idx[k]);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "[]: ");
        for (i0 = 0; i0 < h->dimsizes[0]; i0++) {
            uint64_t L = fix + i0;
            if (i0 > 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT, ", ");
            }
            if (_sf_is_string_dtype(h->type_bits)) {
                size_t cell_bytes = (size_t)h->dimsizes[0];
                _sf_print_string_cell((const char *)(blob + L * cell_bytes),
                                      cell_bytes);
            } else {
                _sf_print_elem(blob + (size_t)(L * elem_size),
                               h->type_bits, elem_size);
            }
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "\n");
        return;
    }

    {
        uint64_t t;
        uint64_t kmax = h->dimsizes[lev];

        for (t = 0; t < kmax; t++) {
            idx[lev] = t;
            _sf_print_record_slice(h, blob, nd, mult, idx, lev - 1,
                                   elem_size, first_line);
        }
    }
}

void
tpbcli_dump_fmt_print_record_data(const tpb_meta_header_t *hdrs,
                                  uint32_t nheader,
                                  const void *data,
                                  uint64_t datasize)
{
    const uint8_t *base = (const uint8_t *)data;
    uint64_t off = 0;
    uint32_t hi;

    for (hi = 0; hi < nheader; hi++) {
        const tpb_meta_header_t *h = &hdrs[hi];
        uint64_t dsz = h->data_size;
        uint32_t nd = h->ndim;
        uint64_t total_el = 1;
        uint32_t j;
        uint64_t mult[TPBM_DATA_NDIM_MAX];
        size_t elem_size;
        const uint8_t *blob;
        int first_line = 1;

        if (dsz == 0) {
            continue;
        }
        if (off + dsz > datasize) {
            break;
        }
        blob = base + off;
        off += dsz;

        if (nd > TPBM_DATA_NDIM_MAX) {
            nd = TPBM_DATA_NDIM_MAX;
        }

        for (j = 0; j < nd; j++) {
            uint64_t d = h->dimsizes[j];
            if (d == 0) {
                total_el = 0;
                break;
            }
            if (total_el > UINT64_MAX / d) {
                total_el = 0;
                break;
            }
            total_el *= d;
        }
        if (total_el == 0) {
            continue;
        }

        elem_size = (size_t)(dsz / total_el);
        if (elem_size == 0 || (uint64_t)elem_size * total_el != dsz) {
            elem_size = _sf_dtype_elem_size(h->type_bits);
            if (elem_size == 0 || (uint64_t)elem_size * total_el != dsz) {
                elem_size = 1;
            }
        }

        mult[0] = 1u;
        for (j = 1; j < nd; j++) {
            mult[j] = mult[j - 1] * h->dimsizes[j - 1];
        }

        tpblog_print_hline('-');
        _sf_print_header_axis_title(h);
        tpblog_print_hline('-');

        if (_sf_is_string_dtype(h->type_bits)) {
            size_t cell_bytes = (size_t)h->dimsizes[0];

            if (cell_bytes == 0) {
                continue;
            }
            if (nd <= 1) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT, "[]: ");
                _sf_print_string_cell((const char *)blob, (size_t)dsz);
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT, "\n");
                continue;
            }
            {
                uint64_t idx[TPBM_DATA_NDIM_MAX];
                memset(idx, 0, sizeof(idx));
                _sf_print_record_slice(h, blob, nd, mult, idx,
                                       (int)nd - 1, cell_bytes, &first_line);
            }
            continue;
        }

        if (nd <= 1) {
            uint64_t d0 = nd == 0 ? 1u : h->dimsizes[0];
            uint64_t i0;

            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                       TPBLOG_FLAG_DIRECT, "[]: ");
            for (i0 = 0; i0 < d0; i0++) {
                if (i0 > 0) {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                               TPBLOG_FLAG_DIRECT, ", ");
                }
                _sf_print_elem(blob + (size_t)(i0 * elem_size),
                               h->type_bits, elem_size);
            }
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                       TPBLOG_FLAG_DIRECT, "\n");
            continue;
        }

        {
            uint64_t idx[TPBM_DATA_NDIM_MAX];
            memset(idx, 0, sizeof(idx));
            _sf_print_record_slice(h, blob, nd, mult, idx,
                                   (int)nd - 1, elem_size, &first_line);
        }
    }
}

void
tpbcli_dump_fmt_print_file_end(const unsigned char end_magic[8])
{
    tpblog_print_hline('=');
    tpbcli_dump_fmt_print_magic_line(end_magic);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "END OF FILE\n");
    tpblog_print_hline('=');
}
