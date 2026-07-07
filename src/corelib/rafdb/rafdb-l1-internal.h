/**
 * @file rafdb-l1-internal.h
 * @brief L1 cross-file internal API for rafdb framework layer.
 */

#ifndef RAFDB_L1_INTERNAL_H
#define RAFDB_L1_INTERNAL_H

#include <stdio.h>
#include <stdint.h>
#include "../../include/tpb-public.h"
#include "rafdb-l1-types.h"

/* rafdb-l1-record-path.c */
void _tpb_raf_l1_build_entry_path(const char *workspace, uint8_t domain,
                                  char *out, size_t outlen);
void _tpb_raf_l1_build_record_path(const char *workspace, uint8_t domain,
                                   const unsigned char id[20],
                                   char *out, size_t outlen);
void _tpb_raf_l1_build_record_path_int32(const char *workspace, uint8_t domain,
                                         int32_t id, char *out, size_t outlen);

/* rafdb-l1-record-io.c */
int _tpb_raf_l1_write_u32(FILE *fp, uint32_t v);
int _tpb_raf_l1_write_u64(FILE *fp, uint64_t v);
int _tpb_raf_l1_read_u32(FILE *fp, uint32_t *v);
int _tpb_raf_l1_read_u64(FILE *fp, uint64_t *v);
long _tpb_raf_l1_hdr_serialized_len_at(FILE *fp, long hdr_base);
int _tpb_raf_l1_write_headers(FILE *fp, const tpb_meta_header_t *hdrs,
                              uint32_t n);
int _tpb_raf_l1_read_headers(FILE *fp, tpb_meta_header_t **hdrs_out,
                             uint32_t n);
int _tpb_raf_l1_write_magic_and_data(FILE *fp, uint8_t domain,
                                     const void *data, uint64_t datasize);
int _tpb_raf_l1_find_header_by_name(const tpb_meta_header_t *hdrs,
                                    uint32_t nheader, const char *name);

/* rafdb-l1-entry.c */
int _tpb_raf_l1_entry_append(const char *workspace, uint8_t domain,
                             const void *entry_data, size_t entry_size);
int _tpb_raf_l1_entry_list(const char *workspace, uint8_t domain,
                           void **entries_out, int *count_out,
                           size_t entry_size);

#endif /* RAFDB_L1_INTERNAL_H */
