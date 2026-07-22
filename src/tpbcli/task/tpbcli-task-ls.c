/*
 * tpbcli-task-ls.c
 * `tpbcli task ls|list` — entrypoint listing, filters, and RIDMAP update.
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-task-internal.h"

/* Local Function Prototypes */
static int _sf_cmp_rows(const void *a, const void *b);
static const char *_sf_lookup_kernel_name(const kernel_entry_t *kentries,
                                          int nk,
                                          const unsigned char kernel_id[20],
                                          char *fallback,
                                          size_t fallback_len);
static int _sf_hex_prefix_match(const unsigned char id[20], const char *prefix);
static int _sf_match_filters(const tpbcli_task_row_t *row,
                             const kernel_entry_t *kentries,
                             int nk,
                             const tpbcli_task_ls_opts_t *opts);
static int _sf_subproc_cell(const char *workspace, const task_entry_t *entry,
                            char *out, size_t outlen, int *warned);

static int
_sf_cmp_rows(const void *a, const void *b)
{
    const tpbcli_task_row_t *ra = (const tpbcli_task_row_t *)a;
    const tpbcli_task_row_t *rb = (const tpbcli_task_row_t *)b;

    if (ra->entry.utc_bits < rb->entry.utc_bits) {
        return -1;
    }
    if (ra->entry.utc_bits > rb->entry.utc_bits) {
        return 1;
    }
    if (ra->index_order < rb->index_order) {
        return -1;
    }
    if (ra->index_order > rb->index_order) {
        return 1;
    }
    return 0;
}

static const char *
_sf_lookup_kernel_name(const kernel_entry_t *kentries, int nk,
                       const unsigned char kernel_id[20], char *fallback,
                       size_t fallback_len)
{
    int i;

    if (kentries == NULL || nk <= 0) {
        tpbcli_task_format_id_prefix6(kernel_id, fallback, fallback_len);
        return fallback;
    }
    for (i = 0; i < nk; i++) {
        if (memcmp(kentries[i].kernel_id, kernel_id, 20) == 0) {
            if (kentries[i].kernel_name[0] != '\0') {
                return kentries[i].kernel_name;
            }
            break;
        }
    }
    tpbcli_task_format_id_prefix6(kernel_id, fallback, fallback_len);
    return fallback;
}

static int
_sf_hex_prefix_match(const unsigned char id[20], const char *prefix)
{
    char hex[41];
    size_t n;
    size_t i;

    if (prefix == NULL) {
        return 0;
    }
    n = strlen(prefix);
    if (n < 4 || n > 40) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!isxdigit((unsigned char)prefix[i])) {
            return 0;
        }
    }
    tpb_raf_id_to_hex(id, hex);
    for (i = 0; i < n; i++) {
        char a = (char)tolower((unsigned char)hex[i]);
        char b = (char)tolower((unsigned char)prefix[i]);

        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int
_sf_match_filters(const tpbcli_task_row_t *row,
                  const kernel_entry_t *kentries, int nk,
                  const tpbcli_task_ls_opts_t *opts)
{
    int i;
    char kfb[8];
    const char *kname;

    if (opts == NULL || row == NULL) {
        return 0;
    }
    for (i = 0; i < opts->nfilter; i++) {
        const tpbcli_task_filter_t *f = &opts->filters[i];

        switch (f->key) {
        case TPBCLI_TASK_FKEY_KERNEL_ID:
            if (!_sf_hex_prefix_match(row->entry.kernel_id, f->value)) {
                return 0;
            }
            break;
        case TPBCLI_TASK_FKEY_KERNEL_NAME:
            kname = _sf_lookup_kernel_name(kentries, nk, row->entry.kernel_id,
                                           kfb, sizeof(kfb));
            /* Prefix fallback (xxxxxx*) never equals a real kernel name. */
            if (strcmp(kname, f->value) != 0) {
                return 0;
            }
            break;
        case TPBCLI_TASK_FKEY_TBATCH_ID:
            if (!_sf_hex_prefix_match(row->entry.tbatch_id, f->value)) {
                return 0;
            }
            break;
        case TPBCLI_TASK_FKEY_EXIT_CODE:
            if (row->entry.exit_code != f->exit_code) {
                return 0;
            }
            break;
        case TPBCLI_TASK_FKEY_DATETIME: {
            tpb_dtbits_t bits = row->entry.utc_bits;
            int ok = 0;

            switch (f->op) {
            case TPBCLI_TASK_FOP_EQ:
                ok = (bits == f->datetime_utc);
                break;
            case TPBCLI_TASK_FOP_GT:
                ok = (bits > f->datetime_utc);
                break;
            case TPBCLI_TASK_FOP_GE:
                ok = (bits >= f->datetime_utc);
                break;
            case TPBCLI_TASK_FOP_LT:
                ok = (bits < f->datetime_utc);
                break;
            case TPBCLI_TASK_FOP_LE:
                ok = (bits <= f->datetime_utc);
                break;
            }
            if (!ok) {
                return 0;
            }
            break;
        }
        default:
            return 0;
        }
    }
    return 1;
}

static int
_sf_subproc_cell(const char *workspace, const task_entry_t *entry, char *out,
                 size_t outlen, int *warned)
{
    task_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    unsigned char (*members)[20] = NULL;
    int nmembers = 0;
    int err;
    static const unsigned char ff20[20] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    if (out == NULL || outlen == 0 || entry == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    /* Fast path: non-capsule candidate. */
    if (memcmp(entry->inherit_from, ff20, 20) != 0) {
        snprintf(out, outlen, "0");
        return TPBE_SUCCESS;
    }

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_task(workspace, entry->task_record_id, &attr,
                                   &data, &datasize);
    if (err != TPBE_SUCCESS) {
        snprintf(out, outlen, "?");
        if (warned != NULL && !*warned) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                            TPBLOG_FLAG_DIRECT,
                            "WARNING: failed to read capsule candidate; "
                            "Subproc shown as ?\n");
            *warned = 1;
        }
        return TPBE_SUCCESS;
    }

    if (!tpbcli_task_confirm_capsule(&attr, data, datasize)) {
        /* Looks like capsule in index bits but body is a normal entry. */
        snprintf(out, outlen, "0");
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
        return TPBE_SUCCESS;
    }

    err = tpbcli_task_read_capsule_members(&attr, data, datasize, &members,
                                           &nmembers);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    free(data);
    if (err != TPBE_SUCCESS) {
        snprintf(out, outlen, "?");
        if (warned != NULL && !*warned) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                            TPBLOG_FLAG_DIRECT,
                            "WARNING: capsule member list unreadable; "
                            "Subproc shown as ?\n");
            *warned = 1;
        }
        return TPBE_SUCCESS;
    }
    snprintf(out, outlen, "%d", nmembers);
    free(members);
    return TPBE_SUCCESS;
}

/**
 * @brief Implement `tpbcli task ls|list`.
 */
int
tpbcli_task_ls(const char *workspace, const tpbcli_task_ls_opts_t *opts)
{
    task_entry_t *all = NULL;
    kernel_entry_t *kentries = NULL;
    tpbcli_task_row_t *rows = NULL;
    tpbcli_task_row_t *matched = NULL;
    unsigned char (*rid_ids)[20] = NULL;
    int nall = 0;
    int nk = 0;
    int nentry = 0;
    int nmatched = 0;
    int nshow = 0;
    int i;
    int err;
    int kernel_warned = 0;
    int subproc_warned = 0;
    int start;
    int step;
    char ridmap_path[PATH_MAX];
    const char *headers[9];
    const char *cells[9];
    float ratios[9] = {
        4.0f, 22.0f, 14.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 9.0f
    };
    static const unsigned char z20[20] = {0};

    if (workspace == NULL || opts == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    err = tpbcli_task_ridmap_path(workspace, ridmap_path, sizeof(ridmap_path));
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "RIDMAP: %s\n", ridmap_path);

    err = tpb_raf_entry_list_task(workspace, &all, &nall);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "tpb_raf_entry_list_task");
    }

    for (i = 0; i < nall; i++) {
        if (memcmp(all[i].derive_to, z20, 20) == 0) {
            nentry++;
        }
    }
    if (nentry > 0) {
        rows = (tpbcli_task_row_t *)calloc((size_t)nentry, sizeof(*rows));
        if (rows == NULL) {
            free(all);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        nentry = 0;
        for (i = 0; i < nall; i++) {
            if (memcmp(all[i].derive_to, z20, 20) == 0) {
                rows[nentry].entry = all[i];
                rows[nentry].index_order = i;
                nentry++;
            }
        }
    }
    free(all);
    all = NULL;

    qsort(rows, (size_t)nentry, sizeof(*rows), _sf_cmp_rows);

    err = tpb_raf_entry_list_kernel(workspace, &kentries, &nk);
    if (err != TPBE_SUCCESS) {
        if (!kernel_warned) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                            TPBLOG_FLAG_DIRECT,
                            "WARNING: kernel index unavailable; "
                            "Kernel column uses ID prefixes\n");
            kernel_warned = 1;
        }
        kentries = NULL;
        nk = 0;
    }

    matched = (tpbcli_task_row_t *)calloc(
        nentry > 0 ? (size_t)nentry : 1u, sizeof(*matched));
    if (matched == NULL) {
        free(rows);
        free(kentries);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    for (i = 0; i < nentry; i++) {
        if (_sf_match_filters(&rows[i], kentries, nk, opts)) {
            matched[nmatched++] = rows[i];
        }
    }
    free(rows);
    rows = NULL;

    if (opts->count < 0 || opts->count == 0) {
        nshow = nmatched;
    } else {
        nshow = (nmatched < opts->count) ? nmatched : opts->count;
    }

    headers[0] = "Rid";
    headers[1] = "Start Time (Local)";
    headers[2] = "Kernel";
    headers[3] = "Exit";
    headers[4] = "Duration(s)";
    headers[5] = "Handle";
    headers[6] = "Subproc";
    headers[7] = "Task ID";
    headers[8] = "TBatch ID";
    tpblog_printf_c(ratios, 9, TPBCLI_TASK_COL_GAP, headers);

    if (nshow > 0) {
        rid_ids = (unsigned char (*)[20])calloc((size_t)nshow, sizeof(*rid_ids));
        if (rid_ids == NULL) {
            free(matched);
            free(kentries);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
    }

    if (opts->from_oldest) {
        start = 0;
        step = 1;
    } else {
        start = nmatched - 1;
        step = -1;
    }

    {
        int shown = 0;
        int idx;

        for (idx = start;
             shown < nshow && idx >= 0 && idx < nmatched;
             idx += step) {
            const task_entry_t *e = &matched[idx].entry;
            char c0[16];
            char c1[40];
            char c2[72];
            char c3[16];
            char c4[32];
            char c5[16];
            char c6[16];
            char c7[8];
            char c8[8];
            char kfb[8];
            const char *kname;
            double dur_sec = (double)e->duration / 1e9;

            snprintf(c0, sizeof(c0), "%d", shown);
            if (tpbcli_task_time_format_local(e->utc_bits, c1,
                                              sizeof(c1)) != TPBE_SUCCESS) {
                snprintf(c1, sizeof(c1), "-");
            }
            kname = _sf_lookup_kernel_name(kentries, nk, e->kernel_id, kfb,
                                           sizeof(kfb));
            snprintf(c2, sizeof(c2), "%s", kname);
            snprintf(c3, sizeof(c3), "%u", e->exit_code);
            snprintf(c4, sizeof(c4), "%.3f", dur_sec);
            snprintf(c5, sizeof(c5), "%u", e->handle_index);
            (void)_sf_subproc_cell(workspace, e, c6, sizeof(c6),
                                   &subproc_warned);
            tpbcli_task_format_id_prefix6(e->task_record_id, c7, sizeof(c7));
            tpbcli_task_format_id_prefix6(e->tbatch_id, c8, sizeof(c8));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            cells[6] = c6;
            cells[7] = c7;
            cells[8] = c8;
            tpblog_printf_c(ratios, 9, TPBCLI_TASK_COL_GAP, cells);

            if (rid_ids != NULL) {
                memcpy(rid_ids[shown], e->task_record_id, 20);
            }
            shown++;
        }
        nshow = shown;
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Found %d in %d task records, shown %d results.\n",
                    nmatched, nentry, nshow);

    if (nshow > 0) {
        err = tpbcli_task_ridmap_write_atomic(workspace, rid_ids, nshow);
        free(rid_ids);
        rid_ids = NULL;
        if (err != TPBE_SUCCESS) {
            free(matched);
            free(kentries);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "RIDMAP updated with %d entr%s.\n",
                        nshow, nshow == 1 ? "y" : "ies");
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "RIDMAP not updated (zero shown results).\n");
    }

    free(matched);
    free(kentries);
    return TPBE_SUCCESS;
}
