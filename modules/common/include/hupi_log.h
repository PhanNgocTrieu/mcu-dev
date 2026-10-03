/**
 * @file hupi_log.h
 * @brief Process-wide tracing for HUPI USB modules.
 *
 * Every hotplug, session transition, AOA step, and media event should call
 * one of these macros so journald / systemd and the host sim show the same
 * trail. Levels map to syslog priorities when stderr is a tty they also print
 * with a short prefix.
 */
#ifndef HUPI_LOG_H
#define HUPI_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HUPI_LOG_ERROR = 0,
    HUPI_LOG_WARN,
    HUPI_LOG_INFO,
    HUPI_LOG_TRACE
} hupi_log_level_t;

/**
 * @brief Open the logger for this process.
 * @param tag Short process name (e.g. "usb-driver", "usb-man").
 * @param level Minimum level that will be emitted.
 */
void hupi_log_open(const char *tag, hupi_log_level_t level);

/** @brief Change the runtime threshold (default INFO). */
void hupi_log_set_level(hupi_log_level_t level);

/** @brief Parse "error|warn|info|trace" or a digit. Returns 0 on success. */
int hupi_log_level_from_str(const char *text, hupi_log_level_t *out);

void hupi_log_v(hupi_log_level_t level, const char *fmt, va_list ap);
void hupi_log(hupi_log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define HUPI_LOGE(...) hupi_log(HUPI_LOG_ERROR, __VA_ARGS__)
#define HUPI_LOGW(...) hupi_log(HUPI_LOG_WARN, __VA_ARGS__)
#define HUPI_LOGI(...) hupi_log(HUPI_LOG_INFO, __VA_ARGS__)
#define HUPI_LOGT(...) hupi_log(HUPI_LOG_TRACE, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
