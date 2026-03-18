/**
 * @file strftime.c
 * @brief Implementation of internal datetime module.
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/tpb-public.h"
#include "strftime.h"

/* Bit layout constants for 64-bit datetime representation */
#define TPB_TS_BIT_SEC      0
#define TPB_TS_BIT_MIN      6
#define TPB_TS_BIT_HOUR     12
#define TPB_TS_BIT_DAY      17
#define TPB_TS_BIT_MONTH    22
#define TPB_TS_BIT_YEAR     26
#define TPB_TS_BIT_TZ       34

/*
 * Field masks positioned at their bit locations for clearer representation.
 * Format: (field_mask << bit_position)
 */
#define TPB_TS_MASK_SEC     (0x3FULL << 0)    /* bits 0-5:   seconds (0-59) */
#define TPB_TS_MASK_MIN     (0x3FULL << 6)    /* bits 6-11:  minutes (0-59) */
#define TPB_TS_MASK_HOUR    (0x1FULL << 12)   /* bits 12-16: hours (0-23) */
#define TPB_TS_MASK_DAY     (0x1FULL << 17)   /* bits 17-21: day (1-31) */
#define TPB_TS_MASK_MONTH   (0xFULL << 22)    /* bits 22-25: month (1-12) */
#define TPB_TS_MASK_YEAR    (0xFFULL << 26)   /* bits 26-33: year bias (0-255) */
#define TPB_TS_MASK_TZ      (0xFFULL << 34)   /* bits 34-41: timezone (15-min increments) */

#define TPB_TS_YEAR_BIAS    1970          /* Year bias for encoding */
#define TPB_TS_TZ_INCREMENT 15          /* Timezone bias stored in 15-min increments */

/* Forward declarations */
static int tpb_ts_from_timespec(tpb_datetime_t *dt, const struct timespec *ts);
static int tpb_ts_from_tm(tpb_datetime_t *dt, const struct tm *tm);

/**
 * @brief Acquire current datetime based on mode.
 *
 * TPBM_TS_UTC:   Get current UTC datetime
 * TPBM_TS_LOCAL: Get current local datetime
 */
int
tpb_ts_get_datetime(uint32_t mode, tpb_datetime_t *dt)
{
    if (dt == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    memset(dt, 0, sizeof(tpb_datetime_t));

    switch (mode) {
    case TPBM_TS_UTC:
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            tpb_ts_from_timespec(dt, &ts);
        }
        break;

    case TPBM_TS_LOCAL:
        {
            time_t now;
            struct tm tm_buf;
            time(&now);
            localtime_r(&now, &tm_buf);
            tpb_ts_from_tm(dt, &tm_buf);
        }
        break;

    default:
        /* Invalid mode: return error */
        return TPBE_ILLEGAL_CALL;
    }

    return 0;
}

/**
 * @brief Get boot-time timestamp.
 */
int
tpb_ts_get_btime(tpb_btime_t *btime)
{
    if (btime == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0) {
        /* Fallback to MONOTONIC if BOOTTIME not available */
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
            return TPBE_ILLEGAL_CALL;
        }
    }

    btime->sec = (uint64_t)ts.tv_sec;
    btime->nsec = (uint32_t)ts.tv_nsec;

    return 0;
}

/**
 * @brief Convert boot-time seconds to calendar datetime (rough approximation).
 *
 * Uses approximate values:
 * - 24 hours per day
 * - 730 hours per month (365/12 * 24)
 * - 8760 hours per year (365 * 24)
 *
 * Note: This assumes the system booted at 1970-01-01 00:00:00 UTC
 * and provides only approximate calendar dates.
 */
int
tpb_ts_btime_to_datetime(const tpb_btime_t *btime, tpb_datetime_t *dt)
{
    if (btime == NULL || dt == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Rough conversion constants */
    const uint64_t SEC_PER_MIN = 60;
    const uint64_t SEC_PER_HOUR = 3600;
    const uint64_t SEC_PER_DAY = 86400;
    const uint64_t HOURS_PER_MONTH = 730;      /* 365/12 * 24 */
    const uint64_t HOURS_PER_YEAR = 8760;      /* 365 * 24 */
    const uint64_t SEC_PER_MONTH = HOURS_PER_MONTH * SEC_PER_HOUR;
    const uint64_t SEC_PER_YEAR = HOURS_PER_YEAR * SEC_PER_HOUR;

    uint64_t total_sec = btime->sec;

    /* Calculate year (starting from 1970) */
    uint64_t years = total_sec / SEC_PER_YEAR;
    if (years > 255) {
        years = 255;  /* Clamp to valid range (1970-2226) */
    }
    dt->year = (uint16_t)(TPB_TS_YEAR_BIAS + years);
    total_sec -= years * SEC_PER_YEAR;

    /* Calculate month */
    uint64_t months = total_sec / SEC_PER_MONTH;
    if (months > 11) {
        months = 11;
    }
    dt->month = (uint8_t)(1 + months);
    total_sec -= months * SEC_PER_MONTH;

    /* Calculate day */
    uint64_t days = total_sec / SEC_PER_DAY;
    if (days > 30) {
        days = 30;
    }
    dt->day = (uint8_t)(1 + days);
    total_sec -= days * SEC_PER_DAY;

    /* Calculate hour */
    uint64_t hours = total_sec / SEC_PER_HOUR;
    dt->hour = (uint8_t)hours;
    total_sec -= hours * SEC_PER_HOUR;

    /* Calculate minute */
    uint64_t mins = total_sec / SEC_PER_MIN;
    dt->min = (uint8_t)mins;
    total_sec -= mins * SEC_PER_MIN;

    /* Remaining seconds */
    dt->sec = (uint8_t)total_sec;

    return 0;
}

/**
 * @brief Convert datetime to 64-bit representation.
 */
int
tpb_ts_datetime_to_bits(const tpb_datetime_t *dt, int16_t tz_bias_min,
                           tpb_dtbits_t *bits)
{
    if (dt == NULL || bits == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Validate year range (1970-2226) */
    if (dt->year < TPB_TS_YEAR_BIAS || dt->year > TPB_TS_YEAR_BIAS + 255) {
        return TPBE_ILLEGAL_CALL;
    }

    uint64_t b = 0;

    /* Pack components into 64-bit value using positioned masks */
    b |= ((uint64_t)(dt->sec & 0x3F) << TPB_TS_BIT_SEC);
    b |= ((uint64_t)(dt->min & 0x3F) << TPB_TS_BIT_MIN);
    b |= ((uint64_t)(dt->hour & 0x1F) << TPB_TS_BIT_HOUR);
    b |= ((uint64_t)(dt->day & 0x1F) << TPB_TS_BIT_DAY);
    b |= ((uint64_t)(dt->month & 0xF) << TPB_TS_BIT_MONTH);
    b |= ((uint64_t)((dt->year - TPB_TS_YEAR_BIAS) & 0xFF) << TPB_TS_BIT_YEAR);

    /* Convert timezone bias to 15-min increments and store */
    /* Range: -32 to +31.75 hours (-128 to +127 increments) */
    int tz_inc = tz_bias_min / TPB_TS_TZ_INCREMENT;
    if (tz_inc < -128) tz_inc = -128;
    if (tz_inc > 127) tz_inc = 127;
    b |= ((uint64_t)(tz_inc & 0xFF) << TPB_TS_BIT_TZ);

    *bits = b;
    return 0;
}

/**
 * @brief Convert 64-bit representation to datetime.
 */
int
tpb_ts_bits_to_datetime(tpb_dtbits_t bits, tpb_datetime_t *dt,
                         int16_t *tz_bias_min)
{
    if (dt == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Extract fields using positioned masks: mask isolates field, shift normalizes */
    dt->sec = (uint8_t)((bits & TPB_TS_MASK_SEC) >> TPB_TS_BIT_SEC);
    dt->min = (uint8_t)((bits & TPB_TS_MASK_MIN) >> TPB_TS_BIT_MIN);
    dt->hour = (uint8_t)((bits & TPB_TS_MASK_HOUR) >> TPB_TS_BIT_HOUR);
    dt->day = (uint8_t)((bits & TPB_TS_MASK_DAY) >> TPB_TS_BIT_DAY);
    dt->month = (uint8_t)((bits & TPB_TS_MASK_MONTH) >> TPB_TS_BIT_MONTH);
    dt->year = (uint16_t)((bits & TPB_TS_MASK_YEAR) >> TPB_TS_BIT_YEAR) + TPB_TS_YEAR_BIAS;

    /* Decode timezone bias from 15-min increments */
    if (tz_bias_min != NULL) {
        uint8_t tz_inc_u = (uint8_t)((bits & TPB_TS_MASK_TZ) >> TPB_TS_BIT_TZ);
        int8_t tz_inc = (tz_inc_u <= 127) ? (int8_t)tz_inc_u : (int8_t)(tz_inc_u - 256);
        // int8_t tz_inc = (int8_t)((bits & TPB_TS_MASK_TZ) >> TPB_TS_BIT_TZ);
        *tz_bias_min = (int16_t)tz_inc * TPB_TS_TZ_INCREMENT;
    }

    return 0;
}

/**
 * @brief Convert 64-bit to ISO 8601 UTC string (YYYY-MM-DDThh:mm:ssZ).
 */
int
tpb_ts_bits_to_isoutc(tpb_dtbits_t bits, tpb_datetime_str_t *str)
{
    tpb_datetime_t dt;

    if (str == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (tpb_ts_bits_to_datetime(bits, &dt, NULL) != 0) {
        return TPBE_ILLEGAL_CALL;
    }

    int written = snprintf(str->str, sizeof(str->str),
                           "%04u-%02u-%02uT%02u:%02u:%02uZ",
                           dt.year, dt.month, dt.day,
                           dt.hour, dt.min, dt.sec);

    if (written < 0 || (size_t)written >= sizeof(str->str)) {
        return TPBE_CLI_FAIL;
    }

    return 0;
}

/**
 * @brief Convert 64-bit to ISO 8601 with timezone (YYYY-MM-DDThh:mm:ss+HH:MM).
 */
int
tpb_ts_bits_to_isotz(tpb_dtbits_t bits, int16_t tz_bias_min,
                      tpb_datetime_str_t *str)
{
    tpb_datetime_t dt;

    if (str == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (tpb_ts_bits_to_datetime(bits, &dt, NULL) != 0) {
        return TPBE_ILLEGAL_CALL;
    }

    int tz_sign = (tz_bias_min >= 0) ? 1 : -1;
    int tz_abs_min = (tz_bias_min >= 0) ? tz_bias_min : -tz_bias_min;
    int tz_hours = tz_abs_min / 60;
    int tz_mins = tz_abs_min % 60;

    int written = snprintf(str->str, sizeof(str->str),
                           "%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02d",
                           dt.year, dt.month, dt.day,
                           dt.hour, dt.min, dt.sec,
                           (tz_sign >= 0) ? '+' : '-',
                           tz_hours, tz_mins);

    if (written < 0 || (size_t)written >= sizeof(str->str)) {
        return TPBE_CLI_FAIL;
    }

    return 0;
}

/**
 * @brief Parse ISO 8601 UTC string to 64-bit representation.
 */
int
tpb_ts_isoutc_to_bits(const tpb_datetime_str_t *str, tpb_dtbits_t *bits)
{
    tpb_datetime_t dt = {0};

    if (str == NULL || bits == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Parse YYYY-MM-DDThh:mm:ssZ */
    unsigned int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    int parsed = sscanf(str->str, "%4u-%2u-%2uT%2u:%2u:%2uZ",
                        &year, &month, &day, &hour, &min, &sec);

    if (parsed != 6) {
        return TPBE_ILLEGAL_CALL;
    }

    dt.year = (uint16_t)year;
    dt.month = (uint8_t)month;
    dt.day = (uint8_t)day;
    dt.hour = (uint8_t)hour;
    dt.min = (uint8_t)min;
    dt.sec = (uint8_t)sec;

    return tpb_ts_datetime_to_bits(&dt, 0, bits);  /* UTC has 0 timezone bias */
}

/**
 * @brief Parse ISO 8601 string with timezone to 64-bit representation.
 */
int
tpb_ts_isotz_to_bits(const tpb_datetime_str_t *str, tpb_dtbits_t *bits,
                      int16_t *tz_bias_min)
{
    tpb_datetime_t dt = {0};

    if (str == NULL || bits == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Parse YYYY-MM-DDThh:mm:ss+HH:MM or YYYY-MM-DDThh:mm:ss-HH:MM */
    unsigned int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    int tz_hour = 0, tz_min = 0;
    char tz_sign = '+';

    /* Try format with colon in timezone: YYYY-MM-DDThh:mm:ss+HH:MM */
    int parsed = sscanf(str->str, "%4u-%2u-%2uT%2u:%2u:%2u%c%2d:%2d",
                        &year, &month, &day, &hour, &min, &sec,
                        &tz_sign, &tz_hour, &tz_min);

    if (parsed != 9) {
        /* Try format without colon in timezone: YYYY-MM-DDThh:mm:ss+HHMM */
        parsed = sscanf(str->str, "%4u-%2u-%2uT%2u:%2u:%2u%c%2d%2d",
                        &year, &month, &day, &hour, &min, &sec,
                        &tz_sign, &tz_hour, &tz_min);
        if (parsed != 9) {
            /* Try format with just hour offset: YYYY-MM-DDThh:mm:ss+HH */
            tz_min = 0;
            parsed = sscanf(str->str, "%4u-%2u-%2uT%2u:%2u:%2u%c%2d",
                            &year, &month, &day, &hour, &min, &sec,
                            &tz_sign, &tz_hour);
            if (parsed != 8) {
                return TPBE_ILLEGAL_CALL;
            }
        }
    }

    dt.year = (uint16_t)year;
    dt.month = (uint8_t)month;
    dt.day = (uint8_t)day;
    dt.hour = (uint8_t)hour;
    dt.min = (uint8_t)min;
    dt.sec = (uint8_t)sec;

    int16_t bias = (int16_t)(tz_hour * 60 + tz_min);
    if (tz_sign == '-') {
        bias = -bias;
    }

    if (tz_bias_min != NULL) {
        *tz_bias_min = bias;
    }

    return tpb_ts_datetime_to_bits(&dt, bias, bits);
}

/**
 * @brief Helper: Convert timespec to datetime (UTC).
 */
static int
tpb_ts_from_timespec(tpb_datetime_t *dt, const struct timespec *ts)
{
    time_t sec = ts->tv_sec;
    struct tm tm_buf;

    if (dt == NULL || ts == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    gmtime_r(&sec, &tm_buf);
    tpb_ts_from_tm(dt, &tm_buf);

    return 0;
}

/**
 * @brief Helper: Convert tm to datetime.
 */
static int
tpb_ts_from_tm(tpb_datetime_t *dt, const struct tm *tm)
{
    if (dt == NULL || tm == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    dt->year = (uint16_t)(tm->tm_year + 1900);
    dt->month = (uint8_t)(tm->tm_mon + 1);
    dt->day = (uint8_t)(tm->tm_mday);
    dt->hour = (uint8_t)(tm->tm_hour);
    dt->min = (uint8_t)(tm->tm_min);
    dt->sec = (uint8_t)(tm->tm_sec);

    return 0;
}
