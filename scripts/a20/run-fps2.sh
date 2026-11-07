#!/bin/bash
# usage: run-fps2.sh NAME SECONDS 'video-sink string' SYNC(true|false)
export GST_REGISTRY="$HOME/test/gst-registry-mix-root.bin" GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
vmstat 1 $(( $2 - 2 )) > "$HOME/test/$1.vm" &
( time sudo env GST_REGISTRY="$GST_REGISTRY" GST_PLUGIN_SYSTEM_PATH_1_0="$GST_PLUGIN_SYSTEM_PATH_1_0" timeout "$2" stdbuf -oL gst-launch-1.0 -v filesrc location="$HOME/film_test.mp4" ! qtdemux name=d d.video_0 ! h264parse ! v4l2slh264dec ! fpsdisplaysink text-overlay=false sync="$4" video-sink="$3" > "$HOME/test/$1.log" 2>&1 ) 2> "$HOME/test/$1.time"
wait
