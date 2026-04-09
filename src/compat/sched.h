/*
  POSIX sched.h compatibility shim for MSVC/Windows.
*/

#pragma once

#ifdef _WIN32

#include <windows.h>
#include "win32_undef.h"

typedef struct
{
  unsigned long _mask;
} cpu_set_t;

#define CPU_ZERO(set)      ((set)->_mask = 0)
#define CPU_SET(cpu, set)  ((set)->_mask |= (1UL << (cpu)))
#define CPU_ISSET(cpu, set) (((set)->_mask & (1UL << (cpu))) != 0)

static inline int sched_setaffinity(int pid, size_t size, const cpu_set_t *set)
{
  (void)pid;
  (void)size;
  (void)set;
  return 0;
}

static inline int sched_getaffinity(int pid, size_t size, cpu_set_t *set)
{
  (void)pid;
  (void)size;
  CPU_ZERO(set);
  return 0;
}

#else
#include_next <sched.h>
#endif
