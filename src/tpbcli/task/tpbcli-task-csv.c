/*
 * tpbcli-task-csv.c
 * Double-comma CSV writer for task export (design §8.5–8.6).
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-task-csv.h"

/* Local Function Prototypes */
static int _sf_write_raw(tpbcli_task_csv_writer_t *w, const char *s, size_t n);
static int _sf_write_escaped_text(tpbcli_task_csv_writer_t *w, const char *text);

static int
_sf_write_raw(tpbcli_task_csv_writer_t *w, const char *s, size_t n)
{
    size_t i;

    if (w == NULL || w->fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    for (i = 0; i < n; i++) {
        if (fputc((unsigned char)s[i], w->fp) == EOF) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
        }
    }
    return TPBE_SUCCESS;
}

static int
_sf_write_escaped_text(tpbcli_task_csv_writer_t *w, const char *text)
{
    const char *p;
    int err;

    if (text == NULL || text[0] == '\0') {
        return TPBE_SUCCESS;
    }
    for (p = text; *p != '\0'; p++) {
        if (*p == '\\') {
            err = _sf_write_raw(w, "\\\\", 2);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        } else if (*p == '\r') {
            err = _sf_write_raw(w, "\\r", 2);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        } else if (*p == '\n') {
            err = _sf_write_raw(w, "\\n", 2);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        } else if (*p == ',') {
            if (p[1] == ',') {
                err = _sf_write_raw(w, ", ,", 3);
                TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
                p++;
            } else {
                err = _sf_write_raw(w, ",", 1);
                TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
            }
        } else {
            if (fputc((unsigned char)*p, w->fp) == EOF) {
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
            }
        }
    }
    if (text[strlen(text) - 1] == ',') {
        err = _sf_write_raw(w, " ", 1);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    return TPBE_SUCCESS;
}

void
tpbcli_task_csv_writer_init(tpbcli_task_csv_writer_t *w, FILE *fp)
{
    if (w == NULL) {
        return;
    }
    w->fp = fp;
    w->in_row = 0;
    w->ncells = 0;
}

int
tpbcli_task_csv_row_begin(tpbcli_task_csv_writer_t *w)
{
    int err;

    if (w == NULL || w->fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (w->in_row) {
        err = tpbcli_task_csv_row_end(w);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    w->in_row = 1;
    w->ncells = 0;
    return TPBE_SUCCESS;
}

int
tpbcli_task_csv_cell(tpbcli_task_csv_writer_t *w, const char *text)
{
    int err;

    if (w == NULL || w->fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (!w->in_row) {
        err = tpbcli_task_csv_row_begin(w);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    if (w->ncells > 0) {
        err = _sf_write_raw(w, ",,", 2);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    err = _sf_write_escaped_text(w, text);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    w->ncells++;
    return TPBE_SUCCESS;
}

int
tpbcli_task_csv_cell_byte(tpbcli_task_csv_writer_t *w, unsigned char b)
{
    int err;

    if (w == NULL || w->fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (!w->in_row) {
        err = tpbcli_task_csv_row_begin(w);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    if (w->ncells > 0) {
        err = _sf_write_raw(w, ",,", 2);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    if (b == ',') {
        if (fputc(0x1C, w->fp) == EOF) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
        }
    } else if (isprint((int)b)) {
        if (fputc(b, w->fp) == EOF) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
        }
    } else {
        char hx[8];
        snprintf(hx, sizeof(hx), "0x%02X", (unsigned)b);
        err = _sf_write_raw(w, hx, strlen(hx));
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }
    w->ncells++;
    return TPBE_SUCCESS;
}

int
tpbcli_task_csv_row_end(tpbcli_task_csv_writer_t *w)
{
    if (w == NULL || w->fp == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (!w->in_row) {
        return TPBE_SUCCESS;
    }
    if (fputc('\n', w->fp) == EOF) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    w->in_row = 0;
    w->ncells = 0;
    return TPBE_SUCCESS;
}
