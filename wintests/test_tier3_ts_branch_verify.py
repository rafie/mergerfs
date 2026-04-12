"""Verify timestamps on both mount and branch after os.utime."""
import os

MOUNT = "M:"
BRANCH = r"N:\lab\win\02-mergefs\test\branch1"

f_mount = os.path.join(MOUNT, "ts_verify.txt")
f_branch = os.path.join(BRANCH, "ts_verify.txt")

with open(f_mount, "w") as fh:
    fh.write("timestamp verify\n")

target_time = 1577836800.0  # 2020-01-01 00:00:00 UTC
print(f"Setting utime to {target_time} via mount...")
os.utime(f_mount, (target_time, target_time))

st_mount = os.stat(f_mount)
st_branch = os.stat(f_branch)

print(f"Mount  atime={st_mount.st_atime}  mtime={st_mount.st_mtime}")
print(f"Branch atime={st_branch.st_atime} mtime={st_branch.st_mtime}")
print(f"Branch mtime matches target: {abs(st_branch.st_mtime - target_time) < 2}")
print(f"Mount  mtime matches target: {abs(st_mount.st_mtime - target_time) < 2}")

os.remove(f_mount)
