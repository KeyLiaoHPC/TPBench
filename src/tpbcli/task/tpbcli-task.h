/**
 * @file tpbcli-task.h
 * @brief tpbcli `task` subcommand interface.
 */

#ifndef TPBCLI_TASK_H
#define TPBCLI_TASK_H

/**
 * @brief Dispatch `tpbcli task` after argv[0..1] (`tpbcli`, `task`).
 * @param argc Argument count
 * @param argv Argument vector
 * @return TPBE_SUCCESS, TPBE_EXIT_ON_HELP, or a TPBE_* error code
 */
int tpbcli_task(int argc, char **argv);

#endif /* TPBCLI_TASK_H */
