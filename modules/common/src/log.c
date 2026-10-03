/**
 * @file log.c
 * @brief Logger có timestamp + tag ra stderr và syslog.
 */
#include "hupi_log.h"

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

static char g_tag[32] = "hupi";
static hupi_log_level_t g_level = HUPI_LOG_INFO;
static int g_syslog;

static int syslog_prio(hupi_log_level_t level)
{
    switch (level) {
    case HUPI_LOG_ERROR:
        return LOG_ERR;
    case HUPI_LOG_WARN:
        return LOG_WARNING;
    case HUPI_LOG_INFO:
        return LOG_INFO;
    default:
        return LOG_DEBUG;
    }
}

static const char *level_name(hupi_log_level_t level)
{
    switch (level) {
    case HUPI_LOG_ERROR:
        return "E";
    case HUPI_LOG_WARN:
        return "W";
    case HUPI_LOG_INFO:
        return "I";
    default:
        return "T";
    }
}

void hupi_log_open(const char *tag, hupi_log_level_t level)
{
    if (tag && tag[0]) {
        snprintf(g_tag, sizeof g_tag, "%s", tag);
    }
    g_level = level;
    openlog(g_tag, LOG_PID | LOG_CONS, LOG_DAEMON);
    g_syslog = 1;
    HUPI_LOGI("logger open level=%s", level_name(level));
}

void hupi_log_set_level(hupi_log_level_t level)
{
    g_level = level;
}

int hupi_log_level_from_str(const char *text, hupi_log_level_t *out)
{
    if (!text || !out) {
        return -1;
    }
    if (!strcmp(text, "error") || !strcmp(text, "0")) {
        *out = HUPI_LOG_ERROR;
        return 0;
    }
    if (!strcmp(text, "warn") || !strcmp(text, "1")) {
        *out = HUPI_LOG_WARN;
        return 0;
    }
    if (!strcmp(text, "info") || !strcmp(text, "2")) {
        *out = HUPI_LOG_INFO;
        return 0;
    }
    if (!strcmp(text, "trace") || !strcmp(text, "3") || !strcmp(text, "debug")) {
        *out = HUPI_LOG_TRACE;
        return 0;
    }
    return -1;
}

void hupi_log_v(hupi_log_level_t level, const char *fmt, va_list ap)
{
    char msg[1024];
    struct timespec ts;
    struct tm tm;
    char tbuf[32];
    va_list copy;

    if (level > g_level) {
        return;
    }
    va_copy(copy, ap);
    vsnprintf(msg, sizeof msg, fmt, ap);
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm);
    fprintf(stderr, "%s.%03ld %s %s: %s\n", tbuf, ts.tv_nsec / 1000000L, level_name(level), g_tag, msg);
    if (g_syslog) {
        vsyslog(syslog_prio(level), fmt, copy);
    }
    va_end(copy);
}

void hupi_log(hupi_log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    hupi_log_v(level, fmt, ap);
    va_end(ap);
}
