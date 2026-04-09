/*
  Linux sys/mount.h compatibility shim for MSVC/Windows.
*/

#pragma once

#ifdef _WIN32
/* No mount syscall on Windows — stub */
#else
#include_next <sys/mount.h>
#endif
