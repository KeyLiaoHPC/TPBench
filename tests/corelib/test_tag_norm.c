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

    if (_sf_normalize_tags(out, sizeof(out), "fom,bandwidth,FOM") != 0) {
        return 1;
    }
    if (strcmp(out, "BANDWIDTH,FOM") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    if (_sf_finalize_role_tags(out, sizeof(out), "fom,bandwidth,FOM",
                               TPB_TAG_OUT) != 0) {
        return 1;
    }
    if (strcmp(out, "BANDWIDTH,FOM,TPBOUT") != 0) {
        fprintf(stderr, "got '%s'\n", out);
        return 1;
    }
    if (_sf_finalize_role_tags(out, sizeof(out), NULL, TPB_TAG_ARG) != 0) {
        return 1;
    }
    if (strcmp(out, "TPBARG") != 0) {
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
    if (_sf_validate_user_tags("FOM,BANDWIDTH") != 0) {
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

    if (_sf_format_tags_display(out, sizeof(out), "BANDWIDTH,FOM,TPBOUT") != 0) {
        return 1;
    }
    if (strcmp(out, "BANDWIDTH, FOM, TPBOUT") != 0) {
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

static const case_t cases[] = {
    { "A11.1", "normalize", test_normalize },
    { "A11.2", "validate", test_validate },
    { "A11.3", "display", test_display },
    { "A11.4", "idempotent", test_idempotent },
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
