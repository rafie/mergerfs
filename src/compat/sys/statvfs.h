/*
  POSIX sys/statvfs.h compatibility shim for MSVC/Windows.
  Provides struct statvfs and statvfs()/fstatvfs() for mergerfs.
*/

#pragma once

#ifdef _WIN32

#include <stdint.h>
#include <windows.h>
#include "win32_undef.h"

/* statvfs flags */
#ifndef ST_RDONLY
#define ST_RDONLY 1
#endif
#ifndef ST_NOSUID
#define ST_NOSUID 2
#endif

struct statvfs
{
  uint64_t f_bsize;    /* Filesystem block size */
  uint64_t f_frsize;   /* Fragment size */
  uint64_t f_blocks;   /* Total blocks (in f_frsize units) */
  uint64_t f_bfree;    /* Free blocks */
  uint64_t f_bavail;   /* Free blocks for unprivileged users */
  uint64_t f_files;    /* Total inodes */
  uint64_t f_ffree;    /* Free inodes */
  uint64_t f_favail;   /* Free inodes for unprivileged users */
  uint64_t f_fsid;     /* Filesystem ID */
  uint64_t f_flag;     /* Mount flags */
  uint64_t f_namemax;  /* Maximum filename length */
};

static inline int statvfs(const char *path, struct statvfs *buf)
{
  ULARGE_INTEGER free_bytes_available;
  ULARGE_INTEGER total_bytes;
  ULARGE_INTEGER total_free_bytes;

  if(!GetDiskFreeSpaceExA(path,
                          &free_bytes_available,
                          &total_bytes,
                          &total_free_bytes))
    return -1;

  buf->f_bsize   = 4096;
  buf->f_frsize  = 4096;
  buf->f_blocks  = total_bytes.QuadPart / 4096;
  buf->f_bfree   = total_free_bytes.QuadPart / 4096;
  buf->f_bavail  = free_bytes_available.QuadPart / 4096;
  buf->f_files   = 0;
  buf->f_ffree   = 0;
  buf->f_favail  = 0;
  buf->f_fsid    = 0;
  buf->f_flag    = 0;
  buf->f_namemax = 255;

  return 0;
}

static inline int fstatvfs(int fd, struct statvfs *buf)
{
  (void)fd;
  /* Fallback: return generic values */
  buf->f_bsize   = 4096;
  buf->f_frsize  = 4096;
  buf->f_blocks  = 0;
  buf->f_bfree   = 0;
  buf->f_bavail  = 0;
  buf->f_files   = 0;
  buf->f_ffree   = 0;
  buf->f_favail  = 0;
  buf->f_fsid    = 0;
  buf->f_flag    = 0;
  buf->f_namemax = 255;
  return 0;
}

#else
#include_next <sys/statvfs.h>
#endif
