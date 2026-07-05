/**
 * @file rafdb-l1-sha1.h
 * @brief L1 SHA1 hash implementation for rafdb.
 */

#ifndef RAFDB_L1_SHA1_H
#define RAFDB_L1_SHA1_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[5];
    uint64_t count;
    unsigned char buf[64];
} tpb_sha1_ctx_t;

void tpb_sha1_init(tpb_sha1_ctx_t *ctx);
void tpb_sha1_update(tpb_sha1_ctx_t *ctx, const void *data, size_t len);
void tpb_sha1_final(tpb_sha1_ctx_t *ctx, unsigned char digest[20]);

#endif /* RAFDB_L1_SHA1_H */
