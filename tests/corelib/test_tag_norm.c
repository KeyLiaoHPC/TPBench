/*
 * test_tag_norm.c
 * Pack A11: tag normalize / validate / display helpers.
 */

#include <stdio.h>
#include <string.h>

#include "include/tpb-public.h"
#include "corelib/tpb-tag-norm.h"

typedef struct {
    const char *id;
    const char *name;
    int (*fn)(void);
} case_t;

static int
test_normalize(void)
{
    char out[256];

    if (_sf_normalize_tags(out, sizeof(out), "tpbfom,bandwidth,TPBFOM") != 0) {
        return 1;
    }
    if (strcmp(out, "BANDWIDTH,TPBFOM") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    if (_sf_finalize_role_tags(out, sizeof(out), "tpbfom,bandwidth,TPBFOM",
                               TPB_TAG_OUTPUT) != 0) {
        return 1;
    }
    if (strcmp(out, "BANDWIDTH,TPBFOM,TPBOUTPUT") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    if (_sf_finalize_role_tags(out, sizeof(out), NULL, TPB_TAG_INPUT) != 0) {
        return 1;
    }
    if (strcmp(out, "TPBINPUT") != 0) {
        return 1;
    }
    /* ENV-sourced args append both TPBINPUT and TPBENVVAR. */
    if (_sf_finalize_role_tags(out, sizeof(out), NULL,
                               TPB_TAG_INPUT "," TPB_TAG_ENVVAR) != 0) {
        return 1;
    }
    if (strcmp(out, "TPBENVVAR,TPBINPUT") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    return 0;
}

static int
test_validate(void)
{
    if (_sf_validate_header_name("Triad") != 0) {
        return 1;
    }
    if (_sf_validate_header_name("bad:name") == 0) {
        return 1;
    }
    if (_sf_validate_header_name("") == 0) {
        return 1;
    }
    if (_sf_validate_user_tags("TPBFOM,BANDWIDTH") != 0) {
        return 1;
    }
    if (_sf_validate_user_tags("bad:tag") == 0) {
        return 1;
    }
    return 0;
}

static int
test_display(void)
{
    char out[256];

    if (_sf_format_tags_display(out, sizeof(out),
                                "BANDWIDTH,TPBFOM,TPBOUTPUT") != 0) {
        return 1;
    }
    if (strcmp(out, "BANDWIDTH, TPBFOM, TPBOUTPUT") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    return 0;
}

static int
test_idempotent(void)
{
    char a[256];
    char b[256];

    if (_sf_normalize_tags(a, sizeof(a), "time,event,TIME") != 0) {
        return 1;
    }
    if (_sf_normalize_tags(b, sizeof(b), a) != 0) {
        return 1;
    }
    return strcmp(a, b) != 0;
}

static int
test_preset_macros(void)
{
    if (strcmp(TPB_TAG_INPUT, "TPBINPUT") != 0 ||
        strcmp(TPB_TAG_ENVVAR, "TPBENVVAR") != 0 ||
        strcmp(TPB_TAG_OUTPUT, "TPBOUTPUT") != 0 ||
        strcmp(TPB_TAG_FOM, "TPBFOM") != 0 ||
        strcmp(TPB_TAG_VERIFYVAR, "TPBVERIFYVAR") != 0 ||
        strcmp(TPB_TAG_LINK, "TPBLINK") != 0) {
        return 1;
    }
    return 0;
}

static int
test_dedupe_combo(void)
{
    char out[256];

    /* User already lists TPBOUTPUT; system append must not duplicate. */
    if (_sf_finalize_role_tags(out, sizeof(out), "TPBOUTPUT,TPBFOM",
                               TPB_TAG_OUTPUT) != 0) {
        return 1;
    }
    if (strcmp(out, "TPBFOM,TPBOUTPUT") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    if (_sf_finalize_role_tags(out, sizeof(out), TPB_TAG_INPUT,
                               TPB_TAG_INPUT "," TPB_TAG_ENVVAR) != 0) {
        return 1;
    }
    if (strcmp(out, "TPBENVVAR,TPBINPUT") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    return 0;
}

static const case_t cases[] = {
    { "A11.1", "normalize", test_normalize },
    { "A11.2", "validate", test_validate },
    { "A11.3", "display", test_display },
    { "A11.4", "idempotent", test_idempotent },
    { "A11.5", "preset_macros", test_preset_macros },
    { "A11.6", "dedupe_combo", test_dedupe_combo },
};

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (filter != NULL && strcmp(filter, cases[i].id) != 0 &&
            strcmp(filter, cases[i].name) != 0) {
            continue;
        }
        if (cases[i].fn() != 0) {
            fprintf(stderr, "[%s] %s FAIL\n", cases[i].id, cases[i].name);
            fail = 1;
        } else {
            fprintf(stderr, "[%s] %s PASS\n", cases[i].id, cases[i].name);
        }
    }
    return fail;
}
