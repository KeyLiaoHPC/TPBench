/*
 * tpbcli-database.c
 * `tpbcli database` subcommand — dispatch list/ls vs dump.
 */

#include <string.h>

#include "corelib/rafdb/tpb-raf-types.h"
#include "tpbcli-database.h"

/*
 * When: tpbcli dispatches to the `database` command.
 * Input: argc, argv with argv[1] == "database".
 * Output: Delegates to ls or dump; return TPBE_* (see header).
 */
int
tpbcli_database(int argc, char **argv)
{
    char workspace[TPB_RAF_PATH_MAX];
    int err;

    if (argc < 3) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Usage: tpbcli database <list|ls|dump> ...\n");
        return TPBE_CLI_FAIL;
    }

    err = tpb_raf_resolve_workspace(workspace,
                                      sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to resolve workspace.\n");
        return err;
    }

    if (strcmp(argv[2], "list") == 0 ||
        strcmp(argv[2], "ls") == 0) {
        return tpbcli_database_ls(workspace);
    }

    if (strcmp(argv[2], "dump") == 0) {
        return tpbcli_database_dump(argc, argv, workspace);
    }

    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Unknown database action: %s\n"
               "Usage: tpbcli database <list|ls|dump ...>\n",
               argv[2]);
    return TPBE_CLI_FAIL;
}
