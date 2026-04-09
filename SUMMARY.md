# mergerfs Windows Port — Phase 0 & Phase 1 Summary

## Goal

Port mergerfs (a Linux FUSE union/merge filesystem, ~151 C++17 source files) to compile on Windows using MSVC (`cl.exe`/`link.exe`) invoked from MSYS2, with WinFSP providing FUSE compatibility. The objective for Phase 0 and Phase 1 was:

1. **Phase 0:** Create a working build system — Makefile adapted for Windows/MSVC, platform detection, WinFSP SDK integration, vendored libfuse skipped on Windows.
2. **Phase 1:** Create a POSIX compatibility layer so that all 151 mergerfs source files compile on MSVC without errors, deferring actual Windows API implementations to later phases.

## What Was Done

### Phase 0: Build System (completed prior to this session)

- Makefile adapted: platform detection (MSYS/MINGW → `windows`), MSVC compiler flags (`-std:c++17`, `-EHsc`, `-permissive-`, etc.), WinFSP SDK paths, skip vendored libfuse on Windows.
- Toolchain: `cl.exe`/`link.exe` from MSVC, invoked via MSYS2 shell with pre-configured `PATH`/`INCLUDE`/`LIB`.

### Phase 1: POSIX Compat Layer & Portability Fixes

Rather than creating a `src/platform/` abstraction (as originally planned), we used **shadow headers** in `src/compat/` that intercept standard `#include` directives and provide POSIX-compatible types, macros, and stub functions on MSVC.

#### Compat Headers Created/Modified

| Header | Purpose |
|---|---|
| `compat/unistd.h` | POSIX types (`uid_t`, `gid_t`, `pid_t`, `mode_t`, `ssize_t`, `nlink_t`, `blksize_t`, `blkcnt_t`), macros (`AT_FDCWD`, `O_DIRECTORY`, `O_NOFOLLOW`, `F_OK`/`R_OK`/`W_OK`/`X_OK`, `PATH_MAX`, `STDIN_FILENO`, `CLOCK_REALTIME`, signal constants), function stubs (`open`, `mkdir`, `mkdirat`, `truncate`, `openat`, `fcntl`, `ioctl`, `pread`/`pwrite`, `ftruncate`, `fsync`, `realpath`, `faccessat`, `futimesat`, `utimensat`, `symlink`, `readlink`, `link`, `chown`/`fchown`/`lchown`, `chmod`/`fchmod`/`fchmodat`, `mknod`, `sigaction`, `sigprocmask`, `pthread_sigmask`, `clock_gettime`, `sysconf`, `fpathconf`, `pathconf`, `copy_file_range`, `setpriority`/`getpriority`, `setreuid`/`setregid`, `getpid`/`getuid`/`getgid`/`geteuid`/`getegid`, `__builtin_expect`, `__builtin_mul_overflow`) |
| `compat/sys/stat.h` | Shadow for MSVC's `<sys/stat.h>`. Provides POSIX-compatible `struct stat` with `st_atim`/`st_mtim`/`st_ctim` (struct timespec members), `st_blocks`, `st_blksize`. Wrapper functions (`stat`, `lstat`, `fstat`, `fstatat`) call `_stat64`/`_fstat64` internally and convert via `_stat64_to_posix_stat()`. File mode macros (`S_ISREG`, `S_ISDIR`, `S_IFLNK`, etc.) |
| `compat/sys/time.h` | `struct timeval`, `gettimeofday()` (via `GetSystemTimeAsFileTime`), `futimes()`, `lutimes()` stubs, `TIMESPEC_TO_TIMEVAL`/`TIMEVAL_TO_TIMESPEC` macros |
| `compat/dirent.h` | `struct dirent` (with `d_ino`, `d_off`, `d_reclen`, `d_type`, `d_name`), `DIR`, `opendir`/`readdir`/`closedir` (via `FindFirstFileA`/`FindNextFileA`), `DT_UNKNOWN`/`DT_DIR`/`DT_REG`/`DT_LNK` |
| `compat/pthread.h` | Minimal pthreads stubs using Windows threads (`pthread_create`, `pthread_join`, `pthread_mutex_*`, `pthread_rwlock_*`, `pthread_cond_*`, `pthread_exit`) |
| `compat/off_t_fix.h` | Force-included (`-FI`) before all source files to define `off_t` as `int64_t` before MSVC's `<sys/types.h>` can define it as 32-bit `long` |
| `compat/win32_undef.h` | Undefines problematic Windows macros (`near`, `far`, `small`, `CreateFile`, `DeleteFile`, `MoveFile`, `CopyFile`, `GetCurrentDirectory`, `CreateDirectory`, `RemoveDirectory`, `GetFreeSpace`, `__reserved`, etc.) |
| `compat/sys/statvfs.h` | `struct statvfs`, `statvfs()`/`fstatvfs()` stubs |
| Other compat headers | `sys/file.h`, `sys/ioctl.h`, `sys/mount.h`, `sys/resource.h`, `sys/syscall.h`, `sys/sysmacros.h`, `sys/uio.h`, `syslog.h`, `grp.h` — minimal stubs |

#### Source Files Modified for Portability

| Category | Files | Fix |
|---|---|---|
| `fs::path` wrapper | `fs_path.hpp` | Created wrapper class on Windows with implicit `std::string` conversion (MSVC's `path::operator string_type()` returns `wstring`). Added constructors from `std::filesystem::path` base class. |
| `path.c_str()` → `.string().c_str()` | `branches.cpp`, `fuse_link.cpp`, `fs_mktemp.cpp`, `option_parser.cpp`, `fs_fstatat.hpp`, `fs_mkdirat.hpp`, `fs_mkdir.hpp`, `fuse_rename.cpp` | On Windows `path::c_str()` returns `wchar_t*`; changed to `.string().c_str()` for `const char*` |
| `string_view` iterators | `from_string.cpp`, `fuse_readdir_factory.cpp` | MSVC's `string_view::iterator` is a class, not `const char*`. Changed `.begin()`/`.end()` to `.data()`/`.data()+.size()` for `std::from_chars` and `std::regex_match`. |
| `__attribute__((constructor))` | `rnd.cpp` | Replaced with MSVC static object auto-init pattern |
| Designated initializers | `fs_cow.cpp`, `fs_movefile_and_open.cpp`, `mergerfs_fsck.cpp` | `{.field=value}` requires C++20 on MSVC; changed to positional initialization |
| Duplicate function defs | `fs_futimens_generic.hpp`, `fs_utimensat_generic.hpp` | Extracted shared timespec helpers into `fs_timespec_helpers.hpp` |
| Circular includes | `fs_utimensat_generic.hpp` ↔ `fs_lutimens.hpp` | Removed circular include, added inline `fs::lutimes` wrapper |
| `#error "Not Supported!"` | `fs_mknod_as.hpp`, `fs_open_as.hpp`, `fs_symlink_as.hpp` | Added `#elif defined(_WIN32)` cases (delegate to fs:: function directly, no uid/gid) |
| Missing includes | ~15 files | Added `#ifdef _WIN32 / #include <unistd.h>` for stubs (`fcntl`, `open`, `realpath`, `truncate`, `PATH_MAX`, `ssize_t`, `mode_t`, etc.) |
| Platform-specific code | `fs_umount2.hpp`, `fs_getdents64.cpp`, `fs_sendfile.hpp` | Added `#ifdef _WIN32` stubs returning `-ENOSYS`/`-ENOTSUP` |
| `subprocess.hpp` (vendored) | `vendored/subprocess/subprocess.hpp` | Added `#ifdef _WINDOWS_` guard to skip manual Win32 declarations when `<windows.h>` is already included |
| `fuse_kernel.h` (vendored) | `vendored/libfuse/include/fuse_kernel.h` | `#undef __reserved` (Windows SDK `sal.h` macro breaks struct fields) |
| `stat_utils.h` (vendored) | `vendored/libfuse/include/stat_utils.h` | Added `_WIN32` alongside `__linux__` for `ST_ATIM_NSEC` macros |
| `mutex_debug.hpp` (vendored) | `vendored/libfuse/include/mutex_debug.hpp` | Added `#include <unistd.h>` for `CLOCK_REALTIME`/`clock_gettime` on Windows |
| Makefile | `Makefile` | Added `-FS` (parallel PDB writes), `-FIoff_t_fix.h` (force-include for 64-bit off_t) |

#### Key Technical Challenges Solved

1. **`struct stat` incompatibility**: MSVC's `struct _stat64` lacks `st_atim`/`st_mtim`/`st_ctim` (timespec), `st_blocks`, `st_blksize`. Created a full shadow `compat/sys/stat.h` that declares `_stat64` directly (avoiding recursive inclusion of the real header), defines a POSIX `struct stat`, and provides conversion wrappers.

2. **`off_t` is 32-bit on MSVC**: Windows MSVC defines `off_t` as `long` (32-bit even on Win64). Used a force-included header (`-FIoff_t_fix.h`) that defines `_OFF_T_DEFINED` with `int64_t` typedef before any standard header can set the 32-bit version.

3. **`std::filesystem::path` portability**: On Windows, `path::c_str()` returns `wchar_t*` and `path::operator string_type()` returns `wstring`. Created a wrapper class in `fs_path.hpp` that adds `operator std::string()` and constructors from the base class (needed because `operator/` returns `std::filesystem::path`, not `fs::path`).

4. **Windows macro pollution**: Windows SDK headers define macros like `near`, `far`, `CreateFile`, `DeleteFile`, `__reserved` that collide with code identifiers. Handled via `win32_undef.h` included after `<windows.h>`.

5. **`#include_next` not available on MSVC**: Compat shadow headers that need to include the "real" header can't use `#include_next`. Worked around by declaring MSVC CRT types directly (for `sys/stat.h`) or using the `-FI` force-include approach (for `off_t`).

## Result

- **151/151** mergerfs `.cpp` source files compile to `.obj` files with zero compilation errors.
- The only remaining errors are **linker errors** from unresolved symbols in the vendored libfuse library (not yet ported) and the fmt library (not yet compiled). These are expected and will be resolved in Phase 2.

## What's Next

- **Phase 2**: Port/stub the vendored libfuse C library for Windows (or bridge to WinFSP's FUSE compat layer). Compile the fmt library. Resolve all linker errors.
- **Phase 2+**: Implement actual Windows API calls in the stub functions (currently most return 0 or -ENOSYS). Wire up WinFSP FUSE operations.
