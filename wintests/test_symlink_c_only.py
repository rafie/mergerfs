"""Symlink test targeting only C: branch files.
Creates target and symlink files starting with 'c_' prefix.
Runs multiple attempts to demonstrate symlink create works when policy picks C:.
"""
import os, sys, stat

MOUNT = "M:"
C_BRANCH = r"C:\temp\mergerfs_local_branch"
N_BRANCH = r"N:\lab\win\02-mergefs\test\branch1"

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

# --- 1. Read existing symlink from N: branch ---
print("--- 1. Existing symlink visibility ---")
lnk = os.path.join(MOUNT, "sym_link.txt")
s = os.lstat(lnk)
check("lstat S_IFLNK", stat.S_ISLNK(s.st_mode))
check("readlink", os.readlink(lnk) == "sym_target.txt")
with open(lnk) as f:
    check("content via symlink", f.read().strip() == "symlink test via UNC")

# --- 2. File symlink creation (retry to hit C: branch) ---
print("\n--- 2. File symlink creation through mount ---")
succeeded = False
for attempt in range(5):
    name = f"csym_target_{attempt}.txt"
    lname = f"csym_link_{attempt}.txt"
    src_m = os.path.join(MOUNT, name)
    lnk_m = os.path.join(MOUNT, lname)

    with open(src_m, "w") as f:
        f.write(f"symlink test {attempt}\n")

    try:
        os.symlink(name, lnk_m)
        # Verify
        s = os.lstat(lnk_m)
        is_lnk = stat.S_ISLNK(s.st_mode)
        target = os.readlink(lnk_m)
        with open(lnk_m) as f:
            content = f.read().strip()
        # Check branch
        on_c = os.path.islink(os.path.join(C_BRANCH, lname))
        on_n = os.path.islink(os.path.join(N_BRANCH, lname))
        print(f"  attempt {attempt}: CREATED on {'C:' if on_c else 'N:'}")
        print(f"    lstat S_IFLNK={is_lnk} readlink={target} content={content!r}")
        succeeded = True
        # Cleanup
        os.remove(lnk_m)
        os.remove(src_m)
        break
    except Exception as e:
        print(f"  attempt {attempt}: FAIL ({e}) - policy picked N: branch")
        os.remove(src_m)

check("file symlink create (at least once)", succeeded)

# --- 3. Dir symlink ---
print("\n--- 3. Dir symlink through mount ---")
dir_succeeded = False
for attempt in range(5):
    dname = f"csym_dir_{attempt}"
    dlname = f"csym_dir_link_{attempt}"
    subdir = os.path.join(MOUNT, dname)
    dlnk = os.path.join(MOUNT, dlname)

    os.makedirs(subdir, exist_ok=True)
    with open(os.path.join(subdir, "inside.txt"), "w") as f:
        f.write("inside dir\n")

    try:
        os.symlink(dname, dlnk, target_is_directory=True)
        s = os.lstat(dlnk)
        is_lnk = stat.S_ISLNK(s.st_mode)
        with open(os.path.join(dlnk, "inside.txt")) as f:
            content = f.read().strip()
        on_c = os.path.islink(os.path.join(C_BRANCH, dlname))
        print(f"  attempt {attempt}: CREATED on {'C:' if on_c else 'N:'}")
        print(f"    lstat S_IFLNK={is_lnk} content={content!r}")
        dir_succeeded = True
        os.remove(dlnk)
        os.remove(os.path.join(subdir, "inside.txt"))
        os.rmdir(subdir)
        break
    except Exception as e:
        print(f"  attempt {attempt}: FAIL ({e}) - policy picked N:")
        try:
            os.remove(os.path.join(subdir, "inside.txt"))
            os.rmdir(subdir)
        except:
            pass

check("dir symlink create (at least once)", dir_succeeded)

# --- Summary ---
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
print(f"\n=== {passed} passed, {failed} failed out of {len(results)} ===")
sys.exit(0 if failed == 0 else 1)
