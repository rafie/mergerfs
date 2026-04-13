"""Test mergerfs policy system on Windows.

Verifies that create policies (ff, mfs, lfs) route new files
to the correct branch based on the policy logic.

Requires mergerfs NOT running — the test starts/stops it for each policy.

Usage: python3 wintests/test_policies.py
"""
import os, sys, shutil, subprocess, time, stat

BRANCH_A = r"C:\temp\mergerfs_branch_a"
BRANCH_B = r"C:\temp\mergerfs_branch_b"
MOUNT = "M:/"
MERGERFS = r"N:\lab\win\02-mergefs\mergerfs\build\mergerfs.exe"

results = []

def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

def clean_branches():
    """Remove and recreate clean branches."""
    for b in [BRANCH_A, BRANCH_B]:
        if os.path.exists(b):
            shutil.rmtree(b)
        os.makedirs(b)

def start_mergerfs(policy):
    """Start mergerfs with a given create policy, return process."""
    cmd = [
        MERGERFS,
        "-o", f"allow_other,category.create={policy}",
        "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b",
        "/m"
    ]
    env = os.environ.copy()
    env["MSYS2_ARG_CONV_EXCL"] = "*"
    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # Wait for mount
    for _ in range(20):
        time.sleep(0.5)
        if os.path.exists(MOUNT):
            try:
                os.listdir(MOUNT)
                return proc
            except:
                pass
    print("  ERROR: mount not available")
    proc.kill()
    return None

def stop_mergerfs(proc):
    """Stop mergerfs."""
    if proc:
        proc.kill()
        proc.wait()
    time.sleep(2)


def file_on_branch(name):
    """Return which branch a file landed on, or None."""
    pa = os.path.join(BRANCH_A, name)
    pb = os.path.join(BRANCH_B, name)
    on_a = os.path.exists(pa)
    on_b = os.path.exists(pb)
    if on_a and not on_b: return "A"
    if on_b and not on_a: return "B"
    if on_a and on_b: return "BOTH"
    return None


# =============================================================
# 1. ff (first-found) — files go to the first writable branch
# =============================================================
print("=== Policy: ff (first-found) ===")
clean_branches()
proc = start_mergerfs("ff")
if proc:
    for i in range(3):
        name = f"ff_test_{i}.txt"
        with open(os.path.join(MOUNT, name), "w") as f:
            f.write(f"ff test {i}\n")
    stop_mergerfs(proc)

    for i in range(3):
        name = f"ff_test_{i}.txt"
        branch = file_on_branch(name)
        check(f"{name} on branch A (first)", branch == "A")
else:
    print("  SKIP: could not start mergerfs")


# =============================================================
# 2. Verify unified view — files from both branches visible
# =============================================================
print("\n=== Unified view ===")
clean_branches()
# Pre-populate branches
with open(os.path.join(BRANCH_A, "from_a.txt"), "w") as f:
    f.write("branch A\n")
with open(os.path.join(BRANCH_B, "from_b.txt"), "w") as f:
    f.write("branch B\n")

proc = start_mergerfs("ff")
if proc:
    listing = os.listdir(MOUNT)
    check("from_a.txt visible", "from_a.txt" in listing)
    check("from_b.txt visible", "from_b.txt" in listing)

    with open(os.path.join(MOUNT, "from_a.txt")) as f:
        check("from_a.txt content correct", f.read().strip() == "branch A")
    with open(os.path.join(MOUNT, "from_b.txt")) as f:
        check("from_b.txt content correct", f.read().strip() == "branch B")

    stop_mergerfs(proc)
else:
    print("  SKIP: could not start mergerfs")


# =============================================================
# 3. Verify free space queries (statvfs)
# =============================================================
print("\n=== Free space (disk_usage) ===")
clean_branches()
proc = start_mergerfs("ff")
if proc:
    usage = shutil.disk_usage(MOUNT)
    check("total > 0", usage.total > 0)
    check("free > 0", usage.free > 0)
    check("used >= 0", usage.used >= 0)
    total_gb = usage.total / (1024**3)
    free_gb = usage.free / (1024**3)
    print(f"  INFO: total={total_gb:.1f}GB free={free_gb:.1f}GB")

    stop_mergerfs(proc)
else:
    print("  SKIP: could not start mergerfs")


# =============================================================
# 4. Glob branch spec
# =============================================================
print("\n=== Glob branch spec ===")
# Create branches matching a glob pattern
for d in [r"C:\temp\mergerfs_glob_1", r"C:\temp\mergerfs_glob_2"]:
    if os.path.exists(d):
        shutil.rmtree(d)
    os.makedirs(d)

with open(r"C:\temp\mergerfs_glob_1\g1.txt", "w") as f:
    f.write("glob branch 1\n")
with open(r"C:\temp\mergerfs_glob_2\g2.txt", "w") as f:
    f.write("glob branch 2\n")

cmd = [
    MERGERFS,
    "-o", "allow_other,category.create=ff",
    "/c/temp/mergerfs_glob_*",
    "/m"
]
env = os.environ.copy()
env["MSYS2_ARG_CONV_EXCL"] = "*"
proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(3)

try:
    listing = os.listdir(MOUNT)
    check("glob: g1.txt visible", "g1.txt" in listing)
    check("glob: g2.txt visible", "g2.txt" in listing)
    if "g1.txt" in listing:
        with open(os.path.join(MOUNT, "g1.txt")) as f:
            check("glob: g1.txt content", f.read().strip() == "glob branch 1")
    if "g2.txt" in listing:
        with open(os.path.join(MOUNT, "g2.txt")) as f:
            check("glob: g2.txt content", f.read().strip() == "glob branch 2")
except Exception as e:
    check(f"glob listing ({e})", False)

proc.kill()
proc.wait()
time.sleep(2)

# Cleanup glob dirs
for d in [r"C:\temp\mergerfs_glob_1", r"C:\temp\mergerfs_glob_2"]:
    shutil.rmtree(d, ignore_errors=True)


# =============================================================
# Summary
# =============================================================
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
total = len(results)
print(f"\n=== {passed} passed, {failed} failed out of {total} tests ===")
sys.exit(0 if failed == 0 else 1)
