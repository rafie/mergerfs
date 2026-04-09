/*
  MSYS-style ↔ Windows native path conversion.

  /d/path         → D:\path
  //server/share  → \\server\share
  /m              → M:\

  Used at OS boundaries so internal mergerfs logic can use MSYS-style
  paths (which avoid colons and thus don't conflict with the branch
  delimiter).
*/

#pragma once

#include <string>
#include <cctype>

namespace msys_path
{
  // Convert MSYS-style path to Windows native path.
  // Returns the input unchanged if it doesn't match an MSYS pattern.
  static
  inline
  std::string
  to_native(const std::string &path_)
  {
#ifndef _WIN32
    return path_;
#else
    if(path_.empty())
      return path_;

    // UNC path: //server/share/... → \\server\share\...
    if(path_.size() >= 2 && path_[0] == '/' && path_[1] == '/')
      {
        std::string result = path_;
        for(auto &c : result)
          if(c == '/') c = '\\';
        return result;
      }

    // Drive letter path: /d/... → D:\...
    // Single letter after leading / that is alpha
    if(path_.size() >= 2 && path_[0] == '/' && std::isalpha((unsigned char)path_[1]))
      {
        // Must be /x or /x/ or /x/...
        if(path_.size() == 2 || path_[2] == '/')
          {
            std::string result;
            result += (char)std::toupper((unsigned char)path_[1]);
            result += ':';
            if(path_.size() > 2)
              {
                result += path_.substr(2);
                for(size_t i = 2; i < result.size(); i++)
                  if(result[i] == '/') result[i] = '\\';
              }
            else
              {
                result += '\\';
              }
            return result;
          }
      }

    // Not an MSYS-style path — return with forward slashes converted
    std::string result = path_;
    for(auto &c : result)
      if(c == '/') c = '\\';
    return result;
#endif
  }

  // Convert Windows native path to MSYS-style path.
  static
  inline
  std::string
  to_msys(const std::string &path_)
  {
#ifndef _WIN32
    return path_;
#else
    if(path_.empty())
      return path_;

    // UNC path: \\server\share\... → //server/share/...
    if(path_.size() >= 2 && path_[0] == '\\' && path_[1] == '\\')
      {
        std::string result = path_;
        for(auto &c : result)
          if(c == '\\') c = '/';
        return result;
      }

    // Drive letter: D:\... → /d/...
    if(path_.size() >= 2 && std::isalpha((unsigned char)path_[0]) && path_[1] == ':')
      {
        std::string result = "/";
        result += (char)std::tolower((unsigned char)path_[0]);
        if(path_.size() > 2)
          {
            result += path_.substr(2);
            for(size_t i = 2; i < result.size(); i++)
              if(result[i] == '\\') result[i] = '/';
          }
        return result;
      }

    // Not a Windows-style path — return with backslashes converted
    std::string result = path_;
    for(auto &c : result)
      if(c == '\\') c = '/';
    return result;
#endif
  }
}
