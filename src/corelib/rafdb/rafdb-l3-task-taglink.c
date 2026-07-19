/*
 * rafdb-l3-task-taglink.c
 * Append TaskRecordID to a task capsule .tpbr under file lock.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"

#define TPB_RAF_TASK_FILE_OFF_DATASIZE      16
#define TPB_RAF_TASK_FILE_OFF_NHEADER       172
#define TPB_RAF_TASK_FILE_HDR0_BASE       308
#define TPB_RAF_TASK_FILE_HDR0_DATA_SIZE  316
#define TPB_RAF_TASK_FILE_HDR0_NAME       340
/* dimsizes[0] after scalars(32)+name(256)+tag(256)+note(2048) from HDR0_BASE */
#define TPB_RAF_TASK_FILE_HDR0_DIM0       2900
#define TPB_RAF_TASK_CAPSULE_HDR_NAME     "TaskID"

/**
 * @brief Append a TaskRecordID to a task capsule .tpbr under file lock.
 */
int
tpb_raf_record_append_task_capsule(const char *workspace,
                                   const unsigned char capsule_id[20],
                                   const unsigned char task_id[20])
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    FILE *fp;
    uint64_t metasize;
    uint64_t datasize;
    uint32_t nheader;
    unsigned char namechk[256];
    struct flock fl;
    int fd;
    long split_off;
    long end_magic_off;
    uint64_t new_ds;
    uint64_t new_dim0;
    uint64_t new_hdr_ds;

    if (!workspace || !capsule_id || !task_id) {
        TPB_FAIL(TPB_MOD_RAF_L3_TASK, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TASK,
                                  capsule_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "r+b");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L3_TASK, TPBE_FILE_IO_FAIL, NULL);
    }
    (void)setvbuf(fp, NULL, _IONBF, 0);
    fd = fileno(fp);
    if (fd < 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_RAF_L3_TASK, TPBE_FILE_IO_FAIL, NULL);
    }

    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLKW, &fl) != 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_RAF_L3_TASK, TPBE_FILE_IO_FAIL, NULL);
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TASK,
                                TPB_RAF_POS_START)) {
        goto fail_unlock;
    }

    if (_tpb_raf_l1_read_u64(fp, &metasize) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u64(fp, &datasize) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TASK_FILE_OFF_NHEADER, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u32(fp, &nheader) != 0) {
        goto fail_unlock;
    }
    if (nheader != 1) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_NAME, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (fread(namechk, 1, sizeof(namechk), fp) != sizeof(namechk)) {
        goto fail_unlock;
    }
    {
        size_t hn = strlen(TPB_RAF_TASK_CAPSULE_HDR_NAME);

        if (strncmp((char *)namechk, TPB_RAF_TASK_CAPSULE_HDR_NAME, hn) != 0
            || (hn < sizeof(namechk) && namechk[hn] != '\0')) {
            goto fail_unlock;
        }
    }

    if (datasize % 20U != 0U) {
        goto fail_unlock;
    }

    {
        uint32_t hdr0_ndim;

        if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_BASE + (long)sizeof(uint32_t),
                  SEEK_SET) != 0) {
            goto fail_unlock;
        }
        if (_tpb_raf_l1_read_u32(fp, &hdr0_ndim) != 0) {
            goto fail_unlock;
        }
        split_off = (long)TPB_RAF_TASK_FILE_HDR0_BASE + 32L + 256L + 256L
            + 2048L + (long)hdr0_ndim * 8L + (long)hdr0_ndim * 64L;
    }
    (void)metasize;

    end_magic_off = split_off + 8 + (long)datasize;

    new_ds = datasize + 20U;
    new_dim0 = new_ds / 20U;
    new_hdr_ds = new_ds;

    if (fseek(fp, end_magic_off, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TASK,
                                TPB_RAF_POS_END)) {
        goto fail_unlock;
    }

    if (fseek(fp, end_magic_off, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (fwrite(task_id, 1, 20, fp) != 20) {
        goto fail_unlock;
    }

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD, TPB_RAF_DOM_TASK,
                        TPB_RAF_POS_END, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TASK_FILE_OFF_DATASIZE, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u64(fp, new_ds) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_DATA_SIZE, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u64(fp, new_hdr_ds) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_DIM0, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u64(fp, new_dim0) != 0) {
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
    TPB_FAIL(TPB_MOD_RAF_L3_TASK, TPBE_FILE_IO_FAIL, NULL);
}
