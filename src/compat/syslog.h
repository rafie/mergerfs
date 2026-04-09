/*
  POSIX syslog.h compatibility shim for MSVC/Windows.
  Maps syslog calls to OutputDebugString / stderr.
*/

#pragma once

#ifdef _WIN32

#include <stdio.h>
#include <stdarg.h>
#include <windows.h>
#include "win32_undef.h"

/* Priority levels */
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

/* Facility */
#define LOG_USER    (1 << 3)
#define LOG_LOCAL0  (16 << 3)

/* openlog flags */
#define LOG_PID     0x01
#define LOG_CONS    0x02
#define LOG_NDELAY  0x08

static inline void openlog(const char *ident, int option, int facility)
{
  (void)ident;
  (void)option;
  (void)facility;
}

static inline void closelog(void)
{
}

static inline void syslog(int priority, const char *format, ...)
{
  char buf[1024];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  /* Output to debugger and stderr */
  OutputDebugStringA(buf);
  fprintf(stderr, "[%d] %s\n", priority, buf);
}

#else
#include_next <syslog.h>
#endif
