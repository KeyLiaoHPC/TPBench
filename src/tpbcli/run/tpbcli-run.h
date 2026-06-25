/**
 * @file tpbcli-run.h
 * @brief Header for TPBench CLI run subcommand.
 */

#ifndef TPBCLI_RUN_H
#define TPBCLI_RUN_H

/**
 * @brief Execute the run subcommand.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return 0 on success, error code otherwise.
 */
int tpbcli_run(int argc, char **argv);

#endif /* TPBCLI_RUN_H */
