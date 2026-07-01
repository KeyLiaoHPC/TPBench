/*
 * tpbcli-print-kernel-help.c
 * Formatted kernel help output (parameters, metrics, layout).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-print-kernel-help.h"
#include "tpbcli-kernel-table.h"

#define TPBCLI_KERNEL_LEFT_COL_WIDTH   32
#define TPBCLI_KERNEL_GAP              4
#define TPBCLI_KERNEL_RIGHT_COL_WIDTH  50
#define TPBCLI_KERNEL_TABLE_WIDTH \
    (TPBCLI_KERNEL_LEFT_COL_WIDTH + TPBCLI_KERNEL_GAP + \
     TPBCLI_KERNEL_RIGHT_COL_WIDTH)

/* Local Function Prototypes */

static const char *_sf_dtype_to_str(uint32_t type_bits);
static const char *_sf_parm_source_section(uint32_t type_bits);
static const char *_sf_unit_category_str(uint64_t uattr_bits);
static void _sf_split_metric_name(const char *name, char *tags_out,
                                  size_t tags_len, char *real_out,
                                  size_t real_len);
static void _sf_print_section_line(FILE *out);
static void _sf_print_col_line(FILE *out, const char *left,
                               const char *right);
static void _sf_print_parm_section_header(FILE *out);
static void _sf_print_metric_section_header(FILE *out);
static void _sf_print_parm_entry(FILE *out, const char *name,
                                 const char *note, uint32_t type_bits);
static void _sf_print_metric_entry(FILE *out, const char *full_name,
                                   const char *note, uint64_t uattr_bits);
static int _sf_get_header_payload(const kernel_attr_t *attr,
                                  const void *data, const char *hdr_name,
                                  char *buf, size_t bufsz);
static void _sf_variation_summary(const char *payload, char *out,
                                  size_t outlen);
static void _sf_print_parm_sections_from_bits(FILE *out,
                                              const tpb_meta_header_t *headers,
                                              uint32_t nparm);
static void _sf_print_parm_sections_from_kernel(FILE *out,
                                                const tpb_kernel_t *kernel);
static void _sf_print_metrics_from_headers(FILE *out,
                                           const tpb_meta_header_t *headers,
                                           uint32_t nparm, uint32_t nmetric,
                                           int verbose);
static void _sf_print_metrics_from_kernel(FILE *out,
                                          const tpb_kernel_t *kernel,
                                          int verbose);
static void _sf_print_kernel_header(FILE *out, const char *name,
                                    const char *kernel_id_hex, int active,
                                    const char *version, const char *notes,
                                    const char *variation);

/* Local Function Implementations */

static void
_sf_print_section_line(FILE *out)
{
    int i;

    for (i = 0; i < TPBCLI_KERNEL_TABLE_WIDTH; i++) {
        fputc('-', out);
    }
    fputc('\n', out);
}

static const char *
_sf_dtype_to_str(uint32_t type_bits)
{
    uint32_t tc = type_bits & (uint32_t)TPB_PARM_TYPE_MASK;

    switch (tc) {
    case (uint32_t)(TPB_DOUBLE_T & TPB_PARM_TYPE_MASK):
        return "double";
    case (uint32_t)(TPB_FLOAT_T & TPB_PARM_TYPE_MASK):
        return "float";
    case (uint32_t)(TPB_LONG_DOUBLE_T & TPB_PARM_TYPE_MASK):
        return "long double";
    case (uint32_t)(TPB_INT_T & TPB_PARM_TYPE_MASK):
        return "int";
    case (uint32_t)(TPB_INT32_T & TPB_PARM_TYPE_MASK):
        return "int";
    case (uint32_t)(TPB_INT64_T & TPB_PARM_TYPE_MASK):
        return "long int";
    case (uint32_t)(TPB_LONG_T & TPB_PARM_TYPE_MASK):
        return "long int";
    case (uint32_t)(TPB_LONG_LONG_T & TPB_PARM_TYPE_MASK):
        return "long long int";
    case (uint32_t)(TPB_UNSIGNED_T & TPB_PARM_TYPE_MASK):
        return "unsigned int";
    case (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK):
        return "unsigned int";
    case (uint32_t)(TPB_UINT64_T & TPB_PARM_TYPE_MASK):
        return "unsigned long int";
    case (uint32_t)(TPB_UNSIGNED_LONG_T & TPB_PARM_TYPE_MASK):
        return "unsigned long int";
    case (uint32_t)(TPB_UNSIGNED_LONG_LONG_T & TPB_PARM_TYPE_MASK):
        return "unsigned long long int";
    case (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK):
        return "string";
    case (uint32_t)(TPB_CHAR_T & TPB_PARM_TYPE_MASK):
        return "char";
    default:
        return "unknown";
    }
}

static const char *
_sf_parm_source_section(uint32_t type_bits)
{
    uint32_t src = type_bits & (uint32_t)TPB_PARM_SOURCE_MASK;

    if (src == (uint32_t)(TPB_PARM_CLI & TPB_PARM_SOURCE_MASK)) {
        return "CLI";
    }
    if (src == (uint32_t)(TPB_PARM_ENV & TPB_PARM_SOURCE_MASK)) {
        return "Environment Variables";
    }
    if (src == (uint32_t)(TPB_PARM_MACRO & TPB_PARM_SOURCE_MASK)) {
        return "Macro";
    }
    if (src == (uint32_t)(TPB_PARM_FILE & TPB_PARM_SOURCE_MASK)) {
        return "File";
    }
    if (src == (uint32_t)(TPB_PARM_WRAPPER_CLI & TPB_PARM_SOURCE_MASK)) {
        return "Wrapper CLI";
    }
    return "Other";
}

static const char *
_sf_unit_category_str(uint64_t uattr_bits)
{
    TPB_UNIT_T unit = (TPB_UNIT_T)uattr_bits;
    TPB_UNIT_T uname;

    if (unit == 0 || unit == TPB_UNIT_UNDEF) {
        return "Unspecified unit";
    }

    uname = unit & TPB_UNAME_MASK;
    if (uname == TPB_UNAME_UNDEF) {
        return "Unspecified unit";
    }
    if (uname == TPB_UNAME_WALLTIME) {
        return "Wall time (e.g. ms, s)";
    }
    if (uname == TPB_UNAME_PHYSTIME) {
        return "Physical time (e.g. cy, Mcy)";
    }
    if (uname == TPB_UNAME_DATETIME) {
        return "Date time (e.g. s, min, day)";
    }
    if (uname == TPB_UNAME_TICKTIME) {
        return "Tick time (e.g. tick, Ktick)";
    }
    if (uname == TPB_UNAME_TIMERTIME) {
        return "Timer unit (follows timer)";
    }
    if (uname == TPB_UNAME_DATASIZE) {
        return "Data size (e.g. B, MB, GB)";
    }
    if (uname == TPB_UNAME_OP) {
        return "Operation count (e.g. op, Mop)";
    }
    if (uname == TPB_UNAME_GRIDSIZE) {
        return "Grid size (e.g. grids)";
    }
    if (uname == TPB_UNAME_BITSIZE) {
        return "Binary size (e.g. B, KiB, MiB)";
    }
    if (uname == TPB_UNAME_OPS) {
        return "OPS (e.g. Mop/s, Gop/s)";
    }
    if (uname == TPB_UNAME_FLOPS) {
        return "FLOPS (e.g. GFlop/s, TFlop/s)";
    }
    if (uname == TPB_UNAME_TOKENPS) {
        return "Token rate (e.g. token/s, Mtoken/s)";
    }
    if (uname == TPB_UNAME_TPS) {
        return "Transaction rate (e.g. TPS, MTPS)";
    }
    if (uname == TPB_UNAME_BITPS) {
        return "Bit throughput (e.g. MiB/s, GiB/s)";
    }
    if (uname == TPB_UNAME_DATAPS) {
        return "Data throughput (e.g. MB/s, GB/s)";
    }
    if (uname == TPB_UNAME_BITPCY) {
        return "Bit per cycle (e.g. Byte/cy, KiB/cy)";
    }
    if (uname == TPB_UNAME_DATAPCY) {
        return "Data per cycle (e.g. MB/cy, GB/cy)";
    }
    if (uname == TPB_UNAME_BITPTICK) {
        return "Bit per tick (e.g. Byte/tick, KiB/tick)";
    }
    if (uname == TPB_UNAME_DATAPTICK) {
        return "Data per tick (e.g. KB/tick, MB/tick)";
    }
    return "Unspecified unit";
}

static void
_sf_split_metric_name(const char *name, char *tags_out, size_t tags_len,
                      char *real_out, size_t real_len)
{
    const char *sep;

    if (tags_out != NULL && tags_len > 0) {
        tags_out[0] = '\0';
    }
    if (real_out != NULL && real_len > 0) {
        real_out[0] = '\0';
    }
    if (name == NULL) {
        return;
    }

    sep = strstr(name, "::");
    if (sep == NULL) {
        if (real_out != NULL && real_len > 0) {
            snprintf(real_out, real_len, "%s", name);
        }
        return;
    }

    if (tags_out != NULL && tags_len > 0) {
        size_t tlen = (size_t)(sep - name);
        if (tlen >= tags_len) {
            tlen = tags_len - 1;
        }
        memcpy(tags_out, name, tlen);
        tags_out[tlen] = '\0';
    }
    if (real_out != NULL && real_len > 0) {
        snprintf(real_out, real_len, "%s", sep + 2);
    }
}

static void
_sf_print_col_line(FILE *out, const char *left, const char *right)
{
    const char *cells[2];
    int widths[2] = {TPBCLI_KERNEL_LEFT_COL_WIDTH,
                     TPBCLI_KERNEL_RIGHT_COL_WIDTH};

    cells[0] = (left != NULL) ? left : "";
    cells[1] = (right != NULL) ? right : "";
    tpbcli_kernel_table_print_row(out, cells, widths, 2,
                                  TPBCLI_KERNEL_GAP);
}

static void
_sf_print_wrapped_note(FILE *out, const char *text)
{
    const char *cells[2];
    int widths[2] = {TPBCLI_KERNEL_LEFT_COL_WIDTH,
                     TPBCLI_KERNEL_RIGHT_COL_WIDTH};

    cells[0] = "";
    cells[1] = (text != NULL) ? text : "";
    tpbcli_kernel_table_print_row(out, cells, widths, 2,
                                  TPBCLI_KERNEL_GAP);
}

static void
_sf_print_parm_section_header(FILE *out)
{
    _sf_print_col_line(out, "Name", "Type/Description");
}

static void
_sf_print_metric_section_header(FILE *out)
{
    _sf_print_col_line(out, "Name", "Tags/Unit/Description");
}

static void
_sf_print_parm_entry(FILE *out, const char *name, const char *note,
                     uint32_t type_bits)
{
    _sf_print_col_line(out, name, _sf_dtype_to_str(type_bits));
    _sf_print_wrapped_note(out, note);
}

static void
_sf_print_metric_entry(FILE *out, const char *full_name, const char *note,
                       uint64_t uattr_bits)
{
    char tags[TPBM_NAME_STR_MAX_LEN];
    char real_name[TPBM_NAME_STR_MAX_LEN];
    const char *tag_line;

    _sf_split_metric_name(full_name, tags, sizeof(tags),
                          real_name, sizeof(real_name));
    if (real_name[0] == '\0') {
        snprintf(real_name, sizeof(real_name), "%s",
                 (full_name != NULL) ? full_name : "");
    }
    tag_line = (tags[0] != '\0') ? tags : "-";
    _sf_print_col_line(out, real_name, tag_line);
    _sf_print_wrapped_note(out, _sf_unit_category_str(uattr_bits));
    _sf_print_wrapped_note(out, note);
}

static int
_sf_get_header_payload(const kernel_attr_t *attr, const void *data,
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
    if (attr == NULL) {
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

static void
_sf_variation_summary(const char *payload, char *out, size_t outlen)
{
    const char *line;
    const char *p;

    if (out == NULL || outlen == 0) {
        return;
    }
    out[0] = '\0';
    if (payload == NULL || payload[0] == '\0') {
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

static void
_sf_print_kernel_header(FILE *out, const char *name,
                        const char *kernel_id_hex, int active,
                        const char *version, const char *notes,
                        const char *variation)
{
    if (version == NULL || version[0] == '\0') {
        version = "-";
    }
    if (notes == NULL) {
        notes = "";
    }
    if (variation == NULL || variation[0] == '\0') {
        variation = "-";
    }

    fprintf(out, "Kernel: %s (%s)\n", name, kernel_id_hex);
    _sf_print_section_line(out);
    fprintf(out, "Active: %d\n", active);
    fprintf(out, "Version: %s\n", version);
    fprintf(out, "Notes: %s\n", notes);
    fprintf(out, "Variation: %s\n", variation);
    _sf_print_section_line(out);
}

static void
_sf_print_parm_sections_from_bits(FILE *out,
                                  const tpb_meta_header_t *headers,
                                  uint32_t nparm)
{
    static const char *order[] = {
        "CLI",
        "Environment Variables",
        "Macro",
        "File",
        "Wrapper CLI",
        "Other"
    };
    uint32_t s;
    uint32_t i;
    int has_any;

    for (s = 0; s < sizeof(order) / sizeof(order[0]); s++) {
        has_any = 0;
        for (i = 0; i < nparm; i++) {
            if (strcmp(_sf_parm_source_section(headers[i].type_bits),
                       order[s]) != 0) {
                continue;
            }
            if (!has_any) {
                fprintf(out, "Parameters::%s\n", order[s]);
                fprintf(out, "---\n");
                _sf_print_parm_section_header(out);
                has_any = 1;
            }
            _sf_print_parm_entry(out, headers[i].name, headers[i].note,
                                 headers[i].type_bits);
        }
        if (has_any) {
            _sf_print_section_line(out);
        }
    }
}

static void
_sf_print_parm_sections_from_kernel(FILE *out, const tpb_kernel_t *kernel)
{
    static const char *order[] = {
        "CLI",
        "Environment Variables",
        "Macro",
        "File",
        "Wrapper CLI",
        "Other"
    };
    uint32_t s;
    int i;
    int has_any;

    if (kernel == NULL) {
        return;
    }
    for (s = 0; s < sizeof(order) / sizeof(order[0]); s++) {
        has_any = 0;
        for (i = 0; i < kernel->info.nparms; i++) {
            tpb_rt_parm_t *p = &kernel->info.parms[i];
            if (strcmp(_sf_parm_source_section((uint32_t)p->ctrlbits),
                       order[s]) != 0) {
                continue;
            }
            if (!has_any) {
                fprintf(out, "Parameters::%s\n", order[s]);
                fprintf(out, "---\n");
                _sf_print_parm_section_header(out);
                has_any = 1;
            }
            _sf_print_parm_entry(out, p->name, p->note,
                                 (uint32_t)p->ctrlbits);
        }
        if (has_any) {
            _sf_print_section_line(out);
        }
    }
}

static void
_sf_print_metrics_from_headers(FILE *out,
                               const tpb_meta_header_t *headers,
                               uint32_t nparm, uint32_t nmetric,
                               int verbose)
{
    uint32_t i;

    if (nmetric == 0) {
        return;
    }
    if (verbose) {
        fprintf(out, "Metrics\n");
        fprintf(out, "---\n");
        _sf_print_metric_section_header(out);
        for (i = 0; i < nmetric; i++) {
            _sf_print_metric_entry(out, headers[nparm + i].name,
                                   headers[nparm + i].note,
                                   headers[nparm + i].uattr_bits);
        }
        _sf_print_section_line(out);
        return;
    }

    for (i = 0; i < nmetric; i++) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s\n", headers[nparm + i].name);
    }
}

static void
_sf_print_metrics_from_kernel(FILE *out, const tpb_kernel_t *kernel,
                              int verbose)
{
    int i;

    if (kernel == NULL || kernel->info.nouts <= 0) {
        return;
    }
    if (verbose) {
        fprintf(out, "Metrics\n");
        fprintf(out, "---\n");
        _sf_print_metric_section_header(out);
        for (i = 0; i < kernel->info.nouts; i++) {
            tpb_k_output_t *o = &kernel->info.outs[i];
            _sf_print_metric_entry(out, o->name, o->note, (uint64_t)o->unit);
        }
        _sf_print_section_line(out);
        return;
    }

    for (i = 0; i < kernel->info.nouts; i++) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s\n", kernel->info.outs[i].name);
    }
}

void
tpbcli_print_kernel_help_from_kernel(FILE *out, const tpb_kernel_t *kernel,
                                     const char *kernel_id_hex, int active)
{
    char workspace[PATH_MAX];
    char kid_hex[41];
    char payload[4096];
    char variation[512];
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    const char *version = "-";
    const char *notes = "";

    if (out == NULL || kernel == NULL) {
        return;
    }

    if (kernel_id_hex != NULL && kernel_id_hex[0] != '\0') {
        snprintf(kid_hex, sizeof(kid_hex), "%s", kernel_id_hex);
    } else {
        tpb_raf_id_to_hex(kernel->info.kernel_id, kid_hex);
    }

    notes = kernel->info.note;
    variation[0] = '\0';
    if (tpb_raf_resolve_workspace(workspace, sizeof(workspace)) ==
        TPBE_SUCCESS) {
        memset(&attr, 0, sizeof(attr));
        if (tpb_raf_record_read_kernel(workspace, kernel->info.kernel_id,
                                       &attr, &data, &datasize) ==
            TPBE_SUCCESS) {
            if (attr.version[0] != '\0') {
                version = attr.version;
            }
            if (attr.description[0] != '\0') {
                notes = attr.description;
            }
            if (_sf_get_header_payload(&attr, data,
                                       TPB_RAF_KERNEL_HDR_VARIATION,
                                       payload, sizeof(payload)) ==
                TPBE_SUCCESS) {
                _sf_variation_summary(payload, variation, sizeof(variation));
            }
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
        }
    }

    _sf_print_kernel_header(out, kernel->info.name, kid_hex, active,
                            version, notes, variation);
    _sf_print_parm_sections_from_kernel(out, kernel);
    _sf_print_metrics_from_kernel(out, kernel, 1);
}

void
tpbcli_print_kernel_help_from_attr(FILE *out, const kernel_attr_t *attr,
                                   const void *data, uint64_t datasize)
{
    char kid_hex[41];
    char payload[4096];
    char variation[512];

    if (out == NULL || attr == NULL) {
        return;
    }

    tpb_raf_id_to_hex(attr->kernel_id, kid_hex);
    variation[0] = '\0';
    if (_sf_get_header_payload(attr, data, TPB_RAF_KERNEL_HDR_VARIATION,
                               payload, sizeof(payload)) == TPBE_SUCCESS) {
        _sf_variation_summary(payload, variation, sizeof(variation));
    }

    _sf_print_kernel_header(out, attr->kernel_name, kid_hex,
                            (int)attr->active,
                            attr->version, attr->description, variation);
    _sf_print_parm_sections_from_bits(out, attr->headers, attr->nparm);
    _sf_print_metrics_from_headers(out, attr->headers, attr->nparm,
                                   attr->nmetric, 1);
    (void)datasize;
}

void
tpbcli_print_kernel_names_from_attr(const kernel_attr_t *attr)
{
    uint32_t i;

    if (attr == NULL) {
        return;
    }
    for (i = 0; i < attr->nparm; i++) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s\n", attr->headers[i].name);
    }
    _sf_print_metrics_from_headers(stdout, attr->headers, attr->nparm,
                                   attr->nmetric, 0);
}
