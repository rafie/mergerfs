/*
  POSIX dirent.h compatibility shim for MSVC/Windows.
  Provides DIR, struct dirent, opendir/readdir/closedir.
*/

#pragma once

#ifdef _WIN32

#include <windows.h>
#include "win32_undef.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8
#define DT_LNK    10

#ifndef NAME_MAX
#define NAME_MAX 260
#endif

struct dirent
{
  unsigned long  d_ino;
  long long      d_off;
  unsigned short d_reclen;
  unsigned char  d_type;
  char           d_name[NAME_MAX + 1];
};

typedef struct
{
  HANDLE          _handle;
  WIN32_FIND_DATAA _data;
  int             _first;
  struct dirent   _entry;
  char            _pattern[MAX_PATH + 3];
} DIR;

static inline DIR *opendir(const char *name)
{
  DIR *d = (DIR *)malloc(sizeof(DIR));
  if(!d) return NULL;

  size_t len = strlen(name);
  if(len >= MAX_PATH)
  {
    free(d);
    errno = ENAMETOOLONG;
    return NULL;
  }

  /* Build search pattern: path\* */
  memcpy(d->_pattern, name, len);
  if(len > 0 && name[len - 1] != '/' && name[len - 1] != '\\')
    d->_pattern[len++] = '\\';
  d->_pattern[len++] = '*';
  d->_pattern[len] = '\0';

  d->_handle = FindFirstFileA(d->_pattern, &d->_data);
  if(d->_handle == INVALID_HANDLE_VALUE)
  {
    free(d);
    errno = ENOENT;
    return NULL;
  }

  d->_first = 1;
  return d;
}

static inline struct dirent *readdir(DIR *d)
{
  if(!d)
    return NULL;

  if(d->_first)
  {
    d->_first = 0;
  }
  else
  {
    if(!FindNextFileA(d->_handle, &d->_data))
      return NULL;
  }

  strncpy(d->_entry.d_name, d->_data.cFileName, NAME_MAX);
  d->_entry.d_name[NAME_MAX] = '\0';
  d->_entry.d_ino = 0;

  if(d->_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    d->_entry.d_type = DT_LNK;
  else if(d->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    d->_entry.d_type = DT_DIR;
  else
    d->_entry.d_type = DT_REG;

  return &d->_entry;
}

static inline int closedir(DIR *d)
{
  if(d)
  {
    if(d->_handle != INVALID_HANDLE_VALUE)
      FindClose(d->_handle);
    free(d);
  }
  return 0;
}

#else
#include_next <dirent.h>
#endif
