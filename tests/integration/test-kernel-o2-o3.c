/*
 * test-kernel-o2-o3.c
 * Pack C3: O2/O3 stream kernel variants via compile history metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int
main(int argc, char **argv)
{
    char cmd[4096];
    char buf[16384];
    int code;
    int has_o2 = 0;
    int has_o3 = 0;
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

    if (strstr(buf, "kernel_cflags=-O2") != NULL ||
        strstr(buf, "kernel_cflags= -O2") != NULL ||
        strstr(buf, "c_flags=") != NULL) {
        has_o2 = 1;
    }
    if (strstr(buf, "kernel_cflags=-O3") != NULL ||
        strstr(buf, "kernel_cflags= -O3") != NULL) {
        has_o3 = 1;
    }

    if (!has_o2 || !has_o3) {
        fprintf(stderr, "C3.1 FAIL: expected O2 and O3 metadata (o2=%d o3=%d)\n",
                has_o2, has_o3);
        fprintf(stderr, "output: %.800s\n", buf);
        return 1;
    }

    printf("C3.1 PASS: found O2 and O3 stream variants\n");
    return 0;
}
