/*
 * rafdb-l3-runtime-environment-counter.c
 * Patch ntask/ntbatch in RTEnv .tpbe and .tpbr under file lock.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"
#include "tpb-raf-runtime-environment.h"

#define TPB_RAF_RTENV_FILE_HDR_SIZE  24
#define TPB_RAF_RTENV_FILE_OFF_NTASK \
    (TPB_RAF_RTENV_FILE_HDR_SIZE + TPB_RAF_RTENV_OFF_NTASK)
#define TPB_RAF_RTENV_FILE_OFF_NTBATCH \
    (TPB_RAF_RTENV_FILE_HDR_SIZE + TPB_RAF_RTENV_OFF_NTBATCH)

/* Local Function Prototypes */
static int _sf_patch_tpbe_counters(const char *workspace, int32_t id,
                                   uint32_t add_ntask, uint32_t add_ntbatch);

static int
_sf_patch_tpbe_counters(const char *workspace, int32_t id,
                          uint32_t add_ntask, uint32_t add_ntbatch)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    rtenv_entry_disk_t disk;
    FILE *fp;
    struct flock fl;
    int fd;
    long pos;
    int32_t row_id;
    uint32_t ntask;
    uint32_t ntbatch;
    int found = 0;

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
            break;
        }
        row_id = disk.id;
        if (row_id == id) {
            ntask = disk.ntask + add_ntask;
            ntbatch = disk.ntbatch + add_ntbatch;
            if (fseek(fp, pos + TPB_RAF_RTENV_OFF_NTASK, SEEK_SET) != 0) {
                goto fail_unlock;
            }
            if (_tpb_raf_l1_write_u32(fp, ntask) != 0) {
                goto fail_unlock;
            }
            if (_tpb_raf_l1_write_u32(fp, ntbatch) != 0) {
                goto fail_unlock;
            }
            found = 1;
            break;
        }
        pos += (long)sizeof(disk);
    }

    if (!found) {
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
 * @brief Patch ntask/ntbatch counters in RTEnv .tpbe row and .tpbr record.
 */
int
tpb_raf_record_patch_rtenv_counters(const char *workspace,
                                     int32_t id,
                                     uint32_t add_ntask,
                                     uint32_t add_ntbatch)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    FILE *fp;
    struct flock fl;
    int fd;
    uint32_t ntask;
    uint32_t ntbatch;
    int err;

    if (!workspace) {
        TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_NULLPTR_ARG, NULL);
    }
    if (add_ntask == 0 && add_ntbatch == 0) {
        return TPBE_SUCCESS;
    }

    _tpb_raf_l1_build_record_path_int32(workspace, TPB_RAF_DOM_RTENV,
                                        id, fpath, sizeof(fpath));
    fp = fopen(fpath, "r+b");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }
    (void)setvbuf(fp, NULL, _IONBF, 0);
    fd = fileno(fp);
    if (fd < 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }

    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLKW, &fl) != 0) {
        fclose(fp);
        TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_RTENV,
                                TPB_RAF_POS_START)) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_RTENV_FILE_OFF_NTASK, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u32(fp, &ntask) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_read_u32(fp, &ntbatch) != 0) {
        goto fail_unlock;
    }
    ntask += add_ntask;
    ntbatch += add_ntbatch;
    if (fseek(fp, TPB_RAF_RTENV_FILE_OFF_NTASK, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u32(fp, ntask) != 0) {
        goto fail_unlock;
    }
    if (_tpb_raf_l1_write_u32(fp, ntbatch) != 0) {
        goto fail_unlock;
    }

    if (fflush(fp) != 0) {
        goto fail_unlock;
    }
    fl.l_type = F_UNLCK;
    (void)fcntl(fd, F_SETLK, &fl);
    fclose(fp);

    err = _sf_patch_tpbe_counters(workspace, id, add_ntask, add_ntbatch);
    if (err != TPBE_SUCCESS) {
        TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;

fail_unlock:
    fl.l_type = F_UNLCK;
    (void)fcntl(fd, F_SETLK, &fl);
    fclose(fp);
    TPB_FAIL(TPB_MOD_RAF_L3_RTENV, TPBE_FILE_IO_FAIL, NULL);
}
