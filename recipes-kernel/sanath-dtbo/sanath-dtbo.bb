SUMMARY = "Sanath IRQ Lab Platform Driver with DT Overlay"
LICENSE = "CLOSED"
inherit module

SRC_URI = "file://sanath-irq-lab.c \
           file://Makefile \
           file://sanath-irq-lab.dts"

S = "${WORKDIR}"

DTC = "/home/sanath/prep/kernel_dev/yocto-rpi/build-rpi/tmp/sysroots-components/x86_64/dtc-native/usr/bin/dtc"

do_compile_dtbo() {
    ${DTC} -@ -I dts -O dtb -o ${WORKDIR}/sanath-irq-lab.dtbo ${WORKDIR}/sanath-irq-lab.dts
}

addtask do_compile_dtbo after do_compile before do_install

do_install:append() {
    install -d ${D}/boot/overlays
    install -m 0644 ${WORKDIR}/sanath-irq-lab.dtbo ${D}/boot/overlays/
}

FILES:${PN} += "/boot/overlays/sanath-irq-lab.dtbo"
