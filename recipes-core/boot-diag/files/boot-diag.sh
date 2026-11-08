#!/bin/sh
OUT=/home/root/boot-diag.txt
sleep 40
{
echo "== uname"; uname -a
echo "== analyze time"; systemd-analyze time
echo "== analyze blame"; systemd-analyze blame --no-pager
echo "== analyze critical-chain"; systemd-analyze critical-chain --no-pager
echo "== dmesg"; dmesg
echo "== journal"; journalctl -b --no-pager
echo "== ip"; ip addr
echo "== iw"; iw dev
echo "== scan"; iw dev wlan0 scan || { sleep 3; iw dev wlan0 scan; }
echo "== units"; systemctl status wpa_supplicant@wlan0.service systemd-networkd.service sshd.socket --no-pager
echo "== drm"; ls /dev/dri; modetest -M vc4 -c | head -60
} > "$OUT" 2>&1
sync
