# Integrating real AASDK / OpenAuto

This prototype uses `DemoAasdkSession` behind `IAasdkSession` so the Connectivity SC
orchestration can be developed without pulling the full Android Auto stack.

## Recommended open-source references

1. **AASDK** — https://github.com/f1xpl/aasdk  
   Protocol/session library (channels: video, audio, input, sensor, …).
2. **OpenAuto** — https://github.com/f1xpl/openauto  
   HU-side player; study how it wires USB transport + Qt/OMX/GStreamer video.
3. **Android Open Accessory (AOAP)** — Android developer docs  
   `GET_PROTOCOL` (51), `SEND_STRING` (52), `START` (53); accessory VID `18d1`, PID `2d00`–`2d05`.

## How to swap in a real session

1. Add a CMake option `USB_MANAGER_WITH_AASDK=ON` and `find_package` / `FetchContent` for aasdk.
2. Implement `class RealAasdkSession : public IAasdkSession` that:
   - Opens AOAP bulk endpoints or NCM socket as required by your phone path
   - Creates `aasdk::USB::AOAPDevice` / messenger / service channels
   - Forwards video frames to HMI SC and audio PCM to Audio SC (do **not** render inside usb-manager)
3. In `main.cpp`, construct `RealAasdkSession` instead of `DemoAasdkSession`.

## Process boundaries

Keep `usb-manager` as a thin orchestrator:

```
usb-manager  --D-Bus-->  HMI_SC / Audio_SC
     |
     +--> AndroidAutoAdapter --> IAasdkSession (may be a separate process later)
```

Video decode and UI composition belong in HMI; usb-manager only owns device lifecycle and session start/stop.
