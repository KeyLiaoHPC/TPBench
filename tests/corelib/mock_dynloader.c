/*
 * Mock dynloader for PLI testing.
 *
 * These definitions override the symbols from libtpbench.so via
 * ELF symbol interposition: the executable's definitions take
 * precedence over the shared library's for all callers, including
 * calls originating inside libtpbench.so itself.
 */

#include <stdint.h>
#include <stddef.h>
#include "include/tpb-public.h"

static const char *g_mock_exec_path = NULL;
static int g_mock_complete = 0;

void
mock_dl_set_exec_path(const char *path)
{
    g_mock_exec_path = path;
}

void
mock_dl_set_complete(int complete)
{
    g_mock_complete = complete;
}

/* Overrides tpb_dl_get_exec_path in libtpbench.so */
const char *
tpb_dl_get_exec_path(const char *kernel_name)
{
    (void)kernel_name;
    return g_mock_exec_path;
}

/* Overrides tpb_dl_is_complete in libtpbench.so */
int
tpb_dl_is_complete(const char *kernel_name)
{
    (void)kernel_name;
    return g_mock_complete;
}

/* Overrides tpb_dl_scan in libtpbench.so (no-op) */
int
tpb_dl_scan(void)
{
    return 0;
}

/* Overrides tpb_dl_get_ktype in libtpbench.so */
TPB_K_CTRL
tpb_dl_get_ktype(const char *kernel_name)
{
    (void)kernel_name;
    return TPB_KTYPE_PLI;
}

/* Overrides tpb_dl_get_tpb_dir in libtpbench.so */
const char *
tpb_dl_get_tpb_dir(void)
{
    return "/tmp/tpbench_mock";
}
