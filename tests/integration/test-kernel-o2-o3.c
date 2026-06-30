/*
 * test-kernel-o2-o3.c
 * Pack C3: O2/O3 stream kernel variants via compile history metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#ifndef TPB_TEST_TPBCLI_STR
#define TPB_TEST_TPBCLI_STR "./bin/tpbcli"
#endif

#ifndef TPB_TEST_KERNEL_WORKSPACE
#define TPB_TEST_KERNEL_WORKSPACE "/tmp/tpb_o2o3_missing"
#endif

static int
run_cmd_capture(const char *cmd, char *outbuf, size_t outbuf_sz)
{
    char full[8192];
    FILE *fp;

    if (outbuf != NULL && outbuf_sz > 0) {
        outbuf[0] = '\0';
    }
    snprintf(full, sizeof(full), "%s 2>&1", cmd);
    fp = popen(full, "r");
    if (fp == NULL) {
        return -1;
    }
    if (outbuf != NULL && outbuf_sz > 0) {
        size_t n = fread(outbuf, 1, outbuf_sz - 1, fp);
        outbuf[n] = '\0';
    } else {
        while (fgetc(fp) != EOF) {
        }
    }
    {
        int st = pclose(fp);
        if (st == -1) {
            return -1;
        }
        return WEXITSTATUS(st);
    }
}

static int
count_kernel_version_rows(const char *buf)
{
    const char *sec;
    const char *line;
    int count = 0;
    int i;

    sec = strstr(buf, "Kernel Versions:");
    if (sec == NULL) {
        return 0;
    }
    line = strchr(sec, '\n');
    if (line == NULL) {
        return 0;
    }
    line = strchr(line + 1, '\n');
    if (line == NULL) {
        return 0;
    }

    while (line != NULL && line[1] != '\0') {
        line++;
        if (strlen(line) < 40) {
            break;
        }
        for (i = 0; i < 40; i++) {
            if (!isxdigit((unsigned char)line[i])) {
                break;
            }
        }
        if (i == 40 && (line[40] == ' ' || line[40] == '\t')) {
            count++;
        }
        line = strchr(line, '\n');
    }
    return count;
}

int
main(int argc, char **argv)
{
    char cmd[4096];
    char buf[16384];
    int code;
    int n_versions;
    (void)argc;
    (void)argv;

    snprintf(cmd, sizeof(cmd),
             "TPB_WORKSPACE=\"%s\" \"" TPB_TEST_TPBCLI_STR
             "\" kernel get -v --kernel stream",
             TPB_TEST_KERNEL_WORKSPACE);
    code = run_cmd_capture(cmd, buf, sizeof(buf));
    if (code != 0) {
        fprintf(stderr, "C3.1 FAIL: get -v exit %d\n", code);
        return 1;
    }

    n_versions = count_kernel_version_rows(buf);
    if (n_versions < 2) {
        fprintf(stderr,
                "C3.1 FAIL: expected at least 2 kernel version rows (got %d)\n",
                n_versions);
        fprintf(stderr, "output: %.800s\n", buf);
        return 1;
    }

    printf("C3.1 PASS: found %d stream kernel version rows\n", n_versions);
    return 0;
}
