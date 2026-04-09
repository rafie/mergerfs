# mergerfs Feature Inventory

Comprehensive catalog of mergerfs features, mapped to source files, test coverage, and Windows porting notes.

**Windows column legend:**
- **OK** — expected to work via WinFSP FUSE compat without changes
- **VERIFY** — has a Windows equivalent but semantics may differ; needs testing
- **EMULATE** — no direct equivalent; must be emulated or reimplemented
- **STUB** — no equivalent and no sensible emulation; return error code
- **N/A** — not applicable on Windows; disable or remove
- **PORTABLE** — platform-independent logic; no OS interaction

---

## 1. FUSE Operations

### 1.1 File Operations

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `open` | `fuse_open.cpp` | Open file; selects branch via search policy | — | VERIFY — flag mapping (`O_RDONLY`→`GENERIC_READ`, etc.) handled by WinFSP; verify `O_NOATIME`, `O_NOFOLLOW` behavior |
| `create` | `fuse_create.cpp` | Create and open file; selects branch via create policy | — | VERIFY — file creation flags and mode bits; WinFSP maps POSIX mode to Windows security |
| `release` | `fuse_release.cpp` | Close file handle | — | OK |
| `read` | `fuse_read.cpp` | Read file data; supports nullrw mode (discard) | `TEST_o_direct` | VERIFY — `pread` semantics; verify positional reads via WinFSP |
| `write` | `fuse_write.cpp` | Write file data; supports nullrw mode, moveonenospc | `TEST_o_direct` | VERIFY — `pwrite` semantics; positional writes via WinFSP |
| `truncate` | `fuse_truncate.cpp` | Truncate file by path | — | VERIFY — uses `truncate()` syscall; Windows equivalent is open + `SetFileInformationByHandle(FileEndOfFileInfo)` |
| `ftruncate` | `fuse_ftruncate.cpp` | Truncate file by descriptor | `TEST_use_ftruncate_after_unlink` | VERIFY — `ftruncate()` on fd; test relies on fd-after-unlink (see unlink) |
| `fallocate` | `fuse_fallocate.cpp` | Pre-allocate disk space | `TEST_use_fallocate_after_unlink` | EMULATE — no `posix_fallocate` on Windows; `SetFileInformationByHandle(FileAllocationInfo)` reserves space but does not zero-fill; `posix_fallocate` in test won't exist on Windows |
| `unlink` | `fuse_unlink.cpp` | Remove file | `TEST_unlink_rename`, `TEST_no_fuse_hidden` | VERIFY — POSIX allows unlinking open files (fd remains valid). Windows traditionally forbids deleting open files. WinFSP can emulate POSIX semantics via `FILE_DISPOSITION_POSIX_SEMANTICS` (Win10+). **All 7 `TEST_*_after_unlink` tests depend on this.** |
| `rename` | `fuse_rename.cpp` | Rename/move file; handles cross-device via rename-exdev policy | `TEST_unlink_rename` | VERIFY — POSIX `rename()` atomically replaces target; `MoveFileEx(MOVEFILE_REPLACE_EXISTING)` is close but cannot replace open files or directories. Cross-volume rename may return different error codes. |
| `link` | `fuse_link.cpp` | Create hard link; handles cross-device via link-exdev policy | — | VERIFY — NTFS supports hard links (`CreateHardLink`), max 1024 per file. FAT/exFAT and some network shares do not support hard links at all. |
| `symlink` | `fuse_symlink.cpp` | Create symbolic link | — | VERIFY — Windows symlinks are reparse points; require Developer Mode or admin on Win10+. Windows distinguishes file vs directory symlinks (Linux does not). WinFSP FUSE compat may handle this distinction. |
| `readlink` | `fuse_readlink.cpp` | Read symbolic link target | — | VERIFY — must parse reparse point data on the underlying branch; WinFSP FUSE compat should handle translation |
| `mknod` | `fuse_mknod.cpp` | Create special file (device node, named pipe, etc.) | — | STUB — device nodes do not exist on Windows. Named pipes exist but use a different API (`CreateNamedPipe`). Return `ENOTSUP`. |
| `tmpfile` | `fuse_tmpfile.cpp` | Create unnamed temporary file (O_TMPFILE) | — | EMULATE — `O_TMPFILE` is Linux 3.11+. Emulate with `CreateFile` + `FILE_FLAG_DELETE_ON_CLOSE`, or create temp file + unlink (if fd-after-unlink works). |
| `copy_file_range` | `fuse_copy_file_range.cpp` | Copy data between file descriptors in-kernel | — | EMULATE — no kernel equivalent on Windows. Implement as userspace read+write loop. |

### 1.2 Directory Operations

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `mkdir` | `fuse_mkdir.cpp` | Create directory; selects branch via create policy | — | OK — `CreateDirectory` is straightforward |
| `rmdir` | `fuse_rmdir.cpp` | Remove directory | — | VERIFY — Windows requires directory to be empty and not in use; same as POSIX, but open handles on child files can prevent removal |
| `opendir` | `fuse_opendir.cpp` | Open directory for listing | — | OK |
| `readdir` | `fuse_readdir.cpp` | Read directory entries (merged across branches) | — | VERIFY — see Section 4 (Readdir Implementations) for `getdents` vs `readdir` issue |
| `releasedir` | `fuse_releasedir.cpp` | Close directory handle | — | OK |

### 1.3 Metadata Operations

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `getattr` | `fuse_getattr.cpp` | Get file attributes (stat) | — | VERIFY — `struct stat` field mapping: `st_uid`/`st_gid` → dummy values; `st_mode` → WinFSP emulated permissions; `st_nlink` → NTFS link count; `st_ino` → see Section 7 (Inode Calculation); `st_blocks` → not meaningful on Windows |
| `fgetattr` | `fuse_fgetattr.cpp` | Get attributes from open file descriptor | `TEST_use_fstat_after_unlink` | VERIFY — same stat mapping as `getattr`; test depends on fd-after-unlink |
| `statx` | `fuse_statx.cpp` | Extended file attributes (newer stat interface) | — | N/A — `statx` is Linux 4.11+. Has compile-time guards (`fuse_statx_supported.icpp` / `fuse_statx_unsupported.icpp`). Use unsupported variant on Windows. |
| `chmod` | `fuse_chmod.cpp` | Change file permissions by path | — | VERIFY — no `mode_t` on Windows. WinFSP emulates POSIX permission bits but underlying FS uses ACLs. On CIFS branches, `chmod` may be a no-op or partially effective. |
| `fchmod` | `fuse_fchmod.cpp` | Change permissions on open descriptor | `TEST_use_fchmod_after_unlink` | VERIFY — same as `chmod`; test depends on fd-after-unlink |
| `chown` | `fuse_chown.cpp` | Change file owner/group by path | — | N/A — no-op on Windows. uid/gid are irrelevant; permissions handled by underlying FS (CIFS/NTFS ACLs). Return success. |
| `fchown` | `fuse_fchown.cpp` | Change owner/group on open descriptor | `TEST_use_fchown_after_unlink` | N/A — no-op; test depends on fd-after-unlink (the unlink part is the real test) |
| `utimens` | `fuse_utimens.cpp` | Set access/modification times by path | — | VERIFY — `timespec` → `FILETIME` conversion needed. Windows also has creation time (not set by `utimens`). Nanosecond precision: FILETIME has 100ns granularity (vs timespec 1ns). |
| `futimens` | `fuse_futimens.cpp` | Set times on open descriptor | `TEST_use_futimens_after_unlink` | VERIFY — same as `utimens`; test depends on fd-after-unlink |
| `access` | `fuse_access.cpp` | Check file access permissions | — | VERIFY — delegates to underlying FS via `eaccess()`/`faccessat()`. Windows equivalent is `_access()` or `AccessCheck` with token. WinFSP FUSE compat may handle this. |
| `statfs` | `fuse_statfs.cpp` | Get filesystem statistics (aggregated across branches) | — | VERIFY — `statvfs` fields map to `GetDiskFreeSpaceEx`. Fields `f_files`/`f_ffree` (inode counts) have no Windows equivalent — return dummy values. See Section 11 (StatFS Modes). |

### 1.4 Extended Attributes

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `getxattr` | `fuse_getxattr.cpp` | Get extended attribute; doubles as control interface | — | VERIFY — WinFSP FUSE compat supports xattr callbacks. The **control interface** (`user.mergerfs.*`) is handled entirely in mergerfs code (no OS xattr call), so it should work. **Passthrough** xattr on branch files depends on WinFSP's xattr emulation (NTFS ADS or internal). See Section 8. |
| `setxattr` | `fuse_setxattr.cpp` | Set extended attribute; doubles as control interface | — | VERIFY — same as `getxattr` |
| `listxattr` | `fuse_listxattr.cpp` | List extended attribute names | — | VERIFY — depends on WinFSP xattr emulation for passthrough mode |
| `removexattr` | `fuse_removexattr.cpp` | Remove extended attribute | — | VERIFY — depends on WinFSP xattr emulation for passthrough mode |

### 1.5 Locking

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `lock` | `fuse_lock.cpp` | POSIX record locking (fcntl) | — | VERIFY — POSIX locks are advisory; Windows locks (`LockFileEx`) are mandatory. WinFSP FUSE compat translates between the two, but semantic difference may cause issues with concurrent access patterns. |
| `flock` | `fuse_flock.cpp` | Advisory file locking (BSD) | — | VERIFY — same advisory vs mandatory concern. WinFSP maps `flock` to Windows locking. |

### 1.6 Sync & Flush

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `flush` | `fuse_flush.cpp` | Flush buffered data on close | — | OK — `FlushFileBuffers` equivalent; WinFSP handles |
| `fsync` | `fuse_fsync.cpp` | Synchronize file data to disk | — | OK — maps to `FlushFileBuffers` |
| `fsyncdir` | `fuse_fsyncdir.cpp` | Synchronize directory to disk | — | VERIFY — directory sync is a no-op on some Windows FS; verify WinFSP behavior |
| `syncfs` | `fuse_syncfs.cpp` | Synchronize entire filesystem | — | EMULATE — Linux-specific. Could iterate open handles and `FlushFileBuffers`, or stub as no-op. |

### 1.7 Other

| Operation | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `init` | `fuse_init.cpp` | Filesystem initialization | — | VERIFY — init negotiates FUSE capabilities with the kernel. WinFSP supports a subset. Capability flags need auditing (e.g., `FUSE_CAP_ASYNC_READ`, `FUSE_CAP_POSIX_LOCKS`, `FUSE_CAP_WRITEBACK_CACHE`). |
| `destroy` | `fuse_destroy.cpp` | Cleanup on unmount | — | OK |
| `ioctl` | `fuse_ioctl.cpp` | I/O control (FS_IOC_GETFLAGS/SETFLAGS/GETVERSION/SETVERSION; blocks BTRFS ioctls) | — | STUB — `FS_IOC_*` are Linux ext2/3/4 ioctls with no Windows equivalent. Return `ENOTTY` for all. Windows file attributes (hidden, system, readonly, archive) could be exposed via a Windows-specific mechanism later. |
| `bmap` | `fuse_bmap.cpp` | Block mapping | — | STUB — returns `ENOSYS`; no Windows equivalent |
| `poll` | `fuse_poll.cpp` | Poll for file events | — | VERIFY — WinFSP may or may not support FUSE poll; likely no-op |
| `setupmapping` | `fuse_setupmapping.cpp` | DAX/virtiofs memory mapping setup | — | N/A — virtiofs-specific; stub with `ENOSYS` |
| `removemapping` | `fuse_removemapping.cpp` | DAX/virtiofs memory mapping removal | — | N/A — virtiofs-specific; stub with `ENOSYS` |

---

## 2. Policies

Policies determine which branch(es) to use for filesystem operations. They are assigned to three categories:

- **create** — file/directory creation (`create`, `mkdir`, `mknod`, `symlink`)
- **search** — lookups and reads (`access`, `getattr`, `getxattr`, `listxattr`, `open`, `readlink`)
- **action** — modifications (`chmod`, `chown`, `link`, `removexattr`, `rename`, `rmdir`, `setxattr`, `truncate`, `unlink`, `utimens`)

Each function can also have its policy overridden individually via `func.<name>` config options.

### 2.1 Policy List

| Policy | Source | Description | Windows |
|---|---|---|---|
| `all` | `policy_all.cpp` | Apply to all branches where the file exists | PORTABLE |
| `epall` | `policy_epall.cpp` | All branches, error-precedence (returns first non-error) | PORTABLE |
| `epff` | `policy_epff.cpp` | First found, error-precedence | PORTABLE |
| `eplfs` | `policy_eplfs.cpp` | Least free space, error-precedence | PORTABLE — depends on `statvfs` abstraction |
| `eplus` | `policy_eplus.cpp` | Last used (search), error-precedence | PORTABLE |
| `epmfs` | `policy_epmfs.cpp` | Most free space, error-precedence | PORTABLE — depends on `statvfs` abstraction |
| `eppfrd` | `policy_eppfrd.cpp` | Percentage-based from random distribution, error-precedence | PORTABLE |
| `eprand` | `policy_eprand.cpp` | Random branch, error-precedence | PORTABLE |
| `erofs` | `policy_erofs.cpp` | Return EROFS (read-only filesystem error) | PORTABLE |
| `ff` | `policy_ff.cpp` | First found (first branch in order) | PORTABLE |
| `lfs` | `policy_lfs.cpp` | Least free space | PORTABLE — depends on `statvfs` abstraction |
| `lup` | `policy_lup.cpp` | Last used path | PORTABLE |
| `lus` | `policy_lus.cpp` | Last used (search) | PORTABLE |
| `mfs` | `policy_mfs.cpp` | Most free space | PORTABLE — depends on `statvfs` abstraction |
| `msplfs` | `policy_msplfs.cpp` | Most shared path, least free space | PORTABLE — depends on `statvfs` abstraction |
| `msplus` | `policy_msplus.cpp` | Most shared path, last used | PORTABLE |
| `mspmfs` | `policy_mspmfs.cpp` | Most shared path, most free space | PORTABLE — depends on `statvfs` abstraction |
| `msppfrd` | `policy_msppfrd.cpp` | Most shared path, percentage from random distribution | PORTABLE |
| `newest` | `policy_newest.cpp` | Most recently modified | PORTABLE — depends on `stat` abstraction |
| `pfrd` | `policy_pfrd.cpp` | Percentage-based from random distribution | PORTABLE |
| `rand` | `policy_rand.cpp` | Random branch | PORTABLE |

**Tests:** None. Policies have no dedicated unit or integration tests.

Space-aware policies (`lfs`, `mfs`, `eplfs`, `epmfs`, `msplfs`, `mspmfs`) depend on `statvfs` → `GetDiskFreeSpaceEx` mapping working correctly for all branch types (NTFS, ReFS, network shares). Verify free space reporting on network shares in particular.

---

## 3. Branch Management

| Feature | Source | Description | Tests | Windows |
|---|---|---|---|---|
| Branch specification | `branch.cpp`, `branches.cpp` | Parse branch paths with optional mode and minfreespace (e.g., `/path=RW,1234`) | `test_config_branches` (unit) | PORTABLE — uses MSYS-style paths; no change needed |
| Branch modes | `branch.hpp` | `RW` (read-write), `RO` (read-only), `NC` (no-create) | `test_config_branches` (unit) | PORTABLE |
| Per-branch minfreespace | `branch.cpp` | Each branch can have its own minimum free space threshold | `test_config_branches` (unit) | PORTABLE — depends on `statvfs` abstraction |
| Colon-separated branch list | `branch.cpp` | Multiple branches joined with `:` delimiter | `test_config_branches` (unit) | PORTABLE — MSYS-style paths avoid colons in paths |
| Glob patterns in branch specs | `branches.cpp` | Branch paths can contain globs (e.g., `/mnt/disk*`) | — | EMULATE — uses POSIX `glob()`. On Windows, implement via `FindFirstFile`/`FindNextFile` after `msys_to_native()` conversion. |
| Runtime branch add/remove | `fuse_setxattr.cpp` | Modify branches at runtime via xattr control interface | — | VERIFY — depends on xattr control interface working (Section 8) |

---

## 4. Readdir Implementations

Multiple readdir strategies, selected via `func.readdir`:

| Mode | Source | Description | Tests | Windows |
|---|---|---|---|---|
| `seq` | `fuse_readdir_seq.cpp` | Sequential: reads branches one at a time | — | VERIFY — two compile-time variants: `getdents` (Linux syscall) and `readdir` (POSIX). **Must use `readdir` variant on Windows**, or replace with `FindFirstFile`/`FindNextFile`. |
| `cor` | `fuse_readdir_cor.cpp` | Concurrent out-of-order: reads branches in parallel via thread pool | — | VERIFY — same `getdents` vs `readdir` issue; thread pool must use Windows threading (pthreads-win32 or C++ `std::thread`) |
| `cosr` | `fuse_readdir_cosr.cpp` | Concurrent ordered sequential results: parallel reads, ordered output | — | VERIFY — same as `cor` |
| `plus` | `fuse_readdir_plus.cpp` | Readdir with attributes (returns ENOTSUP currently) | — | N/A — already returns ENOTSUP on Linux too |

Each concurrent mode supports tuning: `cor:N:M` where N = thread count, M = queue depth multiplier.

Two internal implementations per mode: one using `getdents` (Linux syscall), one using `readdir` (POSIX). Selected at compile time via `*.icpp` includes. **On Windows, only the `readdir`-based (or a new `FindFirstFile`-based) variant can be used.**

---

## 5. Caching

| Feature | Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|---|
| Attribute cache timeout | `cache.attr` | seconds (int) | `config.hpp` | — | OK — passed to WinFSP/FUSE layer |
| Entry cache timeout | `cache.entry` | seconds (int) | `config.hpp` | — | OK — passed to WinFSP/FUSE layer |
| Negative entry cache timeout | `cache.negative-entry` | seconds (int) | `config.hpp` | — | VERIFY — check if WinFSP supports negative entry caching |
| File content caching | `cache.files` | `off`, `partial`, `full`, `auto-full`, `per-process` | `config_cachefiles.cpp` | `test_config_cachefiles` (unit) | VERIFY — `per-process` mode uses `/proc/<pid>/comm` to identify process names; needs Windows equivalent (`QueryFullProcessImageName` or `GetProcessImageFileName`) |
| Per-process cache filter | `cache.files.process-names` | regex patterns | `config.hpp` | — | VERIFY — same `/proc` dependency as above |
| Readdir caching | `cache.readdir` | boolean | `config.hpp` | — | OK — internal mergerfs logic |
| Symlink caching | `cache.symlinks` | boolean | `config.hpp` | — | OK — passed to FUSE layer |
| Statfs cache timeout | `cache.statfs` | seconds (int) | `config.hpp` | — | OK — internal mergerfs logic |
| Writeback caching | `cache.writeback` | boolean (mount-time only) | `config.hpp` | — | VERIFY — requires `FUSE_CAP_WRITEBACK_CACHE`; check WinFSP support |
| Drop cache on close | `dropcacheonclose` | boolean | `config.hpp` | — | VERIFY — uses `posix_fadvise(POSIX_FADV_DONTNEED)` or similar; no direct Windows equivalent. May need `FlushFileBuffers` + purge, or stub as no-op. |

---

## 6. Special Features

### 6.1 Move on ENOSPC

When a write fails with ENOSPC (no space left), automatically moves the file to another branch with available space and retries.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `moveonenospc` | `false`, or policy name (`mfs`, `mspmfs`, `pfrd`, etc.); `true` maps to `pfrd` | `config_moveonenospc.cpp` | `test_config_moveonenospc` (unit) | VERIFY — Windows returns `ERROR_DISK_FULL` (mapped to ENOSPC by WinFSP). Moving an open file across volumes requires copy+delete rather than rename. Verify error code mapping and cross-volume move behavior. |

### 6.2 Cross-Device Link Handling

When `link()` returns EXDEV (cross-device), create a symlink instead.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `link-exdev` | `passthrough`, `rel-symlink`, `abs-base-symlink`, `abs-pool-symlink` | `config_link_exdev.cpp` | — | VERIFY — hard links are always same-volume on NTFS (will always get EXDEV for cross-volume). Symlink fallback requires Developer Mode. Relative symlinks: verify Windows resolves relative reparse point targets correctly. |

### 6.3 Cross-Device Rename Handling

When `rename()` returns EXDEV, create a symlink instead.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `rename-exdev` | `passthrough`, `rel-symlink`, `abs-symlink` | `config_rename_exdev.cpp` | — | VERIFY — `MoveFileEx` across volumes already does copy+delete internally. EXDEV may not be returned by Windows in the same way. Verify error code from WinFSP for cross-volume rename. |

### 6.4 NFS Open Hack

Workaround for NFS clients that open files with O_WRONLY|O_RDWR before checking existence.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `nfsopenhack` | `off`, `git`, `all` | `config_nfsopenhack.cpp` | `test_config_nfsopenhack` (unit) | N/A — NFS client behavior is Linux-specific. Config parsing works; feature is a no-op on Windows. Keep for config compatibility. |

### 6.5 Follow Symlinks

Control whether mergerfs follows symlinks when resolving paths on branches.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `follow-symlinks` | `never`, `directory`, `regular`, `all` | `config_follow_symlinks.cpp` | — | VERIFY — symlink resolution on branches uses `lstat`/`stat` distinction. On Windows, use `FILE_FLAG_OPEN_REPARSE_POINT` to not follow. Windows distinguishes file vs directory symlinks, which may affect the `directory`/`regular` modes. |

### 6.6 Symlinkify

Convert old, unchanged files to symbolic links pointing to the underlying branch path. Saves stat overhead for rarely-changed files.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `symlinkify` | boolean | `config.hpp` | — | VERIFY — creates symlinks programmatically, so requires Developer Mode or admin. File vs directory symlink distinction on Windows may need handling. Converted symlink paths must be in Windows native format (not MSYS-style) since they'll be resolved by the OS. |
| `symlinkify-timeout` | seconds (default 3600) | `config.hpp` | — | PORTABLE — pure timer logic |

### 6.7 Flush on Close

Control when flush is called on file close.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `flush-on-close` | `never`, `opened-for-write`, `always` | `config_flushonclose.cpp` | — | OK — internal mergerfs logic controlling when `FlushFileBuffers` is called |

### 6.8 Passthrough I/O

Bypass FUSE for reads/writes, passing I/O directly to the underlying filesystem (requires kernel support).

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `passthrough.io` | `off`, `ro`, `wo`, `rw` | `config_passthrough_io.cpp` | — | N/A — requires Linux kernel FUSE passthrough support (`FUSE_CAP_PASSTHROUGH`). WinFSP has no equivalent. Config parses but feature is disabled on Windows. |
| `passthrough.max-stack-depth` | integer | `config.hpp` | — | N/A |

### 6.9 Null Read/Write

Discard all writes and return zeros on reads. For testing/benchmarking.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `nullrw` | boolean | `config.hpp` | — | PORTABLE — pure logic; no OS interaction |

### 6.10 Link Copy-on-Write

When a hard-linked file is opened for writing, break the link and create a copy.

| Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|
| `link-cow` | boolean | `config.hpp` | — | VERIFY — depends on detecting hard link count (`st_nlink` from `GetFileInformationByHandle`). Breaking the link requires creating a copy and replacing. NTFS reports `nNumberOfLinks` correctly; verify on network shares. |

---

## 7. Inode Calculation

mergerfs must present unified inode numbers across branches. Multiple strategies available.

| Mode | Description | Source | Tests | Windows |
|---|---|---|---|---|
| `passthrough` | Use underlying inode directly (may collide across branches) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | VERIFY — Windows uses 64-bit FileID (`BY_HANDLE_FILE_INFORMATION.nFileIndex{High,Low}`). NTFS provides stable FileIDs; FAT does not. Network shares may or may not provide FileIDs. |
| `path-hash` | Hash the relative path (64-bit) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | PORTABLE — pure hash computation |
| `devino-hash` | Hash device + inode number (64-bit) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | VERIFY — `st_dev` (device number) has no direct Windows equivalent. Could use volume serial number (`GetVolumeInformation`) as substitute. |
| `hybrid-hash` | Combination of path and devino hashing (64-bit) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | VERIFY — same `st_dev` issue as `devino-hash` |
| `path-hash32` | Hash the relative path (32-bit) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | PORTABLE |
| `devino-hash32` | Hash device + inode (32-bit) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | VERIFY — same `st_dev` issue |
| `hybrid-hash32` | Combination (32-bit) | `config_inodecalc.cpp` | `test_config_inodecalc` (unit) | VERIFY — same `st_dev` issue |

**Windows note:** `st_dev` can be mapped to volume serial number (`GetVolumeInformation` → `lpVolumeSerialNumber`). `st_ino` can be mapped to FileID (`nFileIndexHigh`/`nFileIndexLow`). This mapping must be implemented in the `stat` abstraction layer.

---

## 8. Extended Attribute Control Interface

mergerfs exposes a virtual control file (`.mergerfs` at mount root) and per-file xattrs for runtime configuration and introspection.

### 8.1 Control File (`.mergerfs`)

Reading/writing xattrs on the `.mergerfs` control file gets/sets configuration:

| xattr | Direction | Description | Source | Windows |
|---|---|---|---|---|
| `user.mergerfs.<option>` | get/set | Read or change any config option at runtime | `fuse_getxattr.cpp`, `fuse_setxattr.cpp` | VERIFY — handled entirely in mergerfs code (no OS xattr call); works if WinFSP delivers xattr callbacks to the FUSE layer. This is the primary runtime control mechanism — **critical to verify**. |
| `user.mergerfs.cmd.gc` | set | Trigger garbage collection | `fuse_setxattr.cpp` | VERIFY — same as above |
| `user.mergerfs.cmd.gc1` | set | Trigger garbage collection (type 1) | `fuse_setxattr.cpp` | VERIFY — same |
| `user.mergerfs.cmd.invalidate-all-nodes` | set | Invalidate all cached FUSE nodes | `fuse_setxattr.cpp` | VERIFY — same; also verify that WinFSP supports node invalidation |

### 8.2 Per-File xattrs

Reading xattrs on any file in the merged view:

| xattr | Description | Source | Windows |
|---|---|---|---|
| `user.mergerfs.basepath` | Branch path where this file resides | `fuse_getxattr.cpp` | VERIFY — returns MSYS-style branch path; handled in mergerfs code |
| `user.mergerfs.relpath` | Relative path within the branch | `fuse_getxattr.cpp` | VERIFY — same |
| `user.mergerfs.fullpath` | Full path on the underlying filesystem | `fuse_getxattr.cpp` | VERIFY — same |
| `user.mergerfs.allpaths` | NUL-separated list of all branches containing this file | `fuse_getxattr.cpp` | VERIFY — same |

**Windows note:** The control interface depends on WinFSP delivering xattr get/set callbacks to the FUSE filesystem. WinFSP supports this through its FUSE compatibility layer. However, Windows apps typically access xattrs via NTFS Alternate Data Streams, not POSIX xattr APIs. A Windows CLI tool (`mergerfs-ctl.exe`) or named pipe interface may be needed as an alternative for users who can't easily call xattr functions.

**Tests:** None.

---

## 9. ioctl Support

| ioctl | Description | Source | Tests | Windows |
|---|---|---|---|---|
| `FS_IOC_GETFLAGS` | Get file flags (ext2/3/4 attributes like immutable, append-only) | `fuse_ioctl.cpp` | — | STUB — Linux ext2/3/4 specific. No equivalent. Return `ENOTTY`. |
| `FS_IOC_SETFLAGS` | Set file flags | `fuse_ioctl.cpp` | — | STUB — same |
| `FS_IOC_GETVERSION` | Get file version/generation number | `fuse_ioctl.cpp` | — | STUB — same |
| `FS_IOC_SETVERSION` | Set file version/generation number | `fuse_ioctl.cpp` | — | STUB — same |
| BTRFS ioctls | Blocked — returns ENOTTY | `fuse_ioctl.cpp` | — | STUB — already blocked on Linux too |

**Windows note:** WinFSP supports the `ioctl` FUSE callback, but the specific Linux ioctl command codes are meaningless on Windows. All current ioctls should return `ENOTTY`. Future Windows-specific file attribute control (hidden, system, readonly, archive) could be added via new ioctl codes or a separate mechanism.

---

## 10. Extended Attribute Handling Modes

| Mode | Description | Source | Tests | Windows |
|---|---|---|---|---|
| `passthrough` | Forward xattr calls to underlying filesystem | `config_xattr.cpp` | `test_config_xattr` (unit) | VERIFY — depends on underlying FS xattr support. NTFS: WinFSP maps to Alternate Data Streams. Network shares: xattr support varies. FAT/exFAT: no xattr support (will fail). |
| `noattr` | Return ENOATTR for all xattr calls | `config_xattr.cpp` | `test_config_xattr` (unit) | PORTABLE — pure logic |
| `nosys` | Return ENOSYS for all xattr calls | `config_xattr.cpp` | `test_config_xattr` (unit) | PORTABLE — pure logic |

**Windows note:** `noattr` or `nosys` are safe defaults on Windows if xattr passthrough proves problematic on certain branch FS types. The mergerfs control interface (`user.mergerfs.*`) bypasses the xattr mode setting and always works.

---

## 11. StatFS Modes

| Feature | Config Key | Values | Source | Tests | Windows |
|---|---|---|---|---|---|
| StatFS mode | `statfs` | `base` (first branch only), `full` (aggregate all branches) | `config_statfs.cpp` | `test_config_statfs` (unit) | VERIFY — `statvfs` → `GetDiskFreeSpaceEx` mapping. Available fields: total bytes, free bytes, free bytes for caller. Missing fields: `f_files`, `f_ffree`, `f_favail` (inode counts) — return `UINT64_MAX` or 0. `f_bsize`/`f_frsize` — use cluster size from `GetDiskFreeSpace`. |
| StatFS ignore | `statfs-ignore` | `none`, `ro` (ignore read-only branches), `nc` (ignore no-create branches) | `config_statfsignore.cpp` | `test_config_statfsignore` (unit) | PORTABLE — pure filtering logic |

---

## 12. Security & Permissions

| Feature | Config Key | Source | Tests | Windows |
|---|---|---|---|---|
| POSIX ACL support | `posix-acl` | `config.hpp` | — | N/A — POSIX ACLs don't exist on Windows. WinFSP has its own security descriptor mapping. Disable this option. |
| Security capabilities | `security-capability` | `config.hpp` | — | N/A — Linux capabilities model. Disable. The `security.capability` xattr check in `fuse_getxattr.cpp` can return ENOATTR unconditionally. |
| Handle killpriv | `handle-killpriv` | `config.hpp` | — | N/A — Linux-specific (clear setuid/setgid on write). Disable. |
| Handle killpriv v2 | `handle-killpriv-v2` | `config.hpp` | — | N/A — same |
| Kernel permission checks | `kernel-permissions-check` | `config.hpp` | — | VERIFY — WinFSP has its own permission checking model. Check how this interacts with the FUSE compat layer's `default_permissions` option. |
| NFS export support | `export-support` | `config.hpp` | — | N/A — Linux NFS server specific. Disable. |

---

## 13. I/O & Threading

| Feature | Config Key | Description | Source | Tests | Windows |
|---|---|---|---|---|---|
| Async read | `async-read` | Enable async reads | `config.hpp` | `test_config` (unit, partial) | VERIFY — maps to `FUSE_CAP_ASYNC_READ`; check WinFSP support |
| Direct I/O allow mmap | `direct-io-allow-mmap` | Allow mmap when using direct I/O | `config.hpp` | — | VERIFY — mmap on Windows uses `CreateFileMapping`/`MapViewOfFile`. Direct I/O + mmap interaction may differ. WinFSP FUSE compat may not support `FUSE_CAP_DIRECT_IO_ALLOW_MMAP`. |
| Parallel direct writes | `parallel-direct-writes` | Allow concurrent direct writes | `config.hpp` | — | VERIFY — maps to `FUSE_CAP_PARALLEL_DIROPS`; check WinFSP support |
| Readahead | `readahead` | Readahead size in bytes | `config.hpp` | — | VERIFY — Linux kernel readahead; may not be configurable via WinFSP |
| FUSE message size | `fuse-msg-size` | FUSE kernel message buffer size | `config.hpp` | — | VERIFY — WinFSP may have its own buffer sizing |
| Thread count | `threads` | Number of FUSE worker threads | `config.hpp` | — | OK — WinFSP supports configurable thread count |
| Process thread count | `process-thread-count` | Processing threads | `config.hpp` | — | VERIFY — internal thread pool; uses pthreads. Must use `std::thread` or Windows threads on MSVC. |
| Process thread queue depth | `process-thread-queue-depth` | Processing queue depth | `config.hpp` | — | PORTABLE — queue logic |
| Read thread count | `read-thread-count` | Dedicated read threads | `config.hpp` | — | VERIFY — same threading concern as process-thread-count |
| Pin threads | `pin-threads` | Pin worker threads to CPU cores | `config.hpp` | — | EMULATE — Linux uses `sched_setaffinity`/`pthread_setaffinity_np`; Windows equivalent is `SetThreadAffinityMask`. |
| Scheduling priority | `scheduling-priority` | Process scheduling priority | `config.hpp` | — | EMULATE — Linux uses `setpriority`/`nice`; Windows uses `SetPriorityClass`. Different priority models. |
| Proxy I/O priority | `proxy-ioprio` | Inherit I/O priority from requesting process | `config_proxy_ioprio.cpp` | — | EMULATE — Linux uses `ioprio_get`/`ioprio_set` syscalls. Windows has I/O priority via `SetThreadPriority` with `THREAD_MODE_BACKGROUND_BEGIN`, but cannot query another process's I/O priority the same way. May need to stub. |

---

## 14. Logging & Debug

| Feature | Config Key | Source | Tests | Windows |
|---|---|---|---|---|
| Debug mode | `debug` | `config_debug.cpp` | — | VERIFY — debug output goes to `syslog` on Linux. On Windows, redirect to `OutputDebugString` (viewable in debugger/DebugView) or stderr. |
| Log file | `log.file` | `config_log_file.cpp` | — | OK — file logging is portable; just uses file I/O |

---

## 15. Configuration System

| Feature | Source | Description | Tests | Windows |
|---|---|---|---|---|
| Config types (bool, int, uint64, str) | `config.hpp`, `config_set.cpp` | Type-safe config value parsing | `test_config_bool`, `test_config_uint64`, `test_config_int`, `test_config_str` (unit) | PORTABLE |
| Config file loading | `config.cpp` | Hierarchical config file (up to 5 levels, prevents circular includes) | — | PORTABLE — uses standard file I/O |
| Runtime config get/set | `config_set.cpp` | Dynamic get/set via string keys | `test_config` (unit) | PORTABLE |
| String utilities | `str.hpp` | Split, join, parse helpers used by config | `test_str_stuff` (unit) | PORTABLE |

---

## 16. Miscellaneous

| Feature | Config Key | Description | Source | Tests | Windows |
|---|---|---|---|---|---|
| Filesystem name | `fsname` | Custom name reported by statfs | `config.hpp` | — | OK — passed to WinFSP |
| Ignore path-preserving on rename | `ignorepponrename` | Skip path-preserving policy on rename | `config.hpp` | — | PORTABLE |
| Lazy unmount | `lazy-umount-mountpoint` | Lazy unmount behavior | `config.hpp` | — | N/A — Linux `MNT_DETACH` specific. On Windows, use WinFSP's `FspFileSystemStopDispatcher` or `FspServiceStop`. |
| Never forget nodes | `never-forget-nodes` | Don't release FUSE node references | `config.hpp` | — | VERIFY — depends on WinFSP's node management |
| No forget | `noforget` | Node forget policy | `config_noforget.hpp` | — | VERIFY — same |
| PID | `pid` | Process ID (read-only) | `config_pid.hpp` | — | OK — `GetCurrentProcessId()` on Windows |
| Page size | `pagesize` | System page size (read-only) | `config_pagesize.cpp` | — | VERIFY — Linux uses `sysconf(_SC_PAGESIZE)`. Windows uses `GetSystemInfo` → `dwPageSize`. |
| Branch mount timeout | `branches-mount-timeout` | Wait for branches to mount | `config.hpp` | — | VERIFY — Linux checks `/proc/self/mountinfo` for branch mounts. On Windows, check volume availability via `GetVolumePathName` or `GetDriveType`. Different detection mechanism needed. |

---

## 17. Companion Tools

| Tool | Description | Source | Windows |
|---|---|---|---|
| `fsck.mergerfs` | Filesystem check — symlink to main binary, dispatched by name | `mergerfs.cpp` | VERIFY — dispatch by executable name works on Windows. Symlink-as-binary: on Windows, use a copy or a shim `.exe` instead of symlink. |
| `mergerfs.collect-info` | Diagnostic info collector — symlink to main binary | `mergerfs.cpp` | VERIFY — same as above. Info collection likely reads `/proc` and Linux-specific diagnostics — needs Windows equivalents. |
| `preload.so` | LD_PRELOAD library for optimizing access patterns | `Makefile` | N/A — Linux `LD_PRELOAD` mechanism. Not ported. |

---

## Test Coverage Summary

### Unit Tests (`tests/tests.cpp`, acutest framework)

| Test | Covers | Windows |
|---|---|---|
| `test_nop` | Framework sanity check | PORTABLE |
| `test_config_bool` | ConfigBOOL parsing (true/false/invalid) | PORTABLE |
| `test_config_uint64` | ConfigUINT64 parsing | PORTABLE |
| `test_config_int` | ConfigINT parsing | PORTABLE |
| `test_config_str` | ConfigSTR parsing | PORTABLE |
| `test_config_branches` | Branch spec parsing (paths, modes, minfreespace, colon delimiter) | PORTABLE |
| `test_config_cachefiles` | CacheFiles modes (off/partial/full/auto-full) | PORTABLE |
| `test_config_inodecalc` | InodeCalc modes (passthrough/path-hash/devino-hash/hybrid-hash, 32/64-bit) | PORTABLE |
| `test_config_moveonenospc` | MoveOnENOSPC parsing (false/mfs/mspmfs/true→pfrd) | PORTABLE |
| `test_config_nfsopenhack` | NFSOpenHack modes (off/git/all) | PORTABLE |
| `test_config_readdir` | Empty — no assertions | PORTABLE |
| `test_config_statfs` | StatFS modes (base/full) | PORTABLE |
| `test_config_statfsignore` | StatFSIgnore modes (none/ro/nc) | PORTABLE |
| `test_config_xattr` | XAttr modes (passthrough/noattr/nosys) | PORTABLE |
| `test_config` | General config set ("async-read=true") | PORTABLE |
| `test_str_stuff` | String split/lsplit1/rsplit1 utilities | PORTABLE |

### Integration Tests (`tests/TEST_*`, Python scripts, require live mount)

| Test | Covers | Windows |
|---|---|---|
| `TEST_o_direct` | O_DIRECT read/write with mmap on an unlinked file | VERIFY — `O_DIRECT` has no direct equivalent; `mmap` differs; fd-after-unlink depends on WinFSP. Test may need rewriting. |
| `TEST_no_fuse_hidden` | Verifies .fuse_hidden files don't appear in directory listing after unlink | VERIFY — .fuse_hidden is a FUSE implementation detail for handling unlink-while-open. WinFSP may use a different mechanism. |
| `TEST_unlink_rename` | fstat remains valid after unlink+rename sequence | VERIFY — depends on fd-after-unlink semantics |
| `TEST_use_fallocate_after_unlink` | posix_fallocate works on fd after file is unlinked | VERIFY — `posix_fallocate` doesn't exist on Windows; needs emulation. fd-after-unlink. |
| `TEST_use_fchmod_after_unlink` | fchmod works on fd after file is unlinked | VERIFY — `fchmod` emulation + fd-after-unlink |
| `TEST_use_fchown_after_unlink` | fchown works on fd after file is unlinked | N/A — `fchown` is a no-op on Windows; test only verifies fd-after-unlink |
| `TEST_use_fstat_after_unlink` | fstat works on fd after file is unlinked | VERIFY — fd-after-unlink |
| `TEST_use_ftruncate_after_unlink` | ftruncate works on fd after file is unlinked | VERIFY — fd-after-unlink |
| `TEST_use_futimens_after_unlink` | utime works on fd after file is unlinked | VERIFY — fd-after-unlink |

### Coverage Gaps

The following features have **no test coverage**:

- All policies (no functional tests for branch selection behavior)
- All FUSE operations except the unlink-related integration tests
- Readdir modes (seq/cor/cosr)
- Extended attribute control interface
- Runtime configuration changes
- Cross-device link/rename handling
- Follow symlinks behavior
- Symlinkify
- Move on ENOSPC (functional, only config parsing tested)
- Passthrough I/O
- Branch glob expansion
- StatFS aggregation
- File locking
- Multiple branch interaction scenarios

---

## Appendix: Windows Specifics

### A.1 WinFSP FUSE Capability Flags

The following FUSE capability flags are used during `init` and need verification against WinFSP:

| Capability | Used by | WinFSP support |
|---|---|---|
| `FUSE_CAP_ASYNC_READ` | `async-read` config | Likely supported |
| `FUSE_CAP_POSIX_LOCKS` | `lock` operation | Supported (mapped to Windows locks) |
| `FUSE_CAP_FLOCK_LOCKS` | `flock` operation | Supported (mapped to Windows locks) |
| `FUSE_CAP_WRITEBACK_CACHE` | `cache.writeback` config | Unknown — needs testing |
| `FUSE_CAP_PASSTHROUGH` | `passthrough.io` config | Not supported |
| `FUSE_CAP_PARALLEL_DIROPS` | `parallel-direct-writes` | Unknown — needs testing |
| `FUSE_CAP_DIRECT_IO_ALLOW_MMAP` | `direct-io-allow-mmap` | Unknown — needs testing |

### A.2 Windows File Timestamps

Windows has four timestamps vs Linux's three:

| Windows | Linux | Notes |
|---|---|---|
| CreationTime | birthtime (not universally supported) | WinFSP FUSE compat adds `setcrtime` callback. mergerfs does not currently set creation time. |
| LastAccessTime | atime | Mapped by `utimens`/`futimens` |
| LastWriteTime | mtime | Mapped by `utimens`/`futimens` |
| ChangeTime | ctime | Linux ctime is metadata change time (not settable). WinFSP adds `setchgtime` callback. |

mergerfs should implement `setcrtime` and `setchgtime` WinFSP extensions to properly handle Windows timestamps.

### A.3 Windows File Attributes

Windows files have attributes not present on Linux:

| Attribute | Description | Relevance |
|---|---|---|
| `FILE_ATTRIBUTE_HIDDEN` | Hidden file (`.` prefix convention on Linux) | Could map dotfiles ↔ hidden attribute |
| `FILE_ATTRIBUTE_SYSTEM` | System file | Not typically needed |
| `FILE_ATTRIBUTE_READONLY` | Read-only flag | Overlaps with `chmod` removing write bits |
| `FILE_ATTRIBUTE_ARCHIVE` | Modified since last backup | Set by OS on modification |
| `FILE_ATTRIBUTE_COMPRESSED` | NTFS compression | Passthrough from underlying FS |
| `FILE_ATTRIBUTE_ENCRYPTED` | EFS encryption | Passthrough from underlying FS |

These attributes are transparently passed through by WinFSP's FUSE compat layer via the `st_mode` mapping, but some (hidden, readonly) may need explicit handling if the mergerfs getattr/chmod code doesn't account for them.

### A.4 Process Identification

Several features need to identify the calling process:

| Feature | Linux mechanism | Windows equivalent |
|---|---|---|
| `cache.files=per-process` | `/proc/<pid>/comm` | `QueryFullProcessImageName` or `GetProcessImageFileName` |
| `proxy-ioprio` | `/proc/<pid>/io` + `ioprio_get` | No direct equivalent; stub |
| fuse_req_ctx `pid` | `fuse_req_ctx.pid` | WinFSP provides caller PID in request context |

### A.5 Path Conversion Boundary

MSYS-style paths are used throughout mergerfs internals. Conversion to Windows native happens at these points:

| Boundary | Direction | Example |
|---|---|---|
| `fs_open`, `fs_mkdir`, etc. | MSYS → native | `/d/media` → `D:\media` |
| `fs_stat`, `fs_readlink`, etc. | native → MSYS (for return values containing paths) | `D:\media\file.txt` → `/d/media/file.txt` |
| Branch glob expansion | MSYS → native (for `FindFirstFile`) | `/d/disk*` → `D:\disk*` |
| Symlink targets (symlinkify) | MSYS → native (OS resolves the symlink) | `/d/media/file.txt` → `D:\media\file.txt` |
| xattr path responses | Keep MSYS-style (internal to mergerfs) | Returns `/d/media` |

### A.6 Error Code Mapping

WinFSP maps Windows error codes to POSIX errno values, but some edge cases need attention:

| Scenario | Linux | Windows | Notes |
|---|---|---|---|
| Disk full | `ENOSPC` | `ERROR_DISK_FULL` → `ENOSPC` | Should map correctly |
| Cross-device rename | `EXDEV` | `MoveFileEx` may not return `EXDEV` — it may do copy+delete silently | Verify WinFSP behavior |
| Delete open file | `EBUSY` (only for dirs) | `ERROR_SHARING_VIOLATION` → `EBUSY` | WinFSP POSIX semantics may override |
| Permission denied | `EACCES` / `EPERM` | `ERROR_ACCESS_DENIED` → `EACCES` | Should map correctly |
| File exists | `EEXIST` | `ERROR_ALREADY_EXISTS` → `EEXIST` | Should map correctly |
| No xattr support | `ENOTSUP` / `ENOATTR` | Varies by FS | Verify per branch FS type |
