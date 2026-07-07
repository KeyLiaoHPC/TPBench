/*
 * rafdb-l3-runtime-environment-derive.c
 * Append child RTEnv IDs to parent DeriveTo link header.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"
#include "tpb-raf-runtime-environment.h"

#define TPB_RAF_RTENV_FILE_HDR_SIZE  24
#define TPB_RAF_RTENV_FILE_OFF_DERIVE \
    (TPB_RAF_RTENV_FILE_HDR_SIZE + TPB_RAF_RTENV_OFF_DERIVE)

/* Local Function Prototypes */
static int _sf_patch_tpbe_derive_to(const char *workspace, int32_t id,
                                    int32_t derive_to);
static int _sf_patch_tpbr_derive_to(const char *workspace, int32_t id,
                                    int32_t derive_to);
static int _sf_find_derive_header(const tpb_meta_header_t *hdrs,
                                  uint32_t nheader);
static size_t _sf_header_data_offset(const tpb_meta_header_t *hdrs,
                                     uint32_t nheader, uint32_t idx);

static int
_sf_patch_tpbe_derive_to(const char *workspace, int32_t id, int32_t derive_to)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    rtenv_entry_disk_t disk;
    FILE *fp;
    struct flock fl;
    int fd;
    long pos;

    _tpb_raf_l1_build_entry_path(workspace, TPB_RAF_DOM_RTENV,
                                 fpath, sizeof(fpath));
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

    if (fread(start_magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(start_magic, TPB_RAF_FTYPE_ENTRY,
                                TPB_RAF_DOM_RTENV,
                                TPB_RAF_POS_START)) {
        goto fail_unlock;
    }

    pos = (long)TPB_RAF_MAGIC_LEN;
    for (;;) {
        if (fseek(fp, pos, SEEK_SET) != 0) {
            goto fail_unlock;
        }
        if (fread(&disk, sizeof(disk), 1, fp) != 1) {
            goto fail_unlock;
        }
        if (disk.id == id) {
            if (fseek(fp, pos + TPB_RAF_RTENV_OFF_DERIVE, SEEK_SET) != 0) {
                goto fail_unlock;
            }
            if (fwrite(&derive_to, sizeof(derive_to), 1, fp) != 1) {
                goto fail_unlock;
            }
            if (fflush(fp) != 0) {
                goto fail_unlock;
            }
            fl.l_type = F_UNLCK;
            (void)fcntl(fd, F_SETLK, &fl);
            fclose(fp);
            return TPBE_SUCCESS;
        }
        pos += (long)sizeof(disk);
    }

fail_unlock:
    fl.l_type = F_UNLCK;
    (void)fcntl(fd, F_SETLK, &fl);
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

static int
_sf_patch_tpbr_derive_to(const char *workspace, int32_t id, int32_t derive_to)
{
    char fpath[TPB_RAF_PATH_MAX];
    FILE *fp;

    _tpb_raf_l1_build_record_path_int32(workspace, TPB_RAF_DOM_RTENV,
                                        id, fpath, sizeof(fpath));
    fp = fopen(fpath, "r+b");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }
    if (fseek(fp, TPB_RAF_RTENV_FILE_OFF_DERIVE, SEEK_SET) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (fwrite(&derive_to, sizeof(derive_to), 1, fp) != 1) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (fclose(fp) != 0) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;
}

static int
_sf_find_derive_header(const tpb_meta_header_t *hdrs, uint32_t nheader)
{
    uint32_t i;

    for (i = 0; i < nheader; i++) {
        if (strcmp(hdrs[i].name, TPB_RAF_RTENV_HDR_DERIVE_TO) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static size_t
_sf_header_data_offset(const tpb_meta_header_t *hdrs, uint32_t nheader,
                       uint32_t idx)
{
    size_t off = 0;
    uint32_t i;

    for (i = 0; i < idx && i < nheader; i++) {
        off += (size_t)hdrs[i].data_size;
    }
    return off;
}

/**
 * @brief Append a child RTEnv ID to parent's DeriveTo link and derive_to field.
 */
int
tpb_raf_record_append_rtenv_derive(const char *workspace,
                                   int32_t parent_id,
                                   int32_t child_id)
{
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *headers = NULL;
    tpb_meta_header_t *out_hdrs = NULL;
    void *data = NULL;
    void *out_data = NULL;
    uint64_t datasize = 0;
    uint64_t out_datasize = 0;
    int derive_idx;
    int err;
    uint32_t i;

    if (!workspace) {
        TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_NULLPTR_ARG, NULL);
    }

    err = tpb_raf_record_read_rtenv(workspace, parent_id, &attr,
                                    &headers, &data, &datasize);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L3_RTENV, err, NULL);
    }

    derive_idx = _sf_find_derive_header(headers, attr.nheader);
    if (derive_idx >= 0) {
        size_t off = _sf_header_data_offset(headers, attr.nheader,
                                            (uint32_t)derive_idx);
        const int32_t *old_ids = (const int32_t *)((const char *)data + off);
        uint64_t n = headers[derive_idx].dimsizes[0];
        uint64_t k;

        for (k = 0; k < n; k++) {
            if (old_ids[k] == child_id) {
                goto patch_only;
            }
        }

        out_hdrs = (tpb_meta_header_t *)calloc(attr.nheader, sizeof(*out_hdrs));
        if (!out_hdrs) {
            goto fail_read;
        }
        memcpy(out_hdrs, headers, attr.nheader * sizeof(*out_hdrs));
        out_hdrs[derive_idx].dimsizes[0] = n + 1;
        out_hdrs[derive_idx].data_size = (n + 1) * sizeof(int32_t);

        out_datasize = datasize + sizeof(int32_t);
        out_data = malloc((size_t)out_datasize);
        if (!out_data) {
            free(out_hdrs);
            goto fail_read;
        }
        if (off > 0) {
            memcpy(out_data, data, off);
        }
        memcpy((char *)out_data + off, old_ids, (size_t)(n * sizeof(int32_t)));
        memcpy((char *)out_data + off + n * sizeof(int32_t),
               &child_id, sizeof(child_id));
        if (off + headers[derive_idx].data_size < datasize) {
            memcpy((char *)out_data + off + (n + 1) * sizeof(int32_t),
                   (const char *)data + off + headers[derive_idx].data_size,
                   (size_t)(datasize - off - headers[derive_idx].data_size));
        }
    } else {
        tpb_meta_header_t dh;

        out_hdrs = (tpb_meta_header_t *)calloc(attr.nheader + 1,
                                               sizeof(*out_hdrs));
        if (!out_hdrs) {
            goto fail_read;
        }
        for (i = 0; i < attr.nheader; i++) {
            out_hdrs[i] = headers[i];
        }
        memset(&dh, 0, sizeof(dh));
        dh.ndim = 1;
        dh.dimsizes[0] = 1;
        dh.data_size = sizeof(int32_t);
        dh.type_bits = (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK);
        dh.block_size = TPB_RAF_HDR_FIXED_SIZE;
        snprintf(dh.name, TPBM_NAME_STR_MAX_LEN, "%s",
                 TPB_RAF_RTENV_HDR_DERIVE_TO);
        out_hdrs[attr.nheader] = dh;
        attr.nheader += 1;

        out_datasize = datasize + sizeof(int32_t);
        out_data = malloc((size_t)out_datasize);
        if (!out_data) {
            free(out_hdrs);
            goto fail_read;
        }
        if (datasize > 0) {
            memcpy(out_data, data, (size_t)datasize);
        }
        memcpy((char *)out_data + datasize, &child_id, sizeof(child_id));
    }

    attr.derive_to = child_id;
    err = tpb_raf_record_write_rtenv(workspace, &attr, out_hdrs,
                                     out_data, out_datasize);
    free(out_hdrs);
    free(out_data);
    tpb_raf_free_headers(headers, attr.nheader);
    free(data);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L3_RTENV, err, NULL);
    }

    err = _sf_patch_tpbe_derive_to(workspace, parent_id, child_id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L3_RTENV, err, NULL);
    }
    err = _sf_patch_tpbr_derive_to(workspace, parent_id, child_id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L3_RTENV, err, NULL);
    }
    return TPBE_SUCCESS;

patch_only:
    tpb_raf_free_headers(headers, attr.nheader);
    free(data);
    err = _sf_patch_tpbe_derive_to(workspace, parent_id, child_id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L3_RTENV, err, NULL);
    }
    err = _sf_patch_tpbr_derive_to(workspace, parent_id, child_id);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L3_RTENV, err, NULL);
    }
    return TPBE_SUCCESS;

fail_read:
    tpb_raf_free_headers(headers, attr.nheader);
    free(data);
    TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_MALLOC_FAIL, NULL);
}
