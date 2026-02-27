/*
 * tpbcli-run-dim.c
 * TPBench dimension argument parsing implementation.
 * Provides parsing and value generation for variable parameter sequences.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "tpbcli-run-dim.h"
#include "corelib/tpb-io.h"

/* Local Helper Functions */

/**
 * @brief Trim leading and trailing whitespace from a string.
 * @param str String to trim (modified in place).
 * @return Pointer to trimmed string (may be offset into original).
 */
static char *
trim_whitespace_dim(char *str)
{
    char *end;

    if (str == NULL) {
        return NULL;
    }

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return str;
}

/**
 * @brief Find matching closing bracket/brace.
 * @param str String starting after opening bracket.
 * @param open Opening character ('(' or '[' or '{').
 * @param close Closing character (')' or ']' or '}').
 * @return Pointer to closing bracket, or NULL if not found.
 */
static char *
find_matching_bracket(const char *str, char open, char close)
{
    int depth = 1;
    const char *p = str;

    while (*p && depth > 0) {
        if (*p == open) {
            depth++;
        } else if (*p == close) {
            depth--;
        }
        if (depth > 0) {
            p++;
        }
    }

    return (depth == 0) ? (char *)p : NULL;
}

/**
 * @brief Check if a string represents a numeric value.
 * @param str String to check.
 * @return 1 if numeric, 0 otherwise.
 */
static int
is_numeric_string(const char *str)
{
    char *endptr;

    if (str == NULL || *str == '\0') {
        return 0;
    }

    strtod(str, &endptr);
    while (*endptr && isspace((unsigned char)*endptr)) {
        endptr++;
    }

    return (*endptr == '\0');
}

/* Explicit List Parser */

int
tpb_argp_parse_list(const char *spec, tpb_dim_config_t *cfg)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *p;
    char *end;
    char *saveptr;
    char *tok;
    int count = 0;
    int cap = 16;
    char **str_vals = NULL;
    double *num_vals = NULL;
    int is_string = 0;

    if (spec == NULL || cfg == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* spec should be: [a, b, c, ...] */
    snprintf(buf, sizeof(buf), "%s", spec);
    p = trim_whitespace_dim(buf);

    /* Check for opening bracket */
    if (*p != '[') {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid list sequence: expected '[' at start of '%s'\n", spec);
        return TPBE_CLI_FAIL;
    }
    p++;

    /* Find closing bracket */
    end = find_matching_bracket(p, '[', ']');
    if (end == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid list sequence: missing ']' in '%s'\n", spec);
        return TPBE_CLI_FAIL;
    }
    *end = '\0';

    /* Allocate initial arrays */
    str_vals = (char **)malloc(sizeof(char *) * cap);
    num_vals = (double *)malloc(sizeof(double) * cap);
    if (str_vals == NULL || num_vals == NULL) {
        free(str_vals);
        free(num_vals);
        return TPBE_MALLOC_FAIL;
    }

    /* Parse comma-separated values */
    tok = strtok_r(p, ",", &saveptr);
    while (tok != NULL) {
        char *trimmed = trim_whitespace_dim(tok);

        if (count >= cap) {
            cap *= 2;
            char **new_str = (char **)realloc(str_vals, sizeof(char *) * cap);
            double *new_num = (double *)realloc(num_vals, sizeof(double) * cap);
            if (new_str == NULL || new_num == NULL) {
                for (int i = 0; i < count; i++) {
                    free(str_vals[i]);
                }
                free(str_vals);
                free(num_vals);
                return TPBE_MALLOC_FAIL;
            }
            str_vals = new_str;
            num_vals = new_num;
        }

        str_vals[count] = strdup(trimmed);
        if (str_vals[count] == NULL) {
            for (int i = 0; i < count; i++) {
                free(str_vals[i]);
            }
            free(str_vals);
            free(num_vals);
            return TPBE_MALLOC_FAIL;
        }

        /* Check if numeric */
        if (!is_numeric_string(trimmed)) {
            is_string = 1;
        }
        num_vals[count] = strtod(trimmed, NULL);

        count++;
        tok = strtok_r(NULL, ",", &saveptr);
    }

    if (count == 0) {
        free(str_vals);
        free(num_vals);
        tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid list sequence: empty list\n");
        return TPBE_CLI_FAIL;
    }

    cfg->type = TPB_DIM_LIST;
    cfg->spec.list.n = count;
    cfg->spec.list.str_values = str_vals;
    cfg->spec.list.values = num_vals;
    cfg->spec.list.is_string = is_string;

    return 0;
}

/* Recursive Sequence Parser */

int
tpb_argp_parse_dim_recur(const char *spec, tpb_dim_config_t *cfg)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *p;
    char *op_end;
    char *params_start;
    char *params_end;
    char op_name[16];
    tpb_dim_op_t op;
    double x, st, min, max;
    int nlim;

    if (spec == NULL || cfg == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* spec should be: <op>(@,x)(st,min,max,nlim) */
    snprintf(buf, sizeof(buf), "%s", spec);
    p = trim_whitespace_dim(buf);

    /* Find operator name (before first '(') */
    op_end = strchr(p, '(');
    if (op_end == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing '(' in '%s'\n", spec);
        return TPBE_CLI_FAIL;
    }

    size_t op_len = op_end - p;
    if (op_len >= sizeof(op_name)) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: operator name too long\n");
        return TPBE_CLI_FAIL;
    }
    strncpy(op_name, p, op_len);
    op_name[op_len] = '\0';

    /* Parse operator */
    if (strcmp(op_name, "add") == 0) {
        op = TPB_DIM_OP_ADD;
    } else if (strcmp(op_name, "sub") == 0) {
        op = TPB_DIM_OP_SUB;
    } else if (strcmp(op_name, "mul") == 0) {
        op = TPB_DIM_OP_MUL;
    } else if (strcmp(op_name, "div") == 0) {
        op = TPB_DIM_OP_DIV;
    } else if (strcmp(op_name, "pow") == 0) {
        op = TPB_DIM_OP_POW;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: unknown operator '%s'\n", op_name);
        return TPBE_CLI_FAIL;
    }

    /* Parse (@, x) */
    p = op_end + 1;  /* Skip '(' */
    char *first_paren_end = find_matching_bracket(p, '(', ')');
    if (first_paren_end == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing ')' for operator params\n");
        return TPBE_CLI_FAIL;
    }
    *first_paren_end = '\0';

    /* Parse @,x - find the comma */
    char *comma = strchr(p, ',');
    if (comma == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: expected '@,x' format\n");
        return TPBE_CLI_FAIL;
    }

    /* Verify @ symbol */
    char *at_part = p;
    *comma = '\0';
    at_part = trim_whitespace_dim(at_part);
    if (strcmp(at_part, "@") != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: expected '@' as first operand, got '%s'\n",
                   at_part);
        return TPBE_CLI_FAIL;
    }

    /* Parse x value */
    char *x_part = trim_whitespace_dim(comma + 1);
    x = strtod(x_part, NULL);

    /* Parse (st, min, max, nlim) */
    params_start = first_paren_end + 1;
    params_start = trim_whitespace_dim(params_start);
    if (*params_start != '(') {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: expected '(' for range params\n");
        return TPBE_CLI_FAIL;
    }
    params_start++;

    params_end = find_matching_bracket(params_start, '(', ')');
    if (params_end == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing ')' for range params\n");
        return TPBE_CLI_FAIL;
    }
    *params_end = '\0';

    /* Parse st, min, max, nlim */
    char *saveptr;
    char *tok;

    tok = strtok_r(params_start, ",", &saveptr);
    if (tok == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing start value\n");
        return TPBE_CLI_FAIL;
    }
    st = strtod(trim_whitespace_dim(tok), NULL);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing min value\n");
        return TPBE_CLI_FAIL;
    }
    min = strtod(trim_whitespace_dim(tok), NULL);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing max value\n");
        return TPBE_CLI_FAIL;
    }
    max = strtod(trim_whitespace_dim(tok), NULL);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: missing nlim value\n");
        return TPBE_CLI_FAIL;
    }
    nlim = (int)strtol(trim_whitespace_dim(tok), NULL, 10);

    /* Validate */
    if (min > max) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: min (%g) > max (%g)\n", min, max);
        return TPBE_CLI_FAIL;
    }

    if (st < min || st > max) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid recursive sequence: start (%g) not in [%g, %g]\n",
                   st, min, max);
        return TPBE_CLI_FAIL;
    }

    cfg->type = TPB_DIM_RECUR;
    cfg->spec.recur.op = op;
    cfg->spec.recur.x = x;
    cfg->spec.recur.st = st;
    cfg->spec.recur.min = min;
    cfg->spec.recur.max = max;
    cfg->spec.recur.nlim = nlim;

    return 0;
}

/* Nested Sequence Parser */

int
tpb_argp_parse_dim_nest(const char *spec, tpb_dim_config_t *cfg)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *p;
    char *brace_start;
    char *brace_end;
    char *outer_spec;
    char *nested_spec;
    int err;

    if (spec == NULL || cfg == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    snprintf(buf, sizeof(buf), "%s", spec);
    p = buf;

    /* Find opening brace for nested part */
    brace_start = strchr(p, '{');
    if (brace_start == NULL) {
        /* No nesting, parse as regular dimension */
        return tpb_argp_parse_dim(spec, &cfg);
    }

    /* Extract outer specification (before '{') */
    *brace_start = '\0';
    outer_spec = trim_whitespace_dim(p);

    /* Find matching closing brace */
    brace_end = find_matching_bracket(brace_start + 1, '{', '}');
    if (brace_end == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid nested sequence: missing '}' in '%s'\n", spec);
        return TPBE_CLI_FAIL;
    }
    *brace_end = '\0';
    nested_spec = brace_start + 1;

    /* Parse outer dimension */
    tpb_dim_config_t *outer_cfg = NULL;
    err = tpb_argp_parse_dim(outer_spec, &outer_cfg);
    if (err != 0) {
        return err;
    }

    /* Copy outer config to cfg */
    memcpy(cfg, outer_cfg, sizeof(tpb_dim_config_t));
    cfg->nested = NULL;
    free(outer_cfg);

    /* Parse nested dimension recursively */
    tpb_dim_config_t *nested_cfg = (tpb_dim_config_t *)malloc(sizeof(tpb_dim_config_t));
    if (nested_cfg == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    memset(nested_cfg, 0, sizeof(tpb_dim_config_t));

    /* Check if nested part also has nesting */
    if (strchr(nested_spec, '{') != NULL) {
        err = tpb_argp_parse_dim_nest(nested_spec, nested_cfg);
    } else {
        tpb_dim_config_t *inner = NULL;
        err = tpb_argp_parse_dim(nested_spec, &inner);
        if (err == 0 && inner != NULL) {
            memcpy(nested_cfg, inner, sizeof(tpb_dim_config_t));
            free(inner);
        }
    }

    if (err != 0) {
        free(nested_cfg);
        return err;
    }

    cfg->nested = nested_cfg;

    return 0;
}

/* Main Entry Point */

int
tpb_argp_parse_dim(const char *argstr, tpb_dim_config_t **cfg)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *p;
    char *eq;
    char *spec;
    int err;

    if (argstr == NULL || cfg == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Allocate config */
    *cfg = (tpb_dim_config_t *)malloc(sizeof(tpb_dim_config_t));
    if (*cfg == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    memset(*cfg, 0, sizeof(tpb_dim_config_t));

    snprintf(buf, sizeof(buf), "%s", argstr);
    p = trim_whitespace_dim(buf);

    /* Check for nested sequence first (contains '{' before '=') */
    char *brace = strchr(p, '{');
    eq = strchr(p, '=');

    if (brace != NULL && (eq == NULL || brace < eq)) {
        /* Nested sequence without '=' before '{' - unusual but handle it */
        err = tpb_argp_parse_dim_nest(p, *cfg);
        if (err != 0) {
            free(*cfg);
            *cfg = NULL;
        }
        return err;
    }

    /* Find '=' to separate parameter name from specification */
    if (eq == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid dimension arg: expected 'parm_name=...' format in '%s'\n",
                   argstr);
        free(*cfg);
        *cfg = NULL;
        return TPBE_CLI_FAIL;
    }

    *eq = '\0';
    char *parm_name = trim_whitespace_dim(p);
    spec = trim_whitespace_dim(eq + 1);

    /* Store parameter name */
    snprintf((*cfg)->parm_name, TPBM_NAME_STR_MAX_LEN, "%s", parm_name);

    /* Check for nested sequence (has '{' in spec) */
    brace = strchr(spec, '{');
    if (brace != NULL) {
        /* Re-parse with full string for nested handling */
        char nested_buf[TPBM_CLI_STR_MAX_LEN];
        snprintf(nested_buf, sizeof(nested_buf), "%s=%s", parm_name, spec);
        err = tpb_argp_parse_dim_nest(nested_buf, *cfg);
        if (err != 0) {
            free(*cfg);
            *cfg = NULL;
        }
        return err;
    }

    /* Detect sequence type based on first character */
    if (*spec == '(') {
        /* Linear sequence format has been removed */
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Error: Linear sequence format (st,en,step) has been removed.\n"
                   "Use recursive format instead: add(@,<step>)(<st>,<st>,<en>,<nlim>)\n"
                   "Example: total_memsize=(128,512,128) -> total_memsize=add(@,128)(128,128,512,4)\n");
        err = TPBE_CLI_FAIL;
    } else if (*spec == '[') {
        /* Explicit list: [a, b, c, ...] */
        err = tpb_argp_parse_list(spec, *cfg);
    } else if (isalpha((unsigned char)*spec)) {
        /* Recursive sequence: op(@,x)(st,min,max,nlim) */
        err = tpb_argp_parse_dim_recur(spec, *cfg);
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid dimension arg: unknown format '%s'\n", spec);
        err = TPBE_CLI_FAIL;
    }

    if (err != 0) {
        free(*cfg);
        *cfg = NULL;
    }

    return err;
}

/* Value Generation */

int
tpb_dim_generate_values(tpb_dim_config_t *cfg, tpb_dim_values_t **values)
{
    tpb_dim_values_t *val;
    int n = 0;
    int err;

    if (cfg == NULL || values == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    val = (tpb_dim_values_t *)malloc(sizeof(tpb_dim_values_t));
    if (val == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    memset(val, 0, sizeof(tpb_dim_values_t));

    snprintf(val->parm_name, TPBM_NAME_STR_MAX_LEN, "%s", cfg->parm_name);

    switch (cfg->type) {
    case TPB_DIM_LIST: {
        n = cfg->spec.list.n;

        val->values = (double *)malloc(sizeof(double) * n);
        val->str_values = (char **)malloc(sizeof(char *) * n);
        if (val->values == NULL || val->str_values == NULL) {
            free(val->values);
            free(val->str_values);
            free(val);
            return TPBE_MALLOC_FAIL;
        }

        for (int i = 0; i < n; i++) {
            val->values[i] = cfg->spec.list.values[i];
            val->str_values[i] = strdup(cfg->spec.list.str_values[i]);
            if (val->str_values[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(val->str_values[j]);
                }
                free(val->values);
                free(val->str_values);
                free(val);
                return TPBE_MALLOC_FAIL;
            }
        }

        val->n = n;
        val->is_string = cfg->spec.list.is_string;
        break;
    }

    case TPB_DIM_RECUR: {
        double current = cfg->spec.recur.st;
        double x = cfg->spec.recur.x;
        double min = cfg->spec.recur.min;
        double max = cfg->spec.recur.max;
        int nlim = cfg->spec.recur.nlim;
        tpb_dim_op_t op = cfg->spec.recur.op;

        /* Pre-allocate for maximum possible values */
        int cap = (nlim > 0) ? nlim : TPBM_DIM_MAX_VALUES;
        val->values = (double *)malloc(sizeof(double) * cap);
        if (val->values == NULL) {
            free(val);
            return TPBE_MALLOC_FAIL;
        }

        /* Generate values */
        n = 0;
        while (n < cap) {
            if (current < min || current > max) {
                break;
            }

            val->values[n] = current;
            n++;

            if (nlim > 0 && n >= nlim) {
                break;
            }

            /* Calculate next value */
            switch (op) {
            case TPB_DIM_OP_ADD:
                current = current + x;
                break;
            case TPB_DIM_OP_SUB:
                current = current - x;
                break;
            case TPB_DIM_OP_MUL:
                current = current * x;
                break;
            case TPB_DIM_OP_DIV:
                if (x == 0) {
                    goto recur_done;
                }
                current = current / x;
                break;
            case TPB_DIM_OP_POW:
                current = pow(current, x);
                break;
            }
        }
recur_done:
        val->n = n;
        val->is_string = 0;
        val->str_values = NULL;
        break;
    }

    default:
        free(val);
        return TPBE_CLI_FAIL;
    }

    /* Handle nested dimensions */
    if (cfg->nested != NULL) {
        err = tpb_dim_generate_values(cfg->nested, &val->nested);
        if (err != 0) {
            tpb_dim_values_free(val);
            return err;
        }
    } else {
        val->nested = NULL;
    }

    *values = val;
    return 0;
}

int
tpb_dim_get_total_count(tpb_dim_config_t *cfg)
{
    int count = 0;

    if (cfg == NULL) {
        return 0;
    }

    switch (cfg->type) {
    case TPB_DIM_LIST:
        count = cfg->spec.list.n;
        break;

    case TPB_DIM_RECUR: {
        /* Need to generate to count - use a simplified estimate */
        tpb_dim_values_t *vals = NULL;
        if (tpb_dim_generate_values(cfg, &vals) == 0 && vals != NULL) {
            count = vals->n;
            tpb_dim_values_free(vals);
        } else {
            count = 1;
        }
        break;
    }

    default:
        count = 1;
        break;
    }

    /* Multiply by nested count */
    if (cfg->nested != NULL) {
        int nested_count = tpb_dim_get_total_count(cfg->nested);
        count *= nested_count;
    }

    return count;
}

/* Cleanup Functions */

void
tpb_dim_config_free(tpb_dim_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->type == TPB_DIM_LIST) {
        if (cfg->spec.list.str_values != NULL) {
            for (int i = 0; i < cfg->spec.list.n; i++) {
                free(cfg->spec.list.str_values[i]);
            }
            free(cfg->spec.list.str_values);
        }
        free(cfg->spec.list.values);
    }

    if (cfg->nested != NULL) {
        tpb_dim_config_free(cfg->nested);
    }

    free(cfg);
}

void
tpb_dim_values_free(tpb_dim_values_t *values)
{
    if (values == NULL) {
        return;
    }

    if (values->str_values != NULL) {
        for (int i = 0; i < values->n; i++) {
            free(values->str_values[i]);
        }
        free(values->str_values);
    }

    free(values->values);

    if (values->nested != NULL) {
        tpb_dim_values_free(values->nested);
    }

    free(values);
}
