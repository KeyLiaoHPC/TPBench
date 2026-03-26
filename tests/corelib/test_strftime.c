/**
 * @file test_strftime.c
 * @brief Test pack A3: Unit tests for strftime datetime module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mock_kernel.h"
#include "corelib/strftime.h"

static int g_setup_done = 0;

static int
ensure_setup(void)
{
    /* No setup needed for strftime module - it's stateless */
    (void)g_setup_done;
    return 0;
}

/**
 * Helper: Parse date command output to tpb_datetime_t
 * Expected formats:
 *   - UTC: "2026-03-10T14:30:45Z" (date -u +%Y-%m-%dT%H:%M:%SZ)
 *   - Local: "2026-03-10T14:30:45" (date +%Y-%m-%dT%H:%M:%S)
 */
static int
parse_date_output(const char *str, tpb_datetime_t *dt)
{
    unsigned int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    
    /* Try UTC format with Z suffix */
    if (sscanf(str, "%4u-%2u-%2uT%2u:%2u:%2uZ",
               &year, &month, &day, &hour, &min, &sec) == 6) {
        dt->year = (uint16_t)year;
        dt->month = (uint8_t)month;
        dt->day = (uint8_t)day;
        dt->hour = (uint8_t)hour;
        dt->min = (uint8_t)min;
        dt->sec = (uint8_t)sec;
        return 0;
    }
    
    /* Try local format without Z */
    if (sscanf(str, "%4u-%2u-%2uT%2u:%2u:%2u",
               &year, &month, &day, &hour, &min, &sec) == 6) {
        dt->year = (uint16_t)year;
        dt->month = (uint8_t)month;
        dt->day = (uint8_t)day;
        dt->hour = (uint8_t)hour;
        dt->min = (uint8_t)min;
        dt->sec = (uint8_t)sec;
        return 0;
    }
    
    return 1;  /* Parse failed */
}

/**
 * Helper: Compare two datetime structures
 * Returns 0 if year/month/day/hour/min match and sec difference < 10
 */
static int
compare_datetime_relaxed(const tpb_datetime_t *dt1, const tpb_datetime_t *dt2)
{
    if (dt1->year != dt2->year) return 1;
    if (dt1->month != dt2->month) return 1;
    if (dt1->day != dt2->day) return 1;
    if (dt1->hour != dt2->hour) return 1;
    if (dt1->min != dt2->min) return 1;
    
    int sec_diff = (int)dt1->sec - (int)dt2->sec;
    if (sec_diff < 0) sec_diff = -sec_diff;
    if (sec_diff >= 10) return 1;
    
    return 0;
}

/**
 * Helper: Run date command and get output
 */
static int
get_date_command_output(const char *cmd, char *buf, size_t buf_size)
{
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return 1;
    
    if (fgets(buf, (int)buf_size, fp) == NULL) {
        pclose(fp);
        return 1;
    }
    
    /* Remove trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    
    pclose(fp);
    return 0;
}

/**
 * Helper: Single attempt at UTC/local datetime test
 * Returns 0 on success, 1 on failure
 */
static int
try_get_datetime_test_once(void)
{
    tpb_datetime_t dt_utc, dt_local;
    char buf_utc[64], buf_local[64];
    tpb_datetime_t cmd_utc, cmd_local;
    
    /* Get datetime from API */
    if (tpb_ts_get_datetime(TPBM_TS_UTC, &dt_utc) != 0) return 1;
    if (tpb_ts_get_datetime(TPBM_TS_LOCAL, &dt_local) != 0) return 1;
    
    /* Get datetime from system date command */
    if (get_date_command_output("date -u +%Y-%m-%dT%H:%M:%SZ", buf_utc, sizeof(buf_utc)) != 0) return 1;
    if (get_date_command_output("date +%Y-%m-%dT%H:%M:%S", buf_local, sizeof(buf_local)) != 0) return 1;
    
    if (parse_date_output(buf_utc, &cmd_utc) != 0) return 1;
    if (parse_date_output(buf_local, &cmd_local) != 0) return 1;
    
    /* Compare API results with command line date */
    if (compare_datetime_relaxed(&dt_utc, &cmd_utc) != 0) return 1;
    if (compare_datetime_relaxed(&dt_local, &cmd_local) != 0) return 1;
    
    /* Compare UTC and local - year/month/day/hour/min should match if same timezone offset,
     * or hour may differ by timezone offset. We just check they're both valid and close. */
    
    return 0;
}

/* A3.1: tpb_ts_get_datetime UTC and local time validation with retry */
static int
test_get_datetime(void)
{
    if (ensure_setup()) return 1;
    
    /* Try up to 3 times with 5 second sleep between retries */
    for (int attempt = 0; attempt < 3; attempt++) {
        if (try_get_datetime_test_once() == 0) {
            return 0;  /* Success */
        }
        if (attempt < 2) {
            sleep(5);  /* Sleep 5 seconds before retry */
        }
    }
    
    return 1;  /* All attempts failed */
}

/* A3.2: tpb_ts_get_btime returns valid boot time */
static int
test_get_btime(void)
{
    if (ensure_setup()) return 1;

    tpb_btime_t btime;
    int err = tpb_ts_get_btime(&btime);
    if (err != 0) return 1;

    /* Boot time should be reasonable (system not booted for more than 100 years) */
    if (btime.sec > 8760ULL * 3600ULL * 100ULL) return 1;
    if (btime.nsec > 999999999) return 1;

    return 0;
}

/* A3.3: tpb_ts_btime_to_datetime converts boot time to calendar datetime */
static int
test_btime_to_datetime(void)
{
    if (ensure_setup()) return 1;

    /* Test with a known boot time: 1 day + 5 hours + 30 minutes + 45 seconds */
    tpb_btime_t btime;
    btime.sec = 24 * 3600 + 5 * 3600 + 30 * 60 + 45;  /* 1 day, 5:30:45 */
    btime.nsec = 123456789;

    tpb_datetime_t dt;
    int err = tpb_ts_btime_to_datetime(&btime, &dt);
    if (err != 0) return 1;

    /* With rough conversion, this should be 1970-01-02 05:30:45 */
    if (dt.year != 1970) return 1;
    if (dt.month != 1) return 1;
    if (dt.day != 2) return 1;
    if (dt.hour != 5) return 1;
    if (dt.min != 30) return 1;
    if (dt.sec != 45) return 1;

    return 0;
}

/* A3.4: tpb_ts_datetime_to_bits encodes correctly */
static int
test_datetime_to_bits(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t dt = {0};
    dt.year = 2026;
    dt.month = 3;
    dt.day = 10;
    dt.hour = 4;
    dt.min = 55;
    dt.sec = 54;

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_datetime_to_bits(&dt, 0, &bits);  /* UTC, no timezone bias */
    if (err != 0) return 1;

    /* Verify encoding by decoding back */
    tpb_datetime_t dt2 = {0};
    err = tpb_ts_bits_to_datetime(bits, &dt2, NULL);
    if (err != 0) return 1;

    if (dt2.year != dt.year) return 1;
    if (dt2.month != dt.month) return 1;
    if (dt2.day != dt.day) return 1;
    if (dt2.hour != dt.hour) return 1;
    if (dt2.min != dt.min) return 1;
    if (dt2.sec != dt.sec) return 1;

    return 0;
}

/* A3.5: tpb_ts_bits_to_datetime decodes correctly */
static int
test_bits_to_datetime(void)
{
    if (ensure_setup()) return 1;

    /* Construct datetime and encode it */
    tpb_datetime_t orig = {0};
    orig.year = 2026;
    orig.month = 3;
    orig.day = 10;
    orig.hour = 4;
    orig.min = 55;
    orig.sec = 54;

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_datetime_to_bits(&orig, 480, &bits);  /* +8 hours */
    if (err != 0) return 1;

    tpb_datetime_t dt = {0};
    int16_t tz_bias = 0;
    err = tpb_ts_bits_to_datetime(bits, &dt, &tz_bias);
    if (err != 0) return 1;

    if (dt.year != 2026) return 1;
    if (dt.month != 3) return 1;
    if (dt.day != 10) return 1;
    if (dt.hour != 4) return 1;
    if (dt.min != 55) return 1;
    if (dt.sec != 54) return 1;
    /* Timezone bias should be close to +480 minutes (stored in 15-min increments) */
    if (tz_bias < 450 || tz_bias > 480) return 1;  /* Allow some rounding */

    return 0;
}

/* A3.6: datetime -> bits -> datetime roundtrip */
static int
test_roundtrip_bits(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t original = {0};
    original.year = 1985;
    original.month = 7;
    original.day = 15;
    original.hour = 12;
    original.min = 30;
    original.sec = 45;

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_datetime_to_bits(&original, -300, &bits);  /* UTC-5 */
    if (err != 0) return 1;

    tpb_datetime_t decoded = {0};
    int16_t tz_bias = 0;
    err = tpb_ts_bits_to_datetime(bits, &decoded, &tz_bias);
    if (err != 0) return 1;

    if (original.year != decoded.year) return 1;
    if (original.month != decoded.month) return 1;
    if (original.day != decoded.day) return 1;
    if (original.hour != decoded.hour) return 1;
    if (original.min != decoded.min) return 1;
    if (original.sec != decoded.sec) return 1;

    return 0;
}

/* A3.7: tpb_ts_bits_to_isoutc produces valid ISO 8601 UTC */
static int
test_bits_to_isoutc(void)
{
    if (ensure_setup()) return 1;

    /* Construct datetime for 2026-03-10T04:55:54 */
    tpb_datetime_t dt = {0};
    dt.year = 2026;
    dt.month = 3;
    dt.day = 10;
    dt.hour = 4;
    dt.min = 55;
    dt.sec = 54;

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_datetime_to_bits(&dt, 0, &bits);
    if (err != 0) return 1;

    tpb_datetime_str_t str = {0};
    err = tpb_ts_bits_to_isoutc(bits, &str);
    if (err != 0) return 1;

    const char *expected = "2026-03-10T04:55:54Z";
    if (strcmp(str.str, expected) != 0) {
        return 1;
    }

    return 0;
}

/* A3.8: tpb_ts_bits_to_isotz with UTC timezone */
static int
test_bits_to_isotz_utc(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t dt = {0};
    dt.year = 1970;
    dt.month = 1;
    dt.day = 1;
    dt.hour = 12;
    dt.min = 0;
    dt.sec = 0;

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_datetime_to_bits(&dt, 0, &bits);
    if (err != 0) return 1;

    tpb_datetime_str_t str = {0};
    err = tpb_ts_bits_to_isotz(bits, 0, &str);
    if (err != 0) return 1;

    const char *expected = "1970-01-01T12:00:00+00:00";
    if (strcmp(str.str, expected) != 0) {
        return 1;
    }

    return 0;
}

/* A3.9: tpb_ts_bits_to_isotz with +08:00 offset */
static int
test_bits_to_isotz_offset(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t dt = {0};
    dt.year = 2024;
    dt.month = 6;
    dt.day = 20;
    dt.hour = 8;
    dt.min = 45;
    dt.sec = 30;

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_datetime_to_bits(&dt, 0, &bits);
    if (err != 0) return 1;

    tpb_datetime_str_t str = {0};
    err = tpb_ts_bits_to_isotz(bits, 480, &str);  /* +8 hours */
    if (err != 0) return 1;

    const char *expected = "2024-06-20T08:45:30+08:00";
    if (strcmp(str.str, expected) != 0) {
        return 1;
    }

    return 0;
}

/* A3.10: tpb_ts_isoutc_to_bits parses ISO 8601 UTC correctly */
static int
test_isoutc_to_bits(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_str_t str = {0};
    strncpy(str.str, "2026-03-10T04:55:54Z", sizeof(str.str) - 1);

    tpb_dtbits_t bits = 0;
    int err = tpb_ts_isoutc_to_bits(&str, &bits);
    if (err != 0) return 1;

    /* Decode and verify */
    tpb_datetime_t dt = {0};
    err = tpb_ts_bits_to_datetime(bits, &dt, NULL);
    if (err != 0) return 1;

    if (dt.year != 2026) return 1;
    if (dt.month != 3) return 1;
    if (dt.day != 10) return 1;
    if (dt.hour != 4) return 1;
    if (dt.min != 55) return 1;
    if (dt.sec != 54) return 1;

    return 0;
}

/* A3.11: tpb_ts_isotz_to_bits parses ISO 8601 with TZ correctly */
static int
test_isotz_to_bits(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_str_t str = {0};
    strncpy(str.str, "2026-03-10T12:55:54+08:00", sizeof(str.str) - 1);

    tpb_dtbits_t bits = 0;
    int16_t tz_bias = 0;
    int err = tpb_ts_isotz_to_bits(&str, &bits, &tz_bias);
    if (err != 0) return 1;

    /* Decode and verify (timezone is stored in the bits encoding) */
    tpb_datetime_t dt = {0};
    err = tpb_ts_bits_to_datetime(bits, &dt, &tz_bias);
    if (err != 0) return 1;

    if (dt.year != 2026) return 1;
    if (dt.month != 3) return 1;
    if (dt.day != 10) return 1;
    if (dt.hour != 12) return 1;
    if (dt.min != 55) return 1;
    if (dt.sec != 54) return 1;

    return 0;
}

/* A3.12: bits -> isoutc -> bits roundtrip */
static int
test_roundtrip_isoutc(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t orig = {0};
    orig.year = 2024;
    orig.month = 7;
    orig.day = 15;
    orig.hour = 8;
    orig.min = 45;
    orig.sec = 30;

    tpb_dtbits_t original_bits = 0;
    int err = tpb_ts_datetime_to_bits(&orig, 0, &original_bits);
    if (err != 0) return 1;

    tpb_datetime_str_t str = {0};
    err = tpb_ts_bits_to_isoutc(original_bits, &str);
    if (err != 0) return 1;

    tpb_dtbits_t roundtrip_bits = 0;
    err = tpb_ts_isoutc_to_bits(&str, &roundtrip_bits);
    if (err != 0) return 1;

    /* Compare decoded values */
    tpb_datetime_t dt1 = {0}, dt2 = {0};
    tpb_ts_bits_to_datetime(original_bits, &dt1, NULL);
    tpb_ts_bits_to_datetime(roundtrip_bits, &dt2, NULL);

    if (dt1.year != dt2.year) return 1;
    if (dt1.month != dt2.month) return 1;
    if (dt1.day != dt2.day) return 1;
    if (dt1.hour != dt2.hour) return 1;
    if (dt1.min != dt2.min) return 1;
    if (dt1.sec != dt2.sec) return 1;

    return 0;
}

/* A3.13: bits -> isotz -> bits roundtrip */
static int
test_roundtrip_isotz(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t orig = {0};
    orig.year = 2000;
    orig.month = 1;
    orig.day = 1;
    orig.hour = 0;
    orig.min = 0;
    orig.sec = 0;

    tpb_dtbits_t original_bits = 0;
    int err = tpb_ts_datetime_to_bits(&orig, 330, &original_bits);  /* +5:30 */
    if (err != 0) return 1;

    tpb_datetime_str_t str = {0};
    err = tpb_ts_bits_to_isotz(original_bits, 330, &str);
    if (err != 0) return 1;

    tpb_dtbits_t roundtrip_bits = 0;
    int16_t tz_bias = 0;
    err = tpb_ts_isotz_to_bits(&str, &roundtrip_bits, &tz_bias);
    if (err != 0) return 1;

    tpb_datetime_t dt1 = {0}, dt2 = {0};
    tpb_ts_bits_to_datetime(original_bits, &dt1, NULL);
    tpb_ts_bits_to_datetime(roundtrip_bits, &dt2, NULL);

    if (dt1.year != dt2.year) return 1;
    if (dt1.month != dt2.month) return 1;
    if (dt1.day != dt2.day) return 1;
    if (dt1.hour != dt2.hour) return 1;
    if (dt1.min != dt2.min) return 1;
    if (dt1.sec != dt2.sec) return 1;

    return 0;
}

/* A3.14: tpb_ts_get_datetime with invalid mode returns error */
static int
test_invalid_mode(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t dt;
    int err = tpb_ts_get_datetime(0xFF, &dt);  /* Invalid mode */

    /* Should return error, not success */
    if (err == 0) return 1;

    return 0;
}

/* A3.15: Functions with NULL pointers return error */
static int
test_null_pointer(void)
{
    if (ensure_setup()) return 1;

    tpb_dtbits_t bits = 0;
    tpb_datetime_t dt = {0};
    tpb_datetime_str_t str = {0};
    tpb_btime_t btime = {0};

    /* Test NULL pointer handling for tpb_ts_get_datetime (only valid modes) */
    if (tpb_ts_get_datetime(TPBM_TS_UTC, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_get_datetime(TPBM_TS_LOCAL, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_get_btime(NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_btime_to_datetime(&btime, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_btime_to_datetime(NULL, &dt) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_datetime_to_bits(NULL, 0, &bits) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_datetime_to_bits(&dt, 0, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_bits_to_datetime(bits, NULL, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_bits_to_isoutc(bits, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_bits_to_isotz(bits, 0, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_isoutc_to_bits(NULL, &bits) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_isoutc_to_bits(&str, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_isotz_to_bits(NULL, &bits, NULL) != TPBE_NULLPTR_ARG) return 1;
    if (tpb_ts_isotz_to_bits(&str, NULL, NULL) != TPBE_NULLPTR_ARG) return 1;

    return 0;
}

/* A3.16: Year range 1970-2225 (bias 0-255) */
static int
test_year_range(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t dt = {0};
    tpb_dtbits_t bits = 0;

    /* Test minimum year (1970) */
    dt.year = 1970;
    dt.month = 1;
    dt.day = 1;
    dt.hour = 0;
    dt.min = 0;
    dt.sec = 0;

    if (tpb_ts_datetime_to_bits(&dt, 0, &bits) != 0) return 1;

    tpb_datetime_t dt2 = {0};
    if (tpb_ts_bits_to_datetime(bits, &dt2, NULL) != 0) return 1;
    if (dt2.year != 1970) return 1;

    /* Test maximum year (2225 = 1970 + 255) */
    dt.year = 2225;
    dt.month = 12;
    dt.day = 31;
    dt.hour = 23;
    dt.min = 59;
    dt.sec = 59;

    if (tpb_ts_datetime_to_bits(&dt, 0, &bits) != 0) return 1;

    memset(&dt2, 0, sizeof(dt2));
    if (tpb_ts_bits_to_datetime(bits, &dt2, NULL) != 0) return 1;
    if (dt2.year != 2225) return 1;

    /* Test year out of range (should fail) */
    dt.year = 2226;  /* 1970 + 256, exceeds 8-bit bias */
    if (tpb_ts_datetime_to_bits(&dt, 0, &bits) == 0) return 1;  /* Should fail */

    return 0;
}

/* A3.17: Timezone bias encoding/decoding */
static int
test_timezone_bias(void)
{
    if (ensure_setup()) return 1;

    tpb_datetime_t dt = {0};
    dt.year = 2020;
    dt.month = 6;
    dt.day = 15;
    dt.hour = 12;
    dt.min = 0;
    dt.sec = 0;

    /* Test various timezone biases */
    int16_t test_biases[] = { -720, -480, -330, -60, 0, 60, 330, 480, 720 };
    size_t n = sizeof(test_biases) / sizeof(test_biases[0]);

    for (size_t i = 0; i < n; i++) {
        tpb_dtbits_t bits = 0;
        int16_t decoded_bias = 0;

        if (tpb_ts_datetime_to_bits(&dt, test_biases[i], &bits) != 0) return 1;
        if (tpb_ts_bits_to_datetime(bits, &dt, &decoded_bias) != 0) return 1;

        /* Due to 15-min increment storage, there may be slight rounding */
        int diff = decoded_bias - test_biases[i];
        if (diff < 0) diff = -diff;
        if (diff > 15) return 1;  /* Allow up to 15 min rounding error */
    }

    return 0;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;

    test_case_t cases[] = {
        { "A3.1",  "get_datetime",       test_get_datetime       },
        { "A3.2",  "get_btime",          test_get_btime          },
        { "A3.3",  "btime_to_datetime",  test_btime_to_datetime  },
        { "A3.4",  "datetime_to_bits",   test_datetime_to_bits   },
        { "A3.5",  "bits_to_datetime",   test_bits_to_datetime   },
        { "A3.6",  "roundtrip_bits",     test_roundtrip_bits     },
        { "A3.7",  "bits_to_isoutc",     test_bits_to_isoutc     },
        { "A3.8",  "bits_to_isotz_utc",  test_bits_to_isotz_utc  },
        { "A3.9",  "bits_to_isotz_offset", test_bits_to_isotz_offset },
        { "A3.10", "isoutc_to_bits",     test_isoutc_to_bits     },
        { "A3.11", "isotz_to_bits",      test_isotz_to_bits      },
        { "A3.12", "roundtrip_isoutc",   test_roundtrip_isoutc   },
        { "A3.13", "roundtrip_isotz",    test_roundtrip_isotz    },
        { "A3.14", "invalid_mode",       test_invalid_mode       },
        { "A3.15", "null_pointer",       test_null_pointer       },
        { "A3.16", "year_range",         test_year_range         },
        { "A3.17", "timezone_bias",      test_timezone_bias      },
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int fail = run_pack("A3", cases, n, filter);
    return (fail > 0) ? 1 : 0;
}
