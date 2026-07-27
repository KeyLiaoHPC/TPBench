/*
 * tpbcli-print-kernel-help.c
 * Formatted kernel help output (parameters, data records, layout).
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

#define TPBCLI_KERNEL_LEFT_COL_WIDTH   32
#define TPBCLI_KERNEL_GAP              4
#define TPBCLI_KERNEL_RIGHT_COL_WIDTH  50
#define TPBCLI_KERNEL_TABLE_WIDTH \
    (TPBCLI_KERNEL_LEFT_COL_WIDTH + TPBCLI_KERNEL_GAP + \
     TPBCLI_KERNEL_RIGHT_COL_WIDTH)

static const float s_col_ratios[] = {
    (float)TPBCLI_KERNEL_LEFT_COL_WIDTH,
    (float)TPBCLI_KERNEL_RIGHT_COL_WIDTH
};

/* Local Function Prototypes */

static const char *_sf_dtype_to_str(uint32_t type_bits);
static int _sf_has_preset_data_tag(const char *tag);
static const char *_sf_parm_source_section(uint32_t type_bits);
static void _sf_print_col_line(const char *left, const char *right);
static void _sf_print_data_record_entry(const char *name, const char *tag,
                                        const char *note, uint32_t type_bits,
                                        uint64_t uattr_bits, int has_unit);
static void _sf_print_data_records_from_headers(const tpb_meta_header_t *headers,
                                                uint32_t narg,
                                                uint32_t nmetric);
static void _sf_print_data_records_from_kernel(const tpb_kernel_t *kernel);
static void _sf_print_kernel_header(const char *name, const char *kernel_id_hex,
                                    int active, const char *version,
                                    const char *notes, const char *variation);
static void _sf_print_metric_names_from_headers(const tpb_meta_header_t *headers,
                                                uint32_t narg,
                                                uint32_t nmetric);
static void _sf_print_parm_entry(const char *name, const char *note,
                                 uint32_t type_bits);
static void _sf_print_parm_section_header(void);
static void _sf_print_parm_sections_from_bits(const tpb_meta_header_t *headers,
                                              uint32_t narg);
static void _sf_print_parm_sections_from_kernel(const tpb_kernel_t *kernel);
static void _sf_print_section_line(void);
static void _sf_print_wrapped_note(const char *text);
static int _sf_tag_has_token(const char *tag, const char *token);
static const char *_sf_unit_category_str(uint64_t uattr_bits);

/* Local Function Implementations */

/*
 * Exact comma-token match on a stored (no-space) or display tag string.
 * Returns 1 when token appears as a whole token; never matches substrings.
 */
static int
_sf_tag_has_token(const char *tag, const char *token)
{
    const char *p;
    size_t tlen;

    if (tag == NULL || token == NULL || token[0] == '\0') {
        return 0;
    }
    tlen = strlen(token);
    p = tag;
    while (*p != '\0') {
        const char *start;
        size_t len;

        while (*p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ',') {
            p++;
        }
        len = (size_t)(p - start);
        while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) {
            len--;
        }
        if (len == tlen && memcmp(start, token, tlen) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * True when the tag carries at least one of the six preset data-record roles.
 * Used to decide which schema entries appear in the Data Records table.
 */
static int
_sf_has_preset_data_tag(const char *tag)
{
    return _sf_tag_has_token(tag, TPB_TAG_INPUT) ||
           _sf_tag_has_token(tag, TPB_TAG_ENVVAR) ||
           _sf_tag_has_token(tag, TPB_TAG_OUTPUT) ||
           _sf_tag_has_token(tag, TPB_TAG_FOM) ||
           _sf_tag_has_token(tag, TPB_TAG_VERIFYVAR) ||
           _sf_tag_has_token(tag, TPB_TAG_LINK);
}

static void
_sf_print_section_line(void)
{
    char line[TPBCLI_KERNEL_TABLE_WIDTH + 2];
    int i;

    for (i = 0; i < TPBCLI_KERNEL_TABLE_WIDTH; i++) {
        line[i] = '-';
    }
    line[i] = '\0';
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "%s\n", line);
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
_sf_print_col_line(const char *left, const char *right)
{
    const char *cells[2];

    cells[0] = (left != NULL) ? left : "";
    cells[1] = (right != NULL) ? right : "";
    tpblog_printf_c(s_col_ratios, 2, TPBCLI_KERNEL_GAP, cells);
}

static void
_sf_print_wrapped_note(const char *text)
{
    const char *cells[2];

    cells[0] = "";
    cells[1] = (text != NULL) ? text : "";
    tpblog_printf_c(s_col_ratios, 2, TPBCLI_KERNEL_GAP, cells);
}

static void
_sf_print_parm_section_header(void)
{
    _sf_print_col_line("Name", "Type/Description");
}

/*
 * Print one Data Records row: Name | Tags, then Type, Unit, Description.
 * has_unit=0 for arguments (no recorded unit); 1 for outputs.
 */
static void
_sf_print_data_record_entry(const char *name, const char *tag,
                            const char *note, uint32_t type_bits,
                            uint64_t uattr_bits, int has_unit)
{
    char tags_disp[TPBM_NAME_STR_MAX_LEN * 2];
    const char *tag_line;
    char type_line[96];

    if (name == NULL) {
        name = "";
    }
    tags_disp[0] = '\0';
    if (tag != NULL && tag[0] != '\0') {
        const char *p = tag;
        size_t out = 0;
        int first = 1;

        while (*p != '\0') {
            const char *start;
            size_t tlen;

            while (*p == ',') {
                p++;
            }
            if (*p == '\0') {
                break;
            }
            start = p;
            while (*p != '\0' && *p != ',') {
                p++;
            }
            tlen = (size_t)(p - start);
            if (!first && out + 2 < sizeof(tags_disp)) {
                tags_disp[out++] = ',';
                tags_disp[out++] = ' ';
            }
            if (out + tlen >= sizeof(tags_disp)) {
                break;
            }
            memcpy(tags_disp + out, start, tlen);
            out += tlen;
            first = 0;
        }
        tags_disp[out] = '\0';
    }
    tag_line = (tags_disp[0] != '\0') ? tags_disp : "-";
    _sf_print_col_line(name, tag_line);

    snprintf(type_line, sizeof(type_line), "Type: %s",
             _sf_dtype_to_str(type_bits));
    _sf_print_wrapped_note(type_line);
    if (has_unit) {
        _sf_print_wrapped_note(_sf_unit_category_str(uattr_bits));
    } else {
        _sf_print_wrapped_note("Unspecified unit");
    }
    _sf_print_wrapped_note(note);
}

static void
_sf_print_parm_entry(const char *name, const char *note, uint32_t type_bits)
{
    _sf_print_col_line(name, _sf_dtype_to_str(type_bits));
    _sf_print_wrapped_note(note);
}

static void
_sf_print_kernel_header(const char *name, const char *kernel_id_hex, int active,
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

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Kernel: %s (%s)\n", name, kernel_id_hex);
    _sf_print_section_line();
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Active: %d\n", active);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Version: %s\n", version);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Notes: %s\n", notes);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Variation: %s\n", variation);
    _sf_print_section_line();
}

static void
_sf_print_parm_sections_from_bits(const tpb_meta_header_t *headers,
                                  uint32_t narg)
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
        for (i = 0; i < narg; i++) {
            if (strcmp(_sf_parm_source_section(headers[i].type_bits),
                       order[s]) != 0) {
                continue;
            }
            if (!has_any) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT,
                                "Parameters::%s\n---\n", order[s]);
                _sf_print_parm_section_header();
                has_any = 1;
            }
            _sf_print_parm_entry(headers[i].name, headers[i].note,
                                 headers[i].type_bits);
        }
        if (has_any) {
            _sf_print_section_line();
        }
    }
}

static void
_sf_print_parm_sections_from_kernel(const tpb_kernel_t *kernel)
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
        for (i = 0; i < kernel->info.nargs; i++) {
            tpb_rt_arg_t *p = &kernel->info.args[i];
            if (strcmp(_sf_parm_source_section((uint32_t)p->ctrlbits),
                       order[s]) != 0) {
                continue;
            }
            if (!has_any) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT,
                                "Parameters::%s\n---\n", order[s]);
                _sf_print_parm_section_header();
                has_any = 1;
            }
            _sf_print_parm_entry(p->name, p->note, (uint32_t)p->ctrlbits);
        }
        if (has_any) {
            _sf_print_section_line();
        }
    }
}

/*
 * Single Data Records table over args + metrics from a kernel .tpbr.
 * Each definition appears once; empty categories are omitted entirely.
 */
static void
_sf_print_data_records_from_headers(const tpb_meta_header_t *headers,
                                    uint32_t narg, uint32_t nmetric)
{
    uint32_t i;
    uint32_t total;
    int has_any = 0;

    if (headers == NULL) {
        return;
    }
    total = narg + nmetric;
    for (i = 0; i < total; i++) {
        if (!_sf_has_preset_data_tag(headers[i].tag)) {
            continue;
        }
        if (!has_any) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT, "Data Records\n---\n");
            _sf_print_col_line("Name", "Tags/Type/Unit/Description");
            has_any = 1;
        }
        _sf_print_data_record_entry(headers[i].name, headers[i].tag,
                                    headers[i].note, headers[i].type_bits,
                                    headers[i].uattr_bits,
                                    (i >= narg) ? 1 : 0);
    }
    if (has_any) {
        _sf_print_section_line();
    }
}

/*
 * Same Data Records layout from an in-memory registered kernel
 * (used by run --kernel <name> --help).
 */
static void
_sf_print_data_records_from_kernel(const tpb_kernel_t *kernel)
{
    int i;
    int has_any = 0;

    if (kernel == NULL) {
        return;
    }
    for (i = 0; i < kernel->info.nargs; i++) {
        tpb_rt_arg_t *p = &kernel->info.args[i];

        if (!_sf_has_preset_data_tag(p->tag)) {
            continue;
        }
        if (!has_any) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT, "Data Records\n---\n");
            _sf_print_col_line("Name", "Tags/Type/Unit/Description");
            has_any = 1;
        }
        _sf_print_data_record_entry(p->name, p->tag, p->note,
                                    (uint32_t)p->ctrlbits, 0, 0);
    }
    for (i = 0; i < kernel->info.nouts; i++) {
        tpb_k_output_t *o = &kernel->info.outs[i];

        if (!_sf_has_preset_data_tag(o->tag)) {
            continue;
        }
        if (!has_any) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT, "Data Records\n---\n");
            _sf_print_col_line("Name", "Tags/Type/Unit/Description");
            has_any = 1;
        }
        _sf_print_data_record_entry(o->name, o->tag, o->note,
                                    (uint32_t)o->dtype, (uint64_t)o->unit, 1);
    }
    if (has_any) {
        _sf_print_section_line();
    }
}

static void
_sf_print_metric_names_from_headers(const tpb_meta_header_t *headers,
                                    uint32_t narg, uint32_t nmetric)
{
    uint32_t i;

    for (i = 0; i < nmetric; i++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "%s\n", headers[narg + i].name);
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

    if (kernel == NULL) {
        return;
    }
    (void)out;

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
            if (tpb_raf_kernel_get_header_payload(&attr, data,
                    TPB_RAF_KERNEL_HDR_VARIATION,
                    payload, sizeof(payload)) == TPBE_SUCCESS) {
                tpb_raf_kernel_variation_summary(payload, variation,
                                                 sizeof(variation));
            }
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
        }
    }

    _sf_print_kernel_header(kernel->info.name, kid_hex, active,
                            version, notes, variation);
    _sf_print_parm_sections_from_kernel(kernel);
    _sf_print_data_records_from_kernel(kernel);
}

void
tpbcli_print_kernel_help_from_attr(FILE *out, const kernel_attr_t *attr,
                                   const void *data, uint64_t datasize)
{
    char kid_hex[41];
    char payload[4096];
    char variation[512];

    if (attr == NULL) {
        return;
    }
    (void)out;

    tpb_raf_id_to_hex(attr->kernel_id, kid_hex);
    variation[0] = '\0';
    if (tpb_raf_kernel_get_header_payload(attr, data,
            TPB_RAF_KERNEL_HDR_VARIATION, payload,
            sizeof(payload)) == TPBE_SUCCESS) {
        tpb_raf_kernel_variation_summary(payload, variation,
                                         sizeof(variation));
    }

    _sf_print_kernel_header(attr->kernel_name, kid_hex, (int)attr->active,
                            attr->version, attr->description, variation);
    _sf_print_parm_sections_from_bits(attr->headers, attr->narg);
    _sf_print_data_records_from_headers(attr->headers, attr->narg,
                                        attr->nmetric);
    (void)datasize;
}

void
tpbcli_print_kernel_names_from_attr(const kernel_attr_t *attr)
{
    uint32_t i;

    if (attr == NULL) {
        return;
    }
    for (i = 0; i < attr->narg; i++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "%s\n", attr->headers[i].name);
    }
    _sf_print_metric_names_from_headers(attr->headers, attr->narg,
                                        attr->nmetric);
}
