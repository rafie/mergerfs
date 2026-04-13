"""Create symlinks on branch (UNC path) via admin, verify through mergerfs.
Run: gsudo python3 C:/temp/test_symlink_create_admin.py
"""
import os, sys, stat

BRANCH_UNC = r"\\rafi-lx1\main\lab\win\02-mergefs\test\branch1"

import ctypes
is_admin = ctypes.windll.shell32.IsUserAnAdmin()
print(f"Running as admin: {bool(is_admin)}")

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

# --- Create file symlink on branch ---
print("\n--- File symlink ---")
src = os.path.join(BRANCH_UNC, "sym_test_target.txt")
lnk = os.path.join(BRANCH_UNC, "sym_test_link.txt")

with open(src, "w") as f:
    f.write("admin symlink test\n")

for p in [lnk]:
    try:
        if os.path.islink(p) or os.path.exists(p):
            os.remove(p)
    except:
        pass

try:
    os.symlink("sym_test_target.txt", lnk)
    check("file symlink create (admin)", True)
    check("islink on branch", os.path.islink(lnk))
    check("readlink on branch", os.readlink(lnk) == "sym_test_target.txt")
    with open(lnk) as f:
        check("content on branch", f.read().strip() == "admin symlink test")
except Exception as e:
    check(f"file symlink create ({e})", False)

# --- Create dir symlink on branch ---
print("\n--- Directory symlink ---")
subdir = os.path.join(BRANCH_UNC, "sym_test_dir")
os.makedirs(subdir, exist_ok=True)
testfile = os.path.join(subdir, "inside.txt")
with open(testfile, "w") as f:
    f.write("inside dir\n")
dir_lnk = os.path.join(BRANCH_UNC, "sym_test_dir_link")
try:
    if os.path.islink(dir_lnk) or os.path.exists(dir_lnk):
        os.remove(dir_lnk)
except:
    pass
try:
    os.symlink("sym_test_dir", dir_lnk, target_is_directory=True)
    check("dir symlink create (admin)", True)
    check("dir symlink islink", os.path.islink(dir_lnk))
    inside = os.path.join(dir_lnk, "inside.txt")
    with open(inside) as f:
        check("dir symlink traversal", f.read().strip() == "inside dir")
except Exception as e:
    check(f"dir symlink create ({e})", False)

passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
print(f"\n=== {passed} passed, {failed} failed ===")
print("\nNow verify through mergerfs mount (non-elevated):")
print("  python3 -c \"import os; print(os.path.islink('M:/sym_test_link.txt'))\"")
