#!/bin/sh
# Fetch the Yocto scarthgap layers and install the build configuration.
set -eu

cd "$(dirname "$0")"

clone_branch() {
    url=$1
    dest=$2
    branch=$3
    if [ -d "$dest/.git" ]; then
        echo "already present: $dest"
        return
    fi
    git clone --depth 1 --branch "$branch" "$url" "$dest"
}

clone_branch https://git.yoctoproject.org/poky poky scarthgap
clone_branch https://git.yoctoproject.org/meta-raspberrypi meta-raspberrypi scarthgap

mkdir -p build/conf
cp build-conf/local.conf build/conf/local.conf
cp build-conf/bblayers.conf build/conf/bblayers.conf

# BitBake drops unknown environment variables before it starts its server.
# Keep LOCPATH so a user-installed en_US.UTF-8 locale is visible.
python3 - <<'PY'
from pathlib import Path
p = Path("poky/bitbake/lib/bb/utils.py")
text = p.read_text()
old = "        'LC_ALL',\n        'BBSERVER',"
new = "        'LC_ALL',\n        'LOCPATH',\n        'BBSERVER',"
if old in text:
    p.write_text(text.replace(old, new, 1))
elif "LOCPATH" not in text.split("def preserved_envvars_exported")[1].split("def ")[0]:
    raise SystemExit("could not patch bitbake locale handling")
PY

echo
echo "Layers are ready. From this directory:"
echo "  source poky/oe-init-build-env build"
echo "  bitbake cm3-usb-image"
