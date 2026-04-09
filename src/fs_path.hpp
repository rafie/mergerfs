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

#include <filesystem>
#include <string>


namespace fs
{
#ifdef _WIN32
  /*
   * On MSVC/Windows, std::filesystem::path::operator string_type()
   * returns std::wstring, not std::string.  GCC/POSIX returns std::string.
   * Wrap the type to add an implicit std::string conversion so existing
   * code that passes fs::path to functions taking const std::string& works.
   */
  class path : public std::filesystem::path
  {
  public:
    using std::filesystem::path::path;
    using std::filesystem::path::operator=;

    /* Allow construction from base class (e.g. operator/ returns base) */
    path(const std::filesystem::path &p) : std::filesystem::path(p) {}
    path(std::filesystem::path &&p) : std::filesystem::path(std::move(p)) {}

    /* Allow assignment from base class */
    path& operator=(const std::filesystem::path &p) { std::filesystem::path::operator=(p); return *this; }
    path& operator=(std::filesystem::path &&p) { std::filesystem::path::operator=(std::move(p)); return *this; }

    /* Bring parity with GCC where path implicitly converts to std::string */
    operator std::string() const { return this->string(); }

    /*
     * Override operator/ to handle FUSE absolute paths correctly.
     * FUSE paths always start with "/" (e.g. "/", "/foo/bar").
     * On Windows, std::filesystem::path treats "/" as absolute (root of
     * current drive), which would replace the left operand entirely.
     * Instead, we want: branch_path / "/foo" → branch_path\foo
     */
    path& operator/=(const path &rhs)
    {
      std::string r = rhs.string();
      if(!r.empty() && (r[0] == '/' || r[0] == '\\'))
        {
          size_t pos = r.find_first_not_of("/\\");
          if(pos == std::string::npos)
            return *this;
          r = r.substr(pos);
        }
      std::filesystem::path::operator/=(std::filesystem::path(r));
      return *this;
    }

    path& operator/=(const std::string &rhs)
    {
      return this->operator/=(path(rhs));
    }

    path& operator/=(const char *rhs)
    {
      return this->operator/=(path(rhs));
    }

    friend path operator/(const path &lhs, const path &rhs)
    {
      path result(lhs);
      result /= rhs;
      return result;
    }

    friend path operator/(const path &lhs, const std::string &rhs)
    {
      path result(lhs);
      result /= rhs;
      return result;
    }

    friend path operator/(const path &lhs, const char *rhs)
    {
      path result(lhs);
      result /= rhs;
      return result;
    }
  };
#else
  using path = std::filesystem::path;
#endif
}
