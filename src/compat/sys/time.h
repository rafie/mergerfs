/*
  POSIX sys/time.h compatibility shim for MSVC/Windows.
*/

#pragma once

#ifdef _WIN32

#include <time.h>
#include <io.h>
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

/* Convert Unix timeval to Windows FILETIME */
static inline FILETIME _timeval_to_filetime(const struct timeval *tv)
{
  /* Unix epoch to Windows FILETIME epoch: add 11644473600 seconds */
  uint64_t t = (uint64_t)tv->tv_sec + 11644473600ULL;
  t = t * 10000000ULL + (uint64_t)tv->tv_usec * 10ULL;
  FILETIME ft;
  ft.dwLowDateTime  = (DWORD)(t & 0xFFFFFFFF);
  ft.dwHighDateTime = (DWORD)(t >> 32);
  return ft;
}

/* lutimes — set timestamps without following symlinks.
   On Windows we just set timestamps normally (no symlink distinction). */
static inline int lutimes(const char *path, const struct timeval tv[2])
{
  if(!tv || !path)
    return 0;
  HANDLE h = CreateFileA(path,
                         FILE_WRITE_ATTRIBUTES,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | 4,
                         NULL, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if(h == INVALID_HANDLE_VALUE)
    {
      errno = EACCES;
      return -1;
    }
  FILETIME atime = _timeval_to_filetime(&tv[0]);
  FILETIME mtime = _timeval_to_filetime(&tv[1]);
  BOOL ok = SetFileTime(h, NULL, &atime, &mtime);
  CloseHandle(h);
  if(!ok)
    {
      errno = EACCES;
      return -1;
    }
  return 0;
}

/* futimes — set file timestamps via SetFileTime */
static inline int futimes(int fd, const struct timeval tv[2])
{
  if(!tv)
    return 0;
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  if(h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }
  FILETIME atime = _timeval_to_filetime(&tv[0]);
  FILETIME mtime = _timeval_to_filetime(&tv[1]);
  if(!SetFileTime(h, NULL, &atime, &mtime))
    {
      errno = EACCES;
      return -1;
    }
  return 0;
}

#else
#include_next <sys/time.h>
#endif
