/*
  Shared timespec/timeval conversion helpers for futimens and utimensat
  generic implementations.

  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>
*/

#pragma once

#include "fs_fstat.hpp"
#include "fs_fstatat.hpp"
#include "fs_stat_utils.hpp"

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef UTIME_NOW
# define UTIME_NOW  ((1l << 30) - 1l)
#endif

#ifndef UTIME_OMIT
# define UTIME_OMIT ((1l << 30) - 2l)
#endif


static
inline
bool
_can_call_lutimes(const int          dirfd_,
                  const std::string &path_,
                  const int          flags_)
{
  return ((flags_ == AT_SYMLINK_NOFOLLOW) &&
          ((dirfd_ == AT_FDCWD) ||
           (path_[0] == '/')));
}

static
inline
bool
_should_ignore(const struct timespec ts_[2])
{
  return ((ts_ != NULL) &&
          (ts_[0].tv_nsec == UTIME_OMIT) &&
          (ts_[1].tv_nsec == UTIME_OMIT));
}

static
inline
bool
_should_be_set_to_now(const struct timespec ts_[2])
{
  return ((ts_ == NULL) ||
          ((ts_[0].tv_nsec == UTIME_NOW) &&
           (ts_[1].tv_nsec == UTIME_NOW)));
}

static
inline
bool
_timespec_invalid(const struct timespec &ts_)
{
  return (((ts_.tv_nsec < 0) ||
           (ts_.tv_nsec > 999999999)) &&
          ((ts_.tv_nsec != UTIME_NOW) &&
           (ts_.tv_nsec != UTIME_OMIT)));
}

static
inline
bool
_timespec_invalid(const struct timespec ts_[2])
{
  return ((ts_ != NULL) &&
          (::_timespec_invalid(ts_[0]) ||
           ::_timespec_invalid(ts_[1])));
}

static
inline
bool
_flags_invalid(const int flags_)
{
  return ((flags_ & ~AT_SYMLINK_NOFOLLOW) != 0);
}

static
inline
bool
_any_timespec_is_utime_omit(const struct timespec ts_[2])
{
  return ((ts_[0].tv_nsec == UTIME_OMIT) ||
          (ts_[1].tv_nsec == UTIME_OMIT));
}

static
inline
bool
_any_timespec_is_utime_now(const struct timespec ts_[2])
{
  return ((ts_[0].tv_nsec == UTIME_NOW) ||
          (ts_[1].tv_nsec == UTIME_NOW));
}

static
inline
int
_set_utime_omit_to_current_value(const int              dirfd_,
                                 const std::string     &path_,
                                 const struct timespec  ts_[2],
                                 struct timeval         tv_[2],
                                 const int              flags_)
{
  int rv;
  struct stat st;
  timespec *atime;
  timespec *mtime;

  if(!::_any_timespec_is_utime_omit(ts_))
    return 0;

  rv = fs::fstatat(dirfd_,path_,&st,flags_);
  if(rv < 0)
    return rv;

  atime = fs::stat_atime(&st);
  mtime = fs::stat_mtime(&st);

  if(ts_[0].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv_[0],atime);
  if(ts_[1].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv_[1],mtime);

  return 0;
}

static
inline
int
_set_utime_omit_to_current_value(const int             fd_,
                                 const struct timespec ts_[2],
                                 struct timeval        tv_[2])
{
  int rv;
  struct stat st;
  timespec *atime;
  timespec *mtime;

  if(!::_any_timespec_is_utime_omit(ts_))
    return 0;

  rv = fs::fstat(fd_,&st);
  if(rv < 0)
    return rv;

  atime = fs::stat_atime(&st);
  mtime = fs::stat_mtime(&st);

  if(ts_[0].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv_[0],atime);
  if(ts_[1].tv_nsec == UTIME_OMIT)
    TIMESPEC_TO_TIMEVAL(&tv_[1],mtime);

  return 0;
}

static
inline
int
_set_utime_now_to_now(const struct timespec ts_[2],
                      struct timeval        tv_[2])
{
  int rv;
  struct timeval now;

  if(!::_any_timespec_is_utime_now(ts_))
    return 0;

  rv = ::gettimeofday(&now,NULL);
  if(rv < 0)
    return rv;

  if(ts_[0].tv_nsec == UTIME_NOW)
    tv_[0] = now;
  if(ts_[1].tv_nsec == UTIME_NOW)
    tv_[1] = now;

  return 0;
}

static
inline
int
_convert_timespec_to_timeval(const int               dirfd_,
                             const std::string      &path_,
                             const struct timespec   ts_[2],
                             struct timeval          tv_[2],
                             struct timeval        **tvp_,
                             const int               flags_)
{
  int rv;

  if(::_should_be_set_to_now(ts_))
    return (*tvp_=NULL,0);

  TIMESPEC_TO_TIMEVAL(&tv_[0],&ts_[0]);
  TIMESPEC_TO_TIMEVAL(&tv_[1],&ts_[1]);

  rv = ::_set_utime_omit_to_current_value(dirfd_,path_,ts_,tv_,flags_);
  if(rv < 0)
    return rv;

  rv = ::_set_utime_now_to_now(ts_,tv_);
  if(rv < 0)
    return rv;

  return (*tvp_=tv_,0);
}

static
inline
int
_convert_timespec_to_timeval(const int               fd_,
                             const struct timespec   ts_[2],
                             struct timeval          tv_[2],
                             struct timeval        **tvp_)
{
  int rv;

  if(::_should_be_set_to_now(ts_))
    return (*tvp_=NULL,0);

  TIMESPEC_TO_TIMEVAL(&tv_[0],&ts_[0]);
  TIMESPEC_TO_TIMEVAL(&tv_[1],&ts_[1]);

  rv = ::_set_utime_omit_to_current_value(fd_,ts_,tv_);
  if(rv < 0)
    return rv;

  rv = ::_set_utime_now_to_now(ts_,tv_);
  if(rv < 0)
    return rv;

  return (*tvp_=tv_,0);
}
