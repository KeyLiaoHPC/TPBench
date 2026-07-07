/**
 * @file rafdb-l1-domain-reg.h
 * @brief L1 domain registry (cross-domain catalog).
 */

#ifndef RAFDB_L1_DOMAIN_REG_H
#define RAFDB_L1_DOMAIN_REG_H

#include <stddef.h>
#include <stdint.h>

/** @brief Record file naming style for a rafdb domain */
typedef enum tpb_raf_record_id_style {
    TPB_RAF_IDSTYLE_SHA1_HEX = 0,  /**< 40-hex-char .tpbr filename */
    TPB_RAF_IDSTYLE_DEC_INT32 = 1  /**< Zero-padded decimal .tpbr filename */
} tpb_raf_record_id_style_t;

typedef struct tpb_raf_domain_desc {
    uint8_t     domain_id;
    const char *subdir;
    const char *entry_filename;
    size_t      entry_row_size;
    size_t      record_fixed_meta_size;
    tpb_raf_record_id_style_t record_id_style;
} tpb_raf_domain_desc_t;

#define TPB_RAF_L1_DOMAIN_COUNT  4

const tpb_raf_domain_desc_t *_tpb_raf_l1_domain_desc(uint8_t domain);
const tpb_raf_domain_desc_t *_tpb_raf_l1_domain_by_index(int idx);
int _tpb_raf_l1_domain_count(void);

#endif /* RAFDB_L1_DOMAIN_REG_H */
