/**
 * @file tpbcli-kernel-registry.h
 * @brief Parse kernel_list.cmake.in and scan kernel source entry files.
 */

#ifndef TPBCLI_KERNEL_REGISTRY_H
#define TPBCLI_KERNEL_REGISTRY_H

#include <stddef.h>

#define TPBCLI_KERNEL_REG_NAME_MAX   64
#define TPBCLI_KERNEL_REG_TAGS_MAX   256
#define TPBCLI_KERNEL_REG_PATH_MAX   256
/* Maximum length of an EXTRA_LINK_LIBS field from kernel_link_defs.txt */
#define TPBCLI_KERNEL_REG_LINK_MAX   128
#define TPBCLI_KERNEL_REG_MAX        128

/** @brief One kernel row from registry or source scan */
typedef struct tpbcli_kernel_reg_entry {
    char name[TPBCLI_KERNEL_REG_NAME_MAX];
    char tags[TPBCLI_KERNEL_REG_TAGS_MAX];
    char path[TPBCLI_KERNEL_REG_PATH_MAX];
    int from_registry;
} tpbcli_kernel_reg_entry_t;

/** @brief Loaded kernel catalog */
typedef struct tpbcli_kernel_reg_list {
    tpbcli_kernel_reg_entry_t entries[TPBCLI_KERNEL_REG_MAX];
    int count;
} tpbcli_kernel_reg_list_t;

/**
 * @brief Load registry and merge scanned source entries.
 * @param tpb_home Resolved TPB_HOME path.
 * @param out Output list (cleared on entry).
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel_reg_load(const char *tpb_home,
                           tpbcli_kernel_reg_list_t *out);

/**
 * @brief Find entry by kernel name.
 * @param list Loaded catalog.
 * @param name Kernel name.
 * @return Pointer to entry or NULL.
 */
const tpbcli_kernel_reg_entry_t *
tpbcli_kernel_reg_find(const tpbcli_kernel_reg_list_t *list,
                       const char *name);

/**
 * @brief Collect unique kernel names matching any requested tag.
 * @param list Loaded catalog.
 * @param tag_csv Comma-separated tag list.
 * @param names_out Array of name pointers into list entries.
 * @param names_max Capacity of names_out.
 * @return Number of names written, or negative error code.
 */
int tpbcli_kernel_reg_expand_tags(const tpbcli_kernel_reg_list_t *list,
                                  const char *tag_csv,
                                  const char **names_out,
                                  int names_max);

/**
 * @brief Collect all unique tags from the catalog.
 * @param list Loaded catalog.
 * @param tags_out Buffer for comma-separated tag list.
 * @param tags_len Size of tags_out.
 */
void tpbcli_kernel_reg_all_tags(const tpbcli_kernel_reg_list_t *list,
                                char *tags_out, size_t tags_len);

/**
 * @brief Strip optional outer quotes and split comma-separated tokens.
 * @param value Input list string.
 * @param tokens_out Output token pointers into value buffer copy.
 * @param tokens_max Max tokens.
 * @param scratch Scratch buffer holding mutable copy.
 * @param scratch_len Size of scratch.
 * @return Token count, or negative error code.
 */
int tpbcli_kernel_reg_split_csv(const char *value,
                                char **tokens_out,
                                int tokens_max,
                                char *scratch,
                                size_t scratch_len);

/**
 * @brief Look up extra link libraries for a kernel from kernel_link_defs.txt.
 * @param tpb_home Resolved TPB_HOME path.
 * @param name Kernel name.
 * @param out Output buffer for link libs (e.g. MPI::MPI_C), or empty string.
 * @param outlen Size of out.
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel_reg_link_libs(const char *tpb_home,
                                const char *name,
                                char *out,
                                size_t outlen);

#endif /* TPBCLI_KERNEL_REGISTRY_H */
