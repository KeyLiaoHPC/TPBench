/*
 * tpb-raf-record.c
 * Record file (.tpbr) write and read operations including header
 * serialization for tbatch, kernel, and task domains.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../tpb-types.h"
#include "tpb-raf-types.h"

/* First task header data_size / dimsizes[0] offsets from .tpbr file start */
#define TPB_RAF_TASK_FILE_OFF_METASIZE      8
#define TPB_RAF_TASK_FILE_OFF_DATASIZE      16
/* After magic(8)+metasize(8)+datasize(8)+fixed attrs(156): nheader u32 */
#define TPB_RAF_TASK_FILE_OFF_NHEADER       172
#define TPB_RAF_TASK_FILE_HDR0_BASE       308
#define TPB_RAF_TASK_FILE_HDR0_DATA_SIZE  316
#define TPB_RAF_TASK_FILE_HDR0_NAME       340
#define TPB_RAF_TASK_FILE_HDR0_DIM0       2644
#define TPB_RAF_TASK_CAPSULE_HDR_NAME     "TPBLINK::TaskID"

/* Local Function Prototypes */
static void _sf_build_record_path(const char *workspace,
                                  uint8_t domain,
                                  const unsigned char id[20],
                                  char *out, size_t outlen);
static int _sf_read_headers(FILE *fp, tpb_meta_header_t **hdrs_out,
                            uint32_t n);
static int _sf_read_u32(FILE *fp, uint32_t *v);
static int _sf_read_u64(FILE *fp, uint64_t *v);
static int _sf_write_headers(FILE *fp, const tpb_meta_header_t *hdrs,
                             uint32_t n);
static int _sf_write_magic_and_data(FILE *fp, uint8_t domain,
                                    const void *data,
                                    uint64_t datasize);
static int _sf_write_u32(FILE *fp, uint32_t v);
static int _sf_write_u64(FILE *fp, uint64_t v);

static int
_sf_write_u64(FILE *fp, uint64_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

static int
_sf_write_u32(FILE *fp, uint32_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

static int
_sf_read_u64(FILE *fp, uint64_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

static int
_sf_read_u32(FILE *fp, uint32_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

static void
_sf_build_record_path(const char *workspace, uint8_t domain,
                  const unsigned char id[20],
                  char *out, size_t outlen)
{
    char hex[41];
    const char *dir;

    tpb_raf_id_to_hex(id, hex);

    switch (domain) {
    case TPB_RAF_DOM_KERNEL:
        dir = TPB_RAF_KERNEL_DIR;
        break;
    case TPB_RAF_DOM_TASK:
        dir = TPB_RAF_TASK_DIR;
        break;
    default:
        dir = TPB_RAF_TBATCH_DIR;
        break;
    }
    snprintf(out, outlen, "%s/%s/%s.tpbr",
             workspace, dir, hex);
}

/**
 * @brief Find which domain directory contains a .tpbr for the given ID.
 */
int
tpb_raf_find_record(const char *workspace,
                      const unsigned char id[20],
                      uint8_t *domain_out)
{
    char path[TPB_RAF_PATH_MAX];
    struct stat st;
    uint8_t doms[3];
    int d;

    if (!workspace || !id || !domain_out) {
        return TPBE_NULLPTR_ARG;
    }

    doms[0] = TPB_RAF_DOM_TBATCH;
    doms[1] = TPB_RAF_DOM_KERNEL;
    doms[2] = TPB_RAF_DOM_TASK;

    for (d = 0; d < 3; d++) {
        _sf_build_record_path(workspace, doms[d], id, path, sizeof(path));
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            *domain_out = doms[d];
            return TPBE_SUCCESS;
        }
    }
    return TPBE_FILE_IO_FAIL;
}

/*
 * Serialize headers to file. Each header on disk:
 *   block_size(4) + ndim(4) + data_size(8) + type_bits(4) + _reserve(4) +
 *   uattr_bits(8) + name(256) + note(2048) +
 *   dimsizes[ndim](8*ndim) + dimnames[ndim][64](64*ndim)
 */
static int
_sf_write_headers(FILE *fp, const tpb_meta_header_t *hdrs,
              uint32_t n)
{
    uint32_t i, j;

    for (i = 0; i < n; i++) {
        uint32_t bs = TPB_RAF_HDR_FIXED_SIZE;

        if (_sf_write_u32(fp, bs) != 0) return -1;
        if (_sf_write_u32(fp, hdrs[i].ndim) != 0) return -1;
        if (_sf_write_u64(fp, hdrs[i].data_size) != 0) return -1;
        if (_sf_write_u32(fp, hdrs[i].type_bits) != 0) return -1;
        if (_sf_write_u32(fp, hdrs[i]._reserve) != 0) return -1;
        if (_sf_write_u64(fp, hdrs[i].uattr_bits) != 0) return -1;
        if (fwrite(hdrs[i].name, 1, 256, fp) != 256) return -1;
        if (fwrite(hdrs[i].note, 1, 2048, fp) != 2048) {
            return -1;
        }

        for (j = 0; j < hdrs[i].ndim; j++) {
            if (_sf_write_u64(fp, hdrs[i].dimsizes[j]) != 0) {
                return -1;
            }
        }
        for (j = 0; j < hdrs[i].ndim; j++) {
            if (fwrite(hdrs[i].dimnames[j], 1, 64, fp) != 64) {
                return -1;
            }
        }
    }
    return 0;
}

/*
 * Read headers from file.
 */
static int
_sf_read_headers(FILE *fp, tpb_meta_header_t **hdrs_out,
             uint32_t n)
{
    tpb_meta_header_t *hdrs;
    uint32_t i, j;

    if (n == 0) {
        *hdrs_out = NULL;
        return 0;
    }

    hdrs = (tpb_meta_header_t *)calloc(n, sizeof(*hdrs));
    if (!hdrs) return -1;

    for (i = 0; i < n; i++) {
        if (_sf_read_u32(fp, &hdrs[i].block_size) != 0) goto fail;
        if (_sf_read_u32(fp, &hdrs[i].ndim) != 0) goto fail;
        if (_sf_read_u64(fp, &hdrs[i].data_size) != 0) goto fail;
        if (_sf_read_u32(fp, &hdrs[i].type_bits) != 0) goto fail;
        if (_sf_read_u32(fp, &hdrs[i]._reserve) != 0) goto fail;
        if (_sf_read_u64(fp, &hdrs[i].uattr_bits) != 0) goto fail;
        if (fread(hdrs[i].name, 1, 256, fp) != 256) goto fail;
        if (fread(hdrs[i].note, 1, 2048, fp) != 2048) goto fail;

        for (j = 0; j < hdrs[i].ndim; j++) {
            if (_sf_read_u64(fp, &hdrs[i].dimsizes[j]) != 0) {
                goto fail;
            }
        }
        for (j = 0; j < hdrs[i].ndim; j++) {
            if (fread(hdrs[i].dimnames[j], 1, 64, fp) != 64) {
                goto fail;
            }
        }
    }

    *hdrs_out = hdrs;
    return 0;

fail:
    free(hdrs);
    return -1;
}

/*
 * Write the record_magic, data, and end_magic.
 */
static int
_sf_write_magic_and_data(FILE *fp, uint8_t domain,
                     const void *data, uint64_t datasize)
{
    unsigned char magic[TPB_RAF_MAGIC_LEN];

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD, domain,
                          TPB_RAF_POS_SPLIT, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        return -1;
    }

    if (datasize > 0 && data) {
        if (fwrite(data, 1, (size_t)datasize, fp)
            != (size_t)datasize) {
            return -1;
        }
    }

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD, domain,
                          TPB_RAF_POS_END, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        return -1;
    }

    return 0;
}

/**
 * @brief Free header array allocated by record read functions.
 */
void
tpb_raf_free_headers(tpb_meta_header_t *headers,
                       uint32_t nheader)
{
    (void)nheader;
    free(headers);
}

/**
 * @brief Write a tbatch .tpbr record file.
 */
int
tpb_raf_record_write_tbatch(const char *workspace,
                              const tbatch_attr_t *attr,
                              const void *data,
                              uint64_t datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint32_t i;

    if (!workspace || !attr) return TPBE_NULLPTR_ARG;

    _sf_build_record_path(workspace, TPB_RAF_DOM_TBATCH,
                      attr->tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Calculate metasize: fixed attrs + reserve + headers */
    /* Fixed: 20+20+20+8+8+8+64+64+4+4+4+4+4+4 = 236 bytes */
    metasize = 236 + TPB_RAF_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    /* meta_magic */
    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                          TPB_RAF_DOM_TBATCH,
                          TPB_RAF_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }

    /* metasize, datasize */
    if (_sf_write_u64(fp, metasize) != 0) goto fail;
    if (_sf_write_u64(fp, datasize) != 0) goto fail;

    /* Attributes */
    if (fwrite(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->derive_to, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->inherit_from, 1, 20, fp) != 20) goto fail;
    if (_sf_write_u64(fp, attr->utc_bits) != 0) goto fail;
    if (_sf_write_u64(fp, attr->btime) != 0) goto fail;
    if (_sf_write_u64(fp, attr->duration) != 0) goto fail;
    if (fwrite(attr->hostname, 1, 64, fp) != 64) goto fail;
    if (fwrite(attr->username, 1, 64, fp) != 64) goto fail;
    if (_sf_write_u32(fp, attr->front_pid) != 0) goto fail;
    if (_sf_write_u32(fp, attr->nkernel) != 0) goto fail;
    if (_sf_write_u32(fp, attr->ntask) != 0) goto fail;
    if (_sf_write_u32(fp, attr->nscore) != 0) goto fail;
    if (_sf_write_u32(fp, attr->batch_type) != 0) goto fail;
    if (_sf_write_u32(fp, attr->nheader) != 0) goto fail;

    /* Reserve */
    memset(reserve, 0, TPB_RAF_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    /* Headers */
    if (attr->nheader > 0) {
        if (_sf_write_headers(fp, attr->headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    /* Record data section */
    if (_sf_write_magic_and_data(fp, TPB_RAF_DOM_TBATCH,
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
 * @brief Read a tbatch .tpbr record file.
 */
int
tpb_raf_record_read_tbatch(const char *workspace,
                             const unsigned char tbatch_id[20],
                             tbatch_attr_t *attr,
                             void **data,
                             uint64_t *datasize)
{
    char fpath[TPB_RAF_PATH_MAX];
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize, ds;

    if (!workspace || !tbatch_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    _sf_build_record_path(workspace, TPB_RAF_DOM_TBATCH,
                      tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* meta_magic */
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TBATCH,
                                  TPB_RAF_POS_START)) {
        goto fail;
    }

    if (_sf_read_u64(fp, &metasize) != 0) goto fail;
    if (_sf_read_u64(fp, &ds) != 0) goto fail;

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->derive_to, 1, 20, fp) != 20) goto fail;
    if (fread(attr->inherit_from, 1, 20, fp) != 20) goto fail;
    if (_sf_read_u64(fp, &attr->utc_bits) != 0) goto fail;
    if (_sf_read_u64(fp, &attr->btime) != 0) goto fail;
    if (_sf_read_u64(fp, &attr->duration) != 0) goto fail;
    if (fread(attr->hostname, 1, 64, fp) != 64) goto fail;
    if (fread(attr->username, 1, 64, fp) != 64) goto fail;
    if (_sf_read_u32(fp, &attr->front_pid) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->nkernel) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->ntask) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->nscore) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->batch_type) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->nheader) != 0) goto fail;

    if (fread(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_sf_read_headers(fp, &attr->headers,
                         attr->nheader) != 0) {
            goto fail;
        }
    } else {
        attr->headers = NULL;
    }

    /* record_magic */
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TBATCH,
                                  TPB_RAF_POS_SPLIT)) {
        goto fail_hdrs;
    }

    /* Read data */
    if (data && datasize) {
        *datasize = ds;
        if (ds > 0) {
            *data = malloc((size_t)ds);
            if (!*data) goto fail_hdrs;
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

    /* end_magic */
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TBATCH,
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
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint32_t i;

    if (!workspace || !attr) return TPBE_NULLPTR_ARG;

    _sf_build_record_path(workspace, TPB_RAF_DOM_KERNEL,
                      attr->kernel_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Fixed attrs: 6*20 + 256 + 64 + 2048 + 5*4 = 2508 */
    metasize = 2508 + TPB_RAF_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                          TPB_RAF_DOM_KERNEL,
                          TPB_RAF_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }

    if (_sf_write_u64(fp, metasize) != 0) goto fail;
    if (_sf_write_u64(fp, datasize) != 0) goto fail;

    if (fwrite(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->derive_to, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->inherit_from, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->src_sha1, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->so_sha1, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->bin_sha1, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->kernel_name, 1, 256, fp) != 256) goto fail;
    if (fwrite(attr->version, 1, 64, fp) != 64) goto fail;
    if (fwrite(attr->description, 1, 2048, fp) != 2048) {
        goto fail;
    }
    if (_sf_write_u32(fp, attr->nparm) != 0) goto fail;
    if (_sf_write_u32(fp, attr->nmetric) != 0) goto fail;
    if (_sf_write_u32(fp, attr->kctrl) != 0) goto fail;
    if (_sf_write_u32(fp, attr->nheader) != 0) goto fail;
    if (_sf_write_u32(fp, attr->reserve) != 0) goto fail;

    memset(reserve, 0, TPB_RAF_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_sf_write_headers(fp, attr->headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    if (_sf_write_magic_and_data(fp, TPB_RAF_DOM_KERNEL,
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
    unsigned char reserve[TPB_RAF_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize, ds;

    if (!workspace || !kernel_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    _sf_build_record_path(workspace, TPB_RAF_DOM_KERNEL,
                      kernel_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_KERNEL,
                                  TPB_RAF_POS_START)) {
        goto fail;
    }

    if (_sf_read_u64(fp, &metasize) != 0) goto fail;
    if (_sf_read_u64(fp, &ds) != 0) goto fail;

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->derive_to, 1, 20, fp) != 20) goto fail;
    if (fread(attr->inherit_from, 1, 20, fp) != 20) goto fail;
    if (fread(attr->src_sha1, 1, 20, fp) != 20) goto fail;
    if (fread(attr->so_sha1, 1, 20, fp) != 20) goto fail;
    if (fread(attr->bin_sha1, 1, 20, fp) != 20) goto fail;
    if (fread(attr->kernel_name, 1, 256, fp) != 256) goto fail;
    if (fread(attr->version, 1, 64, fp) != 64) goto fail;
    if (fread(attr->description, 1, 2048, fp) != 2048) goto fail;
    if (_sf_read_u32(fp, &attr->nparm) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->nmetric) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->kctrl) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->nheader) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->reserve) != 0) goto fail;

    if (fread(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_sf_read_headers(fp, &attr->headers,
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
            if (!*data) goto fail_hdrs;
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
    return TPBE_FILE_IO_FAIL;
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
    uint32_t i;

    if (!workspace || !attr) return TPBE_NULLPTR_ARG;

    _sf_build_record_path(workspace, TPB_RAF_DOM_TASK,
                      attr->task_record_id, fpath,
                      sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Fixed: 5*20 + 3*8 + 8*4 = 156 bytes */
    metasize = 156 + TPB_RAF_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAF_HDR_FIXED_SIZE;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD,
                          TPB_RAF_DOM_TASK,
                          TPB_RAF_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }

    if (_sf_write_u64(fp, metasize) != 0) goto fail;
    if (_sf_write_u64(fp, datasize) != 0) goto fail;

    if (fwrite(attr->task_record_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->derive_to, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->inherit_from, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (_sf_write_u64(fp, attr->utc_bits) != 0) goto fail;
    if (_sf_write_u64(fp, attr->btime) != 0) goto fail;
    if (_sf_write_u64(fp, attr->duration) != 0) goto fail;
    if (_sf_write_u32(fp, attr->exit_code) != 0) goto fail;
    if (_sf_write_u32(fp, attr->handle_index) != 0) goto fail;
    if (_sf_write_u32(fp, attr->pid) != 0) goto fail;
    if (_sf_write_u32(fp, attr->tid) != 0) goto fail;
    if (_sf_write_u32(fp, attr->ninput) != 0) goto fail;
    if (_sf_write_u32(fp, attr->noutput) != 0) goto fail;
    if (_sf_write_u32(fp, attr->nheader) != 0) goto fail;
    if (_sf_write_u32(fp, attr->reserve) != 0) goto fail;

    memset(reserve, 0, TPB_RAF_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_sf_write_headers(fp, attr->headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    if (_sf_write_magic_and_data(fp, TPB_RAF_DOM_TASK,
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
    uint64_t metasize, datasize;
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
        return TPBE_NULLPTR_ARG;
    }

    _sf_build_record_path(workspace, TPB_RAF_DOM_TASK,
                      capsule_id, fpath, sizeof(fpath));

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

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TASK,
                                  TPB_RAF_POS_START)) {
        goto fail_unlock;
    }

    if (_sf_read_u64(fp, &metasize) != 0) goto fail_unlock;
    if (_sf_read_u64(fp, &datasize) != 0) goto fail_unlock;

    if (fseek(fp, TPB_RAF_TASK_FILE_OFF_NHEADER, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_sf_read_u32(fp, &nheader) != 0) goto fail_unlock;
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

    /*
     * SPLIT offset: after the last serialized header byte.  Stored metasize
     * uses nheader * TPB_RAF_HDR_FIXED_SIZE (padded slots), but
     * _sf_write_headers only writes 32 + 256 + 2048 + 72 * ndim per header —
     * so 24 + metasize
     * would miss the real SPLIT position.  Capsule records use nheader == 1.
     */
    {
        uint32_t hdr0_ndim;

        if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_BASE + (long)sizeof(uint32_t),
                   SEEK_SET) != 0) {
            goto fail_unlock;
        }
        if (_sf_read_u32(fp, &hdr0_ndim) != 0) {
            goto fail_unlock;
        }
        split_off = (long)TPB_RAF_TASK_FILE_HDR0_BASE + 32L + 256L + 2048L
            + (long)hdr0_ndim * 8L + (long)hdr0_ndim * 64L;
    }
    (void)metasize;

    end_magic_off = split_off + 8 + (long)datasize;

    new_ds = datasize + 20U;
    new_dim0 = new_ds / 20U;
    new_hdr_ds = new_ds;

    if (fseek(fp, end_magic_off, SEEK_SET) != 0) goto fail_unlock;
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TASK,
                                  TPB_RAF_POS_END)) {
        goto fail_unlock;
    }

    if (fseek(fp, end_magic_off, SEEK_SET) != 0) goto fail_unlock;
    if (fwrite(task_id, 1, 20, fp) != 20) goto fail_unlock;

    tpb_raf_build_magic(TPB_RAF_FTYPE_RECORD, TPB_RAF_DOM_TASK,
                          TPB_RAF_POS_END, magic);
    if (fwrite(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        goto fail_unlock;
    }

    if (fseek(fp, TPB_RAF_TASK_FILE_OFF_DATASIZE, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_sf_write_u64(fp, new_ds) != 0) goto fail_unlock;

    if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_DATA_SIZE, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_sf_write_u64(fp, new_hdr_ds) != 0) goto fail_unlock;

    if (fseek(fp, TPB_RAF_TASK_FILE_HDR0_DIM0, SEEK_SET) != 0) {
        goto fail_unlock;
    }
    if (_sf_write_u64(fp, new_dim0) != 0) goto fail_unlock;

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
    uint64_t metasize, ds;

    if (!workspace || !task_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    _sf_build_record_path(workspace, TPB_RAF_DOM_TASK,
                      task_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp)
        != TPB_RAF_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_raf_validate_magic(magic, TPB_RAF_FTYPE_RECORD,
                                  TPB_RAF_DOM_TASK,
                                  TPB_RAF_POS_START)) {
        goto fail;
    }

    if (_sf_read_u64(fp, &metasize) != 0) goto fail;
    if (_sf_read_u64(fp, &ds) != 0) goto fail;

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->task_record_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->derive_to, 1, 20, fp) != 20) goto fail;
    if (fread(attr->inherit_from, 1, 20, fp) != 20) goto fail;
    if (fread(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (_sf_read_u64(fp, &attr->utc_bits) != 0) goto fail;
    if (_sf_read_u64(fp, &attr->btime) != 0) goto fail;
    if (_sf_read_u64(fp, &attr->duration) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->exit_code) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->handle_index) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->pid) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->tid) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->ninput) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->noutput) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->nheader) != 0) goto fail;
    if (_sf_read_u32(fp, &attr->reserve) != 0) goto fail;

    if (fread(reserve, 1, TPB_RAF_RESERVE_SIZE, fp)
        != TPB_RAF_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (_sf_read_headers(fp, &attr->headers,
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
            if (!*data) goto fail_hdrs;
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
