#!/bin/bash
export GST_REGISTRY="$HOME/test/gst-registry-mix.bin"
export GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
Q="queue max-size-buffers=0 max-size-bytes=0 max-size-time=10000000000"
{ time cage -- timeout 80 stdbuf -oL gst-launch-1.0 -v filesrc location="$1" \
    ! matroskademux name=d \
    d.video_0 ! $Q ! h265parse ! v4l2slh265dec \
      ! fpsdisplaysink text-overlay=false video-sink=waylandsink \
    d.audio_0 ! $Q ! aacparse ! faad ! audioconvert ! audioresample \
      ! alsasink device=plughw:1,0 ; } > "$HOME/test/play-av-$(basename "$1").log" 2>&1
