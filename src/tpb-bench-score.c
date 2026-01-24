/*
 * tpb-bench-score.c
 * Score calculation with recursive reference resolution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tpb-bench-score.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Forward declaration for recursive calculation */
static double calculate_score(tpb_benchmark_t *bench, int score_idx);

/* Find batch by id */
static tpb_bench_batch_t *
find_batch_by_id(tpb_benchmark_t *bench, const char *id)
{
    for (int i = 0; i < bench->nbatches; i++) {
        if (strcmp(bench->batches[i].id, id) == 0) {
            return &bench->batches[i];
        }
    }
    return NULL;
}

/* Find score by id */
static int
find_score_idx_by_id(tpb_benchmark_t *bench, const char *id)
{
    for (int i = 0; i < bench->nscores; i++) {
        if (strcmp(bench->scores[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

/* Parse reference string and extract components */
/* Format: @batch[id=='xxx'].v[N] or @score[id==xxx] */
static int
parse_reference(const char *ref, char *type, size_t type_size,
                char *id, size_t id_size, int *index)
{
    if (ref == NULL || ref[0] != '@') {
        return -1;
    }
    
    const char *p = ref + 1;  /* Skip @ */
    
    /* Get type (batch or score) */
    const char *bracket = strchr(p, '[');
    if (bracket == NULL) return -1;
    
    size_t type_len = bracket - p;
    if (type_len >= type_size) type_len = type_size - 1;
    strncpy(type, p, type_len);
    type[type_len] = '\0';
    
    /* Find id value */
    const char *id_start = strstr(bracket, "id==");
    if (id_start == NULL) {
        id_start = strstr(bracket, "id='");
        if (id_start == NULL) return -1;
        id_start += 4;  /* Skip id=' */
        
        /* Find closing quote */
        const char *id_end = strchr(id_start, '\'');
        if (id_end == NULL) return -1;
        
        size_t id_len = id_end - id_start;
        if (id_len >= id_size) id_len = id_size - 1;
        strncpy(id, id_start, id_len);
        id[id_len] = '\0';
    } else {
        id_start += 4;  /* Skip id== */
        
        /* Handle quoted or unquoted id */
        if (*id_start == '\'' || *id_start == '"') {
            char quote = *id_start++;
            const char *id_end = strchr(id_start, quote);
            if (id_end == NULL) return -1;
            
            size_t id_len = id_end - id_start;
            if (id_len >= id_size) id_len = id_size - 1;
            strncpy(id, id_start, id_len);
            id[id_len] = '\0';
        } else {
            /* Unquoted - read until ] */
            size_t i = 0;
            while (*id_start && *id_start != ']' && i < id_size - 1) {
                id[i++] = *id_start++;
            }
            id[i] = '\0';
        }
    }
    
    /* Find index for batch references: .v[N] */
    *index = -1;
    const char *v_bracket = strstr(ref, ".v[");
    if (v_bracket != NULL) {
        *index = atoi(v_bracket + 3);
    }
    
    return 0;
}

double
tpb_bench_score_resolve_ref(tpb_benchmark_t *bench, const char *ref)
{
    char type[32];
    char id[64];
    int index;
    
    if (parse_reference(ref, type, sizeof(type), id, sizeof(id), &index) != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                   "Failed to parse reference: %s\n", ref);
        return NAN;
    }
    
    if (strcmp(type, "batch") == 0) {
        tpb_bench_batch_t *batch = find_batch_by_id(bench, id);
        if (batch == NULL) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                       "Batch not found: %s\n", id);
            return NAN;
        }
        if (index < 0 || index >= batch->nvspecs) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                       "Invalid vspec index %d for batch %s\n", index, id);
            return NAN;
        }
        return batch->vresults[index];
        
    } else if (strcmp(type, "score") == 0) {
        int score_idx = find_score_idx_by_id(bench, id);
        if (score_idx < 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                       "Score not found: %s\n", id);
            return NAN;
        }
        /* Recursive calculation */
        return calculate_score(bench, score_idx);
    }
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
               "Unknown reference type: %s\n", type);
    return NAN;
}

double
tpb_bench_score_apply_modifier(const char *modifier, double *args, int nargs)
{
    if (modifier == NULL || nargs <= 0) {
        return NAN;
    }
    
    if (strcmp(modifier, "raw") == 0) {
        return args[0];
    }
    
    if (strcmp(modifier, "sum") == 0) {
        double sum = 0.0;
        for (int i = 0; i < nargs; i++) {
            sum += args[i];
        }
        return sum;
    }
    
    if (strcmp(modifier, "div") == 0) {
        if (nargs < 2) return args[0];
        double result = args[0];
        for (int i = 1; i < nargs; i++) {
            if (args[i] == 0.0) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                           "Division by zero in score calculation\n");
                return NAN;
            }
            result /= args[i];
        }
        return result;
    }
    
    if (strcmp(modifier, "mul") == 0) {
        double result = 1.0;
        for (int i = 0; i < nargs; i++) {
            result *= args[i];
        }
        return result;
    }
    
    if (strcmp(modifier, "mean") == 0) {
        double sum = 0.0;
        for (int i = 0; i < nargs; i++) {
            sum += args[i];
        }
        return sum / nargs;
    }
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
               "Unknown modifier: %s\n", modifier);
    return NAN;
}

static double
calculate_score(tpb_benchmark_t *bench, int score_idx)
{
    if (score_idx < 0 || score_idx >= bench->nscores) {
        return NAN;
    }
    
    tpb_bench_score_t *score = &bench->scores[score_idx];
    
    /* Return cached value if already calculated */
    if (score->calculated) {
        return score->value;
    }
    
    /* Resolve all arguments */
    double args[TPB_BENCH_MAX_ARGS];
    for (int i = 0; i < score->nargs; i++) {
        args[i] = tpb_bench_score_resolve_ref(bench, score->args[i]);
        if (isnan(args[i])) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                       "Failed to resolve arg %d for score %s: %s\n",
                       i, score->id, score->args[i]);
            score->value = NAN;
            score->calculated = 1;
            return NAN;
        }
    }
    
    /* Apply modifier */
    score->value = tpb_bench_score_apply_modifier(score->modifier, args, score->nargs);
    score->calculated = 1;
    
    return score->value;
}

int
tpb_bench_score_calculate_all(tpb_benchmark_t *bench)
{
    if (bench == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    
    /* Reset calculated flags */
    for (int i = 0; i < bench->nscores; i++) {
        bench->scores[i].calculated = 0;
    }
    
    /* Calculate all scores (order doesn't matter due to recursion) */
    for (int i = 0; i < bench->nscores; i++) {
        calculate_score(bench, i);
    }
    
    return TPBE_SUCCESS;
}

void
tpb_bench_score_display(tpb_benchmark_t *bench)
{
    if (bench == NULL) return;
    
    tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
    tpb_printf(TPBM_PRTN_M_DIRECT, "=== Benchmark Scores ===\n");
    
    for (int i = 0; i < bench->nscores; i++) {
        tpb_bench_score_t *score = &bench->scores[i];
        if (score->display) {
            if (isnan(score->value)) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "%-20s: N/A\n", score->id);
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT, "%-20s: %.6g\n", 
                           score->id, score->value);
            }
        }
    }
    
    tpb_printf(TPBM_PRTN_M_DIRECT, "========================\n");
}
