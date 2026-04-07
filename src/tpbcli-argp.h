/**
 * @file tpbcli-argp.h
 * @brief Tree-based CLI argument parser for tpbcli (stack parse, heuristic help).
 */

#ifndef TPBCLI_ARGP_H
#define TPBCLI_ARGP_H

#include <stdio.h>
#include <stdint.h>

#include "include/tpb-public.h"

typedef struct tpbcli_argnode tpbcli_argnode_t;
typedef struct tpbcli_argtree tpbcli_argtree_t;

/**
 * @brief Node kind: subcommand, option (takes value), or flag (no value).
 */
typedef enum {
    TPBCLI_ARG_CMD,   /**< Subcommand: pushed on stack, no argv value */
    TPBCLI_ARG_OPT,   /**< Option: consumes next argv token as value */
    TPBCLI_ARG_FLAG   /**< Flag: no value token */
} tpbcli_arg_type_t;

/** @brief At most one exclusive sibling may be selected at this depth. */
#define TPBCLI_ARGF_EXCLUSIVE   (1u << 0)
/** @brief Parent requires this child to appear (or have PRESET). */
#define TPBCLI_ARGF_MANDATORY   (1u << 1)
/** @brief Node has default from @c preset_value. */
#define TPBCLI_ARGF_PRESET      (1u << 2)
/**
 * @brief CMD only: after this subcommand token is consumed, stop argv scanning;
 *        run @c parse_fn from post-loop (top-level dispatch to another parser).
 */
#define TPBCLI_ARGF_DELEGATE_SUBCMD (1u << 3)

/**
 * @brief Print help for a node (e.g. matched @c max_chosen==0 help FLAG).
 * @param node Matched node; must not modify the tree.
 * @param out Output stream (typically stderr).
 */
typedef void (*tpbcli_arg_help_fn)(const tpbcli_argnode_t *node, FILE *out);

/**
 * @brief Parse handler after a match.
 * @param node Matched node.
 * @param value Next argv token for OPT; NULL for FLAG.
 * @return 0 on success; non-zero makes @ref tpbcli_parse_args return @c TPBE_CLI_FAIL.
 */
typedef int (*tpbcli_arg_parse_fn)(tpbcli_argnode_t *node, const char *value);

/**
 * @brief Immutable configuration passed to @ref tpbcli_add_arg (read once; not stored).
 */
typedef struct tpbcli_argconf {
    const char             *name;
    const char             *short_name;
    const char             *desc;
    tpbcli_arg_type_t       type;
    uint32_t                flags;
    /** 0=help (if help_fn) or deprecated (if not); 1 once; N>1 N times; -1 unlimited. */
    int                     max_chosen;
    tpbcli_arg_parse_fn     parse_fn;
    tpbcli_arg_help_fn      help_fn;
    const char             *preset_value;
    /** NULL-terminated sibling names that conflict when already selected. */
    const char            **conflict_opts;
    void                   *user_data;
} tpbcli_argconf_t;

/**
 * @brief One node in the argument tree (module-allocated; strings caller-owned).
 */
struct tpbcli_argnode {
    const char             *name;
    const char             *short_name;
    const char             *desc;
    tpbcli_arg_type_t       type;
    uint32_t                flags;
    int                     depth;
    int                     max_chosen;

    struct tpbcli_argnode  *parent;
    struct tpbcli_argnode  *first_child;
    struct tpbcli_argnode  *next_sibling;

    const char            **conflict_opts;
    int                     conflict_count;

    tpbcli_arg_help_fn      help_fn;
    tpbcli_arg_parse_fn     parse_fn;

    const char             *preset_value;

    int                     is_set;
    int                     chosen_count;
    const char             *parsed_value;
    void                   *user_data;
};

/**
 * @brief Tree handle; root is embedded at @c root.
 */
struct tpbcli_argtree {
    struct tpbcli_argnode   root;
};

/**
 * @brief Allocate tree; root is CMD depth 0, @c max_chosen 1.
 * @param prog_name Shown in help; borrowed until destroy; must not be NULL.
 * @param prog_desc One-line summary; borrowed; may be NULL.
 * @return New tree or NULL on allocation failure.
 */
tpbcli_argtree_t *tpbcli_argtree_create(const char *prog_name,
                                        const char *prog_desc);

/**
 * @brief Free tree and all nodes from @ref tpbcli_add_arg; does not free string fields.
 * @param tree May be NULL (no-op).
 */
void tpbcli_argtree_destroy(tpbcli_argtree_t *tree);

/**
 * @brief Append a sealed child under @a parent. Rejects duplicate @a name.
 * @return New node or NULL on duplicate / bad args / OOM.
 */
tpbcli_argnode_t *tpbcli_add_arg(tpbcli_argnode_t *parent,
                                 const tpbcli_argconf_t *conf);

/**
 * @brief Parse @a argv[1..argc-1] using stack algorithm in plan §2.2.
 * @return @c TPBE_SUCCESS, @c TPBE_EXIT_ON_HELP, or @c TPBE_CLI_FAIL.
 */
int tpbcli_parse_args(tpbcli_argtree_t *tree, int argc, char **argv);

/**
 * @brief DFS search: first node at depth @c start->depth + offset matching @a name.
 * @param start Subtree root; must not be NULL.
 * @param name Primary or short name; must not be NULL.
 * @param offset Non-negative depth offset from @a start.
 * @param out Writes pointer or NULL; must not be NULL.
 * @return @c TPBE_SUCCESS or @c TPBE_LIST_NOT_FOUND. Asserts on invalid args.
 */
int tpbcli_find_arg(const tpbcli_argnode_t *start, const char *name, int offset,
                    tpbcli_argnode_t **out);

/**
 * @brief Default @c help_fn for @c -h / @c --help: prints default help for @c node->parent.
 * @param node The matched help FLAG (typically @c -h).
 * @param out Output stream.
 */
void tpbcli_default_help(const tpbcli_argnode_t *node, FILE *out);

#endif /* TPBCLI_ARGP_H */
