/**
 * @file tpb-tag-norm.c
 * @brief Tag normalize / validate helpers used by tpb_k_add_arg and add_output.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/tpb-public.h"
#include "tpb-tag-norm.h"

#define TPB_TAG_MAX_TOKENS 64
#define TPB_TAG_TOKEN_MAX  64

/**
 * @brief qsort comparator for uppercase tag tokens (ascending strcmp).
 */
static int
_sf_tag_token_cmp(const void *a, const void *b)
{
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;

    return strcmp(*sa, *sb);
}

/**
 * @brief Uppercase in place; returns 0, or -1 if any ':'.
 */
static int
_sf_toupper_check(char *s)
{
    size_t i;

    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] == ':') {
            return -1;
        }
        s[i] = (char)toupper((unsigned char)s[i]);
    }
    return 0;
}

int
_sf_normalize_tags(char *dst, size_t dst_sz, const char *raw)
{
    char scratch[TPBM_NAME_STR_MAX_LEN];
    char *tokens[TPB_TAG_MAX_TOKENS];
    int ntok = 0;
    char *save = NULL;
    char *tok;
    size_t out_len;
    int i;
    int j;

    if (dst == NULL || dst_sz == 0) {
        return -1;
    }
    dst[0] = '\0';

    if (raw == NULL || raw[0] == '\0') {
        return 0;
    }

    /* Work on a copy so strtok can mutate. */
    snprintf(scratch, sizeof(scratch), "%s", raw);

    for (tok = strtok_r(scratch, ",", &save);
         tok != NULL;
         tok = strtok_r(NULL, ",", &save)) {
        /* Trim leading/trailing spaces and tabs. */
        while (*tok == ' ' || *tok == '\t') {
            tok++;
        }
        {
            size_t len = strlen(tok);

            while (len > 0 && (tok[len - 1] == ' ' || tok[len - 1] == '\t')) {
                tok[--len] = '\0';
            }
        }
        if (tok[0] == '\0') {
            continue;
        }
        if (_sf_toupper_check(tok) != 0) {
            return -1;
        }
        /* Dedupe against tokens already accepted. */
        for (j = 0; j < ntok; j++) {
            if (strcmp(tokens[j], tok) == 0) {
                break;
            }
        }
        if (j < ntok) {
            continue;
        }
        if (ntok >= TPB_TAG_MAX_TOKENS) {
            return -1;
        }
        tokens[ntok++] = tok;
    }

    if (ntok == 0) {
        return 0;
    }

    qsort(tokens, (size_t)ntok, sizeof(tokens[0]), _sf_tag_token_cmp);

    out_len = 0;
    for (i = 0; i < ntok; i++) {
        size_t tlen = strlen(tokens[i]);

        if (i > 0) {
            if (out_len + 1 >= dst_sz) {
                return -1;
            }
            dst[out_len++] = ',';
        }
        if (out_len + tlen >= dst_sz) {
            return -1;
        }
        memcpy(dst + out_len, tokens[i], tlen);
        out_len += tlen;
    }
    dst[out_len] = '\0';
    return 0;
}

int
_sf_validate_header_name(const char *name)
{
    size_t len;
    size_t i;

    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    len = strlen(name);
    if (len > (size_t)(TPBM_NAME_STR_MAX_LEN - 1)) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        if (name[i] == ':') {
            return -1;
        }
    }
    return 0;
}

int
_sf_validate_user_tags(const char *tag)
{
    size_t len;
    size_t i;

    if (tag == NULL || tag[0] == '\0') {
        return 0;
    }
    len = strlen(tag);
    if (len > (size_t)TPBM_TAG_USER_MAX_LEN) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        if (tag[i] == ':') {
            return -1;
        }
    }
    return 0;
}

int
_sf_finalize_role_tags(char *dst, size_t dst_sz,
                       const char *user_tag, const char *sys_tag)
{
    char combined[TPBM_NAME_STR_MAX_LEN + 64];

    if (dst == NULL || dst_sz == 0 || sys_tag == NULL || sys_tag[0] == '\0') {
        return -1;
    }
    if (_sf_validate_user_tags(user_tag) != 0) {
        return -1;
    }

    if (user_tag == NULL || user_tag[0] == '\0') {
        snprintf(combined, sizeof(combined), "%s", sys_tag);
    } else {
        snprintf(combined, sizeof(combined), "%s,%s", user_tag, sys_tag);
    }
    return _sf_normalize_tags(dst, dst_sz, combined);
}

int
_sf_format_tags_display(char *dst, size_t dst_sz, const char *stored)
{
    const char *p;
    size_t out = 0;
    int first = 1;

    if (dst == NULL || dst_sz == 0) {
        return -1;
    }
    dst[0] = '\0';
    if (stored == NULL || stored[0] == '\0') {
        return 0;
    }

    p = stored;
    while (*p != '\0') {
        const char *start;
        size_t tlen;

        while (*p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ',') {
            p++;
        }
        tlen = (size_t)(p - start);
        if (!first) {
            if (out + 2 >= dst_sz) {
                return -1;
            }
            dst[out++] = ',';
            dst[out++] = ' ';
        }
        if (out + tlen >= dst_sz) {
            return -1;
        }
        memcpy(dst + out, start, tlen);
        out += tlen;
        first = 0;
    }
    dst[out] = '\0';
    return 0;
}
