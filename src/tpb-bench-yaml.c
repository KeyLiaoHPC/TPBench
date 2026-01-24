/*
 * tpb-bench-yaml.c
 * Simple YAML parser for benchmark definition files.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tpb-bench-yaml.h"
#include "tpb-types.h"
#include "tpb-io.h"

/* Parser state */
typedef enum {
    PARSE_ROOT,
    PARSE_BENCHMARK,
    PARSE_BATCH_LIST,
    PARSE_BATCH_ITEM,
    PARSE_BATCH_KARGS,
    PARSE_BATCH_V,
    PARSE_SCORE_LIST,
    PARSE_SCORE_ITEM,
    PARSE_SCORE_ARGS
} parse_state_t;

/* Local helper functions */

static int
get_indent(const char *line)
{
    int indent = 0;
    while (*line == ' ') {
        indent++;
        line++;
    }
    return indent;
}

static char *
trim_whitespace(char *str)
{
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

static void
strip_quotes(char *str)
{
    size_t len = strlen(str);
    if (len >= 2) {
        if ((str[0] == '"' && str[len-1] == '"') ||
            (str[0] == '\'' && str[len-1] == '\'')) {
            memmove(str, str + 1, len - 2);
            str[len - 2] = '\0';
        }
    }
}

static int
parse_key_value(const char *line, char *key, size_t key_size, 
                char *value, size_t value_size)
{
    const char *colon = strchr(line, ':');
    if (colon == NULL) return -1;
    
    size_t key_len = colon - line;
    if (key_len >= key_size) key_len = key_size - 1;
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    
    /* Trim key */
    char *k = trim_whitespace(key);
    memmove(key, k, strlen(k) + 1);
    
    /* Get value after colon */
    const char *val = colon + 1;
    while (*val == ' ') val++;
    
    strncpy(value, val, value_size - 1);
    value[value_size - 1] = '\0';
    
    /* Trim value */
    char *v = trim_whitespace(value);
    memmove(value, v, strlen(v) + 1);
    strip_quotes(value);
    
    return 0;
}

static int
is_list_item(const char *line)
{
    const char *p = line;
    while (*p == ' ') p++;
    return (*p == '-');
}

static const char *
get_list_item_content(const char *line)
{
    const char *p = line;
    while (*p == ' ') p++;
    if (*p == '-') {
        p++;
        while (*p == ' ') p++;
    }
    return p;
}

/* Parse kargs array: [["key", "val"], ["key", "val"], ...] */
static int
parse_kargs_array(const char *str, tpb_bench_batch_t *batch)
{
    if (str == NULL || *str != '[') return -1;
    
    batch->kargs[0] = '\0';
    
    const char *p = str + 1;  /* Skip opening [ */
    int first = 1;
    
    while (*p && *p != ']') {
        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;
        
        if (*p == '[') {
            /* Start of pair */
            p++;
            char key[256] = {0};
            char val[256] = {0};
            
            /* Skip whitespace */
            while (*p && isspace((unsigned char)*p)) p++;
            
            /* Parse key (quoted string) */
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                int i = 0;
                while (*p && *p != quote && i < 255) {
                    key[i++] = *p++;
                }
                key[i] = '\0';
                if (*p == quote) p++;
            }
            
            /* Skip comma and whitespace */
            while (*p && (*p == ',' || isspace((unsigned char)*p))) p++;
            
            /* Parse value (quoted string) */
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                int i = 0;
                while (*p && *p != quote && i < 255) {
                    val[i++] = *p++;
                }
                val[i] = '\0';
                if (*p == quote) p++;
            }
            
            /* Skip to end of pair */
            while (*p && *p != ']') p++;
            if (*p == ']') p++;
            
            /* Append to kargs string */
            if (key[0] && val[0]) {
                char pair[512];
                snprintf(pair, sizeof(pair), "%s%s=%s", 
                         first ? "" : ":", key, val);
                strncat(batch->kargs, pair, sizeof(batch->kargs) - strlen(batch->kargs) - 1);
                first = 0;
            }
        }
        
        /* Skip comma */
        while (*p && (*p == ',' || isspace((unsigned char)*p))) p++;
    }
    
    return 0;
}

/* Parse v array: [["name", "reducer"], ...] */
static int
parse_v_item(const char *str, tpb_bench_vspec_t *vspec)
{
    if (str == NULL || *str != '[') return -1;
    
    const char *p = str + 1;  /* Skip opening [ */
    
    /* Skip whitespace */
    while (*p && isspace((unsigned char)*p)) p++;
    
    /* Parse name (quoted string) */
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        int i = 0;
        while (*p && *p != quote && i < 255) {
            vspec->name[i++] = *p++;
        }
        vspec->name[i] = '\0';
        if (*p == quote) p++;
    }
    
    /* Skip comma and whitespace */
    while (*p && (*p == ',' || isspace((unsigned char)*p))) p++;
    
    /* Parse reducer (quoted string) */
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        int i = 0;
        while (*p && *p != quote && i < 31) {
            vspec->reducer[i++] = *p++;
        }
        vspec->reducer[i] = '\0';
    }
    
    return 0;
}

/* Parse score args item (quoted reference string) */
static int
parse_score_arg(const char *str, char *arg, size_t arg_size)
{
    const char *p = str;
    
    /* Skip leading whitespace and list marker */
    while (*p && (isspace((unsigned char)*p) || *p == '-')) p++;
    
    /* Handle quoted strings */
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        size_t i = 0;
        while (*p && *p != quote && i < arg_size - 1) {
            arg[i++] = *p++;
        }
        arg[i] = '\0';
    } else {
        /* Unquoted - copy until end of line */
        strncpy(arg, p, arg_size - 1);
        arg[arg_size - 1] = '\0';
        trim_whitespace(arg);
    }
    
    return 0;
}

/* Public functions */

int
tpb_bench_yaml_load(const char *path, tpb_benchmark_t *bench)
{
    FILE *fp;
    char line[4096];
    char key[256], value[2048];
    parse_state_t state = PARSE_ROOT;
    int base_indent = 0;
    int batch_indent = 0;
    int score_indent = 0;
    tpb_bench_batch_t *cur_batch = NULL;
    tpb_bench_score_t *cur_score = NULL;
    
    if (path == NULL || bench == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    
    /* Initialize benchmark structure */
    memset(bench, 0, sizeof(tpb_benchmark_t));
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                   "Cannot open benchmark file: %s\n", path);
        return TPBE_FILE_IO_FAIL;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        int indent = get_indent(line);
        char *content = trim_whitespace(line);
        
        /* Skip empty lines and comments */
        if (*content == '\0' || *content == '#') {
            continue;
        }
        
        /* Handle state transitions based on content */
        if (strncmp(content, "benchmark:", 10) == 0) {
            state = PARSE_BENCHMARK;
            base_indent = indent;
            continue;
        }
        
        if (state == PARSE_BENCHMARK || state == PARSE_BATCH_LIST || 
            state == PARSE_BATCH_ITEM || state == PARSE_BATCH_KARGS ||
            state == PARSE_BATCH_V || state == PARSE_SCORE_LIST ||
            state == PARSE_SCORE_ITEM || state == PARSE_SCORE_ARGS) {
            
            /* Handle list items FIRST - they take precedence */
            if (is_list_item(content)) {
                const char *item_content = get_list_item_content(content);
                
                /* New batch item - check for "- id:" pattern at batch level */
                if ((state == PARSE_BATCH_LIST || 
                     state == PARSE_BATCH_ITEM || 
                     state == PARSE_BATCH_KARGS ||
                     state == PARSE_BATCH_V) &&
                    strstr(item_content, "id:") != NULL &&
                    (state == PARSE_BATCH_LIST || indent <= batch_indent + 2)) {
                    
                    if (bench->nbatches < TPB_BENCH_MAX_BATCHES) {
                        cur_batch = &bench->batches[bench->nbatches++];
                        memset(cur_batch, 0, sizeof(tpb_bench_batch_t));
                        state = PARSE_BATCH_ITEM;
                        batch_indent = indent;
                        
                        /* Parse id from same line */
                        if (parse_key_value(item_content, key, sizeof(key), 
                                          value, sizeof(value)) == 0) {
                            if (strcmp(key, "id") == 0) {
                                strncpy(cur_batch->id, value, sizeof(cur_batch->id) - 1);
                            }
                        }
                    }
                    continue;
                }
                
                /* V spec item - array entry in v: list */
                if (state == PARSE_BATCH_V && cur_batch != NULL && 
                    *item_content == '[') {
                    if (cur_batch->nvspecs < TPB_BENCH_MAX_VSPECS) {
                        parse_v_item(item_content, 
                                    &cur_batch->vspecs[cur_batch->nvspecs++]);
                    }
                    continue;
                }
                
                /* New score item - check for "- id:" pattern at score level */
                if ((state == PARSE_SCORE_LIST || 
                     state == PARSE_SCORE_ITEM || 
                     state == PARSE_SCORE_ARGS) &&
                    strstr(item_content, "id:") != NULL &&
                    (state == PARSE_SCORE_LIST || indent <= score_indent + 2)) {
                    
                    if (bench->nscores < TPB_BENCH_MAX_SCORES) {
                        cur_score = &bench->scores[bench->nscores++];
                        memset(cur_score, 0, sizeof(tpb_bench_score_t));
                        cur_score->display = 1;  /* Default to display */
                        state = PARSE_SCORE_ITEM;
                        score_indent = indent;
                        
                        /* Parse id from same line */
                        if (parse_key_value(item_content, key, sizeof(key), 
                                          value, sizeof(value)) == 0) {
                            if (strcmp(key, "id") == 0) {
                                strncpy(cur_score->id, value, sizeof(cur_score->id) - 1);
                            }
                        }
                    }
                    continue;
                }
                
                /* Score args item - quoted reference string */
                if (state == PARSE_SCORE_ARGS && cur_score != NULL) {
                    if (cur_score->nargs < TPB_BENCH_MAX_ARGS) {
                        parse_score_arg(item_content, 
                                       cur_score->args[cur_score->nargs++], 256);
                    }
                    continue;
                }
                
                /* List item not handled - skip */
                continue;
            }
            
            /* Handle key-value pairs (non-list items) */
            if (parse_key_value(content, key, sizeof(key), value, sizeof(value)) == 0) {
                
                /* Top-level benchmark properties */
                if (strcmp(key, "name") == 0 && state == PARSE_BENCHMARK) {
                    strncpy(bench->name, value, sizeof(bench->name) - 1);
                    continue;
                }
                if (strcmp(key, "note") == 0 && state == PARSE_BENCHMARK) {
                    strncpy(bench->note, value, sizeof(bench->note) - 1);
                    continue;
                }
                
                /* Batch list start */
                if (strcmp(key, "batch") == 0) {
                    state = PARSE_BATCH_LIST;
                    batch_indent = indent;
                    continue;
                }
                
                /* Score list start */
                if (strcmp(key, "score") == 0) {
                    state = PARSE_SCORE_LIST;
                    score_indent = indent;
                    continue;
                }
                
                /* Batch item properties */
                if (cur_batch != NULL && 
                    (state == PARSE_BATCH_ITEM || state == PARSE_BATCH_KARGS || 
                     state == PARSE_BATCH_V)) {
                    if (strcmp(key, "id") == 0) {
                        strncpy(cur_batch->id, value, sizeof(cur_batch->id) - 1);
                    } else if (strcmp(key, "note") == 0) {
                        strncpy(cur_batch->note, value, sizeof(cur_batch->note) - 1);
                    } else if (strcmp(key, "kernel") == 0) {
                        strncpy(cur_batch->kernel, value, sizeof(cur_batch->kernel) - 1);
                    } else if (strcmp(key, "kargs") == 0) {
                        parse_kargs_array(value, cur_batch);
                        state = PARSE_BATCH_KARGS;
                    } else if (strcmp(key, "kenvs") == 0) {
                        strncpy(cur_batch->kenvs, value, sizeof(cur_batch->kenvs) - 1);
                    } else if (strcmp(key, "kmpiargs") == 0) {
                        strncpy(cur_batch->kmpiargs, value, sizeof(cur_batch->kmpiargs) - 1);
                    } else if (strcmp(key, "v") == 0) {
                        state = PARSE_BATCH_V;
                    }
                    continue;
                }
                
                /* Score item properties */
                if (cur_score != NULL && 
                    (state == PARSE_SCORE_ITEM || state == PARSE_SCORE_ARGS)) {
                    if (strcmp(key, "id") == 0) {
                        strncpy(cur_score->id, value, sizeof(cur_score->id) - 1);
                    } else if (strcmp(key, "name") == 0) {
                        strncpy(cur_score->name, value, sizeof(cur_score->name) - 1);
                    } else if (strcmp(key, "display") == 0) {
                        cur_score->display = atoi(value);
                    } else if (strcmp(key, "modifier") == 0) {
                        strncpy(cur_score->modifier, value, sizeof(cur_score->modifier) - 1);
                    } else if (strcmp(key, "args") == 0) {
                        state = PARSE_SCORE_ARGS;
                    }
                    continue;
                }
            }
        }
    }
    
    fclose(fp);
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, 
               "Loaded benchmark '%s' with %d batches and %d scores.\n",
               bench->name, bench->nbatches, bench->nscores);
    
    return TPBE_SUCCESS;
}

void
tpb_bench_yaml_free(tpb_benchmark_t *bench)
{
    if (bench != NULL) {
        memset(bench, 0, sizeof(tpb_benchmark_t));
    }
}
