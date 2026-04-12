"""Check what st_ino values our compat stat reports for branch files.
Uses ctypes to call GetFileInformationByHandle directly to compare."""
import os, ctypes, ctypes.wintypes

branch = r"N:\lab\win\02-mergefs\test\branch1"

# Create two test files on the branch
for name in ["stat_a.txt", "stat_b.txt"]:
    with open(os.path.join(branch, name), "w") as f:
        f.write(f"content of {name}\n")

print("=== Branch file inode check ===\n")

kernel32 = ctypes.windll.kernel32

class BY_HANDLE_FILE_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("dwFileAttributes", ctypes.wintypes.DWORD),
        ("ftCreationTime", ctypes.wintypes.FILETIME),
        ("ftLastAccessTime", ctypes.wintypes.FILETIME),
        ("ftLastWriteTime", ctypes.wintypes.FILETIME),
        ("dwVolumeSerialNumber", ctypes.wintypes.DWORD),
        ("nFileSizeHigh", ctypes.wintypes.DWORD),
        ("nFileSizeLow", ctypes.wintypes.DWORD),
        ("nNumberOfLinks", ctypes.wintypes.DWORD),
        ("nFileIndexHigh", ctypes.wintypes.DWORD),
        ("nFileIndexLow", ctypes.wintypes.DWORD),
    ]

GENERIC_READ = 0x80000000
FILE_SHARE_READ = 1
FILE_SHARE_WRITE = 2
FILE_SHARE_DELETE = 4
OPEN_EXISTING = 3
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
INVALID_HANDLE_VALUE = ctypes.wintypes.HANDLE(-1).value

for name in ["stat_a.txt", "stat_b.txt"]:
    path = os.path.join(branch, name)

    # Python os.stat
    st = os.stat(path)
    print(f"{name}:")
    print(f"  os.stat st_ino = {st.st_ino}")
    print(f"  os.stat st_dev = {st.st_dev}")

    # Win32 GetFileInformationByHandle
    h = kernel32.CreateFileW(
        path, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        None, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, None)
    if h == INVALID_HANDLE_VALUE:
        err = kernel32.GetLastError()
        print(f"  CreateFile FAILED err={err}")
    else:
        info = BY_HANDLE_FILE_INFORMATION()
        ok = kernel32.GetFileInformationByHandle(h, ctypes.byref(info))
        kernel32.CloseHandle(h)
        if ok:
            fid = (info.nFileIndexHigh << 32) | info.nFileIndexLow
            print(f"  Win32 FileIndex = {fid} (high={info.nFileIndexHigh} low={info.nFileIndexLow})")
            print(f"  Win32 VolumeSerial = {info.dwVolumeSerialNumber}")
        else:
            print(f"  GetFileInformationByHandle FAILED")
    print()

# Cleanup
for name in ["stat_a.txt", "stat_b.txt"]:
    os.remove(os.path.join(branch, name))
