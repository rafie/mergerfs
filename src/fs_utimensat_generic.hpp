/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "fs_timespec_helpers.hpp"
#include "fs_futimesat.hpp"
#include "to_neg_errno.hpp"

namespace fs
{
  static
  inline
  int
  lutimes(const std::string     &path_,
          const struct timeval   tv_[2])
  {
    return ::to_neg_errno(::lutimes(path_.c_str(),tv_));
  }

  static
  inline
  int
  utimensat(const int              dirfd_,
            const std::string     &path_,
            const struct timespec  ts_[2],
            const int              flags_)
  {
    int rv;
    struct timeval  tv[2];
    struct timeval *tvp;

    if(::_flags_invalid(flags_))
      return -EINVAL;
    if(::_timespec_invalid(ts_))
      return -EINVAL;
    if(::_should_ignore(ts_))
      return 0;

    rv = ::_convert_timespec_to_timeval(dirfd_,path_,ts_,tv,&tvp,flags_);
    if(rv < 0)
      return rv;

    if((flags_ & AT_SYMLINK_NOFOLLOW) == 0)
      return fs::futimesat(dirfd_,path_.c_str(),tvp);
    if(::_can_call_lutimes(dirfd_,path_,flags_))
      return fs::lutimes(path_,tvp);

    return -ENOTSUP;
  }
}
