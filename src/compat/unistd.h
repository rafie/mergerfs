/*
  POSIX unistd.h compatibility shim for MSVC/Windows.
  Provides types, macros, and function declarations needed by mergerfs.
*/

#pragma once

#ifdef _WIN32

#include <io.h>
#include <process.h>
#include <direct.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <windows.h>
#include "win32_undef.h"

/* POSIX types not provided by MSVC */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif

typedef int64_t off64_t;

/* These types are also defined in compat/sys/stat.h — use same guards */
#ifndef _UID_GID_T_DEFINED_COMPAT
#define _UID_GID_T_DEFINED_COMPAT
typedef uint32_t uid_t;
typedef uint32_t gid_t;
#endif

#ifndef _PID_T_DEFINED
typedef int      pid_t;
#define _PID_T_DEFINED
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

typedef uint64_t fsblkcnt_t;
typedef uint64_t fsfilcnt_t;

/* Standard file descriptors */
#ifndef STDIN_FILENO
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

/* Access mode flags */
#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

/* faccessat flags */
#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#ifndef AT_EACCESS
#define AT_EACCESS 0x200
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x200
#endif
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x1000
#endif

/* File mode/type bits and struct stat are now in compat/sys/stat.h */

/* Open flags not in MSVC */
#ifndef O_ACCMODE
#define O_ACCMODE   (O_RDONLY | O_WRONLY | O_RDWR)
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x100000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW  0x200000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK  0x400000
#endif
#ifndef O_PATH
#define O_PATH      0x800000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC   0x1000000
#endif

/* fcntl commands */
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

/* Limits */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/* sysconf names */
#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 30
#endif
#ifndef _PC_NAME_MAX
#define _PC_NAME_MAX 3
#endif

/* Clock */
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static inline int clock_gettime(int clk_id, struct timespec *tp)
{
  (void)clk_id;
  struct _timespec64 ts;
  _timespec64_get(&ts, TIME_UTC);
  /* Cast through void* since struct timespec may vary */
  ((int64_t*)tp)[0] = ts.tv_sec;
  ((long*)((char*)tp + sizeof(int64_t)))[0] = (long)ts.tv_nsec;
  return 0;
}

/* GCC builtins */
#ifndef __builtin_expect
#define __builtin_expect(expr, val) (expr)
#endif

/* On Win64, size_t and uint64_t are the same type (unsigned __int64),
   so we only need int64_t and uint64_t overloads. */
static inline int __builtin_mul_overflow(int64_t a, int64_t b, int64_t *res)
{
  *res = a * b;
  if(a == 0) return 0;
  return (*res / a != b);
}

static inline int __builtin_mul_overflow(uint64_t a, uint64_t b, uint64_t *res)
{
  *res = a * b;
  if(a == 0) return 0;
  return (*res / a != b);
}

/* Misc POSIX macros */
#ifndef TIMESPEC_TO_TIMEVAL
#define TIMESPEC_TO_TIMEVAL(tv, ts) do { \
  (tv)->tv_sec = (long)(ts)->tv_sec;     \
  (tv)->tv_usec = (long)(ts)->tv_nsec / 1000; \
} while(0)
#endif

#ifndef TIMEVAL_TO_TIMESPEC
#define TIMEVAL_TO_TIMESPEC(tv, ts) do { \
  (ts)->tv_sec = (tv)->tv_sec;           \
  (ts)->tv_nsec = (tv)->tv_usec * 1000;  \
} while(0)
#endif

/* Convert dirent d_type to stat mode */
#ifndef DTTOIF
#define DTTOIF(dirtype) ((dirtype) << 12)
#endif
#ifndef IFTODT
#define IFTODT(mode)    (((mode) & 0170000) >> 12)
#endif

/* Signals — minimal stubs */
#ifndef SIGUSR1
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGIO   29
#endif
#ifndef SIG_BLOCK
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#endif
typedef unsigned long sigset_t;

/* Resource limits */
#ifndef RLIMIT_FSIZE
#define RLIMIT_FSIZE 1
#endif
#ifndef RLIM_INFINITY
#define RLIM_INFINITY ((uint64_t)-1)
#endif

/* Process priority */
#ifndef PRIO_PROCESS
#define PRIO_PROCESS 0
#endif

/* errno extensions */
#ifndef EDQUOT
#define EDQUOT 122
#endif

/* Mount flags */
#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif

/* Note: we do NOT use #define macros for POSIX→MSVC function mapping
   because they break C++ namespaced/member functions (e.g. fs::unlink). */

static inline pid_t getpid(void) { return _getpid(); }
static inline uid_t getuid(void) { return 0; }
static inline gid_t getgid(void) { return 0; }
static inline uid_t geteuid(void) { return 0; }
static inline gid_t getegid(void) { return 0; }

/* lstat, fstat, fstatat, stat — now in compat/sys/stat.h */

/* openat — simplified, ignores dirfd, delegates to our open() */
static inline int openat(int dirfd, const char *path, int flags, ...)
{
  (void)dirfd;
  if(flags & _O_CREAT)
    {
      va_list ap;
      va_start(ap, flags);
      int mode = va_arg(ap, int);
      va_end(ap);
      return open(path, flags, mode);
    }
  return open(path, flags);
}

/* fcntl — stub */
static inline int fcntl(int fd, int cmd, ...)
{
  (void)fd; (void)cmd;
  return 0;
}

/* ioctl — stub */
static inline int ioctl(int fd, unsigned long request, ...)
{
  (void)fd; (void)request;
  errno = ENOSYS;
  return -1;
}

static inline long sysconf(int name)
{
  if(name == _SC_PAGESIZE)
    return 4096;
  return -1;
}

static inline long fpathconf(int fd, int name)
{
  (void)fd;
  if(name == _PC_NAME_MAX) return NAME_MAX;
  return -1;
}

/* pread/pwrite — implemented via _lseeki64 + _read/_write */
static inline ssize_t pread(int fd, void *buf, size_t count, int64_t offset)
{
  int64_t orig = _lseeki64(fd, 0, SEEK_CUR);
  if(orig == -1) return -1;
  if(_lseeki64(fd, offset, SEEK_SET) == -1) return -1;
  int n = _read(fd, buf, (unsigned int)count);
  _lseeki64(fd, orig, SEEK_SET);
  return n;
}

static inline ssize_t pwrite(int fd, const void *buf, size_t count, int64_t offset)
{
  int64_t orig = _lseeki64(fd, 0, SEEK_CUR);
  if(orig == -1) return -1;
  if(_lseeki64(fd, offset, SEEK_SET) == -1) return -1;
  int n = _write(fd, buf, (unsigned int)count);
  _lseeki64(fd, orig, SEEK_SET);
  return n;
}

static inline int ftruncate(int fd, int64_t length)
{
  return _chsize_s(fd, length);
}

static inline int fsync(int fd)
{
  return _commit(fd);
}

static inline char *realpath(const char *path, char *resolved)
{
  return _fullpath(resolved, path, PATH_MAX);
}

/* futimesat — stub, calls futimes */
static inline int futimesat(int dirfd, const char *path, const struct timeval tv[2])
{
  (void)dirfd; (void)path; (void)tv;
  return 0;
}

/* POSIX rename with 2 args — MSVC has rename in stdio.h */

static inline int faccessat(int dirfd, const char *path, int mode, int flags)
{
  (void)dirfd;
  (void)flags;
  return _access(path, mode & (R_OK | W_OK));
}

/* Signal stubs */
static inline int sigemptyset(sigset_t *set) { *set = 0; return 0; }
static inline int sigfillset(sigset_t *set) { *set = ~0UL; return 0; }
static inline int sigaddset(sigset_t *set, int sig) { *set |= (1UL << sig); return 0; }
static inline int sigdelset(sigset_t *set, int sig) { *set &= ~(1UL << sig); return 0; }
static inline int sigismember(const sigset_t *set, int sig) { return (*set & (1UL << sig)) != 0; }
static inline int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
  (void)how; (void)set; (void)oldset;
  return 0;
}
static inline int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
  return sigprocmask(how, set, oldset);
}

/* sigaction — stub */
#ifndef SA_RESTART
#define SA_RESTART 0x10000000
#endif
#ifndef SIG_IGN
#define SIG_IGN ((void(*)(int))1)
#endif
#ifndef SIG_DFL
#define SIG_DFL ((void(*)(int))0)
#endif
struct sigaction {
  void (*sa_handler)(int);
  sigset_t sa_mask;
  int sa_flags;
};
static inline int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
  (void)sig; (void)act; (void)oldact;
  return 0;
}

/* SYS_* for ugid syscall wrappers */
#ifndef SYS_setreuid
#define SYS_setreuid -1
#define SYS_setregid -1
#define SYS_geteuid  -1
#define SYS_getegid  -1
#endif

/* Stubs for functions irrelevant on Windows */
static inline int setreuid(uid_t r, uid_t e) { (void)r; (void)e; return 0; }
static inline int setregid(gid_t r, gid_t e) { (void)r; (void)e; return 0; }

/* futimes is in compat/sys/time.h */

static inline int utimensat(int dirfd, const char *path,
                            const struct timespec times[2], int flags)
{
  (void)dirfd; (void)path; (void)times; (void)flags;
  /* TODO: implement via SetFileTime */
  return 0;
}

/* fstatat — now in compat/sys/stat.h */

static inline long pathconf(const char *path, int name)
{
  (void)path;
  if(name == _PC_NAME_MAX) return NAME_MAX;
  return -1;
}

static inline ssize_t copy_file_range(int fd_in, int64_t *off_in,
                                      int fd_out, int64_t *off_out,
                                      size_t len, unsigned int flags)
{
  (void)fd_in; (void)off_in; (void)fd_out; (void)off_out;
  (void)len; (void)flags;
  errno = ENOSYS;
  return -1;
}

static inline int setpriority(int which, int who, int prio)
{
  (void)which; (void)who; (void)prio;
  return 0;
}

static inline int getpriority(int which, int who)
{
  (void)which; (void)who;
  return 0;
}

static inline int symlink(const char *target, const char *linkpath)
{
  (void)target; (void)linkpath;
  /* TODO: implement via CreateSymbolicLink */
  return -1;
}

static inline ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{
  (void)path; (void)buf; (void)bufsiz;
  /* TODO: implement via DeviceIoControl FSCTL_GET_REPARSE_POINT */
  return -1;
}

static inline int link(const char *oldpath, const char *newpath)
{
  (void)oldpath; (void)newpath;
  /* TODO: implement via CreateHardLink */
  return -1;
}

static inline int chown(const char *path, uid_t owner, gid_t group)
{
  (void)path; (void)owner; (void)group;
  return 0; /* no-op on Windows */
}

static inline int lchown(const char *path, uid_t owner, gid_t group)
{
  return chown(path, owner, group);
}

static inline int fchown(int fd, uid_t owner, gid_t group)
{
  (void)fd; (void)owner; (void)group;
  return 0; /* no-op on Windows */
}

static inline int chmod(const char *path, mode_t mode)
{
  (void)mode;
  return _chmod(path, _S_IREAD | _S_IWRITE);
}

static inline int fchmod(int fd, mode_t mode)
{
  (void)fd; (void)mode;
  return 0;
}

static inline int fchmodat(int dirfd, const char *path, mode_t mode, int flags)
{
  (void)dirfd; (void)flags;
  return chmod(path, mode);
}

static inline int mkdir(const char *path, mode_t mode)
{
  (void)mode;
  return _mkdir(path);
}

/* mkdirat — simplified, ignores dirfd */
static inline int mkdirat(int dirfd, const char *path, mode_t mode)
{
  (void)dirfd;
  (void)mode;
  return _mkdir(path);
}

/* Open file using Win32 CreateFileA so we can include FILE_SHARE_DELETE
   in the sharing mode.  This allows unlink/rename while the file is open
   (matching POSIX semantics).  Returns a CRT file descriptor via
   _open_osfhandle. */
static inline int open(const char *path, int flags, ...)
{
  DWORD access = 0;
  DWORD creation = OPEN_EXISTING;
  int osfFlags = 0;

  /* Access mode */
  if((flags & _O_RDWR) == _O_RDWR)
    { access = GENERIC_READ | GENERIC_WRITE; osfFlags = _O_RDWR; }
  else if(flags & _O_WRONLY)
    { access = GENERIC_WRITE; osfFlags = _O_WRONLY; }
  else
    { access = GENERIC_READ; osfFlags = _O_RDONLY; }

  /* Creation disposition */
  if(flags & _O_CREAT)
    {
      if(flags & _O_EXCL)
        creation = CREATE_NEW;
      else if(flags & _O_TRUNC)
        creation = CREATE_ALWAYS;
      else
        creation = OPEN_ALWAYS;
    }
  else if(flags & _O_TRUNC)
    {
      creation = TRUNCATE_EXISTING;
    }

  if(flags & _O_APPEND)
    osfFlags |= _O_APPEND;

  /* Always share read+write+delete so that unlink/rename work while open */
  DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | 4 /*FILE_SHARE_DELETE*/;

  HANDLE h = CreateFileA(path, access, shareMode,
                         NULL, creation,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if(h == INVALID_HANDLE_VALUE)
    {
      /* Map common Win32 errors to errno */
      DWORD err = GetLastError();
      if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
        errno = ENOENT;
      else if(err == ERROR_ACCESS_DENIED)
        errno = EACCES;
      else if(err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS)
        errno = EEXIST;
      else
        errno = EIO;
      return -1;
    }

  /* If O_CREAT: apply POSIX mode (make read-only if no write bits) */
  if((flags & _O_CREAT))
    {
      va_list ap;
      va_start(ap, flags);
      int mode = va_arg(ap, int);
      va_end(ap);
      if(!(mode & 0222))
        {
          /* Mark read-only via Win32 attribute */
          SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
        }
    }

  int fd = _open_osfhandle((intptr_t)h, osfFlags);
  if(fd < 0)
    {
      CloseHandle(h);
      errno = EMFILE;
      return -1;
    }

  return fd;
}

static inline int truncate(const char *path, int64_t length)
{
  int fd = open(path, _O_RDWR);
  if(fd < 0) return -1;
  int rv = _chsize_s(fd, length);
  _close(fd);
  return rv;
}

static inline int mknod(const char *path, mode_t mode, dev_t dev)
{
  (void)path; (void)mode; (void)dev;
  return -1; /* no device nodes on Windows */
}

#else
/* On real POSIX systems, just use the real header */
#include_next <unistd.h>
#endif
