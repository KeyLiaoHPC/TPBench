/**
 * @file tpb-argp-dim.h
 * @brief Header for TPBench dimension argument parsing.
 *
 * This module provides utilities for parsing and generating variable
 * parameter sequences for kernel evaluation. Supports linear sequences,
 * explicit lists, recursive sequences, and nested combinations.
 */

#ifndef TPB_ARGP_DIM_H
#define TPB_ARGP_DIM_H

#include "tpb-types.h"

/** Maximum number of values in a dimension sequence */
#define TPBM_DIM_MAX_VALUES 4096

/** Maximum nesting depth for nested sequences */
#define TPBM_DIM_MAX_NEST_DEPTH 8

/**
 * @brief Dimension sequence types.
 */
typedef enum {
    TPB_DIM_LINEAR,    /**< Linear sequence: (st, en, step) */
    TPB_DIM_LIST,      /**< Explicit list: [a, b, c, ...] */
    TPB_DIM_RECUR,     /**< Recursive sequence: op(@,x)(st,min,max,nlim) */
} tpb_dim_type_t;

/**
 * @brief Recursive operators for TPB_DIM_RECUR type.
 */
typedef enum {
    TPB_DIM_OP_ADD,    /**< Addition: @ + x */
    TPB_DIM_OP_SUB,    /**< Subtraction: @ - x */
    TPB_DIM_OP_MUL,    /**< Multiplication: @ * x */
    TPB_DIM_OP_DIV,    /**< Division: @ / x */
    TPB_DIM_OP_POW,    /**< Power: @ ^ x */
} tpb_dim_op_t;

/**
 * @brief Dimension configuration structure.
 *
 * Stores parsed dimension specification including parameter name,
 * sequence type, type-specific parameters, and optional nested dimensions.
 */
typedef struct tpb_dim_config {
    char parm_name[TPBM_NAME_STR_MAX_LEN];  /**< Parameter name */
    tpb_dim_type_t type;                     /**< Sequence type */

    union {
        /** Linear sequence parameters: start, end, step */
        struct {
            double st;
            double en;
            double step;
        } linear;

        /** Explicit list parameters */
        struct {
            int n;              /**< Number of values */
            char **str_values;  /**< String values (for non-numeric params) */
            double *values;     /**< Numeric values */
            int is_string;      /**< Flag: 1 if string list, 0 if numeric */
        } list;

        /** Recursive sequence parameters */
        struct {
            tpb_dim_op_t op;    /**< Recursive operator */
            double x;           /**< Operand value */
            double st;          /**< Start value */
            double min;         /**< Minimum bound */
            double max;         /**< Maximum bound */
            int nlim;           /**< Maximum recursion steps (0 = unlimited) */
        } recur;
    } spec;

    struct tpb_dim_config *nested;  /**< Pointer to nested dimension config */
} tpb_dim_config_t;

/**
 * @brief Generated dimension values structure.
 *
 * Holds the expanded array of values for a dimension, used during
 * handle expansion.
 */
typedef struct tpb_dim_values {
    char parm_name[TPBM_NAME_STR_MAX_LEN];  /**< Parameter name */
    int n;                                   /**< Number of values */
    char **str_values;                       /**< String values (if applicable) */
    double *values;                          /**< Numeric values */
    int is_string;                           /**< Flag: 1 if string, 0 if numeric */
    struct tpb_dim_values *nested;           /**< Nested dimension values */
} tpb_dim_values_t;

/* ============================================================================
 * Parsing Functions
 * ============================================================================ */

/**
 * @brief Parse a linear sequence specification.
 *
 * Parses format: <parm_name>=(st,en,step)
 * Example: memsize=(128,512,128) generates [128, 256, 384, 512]
 *
 * @param spec Input specification string (after the '=' for linear).
 * @param cfg  Output configuration structure.
 * @return 0 on success, error code on failure.
 */
int tpb_argp_parse_dim_lin(const char *spec, tpb_dim_config_t *cfg);

/**
 * @brief Parse an explicit list specification.
 *
 * Parses format: <parm_name>=[a, b, c, ...]
 * Example: dtype=[double,float,iso-fp16]
 *
 * @param spec Input specification string (after the '=' for list).
 * @param cfg  Output configuration structure.
 * @return 0 on success, error code on failure.
 */
int tpb_argp_parse_list(const char *spec, tpb_dim_config_t *cfg);

/**
 * @brief Parse a recursive sequence specification.
 *
 * Parses format: <parm_name>=<op>(@,x)(st,min,max,nlim)
 * Operators: add, sub, mul, div, pow
 * Example: memsize=mul(@,2)(16,16,128,0) generates [16, 32, 64, 128]
 *
 * @param spec Input specification string (after the '=' for recur).
 * @param cfg  Output configuration structure.
 * @return 0 on success, error code on failure.
 */
int tpb_argp_parse_dim_recur(const char *spec, tpb_dim_config_t *cfg);

/**
 * @brief Parse a nested sequence specification.
 *
 * Parses format: <dim>{<nested_dim1>{<nested_dim2>{...}}}
 * Example: dtype=[double,float]{memsize=mul(@,2)(16,16,128,0)}
 *
 * @param spec Input specification string.
 * @param cfg  Output configuration structure (linked list via nested pointer).
 * @return 0 on success, error code on failure.
 */
int tpb_argp_parse_dim_nest(const char *spec, tpb_dim_config_t *cfg);

/**
 * @brief Main entry point for dimension argument parsing.
 *
 * Auto-detects the sequence type based on syntax and delegates to
 * the appropriate parser.
 *
 * @param argstr Full argument string from --kargs-dim.
 * @param cfg    Output pointer to allocated configuration structure.
 * @return 0 on success, error code on failure.
 */
int tpb_argp_parse_dim(const char *argstr, tpb_dim_config_t **cfg);

/* ============================================================================
 * Value Generation Functions
 * ============================================================================ */

/**
 * @brief Generate value array from dimension configuration.
 *
 * Expands the dimension specification into an array of concrete values.
 *
 * @param cfg     Input dimension configuration.
 * @param values  Output pointer to allocated value structure.
 * @return 0 on success, error code on failure.
 */
int tpb_dim_generate_values(tpb_dim_config_t *cfg, tpb_dim_values_t **values);

/**
 * @brief Get total number of parameter combinations.
 *
 * For nested dimensions, returns the Cartesian product count.
 *
 * @param cfg Input dimension configuration.
 * @return Total number of combinations.
 */
int tpb_dim_get_total_count(tpb_dim_config_t *cfg);

/* ============================================================================
 * Cleanup Functions
 * ============================================================================ */

/**
 * @brief Free dimension configuration structure.
 *
 * Recursively frees nested configurations.
 *
 * @param cfg Configuration to free.
 */
void tpb_dim_config_free(tpb_dim_config_t *cfg);

/**
 * @brief Free dimension values structure.
 *
 * Recursively frees nested values.
 *
 * @param values Values structure to free.
 */
void tpb_dim_values_free(tpb_dim_values_t *values);

#endif /* TPB_ARGP_DIM_H */
