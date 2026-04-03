/**
 * @file tpb-raf-merge.h
 * @brief Internal API for merging multiple task records.
 */

#ifndef TPB_RAF_MERGE_H
#define TPB_RAF_MERGE_H

#include "tpb-raf-types.h"

/**
 * @brief Merge multiple task records into one combined record.
 *
 * Reads source task records, validates they share tbatch_id and
 * kernel_id, sorts by tid, builds merged headers (with interleaving
 * and placeholders), computes merged duration from btime, writes the
 * merged .tpbr and .tpbe, and updates source records' derive_to to point
 * to the merged ID.
 *
 * @param workspace       Workspace root path
 * @param task_ids        Array of 20-byte source task IDs
 * @param n_tasks         Number of source tasks (>= 2)
 * @param is_process_merge 1 for process-level merge
 *                         (adds ProcessIDs/Hosts),
 *                         0 for thread-level merge
 * @param merged_id_out   Output: 20-byte merged task ID
 * @return TPBE_SUCCESS, TPBE_MERGE_MISMATCH, TPBE_MERGE_FAIL,
 *         or other TPBE_* code
 */
int tpb_raf_merge_par(const char *workspace,
                        const unsigned char task_ids[][20],
                        int n_tasks,
                        int is_process_merge,
                        unsigned char merged_id_out[20]);

#endif /* TPB_RAF_MERGE_H */
