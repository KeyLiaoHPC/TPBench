/*
 * tpb-raf-magic.c
 * Magic signature construction, validation, and file detection.
 */

#include <stdio.h>
#include <string.h>
#include "../tpb-types.h"
#include "tpb-raf-types.h"

/* Local Function Prototypes */
/*
 * Return nonzero if the 4-byte prefix matches the TPB magic (E1 54 50 42).
 */
static int _sf_match_magic_prefix(const unsigned char *p);

static int
_sf_match_magic_prefix(const unsigned char *p)
{
    return p[0] == TPB_RAF_MAGIC_B0
        && p[1] == TPB_RAF_MAGIC_B1
        && p[2] == TPB_RAF_MAGIC_B2
        && p[3] == TPB_RAF_MAGIC_B3;
}

/**
 * @brief Build an 8-byte magic signature.
 */
void
tpb_raf_build_magic(uint8_t ftype, uint8_t domain,
                      uint8_t pos, unsigned char out[8])
{
    out[0] = TPB_RAF_MAGIC_B0;
    out[1] = TPB_RAF_MAGIC_B1;
    out[2] = TPB_RAF_MAGIC_B2;
    out[3] = TPB_RAF_MAGIC_B3;
    out[4] = (unsigned char)(ftype | domain);
    out[5] = pos;
    out[6] = TPB_RAF_MAGIC_B6;
    out[7] = TPB_RAF_MAGIC_B7;
}

/**
 * @brief Validate an 8-byte magic signature.
 */
int
tpb_raf_validate_magic(const unsigned char magic[8],
                         uint8_t ftype, uint8_t domain,
                         uint8_t pos)
{
    unsigned char expected[8];
    tpb_raf_build_magic(ftype, domain, pos, expected);
    return (memcmp(magic, expected, 8) == 0) ? 1 : 0;
}

/**
 * @brief Detect rafdb file type and domain from the first 8-byte magic.
 */
int
tpb_raf_detect_file(const char *filepath,
                      uint8_t *ftype_out,
                      uint8_t *domain_out)
{
    FILE *fp;
    unsigned char magic[TPB_RAF_MAGIC_LEN];
    uint8_t hi, lo;

    if (!filepath || !ftype_out || !domain_out) {
        return TPBE_NULLPTR_ARG;
    }

    fp = fopen(filepath, "rb");
    if (!fp) {
        return TPBE_FILE_IO_FAIL;
    }
    if (fread(magic, 1, TPB_RAF_MAGIC_LEN, fp) != TPB_RAF_MAGIC_LEN) {
        fclose(fp);
        return TPBE_FILE_IO_FAIL;
    }
    fclose(fp);

    if (!_sf_match_magic_prefix(magic)) {
        return TPBE_FILE_IO_FAIL;
    }
    if (magic[6] != TPB_RAF_MAGIC_B6 ||
        magic[7] != TPB_RAF_MAGIC_B7) {
        return TPBE_FILE_IO_FAIL;
    }
    if (magic[5] != TPB_RAF_POS_START) {
        return TPBE_FILE_IO_FAIL;
    }

    hi = (uint8_t)(magic[4] & 0xF0u);
    lo = (uint8_t)(magic[4] & 0x0Fu);

    if (hi != TPB_RAF_FTYPE_ENTRY && hi != TPB_RAF_FTYPE_RECORD) {
        return TPBE_FILE_IO_FAIL;
    }
    if (lo != TPB_RAF_DOM_TBATCH &&
        lo != TPB_RAF_DOM_KERNEL &&
        lo != TPB_RAF_DOM_TASK) {
        return TPBE_FILE_IO_FAIL;
    }

    *ftype_out = hi;
    *domain_out = lo;
    return TPBE_SUCCESS;
}
