/*
 * test_tpbcli_argp.c
 * Pack B3: tpbcli-argp unit tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/tpbcli-argp.h"

static int g_pass;
static int g_fail;
static int g_help_root;
static int g_help_run;
static int g_help_kernel;
static int g_delegate_called;

#define ASSERT_EQ_INT(msg, exp, act) do {                               \
    if ((exp) == (act)) {                                               \
        g_pass++;                                                       \
    } else {                                                            \
        g_fail++;                                                       \
        fprintf(stderr, "  FAIL [%s]: expected %d, got %d\n",           \
                (msg), (int)(exp), (int)(act));                         \
    }                                                                   \
} while (0)

#define ASSERT_EQ_STR(msg, exp, act) do {                               \
    if ((exp) != NULL && (act) != NULL && strcmp((exp), (act)) == 0) {    \
        g_pass++;                                                       \
    } else {                                                            \
        g_fail++;                                                       \
        fprintf(stderr, "  FAIL [%s]: expected \"%s\", got \"%s\"\n",     \
                (msg), (exp) ? (exp) : "(null)",                         \
                (act) ? (act) : "(null)");                              \
    }                                                                   \
} while (0)

#define ASSERT_PTR(msg, cond) do {                                      \
    if (cond) {                                                         \
        g_pass++;                                                       \
    } else {                                                            \
        g_fail++;                                                       \
        fprintf(stderr, "  FAIL [%s]\n", (msg));                        \
    }                                                                   \
} while (0)

#define ASSERT_EQ_PTR(msg, exp, act) do {                               \
    if ((exp) == (act)) {                                               \
        g_pass++;                                                       \
    } else {                                                            \
        g_fail++;                                                       \
        fprintf(stderr, "  FAIL [%s]: pointer mismatch\n", (msg));     \
    }                                                                   \
} while (0)

static void
help_cb_root(const tpbcli_argnode_t *node, FILE *out)
{
    (void)out;
    (void)node;
    g_help_root = 1;
}

static void
help_cb_run(const tpbcli_argnode_t *node, FILE *out)
{
    (void)out;
    (void)node;
    g_help_run = 1;
}

static void
help_cb_kernel(const tpbcli_argnode_t *node, FILE *out)
{
    (void)out;
    (void)node;
    g_help_kernel = 1;
}

typedef struct {
    int k_calls;
    const char *k_last;
    int ka_calls;
    const char *ka_last;
    int t_calls;
    const char *t_last;
} parse_track_t;

static int
parse_kernel(tpbcli_argnode_t *node, const char *value)
{
    parse_track_t *tr = (parse_track_t *)node->user_data;
    if (tr != NULL) {
        tr->k_calls++;
        tr->k_last = value;
    }
    return 0;
}

static int
parse_kargs(tpbcli_argnode_t *node, const char *value)
{
    parse_track_t *tr = (parse_track_t *)node->user_data;
    if (tr != NULL) {
        tr->ka_calls++;
        tr->ka_last = value;
    }
    return 0;
}

static int
parse_timer(tpbcli_argnode_t *node, const char *value)
{
    parse_track_t *tr = (parse_track_t *)node->user_data;
    if (tr != NULL) {
        tr->t_calls++;
        tr->t_last = value;
    }
    return 0;
}

static int
parse_p(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return 0;
}

static int
parse_f(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return 0;
}

static int
test_tree_lifecycle(void)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *a;
    tpbcli_argnode_t *b;
    tpbcli_argnode_t *dup;
    int before = g_fail;

    tree = tpbcli_argtree_create("prog", "desc");
    ASSERT_PTR("create non-NULL", tree != NULL);
    if (tree == NULL)
        return (g_fail > before) ? 1 : 0;

    ASSERT_EQ_STR("root name", "prog", tree->root.name);
    ASSERT_EQ_INT("root depth", 0, tree->root.depth);
    ASSERT_EQ_INT("root type", TPBCLI_ARG_CMD, (int)tree->root.type);

    a = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "a",
        .desc = "A",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    b = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "b",
        .desc = "B",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    ASSERT_PTR("add a", a != NULL);
    ASSERT_PTR("add b", b != NULL);
    ASSERT_EQ_INT("a depth", 1, a->depth);
    ASSERT_EQ_PTR("a parent", &tree->root, a->parent);
    ASSERT_EQ_PTR("b parent", &tree->root, b->parent);
    ASSERT_PTR("sibling chain", a->next_sibling == b && b->next_sibling == NULL);

    dup = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "a",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    ASSERT_PTR("dup rejected", dup == NULL);

    tpbcli_argtree_destroy(tree);
    tpbcli_argtree_destroy(NULL);

    return (g_fail > before) ? 1 : 0;
}

static int
test_parse_basic(void)
{
    tpbcli_argtree_t *tree;
    parse_track_t tr;
    tpbcli_argnode_t *run;
    tpbcli_argnode_t *kern;
    tpbcli_argnode_t *out;
    char *argv[] = { "prog", "run", "--kernel", "stream", "--kargs", "n=10" };
    int argc = 6;
    int err;
    int before = g_fail;

    memset(&tr, 0, sizeof(tr));

    tree = tpbcli_argtree_create("prog", "test");
    run = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    kern = tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--kernel",
        .type = TPBCLI_ARG_OPT,
        .flags = TPBCLI_ARGF_MANDATORY,
        .max_chosen = -1,
        .parse_fn = parse_kernel,
        .user_data = &tr,
    });
    tpbcli_add_arg(kern, &(tpbcli_argconf_t){
        .name = "--kargs",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_kargs,
        .user_data = &tr,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--timer",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_timer,
        .user_data = &tr,
    });

    err = tpbcli_parse_args(tree, argc, argv);
    ASSERT_EQ_INT("parse err", TPBE_SUCCESS, err);
    ASSERT_PTR("run set", run->is_set);
    ASSERT_EQ_STR("kernel val", "stream", kern->parsed_value);
    ASSERT_EQ_INT("k calls", 1, tr.k_calls);
    ASSERT_EQ_INT("ka calls", 1, tr.ka_calls);
    ASSERT_EQ_STR("ka last", "n=10", tr.ka_last);
    ASSERT_EQ_INT("run chosen_count", 1, run->chosen_count);
    ASSERT_EQ_INT("kernel chosen_count", 1, kern->chosen_count);

    out = NULL;
    ASSERT_EQ_INT("find leaf", TPBE_SUCCESS,
                  tpbcli_find_arg(&tree->root, "--kargs", 3, &out));
    ASSERT_PTR("leaf found", out != NULL);

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_mandatory_and_preset(void)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *run;
    tpbcli_argnode_t *timer;
    char *argv1[] = { "prog", "run" };
    char *argv2[] = { "prog", "run", "--kernel", "s" };
    int before = g_fail;
    int err;

    tree = tpbcli_argtree_create("prog", "t");
    run = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--kernel",
        .type = TPBCLI_ARG_OPT,
        .flags = TPBCLI_ARGF_MANDATORY,
        .max_chosen = 1,
        .parse_fn = parse_kernel,
    });
    timer = tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--timer",
        .type = TPBCLI_ARG_OPT,
        .flags = TPBCLI_ARGF_MANDATORY | TPBCLI_ARGF_PRESET,
        .max_chosen = 1,
        .preset_value = "cgt",
        .parse_fn = parse_timer,
    });

    err = tpbcli_parse_args(tree, 2, argv1);
    ASSERT_EQ_INT("missing kernel", TPBE_CLI_FAIL, err);

    err = tpbcli_parse_args(tree, 4, argv2);
    ASSERT_EQ_INT("ok with preset timer", TPBE_SUCCESS, err);
    ASSERT_EQ_INT("timer not set", 0, timer->is_set);

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_exclusive_conflict(void)
{
    tpbcli_argtree_t *tree;
    char *argv[] = { "prog", "run", "benchmark" };
    int before = g_fail;

    tree = tpbcli_argtree_create("prog", "t");
    tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "benchmark",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });

    ASSERT_EQ_INT("exclusive", TPBE_CLI_FAIL,
                  tpbcli_parse_args(tree, 3, argv));

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_conflict_opts(void)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *run;
    char *argv_bad[] = { "prog", "run", "-P", "4", "-F", "x" };
    char *argv_ok[] = { "prog", "run", "-P", "4", "--timer", "t" };
    int before = g_fail;

    tree = tpbcli_argtree_create("prog", "t");
    run = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "-P",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .conflict_opts = (const char *[]){ "-F", NULL },
        .parse_fn = parse_p,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "-F",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .conflict_opts = (const char *[]){ "-P", NULL },
        .parse_fn = parse_f,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--timer",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_timer,
    });

    ASSERT_EQ_INT("conflict P F", TPBE_CLI_FAIL,
                  tpbcli_parse_args(tree, 6, argv_bad));
    ASSERT_EQ_INT("P timer ok", TPBE_SUCCESS,
                  tpbcli_parse_args(tree, 6, argv_ok));

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_help_dispatch(void)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *run;
    tpbcli_argnode_t *kern;
    char *av1[] = { "prog", "-h" };
    char *av2[] = { "prog", "run", "--kernel", "-h" };
    char *av3[] = { "prog" };
    int before = g_fail;

    g_help_root = g_help_run = g_help_kernel = 0;

    tree = tpbcli_argtree_create("prog", "t");
    tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "-h",
        .short_name = "--help",
        .type = TPBCLI_ARG_FLAG,
        .max_chosen = 0,
        .help_fn = help_cb_root,
    });
    run = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "-h",
        .short_name = "--help",
        .type = TPBCLI_ARG_FLAG,
        .max_chosen = 0,
        .help_fn = help_cb_run,
    });
    kern = tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--kernel",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_kernel,
    });
    tpbcli_add_arg(kern, &(tpbcli_argconf_t){
        .name = "--kargs",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_kargs,
    });
    tpbcli_add_arg(kern, &(tpbcli_argconf_t){
        .name = "-h",
        .short_name = "--help",
        .type = TPBCLI_ARG_FLAG,
        .max_chosen = 0,
        .help_fn = help_cb_kernel,
    });

    ASSERT_EQ_INT("root -h", TPBE_EXIT_ON_HELP,
                  tpbcli_parse_args(tree, 2, av1));
    ASSERT_EQ_INT("help root", 1, g_help_root);
    ASSERT_EQ_INT("help run 0", 0, g_help_run);

    g_help_root = g_help_run = g_help_kernel = 0;
    ASSERT_EQ_INT("kernel -h", TPBE_EXIT_ON_HELP,
                  tpbcli_parse_args(tree, 5, av2));
    ASSERT_EQ_INT("help kernel", 1, g_help_kernel);
    ASSERT_EQ_INT("help root 0", 0, g_help_root);

    g_help_root = 0;
    ASSERT_EQ_INT("bare prog", TPBE_EXIT_ON_HELP,
                  tpbcli_parse_args(tree, 1, av3));

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_unknown_arg(void)
{
    tpbcli_argtree_t *tree;
    char *argv[] = { "prog", "--bogus" };
    int before = g_fail;

    tree = tpbcli_argtree_create("prog", "t");
    tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });

    ASSERT_EQ_INT("unknown", TPBE_CLI_FAIL,
                  tpbcli_parse_args(tree, 2, argv));

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_stack_pop_retry(void)
{
    tpbcli_argtree_t *tree;
    parse_track_t tr;
    tpbcli_argnode_t *run;
    tpbcli_argnode_t *kern;
    char *av1[] = { "prog", "run", "--kernel", "s", "--kargs", "n=1",
                    "--timer", "t" };
    char *av2[] = { "prog", "run", "--kernel", "a", "--kargs", "x=1",
                    "--kernel", "b", "--kargs", "y=2" };
    int before = g_fail;

    memset(&tr, 0, sizeof(tr));

    tree = tpbcli_argtree_create("prog", "t");
    run = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    kern = tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--kernel",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = parse_kernel,
        .user_data = &tr,
    });
    tpbcli_add_arg(kern, &(tpbcli_argconf_t){
        .name = "--kargs",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = parse_kargs,
        .user_data = &tr,
    });
    tpbcli_add_arg(run, &(tpbcli_argconf_t){
        .name = "--timer",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_timer,
        .user_data = &tr,
    });

    ASSERT_EQ_INT("pop timer", TPBE_SUCCESS,
                  tpbcli_parse_args(tree, 8, av1));
    ASSERT_EQ_INT("timer calls", 1, tr.t_calls);

    memset(&tr, 0, sizeof(tr));
    ASSERT_EQ_INT("double kernel", TPBE_SUCCESS,
                  tpbcli_parse_args(tree, 10, av2));
    ASSERT_EQ_INT("kernel chosen 2", 2, kern->chosen_count);

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
test_max_chosen(void)
{
    tpbcli_argtree_t *t1;
    tpbcli_argtree_t *t2;
    tpbcli_argnode_t *run1;
    char *av_dep[] = { "prog", "run", "--old", "v" };
    char *av_twice[] = { "prog", "run", "run" };
    int before = g_fail;

    t1 = tpbcli_argtree_create("prog", "t");
    run1 = tpbcli_add_arg(&t1->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    tpbcli_add_arg(run1, &(tpbcli_argconf_t){
        .name = "--old",
        .desc = "use --new instead",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 0,
        .parse_fn = parse_kernel,
    });

    ASSERT_EQ_INT("deprecated", TPBE_CLI_FAIL,
                  tpbcli_parse_args(t1, 4, av_dep));
    tpbcli_argtree_destroy(t1);

    t2 = tpbcli_argtree_create("prog", "t");
    tpbcli_add_arg(&t2->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    ASSERT_EQ_INT("run twice fail", TPBE_CLI_FAIL,
                  tpbcli_parse_args(t2, 3, av_twice));
    tpbcli_argtree_destroy(t2);

    t2 = tpbcli_argtree_create("prog", "t");
    tpbcli_add_arg(&t2->root, &(tpbcli_argconf_t){
        .name = "run",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = -1,
    });
    ASSERT_EQ_INT("run twice ok", TPBE_SUCCESS,
                  tpbcli_parse_args(t2, 3, av_twice));

    tpbcli_argtree_destroy(t2);
    return (g_fail > before) ? 1 : 0;
}

static int
test_find_arg(void)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *out;
    tpbcli_argnode_t *a;
    int before = g_fail;
    int r;

    tree = tpbcli_argtree_create("prog", "t");
    a = tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
        .name = "a",
        .type = TPBCLI_ARG_CMD,
        .max_chosen = 1,
    });
    tpbcli_add_arg(a, &(tpbcli_argconf_t){
        .name = "leaf",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = parse_kernel,
    });

    out = NULL;
    r = tpbcli_find_arg(&tree->root, "leaf", 2, &out);
    ASSERT_EQ_INT("find leaf depth2", TPBE_SUCCESS, r);
    ASSERT_PTR("leaf ptr", out != NULL);
    ASSERT_EQ_STR("leaf name", "leaf", out->name);

    out = NULL;
    r = tpbcli_find_arg(&tree->root, "leaf", 1, &out);
    ASSERT_EQ_INT("leaf not depth1", TPBE_LIST_NOT_FOUND, r);

    out = NULL;
    r = tpbcli_find_arg(&tree->root, "a", 1, &out);
    ASSERT_EQ_INT("find a", TPBE_SUCCESS, r);
    ASSERT_EQ_STR("a name", "a", out->name);

    tpbcli_argtree_destroy(tree);
    return (g_fail > before) ? 1 : 0;
}

static int
delegate_parse_cb(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    g_delegate_called = 1;
    return 0;
}

static int
test_delegate_subcmd(void)
{
    tpbcli_argtree_t *tree;
    char *argv[] = { (char *)"prog", (char *)"sub", (char *)"--unparsed" };
    int argc = 3;
    int err;
    int before = g_fail;

    g_delegate_called = 0;
    tree = tpbcli_argtree_create("prog", "test delegate");
    ASSERT_PTR("delegate tree", tree != NULL);
    if (tree == NULL) {
        return (g_fail > before) ? 1 : 0;
    }
    ASSERT_PTR("delegate add_arg",
               tpbcli_add_arg(&tree->root, &(tpbcli_argconf_t){
                   .name = "sub",
                   .type = TPBCLI_ARG_CMD,
                   .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
                   .max_chosen = 1,
                   .parse_fn = delegate_parse_cb,
               }) != NULL);
    if (g_fail > before) {
        tpbcli_argtree_destroy(tree);
        return 1;
    }
    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);
    ASSERT_EQ_INT("delegate rc", TPBE_SUCCESS, err);
    ASSERT_EQ_INT("delegate called", 1, g_delegate_called);
    return (g_fail > before) ? 1 : 0;
}

typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

static int
run_pack(const char *pack, test_case_t *cases, int n, const char *filter)
{
    int pass = 0;
    int fail = 0;
    int run = 0;
    int i;

    if (filter == NULL)
        printf("Running test pack %s (%d cases)\n", pack, n);
    printf("------------------------------------------------------\n");

    for (i = 0; i < n; i++) {
        if (filter != NULL && strcmp(filter, cases[i].id) != 0)
            continue;
        run++;
        if (cases[i].func() == 0) {
            printf("[%s] %-40s PASS\n", cases[i].id, cases[i].name);
            pass++;
        } else {
            printf("[%s] %-40s FAIL\n", cases[i].id, cases[i].name);
            fail++;
        }
    }

    if (filter != NULL && run == 0) {
        fprintf(stderr, "No case matching '%s' in pack %s\n", filter, pack);
        return 1;
    }

    printf("------------------------------------------------------\n");
    if (filter == NULL)
        printf("Pack %s: %d passed, %d failed\n\n", pack, pass, fail);
    return fail;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    test_case_t cases[] = {
        { "B3.1",  "tree_lifecycle",       test_tree_lifecycle       },
        { "B3.2",  "parse_basic",          test_parse_basic          },
        { "B3.3",  "mandatory_and_preset", test_mandatory_and_preset },
        { "B3.4",  "exclusive_conflict",   test_exclusive_conflict   },
        { "B3.5",  "conflict_opts",        test_conflict_opts        },
        { "B3.6",  "help_dispatch",        test_help_dispatch        },
        { "B3.7",  "unknown_arg",          test_unknown_arg          },
        { "B3.8",  "stack_pop_retry",      test_stack_pop_retry      },
        { "B3.9",  "max_chosen",           test_max_chosen           },
        { "B3.10", "find_arg",             test_find_arg             },
        { "B3.11", "delegate_subcmd",      test_delegate_subcmd      },
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));

    g_pass = 0;
    g_fail = 0;
    return run_pack("B3", cases, n, filter) > 0 ? 1 : 0;
}
