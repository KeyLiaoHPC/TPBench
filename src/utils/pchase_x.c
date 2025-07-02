#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "pchase.h"

/* Helper function to read a numeric value from a sysfs file */
static long read_long(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    long v = -1;
    if (fscanf(f, "%ld", &v) != 1)
        v = -1;
    fclose(f);
    return v;
}

/* ------------------------- command line ------------------------- */
typedef struct {
    size_t min_kb, max_kb, step_kb;
} cli_opts_t;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [--min KB] [--max KB] [--step KB]\n", prog);
}

static void parse_args(int argc, char **argv, cli_opts_t *opt)
{
    opt->min_kb = opt->max_kb = opt->step_kb = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--min") && i + 1 < argc) {
            opt->min_kb = strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "--max") && i + 1 < argc) {
            opt->max_kb = strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "--step") && i + 1 < argc) {
            opt->step_kb = strtoul(argv[++i], NULL, 0);
        } else {
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

/* Helper function to remove duplicates and sort a size sequence */
static void deduplicate_and_sort(size_t *seq, int *count)
{
    if (*count <= 1)
        return;
    
    /* Simple bubble sort and deduplication */
    for (int i = 0; i < *count - 1; ++i) {
        for (int j = 0; j < *count - i - 1; ++j) {
            if (seq[j] > seq[j + 1]) {
                size_t temp = seq[j];
                seq[j] = seq[j + 1];
                seq[j + 1] = temp;
            }
        }
    }
    
    /* Remove duplicates */
    int write_idx = 0;
    for (int read_idx = 1; read_idx < *count; ++read_idx) {
        if (seq[read_idx] != seq[write_idx]) {
            write_idx++;
            seq[write_idx] = seq[read_idx];
        }
    }
    *count = write_idx + 1;
}

/* Build test sizes for manual mode */
static int build_manual_sizes(const cli_opts_t *opt, size_t **sizes, int *count)
{
    if (opt->min_kb == 0 || opt->max_kb == 0 || opt->step_kb == 0) {
        return -1;
    }
    
    size_t min = opt->min_kb << 10;
    size_t max = opt->max_kb << 10;
    size_t step = opt->step_kb << 10;
    
    /* Calculate number of sizes needed */
    int n_sizes = 0;
    for (size_t s = min; s <= max; s += step) {
        n_sizes++;
        if (min == max)
            break;
    }
    
    *sizes = malloc(n_sizes * sizeof(size_t));
    if (!*sizes) {
        return -1;
    }
    
    *count = 0;
    for (size_t s = min; s <= max && *count < n_sizes; s += step) {
        (*sizes)[*count] = s;
        (*count)++;
        if (min == max)
            break;
    }
    
    return 0;
}

/* ------------------------- main orchestrator ------------------------- */
int main(int argc, char **argv)
{
    pchase_cache_info_t caches[10] = {0};
    int levels = 0;
    
    if (mem_pchase_get_cache_info(caches, &levels) != 0) {
        fprintf(stderr, "Failed to detect cache hierarchy\n");
        return EXIT_FAILURE;
    }
    
    printf("Cache hierarchy: %d levels\n", levels);
    for (int i = 0; i < levels; ++i) {
        printf("L%d: %zu KB, shared by %d CPUs (%s)\n", 
               i + 1, 
               caches[i].size_bytes >> 10, 
               caches[i].shared_cnt,
               caches[i].shared_cpu_list);
    }
    fflush(stdout);

    cli_opts_t opt = {0};
    parse_args(argc, argv, &opt);

    /* Check if auto-detect mode (not all three parameters specified) */
    bool auto_detect = (opt.min_kb == 0 || opt.max_kb == 0 || opt.step_kb == 0);
    
    size_t *manual_sizes = NULL;
    int manual_count = 0;
    
    if (!auto_detect) {
        /* Manual mode: build both sequences at once */
        if (build_manual_sizes(&opt, &manual_sizes, &manual_count) != 0) {
            fprintf(stderr, "Failed to build manual test sizes\n");
            return EXIT_FAILURE;
        }
        deduplicate_and_sort(manual_sizes, &manual_count);
    }

    printf("=== Cache & Memory Latency Profiler ===\n");
    printf("SMT active: %s | logical CPUs: %ld\n",
           access("/sys/devices/system/cpu/smt/active", R_OK) ? "?" : 
           (read_long("/sys/devices/system/cpu/smt/active") ? "on" : "off"),
           sysconf(_SC_NPROCESSORS_ONLN));

    if (!auto_detect) {
        /* Print test sizes for both scenarios in manual mode */
        printf("\n=== Test Sizes ===\n");
        printf("Manual sizes (%d entries):\n", manual_count);
        for (int i = 0; i < manual_count; ++i) {
            printf("  %zu KB", manual_sizes[i] >> 10);
            if ((i + 1) % 8 == 0 || i == manual_count - 1)
                printf("\n");
            else
                printf(", ");
        }
        printf("\n");
    }

    /* Phase 1: Run all single-core tests */
    printf("=== Phase 1: Single-Core Tests ===\n");
    
    size_t *single_core_sizes = NULL;
    int single_core_count = 0;
    
    if (auto_detect) {
        /* Build single-core sizes dynamically */
        if (mem_pchase_build_sizes(&single_core_sizes, &single_core_count, true) != 0) {
            fprintf(stderr, "Failed to build single-core test sizes\n");
            return EXIT_FAILURE;
        }
        printf("Single-core sizes (%d entries):\n", single_core_count);
        for (int i = 0; i < single_core_count; ++i) {
            printf("  %zu KB", single_core_sizes[i] >> 10);
            if ((i + 1) % 8 == 0 || i == single_core_count - 1)
                printf("\n");
            else
                printf(", ");
        }
        printf("\n");
    } else {
        single_core_sizes = manual_sizes;
        single_core_count = manual_count;
    }
    
    uint64_t *res_sc = calloc(single_core_count, sizeof(uint64_t));
    if (!res_sc) {
        fprintf(stderr, "Failed to allocate single-core results array\n");
        return EXIT_FAILURE;
    }
    
    if (mem_pchase_run(single_core_sizes, single_core_count, 10, 2, res_sc) != 0) {
        fprintf(stderr, "Failed to run single-core tests\n");
        return EXIT_FAILURE;
    }
    
    for (int i = 0; i < single_core_count; ++i) {
        printf("%8zu KB  %.2f ns\n", single_core_sizes[i] >> 10, res_sc[i] / 1000.0);
    }

    /* Phase 2: Run all multi-core tests */
    printf("\n=== Phase 2: Multi-Core Tests ===\n");
    
    size_t *multi_core_sizes = NULL;
    int multi_core_count = 0;
    
    if (auto_detect) {
        /* Build multi-core sizes dynamically */
        if (mem_pchase_build_sizes(&multi_core_sizes, &multi_core_count, false) != 0) {
            fprintf(stderr, "Failed to build multi-core test sizes\n");
            return EXIT_FAILURE;
        }
        printf("Multi-core sizes (%d entries):\n", multi_core_count);
        for (int i = 0; i < multi_core_count; ++i) {
            printf("  %zu KB", multi_core_sizes[i] >> 10);
            if ((i + 1) % 8 == 0 || i == multi_core_count - 1)
                printf("\n");
            else
                printf(", ");
        }
        printf("\n");
    } else {
        multi_core_sizes = manual_sizes;
        multi_core_count = manual_count;
    }
    
    uint64_t *res_mc = calloc(multi_core_count, sizeof(uint64_t));
    if (!res_mc) {
        fprintf(stderr, "Failed to allocate multi-core results array\n");
        return EXIT_FAILURE;
    }
    
    if (mem_pchase_run_allcore(multi_core_sizes, multi_core_count, 10, 2, res_mc) != 0) {
        fprintf(stderr, "Failed to run multi-core tests\n");
        return EXIT_FAILURE;
    }
    
    for (int i = 0; i < multi_core_count; ++i) {
        printf("%8zu KB  %.2f ns\n", multi_core_sizes[i] >> 10, res_mc[i] / 1000.0);
    }

    /* Final summary */
    printf("\n=== Final Results ===\n");
    printf("Single-Core Results:\n");
    printf("%8s  %12s\n", "Size(KB)", "Latency(ns)");
    printf("%8s  %12s\n", "--------", "-----------");
    for (int i = 0; i < single_core_count; ++i) {
        printf("%8zu KB  %12.2f ns\n", 
               single_core_sizes[i] >> 10, res_sc[i] / 1000.0);
    }
    
    printf("\nMulti-Core Results:\n");
    printf("%8s  %12s\n", "Size(KB)", "Latency(ns)");
    printf("%8s  %12s\n", "--------", "-----------");
    for (int i = 0; i < multi_core_count; ++i) {
        printf("%8zu KB  %12.2f ns\n", 
               multi_core_sizes[i] >> 10, res_mc[i] / 1000.0);
    }

    /* Cleanup */
    if (auto_detect) {
        if (single_core_sizes) free(single_core_sizes);
        if (multi_core_sizes) free(multi_core_sizes);
    } else {
        if (manual_sizes) free(manual_sizes);
    }
    free(res_sc);
    free(res_mc);
    return 0;
} 