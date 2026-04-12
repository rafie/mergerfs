#!/bin/bash
# Test POSIX-semantics unlink and rename through mergerfs mount.
# These operations previously failed with EACCES because mergerfs holds files open
# and Windows CRT _unlink/_rename don't work on files with open handles.
# Fix: use SetFileInformationByHandle with FileDispositionInfoEx/FileRenameInfoEx.
#
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_posix_unlink_rename.sh [mountpoint]

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

echo "=== POSIX-semantics unlink & rename tests ==="
echo "Mount: $MOUNT"
echo ""

# --- Unlink tests ---
echo "--- 1. Create file, then unlink ---"
echo "hello" > "$MOUNT/unlink1.txt"
[ -f "$MOUNT/unlink1.txt" ] && echo "  created ok"
rm "$MOUNT/unlink1.txt" 2>&1
rmok=$?
check "rm exit code is 0" "$([ $rmok -eq 0 ] && echo 1 || echo 0)"
check "file gone after rm" "$([ ! -f "$MOUNT/unlink1.txt" ] && echo 1 || echo 0)"

echo ""
echo "--- 2. Create file via python, unlink via python ---"
python3 -c "
import os
f = '${MOUNT}/unlink2.txt'
with open(f, 'w') as fh:
    fh.write('py test\n')
print(f'  created: {os.path.exists(f)}')
os.remove(f)
print(f'  removed: {not os.path.exists(f)}')
print('OK')
" 2>&1
pyok=$?
check "python unlink succeeded" "$([ $pyok -eq 0 ] && echo 1 || echo 0)"

# --- Rename tests ---
echo ""
echo "--- 3. bash mv (rename) ---"
echo "rename me" > "$MOUNT/ren_src.txt"
mv "$MOUNT/ren_src.txt" "$MOUNT/ren_dst.txt" 2>&1
mvok=$?
check "mv exit code is 0" "$([ $mvok -eq 0 ] && echo 1 || echo 0)"
check "source gone after mv" "$([ ! -f "$MOUNT/ren_src.txt" ] && echo 1 || echo 0)"
check "dest exists after mv" "$([ -f "$MOUNT/ren_dst.txt" ] && echo 1 || echo 0)"
# Read back content
content=$(cat "$MOUNT/ren_dst.txt" 2>/dev/null)
check "dest content correct" "$([ "$content" = "rename me" ] && echo 1 || echo 0)"
rm -f "$MOUNT/ren_dst.txt" 2>/dev/null

echo ""
echo "--- 4. python os.rename ---"
echo "pyrename" > "$MOUNT/pyren_src.txt"
python3 -c "
import os
src = '${MOUNT}/pyren_src.txt'
dst = '${MOUNT}/pyren_dst.txt'
os.rename(src, dst)
print(f'  src exists: {os.path.exists(src)}')
print(f'  dst exists: {os.path.exists(dst)}')
with open(dst) as f:
    print(f'  content: {f.read().strip()}')
print('OK')
" 2>&1
pyrenok=$?
check "python rename succeeded" "$([ $pyrenok -eq 0 ] && echo 1 || echo 0)"
rm -f "$MOUNT/pyren_dst.txt" 2>/dev/null

echo ""
echo "--- 5. rename with overwrite (target exists) ---"
echo "original" > "$MOUNT/overwrite_dst.txt"
echo "replacement" > "$MOUNT/overwrite_src.txt"
mv "$MOUNT/overwrite_src.txt" "$MOUNT/overwrite_dst.txt" 2>&1
mvok2=$?
content2=$(cat "$MOUNT/overwrite_dst.txt" 2>/dev/null)
check "overwrite rename exit 0" "$([ $mvok2 -eq 0 ] && echo 1 || echo 0)"
check "overwrite content correct" "$([ "$content2" = "replacement" ] && echo 1 || echo 0)"
rm -f "$MOUNT/overwrite_dst.txt" 2>/dev/null

echo ""
echo "--- 6. rmdir (should still work) ---"
mkdir "$MOUNT/rmdir_test" 2>/dev/null
[ -d "$MOUNT/rmdir_test" ] && echo "  dir created"
rmdir "$MOUNT/rmdir_test" 2>&1
rmdok=$?
check "rmdir exit 0" "$([ $rmdok -eq 0 ] && echo 1 || echo 0)"
check "dir gone" "$([ ! -d "$MOUNT/rmdir_test" ] && echo 1 || echo 0)"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
