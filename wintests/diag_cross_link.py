"""Diagnose why cross_link.txt is not visible through the mergerfs mount.

Tests GetFileAttributes, FindFirstFile, lstat, and stat behavior
on the branch directly and through the mount.
"""
import os, sys, stat, ctypes
from ctypes import wintypes

BRANCH_A = r"C:\temp\mergerfs_branch_a"
MOUNT = "M:/"

kernel32 = ctypes.windll.kernel32

INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF
FILE_ATTRIBUTE_REPARSE_POINT = 0x400
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


def get_file_attributes(path):
    """Call GetFileAttributesA and return attrs or error."""
    attrs = kernel32.GetFileAttributesA(path.encode("utf-8"))
    if attrs == INVALID_FILE_ATTRIBUTES:
        err = kernel32.GetLastError()
        return None, err
    return attrs, 0


def find_first_file(directory, pattern="*"):
    """Use FindFirstFileA to list entries and check if cross_link.txt appears."""
    class WIN32_FIND_DATAA(ctypes.Structure):
        _fields_ = [
            ("dwFileAttributes", wintypes.DWORD),
            ("ftCreationTime", wintypes.FILETIME),
            ("ftLastAccessTime", wintypes.FILETIME),
            ("ftLastWriteTime", wintypes.FILETIME),
            ("nFileSizeHigh", wintypes.DWORD),
            ("nFileSizeLow", wintypes.DWORD),
            ("dwReserved0", wintypes.DWORD),
            ("dwReserved1", wintypes.DWORD),
            ("cFileName", ctypes.c_char * 260),
            ("cAlternateFileName", ctypes.c_char * 14),
        ]

    find_data = WIN32_FIND_DATAA()
    search = os.path.join(directory, pattern).encode("utf-8")
    handle = kernel32.FindFirstFileA(search, ctypes.byref(find_data))
    if handle == INVALID_HANDLE_VALUE:
        err = kernel32.GetLastError()
        print(f"  FindFirstFileA failed: err={err}")
        return []

    results = []
    while True:
        name = find_data.cFileName.decode("utf-8", errors="replace")
        attrs = find_data.dwFileAttributes
        is_reparse = bool(attrs & FILE_ATTRIBUTE_REPARSE_POINT)
        results.append((name, attrs, is_reparse))
        if not kernel32.FindNextFileA(handle, ctypes.byref(find_data)):
            break
    kernel32.FindClose(handle)
    return results


# --- 1. Check branch A directly ---
print("=== Branch A direct checks ===")
cross = os.path.join(BRANCH_A, "cross_link.txt")
alpha = os.path.join(BRANCH_A, "alpha_link.txt")

for label, path in [("cross_link.txt", cross), ("alpha_link.txt", alpha)]:
    print(f"\n--- {label} ({path}) ---")

    # GetFileAttributesA
    attrs, err = get_file_attributes(path)
    if attrs is not None:
        print(f"  GetFileAttributesA: {hex(attrs)} REPARSE={bool(attrs & FILE_ATTRIBUTE_REPARSE_POINT)}")
    else:
        print(f"  GetFileAttributesA: FAILED err={err}")

    # os.lstat (Python's own)
    try:
        s = os.lstat(path)
        print(f"  os.lstat: mode={oct(s.st_mode)} S_ISLNK={stat.S_ISLNK(s.st_mode)}")
    except Exception as e:
        print(f"  os.lstat: FAILED {e}")

    # os.stat (follows symlink)
    try:
        s = os.stat(path)
        print(f"  os.stat: mode={oct(s.st_mode)} S_ISREG={stat.S_ISREG(s.st_mode)}")
    except Exception as e:
        print(f"  os.stat: FAILED {e}")

    # os.readlink
    try:
        target = os.readlink(path)
        print(f"  os.readlink: {target}")
    except Exception as e:
        print(f"  os.readlink: FAILED {e}")


# --- 2. FindFirstFile on branch A ---
print(f"\n=== FindFirstFileA on {BRANCH_A} ===")
entries = find_first_file(BRANCH_A)
for name, attrs, is_reparse in entries:
    tag = " [REPARSE]" if is_reparse else ""
    print(f"  {name}: attrs={hex(attrs)}{tag}")


# --- 3. Check mount ---
print(f"\n=== Mount ({MOUNT}) checks ===")
cross_m = os.path.join(MOUNT, "cross_link.txt")
alpha_m = os.path.join(MOUNT, "alpha_link.txt")

for label, path in [("cross_link.txt", cross_m), ("alpha_link.txt", alpha_m)]:
    print(f"\n--- {label} ({path}) ---")

    attrs, err = get_file_attributes(path)
    if attrs is not None:
        print(f"  GetFileAttributesA: {hex(attrs)} REPARSE={bool(attrs & FILE_ATTRIBUTE_REPARSE_POINT)}")
    else:
        print(f"  GetFileAttributesA: FAILED err={err}")

    try:
        s = os.lstat(path)
        print(f"  os.lstat: mode={oct(s.st_mode)} S_ISLNK={stat.S_ISLNK(s.st_mode)}")
    except Exception as e:
        print(f"  os.lstat: FAILED {e}")

    try:
        s = os.stat(path)
        print(f"  os.stat: mode={oct(s.st_mode)}")
    except Exception as e:
        print(f"  os.stat: FAILED {e}")


# --- 4. os.listdir on mount ---
print(f"\n=== os.listdir({MOUNT}) ===")
try:
    listing = os.listdir(MOUNT)
    print(f"  Entries ({len(listing)}):")
    for name in sorted(listing):
        full = os.path.join(MOUNT, name)
        try:
            s = os.lstat(full)
            mode_str = "LNK" if stat.S_ISLNK(s.st_mode) else "DIR" if stat.S_ISDIR(s.st_mode) else "REG"
        except:
            mode_str = "???"
        print(f"    {name} [{mode_str}]")
    print(f"  cross_link.txt in listing: {'cross_link.txt' in listing}")
except Exception as e:
    print(f"  os.listdir FAILED: {e}")


# --- 5. FindFirstFile on mount ---
print(f"\n=== FindFirstFileA on {MOUNT} ===")
entries = find_first_file(MOUNT)
for name, attrs, is_reparse in entries:
    tag = " [REPARSE]" if is_reparse else ""
    print(f"  {name}: attrs={hex(attrs)}{tag}")
found = any(n == "cross_link.txt" for n, _, _ in entries)
print(f"  cross_link.txt in FindFirstFile results: {found}")
