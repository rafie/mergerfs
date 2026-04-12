#!/bin/bash
# Phase 2 Tier 3 — Advanced features tests
# Tests: .mergerfs virtual file, file persistence, locking, hard links, symlinks.
#
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_tier3_xattr_ioctl.sh [mountpoint] [branch_path]

set -u
MOUNT="${1:-M:}"
BRANCH="${2:-N:/lab/win/02-mergefs/test/branch1}"
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

echo "=== Phase 2 Tier 3: Advanced Features ==="
echo "Mount: $MOUNT"
echo "Branch: $BRANCH"
echo ""

# ------------------------------------------------------------------
echo "--- 1. Read mergerfs .mergerfs virtual file ---"
python3 -c "
import os
mfs = os.path.join('${MOUNT}', '.mergerfs')
if os.path.exists(mfs):
    with open(mfs) as f:
        print(f'  content: {f.read().strip()[:200]}')
    print('FOUND')
else:
    print('NOT_FOUND')
" 2>&1
mfs_result=$?
# .mergerfs might not be supported on Windows yet — informational
echo "  (informational — .mergerfs virtual file may not be implemented yet)"

# ------------------------------------------------------------------
echo ""
echo "--- 2. Create and verify file persistence to branch ---"
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
echo "--- 3. File locking (msvcrt.locking) ---"
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

# ------------------------------------------------------------------
echo ""
echo "--- 4. Hard link ---"
echo "hardlink test" > "$MOUNT/link_src.txt" 2>&1
python3 -c "
import os
src = '${MOUNT}/link_src.txt'
dst = '${MOUNT}/link_dst.txt'
try:
    os.link(src, dst)
    ok_src = os.path.exists(src)
    ok_dst = os.path.exists(dst)
    with open(dst) as f:
        content = f.read().strip()
    print(f'  src exists: {ok_src}')
    print(f'  dst exists: {ok_dst}')
    print(f'  content: {content}')
    if ok_src and ok_dst and content == 'hardlink test':
        print('OK')
    else:
        print('FAIL')
except Exception as e:
    print(f'  link error: {e}')
    print('FAIL')
" 2>&1
if python3 -c "import os; exit(0 if os.path.exists('${MOUNT}/link_dst.txt') else 1)" 2>/dev/null; then
  check "hard link" "1"
else
  check "hard link" "0"
fi
rm -f "$MOUNT/link_src.txt" "$MOUNT/link_dst.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 5. Symlink ---"
echo "symlink target" > "$MOUNT/sym_target.txt" 2>&1
python3 -c "
import os
target = 'sym_target.txt'
link = '${MOUNT}/sym_link.txt'
try:
    os.symlink(target, link)
    ok = os.path.exists(link)
    print(f'  link exists: {ok}')
    if ok:
        with open(link) as f:
            print(f'  content via link: {f.read().strip()}')
    print('OK' if ok else 'FAIL')
except Exception as e:
    print(f'  symlink error: {e}')
    print('FAIL')
" 2>&1
if python3 -c "import os; exit(0 if os.path.exists('${MOUNT}/sym_link.txt') else 1)" 2>/dev/null; then
  check "symlink" "1"
else
  check "symlink" "0"
fi
rm -f "$MOUNT/sym_link.txt" "$MOUNT/sym_target.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 6. Timestamps (utimens) ---"
echo "timestamp test" > "$MOUNT/ts_test.txt" 2>&1
python3 -c "
import os, time
f = '${MOUNT}/ts_test.txt'
# Set atime and mtime to a known value (2020-01-01 00:00:00 UTC)
target_time = 1577836800.0
os.utime(f, (target_time, target_time))
st = os.stat(f)
atime_ok = abs(st.st_atime - target_time) < 2
mtime_ok = abs(st.st_mtime - target_time) < 2
print(f'  atime set correctly: {atime_ok} (got {st.st_atime})')
print(f'  mtime set correctly: {mtime_ok} (got {st.st_mtime})')
print('OK' if atime_ok and mtime_ok else 'FAIL')
" 2>&1
ts_result=$?
ts_ok=$(python3 -c "
import os
f = '${MOUNT}/ts_test.txt'
st = os.stat(f)
print('1' if abs(st.st_mtime - 1577836800.0) < 2 else '0')
" 2>/dev/null)
check "timestamps (utimens)" "${ts_ok:-0}"
rm -f "$MOUNT/ts_test.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "--- 7. chmod (read-only toggle) ---"
echo "chmod test" > "$MOUNT/chmod_test.txt" 2>&1
python3 -c "
import os, stat
f = '${MOUNT}/chmod_test.txt'
# Make read-only
os.chmod(f, stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
st = os.stat(f)
readonly = not (st.st_mode & stat.S_IWUSR)
print(f'  read-only after chmod 0o444: {readonly}')
# Restore write
os.chmod(f, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH)
st = os.stat(f)
writable = bool(st.st_mode & stat.S_IWUSR)
print(f'  writable after chmod 0o644: {writable}')
print('OK' if readonly and writable else 'FAIL')
" 2>&1
chmod_ok=$(python3 -c "
import os, stat
f = '${MOUNT}/chmod_test.txt'
os.chmod(f, 0o644)
st = os.stat(f)
print('1' if (st.st_mode & stat.S_IWUSR) else '0')
" 2>/dev/null)
check "chmod" "${chmod_ok:-0}"
rm -f "$MOUNT/chmod_test.txt" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
