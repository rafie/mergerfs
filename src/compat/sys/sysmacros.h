/*
  Linux sys/sysmacros.h compatibility shim for MSVC/Windows.
  Provides major/minor/makedev macros.
*/

#pragma once

#ifdef _WIN32

#define major(dev) ((unsigned int)(((dev) >> 8) & 0xff))
#define minor(dev) ((unsigned int)((dev) & 0xff))
#define makedev(maj, min) ((dev_t)(((maj) << 8) | (min)))

#else
#include_next <sys/sysmacros.h>
#endif
