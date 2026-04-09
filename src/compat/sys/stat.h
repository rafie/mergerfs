/*
  POSIX sys/stat.h compatibility shim for MSVC/Windows.

  Provides a POSIX-compatible struct stat with struct timespec members
  (st_atim, st_mtim, st_ctim), st_blocks, and st_blksize that MSVC's
  struct _stat64 lacks.

  This header SHADOWS MSVC's real <sys/stat.h>.  It declares struct
  _stat64 and the CRT functions itself so that it never needs to
  include the real header (which would recurse through our shadow).
*/

#pragma once

#ifdef _WIN32

#include <sys/types.h>   /* via compat/sys/types.h: overrides off_t to 64-bit */
#include <stdint.h>
#include <time.h>        /* struct timespec (MSVC 2015+ UCRT) */
#include <string.h>      /* memset */
#include <errno.h>

/* ------------------------------------------------------------------ */
/* POSIX types (guarded — compat/unistd.h uses the same guards)       */
/* ------------------------------------------------------------------ */

#ifndef _UID_GID_T_DEFINED_COMPAT
#define _UID_GID_T_DEFINED_COMPAT
typedef uint32_t uid_t;
typedef uint32_t gid_t;
#endif

#ifndef _MODE_T_DEFINED
typedef uint32_t mode_t;
#define _MODE_T_DEFINED
#endif

#ifndef _NLINK_T_DEFINED_COMPAT
#define _NLINK_T_DEFINED_COMPAT
typedef uint16_t nlink_t;
#endif

#ifndef _BLKSIZE_T_DEFINED_COMPAT
#define _BLKSIZE_T_DEFINED_COMPAT
typedef int32_t  blksize_t;
typedef int64_t  blkcnt_t;
#endif

/* ------------------------------------------------------------------ */
/* MSVC CRT: struct _stat64 and functions                             */
/* We declare these ourselves so we never #include MSVC's sys/stat.h. */
/* The layout matches the UCRT ABI exactly.                           */
/* ------------------------------------------------------------------ */

struct _stat64
{
  _dev_t     st_dev;
  _ino_t     st_ino;      /* unsigned short */
  unsigned short st_mode;
  short      st_nlink;
  short      st_uid;
  short      st_gid;
  _dev_t     st_rdev;
  __int64    st_size;
  __time64_t st_atime;
  __time64_t st_mtime;
  __time64_t st_ctime;
};

#ifdef __cplusplus
extern "C" {
#endif
int __cdecl _stat64(const char *, struct _stat64 *);
int __cdecl _fstat64(int, struct _stat64 *);
int __cdecl _wstat64(const wchar_t *, struct _stat64 *);
int __cdecl _chmod(const char *, int);
#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */
/* MSVC _S_ macros                                                    */
/* ------------------------------------------------------------------ */

#ifndef _S_IFMT
#define _S_IFMT   0xF000
#define _S_IFDIR  0x4000
#define _S_IFCHR  0x2000
#define _S_IFIFO  0x1000
#define _S_IFREG  0x8000
#define _S_IREAD  0x0100
#define _S_IWRITE 0x0080
#define _S_IEXEC  0x0040
#endif

/* ------------------------------------------------------------------ */
/* POSIX file-type bits                                               */
/* ------------------------------------------------------------------ */

#ifndef S_IFMT
#define S_IFMT   _S_IFMT
#define S_IFDIR  _S_IFDIR
#define S_IFCHR  _S_IFCHR
#define S_IFIFO  _S_IFIFO
#define S_IFREG  _S_IFREG
#endif
#ifndef S_IFLNK
#define S_IFLNK  0120000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
#ifndef S_IFBLK
#define S_IFBLK  0060000
#endif

/* ------------------------------------------------------------------ */
/* POSIX permission bits                                              */
/* ------------------------------------------------------------------ */

#ifndef S_IRWXU
#define S_IRWXU  0700
#define S_IRUSR  0400
#define S_IWUSR  0200
#define S_IXUSR  0100
#endif
#ifndef S_IRWXG
#define S_IRWXG  0070
#define S_IRGRP  0040
#define S_IWGRP  0020
#define S_IXGRP  0010
#endif
#ifndef S_IRWXO
#define S_IRWXO  0007
#define S_IROTH  0004
#define S_IWOTH  0002
#define S_IXOTH  0001
#endif
#ifndef S_ISUID
#define S_ISUID  04000
#endif
#ifndef S_ISGID
#define S_ISGID  02000
#endif
#ifndef S_ISVTX
#define S_ISVTX  01000
#endif

/* ------------------------------------------------------------------ */
/* File-type test macros                                              */
/* ------------------------------------------------------------------ */

#ifndef S_ISLNK
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#ifndef S_ISBLK
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#ifndef S_ISREG
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#endif

/* ------------------------------------------------------------------ */
/* POSIX-compatible struct stat                                       */
/* ------------------------------------------------------------------ */

struct stat
{
  dev_t           st_dev;
  uint64_t        st_ino;
  mode_t          st_mode;
  nlink_t         st_nlink;
  uid_t           st_uid;
  gid_t           st_gid;
  dev_t           st_rdev;
  int64_t         st_size;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  blksize_t       st_blksize;
  blkcnt_t        st_blocks;
};

/* ------------------------------------------------------------------ */
/* Conversion helper and POSIX wrappers                               */
/*                                                                    */
/* IMPORTANT: these MUST appear BEFORE the st_atime / st_mtime /      */
/* st_ctime macros below, because they access the identically-named   */
/* fields on struct _stat64.                                          */
/* ------------------------------------------------------------------ */

static inline void
_stat64_to_posix_stat(const struct _stat64 *src, struct stat *dst)
{
  memset(dst, 0, sizeof(*dst));
  dst->st_dev          = src->st_dev;
  dst->st_ino          = src->st_ino;
  dst->st_mode         = src->st_mode;
  dst->st_nlink        = (nlink_t)src->st_nlink;
  dst->st_uid          = 0;
  dst->st_gid          = 0;
  dst->st_rdev         = src->st_rdev;
  dst->st_size         = src->st_size;
  dst->st_atim.tv_sec  = (time_t)src->st_atime;
  dst->st_atim.tv_nsec = 0;
  dst->st_mtim.tv_sec  = (time_t)src->st_mtime;
  dst->st_mtim.tv_nsec = 0;
  dst->st_ctim.tv_sec  = (time_t)src->st_ctime;
  dst->st_ctim.tv_nsec = 0;
  dst->st_blksize      = 4096;
  dst->st_blocks       = (blkcnt_t)((src->st_size + 511) / 512);
}

static inline int stat(const char *path, struct stat *buf)
{
  struct _stat64 tmp;
  int rv = _stat64(path, &tmp);
  if(rv == 0)
    _stat64_to_posix_stat(&tmp, buf);
  return rv;
}

static inline int fstat(int fd, struct stat *buf)
{
  struct _stat64 tmp;
  int rv = _fstat64(fd, &tmp);
  if(rv == 0)
    _stat64_to_posix_stat(&tmp, buf);
  return rv;
}

static inline int lstat(const char *path, struct stat *buf)
{
  return stat(path, buf);  /* Windows doesn't distinguish symlinks */
}

static inline int fstatat(int dirfd, const char *path,
                          struct stat *buf, int flags)
{
  (void)dirfd; (void)flags;
  return stat(path, buf);
}

/* ------------------------------------------------------------------ */
/* Linux-compatible time-accessor macros                              */
/*                                                                    */
/* After this point, any use of  x->st_atime  expands to              */
/* x->st_atim.tv_sec, which works on our struct stat but NOT on       */
/* struct _stat64.  All _stat64 access must be above this line.       */
/* ------------------------------------------------------------------ */

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#else
/* Real POSIX system */
#include_next <sys/stat.h>
#endif
