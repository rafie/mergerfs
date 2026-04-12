#!/bin/bash
# Phase 2 Tier 2 — Read-Write Operations Test Suite
# Usage: MSYS2_ARG_CONV_EXCL="*" bash test_tier2.sh [mountpoint]
#
# Prerequisites:
#   - mergerfs mounted at the given mountpoint (default: M:/)
#   - At least one branch with write access

set -u
MOUNT="${1:-M:}"
PASS=0
FAIL=0
SKIP=0

pass() { echo "  PASS: $1"; ((PASS++)); }
fail() { echo "  FAIL: $1"; ((FAIL++)); }
skip() { echo "  SKIP: $1"; ((SKIP++)); }

cleanup() {
  rm -f "$MOUNT/tier2_test.txt" "$MOUNT/tier2_renamed.txt" 2>/dev/null
  rm -f "$MOUNT/tier2_dir/nested.txt" 2>/dev/null
  rm -f "$MOUNT/tier2_dir/subdir/deep.txt" 2>/dev/null
  rmdir "$MOUNT/tier2_dir/subdir" 2>/dev/null
  rmdir "$MOUNT/tier2_dir" 2>/dev/null
  rm -f "$MOUNT/tier2_trunc.txt" 2>/dev/null
  rm -f "$MOUNT/tier2_large.bin" 2>/dev/null
}

echo "=== Phase 2 Tier 2: Read-Write Operations ==="
echo "Mount: $MOUNT"
echo ""

# Clean up from any previous run
cleanup

# ------------------------------------------------------------------
echo "--- 1. Create new file ---"
echo "hello world" > "$MOUNT/tier2_test.txt" 2>&1
if [ $? -eq 0 ] && [ -f "$MOUNT/tier2_test.txt" ]; then
  pass "create file"
else
  fail "create file"
fi

# ------------------------------------------------------------------
echo "--- 2. Read file ---"
content=$(cat "$MOUNT/tier2_test.txt" 2>&1)
if [ "$content" = "hello world" ]; then
  pass "read file"
else
  fail "read file (got: '$content')"
fi

# ------------------------------------------------------------------
echo "--- 3. Overwrite file (truncate + write) ---"
echo "overwritten" > "$MOUNT/tier2_test.txt" 2>&1
if [ $? -eq 0 ]; then
  content=$(cat "$MOUNT/tier2_test.txt" 2>&1)
  if [ "$content" = "overwritten" ]; then
    pass "overwrite file"
  else
    fail "overwrite file (got: '$content')"
  fi
else
  fail "overwrite file (open for write failed)"
fi

# ------------------------------------------------------------------
echo "--- 4. Append to file ---"
echo "appended" >> "$MOUNT/tier2_test.txt" 2>&1
if [ $? -eq 0 ]; then
  lines=$(wc -l < "$MOUNT/tier2_test.txt" 2>/dev/null)
  if [ "$lines" -ge 2 ]; then
    pass "append to file"
  else
    fail "append to file (expected 2+ lines, got $lines)"
  fi
else
  fail "append to file"
fi

# ------------------------------------------------------------------
echo "--- 5. Create directory ---"
mkdir "$MOUNT/tier2_dir" 2>&1
if [ $? -eq 0 ] && [ -d "$MOUNT/tier2_dir" ]; then
  pass "mkdir"
else
  fail "mkdir"
fi

# ------------------------------------------------------------------
echo "--- 6. Create file in subdirectory ---"
echo "nested content" > "$MOUNT/tier2_dir/nested.txt" 2>&1
if [ $? -eq 0 ]; then
  content=$(cat "$MOUNT/tier2_dir/nested.txt" 2>&1)
  if [ "$content" = "nested content" ]; then
    pass "create file in subdir"
  else
    fail "create file in subdir (got: '$content')"
  fi
else
  fail "create file in subdir"
fi

# ------------------------------------------------------------------
echo "--- 7. Create nested subdirectory ---"
mkdir "$MOUNT/tier2_dir/subdir" 2>&1
if [ $? -eq 0 ] && [ -d "$MOUNT/tier2_dir/subdir" ]; then
  echo "deep" > "$MOUNT/tier2_dir/subdir/deep.txt" 2>&1
  if [ $? -eq 0 ]; then
    pass "nested mkdir + create"
  else
    fail "nested mkdir + create (file create failed)"
  fi
else
  fail "nested mkdir"
fi

# ------------------------------------------------------------------
echo "--- 8. Rename file ---"
mv "$MOUNT/tier2_test.txt" "$MOUNT/tier2_renamed.txt" 2>&1
if [ $? -eq 0 ]; then
  if [ ! -f "$MOUNT/tier2_test.txt" ] && [ -f "$MOUNT/tier2_renamed.txt" ]; then
    pass "rename file"
  else
    fail "rename file (old still exists or new missing)"
  fi
else
  fail "rename file"
fi

# ------------------------------------------------------------------
echo "--- 9. Delete file (unlink) ---"
rm "$MOUNT/tier2_renamed.txt" 2>&1
if [ $? -eq 0 ] && [ ! -f "$MOUNT/tier2_renamed.txt" ]; then
  pass "unlink"
else
  fail "unlink"
fi

# ------------------------------------------------------------------
echo "--- 10. Delete non-empty directory tree ---"
rm "$MOUNT/tier2_dir/subdir/deep.txt" 2>&1
rmdir "$MOUNT/tier2_dir/subdir" 2>&1
rm "$MOUNT/tier2_dir/nested.txt" 2>&1
rmdir "$MOUNT/tier2_dir" 2>&1
if [ $? -eq 0 ] && [ ! -d "$MOUNT/tier2_dir" ]; then
  pass "rmdir"
else
  fail "rmdir"
fi

# ------------------------------------------------------------------
echo "--- 11. Truncate file ---"
echo "some data here" > "$MOUNT/tier2_trunc.txt" 2>&1
truncate -s 5 "$MOUNT/tier2_trunc.txt" 2>&1
if [ $? -eq 0 ]; then
  size=$(stat -c%s "$MOUNT/tier2_trunc.txt" 2>/dev/null || stat -f%z "$MOUNT/tier2_trunc.txt" 2>/dev/null)
  if [ "$size" = "5" ]; then
    pass "truncate"
  else
    fail "truncate (expected size 5, got $size)"
  fi
else
  skip "truncate (truncate command not available or failed)"
fi
rm -f "$MOUNT/tier2_trunc.txt" 2>/dev/null

# ------------------------------------------------------------------
echo "--- 12. Large file write (1 MB) ---"
dd if=/dev/zero of="$MOUNT/tier2_large.bin" bs=1024 count=1024 2>/dev/null
if [ $? -eq 0 ]; then
  size=$(stat -c%s "$MOUNT/tier2_large.bin" 2>/dev/null || stat -f%z "$MOUNT/tier2_large.bin" 2>/dev/null)
  if [ "$size" = "1048576" ]; then
    pass "large file write (1 MB)"
  else
    fail "large file write (expected 1048576, got $size)"
  fi
else
  fail "large file write"
fi
rm -f "$MOUNT/tier2_large.bin" 2>/dev/null

# ------------------------------------------------------------------
echo ""
echo "=== Results ==="
echo "PASS: $PASS  FAIL: $FAIL  SKIP: $SKIP"
if [ $FAIL -eq 0 ]; then
  echo "All tests passed!"
  exit 0
else
  echo "Some tests failed."
  exit 1
fi
