SUMMARY = "Message Queue Kernel Module"
LICENSE = "CLOSED"

inherit module

SRC_URI = "file://01_msg_queue_driver.c \
           file://Makefile"

S = "${WORKDIR}"
