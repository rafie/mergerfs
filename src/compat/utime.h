/*
  POSIX utime.h compatibility shim for MSVC/Windows.
  MSVC has <sys/utime.h> instead of <utime.h>.
*/

#pragma once

#ifdef _WIN32
#include <sys/utime.h>
#else
#include_next <utime.h>
#endif
