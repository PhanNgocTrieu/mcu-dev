# Source this before bitbake. It does not need root.
#   source env.sh
#   bitbake cm3-usb-image

_cm3_root=$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)

if [ ! -d "$HOME/locales/en_US.UTF-8" ]; then
    mkdir -p "$HOME/locales"
    localedef -c -f UTF-8 -i en_US "$HOME/locales/en_US.UTF-8"
fi

export LOCPATH="$HOME/locales"
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

if [ -d "$HOME/.local/bin" ]; then
    export PATH="$HOME/.local/bin:$PATH"
fi

unset OEROOT
. "$_cm3_root/poky/oe-init-build-env" "$_cm3_root/build"
