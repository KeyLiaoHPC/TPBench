/*
 * tpbcli-kernel-list.c
 * Kernel list output for `tpbcli kernel list` / `tpbcli k ls`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#include "tpb-public.h"

#include "tpbcli-kernel-list.h"
#include "tpbcli-kernel-home.h"
#include "tpbcli-kernel-registry.h"
#include "tpbcli-kernel-registry.h"

#define TPBCLI_KERNEL_LIST_MAX_ROWS  (TPBCLI_KERNEL_REG_MAX + 64)

typedef struct {
    char name[TPBCLI_KERNEL_REG_NAME_MAX];
    char kid_str[16];
    char tags[TPBCLI_KERNEL_REG_TAGS_MAX];
    char description[2048];
    int compiled;
    int sort_key;
} tpbcli_kernel_list_row_t;

/* Local Function Prototypes */
static int _sf_kernel_id_is_zero(const unsigned char id[20]);
static int _sf_row_find(tpbcli_kernel_list_row_t *rows, int nrows,
                        const char *name);
static void _sf_rows_sort_uncompiled(tpbcli_kernel_list_row_t *rows,
                                     int nrows, int compiled_start);
static int _sf_row_cmp_name(const void *a, const void *b);

static int
_sf_kernel_id_is_zero(const unsigned char id[20])
{
    static const unsigned char z[20] = {0};

    return memcmp(id, z, 20) == 0;
}

static int
_sf_row_find(tpbcli_kernel_list_row_t *rows, int nrows, const char *name)
{
    int i;

    for (i = 0; i < nrows; i++) {
        if (strcmp(rows[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int
_sf_row_cmp_name(const void *a, const void *b)
{
    const tpbcli_kernel_list_row_t *ra = (const tpbcli_kernel_list_row_t *)a;
    const tpbcli_kernel_list_row_t *rb = (const tpbcli_kernel_list_row_t *)b;

    return strcmp(ra->name, rb->name);
}

static void
_sf_rows_sort_uncompiled(tpbcli_kernel_list_row_t *rows, int nrows,
                        int compiled_start)
{
    int n = nrows - compiled_start;

    if (n <= 1) {
        return;
    }
    qsort(rows + compiled_start, (size_t)n, sizeof(rows[0]), _sf_row_cmp_name);
}

int
tpbcli_kernel_list(int argc, char **argv)
{
    tpbcli_kernel_list_row_t rows[TPBCLI_KERNEL_LIST_MAX_ROWS];
    tpbcli_kernel_reg_list_t reg;
    char tpb_home[PATH_MAX];
    const char *headers[4];
    const char *cells[4];
    float ratios[4] = {15.0f, 9.0f, 20.0f, 40.0f};
    int nrows = 0;
    int ncompiled = 0;
    int nkern;
    int i;
    int err;

    (void)argc;
    (void)argv;

    err = tpbcli_kernel_resolve_home(NULL, tpb_home, sizeof(tpb_home));
    if (err != TPBE_SUCCESS) {
        tpb_home[0] = '\0';
    } else {
        char resolved[PATH_MAX];

        if (realpath(tpb_home, resolved) != NULL) {
            snprintf(tpb_home, sizeof(tpb_home), "%s", resolved);
        }
    }

    nkern = tpb_query_kernel(0, NULL, NULL);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Listing supported kernels.\n");

    for (i = 0; i < nkern; i++) {
        tpb_kernel_t *kernel = NULL;

        tpb_query_kernel(i, NULL, &kernel);
        if (kernel == NULL) {
            continue;
        }
        if (!kernel->info.kernel_record_ok) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                       "Kernel %s: workspace kernel record update failed.\n",
                       kernel->info.name);
        }
        tpb_free_kernel(kernel);
        free(kernel);
    }

    if (tpb_home[0] != '\0') {
        (void)tpbcli_kernel_reg_load(tpb_home, &reg);
    } else {
        memset(&reg, 0, sizeof(reg));
    }

    for (i = 0; i < nkern; i++) {
        tpb_kernel_t *kernel = NULL;
        const tpbcli_kernel_reg_entry_t *ent;
        tpbcli_kernel_list_row_t *row;

        tpb_query_kernel(i, NULL, &kernel);
        if (kernel == NULL) {
            continue;
        }
        if (nrows >= TPBCLI_KERNEL_LIST_MAX_ROWS) {
            tpb_free_kernel(kernel);
            free(kernel);
            break;
        }

        row = &rows[nrows];
        memset(row, 0, sizeof(*row));
        snprintf(row->name, sizeof(row->name), "%s", kernel->info.name);
        row->compiled = 1;
        row->sort_key = ncompiled;

        if (!kernel->info.kernel_record_ok) {
            snprintf(row->kid_str, sizeof(row->kid_str), "[ERROR]");
        } else if (_sf_kernel_id_is_zero(kernel->info.kernel_id)) {
            snprintf(row->kid_str, sizeof(row->kid_str), "-");
        } else {
            char hex[41];

            tpb_raf_id_to_hex(kernel->info.kernel_id, hex);
            snprintf(row->kid_str, sizeof(row->kid_str), "%.6s*", hex);
        }

        ent = tpbcli_kernel_reg_find(&reg, kernel->info.name);
        if (ent != NULL && ent->tags[0] != '\0') {
            snprintf(row->tags, sizeof(row->tags), "%s", ent->tags);
        } else {
            snprintf(row->tags, sizeof(row->tags), "-");
        }
        snprintf(row->description, sizeof(row->description), "%s",
                 kernel->info.note);
        nrows++;
        ncompiled++;
        tpb_free_kernel(kernel);
        free(kernel);
    }

    for (i = 0; i < reg.count; i++) {
        tpbcli_kernel_list_row_t *row;
        int idx;

        idx = _sf_row_find(rows, nrows, reg.entries[i].name);
        if (idx >= 0) {
            continue;
        }
        if (nrows >= TPBCLI_KERNEL_LIST_MAX_ROWS) {
            break;
        }
        row = &rows[nrows];
        memset(row, 0, sizeof(*row));
        snprintf(row->name, sizeof(row->name), "%s", reg.entries[i].name);
        snprintf(row->kid_str, sizeof(row->kid_str), "N/A");
        if (reg.entries[i].tags[0] != '\0') {
            snprintf(row->tags, sizeof(row->tags), "%s",
                     reg.entries[i].tags);
        } else {
            snprintf(row->tags, sizeof(row->tags), "-");
        }
        snprintf(row->description, sizeof(row->description), "-");
        row->compiled = 0;
        nrows++;
    }

    _sf_rows_sort_uncompiled(rows, nrows, ncompiled);

    headers[0] = "Kernel";
    headers[1] = "KernelID";
    headers[2] = "Tags";
    headers[3] = "Description";
    tpblog_printf_c(ratios, 4, 1, headers);

    for (i = 0; i < nrows; i++) {
        cells[0] = rows[i].name;
        cells[1] = rows[i].kid_str;
        cells[2] = rows[i].tags;
        cells[3] = rows[i].description;
        tpblog_printf_c(ratios, 4, 1, cells);
    }

    return 0;
}
