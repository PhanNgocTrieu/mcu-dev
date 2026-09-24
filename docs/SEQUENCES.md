# Sequence diagrams — usb-manager

Các luồng runtime chính của Connectivity SC prototype (USB Host).  
Đối chiếu function: [FUNCTIONS.md](FUNCTIONS.md).

---

## 1. Khởi động daemon

```mermaid
sequenceDiagram
  participant User
  participant Main as main
  participant Reg as DeviceRegistry
  participant Pol as ConnectionPolicy
  participant Orch as SessionOrchestrator
  participant AA as AndroidAutoAdapter
  participant CP as CarPlayStubAdapter
  participant DBus as UsbDbusService
  participant Udev as UdevMonitor

  User->>Main: start usb-manager
  Main->>Reg: construct
  Main->>Pol: construct / optional allowlist
  Main->>Orch: construct(Reg, Pol)
  Main->>AA: shared DemoAasdkSession
  Main->>Orch: setAndroidAutoAdapter(AA)
  Main->>CP: construct
  Main->>Orch: setCarPlayAdapter(CP)
  opt D-Bus enabled
    Main->>DBus: start()
    Main->>Reg: setChangeCallback(emitDeviceChanged)
    Main->>Orch: setSessionChangeCallback(emitSessionChanged)
  end
  Main->>Udev: setCallback(onAdd/onRemove)
  Main->>Udev: start()
  opt auto enum
    Main->>Udev: enumerateExisting()
    Udev-->>Orch: HotplugEvent Add (per device)
  end
  loop until SIGINT/SIGTERM
    Main->>Udev: pollOnce(500)
  end
  Main->>DBus: stop()
  Main->>Udev: stop()
```

---

## 2. Android phone plug-in → AA demo session (happy path)

Luồng mặc định khi classify ra `Android`: probe xong → **auto** `startSession`.

```mermaid
sequenceDiagram
  participant Phone
  participant Kernel
  participant Udev as UdevMonitor
  participant Clf as DeviceClassifier
  participant Orch as SessionOrchestrator
  participant Reg as DeviceRegistry
  participant Pol as ConnectionPolicy
  participant AA as AndroidAutoAdapter
  participant Probe as TransportProber
  participant Demo as DemoAasdkSession
  participant DBus as UsbDbusService
  participant HMI as HMI_SC

  Phone->>Kernel: USB enumerate (Host)
  Kernel->>Udev: udev add netlink
  Udev->>Udev: fillFromUdev + enrich
  Udev->>Clf: classify/enrich
  Udev->>Orch: onDeviceAdded(device)
  Orch->>Reg: upsert(Enumerating)
  Orch->>Reg: transition(Classified)
  Orch->>Orch: probeDevice(id)

  Orch->>Reg: transition(Probing)
  Orch->>Pol: preferredMode -> AndroidAuto
  Orch->>AA: probe(device)
  AA->>Probe: probeAoap(device)
  Probe-->>AA: AoapProbeResult
  AA->>Probe: probeNcm(device)
  Probe-->>AA: NcmProbeResult
  AA-->>Orch: AdapterResult ok
  Orch->>Reg: updateFields(aoap/ncm)
  Orch->>Reg: transition(Ready)

  Orch->>Orch: startSession(id, AndroidAuto)
  Orch->>Pol: canStart(...)
  Pol-->>Orch: allowed
  Orch->>Reg: transition(Connecting)
  Orch->>DBus: SessionChanged(Starting)
  DBus-->>HMI: signal SessionChanged
  Orch->>AA: start(device)
  AA->>Demo: start(device)
  Note over Demo: Log Video/Audio/Input/Sensor channels
  Demo-->>AA: true
  AA-->>Orch: ok
  Orch->>Reg: transition(Active)
  Orch->>DBus: SessionChanged(Active)
  DBus-->>HMI: signal SessionChanged
  Reg->>DBus: DeviceChanged(Active)
  DBus-->>HMI: signal DeviceChanged
```

---

## 3. iPhone plug-in → CarPlay stub (`MFI_REQUIRED`)

```mermaid
sequenceDiagram
  participant Phone as iPhone
  participant Udev as UdevMonitor
  participant Orch as SessionOrchestrator
  participant Reg as DeviceRegistry
  participant Pol as ConnectionPolicy
  participant CP as CarPlayStubAdapter
  participant DBus as UsbDbusService
  participant HMI as HMI_SC

  Phone->>Udev: USB add (VID 05ac)
  Udev->>Orch: onDeviceAdded(type=IPhone)
  Orch->>Reg: Classified
  Orch->>Orch: probeDevice
  Orch->>Pol: preferredMode -> CarPlay
  Orch->>CP: probe(device)
  CP-->>Orch: ok detected_apple_stub_only
  Orch->>Reg: Ready
  Note over Orch: Không auto-start CarPlay thành công
  Orch->>DBus: SessionChanged(Failed, MFI_REQUIRED)
  DBus-->>HMI: show CarPlay unavailable

  opt HMI vẫn gọi StartSession carplay
    HMI->>DBus: StartSession(id, carplay)
    DBus->>Orch: startSession
    Orch->>Pol: canStart
    Orch->>CP: start
    CP-->>Orch: false MFI_REQUIRED
    Orch->>Reg: Failed
    Orch->>DBus: SessionChanged(Failed, MFI_REQUIRED)
  end
```

---

## 4. Unplug khi đang Active

```mermaid
sequenceDiagram
  participant Phone
  participant Udev as UdevMonitor
  participant Orch as SessionOrchestrator
  participant AA as AndroidAutoAdapter
  participant Demo as DemoAasdkSession
  participant Reg as DeviceRegistry
  participant DBus as UsbDbusService

  Phone->>Udev: USB remove
  Udev->>Orch: onDeviceRemoved(id)
  alt session Active on this device
    Orch->>Orch: stopSession(id)
    Orch->>AA: stop(device)
    AA->>Demo: stop()
    Orch->>Reg: transition(Ready) then cleanup
    Orch->>DBus: SessionChanged(Idle)
  end
  Orch->>Reg: transition(Disconnecting)
  Orch->>Reg: remove(id)
  Reg->>DBus: DeviceChanged
```

---

## 5. Exclusive session (AA đang chạy, xin CarPlay)

```mermaid
sequenceDiagram
  participant HMI as HMI_SC
  participant DBus as UsbDbusService
  participant Orch as SessionOrchestrator
  participant Pol as ConnectionPolicy

  Note over Orch: active = Android device A, AndroidAuto, Active
  HMI->>DBus: StartSession(iPhoneB, carplay)
  DBus->>Orch: startSession(B, CarPlay)
  Orch->>Pol: canStart(B, CarPlay, active=A)
  Pol-->>Orch: allowed=false reason=exclusive_session_active:A
  Orch-->>DBus: false
  DBus-->>HMI: error / boolean false
```

---

## 6. HMI điều khiển qua D-Bus (manual start/stop)

```mermaid
sequenceDiagram
  participant HMI as HMI_SC
  participant DBus as UsbDbusService
  participant Orch as SessionOrchestrator
  participant Reg as DeviceRegistry
  participant AA as AndroidAutoAdapter

  HMI->>DBus: ListDevices()
  DBus->>Reg: list()
  Reg-->>DBus: devices
  DBus-->>HMI: aa{sv}

  HMI->>DBus: StartSession(id, android_auto)
  DBus->>Orch: startSession
  Orch->>AA: start
  Orch-->>DBus: true/false
  DBus-->>HMI: b
  DBus-->>HMI: SessionChanged (signal)

  HMI->>DBus: StopSession(id)
  DBus->>Orch: stopSession
  Orch->>AA: stop
  DBus-->>HMI: SessionChanged Idle
```

---

## 7. Chi tiết AOAP probe (P2)

```mermaid
sequenceDiagram
  participant AA as AndroidAutoAdapter
  participant Probe as TransportProber
  participant Libusb as libusb
  participant Phone

  AA->>Probe: probeAoap(device)
  alt already Google accessory PID 2d00-2d05
    Probe-->>AA: supported already_in_aoap_mode
  else need GET_PROTOCOL
    Probe->>Libusb: libusb_init / open vid:pid
    alt open fail (permission/busy)
      Probe-->>AA: attempted, not supported, open_failed_...
    else open ok
      Probe->>Phone: control IN GET_PROTOCOL (51)
      Phone-->>Probe: protocol >= 1
      Probe-->>AA: supported aoap_protocol=N
    end
    Probe->>Libusb: close / exit
  end
  AA->>Probe: probeNcm(device)
  Probe->>Probe: scan /sys/class/net
  Probe-->>AA: ifacePresent + ifName (optional)
```

---

## 8. Mass storage / Unknown / HID

```mermaid
sequenceDiagram
  participant Udev as UdevMonitor
  participant Orch as SessionOrchestrator
  participant Reg as DeviceRegistry

  alt type Hid or Unknown
    Udev->>Orch: onDeviceAdded
    Orch->>Reg: Ignored
    Note over Orch: Không probe projection
  else type MassStorage
    Udev->>Orch: onDeviceAdded
    Orch->>Reg: Classified
    Orch->>Orch: probeDevice
    Note over Orch: adapterFor(Storage)=null
    Orch->>Reg: Ready storage_ready
    Note over Orch: StartSession storage = stub mount path
  end
```

---

## 9. State machine thiết bị (nhìn ngang)

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Enumerating: udev_add
  Enumerating --> Classified: supported_type
  Enumerating --> Ignored: unknown_or_hid
  Classified --> Probing: probeDevice
  Probing --> Ready: probe_ok
  Probing --> Failed: probe_fail
  Probing --> Ignored: no_adapter
  Ready --> Connecting: startSession
  Connecting --> Active: adapter_start_ok
  Connecting --> Failed: adapter_start_fail
  Active --> Disconnecting: udev_remove
  Active --> Ready: stopSession
  Failed --> Idle: remove_or_retry
  Disconnecting --> Idle: registry_remove
  Ignored --> Idle: remove
```

---

## Gợi ý đọc diagram

1. Bắt đầu với **§2 Android plug-in** — đây là đường chính của prototype.
2. So sánh với **§3 CarPlay stub** — cùng orchestrator, khác adapter outcome.
3. Đọc `SessionOrchestrator.cpp` theo đúng tên method xuất hiện trên mũi tên.
4. Khi học AASDK thật, diagram §2 đoạn `DemoAasdkSession` sẽ được thay bằng channel AASDK thật (vẫn giữ biên `IAasdkSession`).
