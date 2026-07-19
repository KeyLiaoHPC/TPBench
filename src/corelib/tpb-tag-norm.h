/**
 * @file tpb-tag-norm.h
 * @brief Tag string validation and normalization for meta headers.
 *
 * Stored form: comma-separated, no spaces, uppercase, deduped, sorted ascending.
 * Display form (elsewhere): join tokens with ", ".
 */

#ifndef TPB_TAG_NORM_H
#define TPB_TAG_NORM_H

#include <stddef.h>

/**
 * @brief Normalize a raw tag string into canonical storage form.
 *
 * Steps: split on ',', trim whitespace, uppercase each token, drop empties,
 * dedupe (case-insensitive before upper), sort with strcmp ascending, join
 * with ',' (no spaces). Empty/NULL raw yields empty dst.
 *
 * @param dst     Output buffer (NUL-terminated on success).
 * @param dst_sz  Size of dst (must be >= 1).
 * @param raw     Input tag list (may be NULL or empty).
 * @return 0 on success, -1 if overflow or a token contains ':'.
 */
int _sf_normalize_tags(char *dst, size_t dst_sz, const char *raw);

/**
 * @brief Validate a local header/argument/output name.
 *
 * Non-NULL, non-empty, length <= 255, no ':' character.
 *
 * @return 0 if valid, -1 otherwise.
 */
int _sf_validate_header_name(const char *name);

/**
 * @brief Validate user-supplied tag text before system append.
 *
 * NULL or "" allowed. Otherwise length <= TPBM_TAG_USER_MAX_LEN and no ':'.
 *
 * @return 0 if valid, -1 otherwise.
 */
int _sf_validate_user_tags(const char *tag);

/**
 * @brief Append a system role tag and normalize into dst.
 *
 * Concatenates user_tag with "," and sys_tag (when user_tag non-empty),
 * then runs _sf_normalize_tags.
 *
 * @param dst      Output buffer for canonical tags.
 * @param dst_sz   Size of dst.
 * @param user_tag User tags (may be NULL/empty).
 * @param sys_tag  System role tag (e.g. TPB_TAG_ARG / TPB_TAG_OUT); required.
 * @return 0 on success, -1 on validation/overflow failure.
 */
int _sf_finalize_role_tags(char *dst, size_t dst_sz,
                           const char *user_tag, const char *sys_tag);

/**
 * @brief Format stored tags for human display ("A, B, C").
 *
 * @param dst     Output buffer.
 * @param dst_sz  Size of dst.
 * @param stored  Canonical storage form (no spaces).
 * @return 0 on success, -1 on overflow.
 */
int _sf_format_tags_display(char *dst, size_t dst_sz, const char *stored);

#endif /* TPB_TAG_NORM_H */
