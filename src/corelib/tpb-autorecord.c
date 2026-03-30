/*
 * tpb-autorecord.c
 * Auto-record: tbatch records (parent) and task records (kernel child).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <inttypes.h>
#include <time.h>
#ifdef __linux__
#include <linux/limits.h>
#include <sys/syscall.h>
#else
#include <limits.h>
#endif

#include "tpb-autorecord.h"
#include "tpb-driver.h"
#include "strftime.h"
#include "rafdb/tpb-raf-types.h"

/* ===== Batch-side module state (tpbcli parent process) ===== */

static int s_batch_active = 0;
static unsigned char s_tbatch_id[20];
static char s_tbatch_id_hex[41];
static char s_workspace[PATH_MAX];
static tpb_dtbits_t s_batch_utc_bits;
static uint64_t s_batch_btime_ns;
static char s_batch_hostname[64];
static char s_batch_username[64];
static uint32_t s_batch_pid;
static uint32_t s_batch_type;

/* ===== Internal helpers ===== */

static void
get_host_and_user(char *hostname, size_t hlen, char *username, size_t ulen)
{
    if (gethostname(hostname, hlen) != 0) {
        snprintf(hostname, hlen, "unknown");
    }
    hostname[hlen - 1] = '\0';

    struct passwd *pw = getpwuid(geteuid());
    if (pw && pw->pw_name) {
        snprintf(username, ulen, "%s", pw->pw_name);
    } else {
        snprintf(username, ulen, "unknown");
    }
}

static int
get_current_timestamps(tpb_dtbits_t *utc_bits, uint64_t *btime_ns)
{
    tpb_datetime_t dt;
    tpb_btime_t bt;
    int err;

    err = tpb_ts_get_datetime(TPBM_TS_UTC, &dt);
    if (err) return err;

    err = tpb_ts_datetime_to_bits(&dt, 0, utc_bits);
    if (err) return err;

    err = tpb_ts_get_btime(&bt);
    if (err) return err;

    *btime_ns = bt.sec * 1000000000ULL + bt.nsec;
    return 0;
}

static uint32_t
get_current_tid(void)
{
#ifdef __linux__
    return (uint32_t)syscall(SYS_gettid);
#else
    return (uint32_t)getpid();
#endif
}

static int
hex_to_id(const char *hex, unsigned char id[20])
{
    if (strlen(hex) != 40) return -1;
    for (int i = 0; i < 20; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%02x", &byte) != 1) return -1;
        id[i] = (unsigned char)byte;
    }
    return 0;
}

/* ===== Batch-side API ===== */

int
tpb_record_begin_batch(uint32_t batch_type)
{
    int err;

    err = tpb_raf_resolve_workspace(s_workspace, sizeof(s_workspace));
    if (err) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Auto-record: failed to resolve workspace (%d)\n", err);
        return err;
    }

    err = get_current_timestamps(&s_batch_utc_bits, &s_batch_btime_ns);
    if (err) return err;

    get_host_and_user(s_batch_hostname, sizeof(s_batch_hostname),
                      s_batch_username, sizeof(s_batch_username));
    s_batch_pid = (uint32_t)getpid();
    s_batch_type = batch_type;

    err = tpb_raf_gen_tbatch_id(s_batch_utc_bits, s_batch_btime_ns,
                                   s_batch_hostname, s_batch_username,
                                   s_batch_pid, s_tbatch_id);
    if (err) return err;

    tpb_raf_id_to_hex(s_tbatch_id, s_tbatch_id_hex);
    s_batch_active = 1;

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Auto-record: batch started, TBatchID=%s\n", s_tbatch_id_hex);
    return 0;
}

const char *
tpb_record_get_tbatch_id_hex(void)
{
    return s_batch_active ? s_tbatch_id_hex : NULL;
}

const char *
tpb_record_get_workspace(void)
{
    return s_batch_active ? s_workspace : NULL;
}

int
tpb_record_end_batch(int ntask)
{
    int err;
    tpb_dtbits_t end_utc;
    uint64_t end_btime_ns;

    if (!s_batch_active) return 0;

    err = get_current_timestamps(&end_utc, &end_btime_ns);
    if (err) return err;

    uint64_t duration = 0;
    if (end_btime_ns > s_batch_btime_ns)
        duration = end_btime_ns - s_batch_btime_ns;

    /* Read task entries to collect TaskRecordIDs belonging to this batch */
    task_entry_t *task_entries = NULL;
    int task_count = 0;
    err = tpb_raf_entry_list_task(s_workspace, &task_entries, &task_count);

    int matched = 0;
    int nkernel = 0;
    unsigned char *task_id_list = NULL;

    if (err == 0 && task_count > 0 && task_entries != NULL) {
        task_id_list = (unsigned char *)malloc(20 * task_count);
        if (task_id_list) {
            for (int i = 0; i < task_count; i++) {
                if (memcmp(task_entries[i].tbatch_id, s_tbatch_id, 20) == 0) {
                    int seen = 0;

                    memcpy(task_id_list + matched * 20,
                           task_entries[i].task_record_id, 20);
                    for (int j = 0; j < i; j++) {
                        if (memcmp(task_entries[j].tbatch_id, s_tbatch_id,
                                   20) == 0 &&
                            memcmp(task_entries[j].kernel_id,
                                   task_entries[i].kernel_id, 20) == 0) {
                            seen = 1;
                            break;
                        }
                    }
                    if (!seen) {
                        nkernel++;
                    }
                    matched++;
                }
            }
        }
        free(task_entries);
    }

    /* Build tbatch attributes */
    tbatch_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memcpy(attr.tbatch_id, s_tbatch_id, 20);
    /* dup_to and dup_from stay zero */
    attr.utc_bits = s_batch_utc_bits;
    attr.btime = s_batch_btime_ns;
    attr.duration = duration;
    snprintf(attr.hostname, sizeof(attr.hostname), "%s", s_batch_hostname);
    snprintf(attr.username, sizeof(attr.username), "%s", s_batch_username);
    attr.front_pid = s_batch_pid;
    attr.nkernel = (uint32_t)nkernel;
    attr.ntask = (uint32_t)matched;
    attr.nscore = 0;
    attr.batch_type = s_batch_type;

    /* Build 3 fixed headers */
    attr.nheader = 3;
    tpb_meta_header_t headers[3];
    memset(headers, 0, sizeof(headers));

    /* header[0]: KernelRecordIDs (empty) */
    headers[0].block_size = TPB_RAF_HDR_FIXED_SIZE;
    headers[0].ndim = 1;
    headers[0].dimsizes[0] = 0;
    headers[0].data_size = 0;
    headers[0].type_bits = (uint32_t)(TPB_UNSIGNED_CHAR_T & TPB_PARM_TYPE_MASK);
    snprintf(headers[0].name, sizeof(headers[0].name), "KernelRecordIDs");

    /* header[1]: TaskRecordIDs */
    headers[1].block_size = TPB_RAF_HDR_FIXED_SIZE;
    headers[1].ndim = 1;
    headers[1].dimsizes[0] = (uint64_t)matched;
    headers[1].data_size = (uint64_t)(matched * 20);
    headers[1].type_bits = (uint32_t)(TPB_UNSIGNED_CHAR_T & TPB_PARM_TYPE_MASK);
    snprintf(headers[1].name, sizeof(headers[1].name), "TaskRecordIDs");

    /* header[2]: ScoreRecordIDs (empty) */
    headers[2].block_size = TPB_RAF_HDR_FIXED_SIZE;
    headers[2].ndim = 1;
    headers[2].dimsizes[0] = 0;
    headers[2].data_size = 0;
    headers[2].type_bits = (uint32_t)(TPB_UNSIGNED_CHAR_T & TPB_PARM_TYPE_MASK);
    snprintf(headers[2].name, sizeof(headers[2].name), "ScoreRecordIDs");

    attr.headers = headers;

    uint64_t datasize = (uint64_t)(matched * 20);
    err = tpb_raf_record_write_tbatch(s_workspace, &attr,
                                         task_id_list, datasize);
    if (err) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Auto-record: failed to write tbatch record (%d)\n", err);
        free(task_id_list);
        s_batch_active = 0;
        return err;
    }

    /* Write tbatch entry */
    tbatch_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.tbatch_id, s_tbatch_id, 20);
    entry.start_utc_bits = s_batch_utc_bits;
    entry.duration = duration;
    snprintf(entry.hostname, sizeof(entry.hostname), "%s", s_batch_hostname);
    entry.nkernel = (uint32_t)nkernel;
    entry.ntask = (uint32_t)matched;
    entry.nscore = 0;
    entry.batch_type = s_batch_type;

    err = tpb_raf_entry_append_tbatch(s_workspace, &entry);
    if (err) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Auto-record: failed to append tbatch entry (%d)\n", err);
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Auto-record: batch ended, %d tasks recorded.\n", matched);

    free(task_id_list);
    s_batch_active = 0;
    return err;
}

/* ===== Task-side API (kernel child process) ===== */

int
tpb_record_write_task(tpb_k_rthdl_t *hdl, int exit_code)
{
    int err;
    char workspace[PATH_MAX];
    unsigned char tbatch_id[20];
    unsigned char kernel_id[20];
    unsigned char task_id[20];
    tpb_dtbits_t utc_bits;
    uint64_t btime_ns;
    char hostname[64], username[64];
    uint32_t handle_index = 0;
    uint32_t pid;
    uint32_t tid;

    if (!hdl) return TPBE_NULLPTR_ARG;

    /* Read TBatchID from env (or zero) */
    const char *env_bid = getenv("TPB_TBATCH_ID");
    memset(tbatch_id, 0, 20);
    if (env_bid && strlen(env_bid) == 40) {
        hex_to_id(env_bid, tbatch_id);
    }

    /* Read handle index from env */
    const char *env_idx = getenv("TPB_HANDLE_INDEX");
    if (env_idx) {
        handle_index = (uint32_t)atoi(env_idx);
    }

    /* Read KernelID from env (or zero) */
    const char *env_kid = getenv("TPB_KERNEL_ID");
    memset(kernel_id, 0, 20);
    /*
     * Kernel process receives TPB_KERNEL_ID from parent runner.
     * Keep compatibility fallback to zero ID when env is invalid.
     */
    if (env_kid != NULL && strlen(env_kid) == 40) {
        if (hex_to_id(env_kid, kernel_id) != 0) {
            memset(kernel_id, 0, 20);
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "Auto-record: invalid TPB_KERNEL_ID, fallback to zero.\n");
        }
    } else if (env_kid != NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Auto-record: TPB_KERNEL_ID must be 40 hex chars.\n");
    }

    /* Resolve workspace */
    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err) return err;

    err = get_current_timestamps(&utc_bits, &btime_ns);
    if (err) return err;

    get_host_and_user(hostname, sizeof(hostname), username, sizeof(username));
    pid = (uint32_t)getpid();
    tid = get_current_tid();

    err = tpb_raf_gen_task_id(utc_bits, btime_ns, hostname, username,
                                tbatch_id, kernel_id, handle_index,
                                pid, tid, task_id);
    if (err) return err;

    /* Build task_attr_t */
    uint32_t ninput = (uint32_t)hdl->argpack.n;
    uint32_t noutput = (uint32_t)hdl->respack.n;
    uint32_t nheader = ninput + noutput;

    task_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    memcpy(attr.task_record_id, task_id, 20);
    memcpy(attr.tbatch_id, tbatch_id, 20);
    memcpy(attr.kernel_id, kernel_id, 20);
    attr.utc_bits = utc_bits;
    attr.btime = btime_ns;
    attr.duration = 0;
    attr.exit_code = (uint32_t)exit_code;
    attr.handle_index = handle_index;
    attr.pid = pid;
    attr.tid = tid;
    attr.ninput = ninput;
    attr.noutput = noutput;
    attr.nheader = nheader;

    /* Build input + output headers and record data */
    tpb_meta_header_t *headers = NULL;
    void *rec_data = NULL;
    uint64_t rec_datasize = 0;

    if (nheader > 0) {
        headers = (tpb_meta_header_t *)calloc(nheader, sizeof(tpb_meta_header_t));
        if (!headers) return TPBE_MALLOC_FAIL;

        /* --- Input headers --- */
        for (uint32_t i = 0; i < ninput; i++) {
            tpb_rt_parm_t *parm = &hdl->argpack.args[i];
            uint32_t type_code = (uint32_t)(parm->ctrlbits & TPB_PARM_TYPE_MASK);
            uint32_t elem_size = (type_code >> 8) & 0xFF;
            if (elem_size == 0) elem_size = 8;

            headers[i].block_size = TPB_RAF_HDR_FIXED_SIZE;
            headers[i].ndim = 1;
            headers[i].dimsizes[0] = 1;
            headers[i].data_size = elem_size;
            headers[i].type_bits = (uint32_t)(parm->ctrlbits & (TPB_PARM_SOURCE_MASK | TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK));
            snprintf(headers[i].name, sizeof(headers[i].name), "%s", parm->name);
            snprintf(headers[i].note, sizeof(headers[i].note), "%s", parm->note);

            rec_datasize += elem_size;
        }

        /* --- Output headers (1-D arrays) --- */
        for (uint32_t j = 0; j < noutput; j++) {
            uint32_t i = ninput + j;
            tpb_k_output_t *out = &hdl->respack.outputs[j];
            size_t esz = 0;

            if (tpb_dtype_elem_size(out->dtype, &esz) != 0) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                           "Auto-record: unsupported dtype 0x%08llx for output '%s', skipping data.\n",
                           (unsigned long long)out->dtype, out->name);
                esz = 0;
            }

            headers[i].block_size  = TPB_RAF_HDR_FIXED_SIZE;
            headers[i].ndim        = 1;
            headers[i].dimsizes[0] = (uint64_t)out->n;
            headers[i].data_size   = (uint64_t)(out->n * esz);
            headers[i].type_bits   = (uint32_t)(out->dtype & TPB_PARM_TYPE_MASK);
            headers[i].uattr_bits  = (uint64_t)out->unit;
            snprintf(headers[i].name, sizeof(headers[i].name), "%s", out->name);
            snprintf(headers[i].note, sizeof(headers[i].note), "%s", out->note);

            rec_datasize += (uint64_t)(out->n * esz);
        }

        /* Serialize data: inputs then outputs */
        rec_data = calloc(1, (size_t)(rec_datasize > 0 ? rec_datasize : 1));
        if (!rec_data) { free(headers); return TPBE_MALLOC_FAIL; }

        uint8_t *ptr = (uint8_t *)rec_data;

        /* Input values */
        for (uint32_t i = 0; i < ninput; i++) {
            tpb_rt_parm_t *parm = &hdl->argpack.args[i];
            uint32_t elem_size = (uint32_t)headers[i].data_size;
            memcpy(ptr, &parm->value, elem_size);
            ptr += elem_size;
        }

        /* Output arrays */
        for (uint32_t j = 0; j < noutput; j++) {
            tpb_k_output_t *out = &hdl->respack.outputs[j];
            uint64_t blksz = headers[ninput + j].data_size;
            if (blksz > 0 && out->p != NULL) {
                memcpy(ptr, out->p, (size_t)blksz);
            }
            ptr += blksz;
        }
    }

    attr.headers = headers;

    /* Write task record (.tpbr) */
    err = tpb_raf_record_write_task(workspace, &attr, rec_data, rec_datasize);
    if (err) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Auto-record: failed to write task record (%d)\n", err);
        free(headers);
        free(rec_data);
        return err;
    }

    /* Write task entry (.tpbe) */
    task_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.task_record_id, task_id, 20);
    memcpy(entry.tbatch_id, tbatch_id, 20);
    memcpy(entry.kernel_id, kernel_id, 20);
    entry.utc_bits = utc_bits;
    entry.duration = 0;
    entry.exit_code = (uint32_t)exit_code;
    entry.handle_index = handle_index;

    err = tpb_raf_entry_append_task(workspace, &entry);
    if (err) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Auto-record: failed to append task entry (%d)\n", err);
    } else {
        char task_hex[41];
        tpb_raf_id_to_hex(task_id, task_hex);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "Auto-record: task recorded, TaskID=%s\n", task_hex);
    }

    free(headers);
    free(rec_data);
    return err;
}

/* ===== Public wrapper for kernel callers ===== */

int
tpb_k_write_task(tpb_k_rthdl_t *hdl, int exit_code)
{
    return tpb_record_write_task(hdl, exit_code);
}
