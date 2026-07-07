/*
 * rafdb-l2-runtime-environment.c
 * Runtime environment domain entry, ID, and record I/O (TPB_RAF_DOM_RTENV).
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#include "../../include/tpb-public.h"
#include "../tpb-types.h"
#include "rafdb-l1-internal.h"
#include "tpb-raf-runtime-environment.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(rtenv_entry_disk_t) == TPB_RAF_RTENV_ENTRY_SIZE,
               "rtenv_entry_disk_t on-disk size");
#endif

/* Local Function Prototypes */
static void _sf_disk_to_entry(const rtenv_entry_disk_t *disk,
                              tpb_raf_rtenv_entry_t *ent);
static void _sf_entry_to_disk(const tpb_raf_rtenv_entry_t *ent,
                                rtenv_entry_disk_t *disk);
static int _sf_write_i32(FILE *fp, int32_t v);
static int _sf_read_i32(FILE *fp, int32_t *v);
static int _sf_write_entry_fields(FILE *fp, const tpb_raf_rtenv_entry_t *ent);
static int _sf_read_entry_fields(FILE *fp, tpb_raf_rtenv_entry_t *ent);
static int _sf_write_attr_meta(FILE *fp, const tpb_raf_rtenv_attr_t *attr);
static int _sf_read_attr_meta(FILE *fp, tpb_raf_rtenv_attr_t *attr);
static int _sf_name_exists(const char *workspace, const char *name);

static void
_sf_disk_to_entry(const rtenv_entry_disk_t *disk, tpb_raf_rtenv_entry_t *ent)
{
    memset(ent, 0, sizeof(*ent));
    ent->id = disk->id;
    memcpy(ent->name, disk->name, sizeof(ent->name));
    memcpy(ent->hostname, disk->hostname, sizeof(ent->hostname));
    ent->utc_bits = disk->utc_bits;
    ent->inherit_from = disk->inherit_from;
    ent->derive_to = disk->derive_to;
    ent->ntask = disk->ntask;
    ent->ntbatch = disk->ntbatch;
    memcpy(ent->note, disk->note, sizeof(ent->note));
    ent->napp = disk->napp;
    ent->nenv = disk->nenv;
    memcpy(ent->reserve, disk->reserve, sizeof(ent->reserve));
}

static void
_sf_entry_to_disk(const tpb_raf_rtenv_entry_t *ent, rtenv_entry_disk_t *disk)
{
    memset(disk, 0, sizeof(*disk));
    disk->id = ent->id;
    memcpy(disk->name, ent->name, sizeof(disk->name));
    memcpy(disk->hostname, ent->hostname, sizeof(disk->hostname));
    disk->utc_bits = ent->utc_bits;
    disk->inherit_from = ent->inherit_from;
    disk->derive_to = ent->derive_to;
    disk->ntask = ent->ntask;
    disk->ntbatch = ent->ntbatch;
    memcpy(disk->note, ent->note, sizeof(disk->note));
    disk->napp = ent->napp;
    disk->nenv = ent->nenv;
    memcpy(disk->reserve, ent->reserve, sizeof(disk->reserve));
}

static int
_sf_write_i32(FILE *fp, int32_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

static int
_sf_read_i32(FILE *fp, int32_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

static int
_sf_write_entry_fields(FILE *fp, const tpb_raf_rtenv_entry_t *ent)
{
    unsigned char zreserve[TPB_RAF_RTENV_RESERVE_LEN];

    if (_sf_write_i32(fp, ent->id) != 0) {
        return -1;
    }
    if (fwrite(ent->name, 1, TPB_RAF_RTENV_NAME_LEN, fp)
        != TPB_RAF_RTENV_NAME_LEN) {
        return -1;
    }
    if (fwrite(ent->hostname, 1, TPB_RAF_RTENV_HOST_LEN, fp)
        != TPB_RAF_RTENV_HOST_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_write_u64(fp, ent->utc_bits) != 0) {
        return -1;
    }
    if (_sf_write_i32(fp, ent->inherit_from) != 0) {
        return -1;
    }
    if (_sf_write_i32(fp, ent->derive_to) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent->ntask) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent->ntbatch) != 0) {
        return -1;
    }
    if (fwrite(ent->note, 1, TPB_RAF_RTENV_NOTE_LEN, fp)
        != TPB_RAF_RTENV_NOTE_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent->napp) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent->nenv) != 0) {
        return -1;
    }
    memset(zreserve, 0, sizeof(zreserve));
    if (fwrite(zreserve, 1, sizeof(zreserve), fp) != sizeof(zreserve)) {
        return -1;
    }
    return 0;
}

static int
_sf_read_entry_fields(FILE *fp, tpb_raf_rtenv_entry_t *ent)
{
    if (_sf_read_i32(fp, &ent->id) != 0) {
        return -1;
    }
    if (fread(ent->name, 1, TPB_RAF_RTENV_NAME_LEN, fp)
        != TPB_RAF_RTENV_NAME_LEN) {
        return -1;
    }
    if (fread(ent->hostname, 1, TPB_RAF_RTENV_HOST_LEN, fp)
        != TPB_RAF_RTENV_HOST_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_read_u64(fp, &ent->utc_bits) != 0) {
        return -1;
    }
    if (_sf_read_i32(fp, &ent->inherit_from) != 0) {
        return -1;
    }
    if (_sf_read_i32(fp, &ent->derive_to) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &ent->ntask) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &ent->ntbatch) != 0) {
        return -1;
    }
    if (fread(ent->note, 1, TPB_RAF_RTENV_NOTE_LEN, fp)
        != TPB_RAF_RTENV_NOTE_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &ent->napp) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &ent->nenv) != 0) {
        return -1;
    }
    if (fread(ent->reserve, 1, TPB_RAF_RTENV_RESERVE_LEN, fp)
        != TPB_RAF_RTENV_RESERVE_LEN) {
        return -1;
    }
    return 0;
}

static int
_sf_write_attr_meta(FILE *fp, const tpb_raf_rtenv_attr_t *attr)
{
    tpb_raf_rtenv_entry_t ent;

    memset(&ent, 0, sizeof(ent));
    ent.id = attr->id;
    memcpy(ent.name, attr->name, sizeof(ent.name));
    memcpy(ent.hostname, attr->hostname, sizeof(ent.hostname));
    ent.utc_bits = attr->utc_bits;
    ent.inherit_from = attr->inherit_from;
    ent.derive_to = attr->derive_to;
    ent.ntask = attr->ntask;
    ent.ntbatch = attr->ntbatch;
    memcpy(ent.note, attr->note, sizeof(ent.note));
    ent.napp = attr->napp;
    ent.nenv = attr->nenv;

    if (_sf_write_i32(fp, ent.id) != 0) {
        return -1;
    }
    if (fwrite(ent.name, 1, TPB_RAF_RTENV_NAME_LEN, fp)
        != TPB_RAF_RTENV_NAME_LEN) {
        return -1;
    }
    if (fwrite(ent.hostname, 1, TPB_RAF_RTENV_HOST_LEN, fp)
        != TPB_RAF_RTENV_HOST_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_write_u64(fp, ent.utc_bits) != 0) {
        return -1;
    }
    if (_sf_write_i32(fp, ent.inherit_from) != 0) {
        return -1;
    }
    if (_sf_write_i32(fp, ent.derive_to) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent.ntask) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent.ntbatch) != 0) {
        return -1;
    }
    if (fwrite(ent.note, 1, TPB_RAF_RTENV_NOTE_LEN, fp)
        != TPB_RAF_RTENV_NOTE_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent.napp) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, ent.nenv) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_write_u32(fp, attr->nheader) != 0) {
        return -1;
    }
    if (fwrite(attr->reserve, 1, TPB_RAF_RTENV_RESERVE_LEN, fp)
        != TPB_RAF_RTENV_RESERVE_LEN) {
        return -1;
    }
    return 0;
}

static int
_sf_read_attr_meta(FILE *fp, tpb_raf_rtenv_attr_t *attr)
{
    if (_sf_read_i32(fp, &attr->id) != 0) {
        return -1;
    }
    if (fread(attr->name, 1, TPB_RAF_RTENV_NAME_LEN, fp)
        != TPB_RAF_RTENV_NAME_LEN) {
        return -1;
    }
    if (fread(attr->hostname, 1, TPB_RAF_RTENV_HOST_LEN, fp)
        != TPB_RAF_RTENV_HOST_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_read_u64(fp, &attr->utc_bits) != 0) {
        return -1;
    }
    if (_sf_read_i32(fp, &attr->inherit_from) != 0) {
        return -1;
    }
    if (_sf_read_i32(fp, &attr->derive_to) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->ntask) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->ntbatch) != 0) {
        return -1;
    }
    if (fread(attr->note, 1, TPB_RAF_RTENV_NOTE_LEN, fp)
        != TPB_RAF_RTENV_NOTE_LEN) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->napp) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nenv) != 0) {
        return -1;
    }
    if (_tpb_raf_l1_read_u32(fp, &attr->nheader) != 0) {
        return -1;
    }
    if (fread(attr->reserve, 1, TPB_RAF_RTENV_RESERVE_LEN, fp)
        != TPB_RAF_RTENV_RESERVE_LEN) {
        return -1;
    }
    return 0;
}

static int
_sf_name_exists(const char *workspace, const char *name)
{
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int i;
    int found = 0;
    int err;

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            found = 1;
            break;
        }
    }
    free(entries);
    return found;
}

/**
 * @brief Allocate the next domain-local RTEnv ID (max existing + 1, or 0).
 */
int
tpb_raf_rtenv_alloc_next_id(const char *workspace, int32_t *id_out)
{
    char fpath[TPB_RAF_PATH_MAX];
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int32_t max_id = -1;
    int i;
    int fd;
    int err;

    if (!workspace || !id_out) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_entry_path(workspace, TPB_RAF_DOM_RTENV,
                                 fpath, sizeof(fpath));
    fd = open(fpath, O_RDONLY);
    if (fd >= 0) {
        if (flock(fd, LOCK_EX) != 0) {
            (void)close(fd);
            TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_FILE_IO_FAIL, NULL);
        }
        (void)close(fd);
    }

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_RAF_L2_RTENV, err, NULL);
    }
    for (i = 0; i < count; i++) {
        if (entries[i].id > max_id) {
            max_id = entries[i].id;
        }
    }
    free(entries);
    *id_out = max_id + 1;
    return TPBE_SUCCESS;
}

/**
 * @brief Append a runtime environment entry to .tpbe (name must be unique).
 */
int
tpb_raf_entry_append_rtenv(const char *workspace,
                             const tpb_raf_rtenv_entry_t *entry)
{
    rtenv_entry_disk_t disk;

    if (!workspace || !entry) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_NULLPTR_ARG, NULL);
    }
    if (_sf_name_exists(workspace, entry->name)) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_LIST_DUP, NULL);
    }
    _sf_entry_to_disk(entry, &disk);
    TPB_PROPAGATE(TPB_MOD_RAF_L2_RTENV,
                  _tpb_raf_l1_entry_append(workspace, TPB_RAF_DOM_RTENV,
                                           &disk, sizeof(rtenv_entry_disk_t)),
                  NULL);
    return TPBE_SUCCESS;
}

/**
 * @brief List all runtime environment entries from .tpbe.
 */
int
tpb_raf_entry_list_rtenv(const char *workspace,
                          tpb_raf_rtenv_entry_t **entries,
                          int *count)
{
    rtenv_entry_disk_t *disk_buf = NULL;
    tpb_raf_rtenv_entry_t *out = NULL;
    int n = 0;
    int i;
    int err;

    if (!workspace || !entries || !count) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_NULLPTR_ARG, NULL);
    }
    TPB_PROPAGATE(TPB_MOD_RAF_L2_RTENV,
                  _tpb_raf_l1_entry_list(workspace, TPB_RAF_DOM_RTENV,
                                         (void **)&disk_buf, &n,
                                         sizeof(rtenv_entry_disk_t)),
                  NULL);
    if (n == 0) {
        *entries = NULL;
        *count = 0;
        return TPBE_SUCCESS;
    }
    out = (tpb_raf_rtenv_entry_t *)calloc((size_t)n, sizeof(*out));
    if (!out) {
        free(disk_buf);
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_MALLOC_FAIL, NULL);
    }
    for (i = 0; i < n; i++) {
        _sf_disk_to_entry(&disk_buf[i], &out[i]);
    }
    free(disk_buf);
    *entries = out;
    *count = n;
    return TPBE_SUCCESS;
}

/**
 * @brief Write a runtime environment .tpbr record file.
 */
int
tpb_raf_record_write_rtenv(const char *workspace,
                           const tpb_raf_rtenv_attr_t *attr,
                           const tpb_meta_header_t *headers,
                           const void *data,
                           uint64_t datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    FILE *fp;
    uint64_t metasize;

    if (!workspace || !attr) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_NULLPTR_ARG, NULL);
    }
    if (attr->nheader > 0 && headers == NULL) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_record_path_int32(workspace, TPB_RAF_DOM_RTENV,
                                        attr->id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }

    metasize = (uint64_t)TPB_RAF_RTENV_RECORD_META_SIZE;
    metasize += (uint64_t)attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                        TPB_RAF_DOM_RTENV,
                        TPB_RAF_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, metasize) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_write_u64(fp, datasize) != 0) {
        goto fail;
    }
    if (_sf_write_attr_meta(fp, attr) != 0) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_tpb_raf_l1_write_headers(fp, headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    if (_tpb_raf_l1_write_magic_and_data(fp, TPB_RAF_DOM_RTENV,
                                         data, datasize) != 0) {
        goto fail;
    }

    if (fclose(fp) != 0) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;

fail:
    fclose(fp);
    TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_FILE_IO_FAIL, NULL);
}

/**
 * @brief Read a runtime environment .tpbr record by numeric ID.
 */
int
tpb_raf_record_read_rtenv(const char *workspace,
                           int32_t id,
                           tpb_raf_rtenv_attr_t *attr,
                           tpb_meta_header_t **headers,
                           void **data,
                           uint64_t *datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    FILE *fp;
    uint64_t metasize;
    uint64_t dsize;

    if (!workspace || !attr || !headers || !data || !datasize) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_NULLPTR_ARG, NULL);
    }

    _tpb_raf_l1_build_record_path_int32(workspace, TPB_RAF_DOM_RTENV,
                                        id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) {
        TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_FILE_IO_FAIL, NULL);
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_RTENV,
                                TPB_RAF_POS_START)) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &metasize) != 0) {
        goto fail;
    }
    if (_tpb_raf_l1_read_u64(fp, &dsize) != 0) {
        goto fail;
    }
    if (_sf_read_attr_meta(fp, attr) != 0) {
        goto fail;
    }

    *headers = NULL;
    if (attr->nheader > 0) {
        if (_tpb_raf_l1_read_headers(fp, headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_RTENV,
                                TPB_RAF_POS_SPLIT)) {
        goto fail_hdrs;
    }

    *data = NULL;
    *datasize = dsize;
    if (dsize > 0) {
        void *buf = malloc((size_t)dsize);

        if (!buf) {
            goto fail_hdrs;
        }
        if (fread(buf, 1, (size_t)dsize, fp) != (size_t)dsize) {
            free(buf);
            goto fail_hdrs;
        }
        *data = buf;
    }

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_payload;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                TPB_RAF_DOM_RTENV,
                                TPB_RAF_POS_END)) {
        goto fail_payload;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail_payload:
    if (*data) {
        free(*data);
        *data = NULL;
    }
fail_hdrs:
    if (*headers) {
        tpb_raf_free_headers(*headers, attr->nheader);
        *headers = NULL;
    }
fail:
    fclose(fp);
    TPB_FAIL(TPB_MOD_RAF_L2_RTENV, TPBE_FILE_IO_FAIL, NULL);
}
