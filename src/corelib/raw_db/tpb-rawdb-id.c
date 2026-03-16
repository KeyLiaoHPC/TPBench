/*
 * tpb-rawdb-id.c
 * SHA1-based ID generation for tbatch, kernel, and task records.
 */

#include <stdio.h>
#include <string.h>
#include "../tpb-types.h"
#include "tpb-rawdb-types.h"
#include "tpb-sha1.h"

int
tpb_rawdb_gen_tbatch_id(tpb_dtbits_t utc_bits,
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

int
tpb_rawdb_gen_kernel_id(const char *kernel_name,
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

int
tpb_rawdb_gen_task_id(tpb_dtbits_t utc_bits,
                      uint64_t btime,
                      const char *hostname,
                      const char *username,
                      const unsigned char tbatch_id[20],
                      const unsigned char kernel_id[20],
                      uint32_t order,
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
                   (unsigned)order);
    tpb_sha1_update(&ctx, numbuf, (size_t)len);

    tpb_sha1_final(&ctx, id_out);
    return TPBE_SUCCESS;
}

void
tpb_rawdb_id_to_hex(const unsigned char id[20], char hex[41])
{
    int i;
    for (i = 0; i < 20; i++) {
        snprintf(hex + i * 2, 3, "%02x", id[i]);
    }
    hex[40] = '\0';
}
