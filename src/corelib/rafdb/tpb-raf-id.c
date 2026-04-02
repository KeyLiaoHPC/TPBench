/*
 * tpb-raf-id.c
 * SHA1-based ID generation for tbatch, kernel, and task records.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../tpb-types.h"
#include "tpb-raf-types.h"
#include "tpb-sha1.h"

/* Local Function Prototypes */
/* Map one hex ASCII character to its numeric value, or -1 if invalid. */
static int _sf_hex_nibble(int c);

/**
 * @brief Generate TBatchID via SHA1.
 */
int
tpb_raf_gen_tbatch_id(tpb_dtbits_t utc_bits,
                        uint64_t btime,
                        const char *hostname,
                        const char *username,
                        uint32_t front_pid,
                        unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;
    char numbuf[32];
    int len;

    if (!hostname || !username || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "tbatch", 6);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)utc_bits);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)btime);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_update(&ctx, hostname, strlen(hostname));
    tpb_sha1_update(&ctx, username, strlen(username));

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)front_pid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

/**
 * @brief Generate KernelID via SHA1.
 */
int
tpb_raf_gen_kernel_id(const char *kernel_name,
                        const unsigned char so_sha1[20],
                        const unsigned char bin_sha1[20],
                        unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;

    if (!kernel_name || !so_sha1 || !bin_sha1 || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "kernel", 6);
    tpb_sha1_update(&ctx, kernel_name, strlen(kernel_name));
    tpb_sha1_update(&ctx, so_sha1, 20);
    tpb_sha1_update(&ctx, bin_sha1, 20);
    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

/**
 * @brief Generate TaskRecordID via SHA1.
 */
int
tpb_raf_gen_task_id(tpb_dtbits_t utc_bits,
                      uint64_t btime,
                      const char *hostname,
                      const char *username,
                      const unsigned char tbatch_id[20],
                      const unsigned char kernel_id[20],
                      uint32_t hdl_id,
                      uint32_t pid,
                      uint32_t tid,
                      unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;
    char numbuf[32];
    int len;

    if (!hostname || !username || !tbatch_id ||
        !kernel_id || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "task", 4);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)utc_bits);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)btime);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_update(&ctx, hostname, strlen(hostname));
    tpb_sha1_update(&ctx, username, strlen(username));
    tpb_sha1_update(&ctx, tbatch_id, 20);
    tpb_sha1_update(&ctx, kernel_id, 20);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)hdl_id);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)pid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)tid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

/**
 * @brief Generate TaskCapsuleRecordID (same inputs as task ID, different
 *        leading hash literal).
 */
int
tpb_raf_gen_taskcapsule_id(tpb_dtbits_t utc_bits,
                             uint64_t btime,
                             const char *hostname,
                             const char *username,
                             const unsigned char tbatch_id[20],
                             const unsigned char kernel_id[20],
                             uint32_t hdl_id,
                             uint32_t pid,
                             uint32_t tid,
                             unsigned char id_out[20])
{
    tpb_sha1_ctx_t ctx;
    char numbuf[32];
    int len;

    if (!hostname || !username || !tbatch_id ||
        !kernel_id || !id_out) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, "taskcapsule", 12);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)utc_bits);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%lu",
                   (unsigned long)btime);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_update(&ctx, hostname, strlen(hostname));
    tpb_sha1_update(&ctx, username, strlen(username));
    tpb_sha1_update(&ctx, tbatch_id, 20);
    tpb_sha1_update(&ctx, kernel_id, 20);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)hdl_id);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)pid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    len = snprintf(numbuf, sizeof(numbuf), "%u",
                   (unsigned)tid);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
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

static int
_sf_hex_nibble(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
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
