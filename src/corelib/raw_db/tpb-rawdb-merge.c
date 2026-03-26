/*
 * tpb-rawdb-merge.c
 * Merge multiple task records into a single combined record.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "tpb-rawdb-merge.h"
#include "tpb-rawdb-types.h"
#include "../strftime.h"

#define MERGE_MAX_UNIQUE_NAMES 256

/* Local Function Prototypes */
static uint32_t get_cur_tid(void);
static int cmp_task_by_tid(const void *a, const void *b);
static int validate_batch_kernel(task_attr_t *tasks, int n);
static void calc_merged_duration(task_attr_t *tasks, int n,
                                 uint64_t *btime_out,
                                 uint64_t *duration_out,
                                 int *earliest_idx);
static int collect_unique_header_names(task_attr_t *tasks,
                                       int n,
                                       char names[][256],
                                       int *nnames_out);
static int build_merge_headers(task_attr_t *tasks,
                               void **src_data,
                               uint64_t *src_datasize,
                               int n,
                               int is_process_merge,
                               tpb_meta_header_t **hdrs_out,
                               uint32_t *nhdrs_out,
                               void **merged_data_out,
                               uint64_t *merged_datasize_out);
static int update_source_dup_to(const char *workspace,
                                const unsigned char task_ids[][20],
                                int n,
                                const unsigned char merged_id[20]);

static uint32_t
get_cur_tid(void)
{
#ifdef __linux__
    return (uint32_t)syscall(SYS_gettid);
#else
    return (uint32_t)getpid();
#endif
}

static int
cmp_task_by_tid(const void *a, const void *b)
{
    const task_attr_t *ta = (const task_attr_t *)a;
    const task_attr_t *tb = (const task_attr_t *)b;
    if (ta->tid < tb->tid) return -1;
    if (ta->tid > tb->tid) return 1;
    return 0;
}

static int
validate_batch_kernel(task_attr_t *tasks, int n)
{
    int i;

    for (i = 1; i < n; i++) {
        if (memcmp(tasks[i].tbatch_id,
                   tasks[0].tbatch_id, 20) != 0 ||
            memcmp(tasks[i].kernel_id,
                   tasks[0].kernel_id, 20) != 0) {
            char hex0[41], hexi[41];
            tpb_rawdb_id_to_hex(tasks[0].tbatch_id, hex0);
            tpb_rawdb_id_to_hex(tasks[i].tbatch_id, hexi);
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "merge: tbatch/kernel ID mismatch "
                       "task[0]=%s vs task[%d]=%s\n",
                       hex0, i, hexi);
            return TPBE_MERGE_MISMATCH;
        }
    }
    return TPBE_SUCCESS;
}

static void
calc_merged_duration(task_attr_t *tasks, int n,
                     uint64_t *btime_out,
                     uint64_t *duration_out,
                     int *earliest_idx)
{
    int i, ei = 0;
    uint64_t min_bt, max_end, end_i;

    min_bt = tasks[0].btime;
    max_end = tasks[0].btime + tasks[0].duration;

    for (i = 1; i < n; i++) {
        if (tasks[i].btime < min_bt) {
            min_bt = tasks[i].btime;
            ei = i;
        }
        end_i = tasks[i].btime + tasks[i].duration;
        if (end_i > max_end)
            max_end = end_i;
    }
    *btime_out = min_bt;
    *duration_out = max_end - min_bt;
    *earliest_idx = ei;
}

static int
collect_unique_header_names(task_attr_t *tasks, int n,
                            char names[][256],
                            int *nnames_out)
{
    int i, j, k, count = 0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < (int)tasks[i].nheader; j++) {
            int found = 0;
            for (k = 0; k < count; k++) {
                if (strcmp(names[k],
                           tasks[i].headers[j].name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found && count < MERGE_MAX_UNIQUE_NAMES) {
                strncpy(names[count],
                        tasks[i].headers[j].name, 255);
                names[count][255] = '\0';
                count++;
            }
        }
    }
    *nnames_out = count;
    return TPBE_SUCCESS;
}

/*
 * Fill one special header in-place with common defaults.
 */
static void
fill_special_hdr(tpb_meta_header_t *h, const char *name,
                 int n, uint32_t type_bits, uint64_t elem_sz)
{
    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAWDB_HDR_FIXED_SIZE;
    h->ndim = 1;
    h->dimsizes[0] = (uint64_t)n;
    h->type_bits = type_bits;
    h->data_size = (uint64_t)n * elem_sz;
    strncpy(h->name, name, sizeof(h->name) - 1);
}

static int
build_merge_headers(task_attr_t *tasks,
                    void **src_data,
                    uint64_t *src_datasize,
                    int n,
                    int is_process_merge,
                    tpb_meta_header_t **hdrs_out,
                    uint32_t *nhdrs_out,
                    void **merged_data_out,
                    uint64_t *merged_datasize_out)
{
    int n_special, n_unames, hi, ui, ti, hj;
    uint32_t nhdrs;
    uint32_t uchar_bits, u32_bits;
    tpb_meta_header_t *hdrs;
    char unames[MERGE_MAX_UNIQUE_NAMES][256];
    uint64_t special_sz, total_sz;
    unsigned char *mdata, *ptr;
    char hostname[64];
    int i;

    uchar_bits = (uint32_t)(TPB_UNSIGNED_CHAR_T
                             & TPB_PARM_TYPE_MASK);
    u32_bits = (uint32_t)(TPB_UINT32_T
                           & TPB_PARM_TYPE_MASK);

    n_special = 2;
    if (is_process_merge) n_special += 2;

    n_unames = 0;
    memset(unames, 0, sizeof(unames));
    collect_unique_header_names(tasks, n,
                                unames, &n_unames);

    nhdrs = (uint32_t)(n_special + n_unames * n);
    hdrs = (tpb_meta_header_t *)calloc(nhdrs,
                                        sizeof(*hdrs));
    if (!hdrs) return TPBE_MALLOC_FAIL;

    hi = 0;

    /* SourceTaskIDs */
    fill_special_hdr(&hdrs[hi], "SourceTaskIDs",
                     n, uchar_bits, 20);
    hi++;

    /* ThreadIDs */
    fill_special_hdr(&hdrs[hi], "ThreadIDs",
                     n, u32_bits, 4);
    hi++;

    if (is_process_merge) {
        /* ProcessIDs */
        fill_special_hdr(&hdrs[hi], "ProcessIDs",
                         n, u32_bits, 4);
        hi++;

        /* Hosts */
        fill_special_hdr(&hdrs[hi], "Hosts",
                         n, uchar_bits, 64);
        hi++;
    }

    /* Interleaved source headers */
    for (ui = 0; ui < n_unames; ui++) {
        for (ti = 0; ti < n; ti++) {
            int found = 0;
            for (hj = 0;
                 hj < (int)tasks[ti].nheader; hj++) {
                if (strcmp(tasks[ti].headers[hj].name,
                           unames[ui]) == 0) {
                    memcpy(&hdrs[hi],
                           &tasks[ti].headers[hj],
                           sizeof(tpb_meta_header_t));
                    hdrs[hi].block_size =
                        TPB_RAWDB_HDR_FIXED_SIZE;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                memset(&hdrs[hi], 0, sizeof(hdrs[hi]));
                hdrs[hi].block_size =
                    TPB_RAWDB_HDR_FIXED_SIZE;
                hdrs[hi].ndim = 0;
                hdrs[hi].data_size = 0;
                strncpy(hdrs[hi].name, unames[ui],
                        sizeof(hdrs[hi].name) - 1);
            }
            hi++;
        }
    }

    /* Build merged data blob — data must be interleaved
       to match the interleaved header order. */
    special_sz = (uint64_t)n * 20
               + (uint64_t)n * 4;
    if (is_process_merge) {
        special_sz += (uint64_t)n * 4
                    + (uint64_t)n * 64;
    }

    total_sz = special_sz;
    for (ui = 0; ui < n_unames; ui++) {
        for (ti = 0; ti < n; ti++) {
            for (hj = 0;
                 hj < (int)tasks[ti].nheader; hj++) {
                if (strcmp(tasks[ti].headers[hj].name,
                           unames[ui]) == 0) {
                    total_sz +=
                        tasks[ti].headers[hj].data_size;
                    break;
                }
            }
        }
    }

    mdata = (unsigned char *)calloc(1, (size_t)total_sz);
    if (!mdata) {
        free(hdrs);
        return TPBE_MALLOC_FAIL;
    }

    ptr = mdata;

    /* SourceTaskIDs payload */
    for (i = 0; i < n; i++) {
        memcpy(ptr, tasks[i].task_record_id, 20);
        ptr += 20;
    }

    /* ThreadIDs payload */
    for (i = 0; i < n; i++) {
        uint32_t tid_val = tasks[i].tid;
        memcpy(ptr, &tid_val, 4);
        ptr += 4;
    }

    if (is_process_merge) {
        /* ProcessIDs payload */
        for (i = 0; i < n; i++) {
            uint32_t pid_val = tasks[i].pid;
            memcpy(ptr, &pid_val, 4);
            ptr += 4;
        }

        /* Hosts payload */
        memset(hostname, 0, sizeof(hostname));
        gethostname(hostname, sizeof(hostname));
        for (i = 0; i < n; i++) {
            memcpy(ptr, hostname, 64);
            ptr += 64;
        }
    }

    /* Interleaved source data: for each unique header
       name and each source, copy the matching header's
       data slice from the source blob. */
    for (ui = 0; ui < n_unames; ui++) {
        for (ti = 0; ti < n; ti++) {
            for (hj = 0;
                 hj < (int)tasks[ti].nheader; hj++) {
                if (strcmp(tasks[ti].headers[hj].name,
                           unames[ui]) == 0) {
                    uint64_t off = 0;
                    int k;
                    for (k = 0; k < hj; k++)
                        off +=
                            tasks[ti].headers[k].data_size;
                    if (tasks[ti].headers[hj].data_size > 0
                        && src_data[ti]) {
                        memcpy(ptr,
                            (const uint8_t *)src_data[ti]
                                + off,
                            (size_t)tasks[ti]
                                .headers[hj].data_size);
                    }
                    ptr +=
                        tasks[ti].headers[hj].data_size;
                    break;
                }
            }
        }
    }

    *hdrs_out = hdrs;
    *nhdrs_out = nhdrs;
    *merged_data_out = mdata;
    *merged_datasize_out = total_sz;
    return TPBE_SUCCESS;
}

static int
update_source_dup_to(const char *workspace,
                     const unsigned char task_ids[][20],
                     int n,
                     const unsigned char merged_id[20])
{
    int i, err;
    task_attr_t attr;
    void *data;
    uint64_t datasize;
    task_entry_t *entries;
    int count, row, j;
    char fpath[TPB_RAWDB_PATH_MAX];
    long offset;
    FILE *fp;

    for (i = 0; i < n; i++) {
        /* Read and rewrite .tpbr with updated dup_to */
        data = NULL;
        memset(&attr, 0, sizeof(attr));
        err = tpb_rawdb_record_read_task(workspace,
                                         task_ids[i],
                                         &attr, &data,
                                         &datasize);
        if (err) {
            free(data);
            tpb_rawdb_free_headers(attr.headers,
                                   attr.nheader);
            return err;
        }

        memcpy(attr.dup_to, merged_id, 20);

        err = tpb_rawdb_record_write_task(workspace,
                                          &attr,
                                          data, datasize);
        free(data);
        tpb_rawdb_free_headers(attr.headers, attr.nheader);
        if (err) return err;

        /* Update dup_to in .tpbe via direct seek */
        entries = NULL;
        count = 0;
        err = tpb_rawdb_entry_list_task(workspace,
                                        &entries, &count);
        if (err) return err;

        row = -1;
        for (j = 0; j < count; j++) {
            if (memcmp(entries[j].task_record_id,
                       task_ids[i], 20) == 0) {
                row = j;
                break;
            }
        }
        free(entries);

        if (row < 0) continue;

        snprintf(fpath, sizeof(fpath), "%s/%s/%s",
                 workspace, TPB_RAWDB_TASK_DIR,
                 TPB_RAWDB_TASK_ENTRY);

        /* dup_to offset in task_entry_t = 104 */
        offset = 8L + (long)row * 232L + 104L;

        fp = fopen(fpath, "r+b");
        if (!fp) return TPBE_FILE_IO_FAIL;

        if (fseek(fp, offset, SEEK_SET) != 0) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        if (fwrite(merged_id, 1, 20, fp) != 20) {
            fclose(fp);
            return TPBE_FILE_IO_FAIL;
        }
        fclose(fp);
    }
    return TPBE_SUCCESS;
}

int
tpb_rawdb_merge_par(const char *workspace,
                    const unsigned char task_ids[][20],
                    int n_tasks,
                    int is_process_merge,
                    unsigned char merged_id_out[20])
{
    int i, err, earliest_idx;
    task_attr_t *tasks = NULL;
    void **src_data = NULL;
    uint64_t *src_datasize = NULL;
    tpb_meta_header_t *mhdrs = NULL;
    uint32_t nmhdrs = 0;
    void *mdata = NULL;
    uint64_t mdatasize = 0;
    uint64_t m_btime, m_duration;
    task_attr_t merged_attr;
    task_entry_t merged_entry;
    char hostname[64];
    const char *username;
    struct passwd *pw;
    unsigned char merged_id[20];

    /* Temp arrays for sort fixup */
    unsigned char *saved_ids = NULL;
    void **saved_data = NULL;
    uint64_t *saved_sz = NULL;

    if (!workspace || !task_ids || !merged_id_out)
        return TPBE_NULLPTR_ARG;
    if (n_tasks < 2)
        return TPBE_MERGE_FAIL;

    tasks = (task_attr_t *)calloc((size_t)n_tasks,
                                  sizeof(task_attr_t));
    src_data = (void **)calloc((size_t)n_tasks,
                                sizeof(void *));
    src_datasize = (uint64_t *)calloc((size_t)n_tasks,
                                      sizeof(uint64_t));
    if (!tasks || !src_data || !src_datasize) {
        err = TPBE_MALLOC_FAIL;
        goto cleanup;
    }

    /* Read all source task records */
    for (i = 0; i < n_tasks; i++) {
        err = tpb_rawdb_record_read_task(workspace,
                                         task_ids[i],
                                         &tasks[i],
                                         &src_data[i],
                                         &src_datasize[i]);
        if (err) goto cleanup;
    }

    /* Save id-to-data mapping before sorting */
    saved_ids = (unsigned char *)malloc(
        (size_t)n_tasks * 20);
    saved_data = (void **)malloc(
        (size_t)n_tasks * sizeof(void *));
    saved_sz = (uint64_t *)malloc(
        (size_t)n_tasks * sizeof(uint64_t));
    if (!saved_ids || !saved_data || !saved_sz) {
        err = TPBE_MALLOC_FAIL;
        goto cleanup;
    }
    for (i = 0; i < n_tasks; i++) {
        memcpy(saved_ids + i * 20,
               tasks[i].task_record_id, 20);
        saved_data[i] = src_data[i];
        saved_sz[i] = src_datasize[i];
    }

    /* Sort tasks by tid */
    qsort(tasks, (size_t)n_tasks,
          sizeof(task_attr_t), cmp_task_by_tid);

    /* Rebuild src_data/src_datasize in sorted order */
    for (i = 0; i < n_tasks; i++) {
        int j;
        for (j = 0; j < n_tasks; j++) {
            if (memcmp(tasks[i].task_record_id,
                       saved_ids + j * 20, 20) == 0) {
                src_data[i] = saved_data[j];
                src_datasize[i] = saved_sz[j];
                break;
            }
        }
    }
    free(saved_ids);  saved_ids = NULL;
    free(saved_data); saved_data = NULL;
    free(saved_sz);   saved_sz = NULL;

    /* Validate same tbatch_id and kernel_id */
    err = validate_batch_kernel(tasks, n_tasks);
    if (err) goto cleanup;

    /* Calculate merged duration */
    calc_merged_duration(tasks, n_tasks,
                         &m_btime, &m_duration,
                         &earliest_idx);

    /* Build merged headers and data */
    err = build_merge_headers(tasks, src_data,
                              src_datasize, n_tasks,
                              is_process_merge,
                              &mhdrs, &nmhdrs,
                              &mdata, &mdatasize);
    if (err) goto cleanup;

    /* Generate merged task ID */
    memset(hostname, 0, sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    pw = getpwuid(geteuid());
    username = pw ? pw->pw_name : "unknown";

    err = tpb_rawdb_gen_task_id(
        tasks[earliest_idx].utc_bits,
        m_btime, hostname, username,
        tasks[0].tbatch_id,
        tasks[0].kernel_id,
        UINT32_MAX,
        (uint32_t)getpid(),
        get_cur_tid(),
        merged_id);
    if (err) goto cleanup;

    /* Build merged task_attr_t */
    memset(&merged_attr, 0, sizeof(merged_attr));
    memcpy(merged_attr.task_record_id, merged_id, 20);
    memset(merged_attr.dup_from, 0xFF, 20);
    memset(merged_attr.dup_to, 0, 20);
    memcpy(merged_attr.tbatch_id,
           tasks[0].tbatch_id, 20);
    memcpy(merged_attr.kernel_id,
           tasks[0].kernel_id, 20);
    merged_attr.utc_bits =
        tasks[earliest_idx].utc_bits;
    merged_attr.btime = m_btime;
    merged_attr.duration = m_duration;
    merged_attr.exit_code = 0;
    merged_attr.handle_index = UINT32_MAX;
    merged_attr.pid = (uint32_t)getpid();
    merged_attr.tid = get_cur_tid();
    merged_attr.ninput = 0;
    merged_attr.noutput = 0;
    merged_attr.nheader = nmhdrs;
    merged_attr.headers = mhdrs;

    /* Write merged .tpbr */
    err = tpb_rawdb_record_write_task(workspace,
                                      &merged_attr,
                                      mdata, mdatasize);
    if (err) goto cleanup;

    /* Write merged .tpbe entry */
    memset(&merged_entry, 0, sizeof(merged_entry));
    memcpy(merged_entry.task_record_id, merged_id, 20);
    memset(merged_entry.dup_from, 0xFF, 20);
    memset(merged_entry.dup_to, 0, 20);
    memcpy(merged_entry.tbatch_id,
           tasks[0].tbatch_id, 20);
    memcpy(merged_entry.kernel_id,
           tasks[0].kernel_id, 20);
    merged_entry.utc_bits =
        tasks[earliest_idx].utc_bits;
    merged_entry.duration = m_duration;
    merged_entry.exit_code = 0;
    merged_entry.handle_index = UINT32_MAX;

    err = tpb_rawdb_entry_append_task(workspace,
                                      &merged_entry);
    if (err) goto cleanup;

    /* Update source records' dup_to */
    err = update_source_dup_to(workspace, task_ids,
                               n_tasks, merged_id);
    if (err) goto cleanup;

    memcpy(merged_id_out, merged_id, 20);
    err = TPBE_SUCCESS;

cleanup:
    free(saved_ids);
    free(saved_data);
    free(saved_sz);
    free(mdata);
    free(mhdrs);
    if (src_data) {
        for (i = 0; i < n_tasks; i++)
            free(src_data[i]);
    }
    free(src_data);
    free(src_datasize);
    if (tasks) {
        for (i = 0; i < n_tasks; i++) {
            tpb_rawdb_free_headers(tasks[i].headers,
                                   tasks[i].nheader);
        }
    }
    free(tasks);
    return err;
}

/* Public wrappers */

int
tpb_k_merge_record_thread(const unsigned char task_ids[][20],
                           int n_tasks,
                           unsigned char merged_id_out[20])
{
    char workspace[PATH_MAX];
    int err = tpb_rawdb_resolve_workspace(workspace,
                                          sizeof(workspace));
    if (err) return err;
    return tpb_rawdb_merge_par(workspace, task_ids,
                               n_tasks, 0,
                               merged_id_out);
}

int
tpb_k_merge_record_process(const unsigned char task_ids[][20],
                            int n_tasks,
                            unsigned char merged_id_out[20])
{
    char workspace[PATH_MAX];
    int err = tpb_rawdb_resolve_workspace(workspace,
                                          sizeof(workspace));
    if (err) return err;
    return tpb_rawdb_merge_par(workspace, task_ids,
                               n_tasks, 1,
                               merged_id_out);
}
