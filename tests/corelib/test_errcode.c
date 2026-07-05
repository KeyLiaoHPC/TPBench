/*
 * test_errcode.c
 * Test pack A9: Unit tests for layered TPBE module/cause error codes.
 */

#include <stdio.h>
#include <string.h>
#include "include/tpb-public.h"

static int
test_make_cause_module_roundtrip(void)
{
    int err;

    err = TPBE_MAKE(TPB_MOD_RAF_L2_TBATCH, TPBE_FILE_IO_FAIL);
    if (TPBE_CAUSE(err) != TPBE_FILE_IO_FAIL) {
        return 1;
    }
    if (TPBE_MODULE(err) != TPB_MOD_RAF_L2_TBATCH) {
        return 1;
    }
    if (!TPBE_IS_ENCODED(err)) {
        return 1;
    }
    return 0;
}

static int
test_propagate_swaps_module(void)
{
    int origin;
    int propagated;

    origin = TPBE_MAKE(TPB_MOD_RAF_L2_TBATCH, TPBE_FILE_IO_FAIL);
    propagated = tpb_err_propagate(TPB_MOD_RAF_MISC, origin);
    if (TPBE_CAUSE(propagated) != TPBE_FILE_IO_FAIL) {
        return 1;
    }
    if (TPBE_MODULE(propagated) != TPB_MOD_RAF_MISC) {
        return 1;
    }
    return 0;
}

static int
test_propagate_legacy_bare_cause(void)
{
    int propagated;

    propagated = tpb_err_propagate(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG);
    if (TPBE_CAUSE(propagated) != TPBE_NULLPTR_ARG) {
        return 1;
    }
    if (TPBE_MODULE(propagated) != TPB_MOD_DRIVER) {
        return 1;
    }
    return 0;
}

static int
test_exit_status_uses_cause(void)
{
    int err;

    err = TPBE_MAKE(TPB_MOD_CLI_RUN, TPBE_CLI_FAIL);
    if (tpb_err_to_exit_status(err) != TPBE_CLI_FAIL) {
        return 1;
    }
    if (tpb_err_to_exit_status(TPBE_EXIT_ON_HELP) != 0) {
        return 1;
    }
    if (tpb_err_to_exit_status(TPBE_SUCCESS) != 0) {
        return 1;
    }
    return 0;
}

static int
test_get_err_msg_unknown_cause(void)
{
    if (strcmp(tpb_get_err_msg(999), "Unknown error.") != 0) {
        return 1;
    }
    if (strcmp(tpb_get_err_msg(TPBE_MAKE(TPB_MOD_DRIVER, 999)),
               "Unknown error.") != 0) {
        return 1;
    }
    return 0;
}

static int
test_module_name_lookup(void)
{
    if (strcmp(tpb_err_module_name(TPB_MOD_RAF_L2_TBATCH), "RAF_L2_TBATCH") != 0) {
        return 1;
    }
    if (strcmp(tpb_err_module_name(TPB_MOD_NONE), "NONE") != 0) {
        return 1;
    }
    return 0;
}

static int
test_success_not_encoded(void)
{
    if (TPBE_IS_ENCODED(TPBE_SUCCESS)) {
        return 1;
    }
    if (TPBE_IS_ENCODED(TPBE_EXIT_ON_HELP)) {
        return 1;
    }
    return 0;
}

typedef struct {
    const char *id;
    const char *name;
    int (*fn)(void);
} test_case_t;

static test_case_t cases[] = {
    { "A9.1", "make_roundtrip", test_make_cause_module_roundtrip },
    { "A9.2", "propagate_swap", test_propagate_swaps_module },
    { "A9.3", "propagate_legacy", test_propagate_legacy_bare_cause },
    { "A9.4", "exit_status", test_exit_status_uses_cause },
    { "A9.5", "unknown_msg", test_get_err_msg_unknown_cause },
    { "A9.6", "module_name", test_module_name_lookup },
    { "A9.7", "success_plain", test_success_not_encoded },
};

static int
run_pack_local(const char *pack, test_case_t *list, int n, const char *filter)
{
    int failures = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (filter != NULL && strcmp(list[i].id, filter) != 0) {
            continue;
        }
        if (list[i].fn() != 0) {
            fprintf(stderr, "FAIL %s %s\n", pack, list[i].name);
            failures++;
        }
    }
    return failures;
}

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;

    return run_pack_local("A9", cases,
                          (int)(sizeof(cases) / sizeof(cases[0])), filter);
}
