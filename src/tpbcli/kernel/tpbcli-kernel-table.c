/*
 * tpbcli-kernel-table.c
 * Fixed-width table printing with wrapping and hyphenation.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "tpbcli-kernel-table.h"

/* Local Function Prototypes */
static void _sf_print_prefix(FILE *out, const int *widths, int ncols,
                             int gap, int start_col);
static void _sf_print_wrapped_cell(FILE *out, const char *text, int width,
                                   int col_index, const int *widths,
                                   int ncols, int gap);
static void _sf_print_plain_cell(FILE *out, const char *text, int width);

static void
_sf_print_prefix(FILE *out, const int *widths, int ncols, int gap,
                 int start_col)
{
    int c;
    int pad = 0;

    for (c = 0; c < start_col; c++) {
        pad += widths[c];
        if (c + 1 < ncols) {
            pad += gap;
        }
    }
    if (pad > 0) {
        fprintf(out, "%*s", pad, "");
    }
}

static void
_sf_print_plain_cell(FILE *out, const char *text, int width)
{
    if (text == NULL) {
        text = "";
    }
    fprintf(out, "%-*s", width, text);
}

static void
_sf_print_wrapped_cell(FILE *out, const char *text, int width,
                       int col_index, const int *widths, int ncols,
                       int gap)
{
    size_t pos = 0;
    size_t len;
    int first = 1;

    if (text == NULL || text[0] == '\0') {
        _sf_print_plain_cell(out, "", width);
        return;
    }
    len = strlen(text);

    while (pos < len) {
        size_t remaining;
        size_t chunk;
        size_t i;
        int found_space = 0;

        if (!first) {
            fprintf(out, "\n");
            _sf_print_prefix(out, widths, ncols, gap, col_index);
        }
        first = 0;

        while (pos < len && isspace((unsigned char)text[pos])) {
            pos++;
        }
        if (pos >= len) {
            break;
        }

        remaining = len - pos;
        if (remaining <= (size_t)width) {
            fprintf(out, "%-*s", width, text + pos);
            break;
        }

        chunk = (size_t)width;
        for (i = pos + (size_t)width; i > pos; i--) {
            if (isspace((unsigned char)text[i - 1])) {
                chunk = i - pos;
                found_space = 1;
                break;
            }
        }

        if (!found_space) {
            chunk = (size_t)(width - 1);
            if ((int)chunk < 1) {
                chunk = 1;
            }
            fprintf(out, "%-*.*s-", width, (int)chunk, text + pos);
            pos += chunk;
            continue;
        }

        while (chunk > 0 && isspace((unsigned char)text[pos + chunk - 1])) {
            chunk--;
        }
        fprintf(out, "%-*.*s", width, (int)chunk, text + pos);
        pos += chunk;
    }
}

void
tpbcli_kernel_table_print_header(FILE *out,
                                 const char *headers[],
                                 const int *widths,
                                 int ncols,
                                 int gap)
{
    int c;

    if (out == NULL || headers == NULL || widths == NULL || ncols <= 0) {
        return;
    }
    for (c = 0; c < ncols; c++) {
        _sf_print_plain_cell(out, headers[c], widths[c]);
        if (c + 1 < ncols) {
            fprintf(out, "%*s", gap, "");
        }
    }
    fputc('\n', out);
}

void
tpbcli_kernel_table_print_row(FILE *out,
                              const char *cells[],
                              const int *widths,
                              int ncols,
                              int gap)
{
    const char *lines[TPBCLI_KERNEL_TABLE_MAX_COLS][64];
    int line_count[TPBCLI_KERNEL_TABLE_MAX_COLS];
    char line_bufs[TPBCLI_KERNEL_TABLE_MAX_COLS][64][256];
    int max_lines = 1;
    int c;
    int r;

    if (out == NULL || cells == NULL || widths == NULL || ncols <= 0 ||
        ncols > TPBCLI_KERNEL_TABLE_MAX_COLS) {
        return;
    }

    for (c = 0; c < ncols; c++) {
        const char *text = (cells[c] != NULL) ? cells[c] : "";
        size_t pos = 0;
        size_t len = strlen(text);
        int ln = 0;

        line_count[c] = 0;
        while (pos < len && ln < 64) {
            size_t remaining;
            size_t chunk;
            size_t i;
            int found_space = 0;

            while (pos < len && isspace((unsigned char)text[pos])) {
                pos++;
            }
            if (pos >= len) {
                break;
            }
            remaining = len - pos;
            if (remaining <= (size_t)widths[c]) {
                snprintf(line_bufs[c][ln], sizeof(line_bufs[c][ln]),
                         "%.*s", (int)remaining, text + pos);
                ln++;
                break;
            }
            chunk = (size_t)widths[c];
            for (i = pos + (size_t)widths[c]; i > pos; i--) {
                if (isspace((unsigned char)text[i - 1])) {
                    chunk = i - pos;
                    found_space = 1;
                    break;
                }
            }
            if (!found_space) {
                chunk = (size_t)(widths[c] - 1);
                if ((int)chunk < 1) {
                    chunk = 1;
                }
                snprintf(line_bufs[c][ln], sizeof(line_bufs[c][ln]),
                         "%.*s-", (int)chunk, text + pos);
                pos += chunk;
                ln++;
                continue;
            }
            while (chunk > 0 &&
                   isspace((unsigned char)text[pos + chunk - 1])) {
                chunk--;
            }
            snprintf(line_bufs[c][ln], sizeof(line_bufs[c][ln]),
                     "%.*s", (int)chunk, text + pos);
            pos += chunk;
            ln++;
        }
        if (ln == 0) {
            line_bufs[c][0][0] = '\0';
            ln = 1;
        }
        line_count[c] = ln;
        if (ln > max_lines) {
            max_lines = ln;
        }
        for (r = 0; r < ln; r++) {
            lines[c][r] = line_bufs[c][r];
        }
    }

    for (r = 0; r < max_lines; r++) {
        for (c = 0; c < ncols; c++) {
            const char *cell;

            if (r < line_count[c]) {
                cell = lines[c][r];
            } else {
                cell = "";
            }
            _sf_print_plain_cell(out, cell, widths[c]);
            if (c + 1 < ncols) {
                fprintf(out, "%*s", gap, "");
            }
        }
        fputc('\n', out);
    }
}
