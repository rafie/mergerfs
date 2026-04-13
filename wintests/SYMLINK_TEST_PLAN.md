# Symlink Testing Plan

## Background

mergerfs on Windows uses WinFSP's FUSE compatibility layer. Symlinks on Windows
require either Administrator privileges or Developer Mode (Win10 1703+, local
drives only). The mergerfs `lstat` compat layer was updated to detect reparse
points and report `S_IFLNK`, and `readlink` uses
`DeviceIoControl(FSCTL_GET_REPARSE_POINT)`.

## Test Environment

- **Developer Mode**: enabled (no elevation needed for local drives)
- **Local branch A**: `C:\temp\mergerfs_branch_a`
- **Local branch B**: `C:\temp\mergerfs_branch_b`
- **mergerfs mount**: `M:`
- **Create policy**: `ff` (first-found — new files go to branch A)

The N: drive is a Linux-based Samba share. Creating Windows symlinks over SMB to
a Linux backend does not produce proper symlinks — the Samba server interprets
filesystem operations differently. Testing symlinks on Linux-backed network
branches is a separate scenario best handled via a Docker container running a
Samba server, as described in PLAN.md.

## What We Want to Demonstrate

### Scenario 1 — Same-branch symlink
A symlink and its target both reside on the same local branch. This is the
simplest case: the relative symlink target resolves within the same directory
tree on the same filesystem.

### Scenario 2 — Cross-branch symlink
A symlink on branch A points (via a relative FUSE path) to a file that
physically exists on branch B. mergerfs presents a unified namespace, so the
symlink target resolves through the mount even though the symlink and target
are on different underlying branches.

### Scenario 3 — Symlink visibility and attributes
Symlinks created directly on a local branch (outside mergerfs) are correctly
reported through the mount: `lstat` returns `S_IFLNK`, `readlink` returns the
target string, and content is accessible by following the link. Both file and
directory symlinks are tested.

### Scenario 4 — Symlink creation through the mount
`os.symlink()` called on the M: mount triggers the WinFSP → FUSE `symlink`
callback → `CreateSymbolicLinkA` on the branch. We verify whether the end-to-end
flow succeeds and the symlink appears on the expected branch.

**Known issue**: The FUSE `symlink` callback may succeed (rv=0) but WinFSP
may still return `ERROR_ACCESS_DENIED` to the client. This is a WinFSP FUSE
compat layer issue observed during development — the symlink gets created on the
branch with a `.fuse_hidden*` temp name but WinFSP fails the post-creation
rename step. The test documents this behavior.

## Scripts

### `test_symlink_setup.py` — Fixture Setup

Creates the two local branches and populates them with symlinks directly
(bypasses mergerfs). No elevation needed — Developer Mode handles local symlinks.

```
python3 wintests/test_symlink_setup.py
```

### `test_symlink_check.py` — Verification

Start mergerfs, then run checks:

```
MSYS2_ARG_CONV_EXCL="*" build/mergerfs.exe -o allow_other,category.create=ff \
  "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b" /m

python3 wintests/test_symlink_check.py
```

## Results (2026-04-13)

- **Scenario 1 (same-branch file symlink)**: 3/3 PASS — lstat S_IFLNK, readlink, content
- **Scenario 2 (same-branch directory symlink)**: 3/3 PASS — lstat S_IFLNK, readlink, traversal
- **Scenario 3 (cross-branch file symlink)**: 3/3 PASS — visible through mount, readlink, content
- **Scenario 4 (symlink creation through mount)**: FAIL — WinFSP returns ACCESS_DENIED (known issue)
- **Unified listing**: 7/7 PASS — all files from both branches visible

**Total: 16/17 passed. The single failure is a WinFSP FUSE compat layer issue, not mergerfs.**

### Bug fixes during testing

- `lstat` (compat/sys/stat.h): Was `return stat(path, buf)` which followed symlinks.
  Replaced with full reparse point detection using `GetFileAttributesA` +
  `CreateFileA(FILE_FLAG_OPEN_REPARSE_POINT)`.
- `readdir` (compat/dirent.h): Checked `FILE_ATTRIBUTE_DIRECTORY` before
  `FILE_ATTRIBUTE_REPARSE_POINT`, causing directory symlinks to report `DT_DIR`
  instead of `DT_LNK`. Reordered to check reparse point first.
- `msys_path::to_native("/m")`: Produced `M:\` with trailing backslash, which
  WinFSP rejected as invalid mount point. Removed trailing backslash for bare
  drive letters.

## Out of Scope (Future)

- **Symlinks on Linux-backed Samba shares**: Requires Docker container running
  Samba with proper symlink support configured. See PLAN.md.
- **Hard links**: Not supported by WinFSP FUSE compat layer (`link` callback
  marked unsupported).
