"""Diagnose .mergerfs control file visibility on Windows."""
import os, sys, ctypes

MOUNT = "M:/"
ctrl = os.path.join(MOUNT, ".mergerfs")

print("=== .mergerfs control file diagnostics ===")

# 1. os.path.exists
print(f"os.path.exists('{ctrl}'): {os.path.exists(ctrl)}")

# 2. os.stat
try:
    s = os.stat(ctrl)
    print(f"os.stat: mode=0o{s.st_mode:o} size={s.st_size} ino={s.st_ino}")
except Exception as e:
    print(f"os.stat error: {e}")

# 3. os.lstat
try:
    s = os.lstat(ctrl)
    print(f"os.lstat: mode=0o{s.st_mode:o} size={s.st_size} ino={s.st_ino}")
except Exception as e:
    print(f"os.lstat error: {e}")

# 4. GetFileAttributesW
kernel32 = ctypes.windll.kernel32
attrs = kernel32.GetFileAttributesW("M:\\.mergerfs")
if attrs == 0xFFFFFFFF:
    err = ctypes.GetLastError()
    print(f"GetFileAttributesW: INVALID (error={err})")
else:
    print(f"GetFileAttributesW: {hex(attrs)}")

# 5. CreateFileW to open it
GENERIC_READ = 0x80000000
FILE_SHARE_READ = 1
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80

kernel32.CreateFileW.restype = ctypes.c_void_p
handle = kernel32.CreateFileW(
    "M:\\.mergerfs",
    GENERIC_READ,
    FILE_SHARE_READ,
    None,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    None
)
INVALID_HANDLE = ctypes.c_void_p(-1).value
if handle == INVALID_HANDLE:
    err = ctypes.GetLastError()
    print(f"CreateFileW: FAILED (error={err})")
else:
    print(f"CreateFileW: SUCCESS (handle={handle})")
    kernel32.CloseHandle(ctypes.c_void_p(handle))

# 6. listdir for dotfiles
print(f"\nos.listdir('{MOUNT}'):")
for f in os.listdir(MOUNT):
    print(f"  {f}")

# 7. Try accessing via \\?\
unc_path = "\\\\?\\M:\\.mergerfs"
attrs2 = kernel32.GetFileAttributesW(unc_path)
if attrs2 == 0xFFFFFFFF:
    err = ctypes.GetLastError()
    print(f"\nGetFileAttributesW(UNC '{unc_path}'): INVALID (error={err})")
else:
    print(f"\nGetFileAttributesW(UNC): {hex(attrs2)}")

# 8. Try with forward slashes
attrs3 = kernel32.GetFileAttributesW("M:/.mergerfs")
if attrs3 == 0xFFFFFFFF:
    err = ctypes.GetLastError()
    print(f"GetFileAttributesW('M:/.mergerfs'): INVALID (error={err})")
else:
    print(f"GetFileAttributesW('M:/.mergerfs'): {hex(attrs3)}")

# 9. Check what winfsp sees - try FindFirstFileW
import ctypes.wintypes
class WIN32_FIND_DATA(ctypes.Structure):
    _fields_ = [
        ("dwFileAttributes", ctypes.wintypes.DWORD),
        ("ftCreationTime", ctypes.wintypes.FILETIME),
        ("ftLastAccessTime", ctypes.wintypes.FILETIME),
        ("ftLastWriteTime", ctypes.wintypes.FILETIME),
        ("nFileSizeHigh", ctypes.wintypes.DWORD),
        ("nFileSizeLow", ctypes.wintypes.DWORD),
        ("dwReserved0", ctypes.wintypes.DWORD),
        ("dwReserved1", ctypes.wintypes.DWORD),
        ("cFileName", ctypes.c_wchar * 260),
        ("cAlternateFileName", ctypes.c_wchar * 14),
    ]
fd = WIN32_FIND_DATA()
kernel32.FindFirstFileW.restype = ctypes.c_void_p
handle = kernel32.FindFirstFileW("M:\\.mergerfs", ctypes.byref(fd))
INVALID_HANDLE = ctypes.c_void_p(-1).value
if handle == INVALID_HANDLE:
    err = ctypes.GetLastError()
    print(f"\nFindFirstFileW('M:\\.mergerfs'): FAILED (error={err})")
else:
    print(f"\nFindFirstFileW: attrs={hex(fd.dwFileAttributes)} name='{fd.cFileName}'")
    kernel32.FindClose(ctypes.c_void_p(handle))

# 10. Try listing all files including hidden
handle2 = kernel32.FindFirstFileW("M:\\*", ctypes.byref(fd))
if handle2 != INVALID_HANDLE:
    print("\nAll files via FindFirstFileW('M:\\*'):")
    while True:
        print(f"  attrs={hex(fd.dwFileAttributes)} name='{fd.cFileName}'")
        if not kernel32.FindNextFileW(ctypes.c_void_p(handle2), ctypes.byref(fd)):
            break
    kernel32.FindClose(ctypes.c_void_p(handle2))
