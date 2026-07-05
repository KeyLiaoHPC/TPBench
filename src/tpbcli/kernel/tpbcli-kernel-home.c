/*
 * tpbcli-kernel-home.c
 * TPB_HOME resolution and kernel-name validation for kernel init/build.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
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
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_NULLPTR_ARG, NULL);
    }

    if (override != NULL && override[0] != '\0') {
        if (strlen(override) >= outlen) {
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
        }
        snprintf(out, outlen, "%s", override);
        return TPBE_SUCCESS;
    }

    env_home = getenv("TPB_HOME");
    if (env_home != NULL && env_home[0] != '\0') {
        if (strlen(env_home) >= outlen) {
            TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
        }
        snprintf(out, outlen, "%s", env_home);
        return TPBE_SUCCESS;
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    if (snprintf(out, outlen, "%s/.tpbench", home) >= (int)outlen) {
        TPB_FAIL(TPB_MOD_CLI_KERNEL, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}
