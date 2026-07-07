/*
 * rafdb-l1-domain-reg.c
 * L1 static domain registry for path building and workspace init.
 */

#include "rafdb-l1-domain-reg.h"
#include "rafdb-l1-types.h"

static const tpb_raf_domain_desc_t g_domains[TPB_RAF_L1_DOMAIN_COUNT] = {
    {
        TPB_RAF_DOM_TBATCH,
        TPB_RAF_TBATCH_DIR,
        TPB_RAF_TBATCH_ENTRY,
        264,
        236 + TPB_RAF_RESERVE_SIZE,
        TPB_RAF_IDSTYLE_SHA1_HEX,
    },
    {
        TPB_RAF_DOM_KERNEL,
        TPB_RAF_KERNEL_DIR,
        TPB_RAF_KERNEL_ENTRY,
        264,
        2448 + TPB_RAF_KERNEL_ATTR_RESERVE,
        TPB_RAF_IDSTYLE_SHA1_HEX,
    },
    {
        TPB_RAF_DOM_TASK,
        TPB_RAF_TASK_DIR,
        TPB_RAF_TASK_ENTRY,
        232,
        156 + TPB_RAF_RESERVE_SIZE,
        TPB_RAF_IDSTYLE_SHA1_HEX,
    },
    {
        TPB_RAF_DOM_RTENV,
        TPB_RAF_RTENV_DIR,
        TPB_RAF_RTENV_ENTRY,
        TPB_RAF_RTENV_ENTRY_SIZE,
        TPB_RAF_RTENV_RECORD_META_SIZE,
        TPB_RAF_IDSTYLE_DEC_INT32,
    },
};

const tpb_raf_domain_desc_t *
_tpb_raf_l1_domain_desc(uint8_t domain)
{
    int i;

    for (i = 0; i < TPB_RAF_L1_DOMAIN_COUNT; i++) {
        if (g_domains[i].domain_id == domain) {
            return &g_domains[i];
        }
    }
    return NULL;
}

const tpb_raf_domain_desc_t *
_tpb_raf_l1_domain_by_index(int idx)
{
    if (idx < 0 || idx >= TPB_RAF_L1_DOMAIN_COUNT) {
        return NULL;
    }
    return &g_domains[idx];
}

int
_tpb_raf_l1_domain_count(void)
{
    return TPB_RAF_L1_DOMAIN_COUNT;
}
