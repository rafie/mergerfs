#!/bin/bash
# Test glob branch spec expansion
set -e

MERGERFS="N:/lab/win/02-mergefs/mergerfs/build/mergerfs.exe"

echo "=== Setup ==="
rm -rf /c/temp/mergerfs_glob_1 /c/temp/mergerfs_glob_2
mkdir -p /c/temp/mergerfs_glob_1 /c/temp/mergerfs_glob_2
echo "g1 content" > /c/temp/mergerfs_glob_1/g1.txt
echo "g2 content" > /c/temp/mergerfs_glob_2/g2.txt
echo "OK: created glob dirs"

echo ""
echo "=== Start mergerfs with glob pattern ==="
taskkill //F //IM mergerfs.exe 2>/dev/null || true
sleep 2

MSYS2_ARG_CONV_EXCL="*" "$MERGERFS" -o allow_other,category.create=ff "/c/temp/mergerfs_glob_*" /m 2>/tmp/mergerfs_glob.log &
sleep 3

echo ""
echo "=== Debug log (first 20 lines) ==="
head -20 /tmp/mergerfs_glob.log

echo ""
echo "=== Mount listing ==="
ls M:/ 2>&1 || echo "FAIL: M:/ not accessible"

echo ""
echo "=== Content checks ==="
if [ -f "M:/g1.txt" ]; then
    echo "PASS: g1.txt visible"
    cat M:/g1.txt
else
    echo "FAIL: g1.txt not visible"
fi

if [ -f "M:/g2.txt" ]; then
    echo "PASS: g2.txt visible"
    cat M:/g2.txt
else
    echo "FAIL: g2.txt not visible"
fi

echo ""
echo "=== Cleanup ==="
taskkill //F //IM mergerfs.exe 2>/dev/null || true
sleep 2
rm -rf /c/temp/mergerfs_glob_1 /c/temp/mergerfs_glob_2
echo "Done"
