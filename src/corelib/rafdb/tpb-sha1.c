/*
 * tpb-sha1.c
 * Pure-C SHA1 implementation (RFC 3174). No external dependencies.
 */

#include <string.h>
#include "tpb-sha1.h"

/* Local Function Prototypes */
static void _sf_process_block(tpb_sha1_ctx_t *ctx,
                            const unsigned char block[64]);
static uint32_t _sf_rotl32(uint32_t x, int n);

static uint32_t
_sf_rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

static void
_sf_process_block(tpb_sha1_ctx_t *ctx, const unsigned char block[64])
{
    uint32_t w[80];
    uint32_t a, b, c, d, e, f, k, tmp;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 80; i++) {
        w[i] = _sf_rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 80; i++) {
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        tmp = _sf_rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = _sf_rotl32(b, 30);
        b = a;
        a = tmp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

/**
 * @brief Initialize SHA1 context.
 */
void
tpb_sha1_init(tpb_sha1_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
    memset(ctx->buf, 0, 64);
}

/**
 * @brief Feed data into SHA1 context.
 */
void
tpb_sha1_update(tpb_sha1_ctx_t *ctx, const void *data,
                size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t buf_used = (size_t)(ctx->count & 63);
    size_t space;

    ctx->count += len;

    if (buf_used > 0) {
        space = 64 - buf_used;
        if (len < space) {
            memcpy(ctx->buf + buf_used, p, len);
            return;
        }
        memcpy(ctx->buf + buf_used, p, space);
        _sf_process_block(ctx, ctx->buf);
        p += space;
        len -= space;
    }

    while (len >= 64) {
        _sf_process_block(ctx, p);
        p += 64;
        len -= 64;
    }

    if (len > 0) {
        memcpy(ctx->buf, p, len);
    }
}

/**
 * @brief Finalize SHA1 and produce 20-byte digest.
 */
void
tpb_sha1_final(tpb_sha1_ctx_t *ctx, unsigned char digest[20])
{
    uint64_t total_bits = ctx->count * 8;
    size_t buf_used = (size_t)(ctx->count & 63);
    int i;

    ctx->buf[buf_used++] = 0x80;

    if (buf_used > 56) {
        memset(ctx->buf + buf_used, 0, 64 - buf_used);
        _sf_process_block(ctx, ctx->buf);
        buf_used = 0;
    }
    memset(ctx->buf + buf_used, 0, 56 - buf_used);

    ctx->buf[56] = (unsigned char)(total_bits >> 56);
    ctx->buf[57] = (unsigned char)(total_bits >> 48);
    ctx->buf[58] = (unsigned char)(total_bits >> 40);
    ctx->buf[59] = (unsigned char)(total_bits >> 32);
    ctx->buf[60] = (unsigned char)(total_bits >> 24);
    ctx->buf[61] = (unsigned char)(total_bits >> 16);
    ctx->buf[62] = (unsigned char)(total_bits >> 8);
    ctx->buf[63] = (unsigned char)(total_bits);
    _sf_process_block(ctx, ctx->buf);

    for (i = 0; i < 5; i++) {
        digest[i * 4 + 0] = (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}

/**
 * @brief One-shot SHA1: hash data and produce 20-byte digest.
 */
void
tpb_sha1(const void *data, size_t len, unsigned char digest[20])
{
    tpb_sha1_ctx_t ctx;
    tpb_sha1_init(&ctx);
    tpb_sha1_update(&ctx, data, len);
    tpb_sha1_final(&ctx, digest);
}
