SUMMARY = "Minimal image to bring up the Waveshare DPI display"
LICENSE = "MIT"

inherit image

IMAGE_FEATURES = ""
IMAGE_INSTALL = " \
    packagegroup-core-boot \
    kernel-modules \
    libdrm-tests \
    wpa-supplicant \
    linux-firmware-rpidistro-bcm43430 \
    wireless-regdb-static \
    iw \
    openssh \
    wifi-config \
    avahi-daemon \
    boot-diag \
    systemd-analyze \
    root-ssh-key \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    alsa-utils \
    boot-video \
"

IMAGE_BOOT_FILES:append = " vc4-kms-dpi-sanath.dtbo;overlays/vc4-kms-dpi-sanath.dtbo"
do_image_wic[depends] += "sanath-dpi-overlay:do_deploy"
