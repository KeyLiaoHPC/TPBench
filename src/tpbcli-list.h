/**
 * @file tpbcli-list.h
 * @brief Header for TPBench CLI list subcommand.
 */

#ifndef TPBCLI_LIST_H
#define TPBCLI_LIST_H

/**
 * @brief Execute the list subcommand.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return 0 on success, error code otherwise.
 */
int tpbcli_list(int argc, char **argv);

#endif /* TPBCLI_LIST_H */
