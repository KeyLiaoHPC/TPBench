#ifndef MOCK_KERNEL_H
#define MOCK_KERNEL_H

#include "include/tpb-public.h"

/*
 * Initialize driver and register all mock kernels.
 * Registered PLI kernel: "mock_pli_test"
 * Returns 0 on success.
 */
int mock_setup_driver(void);

/*
 * Build a runtime handle from a registered kernel.
 * Copies kernel metadata and populates argpack with default values.
 * Caller must free hdl->argpack.args when done.
 * Returns 0 on success.
 */
int mock_build_handle(const char *kernel_name, tpb_k_rthdl_t *hdl);

/* Common test harness */
typedef struct {
    const char *id;
    const char *name;
    int (*func)(void);
} test_case_t;

/*
 * Run a test pack. If filter is non-NULL, run only the case whose id
 * matches filter; otherwise run all cases.  Returns number of failures.
 */
int run_pack(const char *pack, test_case_t *cases, int n, const char *filter);

#endif /* MOCK_KERNEL_H */
