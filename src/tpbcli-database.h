/**
 * @file tpbcli-database.h
 * @brief tpbcli `database` subcommand interface.
 */

#ifndef TPBCLI_LOG_H
#define TPBCLI_LOG_H

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
 * @details When: Subcommand is list or ls. Input: resolved workspace path.
 *          Output: Prints latest tbatch rows or empty notice; TPBE_* code.
 * @param workspace NUL-terminated workspace root from tpb_raf_resolve_workspace
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_database_ls(const char *workspace);

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

#endif /* TPBCLI_LOG_H */
