SUMMARY = "Sanath V4L2 Capture Stub Lab 11 Platform Driver and DT Overlay"
LICENSE = "CLOSED"
PACKAGE_ARCH = "${MACHINE_ARCH}"
inherit module

SRC_URI = "file://sanath-v4l2-lab11.c \
           file://Makefile \
           file://sanath-capture-node.dts"

S = "${WORKDIR}"

DTC = "/home/sanath/prep/kernel_dev/yocto-rpi/build-rpi/tmp/sysroots-components/x86_64/dtc-native/usr/bin/dtc"

do_compile:append() {
    ${DTC} -@ -I dts -O dtb -o ${WORKDIR}/sanath-capture-node.dtbo ${WORKDIR}/sanath-capture-node.dts
}

do_install:append() {
    install -d ${D}/boot/overlays
    install -m 0644 ${WORKDIR}/sanath-capture-node.dtbo ${D}/boot/overlays/
}

FILES:${PN} += "/boot/overlays/sanath-capture-node.dtbo"
