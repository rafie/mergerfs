/*
  POSIX sys/time.h compatibility shim for MSVC/Windows.
*/

#pragma once

#ifdef _WIN32

#include <time.h>
#include <windows.h>
#include "win32_undef.h"
#include <stdint.h>

/* Windows SDK defines struct timeval in winsock.h (included via windows.h).
   Only define it ourselves if winsock was NOT included. */
#ifndef _WINSOCKAPI_
struct timeval
{
  long tv_sec;
  long tv_usec;
};
#endif

static inline int gettimeofday(struct timeval *tv, void *tz)
{
  (void)tz;
  if(tv)
  {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    /* FILETIME is 100-nanosecond intervals since 1601-01-01 */
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* Convert to Unix epoch (subtract 11644473600 seconds) */
    t -= 116444736000000000ULL;
    tv->tv_sec  = (long)(t / 10000000ULL);
    tv->tv_usec = (long)((t % 10000000ULL) / 10);
  }
  return 0;
}

/* lutimes — no-op stub on Windows (no symlink timestamp support) */
static inline int lutimes(const char *path, const struct timeval tv[2])
{
  (void)path; (void)tv;
  return 0;
}

/* futimes — stub on Windows */
static inline int futimes(int fd, const struct timeval tv[2])
{
  (void)fd; (void)tv;
  return 0;
}

#else
#include_next <sys/time.h>
#endif
