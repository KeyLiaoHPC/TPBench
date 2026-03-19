/**
 * @file tpb-autorecord.h
 * @brief Auto-record API for tbatch and task record generation.
 *
 * Batch-side functions run in the tpbcli parent process.
 * Task-side functions run in the kernel child process.
 * The TPB_TBATCH_ID environment variable bridges the two.
 */

#ifndef TPB_AUTORECORD_H
#define TPB_AUTORECORD_H

#include <stdint.h>
#include "../include/tpb-public.h"

/* ===== Batch-side API (tpbcli parent process) ===== */

/**
 * @brief Begin a new task batch. Generates TBatchID and caches batch context.
 *
 * Must be called before tpb_driver_run_all(). The generated TBatchID
 * is available via tpb_record_get_tbatch_id_hex() for injection into
 * child process environment variables.
 *
 * @param batch_type TPB_BATCH_TYPE_RUN (0) or TPB_BATCH_TYPE_BENCHMARK (1)
 * @return 0 on success, error code otherwise
 */
int tpb_record_begin_batch(uint32_t batch_type);

/**
 * @brief Get the 40-char hex string of the current TBatchID.
 * @return Pointer to static hex buffer, or NULL if no batch is active.
 */
const char *tpb_record_get_tbatch_id_hex(void);

/**
 * @brief Get the resolved workspace path cached by begin_batch.
 * @return Pointer to static path buffer, or NULL if no batch is active.
 */
const char *tpb_record_get_workspace(void);

/**
 * @brief End the current task batch. Writes tbatch entry and record.
 *
 * Reads task entries from the workspace, filters by TBatchID, and
 * builds the tbatch record with TaskRecordID list in header[1].
 *
 * @param ntask Number of tasks executed in this batch
 * @return 0 on success, error code otherwise
 */
int tpb_record_end_batch(int ntask);

/* ===== Task-side API (kernel child process) ===== */

/**
 * @brief Write a task record after kernel execution.
 *
 * Reads TPB_TBATCH_ID and TPB_HANDLE_INDEX from environment.
 * If TPB_TBATCH_ID is absent, uses all-zero (direct invocation).
 *
 * @param hdl Runtime handle with argpack populated
 * @param exit_code Kernel exit code (0 = success)
 * @return 0 on success, error code otherwise
 */
int tpb_record_write_task(tpb_k_rthdl_t *hdl, int exit_code);

#endif /* TPB_AUTORECORD_H */
