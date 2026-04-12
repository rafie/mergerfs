#!/bin/bash
# Test that mergerfs reports unique inodes for different files.
# Previously all files got the same inode (st_ino=0 from _stat64).
# Fix: use GetFileInformationByHandle to get NTFS file IDs.
#
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_inode_uniqueness.sh [mountpoint]

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

echo "=== Inode uniqueness tests ==="
echo "Mount: $MOUNT"
echo ""

# --- Test 1: Two files get different inodes ---
echo "--- 1. Two different files have distinct inodes ---"
echo "file A" > "$MOUNT/ino_a.txt"
echo "file B" > "$MOUNT/ino_b.txt"
ino_a=$(stat -c%i "$MOUNT/ino_a.txt" 2>/dev/null)
ino_b=$(stat -c%i "$MOUNT/ino_b.txt" 2>/dev/null)
echo "  ino_a=$ino_a"
echo "  ino_b=$ino_b"
check "inodes are different" "$([ "$ino_a" != "$ino_b" ] && echo 1 || echo 0)"

# --- Test 2: File and directory get different inodes ---
echo ""
echo "--- 2. File and directory have distinct inodes ---"
mkdir "$MOUNT/ino_dir" 2>/dev/null
ino_f=$(stat -c%i "$MOUNT/ino_a.txt" 2>/dev/null)
ino_d=$(stat -c%i "$MOUNT/ino_dir" 2>/dev/null)
echo "  ino_file=$ino_f"
echo "  ino_dir=$ino_d"
check "file vs dir inodes differ" "$([ "$ino_f" != "$ino_d" ] && echo 1 || echo 0)"

# --- Test 3: Inode is non-zero ---
echo ""
echo "--- 3. Inodes are non-zero ---"
check "ino_a non-zero" "$([ "$ino_a" != "0" ] && echo 1 || echo 0)"
check "ino_b non-zero" "$([ "$ino_b" != "0" ] && echo 1 || echo 0)"

# --- Test 4: mv overwrite works (requires distinct inodes) ---
echo ""
echo "--- 4. mv overwrite (requires distinct inodes) ---"
echo "original" > "$MOUNT/mv_ow_dst.txt"
echo "replacement" > "$MOUNT/mv_ow_src.txt"
mv "$MOUNT/mv_ow_src.txt" "$MOUNT/mv_ow_dst.txt" 2>&1
mvok=$?
content=$(cat "$MOUNT/mv_ow_dst.txt" 2>/dev/null)
check "mv overwrite exit 0" "$([ $mvok -eq 0 ] && echo 1 || echo 0)"
check "mv overwrite content correct" "$([ "$content" = "replacement" ] && echo 1 || echo 0)"

# Cleanup
rm -f "$MOUNT/ino_a.txt" "$MOUNT/ino_b.txt" "$MOUNT/mv_ow_src.txt" "$MOUNT/mv_ow_dst.txt" 2>/dev/null
rmdir "$MOUNT/ino_dir" 2>/dev/null

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
