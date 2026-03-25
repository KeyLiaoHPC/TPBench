/*
 * tpbcli-database-dump.c
 * `tpbcli database dump` — CSV-style dump of rawdb .tpbr/.tpbe files.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdint.h>
#include <inttypes.h>

#include "corelib/strftime.h"
#include "corelib/raw_db/tpb-rawdb-types.h"
#include "tpbcli-database.h"

/* Scan every tpbr subdirectory when searching by id prefix (global --id). */
#define DOMAIN_FILTER_ALL ((uint8_t)0xFF)

typedef struct id_prefix_match {
    char     full_hex[41];
    uint8_t  domain;
} id_prefix_match_t;

typedef enum {
    DUMP_T_NONE = 0,
    DUMP_T_ID_GLOBAL,
    DUMP_T_TBATCH_ID,
    DUMP_T_KERNEL_ID,
    DUMP_T_TASK_ID,
    DUMP_T_SCORE_ID,
    DUMP_T_FILE,
    DUMP_T_ENTRY
} dump_target_t;

/* Local Function Prototypes */

/*
 * When: While parsing `database dump` argv to choose what to dump.
 * Input: argc, argv (full CLI vector); pathlen, entrylen buffer sizes;
 *        optional consumed_to (may be NULL).
 * Output: Returns target kind; copies first matching path/hex or entry name
 *         into path_or_hex / entry_name; caller uses enum to interpret.
 */
static dump_target_t parse_dump_target(int argc, char **argv, int *consumed_to,
                                       char *path_or_hex, size_t pathlen,
                                       char *entry_name, size_t entrylen);

/*
 * When: After `--entry <name>`; normalize user spelling for domain lookup.
 * Input: in — raw entry label; out buffer with capacity outlen.
 * Output: Fills out with lowercased, space/hyphen→underscore name; TPBE_* code.
 */
static int normalize_entry_name(const char *in, char *out, size_t outlen);

/*
 * When: After normalize_entry_name for `--entry`.
 * Input: norm — normalized entry keyword string.
 * Output: Sets *domain_out to TPB_RAWDB_DOM_*; TPBE_SUCCESS or TPBE_CLI_FAIL.
 */
static int entry_name_to_domain(const char *norm, uint8_t *domain_out);

/*
 * When: Resolving `--file` or basename-only paths under workspace.
 * Input: workspace root; inpath as user typed; out buffer outlen bytes.
 * Output: Fills out with absolute path to existing regular file; TPBE_* code.
 */
static int resolve_file_path(const char *workspace, const char *inpath,
                             char *out, size_t outlen);

/*
 * When: Validating id strings from CLI or filename stem before hex→binary.
 * Input: in — user text; out must hold at least 42 bytes; out_len non-NULL.
 * Output: Lowercased hex in out, length in *out_len; TPBE_SUCCESS or fail.
 */
static int parse_id_hex_arg(const char *in, char *out, size_t outsz,
                            size_t *out_len);

/*
 * When: qsort compare for id_prefix_match_t rows (ambiguous prefix table).
 * Input: a, b — pointers to id_prefix_match_t (const void * for qsort).
 * Output: <0, 0, or >0 per strcmp(full_hex) then domain tie-break.
 */
static int cmp_prefix_match(const void *a, const void *b);

/*
 * When: Partial id (4–39 hex) needs matching .tpbr files in workspace.
 * Input: workspace; prefix and prefix_len; domain_filter or DOMAIN_FILTER_ALL.
 * Output: Allocated *matches_out and *nmatch_out; caller must free(matches_out).
 */
static int scan_tpbr_prefix_matches(const char *workspace,
                                    const char *prefix, size_t prefix_len,
                                    uint8_t domain_filter,
                                    id_prefix_match_t **matches_out,
                                    int *nmatch_out);

/*
 * When: More than one .tpbr shares the same id prefix; tell user full ids.
 * Input: prefix shown to user; matches array and count; workspace for metadata.
 * Output: Prints CSV header + rows to stdout via tpb_printf; no return value.
 */
static void print_ambiguous_id_table(const char *prefix,
                                     id_prefix_match_t *matches, int nmatch,
                                     const char *workspace);

/*
 * When: Full 20-byte id is known together with its record domain.
 * Input: workspace; domain; id[20] binary record id.
 * Output: Dispatches to tpbr dump; returns TPBE_* from underlying reader.
 */
static int dump_tpbr_by_domain(const char *workspace, uint8_t domain,
                               const unsigned char id[20]);

/*
 * When: Handling `--id` / `--tbatch-id` / `--kernel-id` / `--task-id` args.
 * Input: workspace; target kind t; arg — hex string after the flag.
 * Output: Dumps one record or prints ambiguity table; returns TPBE_* code.
 */
static int resolve_hex_arg_and_dump(const char *workspace, dump_target_t t,
                                    const char *arg);

/*
 * When: Emitting one metadata field as "key, value" CSV line (u64).
 * Input: key label; v — numeric value.
 * Output: One line to stdout.
 */
static void dump_print_kv_u64(const char *key, uint64_t v);

/*
 * When: Same as dump_print_kv_u64 for unsigned 32-bit fields.
 * Input: key; v.
 * Output: One CSV line to stdout.
 */
static void dump_print_kv_u32(const char *key, uint32_t v);

/*
 * When: String metadata field dump (empty string if val is NULL).
 * Input: key; val — C string or NULL.
 * Output: One CSV line to stdout.
 */
static void dump_print_kv_str(const char *key, const char *val);

/*
 * When: 20-byte binary id shown as 40 hex chars in CSV.
 * Input: key; id[20] raw bytes.
 * Output: One CSV line to stdout.
 */
static void dump_print_kv_hex20(const char *key, const unsigned char id[20]);

/*
 * When: After loading a .tpbr; dump all tpb_meta_header_t slots as CSV keys.
 * Input: hdrs array; n — number of headers.
 * Output: Many "header[i].field, value" lines to stdout.
 */
static void dump_headers(const tpb_meta_header_t *hdrs, uint32_t n);

/*
 * When: Infer element byte size from header type_bits for array walk.
 * Input: type_bits from tpb_meta_header_t.
 * Output: Non-zero size in bytes (default 8 when nbytes field is zero).
 */
static size_t dtype_elem_size_from_typebits(uint32_t type_bits);

/*
 * When: Recursively print one multi-dimensional header blob as nested indices.
 * Input: header h; blob; nd; mult/idx scratch; lev recursion level; elem_size.
 * Output: Lines "name[idx]..., elem, elem, ..." to stdout.
 */
static void dump_header_record_lines(const tpb_meta_header_t *h,
                                     const uint8_t *blob, uint32_t nd,
                                     uint64_t mult[TPBM_DATA_NDIM_MAX],
                                     uint64_t idx[TPBM_DATA_NDIM_MAX], int lev,
                                     size_t elem_size);

/*
 * When: Formatting a single scalar/array element according to dtype.
 * Input: p — element bytes; type_bits; elem_size (20 triggers id hex).
 * Output: Prints formatted token to stdout (no trailing newline here).
 */
static void print_elem_csv(const uint8_t *p, uint32_t type_bits,
                           size_t elem_size);

/*
 * When: After headers; walk record payload blocks per header slot.
 * Input: hdrs, nheader; data blob and datasize from rawdb read.
 * Output: "Record Data" section and per-header CSV lines to stdout.
 */
static void dump_record_data(const tpb_meta_header_t *hdrs, uint32_t nheader,
                             const void *data, uint64_t datasize);

/*
 * When: User targets a tbatch .tpbr by id.
 * Input: workspace; id[20] tbatch record id.
 * Output: Full metadata + record data dump; frees internal allocations.
 */
static int dump_tpbr_tbatch(const char *workspace,
                            const unsigned char id[20]);

/*
 * When: User targets a kernel .tpbr by id.
 * Input: workspace; id[20] kernel record id.
 * Output: Full metadata + record data dump; frees internal allocations.
 */
static int dump_tpbr_kernel(const char *workspace,
                            const unsigned char id[20]);

/*
 * When: User targets a task .tpbr by id.
 * Input: workspace; id[20] task record id.
 * Output: Full metadata + record data dump; frees internal allocations.
 */
static int dump_tpbr_task(const char *workspace,
                          const unsigned char id[20]);

/*
 * When: `--entry` or .tpbe file detected; list domain index file contents.
 * Input: workspace; domain — TPB_RAWDB_DOM_* for which entry file to read.
 * Output: Prints all indexed entries as CSV-like key/value lines.
 */
static int dump_tpbe_domain(const char *workspace, uint8_t domain);

/*
 * When: `--file` path or basename resolved to a .tpbr or .tpbe under workspace.
 * Input: workspace; filepath as user gave (relative or absolute).
 * Output: Magic detect then tpbe or tpbr dump; TPBE_* on failure.
 */
static int dump_file_path(const char *workspace, const char *filepath);

static void
dump_print_kv_u64(const char *key, uint64_t v)
{
    tpb_printf(TPBM_PRTN_M_DIRECT, "%s, %" PRIu64 "\n", key, v);
}

static void
dump_print_kv_u32(const char *key, uint32_t v)
{
    tpb_printf(TPBM_PRTN_M_DIRECT, "%s, %" PRIu32 "\n", key, v);
}

static void
dump_print_kv_str(const char *key, const char *val)
{
    tpb_printf(TPBM_PRTN_M_DIRECT, "%s, %s\n", key, val ? val : "");
}

static void
dump_print_kv_hex20(const char *key, const unsigned char id[20])
{
    char hex[41];
    tpb_rawdb_id_to_hex(id, hex);
    tpb_printf(TPBM_PRTN_M_DIRECT, "%s, %s\n", key, hex);
}

static int
normalize_entry_name(const char *in, char *out, size_t outlen)
{
    size_t i, j;
    if (!in || !out || outlen == 0) {
        return TPBE_NULLPTR_ARG;
    }
    for (i = 0, j = 0; in[i] != '\0' && j + 1 < outlen; i++) {
        char c = in[i];
        if (c == ' ' || c == '\t' || c == '-') {
            out[j++] = '_';
        } else {
            out[j++] = (char)tolower((unsigned char)c);
        }
    }
    out[j] = '\0';
    return TPBE_SUCCESS;
}

static int
entry_name_to_domain(const char *norm, uint8_t *domain_out)
{
    if (!norm || !domain_out) {
        return TPBE_NULLPTR_ARG;
    }
    if (strcmp(norm, "task_batch") == 0 ||
        strcmp(norm, "tbatch") == 0 ||
        strcmp(norm, "taskbatch") == 0) {
        *domain_out = TPB_RAWDB_DOM_TBATCH;
        return TPBE_SUCCESS;
    }
    if (strcmp(norm, "kernel") == 0) {
        *domain_out = TPB_RAWDB_DOM_KERNEL;
        return TPBE_SUCCESS;
    }
    if (strcmp(norm, "task") == 0) {
        *domain_out = TPB_RAWDB_DOM_TASK;
        return TPBE_SUCCESS;
    }
    return TPBE_CLI_FAIL;
}

static int
resolve_file_path(const char *workspace, const char *inpath,
                  char *out, size_t outlen)
{
    struct stat st;
    const char *base;
    char trybuf[TPB_RAWDB_PATH_MAX];

    if (!workspace || !inpath || !out || outlen == 0) {
        return TPBE_NULLPTR_ARG;
    }

    if (stat(inpath, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(out, outlen, "%s", inpath);
        return TPBE_SUCCESS;
    }

    base = strrchr(inpath, '/');
    base = base ? base + 1 : inpath;

    snprintf(trybuf, sizeof(trybuf), "%s/%s/%s",
             workspace, TPB_RAWDB_TBATCH_DIR, base);
    if (stat(trybuf, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(out, outlen, "%s", trybuf);
        return TPBE_SUCCESS;
    }
    snprintf(trybuf, sizeof(trybuf), "%s/%s/%s",
             workspace, TPB_RAWDB_KERNEL_DIR, base);
    if (stat(trybuf, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(out, outlen, "%s", trybuf);
        return TPBE_SUCCESS;
    }
    snprintf(trybuf, sizeof(trybuf), "%s/%s/%s",
             workspace, TPB_RAWDB_TASK_DIR, base);
    if (stat(trybuf, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(out, outlen, "%s", trybuf);
        return TPBE_SUCCESS;
    }

    return TPBE_FILE_IO_FAIL;
}

/* Leading/trailing whitespace only; 4-40 hex digits, lowercased. No 0x. */
static int
parse_id_hex_arg(const char *in, char *out, size_t outsz, size_t *out_len)
{
    const char *a;
    const char *z;
    size_t n, i;

    if (!in || !out || !out_len || outsz < 42) {
        return TPBE_NULLPTR_ARG;
    }
    a = in;
    while (*a != '\0' && isspace((unsigned char)*a)) {
        a++;
    }
    z = a + strlen(a);
    while (z > a && isspace((unsigned char)z[-1])) {
        z--;
    }
    n = (size_t)(z - a);
    if (n < 4 || n > 40) {
        return TPBE_CLI_FAIL;
    }
    memcpy(out, a, n);
    out[n] = '\0';
    for (i = 0; i < n; i++) {
        if (!isxdigit((unsigned char)out[i])) {
            return TPBE_CLI_FAIL;
        }
        out[i] = (char)tolower((unsigned char)out[i]);
    }
    *out_len = n;
    return TPBE_SUCCESS;
}

static int
cmp_prefix_match(const void *a, const void *b)
{
    const id_prefix_match_t *x = (const id_prefix_match_t *)a;
    const id_prefix_match_t *y = (const id_prefix_match_t *)b;
    int c = strcmp(x->full_hex, y->full_hex);
    if (c != 0) {
        return c;
    }
    return (int)x->domain - (int)y->domain;
}

static int
scan_tpbr_prefix_matches(const char *workspace, const char *prefix,
                         size_t prefix_len, uint8_t domain_filter,
                         id_prefix_match_t **matches_out, int *nmatch_out)
{
    static const struct {
        const char *reldir;
        uint8_t     domain;
    } dirs[3] = {
        { TPB_RAWDB_TBATCH_DIR, TPB_RAWDB_DOM_TBATCH },
        { TPB_RAWDB_KERNEL_DIR, TPB_RAWDB_DOM_KERNEL },
        { TPB_RAWDB_TASK_DIR,   TPB_RAWDB_DOM_TASK },
    };
    char dirpath[TPB_RAWDB_PATH_MAX];
    id_prefix_match_t *matches = NULL;
    int n = 0, cap = 0;
    size_t di;

    if (!workspace || !prefix || !matches_out || !nmatch_out) {
        return TPBE_NULLPTR_ARG;
    }
    *matches_out = NULL;
    *nmatch_out = 0;

    for (di = 0; di < sizeof(dirs) / sizeof(dirs[0]); di++) {
        DIR *dp;
        struct dirent *ent;

        if (domain_filter != DOMAIN_FILTER_ALL &&
            domain_filter != dirs[di].domain) {
            continue;
        }
        snprintf(dirpath, sizeof(dirpath), "%s/%s",
                 workspace, dirs[di].reldir);
        dp = opendir(dirpath);
        if (!dp) {
            continue;
        }
        while ((ent = readdir(dp)) != NULL) {
            const char *name = ent->d_name;
            size_t len = strlen(name);
            char idpart[41];
            size_t k;

            if (len != 45 || strcmp(name + 40, ".tpbr") != 0) {
                continue;
            }
            memcpy(idpart, name, 40);
            idpart[40] = '\0';
            for (k = 0; k < 40; k++) {
                if (!isxdigit((unsigned char)idpart[k])) {
                    break;
                }
                idpart[k] = (char)tolower((unsigned char)idpart[k]);
            }
            if (k != 40 || strncmp(idpart, prefix, prefix_len) != 0) {
                continue;
            }
            if (n >= cap) {
                int nc = cap ? cap * 2 : 16;
                id_prefix_match_t *nm = (id_prefix_match_t *)realloc(
                    matches, (size_t)nc * sizeof(*matches));
                if (!nm) {
                    closedir(dp);
                    free(matches);
                    return TPBE_MALLOC_FAIL;
                }
                matches = nm;
                cap = nc;
            }
            snprintf(matches[n].full_hex, sizeof(matches[n].full_hex),
                     "%s", idpart);
            matches[n].domain = dirs[di].domain;
            n++;
        }
        closedir(dp);
    }

    if (n > 1) {
        qsort(matches, (size_t)n, sizeof(*matches), cmp_prefix_match);
    }
    *matches_out = matches;
    *nmatch_out = n;
    return TPBE_SUCCESS;
}

static void
print_ambiguous_id_table(const char *prefix, id_prefix_match_t *matches,
                         int nmatch, const char *workspace)
{
    int i;

    tpb_printf(TPBM_PRTN_M_DIRECT,
               "The ID %s maps following records, use longer ID for "
               "precise searching:\n",
               prefix);
    tpb_printf(TPBM_PRTN_M_DIRECT,
               "FullID, Domain, Start UTC, Duration\n");

    for (i = 0; i < nmatch; i++) {
        unsigned char id[20];
        tpb_datetime_str_t ts;
        const char *utc_col = "N/A";
        char tsbuf[sizeof(ts.str)];
        double dur_sec = 0.0;
        int have_dur = 0;

        if (tpb_rawdb_hex_to_id(matches[i].full_hex, id) != TPBE_SUCCESS) {
            continue;
        }

        tsbuf[0] = '\0';
        if (matches[i].domain == TPB_RAWDB_DOM_TBATCH) {
            tbatch_attr_t attr;
            memset(&attr, 0, sizeof(attr));
            if (tpb_rawdb_record_read_tbatch(workspace, id, &attr,
                                              NULL, NULL) == TPBE_SUCCESS) {
                if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
                    snprintf(tsbuf, sizeof(tsbuf), "%s", ts.str);
                    utc_col = tsbuf;
                }
                dur_sec = (double)attr.duration / 1e9;
                have_dur = 1;
                tpb_rawdb_free_headers(attr.headers, attr.nheader);
            }
        } else if (matches[i].domain == TPB_RAWDB_DOM_TASK) {
            task_attr_t attr;
            memset(&attr, 0, sizeof(attr));
            if (tpb_rawdb_record_read_task(workspace, id, &attr,
                                           NULL, NULL) == TPBE_SUCCESS) {
                if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
                    snprintf(tsbuf, sizeof(tsbuf), "%s", ts.str);
                    utc_col = tsbuf;
                }
                dur_sec = (double)attr.duration / 1e9;
                have_dur = 1;
                tpb_rawdb_free_headers(attr.headers, attr.nheader);
            }
        } else {
            utc_col = "N/A";
        }

        {
            const char *dname =
                (matches[i].domain == TPB_RAWDB_DOM_TBATCH) ? "task_batch" :
                (matches[i].domain == TPB_RAWDB_DOM_KERNEL) ? "kernel" : "task";

            if (have_dur) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "%s, %s, %s, %.3f\n",
                           matches[i].full_hex, dname, utc_col, dur_sec);
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "%s, %s, %s, N/A\n",
                           matches[i].full_hex, dname, utc_col);
            }
        }
    }
}

static int
dump_tpbr_by_domain(const char *workspace, uint8_t domain,
                    const unsigned char id[20])
{
    if (domain == TPB_RAWDB_DOM_TBATCH) {
        return dump_tpbr_tbatch(workspace, id);
    }
    if (domain == TPB_RAWDB_DOM_KERNEL) {
        return dump_tpbr_kernel(workspace, id);
    }
    return dump_tpbr_task(workspace, id);
}

static int
resolve_hex_arg_and_dump(const char *workspace, dump_target_t t,
                         const char *arg)
{
    char norm[48];
    size_t plen;
    unsigned char id[20];
    uint8_t dom_filter;
    uint8_t dom;
    int err;

    if (parse_id_hex_arg(arg, norm, sizeof(norm), &plen) != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Invalid id (need 4-40 hex digits, no 0x): %s\n", arg);
        return TPBE_CLI_FAIL;
    }

    switch (t) {
    case DUMP_T_ID_GLOBAL:
        dom_filter = DOMAIN_FILTER_ALL;
        break;
    case DUMP_T_TBATCH_ID:
        dom_filter = TPB_RAWDB_DOM_TBATCH;
        break;
    case DUMP_T_KERNEL_ID:
        dom_filter = TPB_RAWDB_DOM_KERNEL;
        break;
    case DUMP_T_TASK_ID:
        dom_filter = TPB_RAWDB_DOM_TASK;
        break;
    default:
        return TPBE_CLI_FAIL;
    }

    if (plen == 40) {
        if (tpb_rawdb_hex_to_id(norm, id) != TPBE_SUCCESS) {
            return TPBE_CLI_FAIL;
        }
        if (t == DUMP_T_ID_GLOBAL) {
            err = tpb_rawdb_find_record(workspace, id, &dom);
            if (err != TPBE_SUCCESS) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "No .tpbr found for id in workspace.\n");
                return err;
            }
            return dump_tpbr_by_domain(workspace, dom, id);
        }
        return dump_tpbr_by_domain(workspace, dom_filter, id);
    }

    {
        id_prefix_match_t *matches = NULL;
        int nmatch = 0;

        err = scan_tpbr_prefix_matches(workspace, norm, plen, dom_filter,
                                       &matches, &nmatch);
        if (err != TPBE_SUCCESS) {
            free(matches);
            return err;
        }

        if (nmatch == 0) {
            free(matches);
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "No .tpbr matches id prefix %s in workspace.\n", norm);
            return TPBE_FILE_IO_FAIL;
        }
        if (nmatch > 1) {
            print_ambiguous_id_table(norm, matches, nmatch, workspace);
            free(matches);
            return TPBE_CLI_FAIL;
        }

        if (tpb_rawdb_hex_to_id(matches[0].full_hex, id) != TPBE_SUCCESS) {
            free(matches);
            return TPBE_CLI_FAIL;
        }
        dom = matches[0].domain;
        free(matches);
        return dump_tpbr_by_domain(workspace, dom, id);
    }
}

static dump_target_t
parse_dump_target(int argc, char **argv, int *consumed_to,
                  char *path_or_hex, size_t pathlen,
                  char *entry_name, size_t entrylen)
{
    int i;

    if (consumed_to) {
        *consumed_to = argc;
    }
    if (!path_or_hex || pathlen == 0) {
        return DUMP_T_NONE;
    }
    path_or_hex[0] = '\0';
    if (entry_name && entrylen > 0) {
        entry_name[0] = '\0';
    }

    for (i = 3; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--id") == 0) {
            if (i + 1 >= argc) {
                return DUMP_T_NONE;
            }
            snprintf(path_or_hex, pathlen, "%s", argv[i + 1]);
            return DUMP_T_ID_GLOBAL;
        }
        if (strcmp(a, "--tbatch-id") == 0) {
            if (i + 1 >= argc) {
                return DUMP_T_NONE;
            }
            snprintf(path_or_hex, pathlen, "%s", argv[i + 1]);
            return DUMP_T_TBATCH_ID;
        }
        if (strcmp(a, "--kernel-id") == 0) {
            if (i + 1 >= argc) {
                return DUMP_T_NONE;
            }
            snprintf(path_or_hex, pathlen, "%s", argv[i + 1]);
            return DUMP_T_KERNEL_ID;
        }
        if (strcmp(a, "--task-id") == 0) {
            if (i + 1 >= argc) {
                return DUMP_T_NONE;
            }
            snprintf(path_or_hex, pathlen, "%s", argv[i + 1]);
            return DUMP_T_TASK_ID;
        }
        if (strcmp(a, "--score-id") == 0) {
            if (i + 1 >= argc) {
                return DUMP_T_NONE;
            }
            snprintf(path_or_hex, pathlen, "%s", argv[i + 1]);
            return DUMP_T_SCORE_ID;
        }
        if (strcmp(a, "--file") == 0) {
            if (i + 1 >= argc) {
                return DUMP_T_NONE;
            }
            snprintf(path_or_hex, pathlen, "%s", argv[i + 1]);
            return DUMP_T_FILE;
        }
        if (strcmp(a, "--entry") == 0) {
            if (i + 1 >= argc || !entry_name) {
                return DUMP_T_NONE;
            }
            snprintf(entry_name, entrylen, "%s", argv[i + 1]);
            return DUMP_T_ENTRY;
        }
    }
    return DUMP_T_NONE;
}

static void
dump_headers(const tpb_meta_header_t *hdrs, uint32_t n)
{
    uint32_t i, j;

    for (i = 0; i < n; i++) {
        const tpb_meta_header_t *h = &hdrs[i];
        char key[320];

        snprintf(key, sizeof(key), "header[%" PRIu32 "].block_size", i);
        dump_print_kv_u32(key, h->block_size);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].ndim", i);
        dump_print_kv_u32(key, h->ndim);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].data_size", i);
        dump_print_kv_u64(key, h->data_size);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].type_bits", i);
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s, 0x%08" PRIx32 "\n",
                   key, h->type_bits);
        snprintf(key, sizeof(key), "header[%" PRIu32 "]._reserve", i);
        dump_print_kv_u32(key, h->_reserve);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].uattr_bits", i);
        dump_print_kv_u64(key, h->uattr_bits);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].name", i);
        dump_print_kv_str(key, h->name);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].note", i);
        dump_print_kv_str(key, h->note);

        for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
            snprintf(key, sizeof(key), "header[%" PRIu32 "].dimsizes[%" PRIu32 "]",
                     i, j);
            dump_print_kv_u64(key, h->dimsizes[j]);
        }
        for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
            snprintf(key, sizeof(key), "header[%" PRIu32 "].dimnames[%" PRIu32 "]",
                     i, j);
            dump_print_kv_str(key, h->dimnames[j]);
        }
    }
}

static size_t
dtype_elem_size_from_typebits(uint32_t type_bits)
{
    TPB_DTYPE dt = (TPB_DTYPE)(type_bits & TPB_PARM_TYPE_MASK);
    uint32_t code = (uint32_t)(dt & 0xFFFFu);
    uint32_t nbytes = (code >> 8) & 0xFFu;
    if (nbytes == 0) {
        nbytes = 8;
    }
    return (size_t)nbytes;
}

static void
dump_header_record_lines(const tpb_meta_header_t *h, const uint8_t *blob,
                         uint32_t nd, uint64_t mult[TPBM_DATA_NDIM_MAX],
                         uint64_t idx[TPBM_DATA_NDIM_MAX], int lev,
                         size_t elem_size)
{
    if (lev == 0) {
        uint64_t i0;
        uint64_t fix = 0;
        uint32_t k;

        for (k = 1; k < nd; k++) {
            fix += idx[k] * mult[k];
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s", h->name);
        for (k = nd; k-- > 1u; ) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "[%" PRIu64 "]", idx[k]);
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "[], ");
        for (i0 = 0; i0 < h->dimsizes[0]; i0++) {
            uint64_t L = fix + i0;
            if (i0 > 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT, ", ");
            }
            print_elem_csv(blob + (size_t)(L * elem_size),
                           h->type_bits, elem_size);
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
        return;
    }
    {
        uint64_t t;
        uint64_t kmax = h->dimsizes[lev];
        for (t = 0; t < kmax; t++) {
            idx[lev] = t;
            dump_header_record_lines(h, blob, nd, mult, idx,
                                     lev - 1, elem_size);
        }
    }
}

static void
print_elem_csv(const uint8_t *p, uint32_t type_bits, size_t elem_size)
{
    TPB_DTYPE dt = (TPB_DTYPE)(type_bits & TPB_PARM_TYPE_MASK);

    if (elem_size == 20u) {
        char hx[41];
        tpb_rawdb_id_to_hex(p, hx);
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s", hx);
        return;
    }

    switch (dt) {
    case TPB_INT8_T:
    case TPB_UINT8_T:
    case TPB_CHAR_T:
    case TPB_UNSIGNED_CHAR_T:
    case TPB_SIGNED_CHAR_T:
    case TPB_BYTE_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%d", (int)*p);
        break;
    case TPB_INT16_T:
    case TPB_UINT16_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%d", (int)*(const int16_t *)(const void *)p);
        break;
    case TPB_INT32_T:
    case TPB_UINT32_T:
    case TPB_INT_T:
    case TPB_UNSIGNED_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%d", (int)*(const int32_t *)(const void *)p);
        break;
    case TPB_INT64_T:
    case TPB_UINT64_T:
    case TPB_LONG_T:
    case TPB_UNSIGNED_LONG_T:
    case TPB_LONG_LONG_T:
    case TPB_UNSIGNED_LONG_LONG_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%" PRId64,
                   (int64_t)*(const int64_t *)(const void *)p);
        break;
    case TPB_FLOAT_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%.8g",
                   (double)*(const float *)(const void *)p);
        break;
    case TPB_DOUBLE_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%.16g",
                   *(const double *)(const void *)p);
        break;
    case TPB_LONG_DOUBLE_T:
        tpb_printf(TPBM_PRTN_M_DIRECT, "%.16Lg",
                   (long double)*(const long double *)(const void *)p);
        break;
    default:
        if (elem_size > 0) {
            size_t k;
            for (k = 0; k < elem_size; k++) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "%s%02x",
                           k ? "" : "0x", (unsigned)p[k]);
            }
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "0");
        }
        break;
    }
}

static void
dump_record_data(const tpb_meta_header_t *hdrs, uint32_t nheader,
                 const void *data, uint64_t datasize)
{
    const uint8_t *base = (const uint8_t *)data;
    uint64_t off = 0;
    uint32_t hi;

    tpb_printf(TPBM_PRTN_M_DIRECT, "Record Data\n");

    for (hi = 0; hi < nheader; hi++) {
        const tpb_meta_header_t *h = &hdrs[hi];
        uint64_t dsz = h->data_size;
        uint32_t nd = h->ndim;
        uint64_t total_el = 1;
        uint32_t j;
        uint64_t mult[TPBM_DATA_NDIM_MAX];
        size_t elem_size;
        const uint8_t *blob;

        if (dsz == 0 || off + dsz > datasize) {
            break;
        }
        blob = base + off;
        off += dsz;

        if (nd > TPBM_DATA_NDIM_MAX) {
            nd = TPBM_DATA_NDIM_MAX;
        }

        for (j = 0; j < nd; j++) {
            uint64_t d = h->dimsizes[j];
            if (d == 0) {
                total_el = 0;
                break;
            }
            if (total_el > UINT64_MAX / d) {
                total_el = 0;
                break;
            }
            total_el *= d;
        }

        if (total_el == 0) {
            continue;
        }

        elem_size = (size_t)(dsz / total_el);
        if (elem_size == 0 || (uint64_t)elem_size * total_el != dsz) {
            elem_size = dtype_elem_size_from_typebits(h->type_bits);
            if (elem_size == 0 || (uint64_t)elem_size * total_el != dsz) {
                elem_size = 1;
            }
        }

        mult[0] = 1u;
        for (j = 1; j < nd; j++) {
            mult[j] = mult[j - 1] * h->dimsizes[j - 1];
        }

        if (nd <= 1) {
            uint64_t d0 = nd == 0 ? 1u : h->dimsizes[0];
            uint64_t i0;
            tpb_printf(TPBM_PRTN_M_DIRECT, "%s[], ", h->name);
            for (i0 = 0; i0 < d0; i0++) {
                if (i0 > 0) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, ", ");
                }
                print_elem_csv(blob + (size_t)(i0 * elem_size),
                               h->type_bits, elem_size);
            }
            tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
            continue;
        }

        {
            uint64_t idx[TPBM_DATA_NDIM_MAX];
            memset(idx, 0, sizeof(idx));
            dump_header_record_lines(h, blob, nd, mult, idx,
                                     (int)nd - 1, elem_size);
        }
    }
}

static int
dump_tpbr_tbatch(const char *workspace, const unsigned char id[20])
{
    tbatch_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_rawdb_record_read_tbatch(workspace, id, &attr,
                                        &data, &datasize);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Metadata\n");
    dump_print_kv_hex20("tbatch_id", attr.tbatch_id);
    dump_print_kv_hex20("dup_to", attr.dup_to);
    dump_print_kv_hex20("dup_from", attr.dup_from);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        dump_print_kv_str("start_utc", ts.str);
    } else {
        dump_print_kv_u64("utc_bits", attr.utc_bits);
    }
    dump_print_kv_u64("btime", attr.btime);
    dump_print_kv_u64("duration", attr.duration);
    dump_print_kv_str("hostname", attr.hostname);
    dump_print_kv_str("username", attr.username);
    dump_print_kv_u32("front_pid", attr.front_pid);
    dump_print_kv_u32("nkernel", attr.nkernel);
    dump_print_kv_u32("ntask", attr.ntask);
    dump_print_kv_u32("nscore", attr.nscore);
    dump_print_kv_u32("batch_type", attr.batch_type);
    dump_print_kv_u32("nheader", attr.nheader);

    dump_headers(attr.headers, attr.nheader);
    dump_record_data(attr.headers, attr.nheader, data, datasize);

    free(data);
    tpb_rawdb_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_kernel(const char *workspace, const unsigned char id[20])
{
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_rawdb_record_read_kernel(workspace, id, &attr,
                                       &data, &datasize);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Metadata\n");
    dump_print_kv_hex20("kernel_id", attr.kernel_id);
    dump_print_kv_hex20("dup_to", attr.dup_to);
    dump_print_kv_hex20("dup_from", attr.dup_from);
    dump_print_kv_hex20("src_sha1", attr.src_sha1);
    dump_print_kv_hex20("so_sha1", attr.so_sha1);
    dump_print_kv_hex20("bin_sha1", attr.bin_sha1);
    dump_print_kv_str("kernel_name", attr.kernel_name);
    dump_print_kv_str("version", attr.version);
    dump_print_kv_str("description", attr.description);
    dump_print_kv_u32("nparm", attr.nparm);
    dump_print_kv_u32("nmetric", attr.nmetric);
    dump_print_kv_u32("kctrl", attr.kctrl);
    dump_print_kv_u32("nheader", attr.nheader);
    dump_print_kv_u32("reserve", attr.reserve);

    dump_headers(attr.headers, attr.nheader);
    dump_record_data(attr.headers, attr.nheader, data, datasize);

    free(data);
    tpb_rawdb_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_task(const char *workspace, const unsigned char id[20])
{
    task_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_rawdb_record_read_task(workspace, id, &attr,
                                     &data, &datasize);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Metadata\n");
    dump_print_kv_hex20("task_record_id", attr.task_record_id);
    dump_print_kv_hex20("dup_to", attr.dup_to);
    dump_print_kv_hex20("dup_from", attr.dup_from);
    dump_print_kv_hex20("tbatch_id", attr.tbatch_id);
    dump_print_kv_hex20("kernel_id", attr.kernel_id);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        dump_print_kv_str("start_utc", ts.str);
    } else {
        dump_print_kv_u64("utc_bits", attr.utc_bits);
    }
    dump_print_kv_u64("btime", attr.btime);
    dump_print_kv_u64("duration", attr.duration);
    dump_print_kv_u32("exit_code", attr.exit_code);
    dump_print_kv_u32("handle_index", attr.handle_index);
    dump_print_kv_u32("pid", attr.pid);
    dump_print_kv_u32("tid", attr.tid);
    dump_print_kv_u32("ninput", attr.ninput);
    dump_print_kv_u32("noutput", attr.noutput);
    dump_print_kv_u32("nheader", attr.nheader);
    dump_print_kv_u32("reserve", attr.reserve);

    dump_headers(attr.headers, attr.nheader);
    dump_record_data(attr.headers, attr.nheader, data, datasize);

    free(data);
    tpb_rawdb_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbe_domain(const char *workspace, uint8_t domain)
{
    int err;
    int n, i;
    const char *fname = NULL;

    if (domain == TPB_RAWDB_DOM_KERNEL) {
        fname = TPB_RAWDB_KERNEL_ENTRY;
    } else if (domain == TPB_RAWDB_DOM_TASK) {
        fname = TPB_RAWDB_TASK_ENTRY;
    } else {
        fname = TPB_RAWDB_TBATCH_ENTRY;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Entry File: %s\n", fname);

    if (domain == TPB_RAWDB_DOM_TBATCH) {
        tbatch_entry_t *e = NULL;
        err = tpb_rawdb_entry_list_tbatch(workspace, &e, &n);
        if (err != TPBE_SUCCESS) {
            return err;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "(%d entries)\n", n);
        for (i = 0; i < n; i++) {
            char p[80];
            tpb_datetime_str_t ts;
            snprintf(p, sizeof(p), "entry[%d].tbatch_id", i);
            dump_print_kv_hex20(p, e[i].tbatch_id);
            snprintf(p, sizeof(p), "entry[%d].dup_from", i);
            dump_print_kv_hex20(p, e[i].dup_from);
            snprintf(p, sizeof(p), "entry[%d].start_utc", i);
            if (tpb_ts_bits_to_isoutc(e[i].start_utc_bits, &ts) == 0) {
                dump_print_kv_str(p, ts.str);
            } else {
                snprintf(p, sizeof(p), "entry[%d].start_utc_bits", i);
                dump_print_kv_u64(p, e[i].start_utc_bits);
            }
            snprintf(p, sizeof(p), "entry[%d].duration", i);
            dump_print_kv_u64(p, e[i].duration);
            snprintf(p, sizeof(p), "entry[%d].hostname", i);
            dump_print_kv_str(p, e[i].hostname);
            snprintf(p, sizeof(p), "entry[%d].nkernel", i);
            dump_print_kv_u32(p, e[i].nkernel);
            snprintf(p, sizeof(p), "entry[%d].ntask", i);
            dump_print_kv_u32(p, e[i].ntask);
            snprintf(p, sizeof(p), "entry[%d].nscore", i);
            dump_print_kv_u32(p, e[i].nscore);
            snprintf(p, sizeof(p), "entry[%d].batch_type", i);
            dump_print_kv_u32(p, e[i].batch_type);
        }
        free(e);
        return TPBE_SUCCESS;
    }

    if (domain == TPB_RAWDB_DOM_KERNEL) {
        kernel_entry_t *e = NULL;
        err = tpb_rawdb_entry_list_kernel(workspace, &e, &n);
        if (err != TPBE_SUCCESS) {
            return err;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "(%d entries)\n", n);
        for (i = 0; i < n; i++) {
            char p[80];
            snprintf(p, sizeof(p), "entry[%d].kernel_id", i);
            dump_print_kv_hex20(p, e[i].kernel_id);
            snprintf(p, sizeof(p), "entry[%d].dup_from", i);
            dump_print_kv_hex20(p, e[i].dup_from);
            snprintf(p, sizeof(p), "entry[%d].kernel_name", i);
            dump_print_kv_str(p, e[i].kernel_name);
            snprintf(p, sizeof(p), "entry[%d].so_sha1", i);
            dump_print_kv_hex20(p, e[i].so_sha1);
            snprintf(p, sizeof(p), "entry[%d].kctrl", i);
            dump_print_kv_u32(p, e[i].kctrl);
            snprintf(p, sizeof(p), "entry[%d].nparm", i);
            dump_print_kv_u32(p, e[i].nparm);
            snprintf(p, sizeof(p), "entry[%d].nmetric", i);
            dump_print_kv_u32(p, e[i].nmetric);
        }
        free(e);
        return TPBE_SUCCESS;
    }

    {
        task_entry_t *e = NULL;
        err = tpb_rawdb_entry_list_task(workspace, &e, &n);
        if (err != TPBE_SUCCESS) {
            return err;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "(%d entries)\n", n);
        for (i = 0; i < n; i++) {
            char p[96];
            tpb_datetime_str_t ts;
            snprintf(p, sizeof(p), "entry[%d].task_record_id", i);
            dump_print_kv_hex20(p, e[i].task_record_id);
            snprintf(p, sizeof(p), "entry[%d].dup_from", i);
            dump_print_kv_hex20(p, e[i].dup_from);
            snprintf(p, sizeof(p), "entry[%d].tbatch_id", i);
            dump_print_kv_hex20(p, e[i].tbatch_id);
            snprintf(p, sizeof(p), "entry[%d].kernel_id", i);
            dump_print_kv_hex20(p, e[i].kernel_id);
            snprintf(p, sizeof(p), "entry[%d].start_utc", i);
            if (tpb_ts_bits_to_isoutc(e[i].utc_bits, &ts) == 0) {
                dump_print_kv_str(p, ts.str);
            } else {
                snprintf(p, sizeof(p), "entry[%d].utc_bits", i);
                dump_print_kv_u64(p, e[i].utc_bits);
            }
            snprintf(p, sizeof(p), "entry[%d].duration", i);
            dump_print_kv_u64(p, e[i].duration);
            snprintf(p, sizeof(p), "entry[%d].exit_code", i);
            dump_print_kv_u32(p, e[i].exit_code);
            snprintf(p, sizeof(p), "entry[%d].handle_index", i);
            dump_print_kv_u32(p, e[i].handle_index);
        }
        free(e);
    }
    return TPBE_SUCCESS;
}

static int
dump_file_path(const char *workspace, const char *filepath)
{
    char resolved[TPB_RAWDB_PATH_MAX];
    uint8_t ftype, domain;
    int err;
    unsigned char id[20];
    char base[256];
    const char *bn;

    err = resolve_file_path(workspace, filepath, resolved, sizeof(resolved));
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpb_rawdb_detect_file(resolved, &ftype, &domain);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (ftype == TPB_RAWDB_FTYPE_ENTRY) {
        return dump_tpbe_domain(workspace, domain);
    }

    bn = strrchr(resolved, '/');
    bn = bn ? bn + 1 : resolved;
    snprintf(base, sizeof(base), "%s", bn);
    {
        char *dot = strstr(base, ".tpbr");
        if (dot) {
            *dot = '\0';
        }
    }
    if (tpb_rawdb_hex_to_id(base, id) == TPBE_SUCCESS) {
        /* full 40-char id */
    } else {
        char norm[48];
        size_t plen;
        id_prefix_match_t *matches = NULL;
        int nmatch = 0;
        int scan_err;

        if (parse_id_hex_arg(base, norm, sizeof(norm), &plen) != TPBE_SUCCESS ||
            plen >= 40) {
            return TPBE_CLI_FAIL;
        }
        scan_err = scan_tpbr_prefix_matches(workspace, norm, plen, domain,
                                            &matches, &nmatch);
        if (scan_err != TPBE_SUCCESS) {
            free(matches);
            return scan_err;
        }
        if (nmatch == 0) {
            free(matches);
            return TPBE_CLI_FAIL;
        }
        if (nmatch > 1) {
            print_ambiguous_id_table(norm, matches, nmatch, workspace);
            free(matches);
            return TPBE_CLI_FAIL;
        }
        if (tpb_rawdb_hex_to_id(matches[0].full_hex, id) != TPBE_SUCCESS) {
            free(matches);
            return TPBE_CLI_FAIL;
        }
        free(matches);
    }

    if (domain == TPB_RAWDB_DOM_TBATCH) {
        return dump_tpbr_tbatch(workspace, id);
    }
    if (domain == TPB_RAWDB_DOM_KERNEL) {
        return dump_tpbr_kernel(workspace, id);
    }
    return dump_tpbr_task(workspace, id);
}

/*
 * When: `database dump` is selected (argv[2] == "dump").
 * Input: full argc/argv and resolved workspace string.
 * Output: Parsed flags choose tpbr/tpbe dump path; return TPBE_* (see header).
 */
int
tpbcli_database_dump(int argc, char **argv, const char *workspace)
{
    dump_target_t t;
    int dummy;
    char buf[TPB_RAWDB_PATH_MAX];
    char entrybuf[256];
    uint8_t dom;

    t = parse_dump_target(argc, argv, &dummy, buf, sizeof(buf),
                          entrybuf, sizeof(entrybuf));
    if (t == DUMP_T_NONE) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Usage: tpbcli database dump "
                   "[--id <hex>|--tbatch-id <hex>|--kernel-id <hex>|"
                   "--task-id <hex>|--score-id <hex>|--file <path>|"
                   "--entry <name>]\n"
                   "Enter a ID, a file name in the workspace, or a full path to a tpbe or tpbr file.\n");
        return TPBE_CLI_FAIL;
    }

    if (t == DUMP_T_SCORE_ID) {
        (void)buf;
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "Score records are not implemented in rawdb yet.\n");
        return TPBE_SUCCESS;
    }

    if (t == DUMP_T_ENTRY) {
        char norm[256];
        if (normalize_entry_name(entrybuf, norm, sizeof(norm)) != TPBE_SUCCESS) {
            return TPBE_CLI_FAIL;
        }
        if (entry_name_to_domain(norm, &dom) != TPBE_SUCCESS) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Unknown entry name \"%s\". Try: task_batch, kernel, task\n",
                       entrybuf);
            return TPBE_CLI_FAIL;
        }
        return dump_tpbe_domain(workspace, dom);
    }

    if (t == DUMP_T_FILE) {
        return dump_file_path(workspace, buf);
    }

    return resolve_hex_arg_and_dump(workspace, t, buf);
}
