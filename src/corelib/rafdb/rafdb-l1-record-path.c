/*
 * rafdb-l1-record-path.c
 * L1 path building via domain registry.
 */

#include <stdio.h>
#include <string.h>
#include "rafdb-l1-domain-reg.h"
#include "rafdb-l1-internal.h"
#include "../../include/tpb-public.h"

void
_tpb_raf_l1_build_entry_path(const char *workspace, uint8_t domain,
                             char *out, size_t outlen)
{
    const tpb_raf_domain_desc_t *desc;

    desc = _tpb_raf_l1_domain_desc(domain);
    if (desc == NULL) {
        snprintf(out, outlen, "%s/rafdb/task_batch/task_batch.tpbe",
                 workspace);
        return;
    }
    snprintf(out, outlen, "%s/%s/%s",
             workspace, desc->subdir, desc->entry_filename);
}

void
_tpb_raf_l1_build_record_path_int32(const char *workspace, uint8_t domain,
                                    int32_t id, char *out, size_t outlen)
{
    const tpb_raf_domain_desc_t *desc;

    desc = _tpb_raf_l1_domain_desc(domain);
    if (desc == NULL) {
        snprintf(out, outlen, "%s/%s/%010d.tpbr",
                 workspace, TPB_RAF_RTENV_DIR, id);
        return;
    }
    snprintf(out, outlen, "%s/%s/%010d.tpbr",
             workspace, desc->subdir, id);
}

void
_tpb_raf_l1_build_record_path(const char *workspace, uint8_t domain,
                              const unsigned char id[20],
                              char *out, size_t outlen)
{
    char hex[41];
    const tpb_raf_domain_desc_t *desc;

    desc = _tpb_raf_l1_domain_desc(domain);
    if (desc != NULL &&
        desc->record_id_style == TPB_RAF_IDSTYLE_DEC_INT32) {
        int32_t rid;

        memcpy(&rid, id, sizeof(rid));
        _tpb_raf_l1_build_record_path_int32(workspace, domain, rid,
                                            out, outlen);
        return;
    }

    tpb_raf_id_to_hex(id, hex);
    if (desc == NULL) {
        snprintf(out, outlen, "%s/%s/%s.tpbr",
                 workspace, TPB_RAF_TBATCH_DIR, hex);
        return;
    }
    snprintf(out, outlen, "%s/%s/%s.tpbr",
             workspace, desc->subdir, hex);
}
