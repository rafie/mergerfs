"""Diagnose statfs/disk_usage on the mergerfs mount."""
import shutil, ctypes
from ctypes import wintypes

MOUNT = "M:\\"
BRANCH_A = "C:\\temp\\mergerfs_branch_a\\"

kernel32 = ctypes.windll.kernel32

def get_disk_free_space(path):
    free_avail = ctypes.c_ulonglong()
    total = ctypes.c_ulonglong()
    total_free = ctypes.c_ulonglong()
    ok = kernel32.GetDiskFreeSpaceExW(
        path,
        ctypes.byref(free_avail),
        ctypes.byref(total),
        ctypes.byref(total_free))
    if ok:
        return total.value, total_free.value, free_avail.value
    err = kernel32.GetLastError()
    return None, None, err

print("=== GetDiskFreeSpaceExW ===")
for label, path in [("Mount M:", MOUNT), ("Branch A (C:)", BRANCH_A)]:
    total, free, avail = get_disk_free_space(path)
    if total is not None:
        print(f"  {label}: total={total/(1024**3):.1f}GB free={free/(1024**3):.1f}GB avail={avail/(1024**3):.1f}GB")
    else:
        print(f"  {label}: FAILED err={avail}")

print("\n=== shutil.disk_usage ===")
for label, path in [("Mount M:", "M:/"), ("Branch A (C:)", BRANCH_A)]:
    try:
        u = shutil.disk_usage(path)
        print(f"  {label}: total={u.total/(1024**3):.1f}GB free={u.free/(1024**3):.1f}GB used={u.used/(1024**3):.1f}GB")
    except Exception as e:
        print(f"  {label}: FAILED {e}")
