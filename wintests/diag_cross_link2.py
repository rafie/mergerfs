"""Diagnose why cross_link.txt is not visible through the mergerfs mount.
Part 2: focus on mount-side behavior.
"""
import os, sys, stat

BRANCH_A = r"C:\temp\mergerfs_branch_a"
MOUNT = "M:/"

# --- 1. Confirm branch A has all entries ---
print("=== Branch A os.listdir ===")
try:
    entries = os.listdir(BRANCH_A)
    for name in sorted(entries):
        full = os.path.join(BRANCH_A, name)
        try:
            s = os.lstat(full)
            mode_str = "LNK" if stat.S_ISLNK(s.st_mode) else "DIR" if stat.S_ISDIR(s.st_mode) else "REG"
        except:
            mode_str = "???"
        print(f"  {name} [{mode_str}]")
    print(f"  cross_link.txt present: {'cross_link.txt' in entries}")
except Exception as e:
    print(f"  FAILED: {e}")

# --- 2. Mount listdir ---
print(f"\n=== Mount ({MOUNT}) os.listdir ===")
try:
    entries = os.listdir(MOUNT)
    for name in sorted(entries):
        full = os.path.join(MOUNT, name)
        try:
            s = os.lstat(full)
            mode_str = "LNK" if stat.S_ISLNK(s.st_mode) else "DIR" if stat.S_ISDIR(s.st_mode) else "REG"
        except:
            mode_str = "???"
        print(f"  {name} [{mode_str}]")
    print(f"  cross_link.txt present: {'cross_link.txt' in entries}")
except Exception as e:
    print(f"  FAILED: {e}")

# --- 3. Direct access to cross_link.txt on mount ---
print(f"\n=== Direct access M:/cross_link.txt ===")
cross_m = os.path.join(MOUNT, "cross_link.txt")
print(f"  os.path.exists: {os.path.exists(cross_m)}")
print(f"  os.path.islink: {os.path.islink(cross_m)}")
print(f"  os.path.lexists: {os.path.lexists(cross_m)}")

try:
    s = os.lstat(cross_m)
    print(f"  os.lstat: mode={oct(s.st_mode)} S_ISLNK={stat.S_ISLNK(s.st_mode)}")
except Exception as e:
    print(f"  os.lstat: FAILED {e}")

try:
    target = os.readlink(cross_m)
    print(f"  os.readlink: {target}")
except Exception as e:
    print(f"  os.readlink: FAILED {e}")

# --- 4. Check alpha_link.txt on mount for comparison ---
print(f"\n=== Direct access M:/alpha_link.txt (comparison) ===")
alpha_m = os.path.join(MOUNT, "alpha_link.txt")
print(f"  os.path.exists: {os.path.exists(alpha_m)}")
print(f"  os.path.islink: {os.path.islink(alpha_m)}")
print(f"  os.path.lexists: {os.path.lexists(alpha_m)}")

try:
    s = os.lstat(alpha_m)
    print(f"  os.lstat: mode={oct(s.st_mode)} S_ISLNK={stat.S_ISLNK(s.st_mode)}")
except Exception as e:
    print(f"  os.lstat: FAILED {e}")

try:
    target = os.readlink(alpha_m)
    print(f"  os.readlink: {target}")
except Exception as e:
    print(f"  os.readlink: FAILED {e}")
