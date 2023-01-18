#include "dc_common_log.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

static void dc_common_vlog_stderr(int, const char *, va_list);
static dc_common_vlog_fn_t dc_common_vlog_func = dc_common_vlog_stderr;
static volatile int dc_common_log_level = 0;

static void
dc_common_vlog_stderr(int facility, const char *format, va_list args)
{
    char str[1024];

    vsnprintf(str, sizeof(str), format, args);

    /* generate timestamp and print the timestamp into str[] array*/
    struct timeval tv;
    struct tm *tm;
    char time_str[64];
    uint64_t thd_id;

    /* Get timestamp */
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    // get thread id
    thd_id = (uint64_t)pthread_self();

    switch (facility)
    {
    case DC_COMMON_LOG_ERROR:
        fprintf(stderr, "%s ERROR: %lu %s\n", time_str, thd_id, str);
        break;
    case DC_COMMON_LOG_WARN:
        fprintf(stderr, "%s WARN: %lu %s\n", time_str, thd_id, str);
        break;
    case DC_COMMON_LOG_INFO:
        fprintf(stderr, "%s INFO: %lu %s\n", time_str, thd_id, str);
    default:
        break;
    }
}

static void
dc_common_vlog1(int facility, const char *format, va_list args)
{
    dc_common_vlog_func(facility, format, args);
}

void dc_common_vlog(int facility, int level, const char *format, va_list args)
{
    if (level <= dc_common_log_level)
        dc_common_vlog1(facility, format, args);
}

void dc_common_log(int facility, int level, const char *format, ...)
{
    va_list args;

    if (level <= dc_common_log_level)
    {
        va_start(args, format);
        dc_common_vlog1(facility, format, args);
        va_end(args);
    }
}

void dc_common_vlog_register(dc_common_vlog_fn_t vlog_func)
{
    if (vlog_func != NULL)
        dc_common_vlog_func = vlog_func;
}

void dc_common_log_setlevel(int level)
{
    dc_common_log_level = level;
}
