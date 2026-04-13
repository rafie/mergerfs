"""Full symlink test through mergerfs with local C: branch + N: branch.
Expects mergerfs running with C:/temp/mergerfs_local_branch as first branch.
"""
import os, sys, stat

MOUNT = "M:/"
C_BRANCH = r"C:\temp\mergerfs_local_branch"

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

# --- 1. Existing symlink (from N: branch) ---
print("--- 1. Existing symlink (from N: branch) ---")
lnk = os.path.join(MOUNT, "sym_link.txt")
if os.path.exists(lnk) or os.path.islink(lnk):
    s = os.lstat(lnk)
    check("lstat S_IFLNK", stat.S_ISLNK(s.st_mode))
    check("readlink", os.readlink(lnk) == "sym_target.txt")
    with open(lnk) as f:
        check("content through symlink", f.read().strip() == "symlink test via UNC")
else:
    print("  SKIP: no existing symlink on N: branch")

# --- 2. Create file symlink through mount (should land on C: branch) ---
print("\n--- 2. Create file symlink through mount ---")
src_m = os.path.join(MOUNT, "newsym_target.txt")
lnk_m = os.path.join(MOUNT, "newsym_link.txt")

with open(src_m, "w") as f:
    f.write("new symlink test\n")

for p in [lnk_m]:
    try:
        if os.path.islink(p) or os.path.exists(p):
            os.remove(p)
    except:
        pass

try:
    os.symlink("newsym_target.txt", lnk_m)
    check("symlink create via mount", True)
    s = os.lstat(lnk_m)
    check("new lstat S_IFLNK", stat.S_ISLNK(s.st_mode))
    check("new readlink", os.readlink(lnk_m) == "newsym_target.txt")
    with open(lnk_m) as f:
        check("new content via link", f.read().strip() == "new symlink test")
    # Check it landed on C: branch
    c_lnk = os.path.join(C_BRANCH, "newsym_link.txt")
    check("landed on C: branch", os.path.islink(c_lnk))
    os.remove(lnk_m)
except Exception as e:
    check(f"symlink create ({e})", False)

try:
    os.remove(src_m)
except:
    pass

# --- 3. Dir symlink through mount ---
print("\n--- 3. Dir symlink through mount ---")
subdir = os.path.join(MOUNT, "sym_testdir")
os.makedirs(subdir, exist_ok=True)
with open(os.path.join(subdir, "inside.txt"), "w") as f:
    f.write("inside dir\n")
dlnk = os.path.join(MOUNT, "sym_testdir_link")

try:
    if os.path.islink(dlnk) or os.path.exists(dlnk):
        os.remove(dlnk)
except:
    pass

try:
    os.symlink("sym_testdir", dlnk, target_is_directory=True)
    check("dir symlink create", True)
    s = os.lstat(dlnk)
    check("dir lstat S_IFLNK", stat.S_ISLNK(s.st_mode))
    with open(os.path.join(dlnk, "inside.txt")) as f:
        check("dir symlink traversal", f.read().strip() == "inside dir")
    os.remove(dlnk)
except Exception as e:
    check(f"dir symlink ({e})", False)

try:
    os.remove(os.path.join(subdir, "inside.txt"))
    os.rmdir(subdir)
except:
    pass

# --- Summary ---
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
print(f"\n=== {passed} passed, {failed} failed out of {len(results)} ===")
sys.exit(0 if failed == 0 else 1)
