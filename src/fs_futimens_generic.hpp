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
#include "to_neg_errno.hpp"

namespace fs
{
  static
  inline
  int
  futimens(const int             fd_,
           const struct timespec ts_[2])
  {
    int rv;
    struct timeval  tv[2];
    struct timeval *tvp;

    if(::_timespec_invalid(ts_))
      return -EINVAL;
    if(::_should_ignore(ts_))
      return 0;

    rv = ::_convert_timespec_to_timeval(fd_,ts_,tv,&tvp);
    if(rv < 0)
      return rv;

    return ::to_neg_errno(::futimes(fd_,tvp));
  }
}
