# rpi-camera-driver-lab

RPi3 camera driver series — built to understand what actually happens when a
camera frame moves through a Linux system. Each driver adds one layer of
understanding to the last.

This is the Yocto/BSP layer (`meta-sanath`) for the series. Kernel module
source, DT overlays, and build recipes live here.

Continues from: https://github.com/sanathsapre/linux-driver-lab

## Build Environment

Yocto release: Kirkstone

### Layer dependencies

Clone the following at the same level as this layer:

    poky:              git clone https://git.yoctoproject.org/poky
                       git checkout cf615e1d

    meta-raspberrypi:  git clone https://github.com/agherzan/meta-raspberrypi
                       git checkout 255500dd

    meta-openembedded: git clone https://github.com/openembedded/meta-openembedded
                       git checkout 8a598a2b

### bblayers.conf

Add this layer to your bblayers.conf:

    /path/to/meta-sanath

### local.conf

    MACHINE = "raspberrypi3"

    # Yocto deletes the work directory after every successful build by default
    # to save disk space (INHERIT += "rm_work" in poky's local.conf.sample).
    # If you are actively modifying a recipe and need to inspect build artifacts,
    # add it to RM_WORK_EXCLUDE to prevent deletion:
    #
    # RM_WORK_EXCLUDE += "sanath-dtbo"
    #
    # Remove from the exclude list once the recipe is stable.

## Maintainer

Sanath Kumar P Sapre
https://embeddedforge.org
