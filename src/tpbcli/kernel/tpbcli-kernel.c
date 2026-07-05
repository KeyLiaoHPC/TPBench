/*
 * tpbcli-kernel.c
 * `tpbcli kernel` / `tpbcli k` — kernel management subcommands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"

#include "tpbcli-kernel.h"
#include "tpbcli-kernel-list.h"
#include "tpbcli-kernel-set.h"
#include "tpbcli-kernel-get.h"
#include "tpbcli-kernel-backup.h"
#include "tpbcli-kernel-init.h"
#include "tpbcli-kernel-build.h"

int
tpbcli_kernel(int argc, char **argv)
{
    int err;

    if (argc < 3) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "Usage: tpbcli kernel <list|ls|set|get|init|build|backup-inactive>\n");
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    if (strcmp(argv[2], "get") == 0) {
        return tpbcli_kernel_get(argc, argv);
    }

    if (strcmp(argv[2], "set") == 0) {
        return tpbcli_kernel_set(argc, argv);
    }

    if (strcmp(argv[2], "backup-inactive") == 0) {
        return tpbcli_kernel_backup_inactive(argc, argv);
    }

    if (strcmp(argv[2], "init") == 0) {
        return tpbcli_kernel_init(argc, argv);
    }

    if (strcmp(argv[2], "build") == 0) {
        return tpbcli_kernel_build(argc, argv);
    }

    err = tpb_register_kernel();
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "tpbcli kernel: tpb_register_kernel failed (%d).\n", err);
        TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, "tpb_register_kernel");
    }

    if (strcmp(argv[2], "list") == 0 || strcmp(argv[2], "ls") == 0) {
        return tpbcli_kernel_list(argc, argv);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "Usage: tpbcli kernel <list|ls|set|get|init|build|backup-inactive>\n");
    TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
}
