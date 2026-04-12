#!/bin/bash
# Test rename, unlink, and rmdir operations on the merged mount.
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_rename_unlink_rmdir.sh [mountpoint]

set -u
MOUNT="${1:-M:}"
PASS=0
FAIL=0

pass() { echo "  PASS: $1"; ((PASS++)); }
fail() { echo "  FAIL: $1 — $2"; ((FAIL++)); }

echo "=== Test: rename / unlink / rmdir ==="
echo "Mount: $MOUNT"
echo ""

# --- Setup ---
echo "setup" > "$MOUNT/del_test.txt" 2>/dev/null

echo "--- 1. unlink via rm ---"
rm "$MOUNT/del_test.txt" 2>&1
rc=$?
echo "  rm exit=$rc"
if [ $rc -eq 0 ] && [ ! -f "$MOUNT/del_test.txt" ]; then
  pass "unlink via rm"
else
  fail "unlink via rm" "exit=$rc, file still exists=$([ -f "$MOUNT/del_test.txt" ] && echo yes || echo no)"
fi

echo "--- 2. unlink via python os.remove ---"
echo "pydelete" > "$MOUNT/py_del.txt" 2>/dev/null
python3 -c "
import os, sys
try:
    os.remove('${MOUNT}/py_del.txt')
    print('  os.remove: ok')
except Exception as e:
    print(f'  os.remove: {e}')
    sys.exit(1)
" 2>&1
rc=$?
if [ $rc -eq 0 ] && [ ! -f "$MOUNT/py_del.txt" ]; then
  pass "unlink via python"
else
  fail "unlink via python" "exit=$rc"
fi

echo "--- 3. rename via mv ---"
echo "moveme" > "$MOUNT/mv_src.txt" 2>/dev/null
mv "$MOUNT/mv_src.txt" "$MOUNT/mv_dst.txt" 2>&1
rc=$?
echo "  mv exit=$rc"
if [ $rc -eq 0 ] && [ ! -f "$MOUNT/mv_src.txt" ] && [ -f "$MOUNT/mv_dst.txt" ]; then
  pass "rename via mv"
else
  fail "rename via mv" "exit=$rc src_exists=$([ -f "$MOUNT/mv_src.txt" ] && echo yes || echo no) dst_exists=$([ -f "$MOUNT/mv_dst.txt" ] && echo yes || echo no)"
fi
rm -f "$MOUNT/mv_dst.txt" 2>/dev/null

echo "--- 4. rename via python os.rename ---"
echo "pyrename" > "$MOUNT/pyren_src.txt" 2>/dev/null
python3 -c "
import os, sys
try:
    os.rename('${MOUNT}/pyren_src.txt', '${MOUNT}/pyren_dst.txt')
    print('  os.rename: ok')
except Exception as e:
    print(f'  os.rename: {e}')
    sys.exit(1)
" 2>&1
rc=$?
if [ $rc -eq 0 ] && [ ! -f "$MOUNT/pyren_src.txt" ] && [ -f "$MOUNT/pyren_dst.txt" ]; then
  pass "rename via python"
else
  fail "rename via python" "exit=$rc"
fi
rm -f "$MOUNT/pyren_dst.txt" 2>/dev/null

echo "--- 5. rmdir ---"
mkdir "$MOUNT/rmdir_test" 2>/dev/null
rmdir "$MOUNT/rmdir_test" 2>&1
rc=$?
echo "  rmdir exit=$rc"
if [ $rc -eq 0 ] && [ ! -d "$MOUNT/rmdir_test" ]; then
  pass "rmdir"
else
  fail "rmdir" "exit=$rc dir_exists=$([ -d "$MOUNT/rmdir_test" ] && echo yes || echo no)"
fi

echo "--- 6. rmdir via python os.rmdir ---"
mkdir "$MOUNT/pyrmdir_test" 2>/dev/null
python3 -c "
import os, sys
try:
    os.rmdir('${MOUNT}/pyrmdir_test')
    print('  os.rmdir: ok')
except Exception as e:
    print(f'  os.rmdir: {e}')
    sys.exit(1)
" 2>&1
rc=$?
if [ $rc -eq 0 ] && [ ! -d "$MOUNT/pyrmdir_test" ]; then
  pass "rmdir via python"
else
  fail "rmdir via python" "exit=$rc"
fi

# Cleanup stragglers
rm -f "$MOUNT/del_test.txt" "$MOUNT/py_del.txt" "$MOUNT/mv_src.txt" "$MOUNT/mv_dst.txt" "$MOUNT/pyren_src.txt" "$MOUNT/pyren_dst.txt" 2>/dev/null
rmdir "$MOUNT/rmdir_test" "$MOUNT/pyrmdir_test" 2>/dev/null

echo ""
echo "=== Results: PASS=$PASS  FAIL=$FAIL ==="
[ $FAIL -eq 0 ] && exit 0 || exit 1
