"""Minimal test: create, stat, unlink — with full error detail."""
import os, sys, stat, ctypes

mount = sys.argv[1] if len(sys.argv) > 1 else "M:\\"
fpath = mount + "\\simple_del.txt"
print(f"Test file: {fpath}")

# Create
with open(fpath, "w") as f:
    f.write("hello\n")
print("Created.")

# Stat
st = os.stat(fpath)
print(f"Mode: {oct(st.st_mode)}  readonly={not (st.st_mode & stat.S_IWRITE)}")

# Check Windows file attributes
attrs = ctypes.windll.kernel32.GetFileAttributesW(fpath)
print(f"Win32 attrs: 0x{attrs:04x}")
if attrs & 1:
    print("  FILE_ATTRIBUTE_READONLY is SET — this blocks delete!")
else:
    print("  FILE_ATTRIBUTE_READONLY is NOT set")

# Unlink
try:
    os.remove(fpath)
    print("Unlink: OK")
except OSError as e:
    print(f"Unlink: FAILED — {e}")
    if hasattr(e, 'winerror'):
        print(f"  WinError: {e.winerror}")
