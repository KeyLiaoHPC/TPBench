/*
 * rafdb-l2-tbatch.c
 * TBatch domain entry, ID, and record I/O (TPB_RAF_DOM_TBATCH).
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "../../include/tpb-public.h"
#include "../tpb-types.h"
#include "rafdb-l1-internal.h"
#include "rafdb-l1-sha1.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(tbatch_entry_t) == 264, "tbatch_entry_t on-disk size");
#endif

#define TPB_RAF_TBATCH_FILE_OFF_DURATION   100
#define TPB_RAF_TBATCH_FILE_OFF_NKERNEL    240
#define TPB_RAF_TBATCH_FILE_OFF_NTASK      244

/**
 * @brief Append a tbatch entry to the .tpbe file.
 */
int
tpb_raf_entry_append_tbatch(const char *workspace,
                            const tbatch_entry_t *entry)
{
    if (!workspace || !entry) {
        return TPBE_NULLPTR_ARG;
    }
    return _tpb_raf_l1_entry_append(workspace, TPB_RAF_DOM_TBATCH,
                                  entry, sizeof(tbatch_entry_t));
}

/**
 * @brief List all tbatch entries from the .tpbe file.
 */
int
tpb_raf_entry_list_tbatch(const char *workspace,
                          tbatch_entry_t **entries,
                          int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return _tpb_raf_l1_entry_list(workspace, TPB_RAF_DOM_TBATCH,
                                 (void **)entries, count,
                                 sizeof(tbatch_entry_t));
}

/**
 * @brief Generate TBatchID via SHA1.
 */
int
tpb_raf_gen_tbatch_id(tpb_dtbits_t utc_bits,
                      uint64_t btime,
                      const char *hostname,
                      const char *username,
                      uint32_t front_pid,
                      unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;
    char numbuf[32];
    int len;

    if (!hostname || !username || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "tbatch", 6);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)utc_bits);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)btime);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_update(&ctx, hostname, strlen(hostname));
    tpb_sha1_update(&ctx, username, strlen(username));

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)front_pid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

/**
 * @brief Write a tbatch .tpbr record file.
 */
int
tpb_raf_record_write_tbatch(const char *workspace,
                            const tbatch_attr_t *attr,
                            const void *data,
                            uint64_t datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;

    if (!workspace || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TBATCH,
                                 attr->tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }

    metasize = 236 + TPB_RAF_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                        TPB_RAF_DOM_TBATCH,
                        TPB_RAF_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }

    if (_tpb_raf_l1_write_u64(fp, metasize) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, datasize) != 0) {
        goto fail;
    }

    if (fwrite(attr->tbatch_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->derive_to, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->inherit_from, 1, 20, fp) != 20) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, attr->utc_bits) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, attr->btime) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, attr->duration) != 0) {
        goto fail;
    }
    if (fwrite(attr->hostname, 1, 64, fp) != 64) {
        goto fail;
    }
    if (fwrite(attr->username, 1, 64, fp) != 64) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->front_pid) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nkernel) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->ntask) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nscore) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->batch_type) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nheader) != 0) {
        goto fail;
    }

    memset(reserve, 0, TPB_RAF_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_tpb_raf_l1_write_headers(fp, attr->headers, attr->nheader)
            != 0) {
            goto fail;
        }
    }

    if (_tpb_raf_l1_write_magic_and_data(fp, TPB_RAF_DOM_TBATCH,
                                         data, datasize) != 0) {
        goto fail;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/**
 * @brief Patch duration, nkernel, and ntask in an existing tbatch .tpbr.
 */
int
tpb_raf_record_patch_tbatch_counters(const char *workspace,
                                      const unsigned char tbatch_id[20],
                                      uint64_t duration,
                                      uint32_t nkernel,
                                      uint32_t ntask)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    FILE *fp;
    struct flock fl;
    int fd;

    if (!workspace || !tbatch_id) {
        return TPBE_NULLPTR_ARG;
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TBATCH,
                                  tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "r+b");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }
    (void)setvbuf(fp, NULL, _IONBF, 0);
    fd = fileno(fp);
    if (fd < 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLKW, &fl) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TBATCH,
                                TPB_RAF_POS_START)) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TBATCH_FILE_OFF_DURATION, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u64(fp, duration) != 0) {
        goto fail_unlock;
    }
    if (fseek(fp, TPB_RAF_TBATCH_FILE_OFF_NKERNEL, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u32(fp, nkernel) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u32(fp, ntask) != 0) {
        goto fail_unlock;
    }

    if (fflush(fp) != 0) {
        goto fail_unlock;
    }

    fl.l_type = F_UNLCK;
    (void)fcntl(fd, F_SETLK, &fl);
    fclose(fp);
    return TPBE_SUCCESS;

fail_unlock:
    fl.l_type = F_UNLCK;
    (void)fcntl(fd, F_SETLK, &fl);
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/**
 * @brief Read a tbatch .tpbr record file.
 */
int
tpb_raf_record_read_tbatch(const char *workspace,
                           const unsigned char tbatch_id[20],
                           tbatch_attr_t *attr,
                           void **data,
                           uint64_t *datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint64_t ds;

    if (!workspace || !tbatch_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TBATCH,
                                  tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TBATCH,
                                TPB_RAF_POS_START)) {
        goto fail;
    }

    if (_tpb_raf_l1_read_u64(fp, &metasize) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &ds) != 0) {
        goto fail;
    }

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->tbatch_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->derive_to, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->inherit_from, 1, 20, fp) != 20) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &attr->utc_bits) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &attr->btime) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &attr->duration) != 0) {
        goto fail;
    }
    if (fread(attr->hostname, 1, 64, fp) != 64) {
        goto fail;
    }
    if (fread(attr->username, 1, 64, fp) != 64) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->front_pid) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nkernel) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->ntask) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nscore) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->batch_type) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nheader) != 0) {
        goto fail;
    }

    if (fread(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_tpb_raf_l1_read_headers(fp, &attr->headers,
                                     attr->nheader) != 0) {
            goto fail;
        }
    } else {
        attr->headers = NULL;
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TBATCH,
                                TPB_RAF_POS_SPLIT)) {
        goto fail_hdrs;
    }

    if (data && datasize) {
        *datasize = ds;
        if (ds > 0) {
            *data = malloc((size_t)ds);
            if (!*data) {
                goto fail_hdrs;
            }
            if (fread(*data, 1, (size_t)ds, fp)
                != (size_t)ds) {
                free(*data);
                *data = NULL;
                goto fail_hdrs;
            }
        } else {
            *data = NULL;
        }
    } else if (ds > 0) {
        if (fseek(fp, (long)ds, SEEK_CUR) != 0) {
            goto fail_hdrs;
        }
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TBATCH,
                                TPB_RAF_POS_END)) {
        goto fail_hdrs;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail_hdrs:
    tpb_raf_free_headers(attr->headers, attr->nheader);
    attr->headers = NULL;
fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}
