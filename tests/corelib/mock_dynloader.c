/*
 * Mock dynloader for PLI testing.
 *
 * These definitions override the symbols from libtpbench via
 * symbol interposition:
 * - Linux ELF: executable definitions take precedence over the .so
 *   for all callers, including calls originating inside libtpbench.so.
 * - macOS: libtpbench is linked with -flat_namespace (Apple-only) so
 *   the same same-name override works; DYLD __DATA,__interpose cannot
 *   intercept intra-dylib calls (tpb_run_pli -> tpb_dl_* in one image).
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

/* Overrides tpb_dl_get_exec_path in libtpbench */
const char *
tpb_dl_get_exec_path(const char *kernel_name)
{
    (void)kernel_name;
    return g_mock_exec_path;
}

/* Overrides tpb_dl_is_complete in libtpbench */
int
tpb_dl_is_complete(const char *kernel_name)
{
    (void)kernel_name;
    return g_mock_complete;
}

/* Overrides tpb_dl_scan in libtpbench (no-op) */
int
tpb_dl_scan(void)
{
    return 0;
}

/* Overrides tpb_dl_scan_kernel in libtpbench (no-op) */
int
tpb_dl_scan_kernel(const char *kernel_name)
{
    (void)kernel_name;
    return 0;
}

/* Overrides tpb_dl_get_tpb_home in libtpbench */
const char *
tpb_dl_get_tpb_home(void)
{
    return "/tmp/tpbench_mock";
}
