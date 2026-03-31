/*
 * tpb-raf-entry.c
 * Entry file (.tpbe) append, read, and list operations for all domains.
 *
 * .tpbe files are small index files.  The tpbcli parent is single-writer for
 * most flows; MPI PLI kernels may append concurrently, so entry_append_generic
 * uses flock(2) around read-modify-write.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "tpb-raf-types.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(tbatch_entry_t) == 264, "tbatch_entry_t on-disk size");
_Static_assert(sizeof(kernel_entry_t) == 264, "kernel_entry_t on-disk size");
_Static_assert(sizeof(task_entry_t) == 232, "task_entry_t on-disk size");
_Static_assert(TPB_RAF_RESERVE_SIZE == 128, "TPB_RAF_RESERVE_SIZE");
#endif

/* Local Function Prototypes */
static void build_entry_path(const char *workspace, uint8_t domain,
                             char *out, size_t outlen);
static int entry_append_generic(const char *workspace,
                                uint8_t domain,
                                const void *entry_data,
                                size_t entry_size);
static int entry_list_generic(const char *workspace,
                              uint8_t domain,
                              void **entries_out,
                              int *count_out,
                              size_t entry_size);

static void
build_entry_path(const char *workspace, uint8_t domain,
                 char *out, size_t outlen)
{
    const char *dir, *fname;

    switch (domain) {
    case TPB_RAF_DOM_KERNEL:
        dir = TPB_RAF_KERNEL_DIR;
        fname = TPB_RAF_KERNEL_ENTRY;
        break;
    case TPB_RAF_DOM_TASK:
        dir = TPB_RAF_TASK_DIR;
        fname = TPB_RAF_TASK_ENTRY;
        break;
    default:
        dir = TPB_RAF_TBATCH_DIR;
        fname = TPB_RAF_TBATCH_ENTRY;
        break;
    }
    snprintf(out, outlen, "%s/%s/%s", workspace, dir, fname);
}

/*
 * Append one fixed-size entry row to a .tpbe file.
 *
 * File layout: [start_magic][entry_0]...[entry_N-1][end_magic]
 */
static int
entry_append_generic(const char *workspace, uint8_t domain,
                     const void *entry_data, size_t entry_size)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    unsigned char end_magic[TPB_RAF_MAGIC_LEN];
    int fd;
    FILE *fp;
    struct stat st;
    long fsize;

    build_entry_path(workspace, domain, fpath, sizeof(fpath));

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
            != TPB_RAF_MAGIC_LEN)
            goto fail;
        if (fwrite(entry_data, 1, entry_size, fp) != entry_size)
            goto fail;
        if (fwrite(end_magic, 1, TPB_RAF_MAGIC_LEN, fp)
            != TPB_RAF_MAGIC_LEN)
            goto fail;

        if (fclose(fp) != 0) {
            return TPBE_FILE_IO_FAIL;
        }
        return TPBE_SUCCESS;
    }

    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAF_MAGIC_LEN)) {
        goto fail;
    }

    /* Verify trailing end_magic */
    if (fseek(fp, fsize - TPB_RAF_MAGIC_LEN, SEEK_SET) != 0) {
        goto fail;
    }
    {
        unsigned char check[TPB_RAF_MAGIC_LEN];
        if (fread(check, 1, TPB_RAF_MAGIC_LEN, fp)
            != TPB_RAF_MAGIC_LEN)
            goto fail;
        if (!tpb_raf_validate_magic(check,
                                      TPB_RAF_FTYPE_ENTRY,
                                      domain,
                                      TPB_RAF_POS_END))
            goto fail;
    }

    /* Overwrite old end_magic with entry + new end_magic */
    if (fseek(fp, fsize - TPB_RAF_MAGIC_LEN, SEEK_SET) != 0) {
        goto fail;
    }
    if (fwrite(entry_data, 1, entry_size, fp) != entry_size)
        goto fail;
    if (fwrite(end_magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN)
        goto fail;

    if (fclose(fp) != 0) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;

fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/*
 * Read all entries from a .tpbe file.
 * Returns allocated array in *entries_out; caller must free.
 */
static int
entry_list_generic(const char *workspace, uint8_t domain,
                   void **entries_out, int *count_out,
                   size_t entry_size)
{
    char fpath[TPB_RAF_PATH_MAX];
    FILE *fp;
    struct stat st;
    long fsize, data_size;
    int n;
    unsigned char magic_check[TPB_RAF_MAGIC_LEN];
    void *buf;

    build_entry_path(workspace, domain, fpath, sizeof(fpath));

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
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Verify start magic */
    if (fread(magic_check, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_raf_validate_magic(magic_check,
                                  TPB_RAF_FTYPE_ENTRY,
                                  domain,
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

    /* Verify end magic */
    if (fread(magic_check, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        free(buf);
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_raf_validate_magic(magic_check,
                                  TPB_RAF_FTYPE_ENTRY,
                                  domain,
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

/* Public API wrappers */

int
tpb_raf_entry_append_tbatch(const char *workspace,
                              const tbatch_entry_t *entry)
{
    if (!workspace || !entry) return TPBE_NULLPTR_ARG;
    return entry_append_generic(workspace, TPB_RAF_DOM_TBATCH,
                                entry, sizeof(tbatch_entry_t));
}

int
tpb_raf_entry_append_kernel(const char *workspace,
                              const kernel_entry_t *entry)
{
    if (!workspace || !entry) return TPBE_NULLPTR_ARG;
    return entry_append_generic(workspace, TPB_RAF_DOM_KERNEL,
                                entry, sizeof(kernel_entry_t));
}

int
tpb_raf_entry_append_task(const char *workspace,
                            const task_entry_t *entry)
{
    if (!workspace || !entry) return TPBE_NULLPTR_ARG;
    return entry_append_generic(workspace, TPB_RAF_DOM_TASK,
                                entry, sizeof(task_entry_t));
}

int
tpb_raf_entry_list_tbatch(const char *workspace,
                            tbatch_entry_t **entries,
                            int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return entry_list_generic(workspace, TPB_RAF_DOM_TBATCH,
                              (void **)entries, count,
                              sizeof(tbatch_entry_t));
}

int
tpb_raf_entry_list_kernel(const char *workspace,
                            kernel_entry_t **entries,
                            int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return entry_list_generic(workspace, TPB_RAF_DOM_KERNEL,
                              (void **)entries, count,
                              sizeof(kernel_entry_t));
}

int
tpb_raf_entry_list_task(const char *workspace,
                          task_entry_t **entries,
                          int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return entry_list_generic(workspace, TPB_RAF_DOM_TASK,
                              (void **)entries, count,
                              sizeof(task_entry_t));
}
