/**
 * @file strftime.h
 * @brief Internal datetime acquisition, parsing, and formatting module.
 *
 * This is a stateless internal module for TPBench corelib that provides
 * datetime operations including:
 * - Acquiring current time (epoch, UTC, or local)
 * - Getting boot-time timestamp (seconds and nanoseconds since boot)
 * - Encoding/decoding datetime to/from 64-bit representation
 * - Formatting to ISO 8601 strings (UTC and timezone-aware)
 * - Parsing ISO 8601 strings back to datetime
 *
 * Bit Layout (64 bits, tpb_dtbits_t):
 * - Bits 0-5:   seconds (6 bits, 0-59)
 * - Bits 6-11:  minutes (6 bits, 0-59)
 * - Bits 12-16: hours (5 bits, 0-23)
 * - Bits 17-21: day (5 bits, 1-31)
 * - Bits 22-25: month (4 bits, 1-12)
 * - Bits 26-33: year bias (8 bits, 0-255 = 1970-2225)
 * - Bits 34-41: timezone bias (8 bits, 15-min increments, -32 to +31.75 hours)
 * - Bits 42-63: reserved (22 bits, unused)
 *
 * Conversion approximations (rough calculations):
 * - 24 hours per day
 * - 730 hours per month (365/12 * 24)
 * - 8760 hours per year (365 * 24)
 */

#ifndef STRFTIME_H
#define STRFTIME_H

#include <stdint.h>

/**
 * @brief Datetime structure storing components from year to second.
 */
typedef struct {
    uint16_t year;    /* Full year, e.g., 2026 (range: 1970-2226) */
    uint8_t  month;   /* 1-12 */
    uint8_t  day;     /* 1-31 */
    uint8_t  hour;    /* 0-23 */
    uint8_t  min;     /* 0-59 */
    uint8_t  sec;     /* 0-59 */
} tpb_datetime_t;

/**
 * @brief 64-bit bit-packed datetime representation.
 *
 * Year is stored as bias from 1970 (0-255, giving range 1970-2226).
 * Timezone bias is stored in 15-minute increments.
 */
typedef uint64_t tpb_dtbits_t;

/**
 * @brief Boot-time timestamp (seconds and nanoseconds since system boot).
 */
typedef struct {
    uint64_t sec;     /* Seconds since boot */
    uint32_t nsec;    /* Nanoseconds within the second (0-999999999) */
} tpb_btime_t;

/**
 * @brief Encapsulated buffer for ISO 8601 formatted datetime strings.
 */
typedef struct {
    char str[32];  /* Buffer for ISO 8601 formatted datetime string */
} tpb_datetime_str_t;

/**
 * @brief Mode constants for datetime acquisition.
 */
#define TPBM_TS_UTC   0x01  /* Current UTC datetime */
#define TPBM_TS_LOCAL 0x02  /* Current local datetime */

/**
 * @brief Boot time reference constant.
 *
 * Used in comments to indicate conversion from boot-time (seconds since
 * system boot) to calendar datetime. Rough conversions use:
 * - 24 hours per day
 * - 730 hours per month
 * - 8760 hours per year
 */
#define TPBM_TS_BOOT  0x04

/**
 * @brief Acquire current datetime based on mode.
 *
 * TPBM_TS_UTC:   Get current UTC datetime (timezone is UTC)
 * TPBM_TS_LOCAL: Get current local datetime (timezone is local system timezone)
 *
 * @param mode One of TPBM_TS_UTC or TPBM_TS_LOCAL
 * @param dt Pointer to output datetime structure (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_get_datetime(uint32_t mode, tpb_datetime_t *dt);

/**
 * @brief Get boot-time timestamp (seconds and nanoseconds since system boot).
 *
 * Uses CLOCK_BOOTTIME for seconds and nanoseconds.
 *
 * @param btime Pointer to output boot-time structure (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_get_btime(tpb_btime_t *btime);

/**
 * @brief Convert boot-time seconds to calendar datetime.
 *
 * This is a rough conversion using approximate values:
 * - 24 hours per day
 * - 730 hours per month (365 days / 12 months * 24 hours)
 * - 8760 hours per year (365 days * 24 hours)
 *
 * Note: This conversion assumes a starting point and provides approximate
 * calendar dates for systems without wall-clock time. For precise datetime,
 * use tpb_ts_get_datetime() with TPBM_TS_UTC.
 *
 * @param btime Pointer to boot-time structure (must not be NULL)
 * @param dt Pointer to output datetime structure (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_btime_to_datetime(const tpb_btime_t *btime, tpb_datetime_t *dt);

/**
 * @brief Convert datetime to 64-bit representation.
 *
 * @param dt Pointer to datetime structure (must not be NULL)
 * @param tz_bias_min Timezone bias in minutes (e.g., +480 for UTC+8, -300 for UTC-5)
 * @param bits Pointer to output 64-bit value (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_datetime_to_bits(const tpb_datetime_t *dt, int16_t tz_bias_min,
                               tpb_dtbits_t *bits);

/**
 * @brief Convert 64-bit representation to datetime.
 *
 * @param bits 64-bit encoded datetime value
 * @param dt Pointer to output datetime structure (must not be NULL)
 * @param tz_bias_min Pointer to output timezone bias in minutes (can be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_bits_to_datetime(tpb_dtbits_t bits, tpb_datetime_t *dt,
                             int16_t *tz_bias_min);

/**
 * @brief Convert 64-bit to ISO 8601 UTC string (YYYY-MM-DDThh:mm:ssZ).
 *
 * @param bits 64-bit encoded datetime value
 * @param str Pointer to output string structure (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_bits_to_isoutc(tpb_dtbits_t bits, tpb_datetime_str_t *str);

/**
 * @brief Convert 64-bit to ISO 8601 with timezone (YYYY-MM-DDThh:mm:ss+HH:MM).
 *
 * @param bits 64-bit encoded datetime value
 * @param tz_bias_min Timezone bias in minutes
 * @param str Pointer to output string structure (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_bits_to_isotz(tpb_dtbits_t bits, int16_t tz_bias_min,
                          tpb_datetime_str_t *str);

/**
 * @brief Parse ISO 8601 UTC string to 64-bit representation.
 *
 * Expects format: YYYY-MM-DDThh:mm:ssZ
 *
 * @param str Pointer to input string structure (must not be NULL)
 * @param bits Pointer to output 64-bit value (must not be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_isoutc_to_bits(const tpb_datetime_str_t *str, tpb_dtbits_t *bits);

/**
 * @brief Parse ISO 8601 string with timezone to 64-bit representation.
 *
 * Expects format: YYYY-MM-DDThh:mm:ss+HH:MM or YYYY-MM-DDThh:mm:ss-HH:MM
 *
 * @param str Pointer to input string structure (must not be NULL)
 * @param bits Pointer to output 64-bit value (must not be NULL)
 * @param tz_bias_min Pointer to output timezone bias in minutes (can be NULL)
 * @return 0 on success, error code otherwise
 */
int tpb_ts_isotz_to_bits(const tpb_datetime_str_t *str, tpb_dtbits_t *bits,
                          int16_t *tz_bias_min);

#endif /* STRFTIME_H */
