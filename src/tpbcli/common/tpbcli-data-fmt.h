/**
 * @file tpbcli-data-fmt.h
 * @brief Shared type and unit name helpers for tpbcli output formatters.
 */

#ifndef TPBCLI_DATA_FMT_H
#define TPBCLI_DATA_FMT_H

#include <stddef.h>
#include <stdint.h>

#include "tpb-public.h"

/**
 * @brief Short dtype label for tables (e.g. "double", "int").
 * @return Static string; "unknown" when unrecognized.
 */
const char *tpbcli_data_fmt_dtype_short(uint32_t type_bits);

/**
 * @brief Base unit macro name from full uattr_bits (e.g. "TPB_UNIT_MBPS").
 * @return Macro name or NULL when not in the built-in map.
 */
const char *tpbcli_data_fmt_unit_macro(uint64_t uattr_bits);

/**
 * @brief Write unit macro into @p out for display; falls back to hex uval.
 */
void tpbcli_data_fmt_format_unit(uint64_t uattr_bits, char *out, size_t outlen);

/**
 * @brief Deep decode of type_bits (same text as db dump Attributes).
 */
void tpbcli_data_fmt_decode_type_bits(uint32_t type_bits, char *out, size_t outsz);

/**
 * @brief Deep decode of uattr_bits (same text as db dump Attributes).
 */
void tpbcli_data_fmt_decode_uattr_bits(uint64_t uattr_bits, char *out, size_t outsz);

#endif /* TPBCLI_DATA_FMT_H */
