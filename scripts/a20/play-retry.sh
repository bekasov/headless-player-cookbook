#!/bin/bash
# usage: play-retry.sh FILE   (play.sh with up to 3 attempts on "Not enough memory" at start)
f="$1"; log="$(mktemp)"
for try in 1 2 3; do
  start=$SECONDS
  "$HOME/bpi-m1-notes/play.sh" "$f" 2> "$log"
  rc=$?
  if [ "$rc" -ne 0 ] && grep -q 'Not enough memory' "$log" && [ $((SECONDS - start)) -lt 10 ]; then
    echo "$(date '+%F %T') attempt $try failed: not enough memory ($f)" >> "$HOME/bpi-m1-notes/retry.log"
    sleep 1
    continue
  fi
  break
done
cat "$log" >&2
rm -f "$log"
exit "$rc"
