SUMMARY = "Write boot diagnostics to /home/root/boot-diag.txt"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://boot-diag.sh file://boot-diag.service"
S = "${WORKDIR}"

inherit systemd
SYSTEMD_SERVICE:${PN} = "boot-diag.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    install -d ${D}${bindir} ${D}${systemd_system_unitdir}
    install -m 0755 ${WORKDIR}/boot-diag.sh ${D}${bindir}/
    install -m 0644 ${WORKDIR}/boot-diag.service ${D}${systemd_system_unitdir}/
}
