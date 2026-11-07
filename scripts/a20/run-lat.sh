#!/bin/bash
# usage: run-lat.sh NAME FILE SECONDS 'middle elements ending with ! (may be empty)' 'extra kmssink props (may be empty)'
mid="${4:-}"
extra="${5:-}"
( for i in $(seq 1 $(( $3 - 2 ))); do awk '/CmaFree/ {print $2}' /proc/meminfo; sleep 1; done > "$HOME/test/$1.cma" ) &
sudo env GST_REGISTRY="$HOME/test/gst-registry-mix-root.bin" GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix" GST_DEBUG=basesink:5 timeout "$3" gst-launch-1.0 filesrc location="$2" ! qtdemux name=d d.video_0 ! h264parse ! v4l2slh264dec ! $mid kmssink driver-name=sun4i-drm plane-id=37 skip-vsync=true qos=false $extra > "$HOME/test/$1.log" 2>&1
wait
grep 'clock returned' "$HOME/test/$1.log" | awk '{j=$NF; s=1; c=substr(j,1,1); if (c=="-") {s=-1; j=substr(j,2)}; split(j,a,":"); print s*(a[1]*3600+a[2]*60+a[3])*1000}' | sort -n | awk '{v[NR]=$1; if ($1>0) l0++; if ($1>20) l20++; if ($1>40) l40++} END {printf "n=%d min=%.1f p50=%.1f p90=%.1f p99=%.1f max=%.1f ms; late>0: %d, >20ms: %d, >40ms: %d\n", NR, v[1], v[int(NR*0.5)], v[int(NR*0.9)], v[int(NR*0.99)], v[NR], l0, l20, l40}'
echo "CmaFree min kB: $(sort -n "$HOME/test/$1.cma" | head -n 1)"
