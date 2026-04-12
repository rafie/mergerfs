#!/bin/bash
# Phase 2 Tier 3 — Advanced features tests (final)
# Tests: file persistence, locking, timestamps, chmod, hard links, symlinks.
#
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_tier3_final.sh [mountpoint] [branch_path]

set -u
MOUNT="${1:-M:}"
BRANCH="${2:-N:/lab/win/02-mergefs/test/branch1}"
PASS=0
FAIL=0
SKIP=0

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

skip() {
  local desc="$1" reason="$2"
  echo "  SKIP: $desc ($reason)"
  SKIP=$((SKIP+1))
}

echo "=== Phase 2 Tier 3: Advanced Features ==="
echo "Mount: $MOUNT"
echo "Branch: $BRANCH"
echo ""

# ------------------------------------------------------------------
echo "--- 1. File persistence to branch ---"
echo "persist test" > "$MOUNT/persist.txt" 2>&1
if [ -f "$BRANCH/persist.txt" ]; then
  content=$(cat "$BRANCH/persist.txt" 2>/dev/null)
  check "file persists to branch" "$([ "$content" = "persist test" ] && echo 1 || echo 0)"
else
  check "file persists to branch" "0"
fi
rm -f "$MOUNT/persist.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 2. File locking (msvcrt.locking) ---"
python3 -c "
import os, sys, msvcrt
f = open('${MOUNT}/locktest.txt', 'w')
f.write('locked\n')
f.flush()
try:
    msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
    msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)
    print('OK')
except Exception as e:
    print(f'FAIL: {e}')
f.close()
" 2>&1
lockresult=$?
lock_ok=$(python3 -c "
import msvcrt
f = open('${MOUNT}/locktest.txt', 'w')
f.write('x')
f.flush()
try:
    msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
    msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)
    print('1')
except:
    print('0')
f.close()
" 2>/dev/null)
check "file locking" "${lock_ok:-0}"
rm -f "$MOUNT/locktest.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 3. Timestamps (utimens) ---"
echo "timestamp test" > "$MOUNT/ts_test.txt" 2>&1
ts_ok=$(python3 -c "
import os
f = '${MOUNT}/ts_test.txt'
target_time = 1577836800.0
os.utime(f, (target_time, target_time))
st = os.stat(f)
ok = abs(st.st_atime - target_time) < 2 and abs(st.st_mtime - target_time) < 2
print('1' if ok else '0')
" 2>/dev/null)
check "timestamps (utimens)" "${ts_ok:-0}"
rm -f "$MOUNT/ts_test.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 4. chmod (read-only toggle) ---"
echo "chmod test" > "$MOUNT/chmod_test.txt" 2>&1
chmod_ok=$(python3 -c "
import os, stat
f = '${MOUNT}/chmod_test.txt'
# Make read-only
os.chmod(f, stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
st = os.stat(f)
readonly = not (st.st_mode & stat.S_IWUSR)
# Restore write
os.chmod(f, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH)
st = os.stat(f)
writable = bool(st.st_mode & stat.S_IWUSR)
print('1' if readonly and writable else '0')
" 2>/dev/null)
check "chmod" "${chmod_ok:-0}"
rm -f "$MOUNT/chmod_test.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 5. Hard link ---"
# WinFSP FUSE compat does not support hard links (link callback never invoked)
skip "hard link" "WinFSP FUSE compat limitation"

# ------------------------------------------------------------------
echo ""
echo "--- 6. Symlink ---"
# Symlinks require Administrator or Developer Mode on Windows
symlink_ok=$(python3 -c "
import os
target = 'sym_target.txt'
link = '${MOUNT}/sym_link.txt'
with open('${MOUNT}/' + target, 'w') as f:
    f.write('symlink test\n')
try:
    os.symlink(target, link)
    ok = os.path.exists(link)
    if ok:
        os.remove(link)
    os.remove('${MOUNT}/' + target)
    print('1' if ok else '0')
except PermissionError:
    os.remove('${MOUNT}/' + target)
    print('NOPERM')
except Exception as e:
    try:
        os.remove('${MOUNT}/' + target)
    except:
        pass
    print('0')
" 2>/dev/null)
if [ "$symlink_ok" = "NOPERM" ]; then
  skip "symlink" "requires Administrator or Developer Mode"
elif [ "$symlink_ok" = "1" ]; then
  check "symlink" "1"
else
  check "symlink" "0"
fi

# ------------------------------------------------------------------
echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
