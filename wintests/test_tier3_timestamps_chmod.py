"""Test timestamps (utimens) and chmod through mergerfs mount."""
import os, stat, time

MOUNT = "M:"

# --- Timestamps ---
print("=== Timestamp Test ===")
f = os.path.join(MOUNT, "ts_test.txt")
with open(f, "w") as fh:
    fh.write("timestamp test\n")

target_time = 1577836800.0  # 2020-01-01 00:00:00 UTC
print(f"  Setting atime/mtime to {target_time}")
os.utime(f, (target_time, target_time))
st = os.stat(f)
atime_ok = abs(st.st_atime - target_time) < 2
mtime_ok = abs(st.st_mtime - target_time) < 2
print(f"  atime: {st.st_atime} (ok={atime_ok})")
print(f"  mtime: {st.st_mtime} (ok={mtime_ok})")
print(f"  Timestamp: {'PASS' if atime_ok and mtime_ok else 'FAIL'}")
os.remove(f)

# --- chmod ---
print("\n=== chmod Test ===")
f = os.path.join(MOUNT, "chmod_test.txt")
with open(f, "w") as fh:
    fh.write("chmod test\n")

st_before = os.stat(f)
print(f"  Initial mode: {oct(st_before.st_mode)}")

# Make read-only
os.chmod(f, stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
st_ro = os.stat(f)
readonly = not (st_ro.st_mode & stat.S_IWUSR)
print(f"  After chmod 0o444: mode={oct(st_ro.st_mode)}, readonly={readonly}")

# Restore writable
os.chmod(f, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH)
st_rw = os.stat(f)
writable = bool(st_rw.st_mode & stat.S_IWUSR)
print(f"  After chmod 0o644: mode={oct(st_rw.st_mode)}, writable={writable}")
print(f"  chmod: {'PASS' if readonly and writable else 'FAIL'}")
os.remove(f)
