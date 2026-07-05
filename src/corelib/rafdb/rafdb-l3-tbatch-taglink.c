/*
 * rafdb-l3-tbatch-taglink.c
 * Append TaskRecordID to tbatch .tpbr header[0] (TPBLINK::TaskID).
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"

#define TPB_RAF_TBATCH_FILE_OFF_DATASIZE   16
#define TPB_RAF_TBATCH_FILE_OFF_NHEADER    256
#define TPB_RAF_TBATCH_FILE_HDR0_BASE      388
#define TPB_RAF_TBATCH_HDR_DATA_SIZE_OFF   8
#define TPB_RAF_TBATCH_HDR_NAME_OFF        32
#define TPB_RAF_TBATCH_HDR_DIM0_OFF        2336
#define TPB_RAF_TBATCH_HDR_TASK_NAME      "TPBLINK::TaskID"
#define TPB_RAF_TBATCH_HDR_KERNEL_NAME    "TPBLINK::KernelID"

/**
 * @brief Append a TaskRecordID to tbatch .tpbr header[0] (TPBLINK::TaskID).
 */
int
tpb_raf_record_append_tbatch(const char *workspace,
                             const unsigned char tbatch_id[20],
                             const unsigned char task_id[20])
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char namechk[256];
    FILE *fp;
    uint64_t metasize;
    uint64_t datasize;
    uint32_t nheader;
    uint64_t hdr0_ds;
    uint64_t hdr1_ds;
    struct flock fl;
    int fd;
    long split_off;
    long hdr0_base;
    long hdr1_base;
    long len0;
    long len1;
    long end_magic_off;
    uint64_t new_total_ds;
    uint64_t new_hdr0_ds;
    uint64_t new_dim0;

    if (!workspace || !tbatch_id || !task_id) {
        TPB_FAIL(TPB_MOD_RAF_L3_TBATCH, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_record_path(workspace, TPB_RAF_DOM_TBATCH,
                                  tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "r+b");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L3_TBATCH, TPBE_FILE_IO_FAIL, NULL);
    }
    (void)setvbuf(fp, NULL, _IONBF, 0);
    fd = fileno(fp);
    if (fd < 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_RAF_L3_TBATCH, TPBE_FILE_IO_FAIL, NULL);
    }

    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLKW, &fl) != 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_RAF_L3_TBATCH, TPBE_FILE_IO_FAIL, NULL);
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

    if (_tpb_raf_l1_read_u64(fp, &metasize) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u64(fp, &datasize) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TBATCH_FILE_OFF_NHEADER, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u32(fp, &nheader) != 0) {
        goto fail_unlock;
    }
    if (nheader != 2) {
        goto fail_unlock;
    }

    hdr0_base = (long)TPB_RAF_TBATCH_FILE_HDR0_BASE;
    if (fseek(fp, hdr0_base + TPB_RAF_TBATCH_HDR_NAME_OFF, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (fread(namechk, 1, sizeof(namechk), fp) != sizeof(namechk)) {
        goto fail_unlock;
    }
    {
        size_t hn = strlen(TPB_RAF_TBATCH_HDR_TASK_NAME);

        if (strncmp((char *)namechk, TPB_RAF_TBATCH_HDR_TASK_NAME, hn) != 0
            || (hn < sizeof(namechk) && namechk[hn] != '\0')) {
            goto fail_unlock;
        }
    }

    len0 = _tpb_raf_l1_hdr_serialized_len_at(fp, hdr0_base);
    if (len0 < 0) {
        goto fail_unlock;
    }

    hdr1_base = hdr0_base + len0;
    if (fseek(fp, hdr1_base + TPB_RAF_TBATCH_HDR_NAME_OFF, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (fread(namechk, 1, sizeof(namechk), fp) != sizeof(namechk)) {
        goto fail_unlock;
    }
    {
        size_t hn = strlen(TPB_RAF_TBATCH_HDR_KERNEL_NAME);

        if (strncmp((char *)namechk, TPB_RAF_TBATCH_HDR_KERNEL_NAME, hn) != 0
            || (hn < sizeof(namechk) && namechk[hn] != '\0')) {
            goto fail_unlock;
        }
    }

    len1 = _tpb_raf_l1_hdr_serialized_len_at(fp, hdr1_base);
    if (len1 < 0) {
        goto fail_unlock;
    }

    split_off = hdr1_base + len1;
    (void)metasize;

    if (fseek(fp, hdr0_base + TPB_RAF_TBATCH_HDR_DATA_SIZE_OFF, SEEK_SET)
        != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u64(fp, &hdr0_ds) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, hdr1_base + TPB_RAF_TBATCH_HDR_DATA_SIZE_OFF, SEEK_SET)
        != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u64(fp, &hdr1_ds) != 0) {
        goto fail_unlock;
    }

    if (hdr0_ds % 20U != 0U) {
        goto fail_unlock;
    }
    if (hdr0_ds + hdr1_ds != datasize) {
        goto fail_unlock;
    }

    end_magic_off = split_off + 8L + (long)datasize;

    if (fseek(fp, end_magic_off, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_TBATCH,
                                TPB_RAF_POS_END)) {
        goto fail_unlock;
    }

    new_total_ds = datasize + 20U;
    new_hdr0_ds = hdr0_ds + 20U;
    new_dim0 = new_hdr0_ds / 20U;

    {
        long insert_off;
        size_t tail_len;
        unsigned char *tail;

        insert_off = split_off + 8L + (long)hdr0_ds;
        tail_len = (size_t)hdr1_ds + (size_t)TPB_RAF_MAGIC_LEN;
        tail = (unsigned char *)malloc(tail_len);
        if (tail == NULL) {
            goto fail_unlock;
        }
        if (fseek(fp, insert_off, SEEK_SET) != 0) {
            free(tail);
            goto fail_unlock;
        }
        if (fread(tail, 1, tail_len, fp) != tail_len) {
            free(tail);
            goto fail_unlock;
        }
        if (fseek(fp, insert_off, SEEK_SET) != 0) {
            free(tail);
            goto fail_unlock;
        }
        if (fwrite(task_id, 1, 20, fp) != 20) {
            free(tail);
            goto fail_unlock;
        }
        if (fwrite(tail, 1, tail_len, fp) != tail_len) {
            free(tail);
            goto fail_unlock;
        }
        free(tail);
    }

    if (fseek(fp, TPB_RAF_TBATCH_FILE_OFF_DATASIZE, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u64(fp, new_total_ds) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, hdr0_base + TPB_RAF_TBATCH_HDR_DATA_SIZE_OFF, SEEK_SET)
        != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u64(fp, new_hdr0_ds) != 0) {
        goto fail_unlock;
    }

    if (fseek(fp, hdr0_base + TPB_RAF_TBATCH_HDR_DIM0_OFF, SEEK_SET) != 0) {
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
    TPB_FAIL(TPB_MOD_RAF_L3_TBATCH, TPBE_FILE_IO_FAIL, NULL);
}
