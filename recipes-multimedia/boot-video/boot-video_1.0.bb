SUMMARY = "Play a video in a loop at boot on the DPI panel"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
SRC_URI = "file://boot-video.sh \
           file://boot-video.service \
           file://video.conf \
           file://test_480x640_h264_aac.mp4"
S = "${WORKDIR}"

inherit systemd
SYSTEMD_SERVICE:${PN} = "boot-video.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    install -d ${D}${bindir} ${D}${systemd_system_unitdir} ${D}${sysconfdir}/modules-load.d ${D}${ROOT_HOME}
    install -m 0755 ${WORKDIR}/boot-video.sh ${D}${bindir}/boot-video.sh
    install -m 0644 ${WORKDIR}/boot-video.service ${D}${systemd_system_unitdir}/boot-video.service
    install -m 0644 ${WORKDIR}/video.conf ${D}${sysconfdir}/modules-load.d/video.conf
    install -m 0644 ${WORKDIR}/test_480x640_h264_aac.mp4 ${D}${ROOT_HOME}/
}

FILES:${PN} += "${ROOT_HOME}"
RDEPENDS:${PN} = "libdrm-tests"
