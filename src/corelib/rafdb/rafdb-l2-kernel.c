/*
 * rafdb-l2-kernel.c
 * Kernel domain entry, ID, and record I/O (TPB_RAF_DOM_KERNEL).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(kernel_entry_t) == 264, "kernel_entry_t on-disk size");
#endif

/**
 * @brief Append a kernel entry to the .tpbe file.
 */
int
tpb_raf_entry_append_kernel(const char *workspace,
                            const kernel_entry_t *entry)
{
    if (!workspace || !entry) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }
    TPB_PROPAGATE(TPB_MOD_RAF_L2_KERNEL,
                  _tpb_raf_l1_entry_append(workspace, TPB_RAF_DOM_KERNEL,
                                           entry, sizeof(kernel_entry_t)),
                  NULL);
    return TPBE_SUCCESS;
}

/**
 * @brief List all kernel entries from the .tpbe file.
 */
int
tpb_raf_entry_list_kernel(const char *workspace,
                          kernel_entry_t **entries,
                          int *count)
{
    if (!workspace || !entries || !count) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }
    TPB_PROPAGATE(TPB_MOD_RAF_L2_KERNEL,
                  _tpb_raf_l1_entry_list(workspace, TPB_RAF_DOM_KERNEL,
                                         (void **)entries, count,
                                         sizeof(kernel_entry_t)),
                  NULL);
    return TPBE_SUCCESS;
}

/**
 * @brief Copy tpbx SHA-1 digest into KernelID output buffer.
 */
int
tpb_raf_gen_kernel_id(const unsigned char tpbx_sha1[20],
                      unsigned char id_out[20])
{
    if (!tpbx_sha1 || !id_out) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }

    memcpy(id_out, tpbx_sha1, 20);
    return TPBE_SUCCESS;
}

/**
 * @brief Write a kernel .tpbr record file.
 */
int
tpb_raf_record_write_kernel(const char *workspace,
                            const kernel_attr_t *attr,
                            const void *data,
                            uint64_t datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_KERNEL_ATTR_RESERVE];
    FILE *fp;
    uint64_t metasize;

    if (!workspace || !attr) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_KERNEL,
                                  attr->kernel_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    metasize = 2448 + TPB_RAF_KERNEL_ATTR_RESERVE;
    metasize += attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                        TPB_RAF_DOM_KERNEL,
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

    if (fwrite(attr->kernel_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->derive_to, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->inherit_from, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->kernel_name, 1, 256, fp) != 256) {
        goto fail;
    }
    if (fwrite(attr->version, 1, 64, fp) != 64) {
        goto fail;
    }
    if (fwrite(attr->description, 1, 2048, fp) != 2048) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nparm) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nmetric) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->kctrl) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nheader) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->active) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, attr->utc_bits) != 0) {
        goto fail;
    }

    memset(reserve, 0, TPB_RAF_KERNEL_ATTR_RESERVE);
    if (fwrite(reserve, 1, TPB_RAF_KERNEL_ATTR_RESERVE, fp)
        != TPB_RAF_KERNEL_ATTR_RESERVE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_tpb_raf_l1_write_headers(fp, attr->headers, attr->nheader)
            != 0) {
            goto fail;
        }
    }

    if (_tpb_raf_l1_write_magic_and_data(fp, TPB_RAF_DOM_KERNEL,
                                         data, datasize) != 0) {
        goto fail;
    }

    fclose(fp);
    return TPBE_SUCCESS;
fail:
    fclose(fp);
    TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_FILE_IO_FAIL, NULL);
}

/**
 * @brief Read a kernel .tpbr record file.
 */
int
tpb_raf_record_read_kernel(const char *workspace,
                           const unsigned char kernel_id[20],
                           kernel_attr_t *attr,
                           void **data,
                           uint64_t *datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_KERNEL_ATTR_RESERVE];
    FILE *fp;
    uint64_t metasize;
    uint64_t ds;

    if (!workspace || !kernel_id || !attr) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_KERNEL,
                                  kernel_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_KERNEL,
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
    if (fread(attr->kernel_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->derive_to, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->inherit_from, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->kernel_name, 1, 256, fp) != 256) {
        goto fail;
    }
    if (fread(attr->version, 1, 64, fp) != 64) {
        goto fail;
    }
    if (fread(attr->description, 1, 2048, fp) != 2048) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nparm) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nmetric) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->kctrl) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nheader) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->active) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &attr->utc_bits) != 0) {
        goto fail;
    }

    if (fread(reserve, 1, TPB_RAF_KERNEL_ATTR_RESERVE, fp)
        != TPB_RAF_KERNEL_ATTR_RESERVE) {
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
                                TPB_RAF_DOM_KERNEL,
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
                                TPB_RAF_DOM_KERNEL,
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
    TPB_FAIL(TPB_MOD_RAF_L2_KERNEL, TPBE_FILE_IO_FAIL, NULL);
}
