/*
 * tpbcli-task-time.c
 * Local timezone display and datetime filter parsing for `tpbcli task`.
 *
 * Records store UTC wall clock in utc_bits. Terminal output converts that
 * instant to the process local zone with an explicit ±HH:MM suffix. Filter
 * inputs may be Z, offset, or bare local wall clock; comparisons always use
 * UTC-normalized bits (timezone field forced to zero).
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tpb-public.h"
#include "tpbcli-task-internal.h"

/* Local Function Prototypes */
static int _sf_parse_int_span(const char *s, int n, int *out);
static int _sf_days_in_month(int year, int month);
static int _sf_validate_civil(int year, int month, int day,
                              int hour, int min, int sec);
static int _sf_civil_to_epoch_utc(int year, int month, int day,
                                  int hour, int min, int sec,
                                  time_t *epoch_out);
static int _sf_local_offset_minutes(time_t epoch, int *offset_min_out);
static int _sf_parse_offset_suffix(const char *s, int *bias_min_out,
                                   const char **end_out);

static int
_sf_parse_int_span(const char *s, int n, int *out)
{
    int i;
    int v = 0;

    if (s == NULL || out == NULL || n <= 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return -1;
        }
        if (v > (INT_MAX - (s[i] - '0')) / 10) {
            return -1;
        }
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return 0;
}

static int
_sf_days_in_month(int year, int month)
{
    static const int mdays[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int leap;

    if (month < 1 || month > 12) {
        return 0;
    }
    leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    if (month == 2 && leap) {
        return 29;
    }
    return mdays[month - 1];
}

static int
_sf_validate_civil(int year, int month, int day, int hour, int min, int sec)
{
    int dim;

    if (year < 1970 || year > 2225) {
        return -1;
    }
    if (month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 ||
        min < 0 || min > 59 || sec < 0 || sec > 59) {
        return -1;
    }
    dim = _sf_days_in_month(year, month);
    if (day > dim) {
        return -1;
    }
    return 0;
}

/*
 * Convert UTC civil time to epoch seconds without relying on timegm().
 * Uses Howard Hinnant civil-from-days style day count from 1970-01-01.
 */
static int
_sf_civil_to_epoch_utc(int year, int month, int day, int hour, int min,
                       int sec, time_t *epoch_out)
{
    int y;
    int m;
    int era;
    int yoe;
    int doy;
    int doe;
    long long days;
    long long secs;

    if (epoch_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (_sf_validate_civil(year, month, day, hour, min, sec) != 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    y = year;
    m = month;
    if (m <= 2) {
        y -= 1;
        m += 9;
    } else {
        m -= 3;
    }
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * m + 2) / 5 + day - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    days = (long long)era * 146097 + (long long)doe - 719468;
    secs = days * 86400LL + (long long)hour * 3600LL +
           (long long)min * 60LL + (long long)sec;
    if (secs < (long long)((time_t)0) && (time_t)secs > 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *epoch_out = (time_t)secs;
    return TPBE_SUCCESS;
}

static int
_sf_local_offset_minutes(time_t epoch, int *offset_min_out)
{
    struct tm local_tm;
    struct tm gmt_tm;
    time_t local_as_utc;
    time_t gmt_as_utc;
    double diff;
    int err;

    if (offset_min_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (localtime_r(&epoch, &local_tm) == NULL ||
        gmtime_r(&epoch, &gmt_tm) == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    err = _sf_civil_to_epoch_utc(local_tm.tm_year + 1900,
                                 local_tm.tm_mon + 1,
                                 local_tm.tm_mday,
                                 local_tm.tm_hour,
                                 local_tm.tm_min,
                                 local_tm.tm_sec,
                                 &local_as_utc);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    err = _sf_civil_to_epoch_utc(gmt_tm.tm_year + 1900,
                                 gmt_tm.tm_mon + 1,
                                 gmt_tm.tm_mday,
                                 gmt_tm.tm_hour,
                                 gmt_tm.tm_min,
                                 gmt_tm.tm_sec,
                                 &gmt_as_utc);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    diff = difftime(local_as_utc, gmt_as_utc);
    *offset_min_out = (int)(diff / 60.0);
    return TPBE_SUCCESS;
}

static int
_sf_parse_offset_suffix(const char *s, int *bias_min_out, const char **end_out)
{
    int sign = 1;
    int hour = 0;
    int min = 0;
    const char *p = s;

    if (s == NULL || bias_min_out == NULL) {
        return -1;
    }
    if (*p == '+') {
        sign = 1;
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    } else {
        return -1;
    }

    if (!isdigit((unsigned char)p[0])) {
        return -1;
    }
    if (isdigit((unsigned char)p[1])) {
        if (_sf_parse_int_span(p, 2, &hour) != 0) {
            return -1;
        }
        p += 2;
    } else {
        hour = p[0] - '0';
        p += 1;
    }
    if (hour > 18) {
        return -1;
    }

    if (*p == ':') {
        p++;
        if (_sf_parse_int_span(p, 2, &min) != 0) {
            return -1;
        }
        p += 2;
    } else if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
        if (_sf_parse_int_span(p, 2, &min) != 0) {
            return -1;
        }
        p += 2;
    } else if (*p != '\0') {
        return -1;
    }
    if (min < 0 || min > 59) {
        return -1;
    }
    *bias_min_out = sign * (hour * 60 + min);
    if (end_out != NULL) {
        *end_out = p;
    }
    return 0;
}

/**
 * @brief Format task utc_bits as local ISO-8601 with explicit offset.
 */
int
tpbcli_task_time_format_local(tpb_dtbits_t bits, char *out, size_t outlen)
{
    tpb_datetime_str_t utc_str;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    int offset_min = 0;
    int off_abs;
    int off_h;
    int off_m;
    char sign;
    time_t epoch;
    struct tm local_tm;
    int err;
    int n;

    if (out == NULL || outlen < 26) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    err = tpb_ts_bits_to_isoutc(bits, &utc_str);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (sscanf(utc_str.str, "%4d-%2d-%2dT%2d:%2d:%2dZ",
               &year, &month, &day, &hour, &min, &sec) != 6) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    err = _sf_civil_to_epoch_utc(year, month, day, hour, min, sec, &epoch);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (localtime_r(&epoch, &local_tm) == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = _sf_local_offset_minutes(epoch, &offset_min);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    sign = (offset_min >= 0) ? '+' : '-';
    off_abs = (offset_min >= 0) ? offset_min : -offset_min;
    off_h = off_abs / 60;
    off_m = off_abs % 60;
    n = snprintf(out, outlen, "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                 local_tm.tm_year + 1900, local_tm.tm_mon + 1,
                 local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min,
                 local_tm.tm_sec, sign, off_h, off_m);
    if (n < 0 || (size_t)n >= outlen) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

/**
 * @brief Format task utc_bits as ISO-8601 UTC (trailing 'Z', no offset).
 */
int
tpbcli_task_time_format_utc(tpb_dtbits_t bits, char *out, size_t outlen)
{
    tpb_datetime_str_t utc_str;
    int err;

    if (out == NULL || outlen == 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    err = tpb_ts_bits_to_isoutc(bits, &utc_str);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    snprintf(out, outlen, "%s", utc_str.str);
    return TPBE_SUCCESS;
}

/**
 * @brief Parse a filter datetime string into UTC bits (tz field zero).
 */
int
tpbcli_task_time_parse_filter(const char *text, tpb_dtbits_t *bits_out)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    int bias_min = 0;
    int have_tz = 0;
    const char *p;
    const char *end = NULL;
    tpb_datetime_t dt;
    time_t epoch;
    time_t check;
    struct tm wall;
    struct tm roundtrip;
    int err;

    if (text == NULL || bits_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (strlen(text) < 19) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid datetime '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid datetime '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (_sf_parse_int_span(text, 4, &year) != 0 ||
        _sf_parse_int_span(text + 5, 2, &month) != 0 ||
        _sf_parse_int_span(text + 8, 2, &day) != 0 ||
        _sf_parse_int_span(text + 11, 2, &hour) != 0 ||
        _sf_parse_int_span(text + 14, 2, &min) != 0 ||
        _sf_parse_int_span(text + 17, 2, &sec) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid datetime '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (_sf_validate_civil(year, month, day, hour, min, sec) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid datetime '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    p = text + 19;
    if (*p == '\0') {
        have_tz = 0;
    } else if (*p == 'Z' && p[1] == '\0') {
        have_tz = 1;
        bias_min = 0;
    } else if (_sf_parse_offset_suffix(p, &bias_min, &end) == 0 &&
               end != NULL && *end == '\0') {
        have_tz = 1;
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid datetime timezone in '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (have_tz) {
        err = _sf_civil_to_epoch_utc(year, month, day, hour, min, sec, &epoch);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        /* Offset means local = UTC + bias; invert to get UTC epoch. */
        epoch -= (time_t)bias_min * 60;
    } else {
        memset(&wall, 0, sizeof(wall));
        wall.tm_year = year - 1900;
        wall.tm_mon = month - 1;
        wall.tm_mday = day;
        wall.tm_hour = hour;
        wall.tm_min = min;
        wall.tm_sec = sec;
        wall.tm_isdst = -1;
        errno = 0;
        epoch = mktime(&wall);
        if (epoch == (time_t)-1 && errno != 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: invalid local datetime '%s'\n", text);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        /* Reject nonexistent DST wall clocks by round-trip. */
        if (localtime_r(&epoch, &roundtrip) == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        if (roundtrip.tm_year != wall.tm_year ||
            roundtrip.tm_mon != wall.tm_mon ||
            roundtrip.tm_mday != wall.tm_mday ||
            roundtrip.tm_hour != wall.tm_hour ||
            roundtrip.tm_min != wall.tm_min ||
            roundtrip.tm_sec != wall.tm_sec) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: nonexistent local datetime '%s'\n", text);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }

    {
        struct tm gmt;

        if (gmtime_r(&epoch, &gmt) == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        memset(&dt, 0, sizeof(dt));
        dt.year = (uint16_t)(gmt.tm_year + 1900);
        dt.month = (uint8_t)(gmt.tm_mon + 1);
        dt.day = (uint8_t)gmt.tm_mday;
        dt.hour = (uint8_t)gmt.tm_hour;
        dt.min = (uint8_t)gmt.tm_min;
        dt.sec = (uint8_t)gmt.tm_sec;
        err = tpb_ts_datetime_to_bits(&dt, 0, bits_out);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

        /* Defensive: ensure re-encoded bits map back to the same UTC instant. */
        err = _sf_civil_to_epoch_utc(dt.year, dt.month, dt.day, dt.hour,
                                     dt.min, dt.sec, &check);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        if (check != epoch) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }
    return TPBE_SUCCESS;
}
