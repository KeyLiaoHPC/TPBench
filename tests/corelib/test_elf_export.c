/*
 * test_elf_export.c
 * Pack A12: tpb-elf-export classify / load-group audit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef TPB_TEST_ELF_EXPORT
#define TPB_TEST_ELF_EXPORT "./bin/tpb-elf-export"
#endif

#ifndef TPB_TEST_WORKDIR
#define TPB_TEST_WORKDIR "/tmp/tpb-elf-export-test"
#endif

static int g_pass;
static int g_fail;

#define FAIL(msg) do { \
    g_fail++; \
    fprintf(stderr, "  FAIL: %s\n", (msg)); \
} while (0)

#define PASS() do { g_pass++; } while (0)

static int
run_cmd(const char *cmd)
{
    int rc = system(cmd);

    if (rc == -1) {
        return -1;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return -1;
}

static int
write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        return -1;
    }
    fputs(text, fp);
    fclose(fp);
    return 0;
}

static int
read_file(const char *path, char *out, size_t outlen)
{
    FILE *fp = fopen(path, "r");
    size_t n;

    if (fp == NULL) {
        return -1;
    }
    n = fread(out, 1, outlen - 1, fp);
    out[n] = '\0';
    fclose(fp);
    return 0;
}

static int
setup_audit_fixture(void)
{
    char cmd[1024];

    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\"",
             TPB_TEST_WORKDIR, TPB_TEST_WORKDIR);
    if (run_cmd(cmd) != 0) {
        return -1;
    }
    if (write_file(TPB_TEST_WORKDIR "/dep.c",
                   "void tpb_test_missing(void);\n"
                   "void tpb_test_dep_call(void) { tpb_test_missing(); }\n")
        != 0) {
        return -1;
    }
    if (write_file(TPB_TEST_WORKDIR "/plugin.c",
                   "int tpb_test_plugin_marker;\n") != 0) {
        return -1;
    }
    snprintf(cmd, sizeof(cmd),
             "cd \"%s\" && cc -shared -fPIC dep.c -o libdep.so && "
             "cc -shared -fPIC plugin.c -L. -ldep -Wl,-rpath,. -o libplugin.so",
             TPB_TEST_WORKDIR);
    return run_cmd(cmd);
}

static int
test_a12_1_audit_unsatisfied(void)
{
    char cmd[2048];
    char log[4096];
    int rc;

    if (setup_audit_fixture() != 0) {
        FAIL("A12.1 fixture setup");
        return 1;
    }
    snprintf(cmd, sizeof(cmd),
             "\"" TPB_TEST_ELF_EXPORT
             "\" audit-needed \"%s/libplugin.so\" "
             ">\"%s/audit.out\" 2>&1",
             TPB_TEST_WORKDIR, TPB_TEST_WORKDIR);
    rc = run_cmd(cmd);
    if (rc == 0) {
        FAIL("A12.1 audit-needed should fail");
        return 1;
    }
    if (read_file(TPB_TEST_WORKDIR "/audit.out", log, sizeof(log)) != 0) {
        FAIL("A12.1 read audit.out");
        return 1;
    }
    if (strstr(log, "undefined symbol") == NULL) {
        fprintf(stderr, "    audit.out: %.500s\n", log);
        FAIL("A12.1 missing 'undefined symbol'");
        return 1;
    }
    if (strstr(log, "libdep") == NULL) {
        fprintf(stderr, "    audit.out: %.500s\n", log);
        FAIL("A12.1 missing 'libdep'");
        return 1;
    }
    PASS();
    return 0;
}

static int
classify_one(const char *so, char *out, size_t outlen)
{
    char cmd[1024];
    FILE *fp;

    snprintf(cmd, sizeof(cmd),
             "\"" TPB_TEST_ELF_EXPORT "\" classify-dso \"%s\"", so);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }
    if (fgets(out, (int)outlen, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    return 0;
}

static int
test_a12_2_classify_dso(void)
{
    char cmd[1024];
    char fam[64];

    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\"",
             TPB_TEST_WORKDIR, TPB_TEST_WORKDIR);
    if (run_cmd(cmd) != 0) {
        FAIL("A12.2 mkdir");
        return 1;
    }
    if (write_file(TPB_TEST_WORKDIR "/dummy.c", "void tpb_dummy(void) {}\n")
        != 0) {
        FAIL("A12.2 write dummy.c");
        return 1;
    }
    snprintf(cmd, sizeof(cmd),
             "cd \"%s\" && cc -shared -fPIC dummy.c -o libdummy.so",
             TPB_TEST_WORKDIR);
    if (run_cmd(cmd) != 0) {
        FAIL("A12.2 compile libdummy.so");
        return 1;
    }
    snprintf(cmd, sizeof(cmd), "%s/libdummy.so", TPB_TEST_WORKDIR);
    if (classify_one(cmd, fam, sizeof(fam)) != 0) {
        FAIL("A12.2 classify-dso");
        return 1;
    }
    if (strstr(fam, "gcc") == NULL && strstr(fam, "clang_rt") == NULL &&
        strstr(fam, "intel") == NULL) {
        FAIL("A12.2 unexpected family");
        return 1;
    }
    PASS();
    return 0;
}

static int
stamp_comment(const char *so, const char *comment_path)
{
    char cmd[2048];

    snprintf(cmd, sizeof(cmd),
             "objcopy --remove-section=.comment \"%s\" && "
             "objcopy --add-section .comment=\"%s\" "
             "--set-section-flags .comment=noload,readonly \"%s\"",
             so, comment_path, so);
    return run_cmd(cmd);
}

static int
test_a12_3_classify_mismatch(void)
{
    char cmd[2048];
    char so_intel[512];
    char so_clang[512];
    char so_plain[512];
    char fam_intel[64];
    char fam_clang[64];
    char fam_plain[64];

    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\"",
             TPB_TEST_WORKDIR, TPB_TEST_WORKDIR);
    if (run_cmd(cmd) != 0) {
        FAIL("A12.3 mkdir");
        return 1;
    }
    if (write_file(TPB_TEST_WORKDIR "/dummy.c", "void tpb_dummy(void) {}\n")
        != 0 ||
        write_file(TPB_TEST_WORKDIR "/c_intel.txt",
                   "Intel(R) C Compiler 2024.0") != 0 ||
        write_file(TPB_TEST_WORKDIR "/c_clang.txt",
                   "clang version 17.0.6 (AOCC)") != 0) {
        FAIL("A12.3 write fixtures");
        return 1;
    }
    snprintf(cmd, sizeof(cmd),
             "cd \"%s\" && cc -shared -fPIC dummy.c -o libintel.so && "
             "cc -shared -fPIC dummy.c -o libclang.so && "
             "cc -shared -fPIC dummy.c -o libplain.so",
             TPB_TEST_WORKDIR);
    if (run_cmd(cmd) != 0) {
        FAIL("A12.3 compile fixtures");
        return 1;
    }
    snprintf(so_intel, sizeof(so_intel), "%s/libintel.so", TPB_TEST_WORKDIR);
    snprintf(so_clang, sizeof(so_clang), "%s/libclang.so", TPB_TEST_WORKDIR);
    snprintf(so_plain, sizeof(so_plain), "%s/libplain.so", TPB_TEST_WORKDIR);
    if (stamp_comment(so_intel, TPB_TEST_WORKDIR "/c_intel.txt") != 0 ||
        stamp_comment(so_clang, TPB_TEST_WORKDIR "/c_clang.txt") != 0) {
        FAIL("A12.3 objcopy .comment");
        return 1;
    }
    snprintf(cmd, sizeof(cmd),
             "objcopy --remove-section=.comment \"%s\"", so_plain);
    (void)run_cmd(cmd);

    if (classify_one(so_intel, fam_intel, sizeof(fam_intel)) != 0 ||
        classify_one(so_clang, fam_clang, sizeof(fam_clang)) != 0 ||
        classify_one(so_plain, fam_plain, sizeof(fam_plain)) != 0) {
        FAIL("A12.3 classify-dso");
        return 1;
    }
    if (strstr(fam_intel, "intel") == NULL) {
        fprintf(stderr, "    intel fam='%s'\n", fam_intel);
        FAIL("A12.3 expected intel from .comment");
        return 1;
    }
    if (strstr(fam_clang, "clang_rt") == NULL) {
        fprintf(stderr, "    clang fam='%s'\n", fam_clang);
        FAIL("A12.3 expected clang_rt from .comment");
        return 1;
    }
    if (strcmp(fam_intel, fam_clang) == 0) {
        FAIL("A12.3 expected distinct toolchain families");
        return 1;
    }
    if (strstr(fam_plain, "intel") != NULL &&
        strstr(fam_plain, "clang_rt") != NULL) {
        FAIL("A12.3 stripped fixture should not be both families");
        return 1;
    }
    PASS();
    return 0;
}

int
main(int argc, char **argv)
{
    const char *id = (argc >= 2) ? argv[1] : "";

    g_pass = 0;
    g_fail = 0;
    if (strcmp(id, "A12.1") == 0) {
        return test_a12_1_audit_unsatisfied();
    }
    if (strcmp(id, "A12.2") == 0) {
        return test_a12_2_classify_dso();
    }
    if (strcmp(id, "A12.3") == 0) {
        return test_a12_3_classify_mismatch();
    }
    test_a12_1_audit_unsatisfied();
    test_a12_2_classify_dso();
    test_a12_3_classify_mismatch();
    return (g_fail > 0) ? 1 : 0;
}
