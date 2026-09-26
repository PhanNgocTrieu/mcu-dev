#include "log.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t g_log_mu = PTHREAD_MUTEX_INITIALIZER;
static usb_log_level_t g_level = USB_LOG_INFO;

static const char* level_name(usb_log_level_t level) {
  switch (level) {
    case USB_LOG_DEBUG: return "DEBUG";
    case USB_LOG_WARN: return "WARN";
    case USB_LOG_ERROR: return "ERROR";
    case USB_LOG_INFO: break;
  }
  return "INFO";
}

void usb_log_set_level(usb_log_level_t level) { g_level = level; }

void usb_log_write(usb_log_level_t level, const char* file, int line, const char* msg) {
  if (level < g_level) return;
  const char* base = file ? file : "?";
  for (const char* p = base; *p; ++p) {
    if (*p == '/') base = p + 1;
  }
  pthread_mutex_lock(&g_log_mu);
  fprintf(stderr, "[%s] %s:%d %s\n", level_name(level), base, line, msg ? msg : "");
  pthread_mutex_unlock(&g_log_mu);
}
