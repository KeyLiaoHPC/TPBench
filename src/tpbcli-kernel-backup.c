/*
 * tpbcli-kernel-backup.c
 * `tpbcli kernel backup-inactive` — backup active .so before rebuild.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-kernel-meta.h"
#include "corelib/rafdb/tpb-sha1.h"
#include "corelib/tpb-dynloader.h"
#include "tpbcli-kernel-backup.h"

static int
_sf_hash_file(const char *path, unsigned char out[20])
{
    FILE *fp;
    tpb_sha1_ctx_t ctx;
    unsigned char buf[4096];
    size_t nread;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    tpb_sha1_init(&ctx);
    while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
        tpb_sha1_update(&ctx, buf, nread);
    }
    tpb_sha1_final(&ctx, out);
    fclose(fp);
    return TPBE_SUCCESS;
}

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
        fprintf(stderr,
                "Usage: tpbcli kernel backup-inactive --kernel <name>\n");
        return TPBE_CLI_FAIL;
    }

    tpb_home = tpb_dl_get_tpb_home();
    if (tpb_home == NULL) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(so_path, sizeof(so_path), "%s/lib/libtpbk_%s.so",
             tpb_home, kernel_name);
    if (access(so_path, R_OK) != 0) {
        return TPBE_SUCCESS;
    }

    err = _sf_hash_file(so_path, kernel_id);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    tpb_raf_id_to_hex(kernel_id, kid_hex);

    snprintf(inactive_dir, sizeof(inactive_dir), "%s/lib/inactive", tpb_home);
    if (mkdir(inactive_dir, 0755) != 0 && access(inactive_dir, F_OK) != 0) {
        return TPBE_FILE_IO_FAIL;
    }

    snprintf(bak_path, sizeof(bak_path),
             "%s/libkernel_%s_%s.so_bak",
             inactive_dir, kernel_name, kid_hex);

    if (rename(so_path, bak_path) != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "backup-inactive: rename failed for %s\n", so_path);
        return TPBE_FILE_IO_FAIL;
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err == TPBE_SUCCESS) {
        (void)tpb_raf_entry_patch_kernel_active(workspace, kernel_id, 0);
        (void)tpb_raf_record_patch_kernel_active(workspace, kernel_id, 0);
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "backup-inactive: %s -> %s\n", so_path, bak_path);
    return TPBE_SUCCESS;
}
