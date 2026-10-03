#!/bin/sh
# rasp4/build-demo — virtual monitors + USB plug simulation on the host.
set -eu
demo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
repo=$(CDPATH= cd -- "$demo/../.." && pwd)
runtime=${HUPI_RUNTIME:-/tmp/hupi-sim}
mkdir -p "$runtime" "$demo/build"

cmake -S "$repo" -B "$demo/build" -DHUPI_HOST_SDL=ON
cmake --build "$demo/build" -j"$(nproc)"

export HUPI_LOG_LEVEL="${HUPI_LOG_LEVEL:-trace}"
"$demo/build/modules/usb-driver-userlayer/usb-driverd" --runtime "$runtime" --log-level trace &
driver=$!
"$demo/build/modules/usb-man/usb-managerd" --runtime "$runtime" --log-level trace &
manager=$!
sleep 0.4
HUPI_RUNTIME="$runtime" "$demo/build/rasp4/build-demo/apps/cluster/hupi-cluster" &
HUPI_RUNTIME="$runtime" "$demo/build/rasp4/build-demo/apps/usb-panel/usb-demo" &
echo "logs: usb-driver / usb-man on stderr (trace). runtime=$runtime"
wait || true
kill "$manager" "$driver" 2>/dev/null || true
wait || true
