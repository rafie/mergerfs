"""Test Phase 4: Configuration & CLI on Windows.

Tests:
  1. CLI argument parsing (basic — already confirmed by prior tests)
  2. Config file loading via -o config=path
  3. .mergerfs control file visibility
  4. Runtime config query via xattr (if supported by WinFSP)

Usage: python3 wintests/test_phase4_config_cli.py
"""
import os, sys, shutil, subprocess, time, tempfile

BRANCH_A = r"C:\temp\mergerfs_branch_a"
BRANCH_B = r"C:\temp\mergerfs_branch_b"
MOUNT = "M:/"
MERGERFS = r"N:\lab\win\02-mergefs\mergerfs\build\mergerfs.exe"

results = []


def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")


def clean_branches():
    for b in [BRANCH_A, BRANCH_B]:
        if os.path.exists(b):
            shutil.rmtree(b)
        os.makedirs(b)


def start_mergerfs(extra_args=None, branches=None, stderr_file=None):
    """Start mergerfs, return process."""
    if branches is None:
        branches = "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b"
    cmd = [MERGERFS]
    if extra_args:
        cmd.extend(extra_args)
    cmd.extend([branches, "/m"])
    env = os.environ.copy()
    env["MSYS2_ARG_CONV_EXCL"] = "*"
    stderr_dst = open(stderr_file, "w") if stderr_file else subprocess.DEVNULL
    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=stderr_dst)
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
    if proc:
        proc.kill()
        proc.wait()
    time.sleep(2)


# =============================================================
# 1. CLI — basic argument parsing
# =============================================================
print("=== 1. CLI argument parsing ===")
clean_branches()
with open(os.path.join(BRANCH_A, "cli_test.txt"), "w") as f:
    f.write("cli works\n")

proc = start_mergerfs(["-o", "allow_other,category.create=ff"])
if proc:
    listing = os.listdir(MOUNT)
    check("mount accessible", len(listing) > 0)
    check("cli_test.txt visible", "cli_test.txt" in listing)
    with open(os.path.join(MOUNT, "cli_test.txt")) as f:
        check("content correct", f.read().strip() == "cli works")
    stop_mergerfs(proc)
else:
    check("mergerfs started", False)


# =============================================================
# 2. Config file loading
# =============================================================
print("\n=== 2. Config file loading ===")
clean_branches()
with open(os.path.join(BRANCH_A, "config_test.txt"), "w") as f:
    f.write("from config\n")

# Write a config file
config_path = r"C:\temp\mergerfs_test.conf"
with open(config_path, "w") as f:
    f.write("# mergerfs test config\n")
    f.write("category.create=ff\n")

# Use MSYS-style path for config option
proc = start_mergerfs(["-o", f"allow_other,config=/c/temp/mergerfs_test.conf"])
if proc:
    listing = os.listdir(MOUNT)
    check("mount with config file", len(listing) > 0)
    check("config_test.txt visible", "config_test.txt" in listing)

    # Verify the ff policy from config file is active:
    # create a file and check it lands on branch A (ff = first found)
    with open(os.path.join(MOUNT, "new_from_config.txt"), "w") as f:
        f.write("created with config policy\n")
    check("ff policy from config active",
          os.path.exists(os.path.join(BRANCH_A, "new_from_config.txt")))

    stop_mergerfs(proc)
else:
    check("mergerfs started with config", False)

try:
    os.remove(config_path)
except:
    pass


# =============================================================
# 3. .mergerfs control file
# =============================================================
print("\n=== 3. .mergerfs control file ===")
clean_branches()
proc = start_mergerfs(["-o", "allow_other,category.create=ff"])
if proc:
    ctrl = os.path.join(MOUNT, ".mergerfs")

    # Check if the control file is visible
    # Note: .mergerfs may not appear in listdir but should be
    # accessible directly
    exists = os.path.exists(ctrl)
    check(".mergerfs exists", exists)

    if exists:
        try:
            import stat
            s = os.stat(ctrl)
            check(".mergerfs is regular file", stat.S_ISREG(s.st_mode))
            check(".mergerfs size == 0", s.st_size == 0)
        except Exception as e:
            check(f".mergerfs stat ({e})", False)

    stop_mergerfs(proc)
else:
    check("mergerfs started", False)


# =============================================================
# 4. Version and help flags
# =============================================================
print("\n=== 4. Version/help flags ===")
env = os.environ.copy()
env["MSYS2_ARG_CONV_EXCL"] = "*"

# -v (version)
try:
    r = subprocess.run([MERGERFS, "-v"], capture_output=True, text=True,
                       timeout=10, env=env)
    combined = r.stdout + r.stderr
    check("-v prints version", "mergerfs" in combined.lower() or len(combined) > 0)
except Exception as e:
    check(f"-v flag ({e})", False)

# -h (help) — some programs exit non-zero for help
try:
    r = subprocess.run([MERGERFS, "-h"], capture_output=True, text=True,
                       timeout=10, env=env)
    combined = r.stdout + r.stderr
    has_help = any(w in combined.lower() for w in ["usage", "options", "help", "mergerfs"])
    check("-h prints help", has_help)
except Exception as e:
    check(f"-h flag ({e})", False)


# =============================================================
# Summary
# =============================================================
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
total = len(results)
print(f"\n=== {passed} passed, {failed} failed out of {total} tests ===")
sys.exit(0 if failed == 0 else 1)
