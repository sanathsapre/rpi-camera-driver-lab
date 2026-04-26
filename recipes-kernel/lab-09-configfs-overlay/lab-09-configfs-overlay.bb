SUMMARY = "Sanath Lab 09 DT Overlay with configfs"
LICENSE = "CLOSED"

PACKAGE_ARCH = "${MACHINE_ARCH}"

SRC_URI = "file://sanath-gpio-keys.dts"
S = "${WORKDIR}"
DTC = "/home/sanath/prep/kernel_dev/yocto-rpi/build-rpi/tmp/sysroots-components/x86_64/dtc-native/usr/bin/dtc"
do_compile_dtbo() {
    ${DTC} -@ -I dts -O dtb -o ${WORKDIR}/sanath-gpio-keys.dtbo ${WORKDIR}/sanath-gpio-keys.dts
}
addtask do_compile_dtbo after do_unpack before do_install
do_install:append() {
    install -d ${D}/boot/overlays
    install -m 0644 ${WORKDIR}/sanath-gpio-keys.dtbo ${D}/boot/overlays/
}
do_deploy() {
    install -d ${DEPLOYDIR}
    install -m 0644 ${WORKDIR}/sanath-gpio-keys.dtbo ${DEPLOYDIR}/
}
addtask do_deploy after do_compile_dtbo before do_build
inherit deploy
FILES:${PN} += "/boot/overlays/sanath-gpio-keys.dtbo"
