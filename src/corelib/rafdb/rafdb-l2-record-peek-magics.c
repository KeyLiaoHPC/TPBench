/*
 * rafdb-l2-record-peek-magics.c
 * Read-only .tpbr START/SPLIT/END magic extraction for CLI dump display.
 *
 * Does not parse attributes or payload. Locates SPLIT by scanning on-disk magic
 * bytes (metasize in the file header can disagree with the serialized metadata
 * span on patched records; SPLIT/END signatures are authoritative). END is
 * positioned at SPLIT + 8 + datasize using the datasize header field.
 */

#include <stdio.h>
#include <string.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"

/* Local Function Prototypes */

static int _sf_peek_validate_domain(uint8_t domain);
static int _sf_file_size(FILE *fp, long *size_out);
static int _sf_read_magic_at(FILE *fp, long pos, unsigned char magic[8]);
static int _sf_find_split_pos(FILE *fp, uint8_t domain, long search_lo,
                              long search_hi, long *split_pos_out);

static int
_sf_peek_validate_domain(uint8_t domain)
{
    return (domain == TPB_RAF_DOM_TBATCH ||
            domain == TPB_RAF_DOM_KERNEL ||
            domain == TPB_RAF_DOM_TASK ||
            domain == TPB_RAF_DOM_RTENV);
}

static int
_sf_file_size(FILE *fp, long *size_out)
{
    long cur;
    long end;

    if (!fp || !size_out) {
        return -1;
    }
    cur = ftell(fp);
    if (cur < 0) {
        return -1;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        return -1;
    }
    end = ftell(fp);
    if (end < 0) {
        return -1;
    }
    if (fseek(fp, cur, SEEK_SET) != 0) {
        return -1;
    }
    *size_out = end;
    return 0;
}

static int
_sf_read_magic_at(FILE *fp, long pos, unsigned char magic[8])
{
    if (fseek(fp, pos, SEEK_SET) != 0) {
        return -1;
    }
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        return -1;
    }
    return 0;
}

static int
_sf_find_split_pos(FILE *fp, uint8_t domain, long search_lo, long search_hi,
                   long *split_pos_out)
{
    long pos;

    if (!split_pos_out) {
        return -1;
    }
    for (pos = search_lo; pos <= search_hi; pos++) {
        unsigned char magic[TPB_RAF_MAGIC_LEN];

        if (_sf_read_magic_at(fp, pos, magic) != 0) {
            return -1;
        }
        if (tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                   domain, TPB_RAF_POS_SPLIT)) {
            *split_pos_out = pos;
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Read and validate the three .tpbr magic signatures from disk.
 */
int
tpb_raf_record_peek_magics(const char *workspace,
                           uint8_t domain,
                           const unsigned char id20[20],
                           int32_t rtenv_id,
                           unsigned char start_magic[8],
                           unsigned char split_magic[8],
                           unsigned char end_magic[8])
{
    char fpath[TPB_RAF_PATH_MAX];
    FILE *fp;
    uint64_t metasize;
    uint64_t datasize;
    long fsize;
    long split_pos;
    long end_pos;
    long search_hi;

    (void)metasize;

    if (!workspace || !start_magic || !split_magic || !end_magic) {
        TPB_FAIL(TPB_MOD_RAF_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (!_sf_peek_validate_domain(domain)) {
        TPB_FAIL(TPB_MOD_RAF_MISC, TPBE_FILE_IO_FAIL, NULL);
    }

    if (domain == TPB_RAF_DOM_RTENV) {
        _tpb_raf_l1_build_record_path_int32(workspace, domain, rtenv_id,
                                            fpath, sizeof(fpath));
    } else {
        if (id20 == NULL) {
            TPB_FAIL(TPB_MOD_RAF_MISC, TPBE_NULLPTR_ARG, NULL);
        }
        _tpb_raf_l1_build_record_path(workspace, domain, id20,
                                      fpath, sizeof(fpath));
    }

    fp = fopen(fpath, "rb");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_MISC, TPBE_FILE_IO_FAIL, NULL);
    }

    /* START magic at file offset 0 */
    if (fread(start_magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(start_magic, TPB_RAF_FTYPE_RECORD,
                                domain, TPB_RAF_POS_START)) {
        goto fail;
    }

    if (_tpb_raf_l1_read_u64(fp, &metasize) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &datasize) != 0) {
        goto fail;
    }

    if (_sf_file_size(fp, &fsize) != 0) {
        goto fail;
    }

    /*
     * Metadata begins at offset 24. Scan forward for the first valid SPLIT
     * signature; do not trust metasize alone (may be stale after patches).
     */
    search_hi = fsize - (long)TPB_RAF_MAGIC_LEN;
    if (datasize <= (uint64_t)(fsize - 24)) {
        long hi_by_data = fsize - (long)TPB_RAF_MAGIC_LEN
            - (long)datasize - (long)TPB_RAF_MAGIC_LEN;
        if (hi_by_data < search_hi) {
            search_hi = hi_by_data;
        }
    }
    if (search_hi < 24) {
        goto fail;
    }
    if (_sf_find_split_pos(fp, domain, 24L, search_hi, &split_pos) != 0) {
        goto fail;
    }
    if (_sf_read_magic_at(fp, split_pos, split_magic) != 0) {
        goto fail;
    }

    end_pos = split_pos + (long)TPB_RAF_MAGIC_LEN + (long)datasize;
    if (end_pos + (long)TPB_RAF_MAGIC_LEN > fsize) {
        goto fail;
    }
    if (_sf_read_magic_at(fp, end_pos, end_magic) != 0) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(end_magic, TPB_RAF_FTYPE_RECORD,
                                domain, TPB_RAF_POS_END)) {
        goto fail;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail:
    fclose(fp);
    TPB_FAIL(TPB_MOD_RAF_MISC, TPBE_FILE_IO_FAIL, NULL);
}
