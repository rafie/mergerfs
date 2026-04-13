"""Full symlink test through mergerfs - run elevated with gsudo.
Starts mergerfs, runs tests, stops mergerfs.
Usage: gsudo python3 //rafi-lx1/main/lab/win/02-mergefs/mergerfs/wintests/test_symlink_elevated.py
"""
import os, sys, stat, subprocess, time, ctypes

is_admin = ctypes.windll.shell32.IsUserAnAdmin()
print(f"Running as admin: {bool(is_admin)}")
if not is_admin:
    print("ERROR: must run with gsudo")
    sys.exit(1)

MERGERFS = r"\\rafi-lx1\main\lab\win\02-mergefs\mergerfs\build\mergerfs.exe"
BRANCH_C = r"C:\temp\mergerfs_local_branch"
BRANCH_N = r"\\rafi-lx1\main\lab\win\02-mergefs\test\branch1"
# MSYS-style paths for mergerfs args (avoids colon-as-delimiter conflict)
MSYS_BRANCHES = "/c/temp/mergerfs_local_branch://rafi-lx1/main/lab/win/02-mergefs/test/branch1"
MOUNT = "M:/"

# Clean up stale files from previous runs
for stale in ["sym_testdir_link", "newsym_link.txt", "newsym_target.txt"]:
    p = os.path.join(BRANCH_C, stale)
    try:
        if os.path.islink(p) or os.path.isfile(p):
            os.remove(p)
        elif os.path.isdir(p):
            import shutil
            shutil.rmtree(p)
    except:
        pass
    p = os.path.join(BRANCH_N, stale)
    try:
        if os.path.islink(p) or os.path.isfile(p):
            os.remove(p)
        elif os.path.isdir(p):
            import shutil
            shutil.rmtree(p)
    except:
        pass

# Ensure local branch dir exists
os.makedirs(BRANCH_C, exist_ok=True)

# Start mergerfs (elevated, ff create policy, MSYS-style paths)
print("\nStarting mergerfs (elevated)...")
proc = subprocess.Popen(
    [MERGERFS, "-f", "-o", "allow_other,category.create=ff",
     MSYS_BRANCHES, "M:"],
    stdout=subprocess.PIPE, stderr=subprocess.PIPE
)
time.sleep(5)

if not os.path.exists(MOUNT):
    print("ERROR: M:/ not accessible after 5 seconds")
    proc.terminate()
    sys.exit(1)

print(f"M: is mounted. Contents: {os.listdir(MOUNT)}")

results = []
def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")

# --- 1. Existing symlink from N: branch ---
print("\n--- 1. Existing symlink (from N: branch) ---")
lnk = os.path.join(MOUNT, "sym_link.txt")
if os.path.exists(lnk) or os.path.islink(lnk):
    s = os.lstat(lnk)
    check("lstat S_IFLNK", stat.S_ISLNK(s.st_mode))
    check("readlink", os.readlink(lnk) == "sym_target.txt")
    with open(lnk) as f:
        check("content via symlink", f.read().strip() == "symlink test via UNC")
else:
    print("  SKIP: no existing symlink from prior test")

# --- 2. Create file symlink via os.symlink ---
print("\n--- 2. Create file symlink (os.symlink) ---")
src_m = os.path.join(MOUNT, "newsym_target.txt")
lnk_m = os.path.join(MOUNT, "newsym_link.txt")
with open(src_m, "w") as f:
    f.write("new symlink test\n")
try:
    os.symlink("newsym_target.txt", lnk_m)
    check("os.symlink file create", True)
    s = os.lstat(lnk_m)
    check("lstat S_IFLNK", stat.S_ISLNK(s.st_mode))
    check("readlink", os.readlink(lnk_m) == "newsym_target.txt")
    with open(lnk_m) as f:
        check("content via link", f.read().strip() == "new symlink test")
    c_lnk = os.path.join(BRANCH_C, "newsym_link.txt")
    check("landed on C: branch", os.path.islink(c_lnk))
    os.remove(lnk_m)
except Exception as e:
    print(f"  os.symlink failed: {e}")
    # Try with CreateSymbolicLinkW directly via ctypes
    print("  Trying ctypes CreateSymbolicLinkW...")
    kernel32 = ctypes.windll.kernel32
    SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE = 0x2
    # Try unprivileged first
    ok = kernel32.CreateSymbolicLinkW(lnk_m, "newsym_target.txt",
                                       SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
    if ok:
        print("  ctypes unprivileged: OK")
        check("ctypes symlink file create", True)
        os.remove(lnk_m)
    else:
        err = kernel32.GetLastError()
        print(f"  ctypes unprivileged: FAIL err={err}")
        # Try privileged (admin)
        ok = kernel32.CreateSymbolicLinkW(lnk_m, "newsym_target.txt", 0)
        if ok:
            print("  ctypes privileged: OK")
            check("ctypes symlink file create", True)
            os.remove(lnk_m)
        else:
            err = kernel32.GetLastError()
            print(f"  ctypes privileged: FAIL err={err}")
            # Try cmd /c mklink
            print("  Trying cmd /c mklink...")
            r = subprocess.run(
                ["cmd", "/c", "mklink", "M:\\newsym_link.txt", "newsym_target.txt"],
                capture_output=True, text=True, cwd="M:\\")
            print(f"    stdout: {r.stdout.strip()}")
            print(f"    stderr: {r.stderr.strip()}")
            print(f"    rc: {r.returncode}")
            if r.returncode == 0:
                check("mklink symlink file create", True)
                os.remove(lnk_m)
            else:
                check("symlink file create (all methods)", False)
try:
    os.remove(src_m)
except:
    pass

# --- 3. Dir symlink ---
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

# Stop mergerfs
print("\nStopping mergerfs...")
proc.terminate()
proc.wait(timeout=10)
print("Done.")
sys.exit(0 if failed == 0 else 1)
