/**
 * @file mpi_wtime.c
 * @brief: Implementation of MPI_Wtime timer (seconds converted to nanoseconds).
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Local Function Prototypes */
static void *_sf_find_mpi_wtime(void);
static int64_t _sf_now_ns(void);
static int _sf_phdr_mpi_wtime(struct dl_phdr_info *info, size_t size,
                              void *data);

typedef double (*tpb_mpi_wtime_fn)(void);

static tpb_mpi_wtime_fn s_mpi_wtime;

static int
_sf_phdr_mpi_wtime(struct dl_phdr_info *info, size_t size, void *data)
{
    void **out;
    const char *name;
    void *handle;
    void *sym;

    (void)size;
    out = data;
    if (out == NULL || *out != NULL) {
        return 1;
    }
    name = info->dlpi_name;
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (strstr(name, "libmpi.so") == NULL) {
        return 0;
    }
    handle = dlopen(name, RTLD_NOLOAD | RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        return 0;
    }
    sym = dlsym(handle, "MPI_Wtime");
    if (sym != NULL) {
        *out = sym;
        return 1;
    }
    return 0;
}

static void *
_sf_find_mpi_wtime(void)
{
    void *sym;

    sym = dlsym(RTLD_DEFAULT, "MPI_Wtime");
    if (sym != NULL) {
        return sym;
    }
    /*
     * Kernel .so files are dlopen'd RTLD_LOCAL, so libmpi is not in the
     * default namespace. Promote an already-loaded libmpi.so and retry.
     */
    dl_iterate_phdr(_sf_phdr_mpi_wtime, &sym);
    return sym;
}

static int64_t
_sf_now_ns(void)
{
    if (s_mpi_wtime == NULL) {
        return 0;
    }
    return (int64_t)(s_mpi_wtime() * 1e9);
}

int
init_timer_mpi_wtime(void)
{
    void *sym;

    sym = _sf_find_mpi_wtime();
    if (sym == NULL) {
        s_mpi_wtime = NULL;
        return -1;
    }
    s_mpi_wtime = (tpb_mpi_wtime_fn)sym;
    return 0;
}

void
tick_mpi_wtime(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns();
    }
}

void
tock_mpi_wtime(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns() - *ts;
    }
}

void
get_time_mpi_wtime(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns();
    }
}
