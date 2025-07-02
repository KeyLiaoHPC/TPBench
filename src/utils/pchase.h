#ifndef PCHASE_H
#define PCHASE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Cache information structure */
typedef struct {
    size_t size_bytes;   /* total reported size of this cache index */
    int shared_cnt;      /* how many logical CPUs share it */
    char shared_cpu_list[128];  /* raw CPU list string */
} pchase_cache_info_t;

/* Public API functions */

/**
 * Run single-core pointer chasing tests
 * @param ptsize Array of test sizes in bytes (NULL for auto-detect)
 * @param nsize Number of sizes in ptsize array (ignored if ptsize is NULL)
 * @param ntest Number of test iterations (currently ignored, uses default)
 * @param nwarm Number of warmup iterations (currently ignored, uses default)
 * @param pres Output array for results in picoseconds
 * @return 0 on success, -1 on error
 */
int mem_pchase_run(size_t *ptsize, int nsize, int ntest, int nwarm, uint64_t *pres);

/**
 * Run multi-core pointer chasing tests
 * @param ptsize Array of test sizes in bytes (NULL for auto-detect)
 * @param nsize Number of sizes in ptsize array (ignored if ptsize is NULL)
 * @param ntest Number of test iterations (currently ignored, uses default)
 * @param nwarm Number of warmup iterations (currently ignored, uses default)
 * @param pres Output array for results in picoseconds
 * @return 0 on success, -1 on error
 */
int mem_pchase_run_allcore(size_t *ptsize, int nsize, int ntest, int nwarm, uint64_t *pres);

/**
 * Get cache hierarchy information
 * @param caches Output array for cache information
 * @param levels Output number of cache levels
 * @return 0 on success, -1 on error
 */
int mem_pchase_get_cache_info(pchase_cache_info_t *caches, int *levels);

/**
 * Build test size sequences
 * @param sizes Output pointer to allocated size array
 * @param count Output number of sizes
 * @param single_core true for single-core sizes, false for multi-core sizes
 * @return 0 on success, -1 on error
 * @note Caller is responsible for freeing the sizes array
 */
int mem_pchase_build_sizes(size_t **sizes, int *count, bool single_core);

#endif /* PCHASE_H */ 