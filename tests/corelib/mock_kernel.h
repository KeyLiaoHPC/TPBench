#ifndef MOCK_KERNEL_H
#define MOCK_KERNEL_H

#include "include/tpb-public.h"

/* Mock FLI kernel runners */
int mock_fli_success(void);
int mock_fli_warn(void);
int mock_fli_fail(void);
int mock_fli_check_arg(void);

/* Accessor for the value retrieved by mock_fli_check_arg */
int64_t mock_get_received_ntest(void);

/*
 * Initialize driver and register all mock kernels.
 * Registered FLI kernels: "mock_fli_ok", "mock_fli_warn", "mock_fli_fail", "mock_fli_getarg"
 * Registered PLI kernel:  "mock_pli_test"
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
