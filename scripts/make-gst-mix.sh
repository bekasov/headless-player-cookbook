#!/bin/bash
rm -rf "$HOME/gst-mix"; mkdir -p "$HOME/gst-mix"
for f in /usr/lib/aarch64-linux-gnu/gstreamer-1.0/*.so; do
  [ "$(basename "$f")" = libgstv4l2codecs.so ] || ln -s "$f" "$HOME/gst-mix/"
done
ln -s "$HOME/build-gst/gst-plugins-bad-1.26.11/build/sys/v4l2codecs/libgstv4l2codecs.so" "$HOME/gst-mix/"
ls "$HOME/gst-mix" | wc -l
