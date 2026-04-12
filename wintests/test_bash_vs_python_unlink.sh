#!/bin/bash
# Compare bash rm vs python os.remove for unlink behavior on merged mount.
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_bash_vs_python_unlink.sh [mountpoint]

set -u
MOUNT="${1:-M:}"

echo "=== bash rm vs python os.remove ==="
echo "Mount: $MOUNT"
echo ""

echo "--- 1. Create file via bash ---"
echo "test data" > "$MOUNT/bash_del.txt"
echo "  create exit=$?"

echo "--- 2. Verify exists ---"
ls -la "$MOUNT/bash_del.txt"

echo "--- 3. bash rm ---"
rm -v "$MOUNT/bash_del.txt" 2>&1
echo "  rm exit=$?"

echo "--- 4. Check after rm ---"
if [ -f "$MOUNT/bash_del.txt" ]; then
  echo "  FILE STILL EXISTS after rm"
else
  echo "  file gone (expected)"
fi

echo ""
echo "--- 5. Create file for python test ---"
echo "test data 2" > "$MOUNT/py_del2.txt"
echo "  create exit=$?"

echo "--- 6. python os.remove ---"
python3 -c "
import os
f = '${MOUNT}/py_del2.txt'
print(f'  target: {f}')
print(f'  exists before: {os.path.exists(f)}')
try:
    os.remove(f)
    print(f'  os.remove: ok')
except Exception as e:
    print(f'  os.remove: {e}')
print(f'  exists after: {os.path.exists(f)}')
"

echo ""
echo "--- 7. Create file, then rename via bash mv ---"
echo "rename data" > "$MOUNT/mv_test_src.txt"
mv "$MOUNT/mv_test_src.txt" "$MOUNT/mv_test_dst.txt" 2>&1
echo "  mv exit=$?"
echo "  src exists: $([ -f "$MOUNT/mv_test_src.txt" ] && echo yes || echo no)"
echo "  dst exists: $([ -f "$MOUNT/mv_test_dst.txt" ] && echo yes || echo no)"

echo ""
echo "--- 8. python os.rename ---"
echo "pyrename data" > "$MOUNT/pyren_test_src.txt"
python3 -c "
import os
src = '${MOUNT}/pyren_test_src.txt'
dst = '${MOUNT}/pyren_test_dst.txt'
try:
    os.rename(src, dst)
    print(f'  os.rename: ok')
    print(f'  src exists: {os.path.exists(src)}')
    print(f'  dst exists: {os.path.exists(dst)}')
except Exception as e:
    print(f'  os.rename: {e}')
"

# Cleanup
rm -f "$MOUNT/bash_del.txt" "$MOUNT/py_del2.txt" "$MOUNT/mv_test_src.txt" "$MOUNT/mv_test_dst.txt" "$MOUNT/pyren_test_src.txt" "$MOUNT/pyren_test_dst.txt" 2>/dev/null

echo ""
echo "--- 9. Check mergerfs debug log for unlink/rename calls ---"
grep -E '^\[bridge\] (unlink|rename)' N:/lab/win/02-mergefs/test/mergerfs_debug.log 2>/dev/null | tail -20
