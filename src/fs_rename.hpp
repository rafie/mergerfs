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

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <errno.h>
#include <cstring>

// POSIX-semantics rename: works even when source has open handles
// (matching Linux rename behavior).
// Requires Windows 10 1709+ and NTFS.
static
inline
int
_rename_posix(const char *oldpath_,
              const char *newpath_)
{
  HANDLE h;

  // DELETE=0x10000, SYNCHRONIZE=0x100000, FILE_SHARE_DELETE=4 (undef'd by win32_undef.h)
  h = CreateFileA(oldpath_,
                  0x00010000L | 0x00100000L,
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

  // Convert newpath to wide string for FILE_RENAME_INFO
  int wlen = MultiByteToWideChar(CP_ACP, 0, newpath_, -1, NULL, 0);
  if(wlen <= 0)
    {
      CloseHandle(h);
      errno = EINVAL;
      return -1;
    }

  // Use FILE_RENAME_INFO struct with FileRenameInfoEx (class 65)
  // Flags: FILE_RENAME_FLAG_REPLACE_IF_EXISTS=0x1 | FILE_RENAME_FLAG_POSIX_SEMANTICS=0x2
  DWORD nameBytes = (DWORD)((wlen - 1) * sizeof(WCHAR));
  size_t infoSize = FIELD_OFFSET(FILE_RENAME_INFO, FileName) + nameBytes + sizeof(WCHAR);
  FILE_RENAME_INFO *info = (FILE_RENAME_INFO*)calloc(1, infoSize);
  if(!info)
    {
      CloseHandle(h);
      errno = ENOMEM;
      return -1;
    }

  // Overlay Flags at struct start (union with ReplaceIfExists)
  *(DWORD*)info = 0x1 | 0x2;
  info->RootDirectory = NULL;
  info->FileNameLength = nameBytes;
  MultiByteToWideChar(CP_ACP, 0, newpath_, -1, info->FileName, wlen);

  BOOL ok = SetFileInformationByHandle(h,
                                       (FILE_INFO_BY_HANDLE_CLASS)65,
                                       info,
                                       (DWORD)infoSize);

  if(!ok)
    {
      // Fallback: try MoveFileExA with REPLACE_EXISTING
      CloseHandle(h);
      free(info);
      ok = MoveFileExA(oldpath_, newpath_, MOVEFILE_REPLACE_EXISTING);
      if(!ok)
        {
          errno = EACCES;
          return -1;
        }
      return 0;
    }

  CloseHandle(h);
  free(info);
  return 0;
}
#endif


namespace fs
{
  static
  inline
  int
  rename(const char *oldpath_,
         const char *newpath_)
  {
    int rv;

    rv = ::rename(oldpath_,newpath_);
#ifdef _WIN32
    if(rv < 0)
      rv = _rename_posix(oldpath_,newpath_);
#endif

    return ::to_neg_errno(rv);
  }

  static
  inline
  int
  rename(const std::string &oldpath_,
         const std::string &newpath_)
  {
    return fs::rename(oldpath_.c_str(),newpath_.c_str());
  }
}
