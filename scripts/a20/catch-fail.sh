#!/bin/bash
# usage: catch-fail.sh FILE N  (repeat 4-second starts, stop at the first "Not enough memory")
f="$1"; n="$2"
for i in $(seq 1 "$n"); do
  before=$(awk '/CmaFree/ {print $2}' /proc/meminfo)
  sudo sh -c "echo MARK-run-$i > /dev/kmsg"
  GST_DEBUG='v4l2codecs*:5' timeout 4 "$HOME/bpi-m1-notes/play.sh" "$f" > "$HOME/test/catch.log" 2>&1
  if grep -q 'Not enough memory' "$HOME/test/catch.log"; then
    echo "run $i of $n: FAIL, CmaFree before=$before kB"
    grep -B6 -m1 'Not enough memory' "$HOME/test/catch.log"
    echo "--- kernel log since the mark:"
    sudo dmesg | sed -n "/MARK-run-$i\$/,\$p"
    echo "--- CMA free blocks:"
    sudo grep ' CMA ' /proc/pagetypeinfo
    cp "$HOME/test/catch.log" "$HOME/test/catch-fail.log"
    exit 1
  fi
done
echo "no failures in $n runs"
