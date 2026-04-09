/*
  POSIX sys/resource.h compatibility shim for MSVC/Windows.
  Stubs for resource limit functions.
*/

#pragma once

#ifdef _WIN32

#include <stdint.h>

#define RLIMIT_NOFILE 7

typedef uint64_t rlim_t;

struct rlimit
{
  rlim_t rlim_cur;
  rlim_t rlim_max;
};

static inline int getrlimit(int resource, struct rlimit *rlp)
{
  (void)resource;
  rlp->rlim_cur = 2048;
  rlp->rlim_max = 2048;
  return 0;
}

static inline int setrlimit(int resource, const struct rlimit *rlp)
{
  (void)resource;
  (void)rlp;
  return 0;
}

#else
#include_next <sys/resource.h>
#endif
