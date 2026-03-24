/**
 * @file tpb_corelib_state.h
 * @brief Internal corelib workspace and caller state shared across tpbench sources.
 */

#ifndef TPB_CORELIB_STATE_H
#define TPB_CORELIB_STATE_H

#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

/**
 * @brief Non-zero after tpb_corelib_init completed successfully in this process.
 */
int tpb_corelib_workspace_ready(void);

/**
 * @brief Copy resolved workspace root into module state (used during init only).
 */
void _tpb_workspace_path_set(const char *path);

/**
 * @brief Current workspace root, or empty string before successful init.
 */
const char *_tpb_workspace_path_get(void);

void _tpb_caller_set(int caller);

int _tpb_caller_get(void);

#endif /* TPB_CORELIB_STATE_H */
