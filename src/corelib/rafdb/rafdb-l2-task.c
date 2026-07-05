/*
 * rafdb-l2-task.c
 * Task domain entry, ID, and record I/O (TPB_RAF_DOM_TASK).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/tpb-public.h"
#include "../tpb-types.h"
#include "rafdb-l1-internal.h"
#include "rafdb-l1-sha1.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(task_entry_t) == 232, "task_entry_t on-disk size");
#endif

/**
 * @brief Append a task entry to the .tpbe file.
 */
int
tpb_raf_entry_append_task(const char *workspace,
                          const task_entry_t *entry)
{
    if (!workspace || !entry) {
        return TPBE_NULLPTR_ARG;
    }
    return _tpb_raf_l1_entry_append(workspace, TPB_RAF_DOM_TASK,
                                  entry, sizeof(task_entry_t));
}

/**
 * @brief List all task entries from the .tpbe file.
 */
int
tpb_raf_entry_list_task(const char *workspace,
                        task_entry_t **entries,
                        int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return _tpb_raf_l1_entry_list(workspace, TPB_RAF_DOM_TASK,
                                  (void **)entries, count,
                                  sizeof(task_entry_t));
}

/**
 * @brief Generate TaskRecordID via SHA1.
 */
int
tpb_raf_gen_task_id(tpb_dtbits_t utc_bits,
                    uint64_t btime,
                    const char *hostname,
                    const char *username,
                    const unsigned char tbatch_id[20],
                    const unsigned char kernel_id[20],
                    uint32_t hdl_id,
                    uint32_t pid,
                    uint32_t tid,
                    unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;
    char numbuf[32];
    int len;

    if (!hostname || !username || !tbatch_id ||
        !kernel_id || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "task", 4);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)utc_bits);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)btime);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_update(&ctx, hostname, strlen(hostname));
    tpb_sha1_update(&ctx, username, strlen(username));
    tpb_sha1_update(&ctx, tbatch_id, 20);
    tpb_sha1_update(&ctx, kernel_id, 20);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)hdl_id);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)pid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)tid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

/**
 * @brief Generate TaskCapsuleRecordID (same inputs as task ID, different
 *        leading hash literal).
 */
int
tpb_raf_gen_taskcapsule_id(tpb_dtbits_t utc_bits,
                           uint64_t btime,
                           const char *hostname,
                           const char *username,
                           const unsigned char tbatch_id[20],
                           const unsigned char kernel_id[20],
                           uint32_t hdl_id,
                           uint32_t pid,
                           uint32_t tid,
                           unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;
    char numbuf[32];
    int len;

    if (!hostname || !username || !tbatch_id ||
        !kernel_id || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "taskcapsule", 12);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)utc_bits);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)btime);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_update(&ctx, hostname, strlen(hostname));
    tpb_sha1_update(&ctx, username, strlen(username));
    tpb_sha1_update(&ctx, tbatch_id, 20);
    tpb_sha1_update(&ctx, kernel_id, 20);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)hdl_id);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)pid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)tid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

/**
 * @brief Write a task .tpbr record file.
 */
int
tpb_raf_record_write_task(const char *workspace,
                          const task_attr_t *attr,
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

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TASK,
                                  attr->task_record_id, fpath,
                                  sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }

    metasize = 156 + TPB_RAF_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                        TPB_RAF_DOM_TASK,
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

    if (fwrite(attr->task_record_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->derive_to, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->inherit_from, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->tbatch_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fwrite(attr->kernel_id, 1, 20, fp) != 20) {
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
    if (_tpb_raf_l1_write_u32(fp, attr->exit_code) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->handle_index) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->pid) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->tid) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->ninput) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->noutput) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nheader) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->reserve) != 0) {
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

    if (_tpb_raf_l1_write_magic_and_data(fp, TPB_RAF_DOM_TASK,
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
 * @brief Write a new task capsule .tpbr (one header, first task ID in data).
 */
int
tpb_raf_record_create_task_capsule(const char *workspace,
                                  const task_attr_t *attr,
                                  const unsigned char first_task_id[20])
{
    if (!workspace || !attr || !first_task_id) {
        return TPBE_NULLPTR_ARG;
    }
    if (attr->nheader != 1 || attr->ninput != 0 || attr->noutput != 0) {
        return TPBE_CLI_FAIL;
    }
    return tpb_raf_record_write_task(workspace, attr, first_task_id, 20);
}

/**
 * @brief Read a task .tpbr record file.
 */
int
tpb_raf_record_read_task(const char *workspace,
                         const unsigned char task_id[20],
                         task_attr_t *attr,
                         void **data,
                         uint64_t *datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint64_t ds;

    if (!workspace || !task_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TASK,
                                  task_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TASK,
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
    if (fread(attr->task_record_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->derive_to, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->inherit_from, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->tbatch_id, 1, 20, fp) != 20) {
        goto fail;
    }
    if (fread(attr->kernel_id, 1, 20, fp) != 20) {
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
    if (_tpb_raf_l1_read_u32(fp, &attr->exit_code) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->handle_index) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->pid) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->tid) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->ninput) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->noutput) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nheader) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->reserve) != 0) {
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
                                TPB_RAF_DOM_TASK,
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
                                TPB_RAF_DOM_TASK,
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
