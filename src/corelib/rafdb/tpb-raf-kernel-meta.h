/**
 * @file tpb-raf-kernel-meta.h
 * @brief Kernel Domain metadata helpers (registered parms/metrics, variation/compilation/dependency).
 */

#ifndef TPB_RAF_KERNEL_META_H
#define TPB_RAF_KERNEL_META_H

#include "../../include/tpb-public.h"

/** Metadata header names in kernel .tpbr */
#define TPB_RAF_KERNEL_HDR_VARIATION    "variation"
#define TPB_RAF_KERNEL_HDR_COMPILATION  "compilation"
#define TPB_RAF_KERNEL_HDR_DEPENDENCY   "dependency"

/** Number of fixed metadata headers appended after parm/metric headers */
#define TPB_RAF_KERNEL_META_HDR_COUNT   3

/** Environment variable: allow overwriting an existing KernelID record */
#define TPB_K_OVERRIDE_ENV              "TPB_K_OVERRIDE"

/**
 * @brief Return nonzero if TPB_K_OVERRIDE is set to a truthy value.
 */
int tpb_raf_kernel_override_enabled(void);

/**
 * @brief Build kernel_attr_t headers and default-value data from a registered kernel.
 *
 * Sets nheader = nparm + nmetric + TPB_RAF_KERNEL_META_HDR_COUNT and initializes
 * variation/compilation/dependency string headers (empty kv except variation defaults).
 *
 * @param kernel Registered kernel (must not be NULL).
 * @param kernel_id 20-byte KernelID.
 * @param attr_out Output attributes; caller must free attr_out->headers.
 * @param data_out Output serialized parm default values (caller frees).
 * @param datasize_out Output data size.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_kernel_build_registered_attr(const tpb_kernel_t *kernel,
                                         const unsigned char kernel_id[20],
                                         kernel_attr_t *attr_out,
                                         void **data_out,
                                         uint64_t *datasize_out);

/**
 * @brief Free headers allocated by tpb_raf_kernel_build_registered_attr.
 */
void tpb_raf_kernel_free_built_attr(kernel_attr_t *attr, void *data);

/**
 * @brief Find a header index by exact name in a kernel attr.
 * @return Index >= 0, or -1 if not found.
 */
int tpb_raf_kernel_find_header(const kernel_attr_t *attr, const char *name);

/**
 * @brief Set or update key=value in a metadata string header payload.
 *
 * @param hdr Header with TPB_STRING_T payload (updated in place; may reallocate payload via caller buffer).
 * @param payload_buf Caller-owned buffer holding current payload (reallocated on grow).
 * @param payload_cap Capacity of payload_buf pointer target.
 * @param key Dotted key without section prefix (e.g. "compiler.id").
 * @param value Value string.
 * @return TPBE_SUCCESS or error code.
 */
int tpb_raf_kernel_meta_kv_set(char **payload_buf, size_t *payload_cap,
                               const char *key, const char *value);

/**
 * @brief Read a key from metadata kv payload; returns empty string if missing.
 */
int tpb_raf_kernel_meta_kv_get(const char *payload, const char *key,
                               char *value_out, size_t value_len);

/**
 * @brief Copy a kernel metadata string header payload by name.
 * @param attr Kernel attributes from tpb_raf_record_read_kernel().
 * @param data Record data buffer from read_kernel (may be NULL).
 * @param hdr_name Header name (e.g. TPB_RAF_KERNEL_HDR_VARIATION).
 * @param buf Output buffer for NUL-terminated payload.
 * @param bufsz Capacity of buf.
 * @return TPBE_SUCCESS, TPBE_LIST_NOT_FOUND, or TPBE_NULLPTR_ARG.
 */
int tpb_raf_kernel_get_header_payload(const kernel_attr_t *attr,
                                      const void *data,
                                      const char *hdr_name,
                                      char *buf, size_t bufsz);

/**
 * @brief Build a one-line variation summary from a metadata kv payload.
 *
 * Prefers the kernel.name key when present (formatted as kernel.name=<value>);
 * otherwise returns the first non-format/section line from the payload.
 *
 * @param payload Metadata kv payload string.
 * @param out Output buffer.
 * @param outlen Capacity of out.
 */
void tpb_raf_kernel_variation_summary(const char *payload, char *out,
                                      size_t outlen);

/**
 * @brief Patch active flag in kernel .tpbe entry for a KernelID.
 */
int tpb_raf_entry_patch_kernel_active(const char *workspace,
                                      const unsigned char kernel_id[20],
                                      uint32_t active);

/**
 * @brief Patch build utc_bits in kernel .tpbe entry for a KernelID.
 */
int tpb_raf_entry_patch_kernel_utc(const char *workspace,
                                   const unsigned char kernel_id[20],
                                   tpb_dtbits_t utc_bits);

/**
 * @brief Patch active flag in kernel .tpbr fixed attributes for a KernelID.
 */
int tpb_raf_record_patch_kernel_active(const char *workspace,
                                       const unsigned char kernel_id[20],
                                       uint32_t active);

/**
 * @brief Deactivate all kernel entries with the given logical name except skip_id.
 */
int tpb_raf_kernel_deactivate_same_name(const char *workspace,
                                        const char *kernel_name,
                                        const unsigned char skip_id[20]);

/**
 * @brief Update one metadata key on an existing kernel .tpbr record.
 *
 * @param full_key Dotted key with section prefix, e.g. "compilation.compiler.id".
 * @return TPBE_SUCCESS, TPBE_LIST_NOT_FOUND if record/header missing.
 */
int tpb_raf_kernel_update_meta_key(const char *workspace,
                                   const unsigned char kernel_id[20],
                                   const char *full_key,
                                   const char *value);

#endif /* TPB_RAF_KERNEL_META_H */
