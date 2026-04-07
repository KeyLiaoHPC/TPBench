/*
 * tpbcli-argp.c
 * Tree-based CLI argument parser (stack parse, heuristic help).
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpbcli-argp.h"

#define TPBCLI_ARGP_STACK_MAX 128

/* Local Function Prototypes */
static void _sf_destroy_children(tpbcli_argnode_t *node);
static void _sf_emit_help(const tpbcli_argnode_t *node, FILE *out);
static tpbcli_argnode_t *_sf_find_child(const tpbcli_argnode_t *parent,
                                        const char *token);
static tpbcli_argnode_t *_sf_find_child_by_name(const tpbcli_argnode_t *parent,
                                                const char *name);
static tpbcli_argnode_t *_sf_find_max_chosen_zero_child(const tpbcli_argnode_t *opt,
                                                        const char *token);
static int _sf_find_arg_dfs(const tpbcli_argnode_t *node, int target_depth,
                            const char *name, tpbcli_argnode_t **out);
static int _sf_resolve_conflict_opts_validate(const tpbcli_argnode_t *node);
static void _sf_reset_parse_state(tpbcli_argnode_t *node);
static tpbcli_argnode_t *_sf_sibling_exclusive_conflict(const tpbcli_argnode_t *match);
static tpbcli_argnode_t *_sf_sibling_conflict_opts_hit(const tpbcli_argnode_t *match);
static int _sf_validate_pre_increment(tpbcli_argnode_t *match, FILE *out);

/**
 * @brief Free all dynamically allocated children (sibling chains), not @a node.
 */
static void
_sf_destroy_children(tpbcli_argnode_t *node)
{
    tpbcli_argnode_t *ch = node->first_child;
    tpbcli_argnode_t *next;

    while (ch != NULL) {
        next = ch->next_sibling;
        _sf_destroy_children(ch);
        free(ch);
        ch = next;
    }
    node->first_child = NULL;
}

static tpbcli_argnode_t *
_sf_find_child(const tpbcli_argnode_t *parent, const char *token)
{
    tpbcli_argnode_t *c;

    if (parent == NULL || token == NULL)
        return NULL;
    for (c = parent->first_child; c != NULL; c = c->next_sibling) {
        if (c->name != NULL && strcmp(c->name, token) == 0)
            return c;
        if (c->short_name != NULL && strcmp(c->short_name, token) == 0)
            return c;
    }
    return NULL;
}

static tpbcli_argnode_t *
_sf_find_child_by_name(const tpbcli_argnode_t *parent, const char *name)
{
    tpbcli_argnode_t *c;

    if (parent == NULL || name == NULL)
        return NULL;
    for (c = parent->first_child; c != NULL; c = c->next_sibling) {
        if (c->name != NULL && strcmp(c->name, name) == 0)
            return c;
    }
    return NULL;
}

static tpbcli_argnode_t *
_sf_find_max_chosen_zero_child(const tpbcli_argnode_t *opt, const char *token)
{
    tpbcli_argnode_t *c;

    if (opt == NULL || token == NULL)
        return NULL;
    for (c = opt->first_child; c != NULL; c = c->next_sibling) {
        if (c->max_chosen != 0)
            continue;
        if (c->name != NULL && strcmp(c->name, token) == 0)
            return c;
        if (c->short_name != NULL && strcmp(c->short_name, token) == 0)
            return c;
    }
    return NULL;
}

static tpbcli_argnode_t *
_sf_sibling_exclusive_conflict(const tpbcli_argnode_t *match)
{
    tpbcli_argnode_t *p;
    tpbcli_argnode_t *s;

    if (match == NULL || match->parent == NULL)
        return NULL;
    p = match->parent;
    for (s = p->first_child; s != NULL; s = s->next_sibling) {
        if (s == match || !s->is_set)
            continue;
        if ((match->flags & TPBCLI_ARGF_EXCLUSIVE) != 0u)
            return s;
        if ((s->flags & TPBCLI_ARGF_EXCLUSIVE) != 0u)
            return s;
    }
    return NULL;
}

static tpbcli_argnode_t *
_sf_sibling_conflict_opts_hit(const tpbcli_argnode_t *match)
{
    int i;
    tpbcli_argnode_t *other;

    if (match == NULL || match->parent == NULL || match->conflict_opts == NULL)
        return NULL;
    for (i = 0; i < match->conflict_count; i++) {
        const char *nm = match->conflict_opts[i];
        if (nm == NULL)
            break;
        other = _sf_find_child_by_name(match->parent, nm);
        if (other != NULL && other->is_set)
            return other;
    }
    return NULL;
}

/**
 * @return TPBE_SUCCESS to continue selection, TPBE_EXIT_ON_HELP, or TPBE_CLI_FAIL.
 */
static int
_sf_validate_pre_increment(tpbcli_argnode_t *match, FILE *out)
{
    tpbcli_argnode_t *x;

    if (match == NULL)
        return TPBE_CLI_FAIL;

    x = _sf_sibling_exclusive_conflict(match);
    if (x != NULL) {
        fprintf(out, "error: cannot use '%s' because '%s' is already selected\n",
                match->name != NULL ? match->name : "?",
                x->name != NULL ? x->name : "?");
        return TPBE_CLI_FAIL;
    }

    x = _sf_sibling_conflict_opts_hit(match);
    if (x != NULL) {
        fprintf(out, "error: '%s' conflicts with '%s'\n",
                match->name != NULL ? match->name : "?",
                x->name != NULL ? x->name : "?");
        return TPBE_CLI_FAIL;
    }

    if (match->max_chosen == 0) {
        if (match->help_fn != NULL) {
            match->help_fn(match, out);
            return TPBE_EXIT_ON_HELP;
        }
        fprintf(out, "%s\n", match->desc != NULL ? match->desc : "deprecated option");
        return TPBE_CLI_FAIL;
    }

    if (match->max_chosen > 0 && match->chosen_count >= match->max_chosen) {
        fprintf(out, "error: '%s' can only be specified %d time(s)\n",
                match->name != NULL ? match->name : "?",
                match->max_chosen);
        return TPBE_CLI_FAIL;
    }

    return TPBE_SUCCESS;
}

static void
_sf_reset_parse_state(tpbcli_argnode_t *node)
{
    tpbcli_argnode_t *c;

    if (node == NULL)
        return;
    node->is_set = 0;
    node->chosen_count = 0;
    node->parsed_value = NULL;
    for (c = node->first_child; c != NULL; c = c->next_sibling)
        _sf_reset_parse_state(c);
}

static int
_sf_resolve_conflict_opts_validate(const tpbcli_argnode_t *node)
{
    int i;
    tpbcli_argnode_t *c;

    if (node == NULL)
        return TPBE_SUCCESS;

    if (node->conflict_opts != NULL) {
        for (i = 0; i < node->conflict_count; i++) {
            const char *nm = node->conflict_opts[i];
            if (nm == NULL)
                break;
            if (_sf_find_child_by_name(node->parent, nm) == NULL) {
                fprintf(stderr,
                        "error: conflict_opts name '%s' is not a sibling of '%s'\n",
                        nm, node->name != NULL ? node->name : "?");
                return TPBE_CLI_FAIL;
            }
        }
    }

    for (c = node->first_child; c != NULL; c = c->next_sibling) {
        if (_sf_resolve_conflict_opts_validate(c) != TPBE_SUCCESS)
            return TPBE_CLI_FAIL;
    }
    return TPBE_SUCCESS;
}

static void
_sf_print_opt_line(FILE *out, const tpbcli_argnode_t *n, const char *value_suffix)
{
    fprintf(out, "  ");
    if (n->name != NULL)
        fprintf(out, "%s", n->name);
    if (n->short_name != NULL)
        fprintf(out, ", %s", n->short_name);
    if (value_suffix != NULL)
        fprintf(out, " %s", value_suffix);
    fprintf(out, "    ");
    if (n->desc != NULL)
        fprintf(out, "%s", n->desc);
    if ((n->flags & TPBCLI_ARGF_MANDATORY) != 0u)
        fprintf(out, "  [required]");
    if ((n->flags & TPBCLI_ARGF_PRESET) != 0u && n->preset_value != NULL)
        fprintf(out, "  (default: %s)", n->preset_value);
    if (n->max_chosen == 0 && n->help_fn == NULL)
        fprintf(out, "  [deprecated]");
    fprintf(out, "\n");
}

static void
_sf_emit_help(const tpbcli_argnode_t *node, FILE *out)
{
    const tpbcli_argnode_t *c;
    int has_cmd = 0;
    int has_opt = 0;
    int first;

    if (node == NULL)
        return;

    /* Leaf */
    if (node->first_child == NULL) {
        fprintf(out, "%s: %s\n",
                node->name != NULL ? node->name : "?",
                node->desc != NULL ? node->desc : "");
        return;
    }

    /* OPT with children: compact format A */
    if (node->type == TPBCLI_ARG_OPT) {
        fprintf(out, "%s <value>: %s\n\nSub-options:\n",
                node->name != NULL ? node->name : "?",
                node->desc != NULL ? node->desc : "");
        for (c = node->first_child; c != NULL; c = c->next_sibling) {
            if (c->type == TPBCLI_ARG_OPT)
                _sf_print_opt_line(out, c, "<value>");
            else
                _sf_print_opt_line(out, c, NULL);
        }
        return;
    }

    /* CMD / root with children */
    for (c = node->first_child; c != NULL; c = c->next_sibling) {
        if (c->type == TPBCLI_ARG_CMD)
            has_cmd = 1;
        else
            has_opt = 1;
    }

    fprintf(out, "Usage: %s", node->name != NULL ? node->name : "?");
    if (has_cmd) {
        fprintf(out, " <");
        first = 1;
        for (c = node->first_child; c != NULL; c = c->next_sibling) {
            if (c->type != TPBCLI_ARG_CMD)
                continue;
            if (!first)
                fprintf(out, "|");
            fprintf(out, "%s", c->name != NULL ? c->name : "?");
            first = 0;
        }
        fprintf(out, ">");
    }
    if (has_opt)
        fprintf(out, " [options]");
    fprintf(out, "\n\n%s\n\n",
            node->desc != NULL ? node->desc : "");

    if (has_cmd) {
        fprintf(out, "Commands:\n");
        for (c = node->first_child; c != NULL; c = c->next_sibling) {
            if (c->type == TPBCLI_ARG_CMD)
                _sf_print_opt_line(out, c, NULL);
        }
    }

    if (has_opt) {
        fprintf(out, "Options:\n");
        for (c = node->first_child; c != NULL; c = c->next_sibling) {
            if (c->type != TPBCLI_ARG_CMD)
                _sf_print_opt_line(out, c,
                                   c->type == TPBCLI_ARG_OPT ? "<value>" : NULL);
        }
    }

    fprintf(out,
            "\nUse \"%s <child> -h\" for more information.\n",
            node->name != NULL ? node->name : "?");
}

/**
 * @brief Default help_fn: emit built-in help for node->parent (or node if root).
 */
void
tpbcli_default_help(const tpbcli_argnode_t *node, FILE *out)
{
    const tpbcli_argnode_t *target;

    if (node == NULL || out == NULL)
        return;
    target = node->parent != NULL ? node->parent : node;
    _sf_emit_help(target, out);
}

static int
_sf_find_arg_dfs(const tpbcli_argnode_t *node, int target_depth, const char *name,
                 tpbcli_argnode_t **out)
{
    tpbcli_argnode_t *c;

    if (node == NULL)
        return TPBE_LIST_NOT_FOUND;
    if (node->depth == target_depth) {
        if (node->name != NULL && strcmp(node->name, name) == 0) {
            *out = (tpbcli_argnode_t *)node;
            return TPBE_SUCCESS;
        }
        if (node->short_name != NULL && strcmp(node->short_name, name) == 0) {
            *out = (tpbcli_argnode_t *)node;
            return TPBE_SUCCESS;
        }
    }
    for (c = node->first_child; c != NULL; c = c->next_sibling) {
        if (_sf_find_arg_dfs(c, target_depth, name, out) == TPBE_SUCCESS)
            return TPBE_SUCCESS;
    }
    return TPBE_LIST_NOT_FOUND;
}

static int
_sf_mandatory_fail(const tpbcli_argnode_t *parent, FILE *out)
{
    const tpbcli_argnode_t *c;

    if (parent == NULL)
        return TPBE_SUCCESS;
    for (c = parent->first_child; c != NULL; c = c->next_sibling) {
        if ((c->flags & TPBCLI_ARGF_MANDATORY) == 0u)
            continue;
        if (c->is_set)
            continue;
        if ((c->flags & TPBCLI_ARGF_PRESET) != 0u)
            continue;
        fprintf(out, "error: missing required option '%s'\n",
                c->name != NULL ? c->name : "?");
        return TPBE_CLI_FAIL;
    }
    return TPBE_SUCCESS;
}

static int
_sf_post_loop(tpbcli_argtree_t *tree, tpbcli_argnode_t **stack, int stack_sz,
                FILE *out)
{
    const tpbcli_argnode_t *c;
    int any_child_set = 0;
    int i;
    int err;

    if (tree == NULL)
        return TPBE_CLI_FAIL;

    for (c = tree->root.first_child; c != NULL; c = c->next_sibling) {
        if (c->is_set) {
            any_child_set = 1;
            break;
        }
    }

    if (tree->root.first_child != NULL && !any_child_set) {
        _sf_emit_help(&tree->root, out);
        return TPBE_EXIT_ON_HELP;
    }

    for (i = stack_sz - 1; i >= 0; i--) {
        err = _sf_mandatory_fail(stack[i], out);
        if (err != TPBE_SUCCESS)
            return err;
    }
    return TPBE_SUCCESS;
}

/**
 * @brief Allocate tree; root is CMD depth 0, max_chosen 1.
 */
tpbcli_argtree_t *
tpbcli_argtree_create(const char *prog_name, const char *prog_desc)
{
    tpbcli_argtree_t *t;

    if (prog_name == NULL)
        return NULL;
    t = (tpbcli_argtree_t *)calloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;
    t->root.name = prog_name;
    t->root.desc = prog_desc;
    t->root.type = TPBCLI_ARG_CMD;
    t->root.depth = 0;
    t->root.max_chosen = 1;
    return t;
}

/**
 * @brief Free tree and all nodes from add_arg; not caller string fields.
 */
void
tpbcli_argtree_destroy(tpbcli_argtree_t *tree)
{
    if (tree == NULL)
        return;
    _sf_destroy_children(&tree->root);
    free(tree);
}

static int
_sf_child_name_exists(const tpbcli_argnode_t *parent, const char *name)
{
    return _sf_find_child_by_name(parent, name) != NULL;
}

/**
 * @brief Append sealed child; rejects duplicate name under parent.
 */
tpbcli_argnode_t *
tpbcli_add_arg(tpbcli_argnode_t *parent, const tpbcli_argconf_t *conf)
{
    tpbcli_argnode_t *n;
    tpbcli_argnode_t *tail;
    int cnt;
    int i;

    if (parent == NULL || conf == NULL || conf->name == NULL)
        return NULL;
    if (_sf_child_name_exists(parent, conf->name))
        return NULL;

    n = (tpbcli_argnode_t *)calloc(1, sizeof(*n));
    if (n == NULL)
        return NULL;

    n->name = conf->name;
    n->short_name = conf->short_name;
    n->desc = conf->desc;
    n->type = conf->type;
    n->flags = conf->flags;
    n->max_chosen = conf->max_chosen;
    n->parse_fn = conf->parse_fn;
    n->help_fn = conf->help_fn;
    n->preset_value = conf->preset_value;
    n->conflict_opts = conf->conflict_opts;
    n->user_data = conf->user_data;
    n->parent = parent;
    n->depth = parent->depth + 1;

    cnt = 0;
    if (conf->conflict_opts != NULL) {
        for (i = 0; conf->conflict_opts[i] != NULL; i++)
            cnt++;
    }
    n->conflict_count = cnt;

    if (parent->first_child == NULL) {
        parent->first_child = n;
    } else {
        for (tail = parent->first_child; tail->next_sibling != NULL;
             tail = tail->next_sibling)
            ;
        tail->next_sibling = n;
    }
    return n;
}

/**
 * @brief Parse argv[1..] with stack algorithm and POST-LOOP checks.
 */
int
tpbcli_parse_args(tpbcli_argtree_t *tree, int argc, char **argv)
{
    tpbcli_argnode_t *stack[TPBCLI_ARGP_STACK_MAX];
    int stack_sz = 0;
    int i;
    int err;
    tpbcli_argnode_t *top;
    tpbcli_argnode_t *match;
    tpbcli_argnode_t *hchild;

    if (tree == NULL || argv == NULL)
        return TPBE_CLI_FAIL;

    _sf_reset_parse_state(&tree->root);
    if (_sf_resolve_conflict_opts_validate(&tree->root) != TPBE_SUCCESS)
        return TPBE_CLI_FAIL;

    if (stack_sz >= TPBCLI_ARGP_STACK_MAX)
        return TPBE_CLI_FAIL;
    stack[stack_sz++] = &tree->root;

    for (i = 1; i < argc; ) {
        const char *tok = argv[i];

        retry:
        top = stack[stack_sz - 1];
        match = _sf_find_child(top, tok);

        if (match != NULL) {
            err = _sf_validate_pre_increment(match, stderr);
            if (err != TPBE_SUCCESS)
                return err;

            if (match->type == TPBCLI_ARG_CMD) {
                match->is_set = 1;
                match->chosen_count++;
                if (stack_sz >= TPBCLI_ARGP_STACK_MAX)
                    return TPBE_CLI_FAIL;
                stack[stack_sz++] = match;
                i++;
                continue;
            }

            if (match->type == TPBCLI_ARG_FLAG) {
                if (match->parse_fn != NULL) {
                    err = match->parse_fn(match, NULL);
                    if (err != 0)
                        return TPBE_CLI_FAIL;
                }
                match->is_set = 1;
                match->chosen_count++;
                i++;
                continue;
            }

            /* OPT */
            if (i + 1 >= argc) {
                fprintf(stderr, "error: '%s' requires a value\n",
                        match->name != NULL ? match->name : "?");
                return TPBE_CLI_FAIL;
            }

            hchild = _sf_find_max_chosen_zero_child(match, argv[i + 1]);
            if (hchild != NULL) {
                err = _sf_validate_pre_increment(hchild, stderr);
                if (err != TPBE_SUCCESS)
                    return err;
                i += 2;
                continue;
            }

            if (match->parse_fn != NULL) {
                err = match->parse_fn(match, argv[i + 1]);
                if (err != 0)
                    return TPBE_CLI_FAIL;
            }
            match->parsed_value = argv[i + 1];
            match->is_set = 1;
            match->chosen_count++;
            if (match->first_child != NULL) {
                if (stack_sz >= TPBCLI_ARGP_STACK_MAX)
                    return TPBE_CLI_FAIL;
                stack[stack_sz++] = match;
            }
            i += 2;
            continue;
        }

        /* pop: reset children of popped node for reuse on rematch */
        if (stack_sz <= 1) {
            fprintf(stderr, "error: unknown argument '%s'\n", tok);
            return TPBE_CLI_FAIL;
        }
        {
            tpbcli_argnode_t *popped = stack[stack_sz - 1];
            tpbcli_argnode_t *ch;

            for (ch = popped->first_child; ch != NULL;
                 ch = ch->next_sibling) {
                _sf_reset_parse_state(ch);
            }
        }
        stack_sz--;
        goto retry;
    }

    return _sf_post_loop(tree, stack, stack_sz, stderr);
}

/**
 * @brief DFS: node at start->depth+offset matching name or short_name.
 */
int
tpbcli_find_arg(const tpbcli_argnode_t *start, const char *name, int offset,
                 tpbcli_argnode_t **out)
{
    int target_depth;

    assert(start != NULL);
    assert(name != NULL);
    assert(out != NULL);
    assert(offset >= 0);

    *out = NULL;
    target_depth = start->depth + offset;
    return _sf_find_arg_dfs(start, target_depth, name, out);
}
