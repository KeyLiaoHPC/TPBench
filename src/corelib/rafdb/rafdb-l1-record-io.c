/*
 * rafdb-l1-record-io.c
 * L1 .tpbr header serialization and primitive record I/O helpers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rafdb-l1-internal.h"
#include "../../include/tpb-public.h"

int
_tpb_raf_l1_write_u64(FILE *fp, uint64_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

int
_tpb_raf_l1_write_u32(FILE *fp, uint32_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

int
_tpb_raf_l1_read_u64(FILE *fp, uint64_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

int
_tpb_raf_l1_read_u32(FILE *fp, uint32_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

long
_tpb_raf_l1_hdr_serialized_len_at(FILE *fp, long hdr_base)
{
    uint32_t ndim;

    if (fseek(fp, hdr_base + 4L, SEEK_SET) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &ndim) != 0) {
        return -1;
    }
    return 32L + 256L + 2048L + (long)ndim * 8L + (long)ndim * 64L;
}

int
_tpb_raf_l1_write_headers(FILE *fp, const tpb_meta_header_t *hdrs,
                          uint32_t n)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < n; i++) {
        uint32_t bs = TPB_RAF_HDR_FIXED_SIZE;

        if (_tpb_raf_l1_write_u32(fp, bs) != 0) {
            return -1;
        }
        if (_tpb_raf_l1_write_u32(fp, hdrs[i].ndim) != 0) {
            return -1;
        }
        if (_tpb_raf_l1_write_u64(fp, hdrs[i].data_size) != 0) {
            return -1;
        }
        if (_tpb_raf_l1_write_u32(fp, hdrs[i].type_bits) != 0) {
            return -1;
        }
        if (_tpb_raf_l1_write_u32(fp, hdrs[i]._reserve) != 0) {
            return -1;
        }
        if (_tpb_raf_l1_write_u64(fp, hdrs[i].uattr_bits) != 0) {
            return -1;
        }
        if (fwrite(hdrs[i].name, 1, 256, fp) != 256) {
            return -1;
        }
        if (fwrite(hdrs[i].note, 1, 2048, fp) != 2048) {
            return -1;
        }

        for (j = 0; j < hdrs[i].ndim; j++) {
            if (_tpb_raf_l1_write_u64(fp, hdrs[i].dimsizes[j]) != 0) {
                return -1;
            }
        }
        for (j = 0; j < hdrs[i].ndim; j++) {
            if (fwrite(hdrs[i].dimnames[j], 1, 64, fp) != 64) {
                return -1;
            }
        }
    }
    return 0;
}

int
_tpb_raf_l1_read_headers(FILE *fp, tpb_meta_header_t **hdrs_out,
                         uint32_t n)
{
    tpb_meta_header_t *hdrs;
    uint32_t i;
    uint32_t j;

    if (n == 0) {
        *hdrs_out = NULL;
        return 0;
    }

    hdrs = (tpb_meta_header_t *)calloc(n, sizeof(*hdrs));
    if (!hdrs) {
        return -1;
    }

    for (i = 0; i < n; i++) {
        if (_tpb_raf_l1_read_u32(fp, &hdrs[i].block_size) != 0) {
            goto fail;
        }
        if (_tpb_raf_l1_read_u32(fp, &hdrs[i].ndim) != 0) {
            goto fail;
        }
        if (_tpb_raf_l1_read_u64(fp, &hdrs[i].data_size) != 0) {
            goto fail;
        }
        if (_tpb_raf_l1_read_u32(fp, &hdrs[i].type_bits) != 0) {
            goto fail;
        }
        if (_tpb_raf_l1_read_u32(fp, &hdrs[i]._reserve) != 0) {
            goto fail;
        }
        if (_tpb_raf_l1_read_u64(fp, &hdrs[i].uattr_bits) != 0) {
            goto fail;
        }
        if (fread(hdrs[i].name, 1, 256, fp) != 256) {
            goto fail;
        }
        if (fread(hdrs[i].note, 1, 2048, fp) != 2048) {
            goto fail;
        }

        for (j = 0; j < hdrs[i].ndim; j++) {
            if (_tpb_raf_l1_read_u64(fp, &hdrs[i].dimsizes[j]) != 0) {
                goto fail;
            }
        }
        for (j = 0; j < hdrs[i].ndim; j++) {
            if (fread(hdrs[i].dimnames[j], 1, 64, fp) != 64) {
                goto fail;
            }
        }
    }

    *hdrs_out = hdrs;
    return 0;

fail:
    free(hdrs);
    return -1;
}

int
_tpb_raf_l1_write_magic_and_data(FILE *fp, uint8_t domain,
                                 const void *data, uint64_t datasize)
{
    unsigned char magic[TPB_RAF_MAGIC_LEN];

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD, domain,
                        TPB_RAF_POS_SPLIT, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        return -1;
    }

    if (datasize > 0 && data) {
        if (fwrite(data, 1, (size_t)datasize, fp) != (size_t)datasize) {
            return -1;
        }
    }

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD, domain,
                        TPB_RAF_POS_END, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        return -1;
    }

    return 0;
}

int
_tpb_raf_l1_find_header_by_name(const tpb_meta_header_t *hdrs,
                                uint32_t nheader, const char *name)
{
    uint32_t i;

    if (hdrs == NULL || name == NULL) {
        return -1;
    }
    for (i = 0; i < nheader; i++) {
        if (strcmp(hdrs[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Free header array allocated by record read functions.
 */
void
tpb_raf_free_headers(tpb_meta_header_t *headers, uint32_t nheader)
{
    (void)nheader;
    free(headers);
}
