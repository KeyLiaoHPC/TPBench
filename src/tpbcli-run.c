/*
 * tpbcli-run.c
 * Run subcommand implementation.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
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
#include "corelib/tpb-driver.h"
#include "corelib/tpb-impl.h"
#include "corelib/tpb-io.h"
#include "corelib/tpb-types.h"
#include "kernels/kernels.h"

/* Maximum number of dimension configs per kernel */
#define MAX_DIM_CONFIGS 16

/* Local Function Prototypes */

/* Pre-parse integration mode (-P/-F) before kernel registration */
static void parse_integ_mode(int argc, char **argv);

/* Parse run-specific arguments */
static int parse_run(int argc, char **argv);

/* Parse outargs string and set output formatting options */
static int parse_outargs_string(const char *outargs_str);

/* Expand handles based on dimension configurations */
static int expand_dim_handles(tpb_dim_config_t *dim_cfg, const char *kernel_name);

/* Apply dimension value to current handle */
static int apply_dim_value(tpb_dim_values_t *dim_val, int index);

/* Recursive helper for expanding nested dimensions */
static int expand_nested_dims(tpb_dim_values_t *vals, int *indices, int depth,
                              int max_depth, const char *kernel_name);

/* Parse comma-separated env key=value string and set environment variables */
static int parse_kenvs_tokstr(const char *tokstr);

/* Apply environment dimension value to current handle */
static int apply_env_dim_value(tpb_dim_values_t *dim_val, int index);

/* Expand handles for env dimensions */
static int expand_env_dim_handles(tpb_dim_config_t *dim_cfg, const char *kernel_name);

/* Parse quoted MPI args string and set/append to current handle */
static int parse_kmpiargs_quoted(const char *arg, int is_kernel_specific);

/* Expand handles for --kmpiargs-dim explicit list */
static int expand_kmpiargs_dim(const char *arg, const char *kernel_name);

/* Local Function Implementations */

static void
parse_integ_mode(int argc, char **argv)
{
    /* Default is PLI mode */
    int mode = TPB_INTEG_MODE_PLI;

    /* Scan for -P or -F switches */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) {
            mode = TPB_INTEG_MODE_PLI;
        } else if (strcmp(argv[i], "-F") == 0) {
            mode = TPB_INTEG_MODE_FLI;
        }
    }

    tpb_driver_set_integ_mode(mode);
}

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
parse_run(int argc, char **argv)
{
    int err = 0;
    int nkern_parsed = 0;

    /* Dimension config tracking per kernel */
    tpb_dim_config_t *pending_dim_cfg = NULL;
    char pending_kernel_name[TPBM_NAME_STR_MAX_LEN] = {0};

    /* Set default timer */
    err = tpb_argp_set_timer("clock_gettime");
    if (err != 0) {
        return err;
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) {
            /* Already handled in parse_integ_mode */
            continue;

        } else if (strcmp(argv[i], "-F") == 0) {
            /* Already handled in parse_integ_mode */
            continue;

        } else if (strcmp(argv[i], "--kernel") == 0 || strcmp(argv[i], "-k") == 0) {
            /* Before adding new kernel, expand any pending dimension config */
            if (pending_dim_cfg != NULL && pending_kernel_name[0] != '\0') {
                err = expand_dim_handles(pending_dim_cfg, pending_kernel_name);
                if (err != 0) {
                    tpb_dim_config_free(pending_dim_cfg);
                    return err;
                }
                tpb_dim_config_free(pending_dim_cfg);
                pending_dim_cfg = NULL;
                pending_kernel_name[0] = '\0';
            }

            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to kernel name */

            /* Add handle for this kernel */
            err = tpb_driver_add_handle(argv[i]);
            if (err != 0) {
                return err;
            }

            /* Track current kernel name for potential dim expansion */
            snprintf(pending_kernel_name, TPBM_NAME_STR_MAX_LEN, "%s", argv[i]);
            nkern_parsed++;

        } else if (strcmp(argv[i], "--kargs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to the argument string */

            /* Parse and set the kargs for current handle */
            err = tpb_argp_set_kargs_tokstr((int)strlen(argv[i]), argv[i], NULL);
            if (err != 0) {
                return err;
            }

        } else if (strcmp(argv[i], "--kargs-dim") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to the dimension argument string */

            /* Parse dimension configuration */
            tpb_dim_config_t *new_cfg = NULL;
            err = tpb_argp_parse_dim(argv[i], &new_cfg);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Failed to parse --kargs-dim: %s\n", argv[i]);
                return err;
            }

            /* Chain nested dimensions if there's already a pending config */
            if (pending_dim_cfg != NULL) {
                /* Find the deepest nested config and attach new one */
                tpb_dim_config_t *tail = pending_dim_cfg;
                while (tail->nested != NULL) {
                    tail = tail->nested;
                }
                tail->nested = new_cfg;
            } else {
                pending_dim_cfg = new_cfg;
            }

        } else if (strcmp(argv[i], "--kenvs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to the env argument string */

            /* Parse and set the environment variables for current handle */
            err = parse_kenvs_tokstr(argv[i]);
            if (err != 0) {
                return err;
            }

        } else if (strcmp(argv[i], "--kenvs-dim") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to the dimension argument string */

            /* Parse dimension configuration for env vars */
            tpb_dim_config_t *env_dim_cfg = NULL;
            err = tpb_argp_parse_dim(argv[i], &env_dim_cfg);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Failed to parse --kenvs-dim: %s\n", argv[i]);
                return err;
            }

            /* Expand env dimensions for current kernel */
            if (pending_kernel_name[0] != '\0') {
                err = expand_env_dim_handles(env_dim_cfg, pending_kernel_name);
                if (err != 0) {
                    tpb_dim_config_free(env_dim_cfg);
                    return err;
                }
            }
            tpb_dim_config_free(env_dim_cfg);

        } else if (strcmp(argv[i], "--kmpiargs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;
            /* Parse quoted MPI args string
             * is_kernel_specific = 1 if we're after a --kernel, 0 otherwise */
            int is_kernel_specific = (pending_kernel_name[0] != '\0');
            err = parse_kmpiargs_quoted(argv[i], is_kernel_specific);
            if (err != 0) {
                return err;
            }

        } else if (strcmp(argv[i], "--kmpiargs-dim") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;

            /* Expand kmpiargs-dim for current kernel */
            if (pending_kernel_name[0] != '\0') {
                err = expand_kmpiargs_dim(argv[i], pending_kernel_name);
                if (err != 0) {
                    return err;
                }
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "--kmpiargs-dim must follow a --kernel definition.\n");
                return TPBE_CLI_FAIL;
            }

        } else if (strcmp(argv[i], "--timer") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;
            err = tpb_argp_set_timer(argv[i]);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid timer: %s\n", argv[i]);
                return TPBE_CLI_FAIL;
            }

        } else if (strcmp(argv[i], "--outargs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;
            err = parse_outargs_string(argv[i]);
            if (err != 0) {
                return err;
            }

        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown option %s.\n", argv[i]);
            return TPBE_CLI_FAIL;
        }
    }

    /* Expand any remaining pending dimension config */
    if (pending_dim_cfg != NULL && pending_kernel_name[0] != '\0') {
        err = expand_dim_handles(pending_dim_cfg, pending_kernel_name);
        if (err != 0) {
            tpb_dim_config_free(pending_dim_cfg);
            return err;
        }
        tpb_dim_config_free(pending_dim_cfg);
        pending_dim_cfg = NULL;
    }

    if (nkern_parsed == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "No kernels specified. Use --kernel option.\n");
        return TPBE_CLI_FAIL;
    }

    return 0;
}

/* ============================================================================
 * Dimension Expansion Functions
 * ============================================================================ */

/**
 * @brief Apply a dimension value to the current handle.
 */
static int
apply_dim_value(tpb_dim_values_t *dim_val, int index)
{
    char value_str[256];

    if (dim_val == NULL || index < 0 || index >= dim_val->n) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (dim_val->is_string && dim_val->str_values != NULL) {
        /* Use string value directly */
        return tpb_driver_set_hdl_karg(dim_val->parm_name, dim_val->str_values[index]);
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

/**
 * @brief Count nesting depth of dimension values.
 */
static int
count_dim_depth(tpb_dim_values_t *vals)
{
    int depth = 0;
    tpb_dim_values_t *p = vals;
    while (p != NULL) {
        depth++;
        p = p->nested;
    }
    return depth;
}

/**
 * @brief Get dimension values at specific depth.
 */
static tpb_dim_values_t *
get_dim_at_depth(tpb_dim_values_t *vals, int depth)
{
    tpb_dim_values_t *p = vals;
    for (int i = 0; i < depth && p != NULL; i++) {
        p = p->nested;
    }
    return p;
}

/**
 * @brief Recursive helper for expanding nested dimensions.
 *
 * Creates handles for each combination of dimension values.
 * The first combination modifies the existing handle, subsequent ones create new handles.
 *
 * @param vals       Dimension values structure.
 * @param indices    Array tracking current index at each depth.
 * @param depth      Current recursion depth.
 * @param max_depth  Total nesting depth.
 * @param kernel_name Kernel name for creating new handles.
 * @param is_first   Pointer to flag indicating if this is the first combination.
 */
static int
expand_nested_dims_impl(tpb_dim_values_t *vals, int *indices, int depth,
                        int max_depth, const char *kernel_name, int *is_first)
{
    int err;
    tpb_dim_values_t *current = get_dim_at_depth(vals, depth);

    if (current == NULL) {
        return 0;
    }

    if (depth == max_depth - 1) {
        /* Innermost dimension: create handles for each value */
        for (int i = 0; i < current->n; i++) {
            indices[depth] = i;

            if (*is_first) {
                /* First combination: apply to existing handle (already created by --kernel) */
                *is_first = 0;
            } else {
                /* Subsequent combinations: add a new handle */
                err = tpb_driver_add_handle(kernel_name);
                if (err != 0) {
                    return err;
                }
            }

            /* Apply all dimension values from outer to inner */
            for (int d = 0; d <= depth; d++) {
                tpb_dim_values_t *dim_at_d = get_dim_at_depth(vals, d);
                err = apply_dim_value(dim_at_d, indices[d]);
                if (err != 0) {
                    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                               "Failed to set dimension value for '%s'\n",
                               dim_at_d->parm_name);
                    /* Continue anyway - parameter might not exist for this kernel */
                }
            }
        }
    } else {
        /* Outer dimension: iterate and recurse */
        for (int i = 0; i < current->n; i++) {
            indices[depth] = i;
            err = expand_nested_dims_impl(vals, indices, depth + 1, max_depth,
                                          kernel_name, is_first);
            if (err != 0) {
                return err;
            }
        }
    }

    return 0;
}

/**
 * @brief Wrapper for expand_nested_dims_impl.
 */
static int
expand_nested_dims(tpb_dim_values_t *vals, int *indices, int depth,
                   int max_depth, const char *kernel_name)
{
    int is_first = 1;
    return expand_nested_dims_impl(vals, indices, depth, max_depth, kernel_name, &is_first);
}

/**
 * @brief Expand handles based on dimension configurations.
 *
 * For a kernel with dimension config, this creates multiple handles,
 * one for each combination of dimension values.
 */
static int
expand_dim_handles(tpb_dim_config_t *dim_cfg, const char *kernel_name)
{
    tpb_dim_values_t *dim_vals = NULL;
    int err;
    int total_count;
    int depth;

    if (dim_cfg == NULL || kernel_name == NULL) {
        return 0;  /* No dimension to expand */
    }

    /* Generate values from configuration */
    err = tpb_dim_generate_values(dim_cfg, &dim_vals);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to generate dimension values\n");
        return err;
    }

    total_count = tpb_dim_get_total_count(dim_cfg);
    depth = count_dim_depth(dim_vals);

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d dimension combinations for kernel '%s'\n",
               total_count, kernel_name);

    /* Allocate indices array for recursive expansion */
    int *indices = (int *)calloc(depth, sizeof(int));
    if (indices == NULL) {
        tpb_dim_values_free(dim_vals);
        return TPBE_MALLOC_FAIL;
    }

    /* Expand dimensions recursively */
    err = expand_nested_dims(dim_vals, indices, 0, depth, kernel_name);

    free(indices);
    tpb_dim_values_free(dim_vals);

    return err;
}

/* ============================================================================
 * Environment Variable Functions
 * ============================================================================ */

/**
 * @brief Parse comma-separated env key=value string and set environment variables.
 */
static int
parse_kenvs_tokstr(const char *tokstr)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *saveptr;
    char *token;

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
        int err = tpb_driver_set_hdl_env(key, value);
        if (err != 0) {
            return err;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

/**
 * @brief Apply environment dimension value to current handle.
 */
static int
apply_env_dim_value(tpb_dim_values_t *dim_val, int index)
{
    char value_str[TPBM_CLI_STR_MAX_LEN];

    if (dim_val == NULL || index < 0 || index >= dim_val->n) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (dim_val->is_string && dim_val->str_values != NULL) {
        /* Use string value directly */
        return tpb_driver_set_hdl_env(dim_val->parm_name, dim_val->str_values[index]);
    } else {
        /* Convert numeric value to string */
        double val = dim_val->values[index];
        if (val == (int64_t)val) {
            snprintf(value_str, sizeof(value_str), "%" PRId64, (int64_t)val);
        } else {
            snprintf(value_str, sizeof(value_str), "%.15g", val);
        }
        return tpb_driver_set_hdl_env(dim_val->parm_name, value_str);
    }
}

/**
 * @brief Recursive helper for expanding env dimensions.
 *
 * @param orig_hdl_idx Index of the original handle to copy kargs from.
 */
static int
expand_env_nested_dims_impl(tpb_dim_values_t *vals, int *indices, int depth,
                            int max_depth, const char *kernel_name,
                            int *is_first, int orig_hdl_idx)
{
    int err;
    tpb_dim_values_t *current = get_dim_at_depth(vals, depth);

    if (current == NULL) {
        return 0;
    }

    if (depth == max_depth - 1) {
        /* Innermost dimension: create handles for each value */
        for (int i = 0; i < current->n; i++) {
            indices[depth] = i;

            if (*is_first) {
                /* First combination: apply to existing handle */
                *is_first = 0;
            } else {
                /* Subsequent combinations: add a new handle and copy kargs */
                err = tpb_driver_add_handle(kernel_name);
                if (err != 0) {
                    return err;
                }
                /* Copy kargs and envs from original handle */
                err = tpb_driver_copy_hdl_from(orig_hdl_idx);
                if (err != 0) {
                    return err;
                }
            }

            /* Apply all env dimension values from outer to inner */
            for (int d = 0; d <= depth; d++) {
                tpb_dim_values_t *dim_at_d = get_dim_at_depth(vals, d);
                err = apply_env_dim_value(dim_at_d, indices[d]);
                if (err != 0) {
                    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                               "Failed to set env dimension value for '%s'\n",
                               dim_at_d->parm_name);
                }
            }
        }
    } else {
        /* Outer dimension: iterate and recurse */
        for (int i = 0; i < current->n; i++) {
            indices[depth] = i;
            err = expand_env_nested_dims_impl(vals, indices, depth + 1, max_depth,
                                              kernel_name, is_first, orig_hdl_idx);
            if (err != 0) {
                return err;
            }
        }
    }

    return 0;
}

/**
 * @brief Expand handles for env dimensions.
 */
static int
expand_env_dim_handles(tpb_dim_config_t *dim_cfg, const char *kernel_name)
{
    tpb_dim_values_t *dim_vals = NULL;
    int err;
    int total_count;
    int depth;

    if (dim_cfg == NULL || kernel_name == NULL) {
        return 0;
    }

    /* Generate values from configuration */
    err = tpb_dim_generate_values(dim_cfg, &dim_vals);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to generate env dimension values\n");
        return err;
    }

    total_count = tpb_dim_get_total_count(dim_cfg);
    depth = count_dim_depth(dim_vals);

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d env dimension combinations for kernel '%s'\n",
               total_count, kernel_name);

    /* Allocate indices array for recursive expansion */
    int *indices = (int *)calloc(depth, sizeof(int));
    if (indices == NULL) {
        tpb_dim_values_free(dim_vals);
        return TPBE_MALLOC_FAIL;
    }

    /* Save original handle index before expansion */
    int orig_hdl_idx = tpb_driver_get_current_hdl_idx();

    /* Expand env dimensions recursively */
    int is_first = 1;
    err = expand_env_nested_dims_impl(dim_vals, indices, 0, depth, kernel_name,
                                      &is_first, orig_hdl_idx);

    free(indices);
    tpb_dim_values_free(dim_vals);

    return err;
}

/**
 * @brief Parse MPI args string and set/append to current handle.
 *
 * The argument can be enclosed in single or double quotes (quotes will be stripped),
 * or unquoted (used as-is). The shell typically consumes outer quotes, so both
 * forms should work.
 *
 * If is_kernel_specific is true, replaces any existing mpiargs.
 * Otherwise, appends to existing mpiargs with a space separator.
 */
static int
parse_kmpiargs_quoted(const char *arg, int is_kernel_specific)
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

    int err;
    if (is_kernel_specific) {
        /* Kernel-specific: replace existing mpiargs */
        err = tpb_driver_set_hdl_mpiargs(content);
    } else {
        /* Common/default: append to existing mpiargs */
        err = tpb_driver_append_hdl_mpiargs(content);
    }

    if (allocated_content != NULL) {
        free(allocated_content);
    }
    return err;
}

/**
 * @brief Parse a single quoted item from a list, returning the content.
 * Advances *pos past the closing quote and any trailing whitespace/comma.
 * Returns NULL if no valid quoted item found.
 */
static char *
parse_kmpiargs_list_item(const char *str, const char **pos)
{
    /* Skip leading whitespace */
    while (**pos == ' ' || **pos == '\t' || **pos == ',') {
        (*pos)++;
    }

    if (**pos == '\0' || **pos == ']' || **pos == '{') {
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

/**
 * @brief Parse explicit list of quoted items: ['opt1', 'opt2', ...]
 * Returns array of strings and count.
 */
static int
parse_kmpiargs_list(const char *str, const char **pos, char ***out_items, int *out_count)
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
            /* Invalid character */
            for (int i = 0; i < count; i++) free(items[i]);
            free(items);
            return TPBE_CLI_FAIL;
        }

        if (count >= capacity) {
            capacity *= 2;
            char **new_items = (char **)realloc(items, sizeof(char *) * capacity);
            if (new_items == NULL) {
                free(item);
                for (int i = 0; i < count; i++) free(items[i]);
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

/**
 * @brief Expand handles for --kmpiargs-dim explicit list.
 *
 * Syntax: ['opt1', 'opt2', ...]{['opta', 'optb', ...]}
 *
 * Generates combinations: base + opt1 + opta, base + opt1 + optb, ...
 */
static int
expand_kmpiargs_dim(const char *arg, const char *kernel_name)
{
    if (arg == NULL || kernel_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    const char *pos = arg;
    char **outer_items = NULL;
    int outer_count = 0;
    char **inner_items = NULL;
    int inner_count = 0;
    int err;

    /* Parse outer list */
    err = parse_kmpiargs_list(arg, &pos, &outer_items, &outer_count);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Failed to parse --kmpiargs-dim list: %s\n", arg);
        return err;
    }

    if (outer_count == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "--kmpiargs-dim: empty list.\n");
        free(outer_items);
        return TPBE_CLI_FAIL;
    }

    /* Check for nested list */
    while (*pos == ' ' || *pos == '\t') {
        pos++;
    }

    if (*pos == '{') {
        pos++;  /* Skip '{' */
        err = parse_kmpiargs_list(arg, &pos, &inner_items, &inner_count);
        if (err != 0) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Failed to parse nested --kmpiargs-dim list.\n");
            for (int i = 0; i < outer_count; i++) free(outer_items[i]);
            free(outer_items);
            return err;
        }
        /* Skip closing '}' */
        while (*pos == ' ' || *pos == '\t') pos++;
        if (*pos == '}') pos++;
    }

    /* Calculate total combinations */
    int total = outer_count * (inner_count > 0 ? inner_count : 1);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Expanding %d kmpiargs-dim combinations for kernel '%s'\n",
               total, kernel_name);

    /* Get original handle index and save base mpiargs */
    int orig_hdl_idx = tpb_driver_get_current_hdl_idx();
    const char *base_mpiargs = tpb_driver_get_hdl_mpiargs();
    char *saved_base = (base_mpiargs != NULL) ? strdup(base_mpiargs) : NULL;
    int is_first = 1;

    /* Generate all combinations */
    for (int i = 0; i < outer_count; i++) {
        int inner_max = (inner_count > 0) ? inner_count : 1;
        for (int j = 0; j < inner_max; j++) {
            if (is_first) {
                is_first = 0;
            } else {
                /* Add new handle and copy from original */
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
            }

            /* Reset mpiargs to base value (before dimension expansion) */
            if (saved_base != NULL) {
                err = tpb_driver_set_hdl_mpiargs(saved_base);
            } else {
                err = tpb_driver_set_hdl_mpiargs("");
            }
            if (err != 0) {
                free(saved_base);
                goto cleanup;
            }

            /* Build combined mpiargs: base + outer[i] + " " + inner[j] */
            if (inner_count > 0) {
                size_t len = strlen(outer_items[i]) + 1 + strlen(inner_items[j]) + 1;
                char *combined = (char *)malloc(len);
                if (combined == NULL) {
                    err = TPBE_MALLOC_FAIL;
                    free(saved_base);
                    goto cleanup;
                }
                sprintf(combined, "%s %s", outer_items[i], inner_items[j]);
                err = tpb_driver_append_hdl_mpiargs(combined);
                free(combined);
            } else {
                err = tpb_driver_append_hdl_mpiargs(outer_items[i]);
            }

            if (err != 0) {
                free(saved_base);
                goto cleanup;
            }
        }
    }

    free(saved_base);
    err = 0;

cleanup:
    for (int i = 0; i < outer_count; i++) free(outer_items[i]);
    free(outer_items);
    for (int i = 0; i < inner_count; i++) free(inner_items[i]);
    free(inner_items);

    return err;
}

/* Public Function Implementations */

int
tpbcli_run(int argc, char **argv)
{
    int err = 0;

    if (argc <= 1) {
        /* No args */
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "No arguments provided, exit after printing help message.\n");
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    /* Parse integration mode BEFORE registering kernels */
    parse_integ_mode(argc, argv);

    /* Print integration mode */
    int mode = tpb_driver_get_integ_mode();
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "TPBench kernel integration mode: %s\n",
               mode == TPB_INTEG_MODE_PLI ? "PLI" : "FLI");

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_register_kernel");

    /* For FLI mode, register the statically linked kernels */
    if (mode == TPB_INTEG_MODE_FLI) {
        tpb_driver_enable_kernel_reg();
        err = register_triad();
        __tpbm_exit_on_error(err, "At tpbcli-run.c: register_triad");
        err = register_stream();
        __tpbm_exit_on_error(err, "At tpbcli-run.c: register_stream");
        tpb_driver_disable_kernel_reg();
    }

    err = parse_run(argc, argv);
    if (err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    __tpbm_exit_on_error(err, "At tpbcli-run.c: parse_run");

    /* Print kernels to run */
    int nhdl = tpb_driver_get_nhdl();
    if (nhdl > 1) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Number of kernels to run: %d\n", nhdl - 1);
    }

    /* Run all handles using driver */
    err = tpb_driver_run_all();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_driver_run_all");

    return err;
}
