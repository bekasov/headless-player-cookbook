#!/bin/bash
export GST_REGISTRY="$HOME/test/gst-registry-mix.bin"
export GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
: > "$HOME/test/cma2.txt"
for f in "$@"; do
  ( while :; do grep CmaFree /proc/meminfo; sleep 0.3; done ) > "$HOME/test/cma-samples.tmp" &
  S=$!
  L="$HOME/test/seq-$(basename "$f")-$(date +%s).log"
  cage -- timeout 8 stdbuf -oL gst-launch-1.0 -v filesrc location="$f" ! matroskademux name=d \
    d.video_0 ! h265parse ! v4l2slh265dec ! fpsdisplaysink text-overlay=false video-sink=waylandsink \
    > "$L" 2>&1
  kill $S 2>/dev/null
  echo "$(basename "$f"): min CmaFree $(tr -s " " < "$HOME/test/cma-samples.tmp" | cut -d" " -f2 | sort -n | head -n 1) kB, errors $(grep -c "Not enough memory" "$L")" >> "$HOME/test/cma2.txt"
done
