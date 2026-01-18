/*
 * tpb-list.c
 * List action implementation.
 */

#include "tpb-list.h"
#include "tpb-driver.h"
#include "tpb-io.h"

int
tpb_list_action(void)
{
    int err;

    err = tpb_register_kernel();

    if (err == 0) {
        tpb_list();
    }

    return err;
}
