# 6 — Design: demo UI (SDL)

## 1. Vai trò

Lab visualization — **không** thay `hu-graphics` production.

| App | Việc |
|---|---|
| `usb-demo` (panel) | Nút sim plug/unplug, stream on/off; hiện `state` |
| `hupi-cluster` | Giả lập cụm đồng hồ + vùng projection + touch |

## 2. Component

```mermaid
flowchart TB
  subgraph panel["usb-panel"]
    BTN[buttons → sim/stream cmds]
    ST[state label]
  end
  subgraph cluster["hupi-cluster"]
    TEX[SDL texture RGB]
    H264L[H264 badge / counter]
    TOUCH[map click → touch cmd]
  end
  MAN[usb-managerd]
  panel <-->|manager.sock| MAN
  cluster <-->|manager.sock| MAN
  MAN -->|stream.sock| cluster
```

## 3. Video path trong cluster

```mermaid
flowchart TD
  R[recv stream] --> M{magic?}
  M -->|FRM1| T[UpdateTexture RGB]
  M -->|H264| C[đếm AU — chưa decode]
  T --> DRAW[RenderCopy]
  C --> BADGE[badge H264 + text bytes]
```

Touch: pixel trong vùng proj → chuẩn hóa 0…10000 → `touch x y down`.

## 4. Dependency

- SDL2 (`HUPI_HOST_SDL=ON`)
- `hupi_wire` (link + kv)
- Font helper trong `apps/common/ui.c` (+ stb nếu dùng)

## 5. Vì sao SDL trên host?

| Lý do | |
|---|---|
| Nhanh iterate session/AOA/sim | Không cần flash board |
| Thấy state + stream ngay | Debug policy |
| Không phụ thuộc Weston/EVB panel | Tách concern graphics thật |
