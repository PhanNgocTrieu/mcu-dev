# meter-pf — EVB main board

Same `modules/` sources as `rasp4/`. This tree holds **board-specific**
configuration for the production EVB (machine conf, display, MFi flags).

| Path | Role |
|---|---|
| `../modules/usb-driver-userlayer` | plug / unplug / enumerate / usbfs |
| `../modules/usb-man` | session policy, AOA, one backend |
| `../modules/libhu-aa` | Android Auto media (enable `HUPI_WITH_AASDK`) |
| `../modules/libhu-carplay` | CarPlay media (enable `HUPI_WITH_MFI`) |
| `../meta-connectivity` | Yocto recipes shared with rasp4 |

## Board image differences vs rasp4

In `conf/local.conf` (or the EVB kas file) set:

```bitbake
EXTRA_OECMAKE:pn-libhu-aa = "-DHUPI_WITH_AASDK=ON"
EXTRA_OECMAKE:pn-libhu-carplay = "-DHUPI_WITH_MFI=ON"
```

and provide `third_party/aasdk` plus the proprietary `libiap2` / `libhu-mfi`
trees under `modules/` or via separate recipes in `meta-connectivity/recipes-conn-lib/`.

The EVB Yocto build should reuse `meta-connectivity` with a different `MACHINE`.
