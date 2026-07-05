/*
 * rafdb-l3-kernel-patch.c
 * Kernel active-flag patching and metadata key updates.
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
#include "rafdb-l1-types.h"
#include "tpb-raf-kernel-meta.h"

/* kernel_entry_t.active on-disk offset from entry start */
#define TPB_KERNEL_ENTRY_ACTIVE_OFF  116

/* Local Function Prototypes */
static int _sf_split_meta_key(const char *full_key, char *section_out,
                              size_t section_len, char *subkey_out,
                              size_t subkey_len);

static int
_sf_split_meta_key(const char *full_key, char *section_out, size_t section_len,
                   char *subkey_out, size_t subkey_len)
{
    const char *dot;

    if (full_key == NULL || section_out == NULL || subkey_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    dot = strchr(full_key, '.');
    if (dot == NULL || dot == full_key) {
        return TPBE_KERN_ARG_FAIL;
    }

    if ((size_t)(dot - full_key) >= section_len) {
        return TPBE_KERN_ARG_FAIL;
    }
    memcpy(section_out, full_key, (size_t)(dot - full_key));
    section_out[dot - full_key] = '\0';
    snprintf(subkey_out, subkey_len, "%s", dot + 1);
    return TPBE_SUCCESS;
}

/**
 * @brief Patch active flag in kernel .tpbe entry for a KernelID.
 */
int
tpb_raf_entry_patch_kernel_active(const char *workspace,
                                  const unsigned char kernel_id[20],
                                  uint32_t active)
{
    char fpath[TPB_RAF_PATH_MAX];
    int fd;
    FILE *fp;
    struct stat st;
    long fsize;
    long off;
    int i;
    int n;
    kernel_entry_t *entries;

    if (workspace == NULL || kernel_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (tpb_raf_entry_list_kernel(workspace, &entries, &n) != TPBE_SUCCESS) {
        return TPBE_FILE_IO_FAIL;
    }

    off = -1;
    for (i = 0; i < n; i++) {
        if (memcmp(entries[i].kernel_id, kernel_id, 20) == 0) {
            off = (long)i;
            break;
        }
    }
    free(entries);
    if (off < 0) {
        return TPBE_LIST_NOT_FOUND;
    }

    snprintf(fpath, sizeof(fpath), "%s/%s/%s",
             workspace, TPB_RAF_KERNEL_DIR, TPB_RAF_KERNEL_ENTRY);

    fd = open(fpath, O_RDWR);
    if (fd < 0) {
        return TPBE_FILE_IO_FAIL;
    }
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return TPBE_FILE_IO_FAIL;
    }

    fp = fdopen(fd, "r+b");
    if (fp == NULL) {
        close(fd);
        return TPBE_FILE_IO_FAIL;
    }

    if (fstat(fd, &st) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    fsize = (long)st.st_size;
    if (fsize < (long)(2 * TPB_RAF_MAGIC_LEN)) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    {
        long entry_off = (long)TPB_RAF_MAGIC_LEN +
            off * (long)sizeof(kernel_entry_t) +
            (long)TPB_KERNEL_ENTRY_ACTIVE_OFF;

        if (fseek(fp, entry_off, SEEK_SET) != 0) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        if (fwrite(&active, sizeof(active), 1, fp) != 1) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
    }

    fclose(fp);
    return TPBE_SUCCESS;
}

/**
 * @brief Patch active flag in kernel .tpbr fixed attributes for a KernelID.
 */
int
tpb_raf_record_patch_kernel_active(const char *workspace,
                                   const unsigned char kernel_id[20],
                                   uint32_t active)
{
    char fpath[TPB_RAF_PATH_MAX];
    char hex[41];
    FILE *fp;
    long off;
    const long active_off = 8 + 8 + 8 + 60 + 256 + 64 + 2048 + 4 + 4 + 4 + 4;

    if (workspace == NULL || kernel_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_raf_id_to_hex(kernel_id, hex);
    snprintf(fpath, sizeof(fpath), "%s/%s/%s.tpbr",
             workspace, TPB_RAF_KERNEL_DIR, hex);

    fp = fopen(fpath, "r+b");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }

    off = active_off;
    if (fseek(fp, off, SEEK_SET) != 0) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    if (fwrite(&active, sizeof(active), 1, fp) != 1) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }

    fclose(fp);
    return TPBE_SUCCESS;
}

/**
 * @brief Deactivate all kernel entries with the given logical name except
 *        skip_id.
 */
int
tpb_raf_kernel_deactivate_same_name(const char *workspace,
                                     const char *kernel_name,
                                     const unsigned char skip_id[20])
{
    kernel_entry_t *entries;
    int n;
    int i;
    int err;

    if (workspace == NULL || kernel_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    err = tpb_raf_entry_list_kernel(workspace, &entries, &n);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    for (i = 0; i < n; i++) {
        if (strcmp(entries[i].kernel_name, kernel_name) != 0) {
            continue;
        }
        if (skip_id != NULL &&
            memcmp(entries[i].kernel_id, skip_id, 20) == 0) {
            continue;
        }
        (void)tpb_raf_entry_patch_kernel_active(workspace,
                                                entries[i].kernel_id, 0);
        (void)tpb_raf_record_patch_kernel_active(workspace,
                                                 entries[i].kernel_id, 0);
    }

    free(entries);
    return TPBE_SUCCESS;
}

/**
 * @brief Update one metadata key on an existing kernel .tpbr record.
 */
int
tpb_raf_kernel_update_meta_key(const char *workspace,
                               const unsigned char kernel_id[20],
                               const char *full_key,
                               const char *value)
{
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    char section[64];
    char subkey[256];
    int hdr_idx;
    char *payload = NULL;
    size_t payload_cap = 0;
    int err;
    uint32_t i;
    uint64_t new_datasize;
    void *new_data;
    uint8_t *ptr;

    if (workspace == NULL || kernel_id == NULL ||
        full_key == NULL || value == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    err = _sf_split_meta_key(full_key, section, sizeof(section),
                             subkey, sizeof(subkey));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, kernel_id, &attr,
                                     &data, &datasize);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    hdr_idx = tpb_raf_kernel_find_header(&attr, section);
    if (hdr_idx < 0) {
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return TPBE_LIST_NOT_FOUND;
    }

    {
        uint64_t off = 0;
        uint32_t j;

        for (j = 0; j < (uint32_t)hdr_idx; j++) {
            off += attr.headers[j].data_size;
        }
        if (data == NULL || off + attr.headers[hdr_idx].data_size > datasize) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            return TPBE_FILE_IO_FAIL;
        }
        payload = strdup((const char *)((const uint8_t *)data + off));
        if (payload == NULL) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            return TPBE_MALLOC_FAIL;
        }
        payload_cap = strlen(payload) + 1;
    }

    err = tpb_raf_kernel_meta_kv_set(&payload, &payload_cap, subkey, value);
    if (err != TPBE_SUCCESS) {
        free(payload);
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return err;
    }

    attr.headers[hdr_idx].dimsizes[0] = strlen(payload) + 1;
    attr.headers[hdr_idx].data_size = strlen(payload) + 1;

    new_datasize = 0;
    for (i = 0; i < attr.nheader; i++) {
        new_datasize += attr.headers[i].data_size;
    }

    new_data = calloc(1, (size_t)new_datasize);
    if (new_data == NULL) {
        free(payload);
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return TPBE_MALLOC_FAIL;
    }

    ptr = (uint8_t *)new_data;
    for (i = 0; i < attr.nheader; i++) {
        if (i == (uint32_t)hdr_idx) {
            memcpy(ptr, payload, (size_t)attr.headers[i].data_size);
        } else {
            uint64_t off = 0;
            uint32_t j;

            for (j = 0; j < i; j++) {
                off += attr.headers[j].data_size;
            }
            if (data != NULL && attr.headers[i].data_size > 0) {
                memcpy(ptr, (const uint8_t *)data + off,
                       (size_t)attr.headers[i].data_size);
            }
        }
        ptr += attr.headers[i].data_size;
    }

    free(payload);
    free(data);

    err = tpb_raf_record_write_kernel(workspace, &attr, new_data, new_datasize);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(new_data);
    return err;
}
