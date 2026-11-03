#!/bin/bash
export GST_REGISTRY="$HOME/test/gst-registry-mix.bin"
export GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
F="$1"
run() { n="$1"; shift
  timeout 12 stdbuf -oL gst-launch-1.0 -v filesrc location="$F" ! matroskademux name=d "$@" \
    > "$HOME/test/bis-$n.log" 2>&1
  echo "$n: PREROLLED=$(grep -c PREROLLED "$HOME/test/bis-$n.log") $(grep last-message "$HOME/test/bis-$n.log" | tail -n 1 | grep -oE "rendered: [0-9]+") ERRORS=$(grep -c ERROR "$HOME/test/bis-$n.log")"
}
V="d.video_0 ! h265parse ! v4l2slh265dec ! fpsdisplaysink text-overlay=false video-sink=fakesink"
VQ="d.video_0 ! queue ! h265parse ! v4l2slh265dec ! fpsdisplaysink text-overlay=false video-sink=fakesink"
run A_video $V
run B_video_queue $VQ
run C_av_fakesink $V d.audio_0 ! aacparse ! faad ! fakesink
run D_av_hdmi $V d.audio_0 ! aacparse ! faad ! audioconvert ! audioresample ! alsasink device=plughw:1,0
run E_av_queues $VQ d.audio_0 ! queue ! aacparse ! faad ! fakesink
run F_audio_only d.audio_0 ! aacparse ! faad ! fakesink
run G_av_raw_audio $V d.audio_0 ! fakesink
run H_av_raw_audio_queues $VQ d.audio_0 ! queue ! fakesink
run I_av_aacparse $V d.audio_0 ! aacparse ! fakesink
run M_raw_both d.video_0 ! fakesink d.audio_0 ! fakesink
run N_raw_h265parse d.video_0 ! h265parse ! fakesink d.audio_0 ! fakesink
run J_av_huge_aq $V d.audio_0 ! queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 ! fakesink
