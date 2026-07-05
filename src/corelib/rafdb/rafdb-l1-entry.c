/*
 * rafdb-l1-entry.c
 * L1 generic .tpbe append and list with flock.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "../../include/tpb-public.h"
#include "rafdb-l1-internal.h"

int
_tpb_raf_l1_entry_append(const char *workspace, uint8_t domain,
                         const void *entry_data, size_t entry_size)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    unsigned char end_magic[TPB_RAF_MAGIC_LEN];
    int fd;
    FILE *fp;
    struct stat st;
    long fsize;

    _tpb_raf_l1_build_entry_path(workspace, domain, fpath, sizeof(fpath));

    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY, domain,
                        TPB_RAF_POS_START, start_magic);
    tpb_raf_build_magic(TPB_RAF_FTYPE_ENTRY, domain,
                        TPB_RAF_POS_END, end_magic);

    fd = open(fpath, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        return TPBE_FILE_IO_FAIL;
    }
    if (flock(fd, LOCK_EX) != 0) {
        (void)close(fd);
        return TPBE_FILE_IO_FAIL;
    }

    fp = fdopen(fd, "r+b");
    if (!fp) {
        (void)close(fd);
        return TPBE_FILE_IO_FAIL;
    }

    if (fstat(fd, &st) != 0) {
        goto fail;
    }

    if (st.st_size == 0) {
        if (fwrite(start_magic, 1, TPB_RAF_MAGIC_LEN, fp)
            != TPB_RAF_MAGIC_LEN) {
            goto fail;
        }
        if (fwrite(entry_data, 1, entry_size, fp) != entry_size) {
            goto fail;
        }
        if (fwrite(end_magic, 1, TPB_RAF_MAGIC_LEN, fp)
            != TPB_RAF_MAGIC_LEN) {
            goto fail;
        }

        if (fclose(fp) != 0) {
            return TPBE_FILE_IO_FAIL;
        }
        return TPBE_SUCCESS;
    }

    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAF_MAGIC_LEN)) {
        goto fail;
    }

    if (fseek(fp, fsize - TPB_RAF_MAGIC_LEN, SEEK_SET) != 0) {
        goto fail;
    }
    {
        unsigned char check[TPB_RAF_MAGIC_LEN];

        if (fread(check, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
            goto fail;
        }
        if (!tpb_raf_validate_magic(check, TPB_RAF_FTYPE_ENTRY, domain,
                                    TPB_RAF_POS_END)) {
            goto fail;
        }
    }

    if (fseek(fp, fsize - TPB_RAF_MAGIC_LEN, SEEK_SET) != 0) {
        goto fail;
    }
    if (fwrite(entry_data, 1, entry_size, fp) != entry_size) {
        goto fail;
    }
    if (fwrite(end_magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }

    if (fclose(fp) != 0) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;

fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

int
_tpb_raf_l1_entry_list(const char *workspace, uint8_t domain,
                       void **entries_out, int *count_out,
                       size_t entry_size)
{
    char fpath[TPB_RAF_PATH_MAX];
    FILE *fp;
    struct stat st;
    long fsize;
    long data_size;
    int n;
    unsigned char magic_check[TPB_RAF_MAGIC_LEN];
    void *buf;

    _tpb_raf_l1_build_entry_path(workspace, domain, fpath, sizeof(fpath));

    if (stat(fpath, &st) != 0) {
        *entries_out = NULL;
        *count_out = 0;
        return TPBE_SUCCESS;
    }

    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAF_MAGIC_LEN)) {
        *entries_out = NULL;
        *count_out = 0;
        return TPBE_FILE_IO_FAIL;
    }

    data_size = fsize - 2 * TPB_RAF_MAGIC_LEN;
    if (data_size < 0 || (data_size % (long)entry_size) != 0) {
        *entries_out = NULL;
        *count_out = 0;
        return TPBE_FILE_IO_FAIL;
    }
    n = (int)(data_size / (long)entry_size);

    fp = fopen(fpath, "rb");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }

    if (fread(magic_check, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_raf_validate_magic(magic_check, TPB_RAF_FTYPE_ENTRY, domain,
                                TPB_RAF_POS_START)) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    if (n == 0) {
        *entries_out = NULL;
        *count_out = 0;
        fclose(fp);
        return TPBE_SUCCESS;
    }

    buf = malloc((size_t)n * entry_size);
    if (!buf) {
        fclose(fp);
        return TPBE_MALLOC_FAIL;
    }

    if (fread(buf, entry_size, (size_t)n, fp) != (size_t)n) {
        free(buf);
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    if (fread(magic_check, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        free(buf);
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_raf_validate_magic(magic_check, TPB_RAF_FTYPE_ENTRY, domain,
                                TPB_RAF_POS_END)) {
        free(buf);
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    fclose(fp);
    *entries_out = buf;
    *count_out = n;
    return TPBE_SUCCESS;
}
