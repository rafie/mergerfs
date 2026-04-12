#!/bin/bash
# Phase 2 Tier 3 — xattr and ioctl tests
# Tests mergerfs-specific features: xattr-based config, runtime policy query.
#
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_tier3_xattr_ioctl.sh [mountpoint]

set -u
MOUNT="${1:-M:}"
PASS=0
FAIL=0

check() {
  local desc="$1" ok="$2"
  if [ "$ok" = "1" ]; then
    echo "  PASS: $desc"
    PASS=$((PASS+1))
  else
    echo "  FAIL: $desc"
    FAIL=$((FAIL+1))
  fi
}

echo "=== Phase 2 Tier 3: xattr & ioctl tests ==="
echo "Mount: $MOUNT"
echo ""

# mergerfs exposes config via xattrs on the mount root.
# On Linux: getfattr -n user.mergerfs.version $MOUNT/.mergerfs
# On Windows: we test via python xattr calls.

echo "--- 1. Read mergerfs version via xattr ---"
version=$(python3 -c "
import ctypes, ctypes.wintypes, os, sys
# On Windows, xattr might be exposed via WinFSP's EA (Extended Attributes)
# or via alternate data streams. Let's try reading .mergerfs file first.
mfs = os.path.join('${MOUNT}', '.mergerfs')
if os.path.exists(mfs):
    with open(mfs) as f:
        print(f.read().strip())
else:
    print('NOT_FOUND')
" 2>&1)
echo "  .mergerfs content: $version"
if [ "$version" != "NOT_FOUND" ] && [ -n "$version" ]; then
  check ".mergerfs file accessible" "1"
else
  echo "  (mergerfs virtual file not available — expected on Windows)"
  check ".mergerfs file accessible" "0"
fi

echo ""
echo "--- 2. Create and verify file persistence ---"
# Basic sanity: create a file, verify it persists on branch
echo "persist test" > "$MOUNT/persist.txt"
branch_exists=$(ls N:/lab/win/02-mergefs/test/branch1/persist.txt 2>/dev/null && echo yes || echo no)
check "file persists to branch" "$([ "$branch_exists" = "yes" ] && echo 1 || echo 0)"
rm -f "$MOUNT/persist.txt" 2>/dev/null

echo ""
echo "--- 3. File locking (flock) ---"
python3 -c "
import os, sys, msvcrt
f = open('${MOUNT}/locktest.txt', 'w')
f.write('locked\n')
f.flush()
try:
    msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
    print('  lock acquired')
    msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)
    print('  lock released')
    print('OK')
except Exception as e:
    print(f'  lock failed: {e}')
    print('FAIL')
f.close()
" 2>&1
lockresult=$?
check "file locking" "$([ $lockresult -eq 0 ] && echo 1 || echo 0)"
rm -f "$MOUNT/locktest.txt" 2>/dev/null

echo ""
echo "--- 4. Hard link ---"
echo "hardlink test" > "$MOUNT/link_src.txt"
python3 -c "
import os
src = '${MOUNT}/link_src.txt'
dst = '${MOUNT}/link_dst.txt'
try:
    os.link(src, dst)
    print(f'  src exists: {os.path.exists(src)}')
    print(f'  dst exists: {os.path.exists(dst)}')
    with open(dst) as f:
        print(f'  content: {f.read().strip()}')
    print('OK')
except Exception as e:
    print(f'  link failed: {e}')
    print('FAIL')
" 2>&1
linkresult=$?
# Hard links might not be supported on all setups
if python3 -c "import os; exit(0 if os.path.exists('${MOUNT}/link_dst.txt') else 1)" 2>/dev/null; then
  check "hard link" "1"
else
  echo "  (hard links may not be supported — not a blocker)"
  check "hard link" "0"
fi
rm -f "$MOUNT/link_src.txt" "$MOUNT/link_dst.txt" 2>/dev/null

echo ""
echo "--- 5. Symlink ---"
echo "symlink target" > "$MOUNT/sym_target.txt"
python3 -c "
import os
target = '${MOUNT}/sym_target.txt'
link = '${MOUNT}/sym_link.txt'
try:
    os.symlink(target, link)
    print(f'  link exists: {os.path.islink(link)}')
    with open(link) as f:
        print(f'  content via link: {f.read().strip()}')
    print('OK')
except Exception as e:
    print(f'  symlink failed: {e}')
    print('FAIL')
" 2>&1
if python3 -c "import os; exit(0 if os.path.exists('${MOUNT}/sym_link.txt') else 1)" 2>/dev/null; then
  check "symlink" "1"
else
  echo "  (symlinks may require Developer Mode — not a blocker)"
  check "symlink" "0"
fi
rm -f "$MOUNT/sym_link.txt" "$MOUNT/sym_target.txt" 2>/dev/null

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
