"""Test if hard links work natively on the branch volume."""
import os

branch = r"N:\lab\win\02-mergefs\test\branch1"
src = os.path.join(branch, "hl_src.txt")
dst = os.path.join(branch, "hl_dst.txt")

with open(src, "w") as f:
    f.write("hardlink test\n")

print(f"Source: {src}")
print(f"Exists: {os.path.exists(src)}")

try:
    os.link(src, dst)
    print(f"Hard link created: {os.path.exists(dst)}")
    with open(dst) as f:
        print(f"Content via link: {f.read().strip()}")
    os.remove(dst)
    print("PASS")
except Exception as e:
    print(f"Hard link failed: {e}")
    print("FAIL")

os.remove(src)
