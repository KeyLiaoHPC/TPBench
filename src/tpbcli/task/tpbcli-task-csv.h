/**
 * @file tpbcli-task-csv.h
 * @brief Double-comma CSV cell and row writer for `task export`.
 */

#ifndef TPBCLI_TASK_CSV_H
#define TPBCLI_TASK_CSV_H

#include <stdio.h>

/** @brief Streaming writer for `,,`-delimited export rows. */
typedef struct tpbcli_task_csv_writer {
    FILE *fp;
    int   in_row;
    int   ncells;
} tpbcli_task_csv_writer_t;

/**
 * @brief Initialize a CSV writer on an open file (caller owns @p fp).
 */
void tpbcli_task_csv_writer_init(tpbcli_task_csv_writer_t *w, FILE *fp);

/**
 * @brief Start a new row (ends any previous row with a newline).
 * @return TPBE_SUCCESS or TPBE_FILE_IO_FAIL
 */
int tpbcli_task_csv_row_begin(tpbcli_task_csv_writer_t *w);

/**
 * @brief Append one field to the current row (escaped per design §8.6).
 * @param text NULL or empty string writes an empty cell.
 */
int tpbcli_task_csv_cell(tpbcli_task_csv_writer_t *w, const char *text);

/**
 * @brief Append one field containing a single raw byte (e.g. 0x1C for comma char).
 */
int tpbcli_task_csv_cell_byte(tpbcli_task_csv_writer_t *w, unsigned char b);

/**
 * @brief Finish the current row with a newline.
 */
int tpbcli_task_csv_row_end(tpbcli_task_csv_writer_t *w);

#endif /* TPBCLI_TASK_CSV_H */
