/**
 * @file rafdb-l1-domain-reg.h
 * @brief L1 domain registry (cross-domain catalog).
 */

#ifndef RAFDB_L1_DOMAIN_REG_H
#define RAFDB_L1_DOMAIN_REG_H

#include <stddef.h>
#include <stdint.h>

typedef struct tpb_raf_domain_desc {
    uint8_t     domain_id;
    const char *subdir;
    const char *entry_filename;
    size_t      entry_row_size;
    size_t      record_fixed_meta_size;
} tpb_raf_domain_desc_t;

#define TPB_RAF_L1_DOMAIN_COUNT  3

const tpb_raf_domain_desc_t *_tpb_raf_l1_domain_desc(uint8_t domain);
const tpb_raf_domain_desc_t *_tpb_raf_l1_domain_by_index(int idx);
int _tpb_raf_l1_domain_count(void);

#endif /* RAFDB_L1_DOMAIN_REG_H */
