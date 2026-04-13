"""Check environment: Developer Mode, drive types, admin status."""
import ctypes, os

# Admin status
is_admin = ctypes.windll.shell32.IsUserAnAdmin()
print(f"Running as admin: {bool(is_admin)}")

# Developer Mode
try:
    import winreg
    key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
        r"SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock")
    val, _ = winreg.QueryValueEx(key, "AllowDevelopmentWithoutDevLicense")
    print(f"Developer Mode: {'enabled' if val else 'disabled'}")
except Exception as e:
    print(f"Developer Mode: {e}")

# Drive types
for d in ["C:", "N:"]:
    path = d + "\\"
    dtype = ctypes.windll.kernel32.GetDriveTypeW(path)
    types = {0: "unknown", 1: "no_root", 2: "removable", 3: "fixed",
             4: "remote", 5: "cdrom", 6: "ramdisk"}
    print(f"Drive {d} type={types.get(dtype, dtype)}")

# Quick symlink test on C: temp
import tempfile
tmpdir = tempfile.mkdtemp(prefix="symtest_")
src = os.path.join(tmpdir, "src.txt")
lnk = os.path.join(tmpdir, "lnk.txt")
with open(src, "w") as f:
    f.write("test")
try:
    os.symlink(src, lnk)
    print(f"Symlink on C: PASS")
    os.remove(lnk)
except Exception as e:
    print(f"Symlink on C: FAIL ({e})")
os.remove(src)
os.rmdir(tmpdir)
