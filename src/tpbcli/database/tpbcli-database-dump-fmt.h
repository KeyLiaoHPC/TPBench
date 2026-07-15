/**
 * @file tpbcli-database-dump-fmt.h
 * @brief Human-readable layout formatters for `tpbcli database dump -i` (.tpbr).
 */

#ifndef TPBCLI_DATABASE_DUMP_FMT_H
#define TPBCLI_DATABASE_DUMP_FMT_H

#include <stdint.h>

#include "tpb-public.h"

/** @brief Maximum key/value pairs in one Attributes or header KV block. */
#define TPBCLI_DUMP_KV_MAX 48

/** @brief One key/value row for batched equal-width printing. */
typedef struct tpbcli_dump_kv_row {
    char key[320];
    char value[768];
} tpbcli_dump_kv_row_t;

/** @brief Domain display name for dump banner (e.g. "task"). */
const char *tpbcli_dump_fmt_domain_name(uint8_t domain);

/** @brief Full-width Domain / ID banner (fill '='). */
void tpbcli_dump_fmt_print_domain_banner(uint8_t domain,
                                         const char *id_display);

/** @brief Print one on-disk magic line: `0x E1 54 …` (uppercase, byte-spaced). */
void tpbcli_dump_fmt_print_magic_line(const unsigned char magic[8]);

/** @brief Section title between '=' rules, e.g. "Section: Metadata". */
void tpbcli_dump_fmt_print_section_banner(const char *section_title);

/** @brief Subsection title between '-' rules, e.g. "Attributes". */
void tpbcli_dump_fmt_print_subsection(const char *title);

/** @brief Print a KV block with per-block key-width clamp [8, 24]. */
void tpbcli_dump_fmt_print_kv_block(const tpbcli_dump_kv_row_t *rows, int nrows);

/** @brief Decode type_bits into parenthetical flag string (deep). */
void tpbcli_dump_fmt_decode_type_bits(uint32_t type_bits,
                                      char *out, size_t outsz);

/** @brief Decode uattr_bits into parenthetical flag string (deep). */
void tpbcli_dump_fmt_decode_uattr_bits(uint64_t uattr_bits,
                                       char *out, size_t outsz);

/** @brief Print all meta headers under a "Headers" subsection. */
void tpbcli_dump_fmt_print_headers(const tpb_meta_header_t *hdrs, uint32_t n);

/** @brief Print record-data payload for all headers. */
void tpbcli_dump_fmt_print_record_data(const tpb_meta_header_t *hdrs,
                                       uint32_t nheader,
                                       const void *data,
                                       uint64_t datasize);

/** @brief END magic line plus "END OF FILE" trailer. */
void tpbcli_dump_fmt_print_file_end(const unsigned char end_magic[8]);

/** @brief Helpers to build KV row values. */
void tpbcli_dump_fmt_kv_set_u64(tpbcli_dump_kv_row_t *row, const char *key,
                                uint64_t v);
void tpbcli_dump_fmt_kv_set_u32(tpbcli_dump_kv_row_t *row, const char *key,
                                uint32_t v);
void tpbcli_dump_fmt_kv_set_i32(tpbcli_dump_kv_row_t *row, const char *key,
                                int32_t v);
void tpbcli_dump_fmt_kv_set_str(tpbcli_dump_kv_row_t *row, const char *key,
                                const char *val);
void tpbcli_dump_fmt_kv_set_hex20(tpbcli_dump_kv_row_t *row, const char *key,
                                  const unsigned char id[20]);
void tpbcli_dump_fmt_kv_set_build_datetime(tpbcli_dump_kv_row_t *row,
                                           const char *key,
                                           tpb_dtbits_t utc_bits);
void tpbcli_dump_fmt_kv_set_type_bits(tpbcli_dump_kv_row_t *row,
                                      const char *key, uint32_t type_bits);
void tpbcli_dump_fmt_kv_set_uattr_bits(tpbcli_dump_kv_row_t *row,
                                       const char *key, uint64_t uattr_bits);

#endif /* TPBCLI_DATABASE_DUMP_FMT_H */
