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

#pragma once

#include "to_neg_errno.hpp"

#include <string>

#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <errno.h>

// POSIX-semantics delete: removes the directory entry immediately even
// when other handles are open (matching Linux unlink behavior).
// Requires Windows 10 1709+ and NTFS.
static
inline
int
_unlink_posix(const char *path_)
{
  HANDLE h;

  // DELETE=0x10000, FILE_SHARE_DELETE=4 (undef'd by win32_undef.h)
  h = CreateFileA(path_,
                  0x00010000L,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | 4,
                  NULL,
                  OPEN_EXISTING,
                  FILE_FLAG_OPEN_REPARSE_POINT,
                  NULL);
  if(h == INVALID_HANDLE_VALUE)
    {
      errno = EACCES;
      return -1;
    }

  // FileDispositionInfoEx = 64
  // FILE_DISPOSITION_FLAG_DELETE = 0x1
  // FILE_DISPOSITION_FLAG_POSIX_SEMANTICS = 0x2
  struct { DWORD Flags; } info;
  info.Flags = 0x1 | 0x2;
  BOOL ok = SetFileInformationByHandle(h,
                                       (FILE_INFO_BY_HANDLE_CLASS)64,
                                       &info,
                                       sizeof(info));
  if(!ok)
    {
      // Fallback: try standard disposition (works when no conflicting handles)
      FILE_DISPOSITION_INFO fdi;
      fdi.DeleteFile = TRUE;
      ok = SetFileInformationByHandle(h,
                                      FileDispositionInfo,
                                      &fdi,
                                      sizeof(fdi));
    }

  CloseHandle(h);

  if(!ok)
    {
      errno = EACCES;
      return -1;
    }

  return 0;
}
#endif


namespace fs
{
  static
  inline
  int
  unlink(const char *path_)
  {
    int rv;

    rv = ::unlink(path_);
#ifdef _WIN32
    if(rv < 0)
      rv = _unlink_posix(path_);
#endif

    return ::to_neg_errno(rv);
  }

  static
  inline
  int
  unlink(const std::string &path_)
  {
    return fs::unlink(path_.c_str());
  }
}
