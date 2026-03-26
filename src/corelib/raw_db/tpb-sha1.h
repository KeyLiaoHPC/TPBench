/**
 * @file tpb-sha1.h
 * @brief Internal SHA1 hash implementation for TPBench rawdb.
 */

#ifndef TPB_SHA1_H
#define TPB_SHA1_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[5];   /**< Intermediate hash state */
    uint64_t count;      /**< Total bytes processed */
    unsigned char buf[64]; /**< Pending input block */
} tpb_sha1_ctx_t;

/**
 * @brief Initialize SHA1 context.
 * @param ctx SHA1 context (must not be NULL)
 */
void tpb_sha1_init(tpb_sha1_ctx_t *ctx);

/**
 * @brief Feed data into SHA1 context.
 * @param ctx SHA1 context
 * @param data Input data
 * @param len Length in bytes
 */
void tpb_sha1_update(tpb_sha1_ctx_t *ctx, const void *data,
                     size_t len);

/**
 * @brief Finalize SHA1 and produce 20-byte digest.
 * @param ctx SHA1 context (consumed; do not reuse)
 * @param digest Output buffer, at least 20 bytes
 */
void tpb_sha1_final(tpb_sha1_ctx_t *ctx,
                    unsigned char digest[20]);

/**
 * @brief One-shot SHA1: hash data and produce 20-byte digest.
 * @param data Input data
 * @param len Length in bytes
 * @param digest Output buffer, at least 20 bytes
 */
void tpb_sha1(const void *data, size_t len,
              unsigned char digest[20]);

#endif /* TPB_SHA1_H */
