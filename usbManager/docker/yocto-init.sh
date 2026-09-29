#!/bin/bash
# Download pinned Poky + BSP layers and create the build directory.
# Safe to run on every container start: an existing checkout stays in
# place unless layers.lock asks for a different revision and the tree
# has no local changes.
set -euo pipefail

MCU_SRC=/work/mcu-dev
YOCTO_ROOT=/work/yocto
BUILD_DIR="${YOCTO_ROOT}/build"
LOCK=/opt/yocto/layers.lock
MARKER="${BUILD_DIR}/conf/.mcu-dev-yocto"

if [[ ! -d "${MCU_SRC}/meta-connectivity" || ! -d "${MCU_SRC}/usb-manager" ]]; then
    echo "error: ${MCU_SRC} is not the mcu-dev tree (meta-connectivity and usb-manager are required)." >&2
    exit 1
fi

if [[ ! -w "${YOCTO_ROOT}" ]]; then
    echo "error: ${YOCTO_ROOT} is not writable by $(id -un)." >&2
    echo "On the host, run: mkdir -p yocto && ./scripts/yocto.sh" >&2
    exit 1
fi

avail_kb=$(df -Pk "${YOCTO_ROOT}" | awk 'NR==2 {print $4}')
if [[ -n "${avail_kb}" && "${avail_kb}" -lt 62914560 ]]; then
    echo "warning: less than 60 GiB free on the Yocto volume. A Raspberry Pi image build often needs 50–100 GiB." >&2
fi

sync_layer() {
    local name="$1" url="$2" rev="$3"
    local dest="${YOCTO_ROOT}/${name}"

    if [[ ! -d "${dest}/.git" ]]; then
        echo "==> cloning ${name} (branch scarthgap)"
        rm -rf "${dest}"
        git clone --branch scarthgap --single-branch "${url}" "${dest}"
    fi

    local head=""
    head=$(git -C "${dest}" rev-parse HEAD 2>/dev/null || true)
    if [[ "${head}" == "${rev}" ]]; then
        echo "==> ${name} is at ${rev}"
        return
    fi

    if [[ -n "$(git -C "${dest}" status --porcelain 2>/dev/null || true)" ]]; then
        echo "warning: ${name} has local changes and is not at ${rev}; left untouched." >&2
        return
    fi

    echo "==> checking out ${name} ${rev}"
    if ! git -C "${dest}" cat-file -e "${rev}^{commit}" 2>/dev/null; then
        git -C "${dest}" fetch --depth 1 origin "${rev}" || git -C "${dest}" fetch origin scarthgap
    fi
    git -C "${dest}" checkout --detach --force "${rev}"
    git -C "${dest}" config advice.detachedHead false
}

while read -r name url rev label; do
    [[ -z "${name}" || "${name}" == \#* ]] && continue
    sync_layer "${name}" "${url}" "${rev}"
    echo "    ${label}"
done < "${LOCK}"

# Compose may create the build directory as root via working_dir before
# the entrypoint drops to the builder user. Take it back when it is empty
# or already ours so conf can be written.
if [[ -d "${BUILD_DIR}" && ! -w "${BUILD_DIR}" ]]; then
    sudo chown "$(id -u):$(id -g)" "${BUILD_DIR}"
fi
mkdir -p "${BUILD_DIR}/conf" "${YOCTO_ROOT}/downloads" "${YOCTO_ROOT}/sstate-cache"

if [[ ! -f "${MARKER}" ]]; then
    echo "==> writing ${BUILD_DIR}/conf (raspberrypi4-64, systemd, SSH prototype login)"
    cat > "${BUILD_DIR}/conf/bblayers.conf" <<EOF
# Generated for the mcu-dev connectivity prototype. Regenerated only when
# this file is missing. Add extra layers here; they are kept across starts.
POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "\${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \\
  ${YOCTO_ROOT}/poky/meta \\
  ${YOCTO_ROOT}/poky/meta-poky \\
  ${YOCTO_ROOT}/meta-openembedded/meta-oe \\
  ${YOCTO_ROOT}/meta-openembedded/meta-python \\
  ${YOCTO_ROOT}/meta-openembedded/meta-networking \\
  ${YOCTO_ROOT}/meta-raspberrypi \\
  ${MCU_SRC}/meta-connectivity \\
  "
EOF

    cat > "${BUILD_DIR}/conf/local.conf" <<'EOF'
# mcu-dev — Raspberry Pi 4 (64-bit) connectivity prototype.
# Edit this file freely; container start will not overwrite it.
MACHINE ?= "raspberrypi4-64"
DISTRO ?= "poky"

# usb-manager is a systemd unit. INIT_MANAGER switches Poky off sysvinit.
INIT_MANAGER = "systemd"

# GPIO 14/15 serial console, 115200. Use this if Ethernet/SSH is not up yet.
ENABLE_UART = "1"

# Cypress/Synaptics Wi-Fi and Bluetooth firmware shipped by meta-raspberrypi.
# The board image will not build without this flag.
LICENSE_FLAGS_ACCEPTED += "synaptics-killswitch"

# Prototype login after flashing: ssh root@<board-ip> with an empty password.
# Set a real password before this image leaves the bench.
EXTRA_IMAGE_FEATURES ?= "debug-tweaks ssh-server-openssh"

DL_DIR = "${TOPDIR}/../downloads"
SSTATE_DIR = "${TOPDIR}/../sstate-cache"
PACKAGE_CLASSES ?= "package_rpm"
SDKMACHINE ?= "x86_64"

BB_DISKMON_DIRS ??= "\
    STOPTASKS,${TMPDIR},1G,100K \
    STOPTASKS,${DL_DIR},1G,100K \
    STOPTASKS,${SSTATE_DIR},1G,100K \
    STOPTASKS,/tmp,100M,100K \
    HALT,${TMPDIR},100M,1K \
    HALT,${DL_DIR},100M,1K \
    HALT,${SSTATE_DIR},100M,1K \
    HALT,/tmp,10M,1K"

CONF_VERSION = "2"
EOF

    cat > "${BUILD_DIR}/conf/conf-notes.txt" <<'EOF'

mcu-dev Yocto build (Raspberry Pi 4, 64-bit)

  bitbake core-image-connectivity

Flash the SD card (check the device with lsblk first):

  flash-sd /dev/sdX YES

Image:

  tmp/deploy/images/raspberrypi4-64/core-image-connectivity-raspberrypi4-64.rootfs.wic.bz2

EOF
    touch "${MARKER}"
else
    echo "==> keeping existing ${BUILD_DIR}/conf"
fi
