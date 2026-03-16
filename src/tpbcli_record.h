/**
 * @file tpbcli_record.h
 * @brief tpbcli record subcommand interface.
 */

#ifndef TPBCLI_RECORD_H
#define TPBCLI_RECORD_H

/**
 * @brief Entry point for `tpbcli record` subcommand.
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, error code on failure
 */
int tpbcli_record(int argc, char **argv);

#endif /* TPBCLI_RECORD_H */
