SUMMARY = "DPI panel overlay with the correct physical size for the 2.8 inch panel"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
SRC_URI = "file://vc4-kms-dpi-sanath.dts"
S = "${WORKDIR}"

inherit deploy
DEPENDS = "dtc-native"
PACKAGE_ARCH = "${MACHINE_ARCH}"

do_compile() {
    dtc -I dts -O dtb -o ${B}/vc4-kms-dpi-sanath.dtbo ${WORKDIR}/vc4-kms-dpi-sanath.dts
}

do_deploy() {
    install -m 0644 ${B}/vc4-kms-dpi-sanath.dtbo ${DEPLOYDIR}/
}
addtask deploy after do_compile before do_build
