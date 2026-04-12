"""Debug test: create a file via mergerfs, inspect its attributes, then try to delete it."""
import os
import sys
import stat

mount = sys.argv[1] if len(sys.argv) > 1 else "M:"
testfile = os.path.join(mount, "unlink_debug.txt")

print(f"Mount: {mount}")
print(f"Test file: {testfile}")

# Step 1: Create
print("\n--- Create ---")
try:
    with open(testfile, "w") as f:
        f.write("delete me\n")
    print("  Created OK")
except Exception as e:
    print(f"  Create FAILED: {e}")
    sys.exit(1)

# Step 2: Check attributes on the merged mount
print("\n--- Stat on mount ---")
try:
    st = os.stat(testfile)
    print(f"  mode: {oct(st.st_mode)}")
    print(f"  size: {st.st_size}")
    print(f"  uid:  {st.st_uid}")
    print(f"  gid:  {st.st_gid}")
    print(f"  is_readonly (Windows): {not (st.st_mode & stat.S_IWRITE)}")
except Exception as e:
    print(f"  Stat FAILED: {e}")

# Step 3: Find which branch has the file
branches = [
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "branch1"),
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "branch2"),
]
for b in branches:
    bp = os.path.join(b, "unlink_debug.txt")
    if os.path.exists(bp):
        print(f"\n--- Stat on branch: {bp} ---")
        st2 = os.stat(bp)
        print(f"  mode: {oct(st2.st_mode)}")
        print(f"  is_readonly (Windows): {not (st2.st_mode & stat.S_IWRITE)}")

        # Step 3b: Try to delete directly from branch
        print("\n--- Direct unlink from branch ---")
        try:
            os.remove(bp)
            print("  Direct unlink OK")
        except Exception as e:
            print(f"  Direct unlink FAILED: {e}")
            # Try clearing read-only first
            print("  Clearing read-only attribute...")
            os.chmod(bp, stat.S_IWRITE | stat.S_IREAD)
            try:
                os.remove(bp)
                print("  Unlink after chmod OK — file was read-only!")
            except Exception as e2:
                print(f"  Still failed: {e2}")
        break

# Step 4: Re-create and try to delete via merged mount
print("\n--- Re-create and unlink via mount ---")
try:
    with open(testfile, "w") as f:
        f.write("delete me again\n")
    print("  Re-created OK")
except Exception as e:
    print(f"  Re-create FAILED: {e}")
    sys.exit(1)

try:
    os.remove(testfile)
    print("  Unlink via mount OK")
except Exception as e:
    print(f"  Unlink via mount FAILED: {e}")
    print(f"  Error type: {type(e).__name__}")
    if hasattr(e, 'winerror'):
        print(f"  WinError: {e.winerror}")
