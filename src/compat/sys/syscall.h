/*
  Linux sys/syscall.h compatibility shim for MSVC/Windows.
  Stubs for direct syscall access (not applicable on Windows).
*/

#pragma once

#ifdef _WIN32

/* SYS_* constants — not used on Windows, define as -1 */
#define SYS_getdents64 -1
#define SYS_copy_file_range -1
#define SYS_renameat2 -1
#define SYS_statx -1

#include <errno.h>

static inline long syscall(long number, ...)
{
  (void)number;
  errno = ENOSYS;
  return -1;
}

#else
#include_next <sys/syscall.h>
#endif
