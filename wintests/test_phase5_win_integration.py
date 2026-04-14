"""Test Phase 5: Windows Integration.

Tests:
  1. Volume label (Explorer shows "MergerFS")
  2. Filesystem type name
  3. Service mode (WinFSP service registration/deregistration)
  4. Drive properties (capacity, free space shown correctly)

Usage: python3 wintests/test_phase5_win_integration.py
"""
import os, sys, subprocess, time, shutil, ctypes, ctypes.wintypes

BRANCH_A = r"C:\temp\mergerfs_branch_a"
BRANCH_B = r"C:\temp\mergerfs_branch_b"
MOUNT = "M:/"
MOUNT_LETTER = "M:"
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


def start_mergerfs(extra_args=None, branches=None):
    """Start mergerfs, return process."""
    if branches is None:
        branches = "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b"
    cmd = [MERGERFS]
    if extra_args:
        cmd.extend(extra_args)
    cmd.extend([branches, "/m"])
    env = os.environ.copy()
    env["MSYS2_ARG_CONV_EXCL"] = "*"
    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
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
# 1. Volume label
# =============================================================
print("=== 1. Volume label ===")
clean_branches()
with open(os.path.join(BRANCH_A, "vol_test.txt"), "w") as f:
    f.write("volume label test\n")

proc = start_mergerfs(["-o", "allow_other,category.create=ff"])
if proc:
    # GetVolumeInformationW returns volume label and filesystem name
    kernel32 = ctypes.windll.kernel32

    vol_name = ctypes.create_unicode_buffer(261)
    vol_serial = ctypes.wintypes.DWORD()
    max_comp_len = ctypes.wintypes.DWORD()
    fs_flags = ctypes.wintypes.DWORD()
    fs_name = ctypes.create_unicode_buffer(261)

    ok = kernel32.GetVolumeInformationW(
        MOUNT_LETTER + "\\",
        vol_name, 261,
        ctypes.byref(vol_serial),
        ctypes.byref(max_comp_len),
        ctypes.byref(fs_flags),
        fs_name, 261
    )

    if ok:
        print(f"  Volume label: '{vol_name.value}'")
        print(f"  FS name: '{fs_name.value}'")
        print(f"  Max component length: {max_comp_len.value}")
        check("volume label is MergerFS", vol_name.value == "MergerFS")
        check("filesystem name contains mergerfs",
              "mergerfs" in fs_name.value.lower())
    else:
        err = ctypes.GetLastError()
        check(f"GetVolumeInformationW (error={err})", False)

    # Also verify mount is functional
    listing = os.listdir(MOUNT)
    check("mount accessible with volume label", "vol_test.txt" in listing)

    stop_mergerfs(proc)
else:
    check("mergerfs started", False)


# =============================================================
# 2. Drive capacity in Explorer
# =============================================================
print("\n=== 2. Drive capacity ===")
clean_branches()
proc = start_mergerfs(["-o", "allow_other,category.create=ff"])
if proc:
    usage = shutil.disk_usage(MOUNT)
    print(f"  Total: {usage.total / (1024**3):.1f} GB")
    print(f"  Used:  {usage.used / (1024**3):.1f} GB")
    print(f"  Free:  {usage.free / (1024**3):.1f} GB")
    check("total > 0", usage.total > 0)
    check("free > 0", usage.free > 0)
    check("free <= total", usage.free <= usage.total)

    stop_mergerfs(proc)
else:
    check("mergerfs started", False)


# =============================================================
# 3. Custom volume label via -o volname=
# =============================================================
print("\n=== 3. Custom volume label override ===")
clean_branches()
proc = start_mergerfs(["-o", "allow_other,category.create=ff,volname=MyMerge"])
if proc:
    kernel32 = ctypes.windll.kernel32
    vol_name = ctypes.create_unicode_buffer(261)
    fs_name = ctypes.create_unicode_buffer(261)

    ok = kernel32.GetVolumeInformationW(
        MOUNT_LETTER + "\\",
        vol_name, 261,
        None, None, None,
        fs_name, 261
    )

    if ok:
        print(f"  Volume label: '{vol_name.value}'")
        # User-specified volname should override default
        print(f"  Expected: 'MyMerge'")
        check("custom volume label applied", vol_name.value == "MyMerge")
    else:
        err = ctypes.GetLastError()
        check(f"GetVolumeInformationW custom (error={err})", False)

    stop_mergerfs(proc)
else:
    check("mergerfs started", False)


# =============================================================
# 4. Service registration (dry run)
# =============================================================
print("\n=== 4. Service mode support ===")
# WinFSP FUSE service mode is automatic through fuse_main.
# We can verify that mergerfs runs as a WinFSP service by checking
# if it registers with the WinFSP launcher.
# For now, just verify that the process name is discoverable.
env = os.environ.copy()
env["MSYS2_ARG_CONV_EXCL"] = "*"

# Test: mergerfs can be started and stopped cleanly
clean_branches()
proc = start_mergerfs(["-o", "allow_other,category.create=ff"])
if proc:
    # Verify it's running
    check("mergerfs process running", proc.poll() is None)

    # Verify mount is functional
    with open(os.path.join(MOUNT, "svc_test.txt"), "w") as f:
        f.write("service test\n")
    check("write works", os.path.exists(os.path.join(BRANCH_A, "svc_test.txt")))

    # Clean stop
    proc.terminate()
    try:
        proc.wait(timeout=10)
        check("clean termination", True)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        check("clean termination (had to force kill)", False)

    time.sleep(2)
else:
    check("mergerfs started for service test", False)


# =============================================================
# Summary
# =============================================================
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
total = len(results)
print(f"\n=== {passed} passed, {failed} failed out of {total} tests ===")
sys.exit(0 if failed == 0 else 1)
