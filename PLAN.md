# mergerfs Windows Port via WinFSP

## Overview

Port [mergerfs](https://github.com/rafie/mergerfs) — a Linux FUSE union/merge filesystem — to Windows using [WinFSP](https://github.com/winfsp/winfsp). The result is a Windows filesystem driver that presents multiple directories (branches) as a single merged mount point (drive letter or directory).

## Architecture Decision: FUSE Compat vs Native WinFSP API

**Decision: FUSE compatibility layer first, native API later (if ever).**

| Approach | Pros | Cons |
|---|---|---|
| WinFSP FUSE compat | Minimal code changes; mergerfs already fills `fuse_operations` and calls `fuse_main()` | Slight overhead; some Windows features inaccessible |
| WinFSP native API | Full Windows integration (security descriptors, named streams, reparse points) | Requires rewriting all ~50 operation handlers |

The FUSE compat layer covers all Tier 1 and Tier 2 operations. We can selectively drop to the native API for specific features later.

---

## Phase 0: Project Bootstrap

### 0.1 Repository Setup
- [x] mergerfs upstream repo clone is in `upstream` (from `https://github.com/trapexit/mergerfs`): use for reference
- [x] mergefs development repo clone is in `mergefs` (from `https://github.com/rafie/mergerfs`): development should be done here
- [x] WinFSP development artifacts are installed in `/c/root/dev/libs/winfsp`
- [x] WinFSP repo clone in `winfsp` (from `https://github.com/winfsp/winfsp.git`): use for reference
- [x] Add WinFSP SDK as a build dependency (header-only at compile time, DLL at runtime)

### 0.2 Build System
Since we operate in MSYS, we can keep GNU Make — no need to switch to CMake.
- [x] Adapt existing Makefile for Windows/MSYS: detect platform, swap libfuse for WinFSP
- [x] Toolchain: MSVC (`cl.exe`, `link.exe`) invoked from MSYS shell. Assume `PATH`, `INCLUDE`, and `LIB` are set for the MSVC/Windows SDK environment.
- [x] Makefile sets `CC=cl`, `CXX=cl`, adapts flags (`-std=c++17` → `/std:c++17`, `-O2` → `/O2`, `-Wall` → `/W3`, etc.)
- [x] Add WinFSP to `INCLUDE`/`LIB` or reference directly (e.g., `/c/root/dev/libs/winfsp/inc/fuse` and `lib/`)
- [ ] CI: GitHub Actions matrix (Windows MSYS + MSVC, Linux GCC)

### 0.3 Vendored libfuse
mergerfs vendors libfuse and builds it with Meson. On Windows we skip this entirely and link WinFSP's FUSE library instead. The Makefile must:
- [x] On Linux: build vendored libfuse as before
- [x] On Windows/MSYS: skip vendored libfuse, link WinFSP's FUSE compat library (`winfsp-x64.dll`)

---

## Phase 1: Platform Abstraction Layer ✅ COMPILATION COMPLETE

**Status (2026-04-08):** All 151/151 mergerfs source files compile successfully with MSVC. Remaining work is linker resolution (vendored libfuse needs porting — Phase 2).

**Approach taken:** Instead of `src/platform/`, used POSIX compat shim headers in `src/compat/` that shadow standard headers (`<unistd.h>`, `<sys/stat.h>`, `<sys/time.h>`, `<dirent.h>`, etc.) to provide POSIX types, macros, and stub functions on MSVC. Key files created/modified:
- `src/compat/unistd.h` — POSIX types, function stubs (open, mkdir, fcntl, sigaction, etc.)
- `src/compat/sys/stat.h` — POSIX struct stat with timespec members, wrapper functions
- `src/compat/sys/time.h` — gettimeofday, futimes, lutimes stubs
- `src/compat/dirent.h` — opendir/readdir/closedir using FindFirstFile/FindNextFile
- `src/compat/off_t_fix.h` — Force-included to make off_t 64-bit globally
- `src/compat/pthread.h` — pthreads stubs using Windows threads
- `src/fs_path.hpp` — Wrapper class for std::filesystem::path with implicit std::string conversion
- `src/fs_timespec_helpers.hpp` — Shared timespec/timeval helpers (extracted from generic impls)

### 1.1 Filesystem Operations (`fs_*` files)

These are the core files that wrap syscalls. Each needs a Windows implementation:

| File(s) | Linux API | Windows API | Notes |
|---|---|---|---|
| `fs_acl.*` | POSIX ACLs | Windows ACLs / WinFSP emulation | Low priority; WinFSP emulates POSIX perms |
| `fs_attr.*` | `lstat`, `fstat` | `GetFileAttributesEx`, `GetFileInformationByHandle` | Map `struct stat` fields |
| `fs_chmod.*` | `chmod`, `fchmod` | `SetFileAttributes` (partial) | Windows has no direct mode_t equivalent |
| `fs_chown.*` | `chown`, `fchown` | `SetNamedSecurityInfo` | Largely a no-op on typical Windows setups |
| `fs_clonefile.*` | `ioctl(FICLONE)` | `CopyFile` / `FSCTL_DUPLICATE_EXTENTS_TO_FILE` (ReFS) | Fallback to copy |
| `fs_clonepath.*` | `mkdir` chain + attrs | `CreateDirectory` chain + attrs | Path separator handling |
| `fs_copy_file_range.*` | `copy_file_range()` | Read+write fallback | No kernel equivalent |
| `fs_eaccess.*` | `eaccess` / `faccessat` | `AccessCheck` + token | Different privilege model |
| `fs_fallocate.*` | `fallocate` | `SetFileInformationByHandle(FileAllocationInfo)` | Partial equivalent |
| `fs_flock.*` | `flock` | `LockFileEx` / `UnlockFileEx` | WinFSP handles via FUSE compat |
| `fs_link.*` | `link` | `CreateHardLink` | NTFS only |
| `fs_lstat.*` | `lstat` | `GetFileAttributesEx` | No symlink-aware stat on Windows; use `FILE_FLAG_OPEN_REPARSE_POINT` |
| `fs_mkdir.*` | `mkdir` | `CreateDirectory` | Straightforward |
| `fs_mknod.*` | `mknod` | N/A | Device nodes don't exist on Windows; stub/error |
| `fs_open.*` | `open` | `CreateFile` | Flag mapping (O_RDONLY→GENERIC_READ, etc.) |
| `fs_read.*` | `read`/`pread` | `ReadFile` | WinFSP handles via FUSE compat |
| `fs_readlink.*` | `readlink` | `DeviceIoControl(FSCTL_GET_REPARSE_POINT)` | Windows symlinks are reparse points |
| `fs_rename.*` | `rename` | `MoveFileEx` | `MOVEFILE_REPLACE_EXISTING` for overwrite |
| `fs_rmdir.*` | `rmdir` | `RemoveDirectory` | Straightforward |
| `fs_stat.*` | `stat` | `GetFileInformationByHandle` | Map all fields |
| `fs_statvfs.*` | `statvfs` | `GetDiskFreeSpaceEx` | Map to `struct statvfs` |
| `fs_symlink.*` | `symlink` | `CreateSymbolicLink` | Requires Developer Mode or admin |
| `fs_truncate.*` | `truncate`/`ftruncate` | `SetFileInformationByHandle(FileEndOfFileInfo)` | Straightforward |
| `fs_unlink.*` | `unlink` | `DeleteFile` | Straightforward |
| `fs_utimens.*` | `utimensat`/`futimens` | `SetFileTime` | Map timespec→FILETIME |
| `fs_write.*` | `write`/`pwrite` | `WriteFile` | WinFSP handles via FUSE compat |
| `fs_xattr.*` | `setxattr`/`getxattr`/`listxattr`/`removexattr` | NTFS Alternate Data Streams or WinFSP emulation | WinFSP FUSE compat supports xattr callbacks |

### 1.2 System Services

| Concern | Linux | Windows | Approach |
|---|---|---|---|
| Logging | `syslog` | `OutputDebugString` / file log / Event Log | Simple file logger initially |
| Signals | `sigaction`, `SIGHUP` reload | `SetConsoleCtrlHandler` | Handle Ctrl+C, service stop |
| UID/GID | `getuid`/`getgid`/`setreuid` | N/A — delegate to underlying FS | Irrelevant on Windows; permissions handled by CIFS/NTFS ACLs. `chown` → no-op, `getattr` returns dummy uid/gid (0/0 or WinFSP SID mapping). Audit mergerfs for any internal uid/gid comparisons. |
| OOM score | `/proc/self/oom_score_adj` | N/A | Stub out |
| Capabilities | `prctl`/`capset` | N/A | Stub out |
| `/proc/self/fd/N` | Procfs | Handle table | Track open handles directly |
| Mount detection | `/proc/self/mountinfo` | `GetLogicalDriveStrings` / `GetVolumeInformation` | Different enumeration |

### 1.3 Path Handling

Use **MSYS-style paths** throughout so that the upstream `:` branch delimiter is preserved unchanged:

| Windows native | MSYS-style | Notes |
|---|---|---|
| `D:\media` | `/d/media` | Drive letter becomes `/x/` prefix |
| `\\server\share\path` | `//server/share/path` | UNC paths keep `//` prefix |
| `M:` (mount point) | `/m` | Mount as drive letter |

This means:
- **No delimiter change needed** — `:` works because paths never contain colons
- Forward slashes only — no `\` normalization needed
- Branch specs look identical to Linux: `branches=/d/media:/e/media:/f/media`
- Mount point spec: `/m` or a directory path

A **path conversion layer** translates MSYS-style ↔ Windows native at the OS boundary:
- Inbound (user config, CLI args): MSYS-style → stored as-is internally
- Outbound (Win32 API calls): MSYS-style → Windows native (`/d/media` → `D:\media`)
- WinFSP FUSE compat already uses `/` internally, so most paths pass through unchanged

```
msys_to_native("/d/media/files")       → "D:\\media\\files"
msys_to_native("//server/share/path")  → "\\\\server\\share\\path"
msys_to_native("/m")                   → "M:\\"
native_to_msys("D:\\media\\files")     → "/d/media/files"
```

---

## Phase 2: FUSE Operation Porting

### Tier 1 — Minimum Viable Mount ✅ COMPLETE

Get a read-only merged view working first:

- [x] `init` / `destroy` — startup/shutdown
- [x] `getattr` — file metadata (`struct stat` mapping)
- [x] `opendir` / `readdir` / `releasedir` — directory listing with merge logic
- [x] `open` / `release` — file open/close
- [x] `read` — file reading
- [x] `statfs` — volume information
- [x] `access` — permission checks

**Milestone: mount two directories as a single drive letter and browse in Explorer. ✅ Achieved 2026-04-09**

### Tier 2 — Read-Write Support ✅ COMPLETE

- [x] `write` — file writing
- [x] `create` — file creation (policy-driven branch selection)
- [x] `mkdir` — directory creation
- [x] `unlink` / `rmdir` — deletion
- [x] `rename` — move/rename across branches
- [x] `truncate` / `ftruncate` — resize files
- [x] `chmod` / `chown` — permissions (best-effort on Windows)
- [x] `utimens` — timestamps
- [x] `flush` / `fsync` / `fsyncdir` — sync to disk
- [x] `symlink` / `readlink` / `link` — links

**Milestone: full read-write filesystem; can create, modify, delete files. ✅ Achieved 2026-04-10**

#### Key Windows adaptations for Tier 2:
- **`open()` with FILE_SHARE_DELETE**: Compat `open()` replaced `_open()` with `CreateFileA` + `_open_osfhandle`, always including `FILE_SHARE_DELETE` in the sharing mode. This enables POSIX-like unlink/rename while files are open.
- **POSIX-semantics unlink**: `fs::unlink` falls back to `SetFileInformationByHandle(FileDispositionInfoEx)` with `FILE_DISPOSITION_FLAG_POSIX_SEMANTICS` when CRT `_unlink` fails. Removes directory entry immediately even with open handles.
- **POSIX-semantics rename**: `fs::rename` falls back to `SetFileInformationByHandle(FileRenameInfoEx)` with `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS`. Handles atomic overwrite and open-handle rename.
- **Unique inodes**: Compat `stat()`/`fstat()` use `GetFileInformationByHandle` to get NTFS file IDs instead of relying on `_stat64.st_ino` (always 0 on Windows). Fixes inode collision in the hybrid-hash inode calculator.

### Tier 3 — Advanced Features ✅ COMPLETE

- [x] `setxattr` / `getxattr` / `listxattr` / `removexattr` — extended attributes (bridge wired)
- [x] `lock` / `flock` — file locking (msvcrt.locking works)
- [x] `fallocate` — preallocation (emulated, stub)
- [x] `ioctl` — mergerfs custom controls (bridge wired)
- [x] `copy_file_range` — emulated as read+write (stub returns ENOSYS, fallback works)
- [x] `mknod` — stub/error on Windows
- [x] `utimens` — timestamps via SetFileTime (lutimes + futimesat implemented)
- [x] `chmod` — read-only toggle via WinFSP chflags callback + FSP_FUSE_CAP_STAT_EX
- [x] `symlink` / `readlink` — implemented (requires Administrator or Developer Mode)
- [x] `link` — implemented but WinFSP FUSE compat does not forward calls (WinFSP limitation)

**Milestone: advanced features functional. ✅ Achieved 2026-04-10**

#### Key Windows adaptations for Tier 3:
- **Timestamps (lutimes/futimesat)**: Implemented using `CreateFileA(FILE_WRITE_ATTRIBUTES)` + `SetFileTime`. Unix epoch converted to Windows FILETIME via 11644473600-second offset. The mergerfs utimens path goes through `lutimes` (not `futimesat`) because it passes `AT_SYMLINK_NOFOLLOW`.
- **chmod via chflags**: WinFSP doesn't forward `SetFileAttributes` to the FUSE `chmod` callback. Instead, it calls `chflags` when `FSP_FUSE_CAP_STAT_EX` is enabled. Bridge translates `FSP_FUSE_UF_READONLY` flag to mode 0444/0644 and calls mergerfs chmod. Getattr sets `st_flags` in `fuse_stat_ex` for the return trip.
- **Hard links**: WinFSP FUSE compat layer marks `link` as unsupported. The callback is never invoked. `CreateHardLinkA` works natively on NTFS but not through WinFSP.
- **Symlinks**: `CreateSymbolicLinkA` with `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE`. Requires Windows 10 1703+ Developer Mode or Administrator privileges. End-to-end tested (see `wintests/SYMLINK_TEST_PLAN.md`):
  - Same-branch file and directory symlinks: fully working (lstat, readlink, content traversal)
  - Cross-branch symlinks (symlink on branch A → file on branch B): fully working through unified namespace
  - Symlink creation through mount: WinFSP returns ACCESS_DENIED to client despite FUSE callback succeeding (known WinFSP FUSE compat layer limitation)
  - `lstat` detects reparse points via `GetFileAttributesA` + `CreateFileA(FILE_FLAG_OPEN_REPARSE_POINT)` and reports `S_IFLNK`
  - `readlink` uses `DeviceIoControl(FSCTL_GET_REPARSE_POINT)` to retrieve symlink target
  - compat `readdir` (dirent.h) checks `FILE_ATTRIBUTE_REPARSE_POINT` before `FILE_ATTRIBUTE_DIRECTORY` to correctly classify directory symlinks as `DT_LNK`

### WinFSP-Specific Extensions

- [ ] `setchgtime` — set change time (Windows has this; Unix doesn't)
- [ ] `setcrtime` — set creation time (Windows first-class; Unix birthtime is spotty)
- [ ] `getpath` — WinFSP path resolution callback

---

## Phase 3: Policy System

The policy engine is the heart of mergerfs and is **largely platform-independent**. Work needed:

### 3.1 Platform-Independent (no changes expected)
- Policy selection logic (`policy_*.cpp`)
- Category mapping (create/search/action → policy)
- Policy algorithms: `all`, `epall`, `mfs`, `lfs`, `epmfs`, `eplfs`, `newest`, `rand`, etc.

### 3.2 Platform-Dependent Adjustments ✅ COMPLETE
- [x] **Branch path parsing**: MSYS-style paths via `msys_path::to_native()` / `msys_path::to_msys()` — upstream `:` delimiter reused unchanged
- [x] **Free space queries**: `statvfs` → `GetDiskFreeSpaceExA` (compat shim in `sys/statvfs.h`); `lstatvfs` uses path-based `statvfs()` on Windows (fstatvfs stub bypassed)
- [x] **Glob patterns in branch specs**: `compat/glob.h` implements glob via `FindFirstFileA`/`FindNextFileA` with `GLOB_ONLYDIR` filtering; tested with `/c/temp/glob_*` pattern
- [x] **minfreespace**: platform-independent via statvfs abstraction — all policies correctly query free space

**Milestone: policy system fully functional on Windows. ✅ Verified 2026-04-13**

---

## Phase 4: Configuration & CLI

### 4.1 Command-Line Interface
```
# Linux:
mergerfs -o branches=/a:/b:/c,policy=mfs /mnt/merged

# Windows (identical syntax thanks to MSYS-style paths):
mergerfs.exe -o branches=/d/a:/e/b:/f/c,policy=mfs /m
```

### 4.2 Config File Support
Add optional config file (`mergerfs.conf` or `mergerfs.ini`) since Windows users don't have `/etc/fstab`:
```ini
[mergerfs]
branches = /d/media:/e/media:/f/media
mount = /m
policy = mfs
minfreespace = 10G
```

### 4.3 Runtime Control
Linux mergerfs uses custom `ioctl` and `xattr` on the mount point for runtime config changes. On Windows:
- Option A: Named pipe (`\\.\pipe\mergerfs-control`)
- Option B: `DeviceIoControl` via WinFSP's ioctl support
- Option C: CLI tool (`mergerfs-ctl.exe`) that communicates via one of the above

---

## Phase 5: Windows Integration

### 5.1 Service Mode
- [ ] Register as a Windows Service via `sc create` or installer
- [ ] Use WinFSP's `FspService` helpers for service lifecycle
- [ ] Auto-start on boot, auto-mount configured branches

### 5.2 Explorer Integration
- [ ] Custom volume label (e.g., "MergerFS (M:)")
- [ ] Volume icon (optional, via autorun.inf equivalent or shell extension)
- [ ] Right-click context menu for branch info (stretch goal)

### 5.3 Installer
- [ ] MSI package or NSIS installer
- [ ] Bundle WinFSP dependency (or check for it)
- [ ] Register service, add to PATH

---

## Phase 6: Testing

### 6.1 Unit Tests
- [ ] Platform abstraction layer tests (stat mapping, path conversion, etc.)
- [ ] Policy engine tests (can likely reuse existing mergerfs tests)

### 6.2 Integration Tests
- [ ] Mount with 2-3 branches, perform CRUD operations
- [ ] Policy correctness (create goes to right branch based on policy)
- [ ] Concurrent access from multiple processes
- [ ] Large files (>4GB)
- [ ] Long paths (>260 chars, `\\?\` prefix)
- [ ] Branch with network path (`//server/share` MSYS-style UNC)
- [ ] Branch hot-add/remove
- [ ] Graceful handling of disconnected branch (USB drive removed)

### 6.3 Compatibility Testing
- [ ] Windows Explorer (copy, paste, drag-drop, properties, search)
- [ ] Common apps (Office, media players, IDEs, games)
- [ ] robocopy, xcopy, PowerShell cmdlets
- [ ] WSL access to the merged mount

---

## Network Volume Testing (Samba via Docker)

A Docker client is available on the Windows dev machine, connected to a remote Linux host. We use this to spin up Samba containers that provide controlled SMB shares for testing mergerfs with network branches.

### Setup

```bash
# Docker connects to a remote Linux host (context: remote)
# Use --network bridge1 (subnet 172.18.0.0/16, routable from Windows)

# 1. Create a Samba container
docker run -d --name mergerfs-smb \
  --network bridge1 \
  -e USERID=1000 -e GROUPID=1000 \
  dperson/samba \
  -u "testuser;testpass" \
  -s "branch1;/share/branch1;yes;no;no;testuser" \
  -s "branch2;/share/branch2;yes;no;no;testuser" \
  -s "branch3;/share/branch3;yes;no;no;testuser"

# 2. Get the container's IP address
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' mergerfs-smb

# 3. Mount from Windows (MSYS2 shell, using the IP from step 2)
net use S: \\<container-ip>\branch1 /user:testuser testpass
net use T: \\<container-ip>\branch2 /user:testuser testpass
net use U: \\<container-ip>\branch3 /user:testuser testpass

# MSYS-style paths for mergerfs config:
# branches=/s:/t:/u  (or //container-ip/branch1://container-ip/branch2://container-ip/branch3)
```

### Test Scenarios

- Mount 2-3 SMB shares as mergerfs branches, verify merged view
- Policy correctness with network branches (free space queries via SMB)
- Read/write operations across network branches
- Concurrent access from multiple processes
- Large file handling over network
- Branch disconnect simulation (stop container, verify graceful degradation)
- Mixed local + network branches (e.g., NTFS local + SMB remote)

### Teardown

```bash
# Remove Windows mounts
net use S: /delete
net use T: /delete
net use U: /delete
# See .claude/skills/running-msys2-bash-commands/ for MSYS2 escaping notes

# Stop and remove container
docker stop mergerfs-smb && docker rm mergerfs-smb
```

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| mergerfs custom ioctls don't map to Windows | Can't change policies at runtime | Named pipe control channel |
| uid/gid irrelevant on Windows | Permission model mismatch | Delegate to underlying FS (CIFS/NTFS ACLs); `chown` no-op; dummy uid/gid in stat |
| Symlinks require Developer Mode or admin | Limited link support | Document requirement; degrade gracefully |
| No `copy_file_range` | Slower file copies within mount | Read+write fallback (transparent to user) |
| `mknod` has no equivalent | Can't create device nodes | Return `ENOTSUP`; not needed for typical use cases |
| NTFS metadata differs from ext4/xfs | Attribute mapping gaps | Best-effort mapping; document differences |
| Large codebase (438 files) | Long porting effort | FUSE compat layer minimizes changes; focus on `fs_*` layer |
| Performance overhead | Slower than native NTFS | Benchmark early; optimize hot paths |

---

## Decisions Made

1. **Path convention**: MSYS-style paths (`/d/path`, `//server/share/path`). Preserves upstream `:` branch delimiter unchanged. Conversion to Windows native at OS boundary only.
2. **Build system**: GNU Make (from MSYS), adapted from upstream Makefile. No CMake.
3. **Toolchain**: MSVC (`cl.exe`/`link.exe`) invoked from MSYS shell. `PATH`, `INCLUDE`, `LIB` assumed pre-configured.
4. **uid/gid handling**: Irrelevant on Windows. Permissions delegated entirely to underlying filesystem (CIFS/NTFS ACLs). `chown` → no-op, `getattr` returns dummy uid/gid.
5. **Naming**: Keep `mergerfs`.
6. **Fork**: Maintained as a fork of the upstream project.
7. **Minimum Windows version**: Windows 10+ (simplifies symlink and long path support).
8. **Branch filesystem support**: Any volume Windows can access — NTFS, ReFS, FAT32, exFAT, network shares (SMB/CIFS), etc. Network support is a requirement.
9. **Preload mechanism**: Not ported. `preload.so` is Linux-specific (`LD_PRELOAD`); no Windows equivalent needed.
10. **Thread model**: Async (multi-threaded) by default. Thread count is configurable; setting it to 1 achieves single-threaded behavior.

---

## Implementation Order Summary

```
Phase 0  ──►  Phase 1  ──►  Phase 2 Tier 1  ──►  Phase 2 Tier 2  ──►  Phase 3
(bootstrap)   (platform)    (read-only mount)     (read-write)         (policies)
                                  │
                                  ▼
                            First working demo
                                  │
                    ┌─────────────┼─────────────┐
                    ▼             ▼             ▼
               Phase 4       Phase 5       Phase 2 Tier 3
              (config/CLI)  (Win integration) (advanced ops)
                    │             │             │
                    └─────────────┼─────────────┘
                                  ▼
                              Phase 6
                             (testing)
```
