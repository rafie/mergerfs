"""Full symlink verification through mergerfs mount.
Tests: lstat, readlink, content follow, create, dir symlink.
Run as admin: gsudo python3 wintests/test_symlink_full.py
Or as normal user (may need Developer Mode for creation).
"""
import os, sys, stat

MOUNT = "M:"
BRANCH = r"N:\lab\win\02-mergefs\test\branch1"

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

# --- 1. Existing symlink (created by admin earlier) ---
print("--- 1. Existing symlink visibility ---")
lnk = os.path.join(MOUNT, "sym_link.txt")
src = os.path.join(MOUNT, "sym_target.txt")

if os.path.exists(lnk):
    s = os.lstat(lnk)
    check("lstat reports S_IFLNK", stat.S_ISLNK(s.st_mode))
    check("os.path.islink", os.path.islink(lnk))
    try:
        target = os.readlink(lnk)
        check("readlink returns target", target == "sym_target.txt")
    except Exception as e:
        check(f"readlink ({e})", False)
    try:
        with open(lnk) as f:
            content = f.read().strip()
        check("content through symlink", content == "symlink test via UNC")
    except Exception as e:
        check(f"content read ({e})", False)
else:
    check("existing symlink found", False)

# --- 2. Create new symlink through mount ---
print("\n--- 2. Create symlink through mergerfs mount ---")
new_src = os.path.join(MOUNT, "sym_new_target.txt")
new_lnk = os.path.join(MOUNT, "sym_new_link.txt")

with open(new_src, "w") as f:
    f.write("created through mount\n")

# Clean up old link if exists
for p in [new_lnk]:
    try:
        if os.path.islink(p) or os.path.exists(p):
            os.remove(p)
    except:
        pass

try:
    os.symlink("sym_new_target.txt", new_lnk)
    check("symlink create through mount", True)

    s = os.lstat(new_lnk)
    check("new symlink lstat S_IFLNK", stat.S_ISLNK(s.st_mode))

    target = os.readlink(new_lnk)
    check("new symlink readlink", target == "sym_new_target.txt")

    with open(new_lnk) as f:
        content = f.read().strip()
    check("new symlink content", content == "created through mount")

    # Verify it appears on branch
    branch_lnk = os.path.join(BRANCH, "sym_new_link.txt")
    check("new symlink on branch", os.path.islink(branch_lnk))

    # Cleanup
    os.remove(new_lnk)
except Exception as e:
    check(f"symlink create ({e})", False)
try:
    os.remove(new_src)
except:
    pass

# --- 3. Directory symlink through mount ---
print("\n--- 3. Directory symlink through mergerfs mount ---")
subdir = os.path.join(MOUNT, "sym_dir_target")
os.makedirs(subdir, exist_ok=True)
testfile = os.path.join(subdir, "inside.txt")
with open(testfile, "w") as f:
    f.write("inside dir\n")
dir_lnk = os.path.join(MOUNT, "sym_dir_link")
try:
    if os.path.islink(dir_lnk) or os.path.exists(dir_lnk):
        os.remove(dir_lnk)
except:
    pass
try:
    os.symlink("sym_dir_target", dir_lnk, target_is_directory=True)
    check("dir symlink create", os.path.isdir(dir_lnk))
    s = os.lstat(dir_lnk)
    check("dir symlink lstat S_IFLNK", stat.S_ISLNK(s.st_mode))

    inside = os.path.join(dir_lnk, "inside.txt")
    with open(inside) as f:
        content = f.read().strip()
    check("dir symlink traversal", content == "inside dir")

    os.remove(dir_lnk)
except Exception as e:
    check(f"dir symlink ({e})", False)
try:
    os.remove(testfile)
    os.rmdir(subdir)
except:
    pass

# --- Summary ---
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
print(f"\n=== {passed} passed, {failed} failed out of {len(results)} tests ===")
sys.exit(0 if failed == 0 else 1)
