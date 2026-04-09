/*
  POSIX fnmatch.h compatibility shim for MSVC/Windows.
  Simple glob-style pattern matching.
*/

#pragma once

#ifdef _WIN32

#define FNM_NOMATCH 1
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD   0x04

static inline int fnmatch(const char *pattern, const char *string, int flags)
{
  (void)flags;
  const char *p = pattern;
  const char *s = string;

  while(*p && *s)
  {
    switch(*p)
    {
    case '?':
      /* Match any single character */
      p++;
      s++;
      break;
    case '*':
      /* Match zero or more characters */
      p++;
      if(!*p)
        return 0; /* trailing * matches everything */
      while(*s)
      {
        if(fnmatch(p, s, flags) == 0)
          return 0;
        s++;
      }
      return FNM_NOMATCH;
    case '[':
    {
      /* Character class */
      int inv = 0;
      int matched = 0;
      p++;
      if(*p == '!' || *p == '^')
      {
        inv = 1;
        p++;
      }
      while(*p && *p != ']')
      {
        if(p[1] == '-' && p[2] && p[2] != ']')
        {
          if(*s >= p[0] && *s <= p[2])
            matched = 1;
          p += 3;
        }
        else
        {
          if(*s == *p)
            matched = 1;
          p++;
        }
      }
      if(*p == ']')
        p++;
      if(matched == inv)
        return FNM_NOMATCH;
      s++;
      break;
    }
    default:
      if(*p != *s)
        return FNM_NOMATCH;
      p++;
      s++;
      break;
    }
  }

  /* Skip trailing *'s in pattern */
  while(*p == '*')
    p++;

  return (*p == '\0' && *s == '\0') ? 0 : FNM_NOMATCH;
}

#else
#include_next <fnmatch.h>
#endif
