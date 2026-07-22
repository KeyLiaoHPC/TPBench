/**
 * @file tpbcli-task-internal.h
 * @brief Shared types and helpers for `tpbcli task` subcommands.
 *
 * Ownership: callers free arrays returned by select/list helpers with free().
 * Capsule member ID arrays are owned by the caller after
 * tpbcli_task_read_capsule_members(). Header arrays from record reads must be
 * released with tpb_raf_free_headers(); payloads with free().
 */

#ifndef TPBCLI_TASK_INTERNAL_H
#define TPBCLI_TASK_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "tpb-public.h"

#define TPBCLI_TASK_RIDMAP_NAME     "tpb_rt_local_ridmap"
#define TPBCLI_TASK_FILTER_MAX      64
#define TPBCLI_TASK_DERIVE_MAX_HOPS 8
#define TPBCLI_TASK_COL_GAP         1
#define TPBCLI_TASK_GR_NAME_MAX     128
#define TPBCLI_TASK_REF_LEN         16
#define TPBCLI_TASK_LOGICAL_MAX     64
#define TPBCLI_TASK_WARN_MAX        128

typedef enum tpbcli_task_filter_op {
    TPBCLI_TASK_FOP_EQ = 0,
    TPBCLI_TASK_FOP_GT,
    TPBCLI_TASK_FOP_GE,
    TPBCLI_TASK_FOP_LT,
    TPBCLI_TASK_FOP_LE
} tpbcli_task_filter_op_t;

typedef enum tpbcli_task_filter_key {
    TPBCLI_TASK_FKEY_KERNEL_ID = 0,
    TPBCLI_TASK_FKEY_KERNEL_NAME,
    TPBCLI_TASK_FKEY_TBATCH_ID,
    TPBCLI_TASK_FKEY_DATETIME,
    TPBCLI_TASK_FKEY_EXIT_CODE
} tpbcli_task_filter_key_t;

typedef struct tpbcli_task_filter {
    tpbcli_task_filter_key_t key;
    tpbcli_task_filter_op_t  op;
    char                     value[256];
    tpb_dtbits_t             datetime_utc;  /**< Valid when key is datetime */
    uint32_t                 exit_code;     /**< Valid when key is exit_code */
} tpbcli_task_filter_t;

typedef struct tpbcli_task_ls_opts {
    int                   count;        /**< -1 means unlimited (default) */
    int                   from_oldest;  /**< 1 when -N was selected */
    int                   nfilter;
    tpbcli_task_filter_t  filters[TPBCLI_TASK_FILTER_MAX];
} tpbcli_task_ls_opts_t;

typedef enum tpbcli_task_sel_kind {
    TPBCLI_TASK_SEL_RID = 0,
    TPBCLI_TASK_SEL_TASK_ID
} tpbcli_task_sel_kind_t;

/** @brief Options for `tpbcli task get-result`. */
typedef struct tpbcli_task_gr_opts {
    tpbcli_task_sel_kind_t sel_kind;
    const char            *rid_spec;
    const char            *task_id_spec;
    int                    show_each_subrank;
    int                    data_name_help;
    int                    meta_name_help;
    int                    ndata_names;
    char                  *data_names[TPBCLI_TASK_GR_NAME_MAX];
    int                    nmeta_names;
    char                  *meta_names[TPBCLI_TASK_GR_NAME_MAX];
} tpbcli_task_gr_opts_t;

/** @brief One user selection before logical-task resolution. */
typedef struct tpbcli_task_init_sel {
    unsigned char          id[20];
    char                   ref[TPBCLI_TASK_REF_LEN];
    tpbcli_task_sel_kind_t sel_kind;
    int                    order;
} tpbcli_task_init_sel_t;

/** @brief One readable member within a logical task. */
typedef struct tpbcli_task_member_rec {
    unsigned char id[20];
    int           subrank;
    task_attr_t   attr;
    void         *data;
    uint64_t      datasize;
    int           readable;
} tpbcli_task_member_rec_t;

/** @brief Resolved logical task (standalone or capsule aggregate). */
typedef struct tpbcli_task_logical {
    unsigned char              root_id[20];
    char                       ref[TPBCLI_TASK_REF_LEN];
    int                        is_capsule;
    int                        nmember;
    tpbcli_task_member_rec_t  *members;
    int                        input_order;
} tpbcli_task_logical_t;

typedef struct tpbcli_task_row {
    task_entry_t entry;
    int          index_order;  /**< Stable order among equal utc_bits */
} tpbcli_task_row_t;

/**
 * @brief Format task utc_bits as local ISO-8601 with explicit offset.
 * @param bits Encoded UTC datetime bits from task records
 * @param out Output buffer (recommended >= 32 bytes)
 * @param outlen Capacity of out
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_task_time_format_local(tpb_dtbits_t bits, char *out, size_t outlen);

/**
 * @brief Parse a filter datetime string into UTC bits (tz field zero).
 * @param text ISO-like datetime, optionally with Z/offset/local wall clock
 * @param bits_out Encoded UTC bits for comparison with task utc_bits
 * @return TPBE_SUCCESS or a TPBE_* error code
 */
int tpbcli_task_time_parse_filter(const char *text, tpb_dtbits_t *bits_out);

/**
 * @brief Build absolute RIDMAP path under workspace/.tmp/.
 */
int tpbcli_task_ridmap_path(const char *workspace, char *out, size_t outlen);

/**
 * @brief Atomically replace RIDMAP with one 40-hex ID per line.
 * @param ids Array of 20-byte TaskRecordIDs in display order
 * @param nids Number of IDs; nids==0 is a no-op that leaves the file unchanged
 */
int tpbcli_task_ridmap_write_atomic(const char *workspace,
                                    const unsigned char (*ids)[20],
                                    int nids);

/**
 * @brief Read RIDMAP IDs, skipping blanks and '#' comments.
 * @param ids_out Newly allocated array of 20-byte IDs (caller frees)
 * @param nids_out Number of IDs
 */
int tpbcli_task_ridmap_read(const char *workspace,
                            unsigned char (**ids_out)[20],
                            int *nids_out);

/**
 * @brief Confirm a loaded task record is a capsule (design §2.3).
 * @return 1 if confirmed capsule, 0 otherwise
 */
int tpbcli_task_confirm_capsule(const task_attr_t *attr,
                                const void *data,
                                uint64_t datasize);

/**
 * @brief Read capsule member IDs from a confirmed capsule record.
 * @param ids_out Newly allocated array (caller frees); NULL when nmembers==0
 * @param nmembers_out Member count
 */
int tpbcli_task_read_capsule_members(const task_attr_t *attr,
                                     const void *data,
                                     uint64_t datasize,
                                     unsigned char (**ids_out)[20],
                                     int *nmembers_out);

/**
 * @brief Format a 20-byte ID as 6-hex prefix plus '*'.
 */
void tpbcli_task_format_id_prefix6(const unsigned char id[20],
                                   char *out,
                                   size_t outlen);

/**
 * @brief Implement `tpbcli task ls|list`.
 */
int tpbcli_task_ls(const char *workspace, const tpbcli_task_ls_opts_t *opts);

/**
 * @brief Parse CSV-style name list (design §7.2).
 * @param names_out Allocated string array (caller frees strings and array)
 */
int tpbcli_task_parse_name_csv(const char *text, char ***names_out,
                               int *nnames_out);

/**
 * @brief Parse RID list spec (decimal ids, commas, closed ranges).
 * @param rids_out Allocated rid integers (caller frees)
 */
int tpbcli_task_parse_rid_list(const char *workspace, const char *spec,
                               int **rids_out, int *nrids_out);

/**
 * @brief Resolve a 6-20 hex task ID prefix to a unique TaskRecordID.
 */
int tpbcli_task_resolve_id_prefix(const char *workspace, const char *prefix,
                                  unsigned char id_out[20]);

/**
 * @brief Follow derive_to to entry root (max @ref TPBCLI_TASK_DERIVE_MAX_HOPS).
 */
int tpbcli_task_follow_derive(const char *workspace,
                              const unsigned char start[20],
                              unsigned char root_out[20]);

/**
 * @brief Build deduplicated logical tasks from initial selections.
 */
int tpbcli_task_build_logical_tasks(const char *workspace,
                                    const tpbcli_task_init_sel_t *sels,
                                    int nsels,
                                    tpbcli_task_logical_t **tasks_out,
                                    int *ntasks_out);

/** @brief Release logical tasks from build_logical_tasks(). */
void tpbcli_task_free_logical_tasks(tpbcli_task_logical_t *tasks, int ntasks);

/**
 * @brief Implement `tpbcli task get-result|gr`.
 */
int tpbcli_task_get_result(const char *workspace,
                           const tpbcli_task_gr_opts_t *opts);

typedef enum tpbcli_task_export_sel_mode {
    TPBCLI_TASK_EXP_SEL_NONE = 0,
    TPBCLI_TASK_EXP_SEL_RID,
    TPBCLI_TASK_EXP_SEL_TASK_ID,
    TPBCLI_TASK_EXP_SEL_FROM_LS
} tpbcli_task_export_sel_mode_t;

typedef enum tpbcli_task_export_trace {
    TPBCLI_TASK_EXP_TRACE_UNSPEC = 0,
    TPBCLI_TASK_EXP_TRACE_TO_ENTRY,
    TPBCLI_TASK_EXP_TRACE_KEEP_CURRENT
} tpbcli_task_export_trace_t;

typedef enum tpbcli_task_export_fkey {
    TPBCLI_TASK_EXP_FKEY_SUBRANK = 0,
    TPBCLI_TASK_EXP_FKEY_SUBTID
} tpbcli_task_export_fkey_t;

typedef struct tpbcli_task_export_filter {
    tpbcli_task_export_fkey_t key;
    int                      nsubrank;
    int                     *subrank; /**< Owned; flattened ranges */
    int                      nsubtid;
    char                     subtid[TPBCLI_TASK_FILTER_MAX][9];
} tpbcli_task_export_filter_t;

/** @brief Options for `tpbcli task export`. */
typedef struct tpbcli_task_export_opts {
    tpbcli_task_export_sel_mode_t sel_mode;
    const char                   *rid_spec;
    const char                   *task_id_spec;
    tpbcli_task_export_trace_t    trace;
    int                           nfilter;
    tpbcli_task_export_filter_t   filters[TPBCLI_TASK_FILTER_MAX];
    const char                   *outdir;
} tpbcli_task_export_opts_t;

/** @brief One export work root after trace resolution. */
typedef struct tpbcli_task_export_work {
    unsigned char root_id[20];
    char          ref[TPBCLI_TASK_REF_LEN];
    int           keep_single_member;
    unsigned char single_member_id[20];
    int           input_order;
} tpbcli_task_export_work_t;

/**
 * @brief Resolve a 4-40 hex task ID prefix (export `-i`).
 */
int tpbcli_task_resolve_id_prefix_export(const char *workspace,
                                         const char *prefix,
                                         unsigned char id_out[20]);

/**
 * @brief Parse `subrank=` / `subtid=` export member filter.
 */
int tpbcli_task_export_parse_filter(const char *text,
                                    tpbcli_task_export_filter_t *out);

/**
 * @brief Build deduplicated export work roots from initial selections.
 */
int tpbcli_task_export_build_works(const char *workspace,
                                   const tpbcli_task_export_opts_t *opts,
                                   tpbcli_task_export_work_t **works_out,
                                   int *nworks_out);

/** @brief Release filters inside export opts and work array. */
void tpbcli_task_export_free_filters(tpbcli_task_export_opts_t *opts);
void tpbcli_task_export_free_works(tpbcli_task_export_work_t *works, int nworks);

/**
 * @brief Implement `tpbcli task export`.
 */
int tpbcli_task_export(const char *workspace,
                       const tpbcli_task_export_opts_t *opts);

#endif /* TPBCLI_TASK_INTERNAL_H */
