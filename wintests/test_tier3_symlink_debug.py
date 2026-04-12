"""Debug symlink through mergerfs mount."""
import os, sys, ctypes

MOUNT = "M:"

# First check if Developer Mode is enabled
try:
    import winreg
    key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                         r"SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock")
    val, _ = winreg.QueryValueEx(key, "AllowDevelopmentWithoutDevLicense")
    print(f"Developer Mode: {'enabled' if val else 'disabled'}")
except Exception as e:
    print(f"Developer Mode check: {e}")

# Check if running as admin
is_admin = ctypes.windll.shell32.IsUserAnAdmin()
print(f"Running as admin: {bool(is_admin)}")

# Test symlink on branch directly
branch = r"N:\lab\win\02-mergefs\test\branch1"
src = os.path.join(branch, "sym_target.txt")
dst = os.path.join(branch, "sym_link.txt")

with open(src, "w") as f:
    f.write("symlink target\n")

print(f"\n--- Test symlink on branch directly ---")
try:
    os.symlink("sym_target.txt", dst)
    print(f"  PASS: symlink created on branch")
    os.remove(dst)
except Exception as e:
    print(f"  FAIL: {e}")

# Test symlink through mount
src_m = os.path.join(MOUNT, "sym_target2.txt")
dst_m = os.path.join(MOUNT, "sym_link2.txt")

with open(src_m, "w") as f:
    f.write("symlink target via mount\n")

print(f"\n--- Test symlink through mergerfs mount ---")
try:
    os.symlink("sym_target2.txt", dst_m)
    print(f"  PASS: symlink created through mount")
    with open(dst_m) as f:
        print(f"  content: {f.read().strip()}")
    os.remove(dst_m)
except Exception as e:
    print(f"  FAIL: {e}")

# Cleanup
os.remove(src)
try:
    os.remove(os.path.join(MOUNT, "sym_target2.txt"))
except:
    pass
