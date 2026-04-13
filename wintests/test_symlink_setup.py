"""Symlink fixture setup — creates two local branches with symlinks.
Developer Mode must be enabled (no elevation needed for local drives).

Usage: python3 wintests/test_symlink_setup.py
"""
import os, sys, shutil

BRANCH_A = r"C:\temp\mergerfs_branch_a"
BRANCH_B = r"C:\temp\mergerfs_branch_b"


def clean_dir(path):
    """Remove and recreate a directory."""
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path)


def make_symlink(target, link, is_dir=False):
    """Create a symlink, print result."""
    try:
        os.symlink(target, link, target_is_directory=is_dir)
        assert os.path.islink(link)
        assert os.readlink(link) == target
        print(f"  OK: {os.path.basename(link)} -> {target}")
    except Exception as e:
        print(f"  FAIL: {os.path.basename(link)} -> {target}: {e}")
        return False
    return True


errors = 0

# --- Create clean branches ---
print("Creating branches...")
clean_dir(BRANCH_A)
clean_dir(BRANCH_B)

# =============================================================
# Branch A: files and symlinks for same-branch testing
# =============================================================
print(f"\n--- Branch A ({BRANCH_A}) ---")

# Regular files
with open(os.path.join(BRANCH_A, "alpha.txt"), "w") as f:
    f.write("alpha content\n")
print("  OK: alpha.txt (regular file)")

# Same-branch file symlink
if not make_symlink("alpha.txt", os.path.join(BRANCH_A, "alpha_link.txt")):
    errors += 1

# Directory + same-branch dir symlink
os.makedirs(os.path.join(BRANCH_A, "subdir_a"))
with open(os.path.join(BRANCH_A, "subdir_a", "nested.txt"), "w") as f:
    f.write("nested in subdir_a\n")
print("  OK: subdir_a/nested.txt (dir with file)")

if not make_symlink("subdir_a", os.path.join(BRANCH_A, "subdir_a_link"), is_dir=True):
    errors += 1

# =============================================================
# Branch B: files for cross-branch testing
# =============================================================
print(f"\n--- Branch B ({BRANCH_B}) ---")

with open(os.path.join(BRANCH_B, "bravo.txt"), "w") as f:
    f.write("bravo content\n")
print("  OK: bravo.txt (regular file)")

os.makedirs(os.path.join(BRANCH_B, "subdir_b"))
with open(os.path.join(BRANCH_B, "subdir_b", "deep.txt"), "w") as f:
    f.write("deep in subdir_b\n")
print("  OK: subdir_b/deep.txt (dir with file)")

# =============================================================
# Cross-branch symlink: symlink on A targets file on B
# =============================================================
print(f"\n--- Cross-branch symlink (on branch A, targets bravo.txt from B) ---")

if not make_symlink("bravo.txt", os.path.join(BRANCH_A, "cross_link.txt")):
    errors += 1


# =============================================================
# Summary
# =============================================================
print(f"\n--- Fixture listing ---")
for base, label in [(BRANCH_A, "Branch A"), (BRANCH_B, "Branch B")]:
    print(f"  {label} ({base}):")
    for root, dirs, files in os.walk(base):
        rel = os.path.relpath(root, base)
        prefix = "    " if rel == "." else f"    {rel}/"
        # Show symlinks in dirs list
        for d in sorted(dirs):
            p = os.path.join(root, d)
            if os.path.islink(p):
                print(f"{prefix}{d} -> {os.readlink(p)} (dir symlink)")
        for f_name in sorted(files):
            p = os.path.join(root, f_name)
            if os.path.islink(p):
                print(f"{prefix}{f_name} -> {os.readlink(p)} (symlink)")
            else:
                print(f"{prefix}{f_name}")

print(f"\nSetup {'DONE' if errors == 0 else f'DONE with {errors} error(s)'}.")
sys.exit(errors)
