/**
 * @file tpbcli-database.h
 * @brief tpbcli `database` subcommand interface.
 */

#ifndef TPBCLI_DATABASE_H
#define TPBCLI_DATABASE_H

/**
 * @brief Dispatches `tpbcli database` or `tpbcli db` after argv[0..1].
 * @details When: CLI invoked as `database`/`db` then subcommand. Input: full argc/argv.
 *          Output: Resolves workspace, runs list/ls or dump, or usage error.
 * @param argc Argument count
 * @param argv Argument vector
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_database(int argc, char **argv);

/**
 * @brief Implements `database list` / `database ls`.
 * @details When: Subcommand is list or ls. Input: resolved workspace path and list
 *          filters. Output: Prints domain rows or empty notice; TPBE_* code.
 * @param workspace NUL-terminated workspace root from tpb_raf_resolve_workspace
 * @param domain TPB_RAF_DOM_TBATCH, _KERNEL, or _TASK
 * @param count Maximum rows to show (default 20 when unset by caller)
 * @param from_oldest 0 for latest N (-n), 1 for oldest N (-N)
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_database_ls(const char *workspace, uint8_t domain,
                       int count, int from_oldest);

/**
 * @brief Run `database dump` using a pre-parsed selector and value.
 * @param workspace Resolved workspace root
 * @param selector_name Long option name: `--id`, `--tbatch-id`, `--kernel-id`,
 *                      `--task-id`, `--score-id`, `--file`, or `--entry`;
 *                      NULL means no selector (usage printed).
 * @param primary_value Hex string, file path, or entry label per selector
 * @param entry_value Reserved; pass NULL
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_database_dump_resolved(const char *workspace,
                                  const char *selector_name,
                                  const char *primary_value,
                                  const char *entry_value);

#endif /* TPBCLI_DATABASE_H */
