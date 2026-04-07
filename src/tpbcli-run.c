/*
 * tpbcli-run.c
 * Run subcommand implementation.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <string.h>
#include <inttypes.h>
#include "tpbcli-run.h"
#include "corelib/tpb-argp.h"
#include "tpbcli-run-dim.h"
#include "tpbcli-argp.h"
#include "corelib/tpb-driver.h"
#include "corelib/tpb-impl.h"
#include "corelib/tpb-io.h"
#include "corelib/tpb-autorecord.h"

/* Maximum number of dimension configs per kernel */
#define MAX_DIM_CONFIGS 16

/* Argp migration: run-subcommand state */
static int g_dry_run;
static int g_kernel_parsed_for_help;
static tpb_dim_config_t *g_pending_dim_cfgs[MAX_DIM_CONFIGS];
static int g_n_pending_dims;
static char g_pending_kernel_name[TPBM_NAME_STR_MAX_LEN];

/* Local Function Prototypes */

static int apply_dim_value(tpb_dim_values_t *dim_val, int index);
static int apply_env_dim_value(tpb_dim_values_t *dim_val, int index);
static int expand_dim_handles(tpb_dim_config_t *dim_cfg,
                              const char *kernel_name);
static int expand_env_dim_handles(tpb_dim_config_t *dim_cfg,
                                  const char *kernel_name);
static int expand_kmpiargs_dim(const char *arg, const char *kernel_name);
static int parse_kenvs_tokstr(const char *tokstr);
static char *parse_kmpiargs_list_item(const char *str, const char **pos);
static int parse_kmpiargs_list(const char *str, const char **pos,
                               char ***out_items, int *out_count);
static int parse_kmpiargs_quoted(const char *arg);
static int parse_outargs_string(const char *outargs_str);

static int _sf_expand_cartesian_kargs(void);
static void _sf_flush_pending_dims(void);
static void _sf_help_kernel_children(const tpbcli_argnode_t *node, FILE *out);
static int _sf_parse_dry_run(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kargs(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kargs_dim(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kenvs(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kenvs_dim(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kernel(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kmpiargs(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_kmpiargs_dim(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_outargs(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_timer(tpbcli_argnode_t *node, const char *value);
static void _sf_print_kernel_list(FILE *out);

/* Local Function Implementations */

static int
parse_outargs_string(const char *outargs_str)
{
    char *str_copy = strdup(outargs_str);
    if (str_copy == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    /* Variables to accumulate settings - use defaults if not specified */
    int unit_cast = 0;      /* Default: no unit casting */
    int sigbit_trim = 5;    /* Default: 5 significant bits */

    char *token = strtok(str_copy, ",");
    while (token != NULL) {
        /* Skip leading whitespace */
        while (*token == ' ' || *token == '\t') {
            token++;
        }

        /* Parse key=value pair */
        char *eq = strchr(token, '=');
        if (eq != NULL) {
            *eq = '\0';
            char *key = token;
            char *value = eq + 1;

            /* Remove trailing whitespace from key */
            char *key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
                *key_end = '\0';
                key_end--;
            }

            /* Skip leading whitespace in value */
            while (*value == ' ' || *value == '\t') {
                value++;
            }

            if (strcmp(key, "unit_cast") == 0) {
                unit_cast = atoi(value);
            } else if (strcmp(key, "sigbit_trim") == 0) {
                sigbit_trim = atoi(value);
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown outargs key: %s\n", key);
                free(str_copy);
                return TPBE_CLI_FAIL;
            }
        }

        token = strtok(NULL, ",");
    }

    /* Apply settings after parsing all key=value pairs */
    tpb_set_outargs(unit_cast, sigbit_trim);

    free(str_copy);
    return 0;
}

static int
apply_dim_value(tpb_dim_values_t *dim_val, int index)
{
    char value_str[256];

    if (dim_val == NULL || index < 0 || index >= dim_val->n) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (dim_val->is_string && dim_val->str_values != NULL) {
        /* Use string value directly */
        return tpb_driver_set_hdl_karg(dim_val->parm_name,
                                       dim_val->str_values[index]);
    } else {
        /* Convert numeric value to string */
        double val = dim_val->values[index];
        if (val == (int64_t)val) {
            snprintf(value_str, sizeof(value_str), "%" PRId64, (int64_t)val);
        } else {
            snprintf(value_str, sizeof(value_str), "%.15g", val);
        }
        return tpb_driver_set_hdl_karg(dim_val->parm_name, value_str);
    }
}

static int
expand_dim_handles(tpb_dim_config_t *dim_cfg, const char *kernel_name)
{
    tpb_dim_values_t *vals = NULL;
    int err;
    int orig_hdl_idx;

    if (dim_cfg == NULL || kernel_name == NULL) {
        return 0;
    }

    err = tpb_dim_generate_values(dim_cfg, &vals);
    if (err != 0) {
        return err;
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d values for '%s' on kernel '%s'\n",
               vals->n, vals->parm_name, kernel_name);

    orig_hdl_idx = tpb_driver_get_current_hdl_idx();

    for (int i = 0; i < vals->n; i++) {
        if (i == 0) {
            err = apply_dim_value(vals, i);
        } else {
            err = tpb_driver_add_handle(kernel_name);
            if (err == 0) {
                err = tpb_driver_copy_hdl_from(orig_hdl_idx);
            }
            if (err == 0) {
                err = apply_dim_value(vals, i);
            }
        }
        if (err != 0) {
            tpb_dim_values_free(vals);
            return err;
        }
    }

    tpb_dim_values_free(vals);
    return 0;
}

static int
_sf_expand_cartesian_kargs(void)
{
    tpb_dim_values_t *vals[MAX_DIM_CONFIGS];
    int ndims = g_n_pending_dims;
    int total = 1;
    int orig_hdl_idx;
    int err = 0;
    int d;
    int c;
    int rem;
    int idx;

    if (ndims <= 0) {
        return 0;
    }

    for (d = 0; d < ndims; d++) {
        err = tpb_dim_generate_values(g_pending_dim_cfgs[d], &vals[d]);
        if (err != 0) {
            for (d = d - 1; d >= 0; d--) {
                tpb_dim_values_free(vals[d]);
            }
            return err;
        }
        total *= vals[d]->n;
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d Cartesian combinations for kernel '%s'\n",
               total, g_pending_kernel_name);

    orig_hdl_idx = tpb_driver_get_current_hdl_idx();

    for (c = 0; c < total; c++) {
        if (c > 0) {
            err = tpb_driver_add_handle(g_pending_kernel_name);
            if (err != 0) {
                break;
            }
            err = tpb_driver_copy_hdl_from(orig_hdl_idx);
            if (err != 0) {
                break;
            }
        }

        rem = c;
        for (d = ndims - 1; d >= 0; d--) {
            idx = rem % vals[d]->n;
            rem /= vals[d]->n;
            err = apply_dim_value(vals[d], idx);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                           "Failed to set dim '%s'\n",
                           vals[d]->parm_name);
            }
        }
    }

    for (d = 0; d < ndims; d++) {
        tpb_dim_values_free(vals[d]);
    }

    return err;
}

static int
parse_kenvs_tokstr(const char *tokstr)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *saveptr;
    char *token;
    int err;

    if (tokstr == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    snprintf(buf, sizeof(buf), "%s", tokstr);
    token = strtok_r(buf, ",", &saveptr);

    while (token != NULL) {
        /* Skip leading whitespace */
        while (*token == ' ' || *token == '\t') {
            token++;
        }

        /* Parse key=value */
        char *eq = strchr(token, '=');
        if (eq == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid env arg \"%s\". Expected KEY=VALUE.\n", token);
            return TPBE_CLI_FAIL;
        }

        *eq = '\0';
        char *key = token;
        char *value = eq + 1;

        /* Remove trailing whitespace from key */
        char *key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
            *key_end = '\0';
            key_end--;
        }

        /* Skip leading whitespace in value */
        while (*value == ' ' || *value == '\t') {
            value++;
        }

        if (*key == '\0') {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid env arg: empty key detected.\n");
            return TPBE_CLI_FAIL;
        }

        /* Set environment variable for current handle */
        err = tpb_driver_set_hdl_env(key, value);
        if (err != 0) {
            return err;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

static int
apply_env_dim_value(tpb_dim_values_t *dim_val, int index)
{
    char value_str[TPBM_CLI_STR_MAX_LEN];

    if (dim_val == NULL || index < 0 || index >= dim_val->n) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (dim_val->is_string && dim_val->str_values != NULL) {
        return tpb_driver_set_hdl_env(dim_val->parm_name,
                                      dim_val->str_values[index]);
    } else {
        double val = dim_val->values[index];
        if (val == (int64_t)val) {
            snprintf(value_str, sizeof(value_str), "%" PRId64, (int64_t)val);
        } else {
            snprintf(value_str, sizeof(value_str), "%.15g", val);
        }
        return tpb_driver_set_hdl_env(dim_val->parm_name, value_str);
    }
}

static int
expand_env_dim_handles(tpb_dim_config_t *dim_cfg, const char *kernel_name)
{
    tpb_dim_values_t *vals = NULL;
    int err;
    int orig_hdl_idx;

    if (dim_cfg == NULL || kernel_name == NULL) {
        return 0;
    }

    err = tpb_dim_generate_values(dim_cfg, &vals);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to generate env dimension values\n");
        return err;
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d env values for '%s' on kernel '%s'\n",
               vals->n, vals->parm_name, kernel_name);

    orig_hdl_idx = tpb_driver_get_current_hdl_idx();

    for (int i = 0; i < vals->n; i++) {
        if (i == 0) {
            err = apply_env_dim_value(vals, i);
        } else {
            err = tpb_driver_add_handle(kernel_name);
            if (err == 0) {
                err = tpb_driver_copy_hdl_from(orig_hdl_idx);
            }
            if (err == 0) {
                err = apply_env_dim_value(vals, i);
            }
        }
        if (err != 0) {
            tpb_dim_values_free(vals);
            return err;
        }
    }

    tpb_dim_values_free(vals);
    return 0;
}

static int
parse_kmpiargs_quoted(const char *arg)
{
    if (arg == NULL || arg[0] == '\0') {
        return TPBE_NULLPTR_ARG;
    }

    const char *content = arg;
    size_t content_len = strlen(arg);
    char *allocated_content = NULL;

    /* Check if the string is quoted (in case quotes survived shell parsing) */
    char quote_char = arg[0];
    if ((quote_char == '\'' || quote_char == '"') && content_len >= 2) {
        /* Check for closing quote */
        if (arg[content_len - 1] == quote_char) {
            /* Extract content between quotes */
            content_len = content_len - 2;
            allocated_content = (char *)malloc(content_len + 1);
            if (allocated_content == NULL) {
                return TPBE_MALLOC_FAIL;
            }
            memcpy(allocated_content, arg + 1, content_len);
            allocated_content[content_len] = '\0';
            content = allocated_content;
        }
    }

    int err = tpb_driver_append_hdl_mpiargs(content);

    if (allocated_content != NULL) {
        free(allocated_content);
    }
    return err;
}

static char *
parse_kmpiargs_list_item(const char *str, const char **pos)
{
    (void)str;

    /* Skip leading whitespace */
    while (**pos == ' ' || **pos == '\t' || **pos == ',') {
        (*pos)++;
    }

    if (**pos == '\0' || **pos == ']') {
        return NULL;
    }

    char quote_char = **pos;
    if (quote_char != '\'' && quote_char != '"') {
        return NULL;
    }

    (*pos)++;  /* Skip opening quote */
    const char *start = *pos;

    /* Find closing quote */
    const char *end = strchr(*pos, quote_char);
    if (end == NULL) {
        return NULL;
    }

    size_t len = end - start;
    char *item = (char *)malloc(len + 1);
    if (item == NULL) {
        return NULL;
    }
    memcpy(item, start, len);
    item[len] = '\0';

    *pos = end + 1;  /* Move past closing quote */
    return item;
}

static int
parse_kmpiargs_list(const char *str, const char **pos, char ***out_items,
                    int *out_count)
{
    /* Skip leading whitespace */
    while (**pos == ' ' || **pos == '\t') {
        (*pos)++;
    }

    if (**pos != '[') {
        return TPBE_CLI_FAIL;
    }
    (*pos)++;  /* Skip '[' */

    char **items = NULL;
    int count = 0;
    int capacity = 8;

    items = (char **)malloc(sizeof(char *) * capacity);
    if (items == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    while (**pos != '\0' && **pos != ']') {
        char *item = parse_kmpiargs_list_item(str, pos);
        if (item == NULL) {
            /* Skip whitespace/commas and check for end */
            while (**pos == ' ' || **pos == '\t' || **pos == ',') {
                (*pos)++;
            }
            if (**pos == ']' || **pos == '\0') {
                break;
            }
            for (int i = 0; i < count; i++) {
                free(items[i]);
            }
            free(items);
            return TPBE_CLI_FAIL;
        }

        if (count >= capacity) {
            capacity *= 2;
            char **new_items = (char **)realloc(items, sizeof(char *) * capacity);
            if (new_items == NULL) {
                free(item);
                for (int i = 0; i < count; i++) {
                    free(items[i]);
                }
                free(items);
                return TPBE_MALLOC_FAIL;
            }
            items = new_items;
        }
        items[count++] = item;
    }

    if (**pos == ']') {
        (*pos)++;  /* Skip ']' */
    }

    *out_items = items;
    *out_count = count;
    return 0;
}

static int
expand_kmpiargs_dim(const char *arg, const char *kernel_name)
{
    if (arg == NULL || kernel_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    const char *pos = arg;
    char **items = NULL;
    int count = 0;
    int err;

    err = parse_kmpiargs_list(arg, &pos, &items, &count);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Failed to parse --kmpiargs-dim list: %s\n", arg);
        return err;
    }

    if (count == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "--kmpiargs-dim: empty list.\n");
        free(items);
        return TPBE_CLI_FAIL;
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d kmpiargs-dim values for kernel '%s'\n",
               count, kernel_name);

    int orig_hdl_idx = tpb_driver_get_current_hdl_idx();
    const char *base_mpiargs = tpb_driver_get_hdl_mpiargs();
    char *saved_base = (base_mpiargs != NULL) ? strdup(base_mpiargs) : NULL;

    for (int i = 0; i < count; i++) {
        if (i == 0) {
            if (saved_base != NULL) {
                err = tpb_driver_set_hdl_mpiargs(saved_base);
            } else {
                err = tpb_driver_set_hdl_mpiargs("");
            }
            if (err != 0) {
                free(saved_base);
                goto cleanup;
            }
            err = tpb_driver_append_hdl_mpiargs(items[i]);
        } else {
            err = tpb_driver_add_handle(kernel_name);
            if (err != 0) {
                free(saved_base);
                goto cleanup;
            }
            err = tpb_driver_copy_hdl_from(orig_hdl_idx);
            if (err != 0) {
                free(saved_base);
                goto cleanup;
            }
            if (saved_base != NULL) {
                err = tpb_driver_set_hdl_mpiargs(saved_base);
            } else {
                err = tpb_driver_set_hdl_mpiargs("");
            }
            if (err != 0) {
                free(saved_base);
                goto cleanup;
            }
            err = tpb_driver_append_hdl_mpiargs(items[i]);
        }
        if (err != 0) {
            free(saved_base);
            goto cleanup;
        }
    }

    free(saved_base);
    err = 0;

cleanup:
    for (int i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);

    return err;
}

static void
_sf_print_kernel_list(FILE *out)
{
    int nkern = tpb_query_kernel(0, NULL, NULL);
    int i;

    fprintf(out,
            "Available kernels:\n"
            "  %-15s %-9s %s\n",
            "Kernel", "KernelID", "Description");

    for (i = 0; i < nkern; i++) {
        tpb_kernel_t *kernel = NULL;

        tpb_query_kernel(i, NULL, &kernel);
        if (kernel == NULL) {
            continue;
        }
        fprintf(out, "  %-15s %-9s %s\n",
                kernel->info.name, "-",
                kernel->info.note);
        tpb_free_kernel(kernel);
        free(kernel);
    }
}

static void
_sf_help_kernel_children(const tpbcli_argnode_t *node, FILE *out)
{
    tpb_kernel_t *kernel = NULL;
    int i;

    if (g_kernel_parsed_for_help) {
        g_kernel_parsed_for_help = 0;
        tpb_query_kernel(-1, g_pending_kernel_name, &kernel);
        if (kernel == NULL) {
            fprintf(out, "Kernel '%s' info not available.\n",
                    g_pending_kernel_name);
            return;
        }
        fprintf(out, "Kernel: %s\n", kernel->info.name);
        fprintf(out, "  %s\n\n", kernel->info.note);

        if (kernel->info.nparms > 0) {
            fprintf(out, "Parameters:\n");
            for (i = 0; i < kernel->info.nparms; i++) {
                tpb_rt_parm_t *p = &kernel->info.parms[i];
                fprintf(out, "  %-20s %s\n", p->name, p->note);
            }
            fprintf(out, "\n");
        }

        if (kernel->info.nouts > 0) {
            fprintf(out, "Outputs:\n");
            for (i = 0; i < kernel->info.nouts; i++) {
                tpb_k_output_t *o = &kernel->info.outs[i];
                fprintf(out, "  %-20s %s\n", o->name, o->note);
            }
        }

        tpb_free_kernel(kernel);
        free(kernel);
        return;
    }

    fprintf(out,
            "--kernel option requires a legal kernel name.\n\n");
    _sf_print_kernel_list(out);
    fprintf(out, "\n");
    tpbcli_default_help(node, out);
}

static void
_sf_flush_pending_dims(void)
{
    int err;

    if (g_n_pending_dims <= 0) {
        return;
    }

    if (g_n_pending_dims == 1) {
        err = expand_dim_handles(g_pending_dim_cfgs[0],
                                 g_pending_kernel_name);
    } else {
        err = _sf_expand_cartesian_kargs();
    }

    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "Dimension expansion failed (%d)\n",
                   err);
    }

    for (int i = 0; i < g_n_pending_dims; i++) {
        tpb_dim_config_free(g_pending_dim_cfgs[i]);
        g_pending_dim_cfgs[i] = NULL;
    }
    g_n_pending_dims = 0;
}

static int
_sf_parse_kernel(tpbcli_argnode_t *node, const char *value)
{
    int err;

    _sf_flush_pending_dims();

    err = tpb_driver_add_handle(value);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "--kernel option requires a legal kernel name.\n");
        _sf_print_kernel_list(stderr);
        fprintf(stderr, "\n");
        tpbcli_default_help(node, stderr);
        return err;
    }

    snprintf(g_pending_kernel_name, TPBM_NAME_STR_MAX_LEN, "%s", value);
    g_kernel_parsed_for_help = 1;

    return 0;
}

static int
_sf_parse_kargs(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    return tpb_argp_set_kargs_tokstr((int)strlen(value), (char *)value, NULL);
}

static int
_sf_parse_kargs_dim(tpbcli_argnode_t *node, const char *value)
{
    tpb_dim_config_t *cfg = NULL;
    int err;

    (void)node;

    if (g_n_pending_dims >= MAX_DIM_CONFIGS) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Too many --kargs-dim options (max %d)\n",
                   MAX_DIM_CONFIGS);
        return TPBE_CLI_FAIL;
    }

    err = tpb_argp_parse_dim(value, &cfg);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Failed to parse --kargs-dim: %s\n", value);
        return err;
    }

    g_pending_dim_cfgs[g_n_pending_dims++] = cfg;
    return 0;
}

static int
_sf_parse_kenvs(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    return parse_kenvs_tokstr(value);
}

static int
_sf_parse_kenvs_dim(tpbcli_argnode_t *node, const char *value)
{
    tpb_dim_config_t *cfg = NULL;
    int err;

    (void)node;

    err = tpb_argp_parse_dim(value, &cfg);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Failed to parse --kenvs-dim: %s\n", value);
        return err;
    }

    err = expand_env_dim_handles(cfg, g_pending_kernel_name);
    tpb_dim_config_free(cfg);
    return err;
}

static int
_sf_parse_kmpiargs(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    return parse_kmpiargs_quoted(value);
}

static int
_sf_parse_kmpiargs_dim(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    return expand_kmpiargs_dim(value, g_pending_kernel_name);
}

static int
_sf_parse_timer(tpbcli_argnode_t *node, const char *value)
{
    int err;

    (void)node;

    err = tpb_argp_set_timer(value);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid timer: %s\n", value);
    }
    return err;
}

static int
_sf_parse_outargs(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    return parse_outargs_string(value);
}

static int
_sf_parse_dry_run(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    g_dry_run = 1;
    return 0;
}

int
tpbcli_run(int argc, char **argv)
{
    int err = 0;
    tpbcli_argtree_t *tree = NULL;
    tpbcli_argnode_t *root;
    tpbcli_argnode_t *run_cmd;
    tpbcli_argnode_t *kernel;
    int nhdl;
    int rec_err;
    int i;

    g_dry_run = 0;
    g_kernel_parsed_for_help = 0;
    g_n_pending_dims = 0;
    g_pending_kernel_name[0] = '\0';

    err = tpb_argp_set_timer("clock_gettime");
    if (err != 0) {
        return err;
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_register_kernel");

    tree = tpbcli_argtree_create(
        (argc > 0 && argv[0] != NULL) ? argv[0] : "tpbcli",
        "Run benchmark kernels");
    if (tree == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    root = &tree->root;

    run_cmd = tpbcli_add_arg(root, &(tpbcli_argconf_t){
        .name = "run",
        .short_name = "r",
        .desc = "Run kernels (this subcommand)",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    if (run_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return TPBE_MALLOC_FAIL;
    }

    kernel = tpbcli_add_arg(run_cmd, &(tpbcli_argconf_t){
        .name = "--kernel",
        .short_name = "-k",
        .desc = "Kernel to run",
        .type = TPBCLI_ARG_OPT,
        .flags = TPBCLI_ARGF_MANDATORY,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kernel,
    });
    if (kernel == NULL) {
        tpbcli_argtree_destroy(tree);
        return TPBE_MALLOC_FAIL;
    }

    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--kargs",
        .desc = "Kernel arguments (key=val,...)",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kargs,
    });
    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--kargs-dim",
        .desc = "Dimension sweep for kernel args",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kargs_dim,
    });
    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--kenvs",
        .desc = "Environment variables (KEY=VAL,...)",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kenvs,
    });
    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--kenvs-dim",
        .desc = "Dimension sweep for env vars",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kenvs_dim,
    });
    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--kmpiargs",
        .desc = "MPI runtime arguments",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kmpiargs,
    });
    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--kmpiargs-dim",
        .desc = "Dimension sweep for MPI args",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = -1,
        .parse_fn = _sf_parse_kmpiargs_dim,
    });
    tpbcli_add_arg(kernel, &(tpbcli_argconf_t){
        .name = "--help",
        .short_name = "-h",
        .desc = "Show kernel options help",
        .type = TPBCLI_ARG_FLAG,
        .max_chosen = 0,
        .help_fn = _sf_help_kernel_children,
    });

    tpbcli_add_arg(run_cmd, &(tpbcli_argconf_t){
        .name = "--timer",
        .desc = "Timer backend",
        .type = TPBCLI_ARG_OPT,
        .flags = TPBCLI_ARGF_PRESET,
        .max_chosen = 1,
        .preset_value = "clock_gettime",
        .parse_fn = _sf_parse_timer,
    });
    tpbcli_add_arg(run_cmd, &(tpbcli_argconf_t){
        .name = "--outargs",
        .desc = "Output format (unit_cast,sigbit_trim)",
        .type = TPBCLI_ARG_OPT,
        .max_chosen = 1,
        .parse_fn = _sf_parse_outargs,
    });
    tpbcli_add_arg(run_cmd, &(tpbcli_argconf_t){
        .name = "--dry-run",
        .short_name = "-d",
        .desc = "Validate args, print commands, skip execution",
        .type = TPBCLI_ARG_FLAG,
        .max_chosen = 1,
        .parse_fn = _sf_parse_dry_run,
    });
    tpbcli_add_arg(run_cmd, &(tpbcli_argconf_t){
        .name = "--help",
        .short_name = "-h",
        .desc = "Show help for this command",
        .type = TPBCLI_ARG_FLAG,
        .max_chosen = 0,
        .help_fn = tpbcli_default_help,
    });

    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);

    if (err != TPBE_SUCCESS) {
        for (i = 0; i < g_n_pending_dims; i++) {
            tpb_dim_config_free(g_pending_dim_cfgs[i]);
            g_pending_dim_cfgs[i] = NULL;
        }
        g_n_pending_dims = 0;
        return err;
    }

    _sf_flush_pending_dims();

    nhdl = tpb_get_nhdl();
    if (nhdl > 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "Number of kernels to run: %d\n", nhdl);
    }

    if (g_dry_run) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "=== DRY RUN MODE ===\n");
        tpb_driver_set_dry_run(1);
        err = tpb_driver_run_all();
        tpb_driver_set_dry_run(0);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "=== DRY RUN COMPLETE ===\n");
        g_dry_run = 0;
        return err;
    }

    usleep((useconds_t)(1000000 + (rand() % 1000) * 1000));

    rec_err = tpb_record_begin_batch(TPB_BATCH_TYPE_RUN);
    if (rec_err) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                   "tpbcli_run: begin_batch failed (%d), "
                   "continuing without recording.\n", rec_err);
    }

    err = tpb_driver_run_all();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_driver_run_all");

    if (!rec_err) {
        rec_err = tpb_record_end_batch(nhdl);
        if (rec_err) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "tpbcli_run: end_batch failed (%d)\n", rec_err);
        }
    }

    return err;
}