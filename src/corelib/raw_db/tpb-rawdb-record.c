/*
 * tpb-rawdb-record.c
 * Record file (.tpbr) write and read operations including header
 * serialization for tbatch, kernel, and task domains.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../tpb-types.h"
#include "tpb-rawdb-types.h"

/* Local Function Prototypes */
static void build_record_path(const char *workspace,
                              uint8_t domain,
                              const unsigned char id[20],
                              char *out, size_t outlen);
static int write_headers(FILE *fp, const tpb_meta_header_t *hdrs,
                         uint32_t n);
static int read_headers(FILE *fp, tpb_meta_header_t **hdrs_out,
                        uint32_t n);
static int write_magic_and_data(FILE *fp, uint8_t domain,
                                const void *data,
                                uint64_t datasize);
static int write_u64(FILE *fp, uint64_t v);
static int write_u32(FILE *fp, uint32_t v);
static int write_i32(FILE *fp, int32_t v);
static int read_u64(FILE *fp, uint64_t *v);
static int read_u32(FILE *fp, uint32_t *v);
static int read_i32(FILE *fp, int32_t *v);

static int
write_u64(FILE *fp, uint64_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

static int
write_u32(FILE *fp, uint32_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

static int
write_i32(FILE *fp, int32_t v)
{
    return (fwrite(&v, sizeof(v), 1, fp) == 1) ? 0 : -1;
}

static int
read_u64(FILE *fp, uint64_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

static int
read_u32(FILE *fp, uint32_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

static int
read_i32(FILE *fp, int32_t *v)
{
    return (fread(v, sizeof(*v), 1, fp) == 1) ? 0 : -1;
}

static void
build_record_path(const char *workspace, uint8_t domain,
                  const unsigned char id[20],
                  char *out, size_t outlen)
{
    char hex[41];
    const char *dir;

    tpb_rawdb_id_to_hex(id, hex);

    switch (domain) {
    case TPB_RAWDB_DOM_KERNEL:
        dir = TPB_RAWDB_KERNEL_DIR;
        break;
    case TPB_RAWDB_DOM_TASK:
        dir = TPB_RAWDB_TASK_DIR;
        break;
    default:
        dir = TPB_RAWDB_TBATCH_DIR;
        break;
    }
    snprintf(out, outlen, "%s/%s/%s.tpbr",
             workspace, dir, hex);
}

int
tpb_rawdb_find_record(const char *workspace,
                      const unsigned char id[20],
                      uint8_t *domain_out)
{
    char path[TPB_RAWDB_PATH_MAX];
    struct stat st;
    uint8_t doms[3];
    int d;

    if (!workspace || !id || !domain_out) {
        return TPBE_NULLPTR_ARG;
    }

    doms[0] = TPB_RAWDB_DOM_TBATCH;
    doms[1] = TPB_RAWDB_DOM_KERNEL;
    doms[2] = TPB_RAWDB_DOM_TASK;

    for (d = 0; d < 3; d++) {
        build_record_path(workspace, doms[d], id, path, sizeof(path));
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
write_headers(FILE *fp, const tpb_meta_header_t *hdrs,
              uint32_t n)
{
    uint32_t i, j;

    for (i = 0; i < n; i++) {
        uint32_t bs = TPB_RAWDB_HDR_FIXED_SIZE;

        if (write_u32(fp, bs) != 0) return -1;
        if (write_u32(fp, hdrs[i].ndim) != 0) return -1;
        if (write_u64(fp, hdrs[i].data_size) != 0) return -1;
        if (write_u32(fp, hdrs[i].type_bits) != 0) return -1;
        if (write_u32(fp, hdrs[i]._reserve) != 0) return -1;
        if (write_u64(fp, hdrs[i].uattr_bits) != 0) return -1;
        if (fwrite(hdrs[i].name, 1, 256, fp) != 256) return -1;
        if (fwrite(hdrs[i].note, 1, 2048, fp) != 2048) {
            return -1;
        }

        for (j = 0; j < hdrs[i].ndim; j++) {
            if (write_u64(fp, hdrs[i].dimsizes[j]) != 0) {
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
read_headers(FILE *fp, tpb_meta_header_t **hdrs_out,
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
        if (read_u32(fp, &hdrs[i].block_size) != 0) goto fail;
        if (read_u32(fp, &hdrs[i].ndim) != 0) goto fail;
        if (read_u64(fp, &hdrs[i].data_size) != 0) goto fail;
        if (read_u32(fp, &hdrs[i].type_bits) != 0) goto fail;
        if (read_u32(fp, &hdrs[i]._reserve) != 0) goto fail;
        if (read_u64(fp, &hdrs[i].uattr_bits) != 0) goto fail;
        if (fread(hdrs[i].name, 1, 256, fp) != 256) goto fail;
        if (fread(hdrs[i].note, 1, 2048, fp) != 2048) goto fail;

        for (j = 0; j < hdrs[i].ndim; j++) {
            if (read_u64(fp, &hdrs[i].dimsizes[j]) != 0) {
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
write_magic_and_data(FILE *fp, uint8_t domain,
                     const void *data, uint64_t datasize)
{
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];

    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_RECORD, domain,
                          TPB_RAWDB_POS_SPLIT, magic);
    if (fwrite(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        return -1;
    }

    if (datasize > 0 && data) {
        if (fwrite(data, 1, (size_t)datasize, fp)
            != (size_t)datasize) {
            return -1;
        }
    }

    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_RECORD, domain,
                          TPB_RAWDB_POS_END, magic);
    if (fwrite(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        return -1;
    }

    return 0;
}

void
tpb_rawdb_free_headers(tpb_meta_header_t *headers,
                       uint32_t nheader)
{
    (void)nheader;
    free(headers);
}

/* TBatch record write */
int
tpb_rawdb_record_write_tbatch(const char *workspace,
                              const tbatch_attr_t *attr,
                              const void *data,
                              uint64_t datasize)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint32_t i;

    if (!workspace || !attr) return TPBE_NULLPTR_ARG;

    build_record_path(workspace, TPB_RAWDB_DOM_TBATCH,
                      attr->tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Calculate metasize: fixed attrs + reserve + headers */
    /* Fixed: 20+20+8+8+8+64+64+4+4+4+4+4+4 = 216 bytes */
    metasize = 216 + TPB_RAWDB_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAWDB_HDR_FIXED_SIZE;

    /* meta_magic */
    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_RECORD,
                          TPB_RAWDB_DOM_TBATCH,
                          TPB_RAWDB_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail;
    }

    /* metasize, datasize */
    if (write_u64(fp, metasize) != 0) goto fail;
    if (write_u64(fp, datasize) != 0) goto fail;

    /* Attributes */
    if (fwrite(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->dup_to, 1, 20, fp) != 20) goto fail;
    if (write_u64(fp, attr->utc_bits) != 0) goto fail;
    if (write_u64(fp, attr->btime) != 0) goto fail;
    if (write_u64(fp, attr->duration) != 0) goto fail;
    if (fwrite(attr->hostname, 1, 64, fp) != 64) goto fail;
    if (fwrite(attr->username, 1, 64, fp) != 64) goto fail;
    if (write_u32(fp, attr->front_pid) != 0) goto fail;
    if (write_u32(fp, attr->nkernel) != 0) goto fail;
    if (write_u32(fp, attr->ntask) != 0) goto fail;
    if (write_u32(fp, attr->nscore) != 0) goto fail;
    if (write_u32(fp, attr->batch_type) != 0) goto fail;
    if (write_u32(fp, attr->nheader) != 0) goto fail;

    /* Reserve */
    memset(reserve, 0, TPB_RAWDB_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAWDB_RESERVE_SIZE, fp)
        != TPB_RAWDB_RESERVE_SIZE) {
        goto fail;
    }

    /* Headers */
    if (attr->nheader > 0) {
        if (write_headers(fp, attr->headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    /* Record data section */
    if (write_magic_and_data(fp, TPB_RAWDB_DOM_TBATCH,
                             data, datasize) != 0) {
        goto fail;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/* TBatch record read */
int
tpb_rawdb_record_read_tbatch(const char *workspace,
                             const unsigned char tbatch_id[20],
                             tbatch_attr_t *attr,
                             void **data,
                             uint64_t *datasize)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize, ds;

    if (!workspace || !tbatch_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    build_record_path(workspace, TPB_RAWDB_DOM_TBATCH,
                      tbatch_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* meta_magic */
    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_TBATCH,
                                  TPB_RAWDB_POS_START)) {
        goto fail;
    }

    if (read_u64(fp, &metasize) != 0) goto fail;
    if (read_u64(fp, &ds) != 0) goto fail;

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->dup_to, 1, 20, fp) != 20) goto fail;
    if (read_u64(fp, &attr->utc_bits) != 0) goto fail;
    if (read_u64(fp, &attr->btime) != 0) goto fail;
    if (read_u64(fp, &attr->duration) != 0) goto fail;
    if (fread(attr->hostname, 1, 64, fp) != 64) goto fail;
    if (fread(attr->username, 1, 64, fp) != 64) goto fail;
    if (read_u32(fp, &attr->front_pid) != 0) goto fail;
    if (read_u32(fp, &attr->nkernel) != 0) goto fail;
    if (read_u32(fp, &attr->ntask) != 0) goto fail;
    if (read_u32(fp, &attr->nscore) != 0) goto fail;
    if (read_u32(fp, &attr->batch_type) != 0) goto fail;
    if (read_u32(fp, &attr->nheader) != 0) goto fail;

    if (fread(reserve, 1, TPB_RAWDB_RESERVE_SIZE, fp)
        != TPB_RAWDB_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (read_headers(fp, &attr->headers,
                         attr->nheader) != 0) {
            goto fail;
        }
    } else {
        attr->headers = NULL;
    }

    /* record_magic */
    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_TBATCH,
                                  TPB_RAWDB_POS_SPLIT)) {
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
    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_TBATCH,
                                  TPB_RAWDB_POS_END)) {
        goto fail_hdrs;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail_hdrs:
    tpb_rawdb_free_headers(attr->headers, attr->nheader);
    attr->headers = NULL;
fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/* Kernel record write */
int
tpb_rawdb_record_write_kernel(const char *workspace,
                              const kernel_attr_t *attr,
                              const void *data,
                              uint64_t datasize)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint32_t i;

    if (!workspace || !attr) return TPBE_NULLPTR_ARG;

    build_record_path(workspace, TPB_RAWDB_DOM_KERNEL,
                      attr->kernel_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Fixed attrs: 5*20 + 256 + 64 + 2048 + 5*4 = 2488 */
    metasize = 2488 + TPB_RAWDB_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAWDB_HDR_FIXED_SIZE;

    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_RECORD,
                          TPB_RAWDB_DOM_KERNEL,
                          TPB_RAWDB_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail;
    }

    if (write_u64(fp, metasize) != 0) goto fail;
    if (write_u64(fp, datasize) != 0) goto fail;

    if (fwrite(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->dup_to, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->src_sha1, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->so_sha1, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->bin_sha1, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->kernel_name, 1, 256, fp) != 256) goto fail;
    if (fwrite(attr->version, 1, 64, fp) != 64) goto fail;
    if (fwrite(attr->description, 1, 2048, fp) != 2048) {
        goto fail;
    }
    if (write_u32(fp, attr->nparm) != 0) goto fail;
    if (write_u32(fp, attr->nmetric) != 0) goto fail;
    if (write_u32(fp, attr->kctrl) != 0) goto fail;
    if (write_u32(fp, attr->nheader) != 0) goto fail;
    if (write_u32(fp, attr->reserve) != 0) goto fail;

    memset(reserve, 0, TPB_RAWDB_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAWDB_RESERVE_SIZE, fp)
        != TPB_RAWDB_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (write_headers(fp, attr->headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    if (write_magic_and_data(fp, TPB_RAWDB_DOM_KERNEL,
                             data, datasize) != 0) {
        goto fail;
    }

    fclose(fp);
    return TPBE_SUCCESS;
fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/* Kernel record read */
int
tpb_rawdb_record_read_kernel(const char *workspace,
                             const unsigned char kernel_id[20],
                             kernel_attr_t *attr,
                             void **data,
                             uint64_t *datasize)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize, ds;

    if (!workspace || !kernel_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    build_record_path(workspace, TPB_RAWDB_DOM_KERNEL,
                      kernel_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_KERNEL,
                                  TPB_RAWDB_POS_START)) {
        goto fail;
    }

    if (read_u64(fp, &metasize) != 0) goto fail;
    if (read_u64(fp, &ds) != 0) goto fail;

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->dup_to, 1, 20, fp) != 20) goto fail;
    if (fread(attr->src_sha1, 1, 20, fp) != 20) goto fail;
    if (fread(attr->so_sha1, 1, 20, fp) != 20) goto fail;
    if (fread(attr->bin_sha1, 1, 20, fp) != 20) goto fail;
    if (fread(attr->kernel_name, 1, 256, fp) != 256) goto fail;
    if (fread(attr->version, 1, 64, fp) != 64) goto fail;
    if (fread(attr->description, 1, 2048, fp) != 2048) goto fail;
    if (read_u32(fp, &attr->nparm) != 0) goto fail;
    if (read_u32(fp, &attr->nmetric) != 0) goto fail;
    if (read_u32(fp, &attr->kctrl) != 0) goto fail;
    if (read_u32(fp, &attr->nheader) != 0) goto fail;
    if (read_u32(fp, &attr->reserve) != 0) goto fail;

    if (fread(reserve, 1, TPB_RAWDB_RESERVE_SIZE, fp)
        != TPB_RAWDB_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (read_headers(fp, &attr->headers,
                         attr->nheader) != 0) {
            goto fail;
        }
    } else {
        attr->headers = NULL;
    }

    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_KERNEL,
                                  TPB_RAWDB_POS_SPLIT)) {
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

    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_KERNEL,
                                  TPB_RAWDB_POS_END)) {
        goto fail_hdrs;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail_hdrs:
    tpb_rawdb_free_headers(attr->headers, attr->nheader);
    attr->headers = NULL;
fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/* Task record write */
int
tpb_rawdb_record_write_task(const char *workspace,
                            const task_attr_t *attr,
                            const void *data,
                            uint64_t datasize)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize;
    uint32_t i;

    if (!workspace || !attr) return TPBE_NULLPTR_ARG;

    build_record_path(workspace, TPB_RAWDB_DOM_TASK,
                      attr->task_record_id, fpath,
                      sizeof(fpath));

    fp = fopen(fpath, "wb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    /* Fixed: 4*20 + 3*8 + 7*4 = 132 bytes */
    metasize = 132 + TPB_RAWDB_RESERVE_SIZE;
    metasize += attr->nheader * TPB_RAWDB_HDR_FIXED_SIZE;

    tpb_rawdb_build_magic(TPB_RAWDB_FTYPE_RECORD,
                          TPB_RAWDB_DOM_TASK,
                          TPB_RAWDB_POS_START, magic);
    if (fwrite(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail;
    }

    if (write_u64(fp, metasize) != 0) goto fail;
    if (write_u64(fp, datasize) != 0) goto fail;

    if (fwrite(attr->task_record_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->dup_to, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fwrite(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (write_u64(fp, attr->utc_bits) != 0) goto fail;
    if (write_u64(fp, attr->btime) != 0) goto fail;
    if (write_u64(fp, attr->duration) != 0) goto fail;
    if (write_u32(fp, attr->exit_code) != 0) goto fail;
    if (write_u32(fp, attr->handle_index) != 0) goto fail;
    if (write_i32(fp, attr->mpi_rank) != 0) goto fail;
    if (write_u32(fp, attr->ninput) != 0) goto fail;
    if (write_u32(fp, attr->noutput) != 0) goto fail;
    if (write_u32(fp, attr->nheader) != 0) goto fail;
    if (write_u32(fp, attr->reserve) != 0) goto fail;

    memset(reserve, 0, TPB_RAWDB_RESERVE_SIZE);
    if (fwrite(reserve, 1, TPB_RAWDB_RESERVE_SIZE, fp)
        != TPB_RAWDB_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (write_headers(fp, attr->headers, attr->nheader) != 0) {
            goto fail;
        }
    }

    if (write_magic_and_data(fp, TPB_RAWDB_DOM_TASK,
                             data, datasize) != 0) {
        goto fail;
    }

    fclose(fp);
    return TPBE_SUCCESS;
fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}

/* Task record read */
int
tpb_rawdb_record_read_task(const char *workspace,
                           const unsigned char task_id[20],
                           task_attr_t *attr,
                           void **data,
                           uint64_t *datasize)
{
    char fpath[TPB_RAWDB_PATH_MAX];
    unsigned char magic[TPB_RAWDB_MAGIC_LEN];
    unsigned char reserve[TPB_RAWDB_RESERVE_SIZE];
    FILE *fp;
    uint64_t metasize, ds;

    if (!workspace || !task_id || !attr) {
        return TPBE_NULLPTR_ARG;
    }

    build_record_path(workspace, TPB_RAWDB_DOM_TASK,
                      task_id, fpath, sizeof(fpath));

    fp = fopen(fpath, "rb");
    if (!fp) return TPBE_FILE_IO_FAIL;

    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_TASK,
                                  TPB_RAWDB_POS_START)) {
        goto fail;
    }

    if (read_u64(fp, &metasize) != 0) goto fail;
    if (read_u64(fp, &ds) != 0) goto fail;

    memset(attr, 0, sizeof(*attr));
    if (fread(attr->task_record_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->dup_to, 1, 20, fp) != 20) goto fail;
    if (fread(attr->tbatch_id, 1, 20, fp) != 20) goto fail;
    if (fread(attr->kernel_id, 1, 20, fp) != 20) goto fail;
    if (read_u64(fp, &attr->utc_bits) != 0) goto fail;
    if (read_u64(fp, &attr->btime) != 0) goto fail;
    if (read_u64(fp, &attr->duration) != 0) goto fail;
    if (read_u32(fp, &attr->exit_code) != 0) goto fail;
    if (read_u32(fp, &attr->handle_index) != 0) goto fail;
    if (read_i32(fp, &attr->mpi_rank) != 0) goto fail;
    if (read_u32(fp, &attr->ninput) != 0) goto fail;
    if (read_u32(fp, &attr->noutput) != 0) goto fail;
    if (read_u32(fp, &attr->nheader) != 0) goto fail;
    if (read_u32(fp, &attr->reserve) != 0) goto fail;

    if (fread(reserve, 1, TPB_RAWDB_RESERVE_SIZE, fp)
        != TPB_RAWDB_RESERVE_SIZE) {
        goto fail;
    }

    if (attr->nheader > 0) {
        if (read_headers(fp, &attr->headers,
                         attr->nheader) != 0) {
            goto fail;
        }
    } else {
        attr->headers = NULL;
    }

    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_TASK,
                                  TPB_RAWDB_POS_SPLIT)) {
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

    if (fread(magic, 1, TPB_RAWDB_MAGIC_LEN, fp)
        != TPB_RAWDB_MAGIC_LEN) {
        goto fail_hdrs;
    }
    if (!tpb_rawdb_validate_magic(magic, TPB_RAWDB_FTYPE_RECORD,
                                  TPB_RAWDB_DOM_TASK,
                                  TPB_RAWDB_POS_END)) {
        goto fail_hdrs;
    }

    fclose(fp);
    return TPBE_SUCCESS;

fail_hdrs:
    tpb_rawdb_free_headers(attr->headers, attr->nheader);
    attr->headers = NULL;
fail:
    fclose(fp);
    return TPBE_FILE_IO_FAIL;
}
