"""Test what errno _unlink sets when called on branch files from the mergerfs process context."""
import ctypes
import ctypes.wintypes
import os
import sys

branch1 = r"N:\lab\win\02-mergefs\test\branch1"

# Create a test file directly on the branch
testf = os.path.join(branch1, "errno_test.txt")
with open(testf, "w") as f:
    f.write("test\n")

print(f"Test file: {testf}")
print(f"Exists: {os.path.exists(testf)}")
st = os.stat(testf)
print(f"Mode: {oct(st.st_mode)}")

# Try _unlink via ctypes to see the exact errno
msvcrt = ctypes.CDLL("ucrtbase.dll")
_unlink = msvcrt._unlink
_unlink.argtypes = [ctypes.c_char_p]
_unlink.restype = ctypes.c_int

# Also get errno
_errno = msvcrt._errno
_errno.restype = ctypes.POINTER(ctypes.c_int)

result = _unlink(testf.encode('ascii'))
err = _errno().contents.value
print(f"\n_unlink result: {result}")
print(f"errno: {err}")

import errno as errno_mod
for name in dir(errno_mod):
    if name.startswith('E') and getattr(errno_mod, name) == err:
        print(f"errno name: {name}")
        break

if result == 0:
    print("File deleted successfully")
else:
    print(f"Delete FAILED")
    # Check if file still exists
    print(f"Still exists: {os.path.exists(testf)}")

# Clean up
if os.path.exists(testf):
    os.remove(testf)
