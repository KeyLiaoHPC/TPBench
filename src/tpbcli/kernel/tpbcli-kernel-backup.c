/*
 * tpbcli-kernel-backup.c
 * `tpbcli kernel backup-inactive` — backup active .so before rebuild.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-kernel-backup.h"

int
tpbcli_kernel_backup_inactive(int argc, char **argv)
{
    const char *kernel_name = NULL;
    char so_path[PATH_MAX];
    char inactive_dir[PATH_MAX];
    char bak_path[PATH_MAX];
    char kid_hex[41];
    char workspace[PATH_MAX];
    unsigned char kernel_id[20];
    const char *tpb_home;
    int err;
    int i;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            kernel_name = argv[++i];
        }
    }

    if (kernel_name == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT, "Usage: tpbcli kernel backup-inactive --kernel <name>\n");
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_CLI_FAIL, NULL);
    }

    tpb_home = tpb_dl_get_tpb_home();
    if (tpb_home == NULL) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    snprintf(so_path, sizeof(so_path), "%s/lib/libtpbk_%s" TPB_SHLIB_EXT,
             tpb_home, kernel_name);
    if (access(so_path, R_OK) != 0) {
        return TPBE_SUCCESS;
    }

    err = tpb_raf_hash_file(so_path, kernel_id);
    TPB_PROPAGATE(TPB_MOD_CLI_KERNEL, err, NULL);
    tpb_raf_id_to_hex(kernel_id, kid_hex);

    snprintf(inactive_dir, sizeof(inactive_dir), "%s/lib/inactive", tpb_home);
    if (mkdir(inactive_dir, 0755) != 0 && access(inactive_dir, F_OK) != 0) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    snprintf(bak_path, sizeof(bak_path),
             "%s/libkernel_%s_%s.so_bak",
             inactive_dir, kernel_name, kid_hex);

    if (rename(so_path, bak_path) != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "backup-inactive: rename failed for %s\n", so_path);
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err == TPBE_SUCCESS) {
        (void)tpb_raf_entry_patch_kernel_active(workspace, kernel_id, 0);
        (void)tpb_raf_record_patch_kernel_active(workspace, kernel_id, 0);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "backup-inactive: %s -> %s\n", so_path, bak_path);
    return TPBE_SUCCESS;
}
