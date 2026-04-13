"""Symlink verification through mergerfs mount.

Requires: test_symlink_setup.py run first, then mergerfs started with:

  MSYS2_ARG_CONV_EXCL="*" build/mergerfs.exe -o allow_other,category.create=ff \
    "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b" /m

Usage: python3 wintests/test_symlink_check.py
"""
import os, sys, stat

MOUNT = "M:/"
BRANCH_A = r"C:\temp\mergerfs_branch_a"
BRANCH_B = r"C:\temp\mergerfs_branch_b"

# --- Preflight ---
if not os.path.exists(MOUNT):
    print(f"ERROR: {MOUNT} not accessible. Is mergerfs running?")
    sys.exit(1)

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

def skip(desc, reason):
    print(f"  SKIP: {desc} ({reason})")


# =============================================================
# 1. Same-branch file symlink (alpha_link.txt -> alpha.txt)
# =============================================================
print("--- 1. Same-branch file symlink ---")
lnk = os.path.join(MOUNT, "alpha_link.txt")
if os.path.islink(lnk) or os.path.exists(lnk):
    s = os.lstat(lnk)
    check("lstat reports S_IFLNK", stat.S_ISLNK(s.st_mode))
    try:
        check("readlink", os.readlink(lnk) == "alpha.txt")
    except Exception as e:
        check(f"readlink ({e})", False)
    try:
        with open(lnk) as f:
            check("content through symlink", f.read().strip() == "alpha content")
    except Exception as e:
        check(f"content ({e})", False)
else:
    skip("alpha_link.txt", "not found — run setup first")


# =============================================================
# 2. Same-branch directory symlink (subdir_a_link -> subdir_a)
# =============================================================
print("\n--- 2. Same-branch directory symlink ---")
dlnk = os.path.join(MOUNT, "subdir_a_link")
if os.path.islink(dlnk) or os.path.exists(dlnk):
    s = os.lstat(dlnk)
    check("lstat reports S_IFLNK", stat.S_ISLNK(s.st_mode))
    try:
        check("readlink", os.readlink(dlnk) == "subdir_a")
    except Exception as e:
        check(f"readlink ({e})", False)
    nested = os.path.join(dlnk, "nested.txt")
    try:
        with open(nested) as f:
            check("traverse + read content", f.read().strip() == "nested in subdir_a")
    except Exception as e:
        check(f"traverse ({e})", False)
else:
    skip("subdir_a_link", "not found — run setup first")


# =============================================================
# 3. Cross-branch symlink
#    Create a symlink on branch A that targets bravo.txt (on branch B).
#    Since mergerfs presents a unified namespace, the relative target
#    "bravo.txt" resolves through the mount even though the symlink
#    and target are on different physical branches.
# =============================================================
print("\n--- 3. Cross-branch file symlink ---")
# cross_link.txt on branch A points to bravo.txt which lives on branch B.
# Through the unified mount, the relative target resolves across branches.
cross_lnk = os.path.join(MOUNT, "cross_link.txt")
if os.path.islink(cross_lnk) or os.path.exists(cross_lnk):
    s = os.lstat(cross_lnk)
    check("lstat reports S_IFLNK", stat.S_ISLNK(s.st_mode))
    try:
        check("readlink", os.readlink(cross_lnk) == "bravo.txt")
    except Exception as e:
        check(f"readlink ({e})", False)
    # The symlink is on branch A, target bravo.txt is on branch B.
    # Through the mount, the relative path should resolve to M:/bravo.txt.
    try:
        with open(cross_lnk) as f:
            content = f.read().strip()
        check("cross-branch content", content == "bravo content")
    except Exception as e:
        check(f"cross-branch content ({e})", False)
else:
    skip("cross_link.txt", "not found — run setup first")


# =============================================================
# 4. Symlink creation through mount
# =============================================================
print("\n--- 4. Symlink creation through mount ---")
tgt_path = os.path.join(MOUNT, "mount_tgt.txt")
lnk_path = os.path.join(MOUNT, "mount_lnk.txt")

with open(tgt_path, "w") as f:
    f.write("created through mount\n")

try:
    os.symlink("mount_tgt.txt", lnk_path)
    check("os.symlink succeeds", True)

    s = os.lstat(lnk_path)
    check("lstat S_IFLNK", stat.S_ISLNK(s.st_mode))

    target = os.readlink(lnk_path)
    check("readlink", target == "mount_tgt.txt")

    with open(lnk_path) as f:
        check("content through link", f.read().strip() == "created through mount")

    c_lnk = os.path.join(BRANCH_A, "mount_lnk.txt")
    check("landed on branch A", os.path.islink(c_lnk))

    os.remove(lnk_path)
except Exception as e:
    check(f"symlink create ({e})", False)
    # The FUSE callback may succeed even if client gets an error.
    # Check if the symlink was created on the branch.
    c_lnk = os.path.join(BRANCH_A, "mount_lnk.txt")
    if os.path.islink(c_lnk):
        print("  NOTE: symlink exists on branch A (FUSE callback succeeded)")
        print("        WinFSP returned error to client — known compat issue")
        os.remove(c_lnk)
    # Also clean up .fuse_hidden* leftovers
    for f_name in os.listdir(BRANCH_A):
        if f_name.startswith(".fuse_hidden"):
            p = os.path.join(BRANCH_A, f_name)
            try:
                os.remove(p)
            except Exception:
                pass

try:
    os.remove(tgt_path)
except Exception:
    pass


# =============================================================
# 5. Unified listing — files from both branches visible
# =============================================================
print("\n--- 5. Unified listing ---")
listing = os.listdir(MOUNT)
expected_from_a = ["alpha.txt", "alpha_link.txt", "subdir_a", "subdir_a_link", "cross_link.txt"]
expected_from_b = ["bravo.txt", "subdir_b"]
for name in expected_from_a:
    check(f"branch A: {name} visible", name in listing)
for name in expected_from_b:
    check(f"branch B: {name} visible", name in listing)


# =============================================================
# Summary
# =============================================================
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
total = len(results)
print(f"\n=== {passed} passed, {failed} failed out of {total} tests ===")
sys.exit(0 if failed == 0 else 1)
