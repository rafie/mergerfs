"""Test if _unlink works natively on branch files (bypassing mergerfs).
Also test if mergerfs is holding file descriptors open."""
import os, sys, stat, ctypes, ctypes.wintypes

branch1 = r"N:\lab\win\02-mergefs\test\branch1"
mount = sys.argv[1] if len(sys.argv) > 1 else r"M:\\"

print("=== Native _unlink test (bypass mergerfs) ===\n")

# Test 1: Create on branch directly, delete directly
print("--- 1. Create+delete directly on branch ---")
testf = os.path.join(branch1, "native_del.txt")
with open(testf, "w") as f:
    f.write("hello\n")
try:
    os.remove(testf)
    print("  PASS: direct unlink works")
except Exception as e:
    print(f"  FAIL: {e}")

# Test 2: Create via mergerfs, check branch, delete via branch
print("\n--- 2. Create via mergerfs, delete via branch ---")
mf = os.path.join(mount, "mfs_del.txt")
with open(mf, "w") as f:
    f.write("mergerfs file\n")
bf = os.path.join(branch1, "mfs_del.txt")
print(f"  Merged path: {mf}")
print(f"  Branch path: {bf}")
print(f"  Exists on branch: {os.path.exists(bf)}")
if os.path.exists(bf):
    st = os.stat(bf)
    print(f"  Branch mode: {oct(st.st_mode)}")
    attrs = ctypes.windll.kernel32.GetFileAttributesW(bf)
    print(f"  Branch Win32 attrs: 0x{attrs:04x}")

    # Check if any process has the file open
    # Try to open with exclusive access
    GENERIC_READ = 0x80000000
    GENERIC_WRITE = 0x40000000
    FILE_SHARE_NONE = 0
    OPEN_EXISTING = 3
    handle = ctypes.windll.kernel32.CreateFileW(
        bf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_NONE,
        None, OPEN_EXISTING, 0, None)
    INVALID_HANDLE = ctypes.wintypes.HANDLE(-1).value
    if handle == INVALID_HANDLE:
        err = ctypes.windll.kernel32.GetLastError()
        print(f"  Exclusive open FAILED — error={err} (32=sharing violation → file is open by another process)")
    else:
        print(f"  Exclusive open OK — file is NOT held open")
        ctypes.windll.kernel32.CloseHandle(handle)

    try:
        os.remove(bf)
        print("  Direct branch unlink: OK")
    except Exception as e:
        print(f"  Direct branch unlink: FAILED — {e}")

# Test 3: Create via mergerfs, delete via mergerfs
print("\n--- 3. Create via mergerfs, delete via mergerfs ---")
mf2 = os.path.join(mount, "mfs_del2.txt")
with open(mf2, "w") as f:
    f.write("mergerfs file 2\n")
print(f"  Created: {mf2}")

# Check if the file is held open on the branch
bf2 = os.path.join(branch1, "mfs_del2.txt")
if os.path.exists(bf2):
    handle = ctypes.windll.kernel32.CreateFileW(
        bf2, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_NONE,
        None, OPEN_EXISTING, 0, None)
    if handle == INVALID_HANDLE:
        err = ctypes.windll.kernel32.GetLastError()
        print(f"  Branch file is HELD OPEN (error={err})")
    else:
        print(f"  Branch file is NOT held open")
        ctypes.windll.kernel32.CloseHandle(handle)

try:
    os.remove(mf2)
    print("  Mergerfs unlink: OK")
except Exception as e:
    print(f"  Mergerfs unlink: FAILED — {e}")

print("\nDone.")
