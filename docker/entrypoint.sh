#!/bin/bash
set -euo pipefail

/opt/yocto/yocto-init.sh

# shellcheck disable=SC1091
set +e +u
source /work/yocto/poky/oe-init-build-env /work/yocto/build
set -e -u

if [[ $# -eq 0 ]]; then
    if [[ -f conf/conf-notes.txt ]]; then
        cat conf/conf-notes.txt
    fi
    exec bash
fi

exec "$@"
