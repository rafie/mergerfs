/*
  POSIX sys/file.h compatibility shim for MSVC/Windows.
  Provides flock() stub.
*/

#pragma once

#ifdef _WIN32

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

static inline int flock(int fd, int operation)
{
  (void)fd;
  (void)operation;
  /* TODO: implement via LockFileEx/UnlockFileEx */
  return 0;
}

#else
#include_next <sys/file.h>
#endif
