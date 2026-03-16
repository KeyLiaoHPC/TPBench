/*
 * tpb-rawdb-entry.c
 * Entry file (.tpbe) append, read, and list operations for all domains.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../tpb-types.h"
#include "tpb-rawdb-types.h"

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

/*
 * Build the full path to a domain's .tpbe file.
 */
static void
build_entry_path(const char *workspace, uint8_t domain,
                 char *out, size_t outlen)
{
    const char *dir, *fname;

    switch (domain) {
    case TPB_RAWDB_DOM_KERNEL:
        dir = TPB_RAWDB_KERNEL_DIR;
        fname = TPB_RAWDB_KERNEL_ENTRY;
        break;
    case TPB_RAWDB_DOM_TASK:
        dir = TPB_RAWDB_TASK_DIR;
        fname = TPB_RAWDB_TASK_ENTRY;
        break;
    default:
        dir = TPB_RAWDB_TBATCH_DIR;
        fname = TPB_RAWDB_TBATCH_ENTRY;
        break;
    }
    snprintf(out, outlen, "%s/%s/%s", workspace, dir, fname);
}

/*
 * Append a 128-byte entry to a .tpbe file.
 * Creates the file with start/end magic if it does not exist.
 * Otherwise inserts before the trailing end_magic.
 */
static int
entry_append_generic(const char *workspace, uint8_t domain,
                     const void *entry_data, size_t entry_size)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char start_magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char end_magic[TPB_RAWDB_MAGIC_LEN];
    FILE *fp;
    struct stat st;
    long fsize;

    build_entry_path(workspace, domain, fpath, sizeof(fpath));

    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_ENTRY, domain,
                          TPB_RAWDB_POS_START, start_magic);
    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_ENTRY, domain,
                          TPB_RAWDB_POS_END, end_magic);

    if (stat(fpath, &st) != 0) {
        /* File does not exist: create with start + entry + end */
        fp = fopen(fpath, "wb");
        if (!fp) return TPBE_FILE_IO_FAIL;

        if (fwrite(start_magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
            != TPB_RAWDB_MAGIC_LEN) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        if (fwrite(entry_data, 1, entry_size, fp)
            != entry_size) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        if (fwrite(end_magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
            != TPB_RAWDB_MAGIC_LEN) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        fclose(fp);
        return TPBE_SUCCESS;
    }

    /* File exists: open r+b, seek before end_magic, write entry + end */
    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAWDB_MAGIC_LEN)) {
        return TPBE_FILE_IO_FAIL;
    }

    fp = fopen(fpath, "r+b");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Verify end_magic at expected position */
    if (fseek(fp, fsize - TPB_RAWDB_MAGIC_LEN, SEEK_SET) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    unsigned char check[TPB_RAWDB_MAGIC_LEN];
    if (fread(check, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_rawdb_validate_magic(check, TPB_RAWDB_FTYPE_ENTRY,
                                  domain, TPB_RAWDB_POS_END)) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    /* Seek to position of old end_magic, overwrite with entry */
    if (fseek(fp, fsize - TPB_RAWDB_MAGIC_LEN, SEEK_SET) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (fwrite(entry_data, 1, entry_size, fp) != entry_size) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (fwrite(end_magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    fclose(fp);
    return TPBE_SUCCESS;
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
    char fpath[TPB_RAWDB_PATH_MAX];
    FILE *fp;
    struct stat st;
    long fsize, data_size;
    int n;
    unsigned char magic_check[TPB_RAWDB_MAGIC_LEN];
    void *buf;

    build_entry_path(workspace, domain, fpath, sizeof(fpath));

    if (stat(fpath, &st) != 0) {
        *entries_out = NULL;
        *count_out = 0;
        return TPBE_SUCCESS;
    }

    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAWDB_MAGIC_LEN)) {
        *entries_out = NULL;
        *count_out = 0;
        return TPBE_FILE_IO_FAIL;
    }

    data_size = fsize - 2 * TPB_RAWDB_MAGIC_LEN;
    if (data_size < 0 || (data_size % (long)entry_size) != 0) {
        *entries_out = NULL;
        *count_out = 0;
        return TPBE_FILE_IO_FAIL;
    }
    n = (int)(data_size / (long)entry_size);

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Verify start magic */
    if (fread(magic_check, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_rawdb_validate_magic(magic_check,
                                  TPB_RAWDB_FTYPE_ENTRY,
                                  domain,
                                  TPB_RAWDB_POS_START)) {
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
    if (fread(magic_check, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        free(buf);
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (!tpb_rawdb_validate_magic(magic_check,
                                  TPB_RAWDB_FTYPE_ENTRY,
                                  domain,
                                  TPB_RAWDB_POS_END)) {
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
tpb_rawdb_entry_append_tbatch(const char *workspace,
                              const tbatch_entry_t *entry)
{
    if (!workspace || !entry) return TPBE_NULLPTR_ARG;
    return entry_append_generic(workspace, TPB_RAWDB_DOM_TBATCH,
                                entry, sizeof(tbatch_entry_t));
}

int
tpb_rawdb_entry_append_kernel(const char *workspace,
                              const kernel_entry_t *entry)
{
    if (!workspace || !entry) return TPBE_NULLPTR_ARG;
    return entry_append_generic(workspace, TPB_RAWDB_DOM_KERNEL,
                                entry, sizeof(kernel_entry_t));
}

int
tpb_rawdb_entry_append_task(const char *workspace,
                            const task_entry_t *entry)
{
    if (!workspace || !entry) return TPBE_NULLPTR_ARG;
    return entry_append_generic(workspace, TPB_RAWDB_DOM_TASK,
                                entry, sizeof(task_entry_t));
}

int
tpb_rawdb_entry_list_tbatch(const char *workspace,
                            tbatch_entry_t **entries,
                            int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return entry_list_generic(workspace, TPB_RAWDB_DOM_TBATCH,
                              (void **)entries, count,
                              sizeof(tbatch_entry_t));
}

int
tpb_rawdb_entry_list_kernel(const char *workspace,
                            kernel_entry_t **entries,
                            int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return entry_list_generic(workspace, TPB_RAWDB_DOM_KERNEL,
                              (void **)entries, count,
                              sizeof(kernel_entry_t));
}

int
tpb_rawdb_entry_list_task(const char *workspace,
                          task_entry_t **entries,
                          int *count)
{
    if (!workspace || !entries || !count) {
        return TPBE_NULLPTR_ARG;
    }
    return entry_list_generic(workspace, TPB_RAWDB_DOM_TASK,
                              (void **)entries, count,
                              sizeof(task_entry_t));
}
