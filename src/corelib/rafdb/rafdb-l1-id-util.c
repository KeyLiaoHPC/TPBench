/*
 * rafdb-l1-id-util.c
 * L1 ID hex conversion utilities.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "../tpb-types.h"

/* Local Function Prototypes */
static int _sf_hex_nibble(int c);

static int
_sf_hex_nibble(int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/**
 * @brief Convert 20-byte ID to 40-char hex string.
 */
void
tpb_raf_id_to_hex(const unsigned char id[20], char hex[41])
{
    int i;

    for (i = 0; i < 20; i++) {
        snprintf(hex + i * 2, 3, "%02x", id[i]);
    }
    hex[40] = '\0';
}

/**
 * @brief Parse exactly 40 hex digits into a 20-byte ID.
 */
int
tpb_raf_hex_to_id(const char *hex, unsigned char id[20])
{
    size_t i;

    if (!hex || !id) {
        return TPBE_NULLPTR_ARG;
    }

    for (i = 0; i < 40; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return TPBE_CLI_FAIL;
        }
    }
    if (hex[40] != '\0') {
        return TPBE_CLI_FAIL;
    }

    for (i = 0; i < 20; i++) {
        int hi = _sf_hex_nibble((unsigned char)hex[i * 2]);
        int lo = _sf_hex_nibble((unsigned char)hex[i * 2 + 1]);

        if (hi < 0 || lo < 0) {
            return TPBE_CLI_FAIL;
        }
        id[i] = (unsigned char)((hi << 4) | lo);
    }
    return TPBE_SUCCESS;
}
