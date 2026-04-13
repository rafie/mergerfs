"""Test symlink on branch with admin, then verify through mergerfs.
Run this script with: gsudo python3 test_symlink_admin.py
"""
import os, sys

BRANCH = r"N:\lab\win\02-mergefs\test\branch1"

# Check admin
import ctypes
is_admin = ctypes.windll.shell32.IsUserAnAdmin()
print(f"Running as admin: {is_admin}")

# Create target file
src = os.path.join(BRANCH, "sym_admin_target.txt")
lnk = os.path.join(BRANCH, "sym_admin_link.txt")

with open(src, "w") as f:
    f.write("admin symlink test content\n")

# Create symlink with admin
try:
    if os.path.islink(lnk):
        os.remove(lnk)
    os.symlink("sym_admin_target.txt", lnk)
    print(f"Symlink created: {os.path.exists(lnk)}")
    print(f"Is symlink: {os.path.islink(lnk)}")
    print(f"Readlink: {os.readlink(lnk)}")
    with open(lnk) as f:
        print(f"Content via link: {f.read().strip()}")
    print("DONE - symlink created on branch. Check M:/sym_admin_link.txt through mergerfs.")
except Exception as e:
    print(f"Failed: {e}")
    # Cleanup
    os.remove(src)
