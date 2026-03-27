/*
 * tpbcli-list.c
 * Kernel list output (invoked after tpb_register_kernel from `tpbcli kernel list`).
 */

#include "tpbcli-list.h"
#include "corelib/tpb-io.h"

int
tpbcli_kernel_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    tpb_list();
    return 0;
}
