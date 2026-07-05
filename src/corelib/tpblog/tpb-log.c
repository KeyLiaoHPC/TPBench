/*
 * tpb-log.c
 * Run-log session lifecycle for the tpblog module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "tpb-log.h"
#include "tpb-public.h"
#include "tpb_corelib_state.h"
#include "rafdb/tpb-raf-types.h"

/* Module-level log state */
static FILE *log_file = NULL;
static char log_filepath[PATH_MAX] = {0};

FILE *
_tpblog_file(void)
{
    return log_file;
}

void
_tpblog_write(const char *msg)
{
    if (log_file != NULL && msg != NULL) {
        fputs(msg, log_file);
        fflush(log_file);
    }
}

int
tpblog_init(void)
{
    char hostname[256] = {0};
    char logdir[PATH_MAX];
    char fail_msg[PATH_MAX + 64];
    char timestamp[32];
    time_t now;
    struct tm *tm_now;
    const char *ws;
    const char *env_path;

    if (log_file != NULL) {
        return TPBE_SUCCESS;
    }

    /* PLI child: append to the parent-provided run log without a new header. */
    env_path = getenv(TPBLOG_FILE_ENV);
    if (env_path != NULL && env_path[0] != '\0') {
        if (strlen(env_path) >= sizeof(log_filepath)) {
            snprintf(fail_msg, sizeof(fail_msg), "%s path too long",
                     TPBLOG_FILE_ENV);
            TPB_FAIL(TPB_MOD_IO, TPBE_FILE_IO_FAIL, fail_msg);
        }
        snprintf(log_filepath, sizeof(log_filepath), "%s", env_path);
        log_file = fopen(log_filepath, "a");
        if (log_file == NULL) {
            snprintf(fail_msg, sizeof(fail_msg),
                     "Could not open log file %s", log_filepath);
            TPB_FAIL(TPB_MOD_IO, TPBE_FILE_IO_FAIL, fail_msg);
        }
        fflush(log_file);
        return TPBE_SUCCESS;
    }

    ws = _tpb_workspace_path_get();
    if (ws == NULL || ws[0] == '\0') {
        TPB_FAIL(TPB_MOD_IO, TPBE_FILE_IO_FAIL,
                 "TPBench workspace not set for logging");
    }

    if (snprintf(logdir, sizeof(logdir), "%s/%s", ws, TPB_RAF_LOG_REL)
        >= (int)sizeof(logdir)) {
        TPB_FAIL(TPB_MOD_IO, TPBE_FILE_IO_FAIL, "Log directory path too long");
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "unknown");
    }

    now = time(NULL);
    tm_now = localtime(&now);
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02dT%02d%02d%02d",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

    snprintf(log_filepath, sizeof(log_filepath),
             "%s/tpbrunlog_%s_%s.log", logdir, timestamp, hostname);

    log_file = fopen(log_filepath, "w");
    if (log_file == NULL) {
        snprintf(fail_msg, sizeof(fail_msg),
                 "Could not open log file %s", log_filepath);
        TPB_FAIL(TPB_MOD_IO, TPBE_FILE_IO_FAIL, fail_msg);
    }

    fprintf(log_file, "TPBench Run Log\n");
    fprintf(log_file, "Session Started: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
            tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    fprintf(log_file, "Hostname: %s\n", hostname);
    fprintf(log_file, "TPB Version: %g\n", TPB_VERSION);
    fprintf(log_file, "TPBench workspace: %s\n", ws);
    fprintf(log_file,
            "Note: Raw database and this log file live under this workspace.\n");
    fprintf(log_file, "\n");
    fflush(log_file);

    return TPBE_SUCCESS;
}

void
tpblog_cleanup(void)
{
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

const char *
tpblog_get_filepath(void)
{
    if (log_filepath[0] != '\0') {
        return log_filepath;
    }
    return NULL;
}
