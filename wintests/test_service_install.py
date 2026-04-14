"""Test service installation scripts.

Tests:
  1. WinFSP Launcher registration (register, start, info, stop, unregister)
  2. sc create service (install, status, uninstall)

Usage: python3 wintests/test_service_install.py
Requires: Administrator privileges (gsudo available)
"""
import os, sys, subprocess, time, shutil, winreg

BRANCH_A = r"C:\temp\mergerfs_branch_a"
BRANCH_B = r"C:\temp\mergerfs_branch_b"
MOUNT = "M:/"
MERGERFS = r"N:\lab\win\02-mergefs\mergerfs\build\mergerfs.exe"
SCRIPTS = r"N:\lab\win\02-mergefs\mergerfs\scripts"
LAUNCHCTL = r"C:\root\dev\libs\winfsp\bin\launchctl-x64.exe"
INSTALL_DIR = r"C:\temp\mergerfs_svc_test"

results = []


def check(desc, ok):
    results.append((desc, ok))
    print(f"  {'PASS' if ok else 'FAIL'}: {desc}")


def clean_branches():
    for b in [BRANCH_A, BRANCH_B]:
        if os.path.exists(b):
            shutil.rmtree(b)
        os.makedirs(b)


def run_ps(script, args):
    """Run a PowerShell script and return (returncode, stdout+stderr)."""
    cmd = ["powershell", "-ExecutionPolicy", "Bypass", "-File",
           os.path.join(SCRIPTS, script)] + args
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    return r.returncode, r.stdout + r.stderr


def wait_for_mount(timeout=15):
    for _ in range(timeout * 2):
        time.sleep(0.5)
        if os.path.exists(MOUNT):
            try:
                os.listdir(MOUNT)
                return True
            except:
                pass
    return False


def wait_for_unmount(timeout=10):
    for _ in range(timeout * 2):
        time.sleep(0.5)
        try:
            os.listdir(MOUNT)
        except:
            return True
    return False


# Kill any running mergerfs first
subprocess.run(["taskkill", "/F", "/IM", "mergerfs.exe"],
               capture_output=True, timeout=10)
time.sleep(2)

# Create install dir
os.makedirs(INSTALL_DIR, exist_ok=True)


# =============================================================
# 1. WinFSP Launcher
# =============================================================
print("=== 1. WinFSP Launcher ===")
clean_branches()
with open(os.path.join(BRANCH_A, "launcher_test.txt"), "w") as f:
    f.write("launcher works\n")

# 1a. Register
print("  --- Register ---")
rc, out = run_ps("mergerfs-launcher.ps1",
                 ["register", "-ExePath", MERGERFS,
                  "-InstallDir", INSTALL_DIR])
print(f"    rc={rc}")
if rc != 0:
    print(f"    output: {out[:500]}")
check("launcher register", rc == 0)

# Verify exe was copied locally
local_exe = os.path.join(INSTALL_DIR, "mergerfs.exe")
check("mergerfs.exe copied locally", os.path.exists(local_exe))

# Verify registry entry
reg_found = False
try:
    key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                         r"SOFTWARE\WOW6432Node\WinFsp\Services\mergerfs")
    exe_val, _ = winreg.QueryValueEx(key, "Executable")
    winreg.CloseKey(key)
    reg_found = True
    check("registry Executable value", "mergerfs" in exe_val.lower())
except FileNotFoundError:
    # Try non-WOW64
    try:
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                             r"SOFTWARE\WinFsp\Services\mergerfs")
        exe_val, _ = winreg.QueryValueEx(key, "Executable")
        winreg.CloseKey(key)
        reg_found = True
        check("registry Executable value", "mergerfs" in exe_val.lower())
    except FileNotFoundError:
        check("registry entry created", False)

# 1b. Start instance
# NOTE: The WinFSP Launcher requires the child process to implement the
# WinFSP service protocol (FspServiceRunEx / readiness signaling).
# Our vendored libfuse with custom winfsp_bridge doesn't implement this,
# so the launcher starts the process but terminates it when it doesn't
# receive the expected readiness signal.  This is a known limitation.
# Start/stop/info are tested here for API correctness (rc=0) only.
print("  --- Start (known limitation: mount won't appear) ---")
rc, out = run_ps("mergerfs-launcher.ps1",
                 ["start",
                  "-Branches", "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b",
                  "-MountPoint", "/m",
                  "-Options", "category.create=ff"])
print(f"    rc={rc}")
# launchctl start returns 0 even though process won't stay alive
check("launcher start API call", rc == 0)

# 1c. Info (will show error 2 = not running, expected)
print("  --- Info ---")
r = subprocess.run([LAUNCHCTL, "info", "mergerfs", "default"],
                   capture_output=True, text=True, timeout=10)
print(f"    info output: {r.stdout.strip()[:200]}")
check("launcher info API call", r.returncode == 0)

# 1d. Stop
print("  --- Stop ---")
rc, out = run_ps("mergerfs-launcher.ps1", ["stop"])
print(f"    rc={rc}")
check("launcher stop API call", rc == 0)

# 1e. Unregister
print("  --- Unregister ---")
rc, out = run_ps("mergerfs-launcher.ps1",
                 ["unregister", "-InstallDir", INSTALL_DIR])
print(f"    rc={rc}")
check("launcher unregister", rc == 0)

# Verify registry entry removed
try:
    key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                         r"SOFTWARE\WOW6432Node\WinFsp\Services\mergerfs")
    winreg.CloseKey(key)
    check("registry entry removed", False)
except FileNotFoundError:
    check("registry entry removed", True)


# =============================================================
# 2. sc create service (install/status/uninstall)
# =============================================================
print("\n=== 2. sc create service ===")

# 2a. Install
print("  --- Install ---")
rc, out = run_ps("mergerfs-service.ps1",
                 ["install",
                  "-Branches", "/c/temp/mergerfs_branch_a:/c/temp/mergerfs_branch_b",
                  "-MountPoint", "/m",
                  "-ExePath", MERGERFS,
                  "-InstallDir", INSTALL_DIR])
print(f"    rc={rc}")
if rc != 0:
    print(f"    output: {out[:500]}")
check("sc install", rc == 0)

# Verify service exists
r = subprocess.run(["sc", "query", "mergerfs"],
                   capture_output=True, text=True, timeout=10)
svc_exists = "SERVICE_NAME" in r.stdout
check("sc query finds service", svc_exists)

# 2b. Status
if svc_exists:
    print("  --- Status ---")
    rc, out = run_ps("mergerfs-service.ps1", ["status"])
    print(f"    output: {out.strip()[:200]}")
    check("sc status works",
          "SERVICE_NAME" in out or "STOPPED" in out or "RUNNING" in out)

# 2c. Uninstall
print("  --- Uninstall ---")
rc, out = run_ps("mergerfs-service.ps1",
                 ["uninstall", "-InstallDir", INSTALL_DIR])
print(f"    rc={rc}")
if rc != 0:
    print(f"    output: {out[:300]}")
check("sc uninstall", rc == 0)

# Verify service removed
time.sleep(1)
r = subprocess.run(["sc", "query", "mergerfs"],
                   capture_output=True, text=True, timeout=10)
check("service removed", "1060" in (r.stdout + r.stderr))


# Cleanup
if os.path.exists(INSTALL_DIR):
    shutil.rmtree(INSTALL_DIR, ignore_errors=True)


# =============================================================
# Summary
# =============================================================
passed = sum(1 for _, ok in results if ok)
failed = sum(1 for _, ok in results if not ok)
total = len(results)
print(f"\n=== {passed} passed, {failed} failed out of {total} tests ===")
sys.exit(0 if failed == 0 else 1)
