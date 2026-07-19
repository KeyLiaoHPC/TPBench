/**
 * @file tpb-raf-runtime-environment.h
 * @brief Private constants for runtime_environment rafdb domain.
 */

#ifndef TPB_RAF_RUNTIME_ENVIRONMENT_H
#define TPB_RAF_RUNTIME_ENVIRONMENT_H

#include "../../include/tpb-public.h"

/** @brief Packed on-disk .tpbe row for runtime_environment */
typedef struct rtenv_entry_disk {
    int32_t      id;
    char         name[TPB_RAF_RTENV_NAME_LEN];
    char         hostname[TPB_RAF_RTENV_HOST_LEN];
    tpb_dtbits_t utc_bits;
    int32_t      inherit_from;
    int32_t      derive_to;
    uint32_t     ntask;
    uint32_t     ntbatch;
    char         note[TPB_RAF_RTENV_NOTE_LEN];
    uint32_t     napp;
    uint32_t     nenv;
    unsigned char reserve[TPB_RAF_RTENV_RESERVE_LEN];
} __attribute__((packed)) rtenv_entry_disk_t;

/** @brief L3 derive link header name on parent .tpbr */
#define TPB_RAF_RTENV_HDR_DERIVE_TO  "DeriveTo"

/** @brief Application table header name */
#define TPB_RAF_RTENV_HDR_APPLICATION "application"

/** @brief File offset of derive_to in .tpbe/.tpbr fixed meta (bytes) */
#define TPB_RAF_RTENV_OFF_DERIVE  528
/** @brief File offset of ntask in .tpbe/.tpbr fixed meta (bytes) */
#define TPB_RAF_RTENV_OFF_NTASK   532
/** @brief File offset of ntbatch in .tpbe/.tpbr fixed meta (bytes) */
#define TPB_RAF_RTENV_OFF_NTBATCH 536

#endif /* TPB_RAF_RUNTIME_ENVIRONMENT_H */
