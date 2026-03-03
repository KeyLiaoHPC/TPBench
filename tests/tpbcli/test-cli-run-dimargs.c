/*
 * test-cli-run-dimargs.c
 * Test pack B1: Dimension argument parsing and value generation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../src/tpbcli-run-dim.h"

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT_EQ_INT(msg, expected, actual) do {                    \
    if ((expected) == (actual)) {                                    \
        g_pass++;                                                    \
    } else {                                                         \
        g_fail++;                                                    \
        fprintf(stderr, "  FAIL [%s]: expected %d, got %d\n",       \
                (msg), (expected), (actual));                        \
    }                                                                \
} while (0)

#define ASSERT_EQ_DBL(msg, expected, actual, tol) do {               \
    if (fabs((expected) - (actual)) <= (tol)) {                      \
        g_pass++;                                                    \
    } else {                                                         \
        g_fail++;                                                    \
        fprintf(stderr, "  FAIL [%s]: expected %g, got %g\n",       \
                (msg), (expected), (actual));                        \
    }                                                                \
} while (0)

#define ASSERT_EQ_STR(msg, expected, actual) do {                    \
    if (strcmp((expected), (actual)) == 0) {                          \
        g_pass++;                                                    \
    } else {                                                         \
        g_fail++;                                                    \
        fprintf(stderr, "  FAIL [%s]: expected \"%s\", got \"%s\"\n",\
                (msg), (expected), (actual));                        \
    }                                                                \
} while (0)

/* B1.1: explicit numeric list expansion */
static int
test_list_expansion(void)
{
    tpb_dim_config_t *cfg = NULL;
    tpb_dim_values_t *vals = NULL;
    int err;
    int before = g_fail;

    err = tpb_argp_parse_dim("total_memsize=[16,32,64]", &cfg);
    ASSERT_EQ_INT("list parse", 0, err);
    if (err != 0) return 1;

    ASSERT_EQ_STR("list parm_name", "total_memsize", cfg->parm_name);
    ASSERT_EQ_INT("list type", TPB_DIM_LIST, cfg->type);
    ASSERT_EQ_INT("list count", 3, cfg->spec.list.n);
    ASSERT_EQ_INT("list is_string", 0, cfg->spec.list.is_string);

    err = tpb_dim_generate_values(cfg, &vals);
    ASSERT_EQ_INT("list gen", 0, err);
    if (err != 0) { tpb_dim_config_free(cfg); return 1; }

    ASSERT_EQ_INT("list vals n", 3, vals->n);
    ASSERT_EQ_DBL("list val[0]", 16.0, vals->values[0], 1e-9);
    ASSERT_EQ_DBL("list val[1]", 32.0, vals->values[1], 1e-9);
    ASSERT_EQ_DBL("list val[2]", 64.0, vals->values[2], 1e-9);

    tpb_dim_values_free(vals);
    tpb_dim_config_free(cfg);
    return (g_fail > before) ? 1 : 0;
}

/* B1.2: recursive sequence expansion */
static int
test_recur_expansion(void)
{
    tpb_dim_config_t *cfg = NULL;
    tpb_dim_values_t *vals = NULL;
    int err;
    int before = g_fail;

    err = tpb_argp_parse_dim(
        "total_memsize=mul(@,2)(16,16,128,0)", &cfg);
    ASSERT_EQ_INT("recur parse", 0, err);
    if (err != 0) return 1;

    ASSERT_EQ_STR("recur parm_name", "total_memsize",
                   cfg->parm_name);
    ASSERT_EQ_INT("recur type", TPB_DIM_RECUR, cfg->type);
    ASSERT_EQ_INT("recur op", TPB_DIM_OP_MUL, cfg->spec.recur.op);
    ASSERT_EQ_DBL("recur x", 2.0, cfg->spec.recur.x, 1e-9);
    ASSERT_EQ_DBL("recur st", 16.0, cfg->spec.recur.st, 1e-9);
    ASSERT_EQ_DBL("recur max", 128.0, cfg->spec.recur.max, 1e-9);

    err = tpb_dim_generate_values(cfg, &vals);
    ASSERT_EQ_INT("recur gen", 0, err);
    if (err != 0) { tpb_dim_config_free(cfg); return 1; }

    ASSERT_EQ_INT("recur vals n", 4, vals->n);
    ASSERT_EQ_DBL("recur val[0]", 16.0,  vals->values[0], 1e-9);
    ASSERT_EQ_DBL("recur val[1]", 32.0,  vals->values[1], 1e-9);
    ASSERT_EQ_DBL("recur val[2]", 64.0,  vals->values[2], 1e-9);
    ASSERT_EQ_DBL("recur val[3]", 128.0, vals->values[3], 1e-9);

    tpb_dim_values_free(vals);
    tpb_dim_config_free(cfg);
    return (g_fail > before) ? 1 : 0;
}

/* B1.3: string list expansion */
static int
test_string_list(void)
{
    tpb_dim_config_t *cfg = NULL;
    tpb_dim_values_t *vals = NULL;
    int err;
    int before = g_fail;

    err = tpb_argp_parse_dim(
        "dtype=[double,float,iso-fp16]", &cfg);
    ASSERT_EQ_INT("strlist parse", 0, err);
    if (err != 0) return 1;

    ASSERT_EQ_INT("strlist type", TPB_DIM_LIST, cfg->type);
    ASSERT_EQ_INT("strlist count", 3, cfg->spec.list.n);
    ASSERT_EQ_INT("strlist is_string", 1, cfg->spec.list.is_string);

    err = tpb_dim_generate_values(cfg, &vals);
    ASSERT_EQ_INT("strlist gen", 0, err);
    if (err != 0) { tpb_dim_config_free(cfg); return 1; }

    ASSERT_EQ_INT("strlist vals n", 3, vals->n);
    ASSERT_EQ_INT("strlist vals is_string", 1, vals->is_string);
    ASSERT_EQ_STR("strlist val[0]", "double",   vals->str_values[0]);
    ASSERT_EQ_STR("strlist val[1]", "float",    vals->str_values[1]);
    ASSERT_EQ_STR("strlist val[2]", "iso-fp16", vals->str_values[2]);

    tpb_dim_values_free(vals);
    tpb_dim_config_free(cfg);
    return (g_fail > before) ? 1 : 0;
}

/* B1.4: nested dimension expansion (Cartesian product) */
static int
test_nested_expansion(void)
{
    tpb_dim_config_t *cfg = NULL;
    tpb_dim_values_t *vals = NULL;
    int err;
    int before = g_fail;

    err = tpb_argp_parse_dim(
        "dtype=[double,float]{total_memsize=[16,32]}", &cfg);
    ASSERT_EQ_INT("nested parse", 0, err);
    if (err != 0) return 1;

    ASSERT_EQ_STR("nested parm_name", "dtype", cfg->parm_name);
    ASSERT_EQ_INT("nested outer count", 2, cfg->spec.list.n);

    ASSERT_EQ_INT("nested has child", 1, cfg->nested != NULL);
    if (cfg->nested == NULL) { tpb_dim_config_free(cfg); return 1; }

    ASSERT_EQ_STR("nested child parm", "total_memsize",
                   cfg->nested->parm_name);
    ASSERT_EQ_INT("nested child count", 2, cfg->nested->spec.list.n);

    err = tpb_dim_generate_values(cfg, &vals);
    ASSERT_EQ_INT("nested gen", 0, err);
    if (err != 0) { tpb_dim_config_free(cfg); return 1; }

    ASSERT_EQ_INT("nested vals n", 2, vals->n);
    ASSERT_EQ_INT("nested child vals", 1, vals->nested != NULL);
    if (vals->nested != NULL) {
        ASSERT_EQ_INT("nested child vals n", 2, vals->nested->n);
    }

    tpb_dim_values_free(vals);
    tpb_dim_config_free(cfg);
    return (g_fail > before) ? 1 : 0;
}

/* B1.5: total count matches Cartesian product */
static int
test_total_count(void)
{
    tpb_dim_config_t *cfg = NULL;
    int err, total;
    int before = g_fail;

    /* Nested: 2 dtypes x 2 memsizes = 4 */
    err = tpb_argp_parse_dim(
        "dtype=[double,float]{total_memsize=[16,32]}", &cfg);
    ASSERT_EQ_INT("count parse", 0, err);
    if (err != 0) return 1;

    total = tpb_dim_get_total_count(cfg);
    ASSERT_EQ_INT("count 2x2", 4, total);
    tpb_dim_config_free(cfg);

    /* Flat list: 3 values */
    cfg = NULL;
    err = tpb_argp_parse_dim("x=[1,2,3]", &cfg);
    ASSERT_EQ_INT("count flat parse", 0, err);
    if (err != 0) return 1;

    total = tpb_dim_get_total_count(cfg);
    ASSERT_EQ_INT("count flat 3", 3, total);
    tpb_dim_config_free(cfg);

    /* Recursive: mul(@,2)(16,16,128,0) -> 4 values */
    cfg = NULL;
    err = tpb_argp_parse_dim(
        "ms=mul(@,2)(16,16,128,0)", &cfg);
    ASSERT_EQ_INT("count recur parse", 0, err);
    if (err != 0) return 1;

    total = tpb_dim_get_total_count(cfg);
    ASSERT_EQ_INT("count recur 4", 4, total);
    tpb_dim_config_free(cfg);

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
    int pass = 0, fail = 0, run = 0;

    if (filter == NULL)
        printf("Running test pack %s (%d cases)\n", pack, n);
    printf("------------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        if (filter != NULL && strcmp(filter, cases[i].id) != 0)
            continue;
        run++;
        int result = cases[i].func();
        if (result == 0) {
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
        { "B1.1", "list_expansion",    test_list_expansion    },
        { "B1.2", "recur_expansion",   test_recur_expansion   },
        { "B1.3", "string_list",       test_string_list       },
        { "B1.4", "nested_expansion",  test_nested_expansion  },
        { "B1.5", "total_count",       test_total_count       },
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int fail = run_pack("B1", cases, n, filter);
    return (fail > 0) ? 1 : 0;
}
