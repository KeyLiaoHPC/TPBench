/*
 * rafdb-l1-record-locate.c
 * L1 cross-domain .tpbr locate by record ID.
 */

#include <sys/stat.h>
#include "../../include/tpb-public.h"
#include "rafdb-l1-domain-reg.h"
#include "rafdb-l1-internal.h"
#include "../tpb-types.h"

/**
 * @brief Find which domain directory contains a .tpbr for the given ID.
 */
int
tpb_raf_find_record(const char *workspace, const unsigned char id[20],
                    uint8_t *domain_out)
{
    char path[TPB_RAF_PATH_MAX];
    struct stat st;
    int i;
    int nd;

    if (!workspace || !id || !domain_out) {
        TPB_FAIL(TPB_MOD_RAF_L1, TPBE_NULLPTR_ARG, NULL);
    }

    nd = _tpb_raf_l1_domain_count();
    for (i = 0; i < nd; i++) {
        const tpb_raf_domain_desc_t *desc = _tpb_raf_l1_domain_by_index(i);

        if (desc == NULL) {
            continue;
        }
        _tpb_raf_l1_build_record_path(workspace, desc->domain_id, id,
                                      path, sizeof(path));
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            *domain_out = desc->domain_id;
            return TPBE_SUCCESS;
        }
    }
    TPB_FAIL(TPB_MOD_RAF_L1, TPBE_FILE_IO_FAIL, NULL);
}
