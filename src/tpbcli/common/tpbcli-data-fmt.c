/*
 * tpbcli-data-fmt.c
 * Shared dtype/unit decode helpers for tpbcli formatters.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "tpbcli-data-fmt.h"

/* Local Function Prototypes */
static void _sf_append_decode(char *out, size_t outsz, const char *tok,
                              int *first);

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
tpbcli_data_fmt_dtype_short(uint32_t type_bits)
{
    uint32_t tc = type_bits & (uint32_t)TPB_PARM_TYPE_MASK;

    switch (tc) {
    case (uint32_t)(TPB_DOUBLE_T & TPB_PARM_TYPE_MASK):
        return "double";
    case (uint32_t)(TPB_FLOAT_T & TPB_PARM_TYPE_MASK):
        return "float";
    case (uint32_t)(TPB_LONG_DOUBLE_T & TPB_PARM_TYPE_MASK):
        return "long double";
    case (uint32_t)(TPB_INT8_T & TPB_PARM_TYPE_MASK):
        return "int8";
    case (uint32_t)(TPB_UINT8_T & TPB_PARM_TYPE_MASK):
        return "uint8";
    case (uint32_t)(TPB_INT16_T & TPB_PARM_TYPE_MASK):
        return "int16";
    case (uint32_t)(TPB_UINT16_T & TPB_PARM_TYPE_MASK):
        return "uint16";
    case (uint32_t)(TPB_INT32_T & TPB_PARM_TYPE_MASK):
        return "int32";
    case (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK):
        return "uint32";
    case (uint32_t)(TPB_INT64_T & TPB_PARM_TYPE_MASK):
        return "int64";
    case (uint32_t)(TPB_UINT64_T & TPB_PARM_TYPE_MASK):
        return "uint64";
    case (uint32_t)(TPB_INT_T & TPB_PARM_TYPE_MASK):
        return "int";
    case (uint32_t)(TPB_UNSIGNED_T & TPB_PARM_TYPE_MASK):
        return "unsigned int";
    case (uint32_t)(TPB_LONG_T & TPB_PARM_TYPE_MASK):
        return "long int";
    case (uint32_t)(TPB_UNSIGNED_LONG_T & TPB_PARM_TYPE_MASK):
        return "unsigned long int";
    case (uint32_t)(TPB_LONG_LONG_T & TPB_PARM_TYPE_MASK):
        return "long long int";
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

typedef struct tpb_unit_map_entry {
    TPB_UNIT_T    value;
    const char   *name;
} tpb_unit_map_entry_t;

static const tpb_unit_map_entry_t s_unit_map[] = {
    { TPB_UNIT_BIT, "TPB_UNIT_BIT" },
    { TPB_UNIT_BYTE, "TPB_UNIT_BYTE" },
    { TPB_UNIT_KIB, "TPB_UNIT_KIB" },
    { TPB_UNIT_MIB, "TPB_UNIT_MIB" },
    { TPB_UNIT_GIB, "TPB_UNIT_GIB" },
    { TPB_UNIT_B, "TPB_UNIT_B" },
    { TPB_UNIT_KB, "TPB_UNIT_KB" },
    { TPB_UNIT_MB, "TPB_UNIT_MB" },
    { TPB_UNIT_GB, "TPB_UNIT_GB" },
    { TPB_UNIT_NS, "TPB_UNIT_NS" },
    { TPB_UNIT_US, "TPB_UNIT_US" },
    { TPB_UNIT_MS, "TPB_UNIT_MS" },
    { TPB_UNIT_SS, "TPB_UNIT_SS" },
    { TPB_UNIT_CY, "TPB_UNIT_CY" },
    { TPB_UNIT_TICK, "TPB_UNIT_TICK" },
    { TPB_UNIT_TIMER, "TPB_UNIT_TIMER" },
    { TPB_UNIT_OP, "TPB_UNIT_OP" },
    { TPB_UNIT_OPS, "TPB_UNIT_OPS" },
    { TPB_UNIT_FLOPS, "TPB_UNIT_FLOPS" },
    { TPB_UNIT_MBPS, "TPB_UNIT_MBPS" },
    { TPB_UNIT_UNDEF, "TPB_UNIT_UNDEF" },
};

const char *
tpbcli_data_fmt_unit_macro(uint64_t uattr_bits)
{
    size_t i;
    /* Strip attribution bits only; unit name/base live above TPB_UNIT_MASK. */
    TPB_UNIT_T unit = (TPB_UNIT_T)(uattr_bits & ~TPB_UATTR_MASK);

    for (i = 0; i < sizeof(s_unit_map) / sizeof(s_unit_map[0]); i++) {
        if (s_unit_map[i].value == unit) {
            return s_unit_map[i].name;
        }
    }
    return NULL;
}

void
tpbcli_data_fmt_format_unit(uint64_t uattr_bits, char *out, size_t outlen)
{
    const char *macro;

    if (out == NULL || outlen == 0) {
        return;
    }
    macro = tpbcli_data_fmt_unit_macro(uattr_bits);
    if (macro != NULL) {
        snprintf(out, outlen, "%s", macro);
    } else {
        snprintf(out, outlen, "UVAL=0x%016" PRIx64,
                 (uint64_t)(uattr_bits & ~TPB_UATTR_MASK));
    }
}

void
tpbcli_data_fmt_decode_type_bits(uint32_t type_bits, char *out, size_t outsz)
{
    uint32_t src;
    uint32_t chk;
    uint32_t typ;
    int first = 1;

    if (!out || outsz == 0) {
        return;
    }
    out[0] = '\0';

    src = type_bits & (uint32_t)TPB_PARM_SOURCE_MASK;
    if (src == (uint32_t)TPB_PARM_CLI) {
        _sf_append_decode(out, outsz, "TPB_PARM_CLI", &first);
    } else if (src == (uint32_t)TPB_PARM_MACRO) {
        _sf_append_decode(out, outsz, "TPB_PARM_MACRO", &first);
    } else if (src == (uint32_t)TPB_PARM_WRAPPER_CLI) {
        _sf_append_decode(out, outsz, "TPB_PARM_WRAPPER_CLI", &first);
    } else if (src == (uint32_t)TPB_PARM_FILE) {
        _sf_append_decode(out, outsz, "TPB_PARM_FILE", &first);
    } else if (src == (uint32_t)TPB_PARM_ENV) {
        _sf_append_decode(out, outsz, "TPB_PARM_ENV", &first);
    } else if (src != 0) {
        char unk[48];
        snprintf(unk, sizeof(unk), "UNKNOWN_SOURCE_0x%02X",
                 (unsigned)(src >> 24));
        _sf_append_decode(out, outsz, unk, &first);
    }

    chk = type_bits & (uint32_t)TPB_PARM_CHECK_MASK;
    if (chk == (uint32_t)TPB_PARM_NOCHECK || chk == 0) {
        _sf_append_decode(out, outsz, "TPB_PARM_NOCHECK", &first);
    } else {
        if (chk & (uint32_t)TPB_PARM_RANGE) {
            _sf_append_decode(out, outsz, "TPB_PARM_RANGE", &first);
        }
        if (chk & (uint32_t)TPB_PARM_LIST) {
            _sf_append_decode(out, outsz, "TPB_PARM_LIST", &first);
        }
        if (chk & (uint32_t)TPB_PARM_CUSTOM) {
            _sf_append_decode(out, outsz, "TPB_PARM_CUSTOM", &first);
        }
    }

    typ = type_bits & (uint32_t)TPB_PARM_TYPE_MASK;
#define TPB_DATAFMT_MATCH_DTYPE(name)                                   \
    do { if (typ == (uint32_t)(name)) {                                 \
        _sf_append_decode(out, outsz, #name, &first);                   \
    } } while (0)
    TPB_DATAFMT_MATCH_DTYPE(TPB_CHAR_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UNSIGNED_CHAR_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_BYTE_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_PACKED_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_SIGNED_CHAR_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_INT8_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UINT8_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_SHORT_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UNSIGNED_SHORT_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_INT16_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UINT16_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_INT_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UNSIGNED_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_FLOAT_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_WCHAR_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_INT32_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UINT32_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_LONG_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UNSIGNED_LONG_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_LONG_LONG_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UNSIGNED_LONG_LONG_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_DOUBLE_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_C_FLOAT_COMPLEX_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_INT64_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_UINT64_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_DTYPE_TIMER_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_STRING_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_LONG_DOUBLE_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_C_DOUBLE_COMPLEX_T);
    TPB_DATAFMT_MATCH_DTYPE(TPB_C_LONG_DOUBLE_COMPLEX_T);
#undef TPB_DATAFMT_MATCH_DTYPE

    if (first) {
        snprintf(out, outsz, "UNKNOWN_TYPE_0x%04X", (unsigned)typ);
    }
}

void
tpbcli_data_fmt_decode_uattr_bits(uint64_t uattr_bits, char *out, size_t outsz)
{
    TPB_UNIT_T ukind;
    TPB_UNIT_T uname;
    TPB_UNIT_T ubase;
    TPB_UNIT_T uattr;
    TPB_UNIT_T shape;
    const char *unit_macro;
    char tmp[64];
    int first = 1;

    if (!out || outsz == 0) {
        return;
    }
    out[0] = '\0';

    if (uattr_bits == 0) {
        snprintf(out, outsz, "UNDEF");
        return;
    }

    uattr = uattr_bits & TPB_UATTR_MASK;
    if (uattr & TPB_UATTR_CAST_Y) {
        _sf_append_decode(out, outsz, "TPB_UATTR_CAST_Y", &first);
    } else {
        _sf_append_decode(out, outsz, "TPB_UATTR_CAST_N", &first);
    }

    shape = uattr & TPB_UATTR_SHAPE_MASK;
    if (shape == TPB_UATTR_SHAPE_POINT) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_POINT", &first);
    } else if (shape == TPB_UATTR_SHAPE_1D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_1D", &first);
    } else if (shape == TPB_UATTR_SHAPE_2D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_2D", &first);
    } else if (shape == TPB_UATTR_SHAPE_3D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_3D", &first);
    } else if (shape == TPB_UATTR_SHAPE_4D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_4D", &first);
    } else if (shape == TPB_UATTR_SHAPE_5D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_5D", &first);
    } else if (shape == TPB_UATTR_SHAPE_6D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_6D", &first);
    } else if (shape == TPB_UATTR_SHAPE_7D) {
        _sf_append_decode(out, outsz, "TPB_UATTR_SHAPE_7D", &first);
    }

    if (uattr & TPB_UATTR_TRIM_N) {
        _sf_append_decode(out, outsz, "TPB_UATTR_TRIM_N", &first);
    } else {
        _sf_append_decode(out, outsz, "TPB_UATTR_TRIM_Y", &first);
    }

    ukind = uattr_bits & TPB_UKIND_MASK;
    if (ukind == TPB_UKIND_TIME) {
        _sf_append_decode(out, outsz, "TPB_UKIND_TIME", &first);
    } else if (ukind == TPB_UKIND_VOL) {
        _sf_append_decode(out, outsz, "TPB_UKIND_VOL", &first);
    } else if (ukind == TPB_UKIND_VOLPTIME) {
        _sf_append_decode(out, outsz, "TPB_UKIND_VOLPTIME", &first);
    } else if (ukind != TPB_UKIND_UNDEF) {
        snprintf(tmp, sizeof(tmp), "UNKNOWN_UKIND_0x%llx",
                 (unsigned long long)(ukind >> 44));
        _sf_append_decode(out, outsz, tmp, &first);
    }

    uname = uattr_bits & TPB_UNAME_MASK;
#define TPB_DATAFMT_MATCH_UNAME(name)                                   \
    do { if (uname == (TPB_UNIT_T)(name)) {                             \
        _sf_append_decode(out, outsz, #name, &first);                     \
    } } while (0)
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_WALLTIME);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_PHYSTIME);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_DATETIME);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_TICKTIME);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_TIMERTIME);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_DATASIZE);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_OP);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_GRIDSIZE);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_BITSIZE);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_OPS);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_FLOPS);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_TOKENPS);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_TPS);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_BITPS);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_DATAPS);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_BITPCY);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_DATAPCY);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_BITPTICK);
    TPB_DATAFMT_MATCH_UNAME(TPB_UNAME_DATAPTICK);
#undef TPB_DATAFMT_MATCH_UNAME

    ubase = uattr_bits & TPB_UBASE_MASK;
    if (ubase == TPB_UBASE_BASE) {
        _sf_append_decode(out, outsz, "TPB_UBASE_BASE", &first);
    } else if (ubase == TPB_UBASE_BIN_EXP_P) {
        _sf_append_decode(out, outsz, "TPB_UBASE_BIN_EXP_P", &first);
    } else if (ubase == TPB_UBASE_BIN_EXP_N) {
        _sf_append_decode(out, outsz, "TPB_UBASE_BIN_EXP_N", &first);
    } else if (ubase == TPB_UBASE_BIN_MUL_P) {
        _sf_append_decode(out, outsz, "TPB_UBASE_BIN_MUL_P", &first);
    } else if (ubase == TPB_UBASE_DEC_EXP_P) {
        _sf_append_decode(out, outsz, "TPB_UBASE_DEC_EXP_P", &first);
    } else if (ubase == TPB_UBASE_DEC_EXP_N) {
        _sf_append_decode(out, outsz, "TPB_UBASE_DEC_EXP_N", &first);
    } else if (ubase == TPB_UBASE_DEC_MUL_P) {
        _sf_append_decode(out, outsz, "TPB_UBASE_DEC_MUL_P", &first);
    } else if (ubase != TPB_UBASE_UNDEF) {
        snprintf(tmp, sizeof(tmp), "UNKNOWN_UBASE_0x%llx",
                 (unsigned long long)(ubase >> 32));
        _sf_append_decode(out, outsz, tmp, &first);
    }

    unit_macro = tpbcli_data_fmt_unit_macro(uattr_bits);
    if (unit_macro != NULL) {
        _sf_append_decode(out, outsz, unit_macro, &first);
    } else {
        uint32_t uval = (uint32_t)(uattr_bits & TPB_UNIT_MASK);
        snprintf(tmp, sizeof(tmp), "UVAL=%" PRIu32, uval);
        _sf_append_decode(out, outsz, tmp, &first);
    }

    if (first) {
        snprintf(out, outsz, "UNKNOWN_UATTR");
    }
}
