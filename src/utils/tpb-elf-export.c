/*
 * tpb-elf-export.c
 * Classify a DSO toolchain family and audit a kernel load group (Linux ELF64).
 */

#define _GNU_SOURCE

#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TPB_ELF_PATH_MAX    4096
#define TPB_ELF_NAME_MAX    256
#define TPB_ELF_NEEDED_MAX  64
#define TPB_ELF_GROUP_MAX   96
#define TPB_ELF_PROV_CAP    65536

typedef struct {
    unsigned char *buf;  /**< Mapped file bytes */
    size_t len;          /**< File size */
} tpb_elf_img_t;

typedef struct {
    char path[TPB_ELF_PATH_MAX];   /**< Canonical file path */
    char label[TPB_ELF_NAME_MAX];  /**< Soname or basename for messages */
    int skip_audit;                /**< Skip-list / libtpbench: providers only */
    tpb_elf_img_t img;             /**< Loaded ELF image */
} tpb_elf_mapped_t;

typedef struct {
    const char *slot[TPB_ELF_PROV_CAP];  /**< Open-addressed name pointers */
    int n;                               /**< Occupied slots */
} tpb_elf_prov_t;

/* Local Function Prototypes */
static int _sf_audit_mapped(tpb_elf_mapped_t *group, int ngrp,
                            const tpb_elf_img_t *kern,
                            const tpb_elf_prov_t *prov);
static int _sf_audit_needed(const char *so_path);
static int _sf_classify_dso(const char *path, char *fam_out, size_t fam_len);
static int _sf_collect_needed(const tpb_elf_img_t *img,
                              char names[][TPB_ELF_NAME_MAX], int *n_out);
static int _sf_collect_providers(const tpb_elf_img_t *img,
                                 tpb_elf_prov_t *prov);
static int _sf_defined_any(const tpb_elf_img_t *img, const char *name);
static void _sf_dso_basename(const char *path, char *out, size_t outlen);
static void _sf_dso_origin(const char *path, char *out, size_t outlen);
static const char *_sf_family_from_text(const char *text);
static int _sf_find_dso(const char *soname, const char *origin,
                        const char *runpath, char *out, size_t outlen);
static Elf64_Shdr *_sf_find_shdr(const tpb_elf_img_t *img, const char *name);
static const char *_sf_find_strtab(const tpb_elf_img_t *img,
                                   const Elf64_Shdr *sym_sh);
static int _sf_group_add(tpb_elf_mapped_t *group, int *ngrp,
                         const char *path, const char *label);
static void _sf_group_free(tpb_elf_mapped_t *group, int ngrp);
static int _sf_group_has(const tpb_elf_mapped_t *group, int ngrp,
                         const char *path);
static unsigned _sf_hash_name(const char *s);
static int _sf_has_needed_lib(const tpb_elf_img_t *img, const char *prefix);
static void _sf_img_free(tpb_elf_img_t *img);
static int _sf_img_load(const char *path, tpb_elf_img_t *img);
static int _sf_is_skip_dso(const char *name);
static int _sf_print_undef_error(const char *sym, const char *label);
static int _sf_prov_add(tpb_elf_prov_t *prov, const char *name);
static int _sf_prov_has(const tpb_elf_prov_t *prov, const char *name);
static int _sf_read_runpath(const tpb_elf_img_t *img, char *out,
                            size_t outlen);
static int _sf_realpath_cpy(const char *in, char *out, size_t outlen);
static int _sf_scan_sec_family(const tpb_elf_img_t *img, const char *sec,
                               char *out, size_t outlen);
static void _sf_usage(void);
static int _sf_valid_ehdr(const tpb_elf_img_t *img);
static int _sf_walk_needed(tpb_elf_mapped_t *group, int *ngrp, int idx);

static void
_sf_usage(void)
{
    fprintf(stderr,
            "Usage: tpb-elf-export <command> [args]\n"
            "  classify-dso <lib.so>\n"
            "  audit-needed <lib.so>\n");
}

static int
_sf_valid_ehdr(const tpb_elf_img_t *img)
{
    const Elf64_Ehdr *eh;

    if (img == NULL || img->buf == NULL || img->len < sizeof(Elf64_Ehdr)) {
        return 0;
    }
    eh = (const Elf64_Ehdr *)img->buf;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0) {
        return 0;
    }
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) {
        return 0;
    }
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) {
        return 0;
    }
    return 1;
}

static int
_sf_img_load(const char *path, tpb_elf_img_t *img)
{
    FILE *fp;
    struct stat st;

    memset(img, 0, sizeof(*img));
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "tpb-elf-export: open %s: %s\n", path,
                strerror(errno));
        return -1;
    }
    if (fstat(fileno(fp), &st) != 0 || st.st_size <= 0) {
        fclose(fp);
        return -1;
    }
    img->len = (size_t)st.st_size;
    img->buf = (unsigned char *)malloc(img->len);
    if (img->buf == NULL) {
        fclose(fp);
        return -1;
    }
    if (fread(img->buf, 1, img->len, fp) != img->len) {
        fclose(fp);
        _sf_img_free(img);
        return -1;
    }
    fclose(fp);
    if (!_sf_valid_ehdr(img)) {
        fprintf(stderr, "tpb-elf-export: not ELF64 LE: %s\n", path);
        _sf_img_free(img);
        return -1;
    }
    return 0;
}

static void
_sf_img_free(tpb_elf_img_t *img)
{
    if (img == NULL) {
        return;
    }
    free(img->buf);
    img->buf = NULL;
    img->len = 0;
}

static Elf64_Shdr *
_sf_find_shdr(const tpb_elf_img_t *img, const char *name)
{
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh;
    const char *shstr;
    uint16_t i;

    eh = (Elf64_Ehdr *)img->buf;
    if ((size_t)eh->e_shoff + (size_t)eh->e_shnum * sizeof(Elf64_Shdr) >
        img->len) {
        return NULL;
    }
    sh = (Elf64_Shdr *)(img->buf + eh->e_shoff);
    if (eh->e_shstrndx >= eh->e_shnum) {
        return NULL;
    }
    shstr = (const char *)(img->buf + sh[eh->e_shstrndx].sh_offset);
    for (i = 0; i < eh->e_shnum; i++) {
        if (strcmp(shstr + sh[i].sh_name, name) == 0) {
            return &sh[i];
        }
    }
    return NULL;
}

static const char *
_sf_find_strtab(const tpb_elf_img_t *img, const Elf64_Shdr *sym_sh)
{
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh;

    eh = (Elf64_Ehdr *)img->buf;
    if (sym_sh->sh_link >= eh->e_shnum) {
        return NULL;
    }
    sh = (Elf64_Shdr *)(img->buf + eh->e_shoff);
    return (const char *)(img->buf + sh[sym_sh->sh_link].sh_offset);
}

static int
_sf_defined_any(const tpb_elf_img_t *img, const char *name)
{
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh;
    uint16_t si;

    eh = (Elf64_Ehdr *)img->buf;
    sh = (Elf64_Shdr *)(img->buf + eh->e_shoff);
    for (si = 0; si < eh->e_shnum; si++) {
        Elf64_Sym *syms;
        const char *str;
        size_t n;
        size_t i;

        if (sh[si].sh_type != SHT_SYMTAB && sh[si].sh_type != SHT_DYNSYM) {
            continue;
        }
        if (sh[si].sh_entsize == 0) {
            continue;
        }
        str = _sf_find_strtab(img, &sh[si]);
        if (str == NULL) {
            continue;
        }
        n = sh[si].sh_size / sh[si].sh_entsize;
        syms = (Elf64_Sym *)(img->buf + sh[si].sh_offset);
        for (i = 1; i < n; i++) {
            if (syms[i].st_shndx == SHN_UNDEF) {
                continue;
            }
            if (strcmp(str + syms[i].st_name, name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int
_sf_is_skip_dso(const char *name)
{
    static const char *const skip[] = {
        "libc.so", "libm.so", "libdl.so", "libpthread.so",
        "ld-linux", "libgcc_s", "libtpbench", "libgomp", "libomp.so",
        "libstdc++", "libatomic", "linux-vdso", "librt.so",
        "libresolv", NULL
    };
    int i;

    if (name == NULL) {
        return 1;
    }
    for (i = 0; skip[i] != NULL; i++) {
        if (strstr(name, skip[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int
_sf_collect_needed(const tpb_elf_img_t *img, char names[][TPB_ELF_NAME_MAX],
                   int *n_out)
{
    Elf64_Ehdr *eh;
    Elf64_Phdr *ph;
    uint16_t i;
    int n = 0;

    eh = (Elf64_Ehdr *)img->buf;
    if (eh->e_phoff == 0) {
        *n_out = 0;
        return 0;
    }
    ph = (Elf64_Phdr *)(img->buf + eh->e_phoff);
    for (i = 0; i < eh->e_phnum; i++) {
        Elf64_Dyn *dyn;
        const char *str = NULL;
        Elf64_Shdr *dynstr;
        size_t ndyn;
        size_t j;

        if (ph[i].p_type != PT_DYNAMIC) {
            continue;
        }
        dynstr = _sf_find_shdr(img, ".dynstr");
        if (dynstr != NULL) {
            str = (const char *)(img->buf + dynstr->sh_offset);
        }
        if (str == NULL) {
            continue;
        }
        dyn = (Elf64_Dyn *)(img->buf + ph[i].p_offset);
        ndyn = ph[i].p_filesz / sizeof(Elf64_Dyn);
        for (j = 0; j < ndyn && n < TPB_ELF_NEEDED_MAX; j++) {
            if (dyn[j].d_tag == DT_NEEDED) {
                snprintf(names[n], TPB_ELF_NAME_MAX, "%s",
                         str + dyn[j].d_un.d_val);
                n++;
            }
        }
    }
    *n_out = n;
    return 0;
}

static int
_sf_read_runpath(const tpb_elf_img_t *img, char *out, size_t outlen)
{
    Elf64_Shdr *dynstr;
    Elf64_Ehdr *eh;
    Elf64_Phdr *ph;
    uint16_t i;
    const char *str = NULL;

    out[0] = '\0';
    dynstr = _sf_find_shdr(img, ".dynstr");
    if (dynstr != NULL) {
        str = (const char *)(img->buf + dynstr->sh_offset);
    }
    eh = (Elf64_Ehdr *)img->buf;
    ph = (Elf64_Phdr *)(img->buf + eh->e_phoff);
    for (i = 0; i < eh->e_phnum && str != NULL; i++) {
        Elf64_Dyn *dyn;
        size_t ndyn;
        size_t j;

        if (ph[i].p_type != PT_DYNAMIC) {
            continue;
        }
        dyn = (Elf64_Dyn *)(img->buf + ph[i].p_offset);
        ndyn = ph[i].p_filesz / sizeof(Elf64_Dyn);
        for (j = 0; j < ndyn; j++) {
            if (dyn[j].d_tag == DT_RUNPATH || dyn[j].d_tag == DT_RPATH) {
                snprintf(out, outlen, "%s", str + dyn[j].d_un.d_val);
                return 0;
            }
        }
    }
    return 0;
}

static int
_sf_has_needed_lib(const tpb_elf_img_t *img, const char *prefix)
{
    char names[TPB_ELF_NEEDED_MAX][TPB_ELF_NAME_MAX];
    int n;
    int i;

    _sf_collect_needed(img, names, &n);
    for (i = 0; i < n; i++) {
        if (strstr(names[i], prefix) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int
_sf_find_dso(const char *soname, const char *origin, const char *runpath,
             char *out, size_t outlen)
{
    char dirs[TPB_ELF_PATH_MAX];
    char *save;
    char *tok;
    const char *ldp;

    if (soname[0] == '/') {
        snprintf(out, outlen, "%s", soname);
        return (access(out, R_OK) == 0) ? 0 : -1;
    }

    snprintf(dirs, sizeof(dirs), "%s:%s:%s",
             (runpath != NULL) ? runpath : "",
             (origin != NULL) ? origin : "",
             ((ldp = getenv("LD_LIBRARY_PATH")) != NULL) ? ldp : "");

    tok = strtok_r(dirs, ":", &save);
    while (tok != NULL) {
        const char *dir = tok;

        if (strncmp(dir, "$ORIGIN", 7) == 0 && origin != NULL) {
            snprintf(out, outlen, "%s%s/%s", origin, dir + 7, soname);
        } else if (dir[0] != '\0') {
            snprintf(out, outlen, "%s/%s", dir, soname);
        } else {
            tok = strtok_r(NULL, ":", &save);
            continue;
        }
        if (access(out, R_OK) == 0) {
            return 0;
        }
        tok = strtok_r(NULL, ":", &save);
    }
    snprintf(out, outlen, "/lib64/%s", soname);
    if (access(out, R_OK) == 0) {
        return 0;
    }
    snprintf(out, outlen, "/usr/lib64/%s", soname);
    if (access(out, R_OK) == 0) {
        return 0;
    }
    return -1;
}

static const char *
_sf_family_from_text(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return NULL;
    }
    if (strcasestr(text, "Intel") != NULL ||
        strstr(text, "icc") != NULL ||
        strstr(text, "icx") != NULL) {
        return "intel";
    }
    if (strcasestr(text, "clang") != NULL ||
        strstr(text, "AOCC") != NULL) {
        return "clang_rt";
    }
    if (strstr(text, "GCC") != NULL || strstr(text, "GNU") != NULL) {
        return "gcc";
    }
    return NULL;
}

static int
_sf_scan_sec_family(const tpb_elf_img_t *img, const char *sec,
                    char *out, size_t outlen)
{
    Elf64_Shdr *sh;
    const char *p;
    size_t i;

    sh = _sf_find_shdr(img, sec);
    if (sh == NULL || sh->sh_size == 0 ||
        (size_t)sh->sh_offset + sh->sh_size > img->len) {
        return 0;
    }
    p = (const char *)(img->buf + sh->sh_offset);
    i = 0;
    while (i < sh->sh_size) {
        size_t rem = sh->sh_size - i;
        size_t n = 0;
        const char *fam;

        while (n < rem && p[i + n] != '\0') {
            n++;
        }
        if (n > 0) {
            char tmp[256];
            size_t cpy = (n < sizeof(tmp) - 1) ? n : sizeof(tmp) - 1;

            memcpy(tmp, p + i, cpy);
            tmp[cpy] = '\0';
            fam = _sf_family_from_text(tmp);
            if (fam != NULL) {
                snprintf(out, outlen, "%s", fam);
                return 1;
            }
        }
        i += n + (n < rem ? 1 : 0);
        if (n == 0) {
            i++;
        }
    }
    return 0;
}

static int
_sf_classify_dso(const char *path, char *fam_out, size_t fam_len)
{
    tpb_elf_img_t img;

    if (_sf_img_load(path, &img) != 0) {
        return -1;
    }
    if (_sf_scan_sec_family(&img, ".comment", fam_out, fam_len) ||
        _sf_scan_sec_family(&img, ".debug_str", fam_out, fam_len)) {
        _sf_img_free(&img);
        return 0;
    }
    if (_sf_has_needed_lib(&img, "libirc") ||
        _sf_has_needed_lib(&img, "libimf")) {
        snprintf(fam_out, fam_len, "intel");
        _sf_img_free(&img);
        return 0;
    }
    snprintf(fam_out, fam_len, "gcc");
    _sf_img_free(&img);
    return 0;
}

static unsigned
_sf_hash_name(const char *s)
{
    unsigned h = 5381;

    while (*s != '\0') {
        h = ((h << 5) + h) + (unsigned char)*s;
        s++;
    }
    return h;
}

static int
_sf_prov_add(tpb_elf_prov_t *prov, const char *name)
{
    unsigned i;
    unsigned h;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (prov->n >= TPB_ELF_PROV_CAP - 1) {
        return -1;
    }
    h = _sf_hash_name(name);
    for (i = 0; i < TPB_ELF_PROV_CAP; i++) {
        unsigned k = (h + i) & (TPB_ELF_PROV_CAP - 1);

        if (prov->slot[k] == NULL) {
            prov->slot[k] = name;
            prov->n++;
            return 0;
        }
        if (strcmp(prov->slot[k], name) == 0) {
            return 0;
        }
    }
    return -1;
}

static int
_sf_prov_has(const tpb_elf_prov_t *prov, const char *name)
{
    unsigned i;
    unsigned h;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    h = _sf_hash_name(name);
    for (i = 0; i < TPB_ELF_PROV_CAP; i++) {
        unsigned k = (h + i) & (TPB_ELF_PROV_CAP - 1);

        if (prov->slot[k] == NULL) {
            return 0;
        }
        if (strcmp(prov->slot[k], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int
_sf_collect_providers(const tpb_elf_img_t *img, tpb_elf_prov_t *prov)
{
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh;
    uint16_t si;

    eh = (Elf64_Ehdr *)img->buf;
    sh = (Elf64_Shdr *)(img->buf + eh->e_shoff);
    for (si = 0; si < eh->e_shnum; si++) {
        Elf64_Sym *syms;
        const char *str;
        size_t n;
        size_t i;

        if (sh[si].sh_type != SHT_DYNSYM || sh[si].sh_entsize == 0) {
            continue;
        }
        str = _sf_find_strtab(img, &sh[si]);
        if (str == NULL) {
            continue;
        }
        n = sh[si].sh_size / sh[si].sh_entsize;
        syms = (Elf64_Sym *)(img->buf + sh[si].sh_offset);
        for (i = 1; i < n; i++) {
            unsigned char bind;
            unsigned char vis;

            if (syms[i].st_shndx == SHN_UNDEF) {
                continue;
            }
            bind = ELF64_ST_BIND(syms[i].st_info);
            vis = ELF64_ST_VISIBILITY(syms[i].st_other);
            if (bind != STB_GLOBAL && bind != STB_WEAK) {
                continue;
            }
            if (vis != STV_DEFAULT) {
                continue;
            }
            if (_sf_prov_add(prov, str + syms[i].st_name) != 0) {
                fprintf(stderr,
                        "tpb-elf-export: provider table full\n");
                return -1;
            }
        }
    }
    return 0;
}

static void
_sf_dso_origin(const char *path, char *out, size_t outlen)
{
    const char *slash;

    snprintf(out, outlen, "%s", path);
    slash = strrchr(out, '/');
    if (slash != NULL) {
        out[slash - out] = '\0';
    } else {
        snprintf(out, outlen, ".");
    }
}

static void
_sf_dso_basename(const char *path, char *out, size_t outlen)
{
    const char *slash = strrchr(path, '/');

    snprintf(out, outlen, "%s", (slash != NULL) ? slash + 1 : path);
}

static int
_sf_realpath_cpy(const char *in, char *out, size_t outlen)
{
    char real[TPB_ELF_PATH_MAX];

    if (realpath(in, real) != NULL) {
        snprintf(out, outlen, "%s", real);
        return 0;
    }
    snprintf(out, outlen, "%s", in);
    return 0;
}

static int
_sf_group_has(const tpb_elf_mapped_t *group, int ngrp, const char *path)
{
    int i;

    for (i = 0; i < ngrp; i++) {
        if (strcmp(group[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void
_sf_group_free(tpb_elf_mapped_t *group, int ngrp)
{
    int i;

    for (i = 0; i < ngrp; i++) {
        _sf_img_free(&group[i].img);
    }
}

static int
_sf_group_add(tpb_elf_mapped_t *group, int *ngrp, const char *path,
              const char *label)
{
    tpb_elf_mapped_t *m;
    char canon[TPB_ELF_PATH_MAX];

    _sf_realpath_cpy(path, canon, sizeof(canon));
    if (_sf_group_has(group, *ngrp, canon)) {
        return 0;
    }
    if (*ngrp >= TPB_ELF_GROUP_MAX) {
        fprintf(stderr,
                "tpb-elf-export: load-group cap %d exceeded at %s\n",
                TPB_ELF_GROUP_MAX, path);
        return -1;
    }
    m = &group[*ngrp];
    memset(m, 0, sizeof(*m));
    snprintf(m->path, sizeof(m->path), "%s", canon);
    if (label != NULL && label[0] != '\0') {
        snprintf(m->label, sizeof(m->label), "%s", label);
    } else {
        _sf_dso_basename(canon, m->label, sizeof(m->label));
    }
    m->skip_audit = _sf_is_skip_dso(m->label) || _sf_is_skip_dso(canon);
    if (_sf_img_load(canon, &m->img) != 0) {
        return -1;
    }
    (*ngrp)++;
    return 0;
}

static int
_sf_walk_needed(tpb_elf_mapped_t *group, int *ngrp, int idx)
{
    char needed[TPB_ELF_NEEDED_MAX][TPB_ELF_NAME_MAX];
    char runpath[TPB_ELF_PATH_MAX];
    char origin[TPB_ELF_PATH_MAX];
    char dep_path[TPB_ELF_PATH_MAX];
    int n;
    int i;

    if (group[idx].skip_audit) {
        return 0;
    }
    _sf_dso_origin(group[idx].path, origin, sizeof(origin));
    _sf_read_runpath(&group[idx].img, runpath, sizeof(runpath));
    _sf_collect_needed(&group[idx].img, needed, &n);
    for (i = 0; i < n; i++) {
        if (_sf_find_dso(needed[i], origin, runpath, dep_path,
                         sizeof(dep_path)) != 0) {
            if (!_sf_is_skip_dso(needed[i])) {
                fprintf(stderr,
                        "tpb-elf-export: cannot locate DT_NEEDED '%s' "
                        "from %s\n",
                        needed[i], group[idx].label);
            }
            continue;
        }
        if (_sf_group_add(group, ngrp, dep_path, needed[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int
_sf_print_undef_error(const char *sym, const char *label)
{
    fprintf(stderr,
            "Error: undefined symbol '%s' in %s\n"
            "  not provided as a default-visible definition in this kernel\n"
            "  or its loaded dependency group.\n"
            "  Recompile that dependency so it contains the function body,\n"
            "  or rebuild the kernel against a library that provides "
            "the symbol.\n",
            sym, label);
    return 1;
}

static int
_sf_audit_mapped(tpb_elf_mapped_t *group, int ngrp,
                 const tpb_elf_img_t *kern, const tpb_elf_prov_t *prov)
{
    int nfail = 0;
    int gi;

    for (gi = 0; gi < ngrp; gi++) {
        Elf64_Ehdr *eh;
        Elf64_Shdr *sh;
        uint16_t si;

        if (group[gi].skip_audit) {
            continue;
        }
        eh = (Elf64_Ehdr *)group[gi].img.buf;
        sh = (Elf64_Shdr *)(group[gi].img.buf + eh->e_shoff);
        for (si = 0; si < eh->e_shnum; si++) {
            Elf64_Sym *syms;
            const char *str;
            size_t ns;
            size_t k;

            if (sh[si].sh_type != SHT_DYNSYM || sh[si].sh_entsize == 0) {
                continue;
            }
            str = _sf_find_strtab(&group[gi].img, &sh[si]);
            if (str == NULL) {
                continue;
            }
            ns = sh[si].sh_size / sh[si].sh_entsize;
            syms = (Elf64_Sym *)(group[gi].img.buf + sh[si].sh_offset);
            for (k = 1; k < ns; k++) {
                const char *nm;
                unsigned char bind;

                if (syms[k].st_shndx != SHN_UNDEF) {
                    continue;
                }
                nm = str + syms[k].st_name;
                if (nm[0] == '\0') {
                    continue;
                }
                bind = ELF64_ST_BIND(syms[k].st_info);
                if (bind == STB_WEAK) {
                    fprintf(stderr,
                            "Warning: weak undefined symbol '%s' in %s\n",
                            nm, group[gi].label);
                    continue;
                }
                if (_sf_prov_has(prov, nm)) {
                    continue;
                }
                if (_sf_defined_any(kern, nm)) {
                    fprintf(stderr,
                            "tpb-elf-export: kernel defines '%s' but it "
                            "is not default-visible (needed by %s)\n",
                            nm, group[gi].label);
                    nfail++;
                    continue;
                }
                (void)_sf_print_undef_error(nm, group[gi].label);
                nfail++;
            }
        }
    }
    return nfail;
}

static int
_sf_audit_needed(const char *so_path)
{
    tpb_elf_mapped_t group[TPB_ELF_GROUP_MAX];
    tpb_elf_prov_t *prov;
    int ngrp = 0;
    int i;
    int nfail;
    int rc = 0;

    memset(group, 0, sizeof(group));
    prov = (tpb_elf_prov_t *)calloc(1, sizeof(*prov));
    if (prov == NULL) {
        fprintf(stderr, "tpb-elf-export: out of memory\n");
        return -1;
    }
    if (_sf_group_add(group, &ngrp, so_path, NULL) != 0) {
        free(prov);
        _sf_group_free(group, ngrp);
        return -1;
    }
    for (i = 0; i < ngrp; i++) {
        if (_sf_walk_needed(group, &ngrp, i) != 0) {
            free(prov);
            _sf_group_free(group, ngrp);
            return -1;
        }
    }
    for (i = 0; i < ngrp; i++) {
        if (_sf_collect_providers(&group[i].img, prov) != 0) {
            free(prov);
            _sf_group_free(group, ngrp);
            return -1;
        }
    }
    nfail = _sf_audit_mapped(group, ngrp, &group[0].img, prov);
    if (nfail < 0) {
        rc = -1;
    } else if (nfail > 0) {
        rc = 1;
    }
    free(prov);
    _sf_group_free(group, ngrp);
    return rc;
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        _sf_usage();
        return 2;
    }
    if (strcmp(argv[1], "classify-dso") == 0 && argc == 3) {
        char fam[32];

        if (_sf_classify_dso(argv[2], fam, sizeof(fam)) != 0) {
            return 1;
        }
        printf("%s\n", fam);
        return 0;
    }
    if (strcmp(argv[1], "audit-needed") == 0 && argc == 3) {
        int rc = _sf_audit_needed(argv[2]);

        return (rc == 0) ? 0 : 1;
    }
    _sf_usage();
    return 2;
}
