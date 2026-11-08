#!/bin/sh
FILE=/home/root/test_480x640_h264_aac.mp4
until [ -e /dev/video10 ] && grep -q Headphones /proc/asound/cards && grep -qx connected /sys/class/drm/card0-DPI-1/status 2>/dev/null; do sleep 0.1; done
CID=$(modetest -M vc4 -c | grep -w DPI-1 | cut -f1)
echo "devices ready, starting player (connector $CID)"
exec gst-launch-1.0 -q filesrc location=$FILE ! qtdemux name=d d.video_0 ! queue ! h264parse ! v4l2h264dec ! kmssink driver-name=vc4 connector-id=$CID d.audio_0 ! queue ! avdec_aac ! audioconvert ! audioresample ! alsasink device=plughw:CARD=Headphones,DEV=0
