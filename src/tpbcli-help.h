/**
 * @file tpbcli-help.h
 * @brief Header for TPBench CLI help subcommand.
 */

#ifndef TPBCLI_HELPER_H
#define TPBCLI_HELPER_H

/**
 * @brief Execute the help subcommand.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return 0 on success, error code otherwise.
 */
int tpbcli_help(int argc, char **argv);

#endif /* TPBCLI_HELPER_H */
