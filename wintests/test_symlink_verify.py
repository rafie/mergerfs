"""Verify symlink functionality: native branch, through mergerfs, and readlink."""
import os, sys

MOUNT = "M:"
BRANCH = r"N:\lab\win\02-mergefs\test\branch1"

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

# --- 1. Native symlink on branch (Developer Mode test) ---
print("--- 1. Native symlink on branch ---")
src = os.path.join(BRANCH, "sym_src.txt")
lnk = os.path.join(BRANCH, "sym_lnk.txt")
with open(src, "w") as f:
    f.write("native symlink test\n")
try:
    os.symlink("sym_src.txt", lnk)
    exists = os.path.exists(lnk)
    content = open(lnk).read().strip() if exists else ""
    check("native symlink create", exists)
    check("native symlink content", content == "native symlink test")
    target = os.readlink(lnk)
    check("native readlink", target == "sym_src.txt")
    os.remove(lnk)
except Exception as e:
    check(f"native symlink ({e})", False)
os.remove(src)

# --- 2. Symlink through mergerfs mount ---
print("\n--- 2. Symlink through mergerfs mount ---")
src_m = os.path.join(MOUNT, "msym_src.txt")
lnk_m = os.path.join(MOUNT, "msym_lnk.txt")
with open(src_m, "w") as f:
    f.write("mergerfs symlink test\n")
try:
    os.symlink("msym_src.txt", lnk_m)
    exists = os.path.exists(lnk_m)
    check("mergerfs symlink create", exists)
    if exists:
        content = open(lnk_m).read().strip()
        check("mergerfs symlink content", content == "mergerfs symlink test")
        # Check readlink through mount
        target = os.readlink(lnk_m)
        check("mergerfs readlink", target == "msym_src.txt")
        # Check that symlink appears on branch
        branch_lnk = os.path.join(BRANCH, "msym_lnk.txt")
        check("symlink visible on branch", os.path.islink(branch_lnk))
        os.remove(lnk_m)
except Exception as e:
    check(f"mergerfs symlink ({e})", False)
try:
    os.remove(src_m)
except:
    pass

# --- 3. Directory symlink ---
print("\n--- 3. Directory symlink through mergerfs ---")
subdir = os.path.join(MOUNT, "sym_dir_target")
os.makedirs(subdir, exist_ok=True)
testfile = os.path.join(subdir, "inside.txt")
with open(testfile, "w") as f:
    f.write("inside dir\n")
dir_lnk = os.path.join(MOUNT, "sym_dir_link")
try:
    os.symlink("sym_dir_target", dir_lnk, target_is_directory=True)
    exists = os.path.isdir(dir_lnk)
    check("dir symlink create", exists)
    if exists:
        inside = os.path.join(dir_lnk, "inside.txt")
        content = open(inside).read().strip()
        check("dir symlink traversal", content == "inside dir")
        os.remove(dir_lnk)
except Exception as e:
    check(f"dir symlink ({e})", False)
os.remove(testfile)
os.rmdir(subdir)

# --- Summary ---
print(f"\n=== {sum(1 for _,ok in results if ok)} passed, "
      f"{sum(1 for _,ok in results if not ok)} failed ===")
