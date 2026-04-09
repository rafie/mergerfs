/*
  POSIX sys/uio.h compatibility shim for MSVC/Windows.
  Provides struct iovec.
*/

#pragma once

#ifdef _WIN32

#include <stddef.h>

struct iovec
{
  void  *iov_base;
  size_t iov_len;
};

#else
#include_next <sys/uio.h>
#endif
