SUMMARY = "Wi-Fi client config for wlan0 (wpa_supplicant + systemd-networkd)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://wpa_supplicant-wlan0.conf file://25-wlan0.network"
S = "${WORKDIR}"
inherit allarch
RDEPENDS:${PN} = "wpa-supplicant"

do_install() {
    install -d ${D}${sysconfdir}/wpa_supplicant ${D}${sysconfdir}/systemd/network
    install -m 0600 ${WORKDIR}/wpa_supplicant-wlan0.conf ${D}${sysconfdir}/wpa_supplicant/
    install -m 0644 ${WORKDIR}/25-wlan0.network ${D}${sysconfdir}/systemd/network/

    install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants
    ln -sf ${systemd_system_unitdir}/wpa_supplicant@.service \
        ${D}${sysconfdir}/systemd/system/multi-user.target.wants/wpa_supplicant@wlan0.service
    ln -sf ${systemd_system_unitdir}/systemd-networkd.service \
        ${D}${sysconfdir}/systemd/system/multi-user.target.wants/systemd-networkd.service
}
FILES:${PN} += "${sysconfdir}"
