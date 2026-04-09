/*
  Force off_t to be 64-bit on MSVC.
  This header is force-included via -FI before all source files.
  It defines _OFF_T_DEFINED before <sys/types.h> can define off_t as long.
*/

#ifdef _WIN32
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
#include <stdint.h>
typedef int64_t _off_t;
typedef int64_t off_t;
#endif
#endif
