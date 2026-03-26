/**
 * @file tpbcli-database.h
 * @brief tpbcli `database` subcommand interface.
 */

#ifndef TPBCLI_LOG_H
#define TPBCLI_LOG_H

/**
 * @brief Dispatches `tpbcli database` after argv[0..1] (`tpbcli`, `database`).
 * @details When: CLI invoked as `database <subcommand>`. Input: full argc/argv.
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
 * @param workspace NUL-terminated workspace root from tpb_rawdb_resolve_workspace
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_database_ls(const char *workspace);

/**
 * @brief Implements `database dump` (flags from argv[3..]).
 * @details When: Subcommand is dump. Input: full argc/argv and workspace.
 *          Output: CSV-style dump to stdout or CLI error; TPBE_* code.
 * @param argc Argument count
 * @param argv Argument vector (argv[2] must be "dump")
 * @param workspace Resolved workspace root
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_database_dump(int argc, char **argv, const char *workspace);

#endif /* TPBCLI_LOG_H */
