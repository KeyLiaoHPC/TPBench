/*
 * tpbcli-kernel-home.c
 * TPB_HOME resolution and kernel-name validation for kernel init/build.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/tpb-public.h"
#include "corelib/rafdb/tpb-raf-types.h"
#include "tpbcli-kernel-home.h"

int
tpbcli_kernel_name_valid(const char *kernel_name)
{
    size_t len;
    size_t i;

    if (kernel_name == NULL || kernel_name[0] == '\0') {
        return 0;
    }
    if (!isalpha((unsigned char)kernel_name[0]) &&
        kernel_name[0] != '_') {
        return 0;
    }
    len = strlen(kernel_name);
    for (i = 1; i < len; i++) {
        if (!isalnum((unsigned char)kernel_name[i]) &&
            kernel_name[i] != '_') {
            return 0;
        }
    }
    return 1;
}

int
tpbcli_kernel_resolve_home(const char *override, char *out, size_t outlen)
{
    const char *env_home;
    const char *home;

    if (out == NULL || outlen == 0) {
        return TPBE_NULLPTR_ARG;
    }

    if (override != NULL && override[0] != '\0') {
        if (strlen(override) >= outlen) {
            return TPBE_FILE_IO_FAIL;
        }
        snprintf(out, outlen, "%s", override);
        return TPBE_SUCCESS;
    }

    env_home = getenv("TPB_HOME");
    if (env_home != NULL && env_home[0] != '\0') {
        if (strlen(env_home) >= outlen) {
            return TPBE_FILE_IO_FAIL;
        }
        snprintf(out, outlen, "%s", env_home);
        return TPBE_SUCCESS;
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return TPBE_FILE_IO_FAIL;
    }
    if (snprintf(out, outlen, "%s/%s", home, TPB_RAF_DEFAULT_DIR) >= (int)outlen) {
        return TPBE_FILE_IO_FAIL;
    }
    return TPBE_SUCCESS;
}
