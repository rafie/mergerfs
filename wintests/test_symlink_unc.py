"""Create symlink on branch using UNC path (works with gsudo).
Run: gsudo python3 wintests/test_symlink_unc.py
"""
import os, ctypes

BRANCH_UNC = r"\\rafi-lx1\main\lab\win\02-mergefs\test\branch1"

is_admin = ctypes.windll.shell32.IsUserAnAdmin()
print(f"Running as admin: {bool(is_admin)}")

# Create target file
src = os.path.join(BRANCH_UNC, "sym_target.txt")
lnk = os.path.join(BRANCH_UNC, "sym_link.txt")

with open(src, "w") as f:
    f.write("symlink test via UNC\n")
print(f"Created target: {src}")

# Remove old link if exists
if os.path.islink(lnk) or os.path.exists(lnk):
    os.remove(lnk)

# Create symlink (relative target)
try:
    os.symlink("sym_target.txt", lnk)
    print(f"Symlink created: {lnk}")
    print(f"  exists: {os.path.exists(lnk)}")
    print(f"  islink: {os.path.islink(lnk)}")
    print(f"  readlink: {os.readlink(lnk)}")
    with open(lnk) as f:
        print(f"  content: {f.read().strip()}")
    print("SUCCESS - now check M:/sym_link.txt through mergerfs mount")
except Exception as e:
    print(f"FAILED: {e}")
    # cleanup
    if os.path.exists(src):
        os.remove(src)
