/*
  POSIX glob.h compatibility shim for MSVC/Windows.
  Implements glob() using FindFirstFile/FindNextFile.
*/

#pragma once

#ifdef _WIN32

#include <windows.h>
#include "win32_undef.h"
#include <stdlib.h>
#include <string.h>

#define GLOB_ERR      0x01
#define GLOB_MARK     0x02
#define GLOB_NOSORT   0x04
#define GLOB_NOCHECK  0x08
#define GLOB_NOESCAPE 0x10
#define GLOB_BRACE    0x20
#define GLOB_ONLYDIR  0x40
#define GLOB_NOMATCH  (-3)

typedef struct
{
  size_t gl_pathc;
  char **gl_pathv;
  size_t _capacity;
} glob_t;

static inline void globfree(glob_t *g)
{
  if(g && g->gl_pathv)
  {
    for(size_t i = 0; i < g->gl_pathc; i++)
      free(g->gl_pathv[i]);
    free(g->gl_pathv);
    g->gl_pathv = NULL;
    g->gl_pathc = 0;
  }
}

static inline int glob(const char *pattern, int flags,
                       int (*errfunc)(const char *, int),
                       glob_t *g)
{
  WIN32_FIND_DATAA fd;
  HANDLE h;
  (void)errfunc;

  g->gl_pathc = 0;
  g->gl_pathv = NULL;
  g->_capacity = 0;

  h = FindFirstFileA(pattern, &fd);
  if(h == INVALID_HANDLE_VALUE)
  {
    if(flags & GLOB_NOCHECK)
    {
      g->gl_pathv = (char **)malloc(sizeof(char *));
      if(!g->gl_pathv) return -1;
      g->gl_pathv[0] = _strdup(pattern);
      g->gl_pathc = 1;
      return 0;
    }
    return GLOB_NOMATCH;
  }

  /* Extract directory prefix from pattern */
  const char *last_sep = strrchr(pattern, '/');
  const char *last_bsep = strrchr(pattern, '\\');
  if(last_bsep && (!last_sep || last_bsep > last_sep))
    last_sep = last_bsep;

  char prefix[MAX_PATH] = {0};
  if(last_sep)
  {
    size_t plen = (size_t)(last_sep - pattern + 1);
    if(plen >= MAX_PATH) plen = MAX_PATH - 1;
    memcpy(prefix, pattern, plen);
  }

  do
  {
    if(strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;
    if((flags & GLOB_ONLYDIR) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      continue;

    if(g->gl_pathc >= g->_capacity)
    {
      g->_capacity = g->_capacity ? g->_capacity * 2 : 16;
      char **tmp = (char **)realloc(g->gl_pathv, g->_capacity * sizeof(char *));
      if(!tmp) { globfree(g); FindClose(h); return -1; }
      g->gl_pathv = tmp;
    }

    char fullpath[MAX_PATH];
    snprintf(fullpath, MAX_PATH, "%s%s", prefix, fd.cFileName);
    g->gl_pathv[g->gl_pathc] = _strdup(fullpath);
    if(!g->gl_pathv[g->gl_pathc]) { globfree(g); FindClose(h); return -1; }
    g->gl_pathc++;
  } while(FindNextFileA(h, &fd));

  FindClose(h);

  if(g->gl_pathc == 0)
    return GLOB_NOMATCH;

  return 0;
}

#else
#include_next <glob.h>
#endif
