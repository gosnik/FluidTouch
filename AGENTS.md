## Overview
FluidTouch is the primary UI/UX application that runs on the ESP32-based touchscreen controller for CNC machines. It owns the full-screen LVGL interface, machine connection lifecycle, and status aggregation from FluidNC/Grbl transports.

## Relationships
- `FluidTouch` is the **host** application.
- `qtdial` is a **USB HID peripheral** that can be attached to FluidTouch to provide encoder inputs and a small secondary display.
- Communication between FluidTouch and qtdial uses a **vendor-defined USB HID protocol** (`qtdial_hid_protocol`), with FluidTouch acting as the host and qtdial as the device.

## Design Intent
- Provide a responsive, kiosk-style UI for CNC operations (Status, Control, Files, Macros, Terminal, Settings).
- Abstract communication backends via `CommManager` (wireless WebSocket vs UART/Grbl).
- Maintain a clear separation between UI, comms, and device/host IO.
- Keep the device resilient: explicit connection state handling, timeouts, and safe UI feedback.

## Key Components
- `CommManager` and backends (`FluidNCClient`, `GrblComm`): connection, status polling, commands.
- `UICommon` and `UITabs`: top-level UI lifecycle and tab orchestration.
- `PowerManager`: backlight dimming/sleep behavior based on activity.
- `UsbHostManager`: USB host + HID integration for qtdial and other devices.

## HID / qtdial Integration
- The host receives encoder input reports and sends **binary status** updates plus display commands.
- Tab transitions can request screen changes on qtdial devices to show axis-specific or mode-specific subsets.
- The HID protocol and report layouts are defined in `include/core/qtdial_hid_protocol.h`.
 - Control sub-tab changes can drive qtdial XML screen selection (see `src/ui/tabs/ui_tab_control.cpp`).

## Expectations for Changes
- Keep UI updates in the main loop efficient and non-blocking.
- Prefer binary HID reports for status telemetry; avoid string-heavy updates unless required.
- Any changes to the HID protocol must be mirrored in `qtdial`.
