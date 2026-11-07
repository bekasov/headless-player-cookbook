#!/bin/bash
# usage: play.sh FILE   (H.264 mp4/mkv with AC-3 audio; HDMI video+audio)
export GST_REGISTRY="$HOME/.cache/gst-registry-mix.bin" GST_PLUGIN_SYSTEM_PATH_1_0="$HOME/gst-mix"
exec gst-launch-1.0 -q filesrc location="$1" ! qtdemux name=d \
  d.video_0 ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! h264parse ! v4l2slh264dec ! queue max-size-buffers=4 max-size-time=0 max-size-bytes=0 ! kmssink driver-name=sun4i-drm plane-id=37 skip-vsync=true qos=false \
  d.audio_0 ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=2000000000 ! avdec_ac3 ! audioconvert ! audio/x-raw,channels=2 ! audioresample ! alsasink device=plughw:CARD=sun4ihdmi,DEV=0
