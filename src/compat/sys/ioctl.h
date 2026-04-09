/*
  Linux sys/ioctl.h compatibility shim for MSVC/Windows.
*/

#pragma once

#ifdef _WIN32
/* ioctl is not available on Windows in the same form.
   mergerfs uses it for FICLONE etc. — stubbed out. */
#ifndef FICLONE
#define FICLONE 0
#endif
#else
#include_next <sys/ioctl.h>
#endif
