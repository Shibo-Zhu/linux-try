#!/bin/bash
set -euo pipefail

SYSFS="/sys/kernel/alloc_demo"
PROC="/proc/alloc_demo"

if [ ! -d "$SYSFS" ]; then
  echo "sysfs entry $SYSFS not found. Is module loaded?"
  exit 1
fi

echo "=== initial /proc/alloc_demo ==="
cat $PROC
echo

# helper to trigger
do_test() {
  local mode=$1
  local size=$2
  local count=$3

  echo "Setting mode=$mode size=$size count=$count"
  echo "$mode" > ${SYSFS}/mode
  echo "$size" > ${SYSFS}/size
  echo "$count" > ${SYSFS}/count

  echo "Allocating..."
  echo "alloc" > ${SYSFS}/action
  echo "After allocation (module proc):"
  cat $PROC
  echo

  # quick slabinfo peek for kmalloc
  if [ "$mode" = "kmalloc" ]; then
    echo "Some kmalloc-* slabinfo lines (size near $size):"
    grep -E "kmalloc|slub" /proc/slabinfo 2>/dev/null | head -n 20 || true
    echo
  else
    echo "(skipping slabinfo special for non-kmalloc)"
  fi

  # For kpageflags: show the virtual addresses we allocated (from /proc output)
  echo "Check /proc/alloc_demo pointers to correlate to /proc/kpageflags (requires root and interpretation):"
  # Print pointer addresses
  awk '/active allocation records:/{flag=1;next} /---- end ----/{flag=0} flag && /ptr=/{print $0}' $PROC || true
  echo

  echo "Freeing..."
  echo "free" > ${SYSFS}/action
  echo "After free (module proc):"
  cat $PROC
  echo
}

echo "=== Test 1: small kmalloc (128 bytes x 10) ==="
do_test kmalloc 128 10

echo "=== Test 2: larger kmalloc (4096 bytes x 4) ==="
do_test kmalloc 4096 4

echo "=== Test 3: alloc_pages (8 KiB) ==="
do_test alloc_pages 8192 2

echo "=== Test 4: vmalloc (1 MiB) ==="
do_test vmalloc $((1024*1024)) 1

echo "=== Done ==="
