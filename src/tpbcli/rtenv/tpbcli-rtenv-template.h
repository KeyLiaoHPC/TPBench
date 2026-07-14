/**
 * @file tpbcli-rtenv-template.h
 * @brief Shared RTEnv template/merge helpers for CLI.
 */

#ifndef TPBCLI_RTENV_TEMPLATE_H
#define TPBCLI_RTENV_TEMPLATE_H

#include <stdint.h>
#include <stdio.h>

#define TPBCLI_RTENV_MAX_APP   32
#define TPBCLI_RTENV_MAX_VAR   128

typedef struct tpbcli_rtenv_app {
    char name[64];
    char version[64];
    char note[64];
} tpbcli_rtenv_app_t;

typedef struct tpbcli_rtenv_var {
    char key[256];
    char value[4096];
    uint32_t on_set;
    uint32_t on_get;
} tpbcli_rtenv_var_t;

typedef struct tpbcli_rtenv_spec {
    char env_name[256];
    char note[256];
    int32_t inherit_from;
    int has_inherit;
    char inherit_from_text[256];
    tpbcli_rtenv_app_t apps[TPBCLI_RTENV_MAX_APP];
    int napp;
    tpbcli_rtenv_var_t vars[TPBCLI_RTENV_MAX_VAR];
    int nenv;
} tpbcli_rtenv_spec_t;

typedef struct tpbcli_rtenv_merged {
    tpbcli_rtenv_app_t apps[TPBCLI_RTENV_MAX_APP];
    int napp;
    tpbcli_rtenv_var_t vars[TPBCLI_RTENV_MAX_VAR];
    int nenv;
} tpbcli_rtenv_merged_t;

int tpbcli_rtenv_write_template(const tpbcli_rtenv_spec_t *spec, FILE *fp);
int tpbcli_rtenv_write_active_template(const char *workspace, FILE *fp);
int tpbcli_rtenv_parse_template_file(const char *path, tpbcli_rtenv_spec_t *spec,
                                     int *error_line);
int tpbcli_rtenv_resolve_active_id_cli(const char *workspace, int32_t *id_out);
int tpbcli_rtenv_merge_chain(const char *workspace, int32_t target_id,
                             tpbcli_rtenv_merged_t *out);
int tpbcli_rtenv_merged_to_spec(const tpbcli_rtenv_merged_t *merged,
                                tpbcli_rtenv_spec_t *spec);
int tpbcli_rtenv_write_record(const char *workspace, int32_t id,
                              const char *name, const char *note,
                              int32_t inherit_from,
                              const tpbcli_rtenv_spec_t *spec);
int tpbcli_rtenv_find_id_by_name(const char *workspace, const char *name,
                                 int32_t *id_out);
int tpbcli_rtenv_id_exists(const char *workspace, int32_t id);
int tpbcli_rtenv_resolve_list_active(const char *workspace, int32_t *id_out,
                                     int *found_out);
int tpbcli_rtenv_new(int argc, char **argv, const char *workspace);
int tpbcli_rtenv_list(int argc, char **argv, const char *workspace);
int tpbcli_rtenv_show(int argc, char **argv, const char *workspace);
int tpbcli_rtenv_load(int argc, char **argv, const char *workspace);

#endif /* TPBCLI_RTENV_TEMPLATE_H */
