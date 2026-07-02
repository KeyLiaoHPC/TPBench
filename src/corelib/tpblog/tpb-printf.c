/*
 * tpb-printf.c
 * Formatted stdout and run-log output for the tpblog module.
 */

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "tpb-log.h"
#include "tpb-printf.h"
#include "tpb-public.h"

/* Global verbosity threshold; messages below this level are suppressed. */
static tpb_log_level_t s_log_level = TPB_LOG_LEVEL_INFO;

void
tpblog_set_level(tpb_log_level_t level)
{
    s_log_level = level;
}

tpb_log_level_t
tpblog_get_level(void)
{
    return s_log_level;
}

const char *
_tpblog_tag_from_type(uint32_t log_type)
{
    if (log_type == TPBLOG_TYPE_WARN || log_type == TPBE_WARN) {
        return "WARN";
    }
    if (log_type == TPBLOG_TYPE_ERRO || log_type == TPBE_FAIL
        || log_type == TPBE_UNKN) {
        return "ERRO";
    }
    return "INFO";
}

int
_tpblog_terminal_width(void)
{
    const char *env_width;
    struct winsize ws;

    env_width = getenv("TPBLOG_TEST_WIDTH");
    if (env_width != NULL && env_width[0] != '\0') {
        return atoi(env_width);
    }

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }

    return TPBLOG_DEFAULT_WIDTH;
}

int
_tpblog_compute_column_widths(int total_width, const float *col_ratios,
                              int ncol, int gap, int *widths_out)
{
    float sum_ratio = 0.0f;
    int usable;
    int i;

    if (widths_out == NULL || ncol <= 0) {
        return 1;
    }

    for (i = 0; i < ncol; i++) {
        widths_out[i] = 0;
    }

    usable = total_width - gap * (ncol - 1);
    if (usable <= 0) {
        return 1;
    }

    if (col_ratios == NULL) {
        sum_ratio = (float)ncol;
    } else {
        for (i = 0; i < ncol; i++) {
            if (col_ratios[i] > 0.0f) {
                sum_ratio += col_ratios[i];
            }
        }
        if (sum_ratio <= 0.0f) {
            sum_ratio = (float)ncol;
            col_ratios = NULL;
        }
    }

    /* Proportional floor allocation, then reserve one char per column. */
    for (i = 0; i < ncol; i++) {
        float ratio = 1.0f;
        if (col_ratios != NULL) {
            ratio = (col_ratios[i] > 0.0f) ? col_ratios[i] : 0.0f;
        }
        widths_out[i] = (int)floor((double)usable * (double)ratio
                                   / (double)sum_ratio);
        widths_out[i] -= 1;
        if (widths_out[i] < 1) {
            return 1;
        }
    }

    return 0;
}

static void
_sf_dual_write(const char *msg)
{
    if (msg == NULL) {
        return;
    }
    fputs(msg, stdout);
    _tpblog_write(msg);
    fflush(stdout);
}

static int
_sf_build_header(char *header_buf, size_t header_size, uint32_t flags,
                 uint32_t log_type)
{
    int header_len = 0;

    if (header_buf == NULL || header_size == 0) {
        return 0;
    }
    header_buf[0] = '\0';

    if (flags & TPBLOG_FLAG_TS) {
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        header_len += snprintf(header_buf + header_len,
                               header_size - (size_t)header_len,
                               "%04d-%02d-%02d %02d:%02d:%02d ",
                               lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                               lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
    if (flags & TPBLOG_FLAG_TAG) {
        header_len += snprintf(header_buf + header_len,
                               header_size - (size_t)header_len,
                               "[%s] ", _tpblog_tag_from_type(log_type));
    }
    return header_len;
}

static void
_sf_vprintf_dual(tpb_log_level_t level, uint32_t log_type, uint32_t flags,
                 const char *fmt, va_list args)
{
    char header_buf[128];
    char msg_buf[4096];
    int header_len;

    if (level < s_log_level || fmt == NULL) {
        return;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args_copy);
    va_end(args_copy);

    header_len = _sf_build_header(header_buf, sizeof(header_buf), flags,
                                  log_type);
    if (header_len > 0) {
        _sf_dual_write(header_buf);
    }
    _sf_dual_write(msg_buf);
}

void
tpblog_printf(tpb_log_level_t level, uint32_t log_type, uint32_t flags,
              const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _sf_vprintf_dual(level, log_type, flags, fmt, args);
    va_end(args);
}

void
tpblog_printf_f(tpb_log_level_t level, uint32_t log_type, uint32_t flags,
                const char *fmt, ...)
{
    va_list args;

    /* Same dual-write path as tpblog_printf; kept as a distinct entry point. */
    va_start(args, fmt);
    _sf_vprintf_dual(level, log_type, flags, fmt, args);
    va_end(args);
}

static void
_sf_print_prefix(int *widths, int ncols, int gap, int start_col)
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
        char spaces[512];
        int n = pad;

        if (n >= (int)sizeof(spaces)) {
            n = (int)sizeof(spaces) - 1;
        }
        memset(spaces, ' ', (size_t)n);
        spaces[n] = '\0';
        _sf_dual_write(spaces);
    }
}

static void
_sf_print_plain_cell(const char *text, int width)
{
    char buf[4096];

    if (text == NULL) {
        text = "";
    }
    snprintf(buf, sizeof(buf), "%-*s", width, text);
    _sf_dual_write(buf);
}

static void
_sf_print_wrapped_cell(const char *text, int width, int col_index,
                       int *widths, int ncols, int gap)
{
    size_t pos = 0;
    size_t len;
    int first = 1;

    if (text == NULL || text[0] == '\0') {
        _sf_print_plain_cell("", width);
        return;
    }
    len = strlen(text);

    while (pos < len) {
        size_t remaining;
        size_t chunk;
        size_t i;
        int found_space = 0;
        char line_buf[4096];

        if (!first) {
            _sf_dual_write("\n");
            _sf_print_prefix(widths, ncols, gap, col_index);
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
            snprintf(line_buf, sizeof(line_buf), "%-*s", width, text + pos);
            _sf_dual_write(line_buf);
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
            snprintf(line_buf, sizeof(line_buf), "%-*.*s-", width, (int)chunk,
                     text + pos);
            _sf_dual_write(line_buf);
            pos += chunk;
            continue;
        }

        while (chunk > 0 && isspace((unsigned char)text[pos + chunk - 1])) {
            chunk--;
        }
        snprintf(line_buf, sizeof(line_buf), "%-*.*s", width, (int)chunk,
                 text + pos);
        _sf_dual_write(line_buf);
        pos += chunk;
    }
}

static void
_sf_print_degenerate_lines(int ncol, const char *const *cells)
{
    int i;

    for (i = 0; i < ncol; i++) {
        const char *text = (cells != NULL && cells[i] != NULL) ? cells[i] : "";
        char line_buf[4096];

        snprintf(line_buf, sizeof(line_buf), "%s\n", text);
        _sf_dual_write(line_buf);
    }
}

void
tpblog_printf_c(const float *col_ratios, int ncol, int gap,
                const char *const *cells)
{
    const char *lines[TPBLOG_COLUMN_MAX][64];
    int line_count[TPBLOG_COLUMN_MAX];
    char line_bufs[TPBLOG_COLUMN_MAX][64][256];
    int widths[TPBLOG_COLUMN_MAX];
    int total_width;
    int max_lines = 1;
    int c;
    int r;

    if (ncol <= 0 || cells == NULL || ncol > TPBLOG_COLUMN_MAX || gap < 0) {
        return;
    }

    total_width = _tpblog_terminal_width();
    if (_tpblog_compute_column_widths(total_width, col_ratios, ncol, gap,
                                      widths) != 0) {
        _sf_print_degenerate_lines(ncol, cells);
        return;
    }

    for (c = 0; c < ncol; c++) {
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
        for (c = 0; c < ncol; c++) {
            const char *cell;
            char gap_buf[64];

            if (r < line_count[c]) {
                cell = lines[c][r];
            } else {
                cell = "";
            }
            _sf_print_plain_cell(cell, widths[c]);
            if (c + 1 < ncol) {
                if (gap > 0) {
                    int n = gap;
                    if (n >= (int)sizeof(gap_buf)) {
                        n = (int)sizeof(gap_buf) - 1;
                    }
                    memset(gap_buf, ' ', (size_t)n);
                    gap_buf[n] = '\0';
                    _sf_dual_write(gap_buf);
                }
            }
        }
        _sf_dual_write("\n");
    }
}
