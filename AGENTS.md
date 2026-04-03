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

## Circle SDK Changes
- Do not leave project-specific edits as direct modifications inside `third_party/circle-stdlib/libs/circle`.
- Any required Circle SDK customization must be captured as patch files under `patches/circle/` and applied by `scripts/pio_circle_env.py` so `platformio run -e rpi_circle` is reproducible from a clean checkout.

## Expectations for Changes
- Keep UI updates in the main loop efficient and non-blocking.
- Prefer binary HID reports for status telemetry; avoid string-heavy updates unless required.
- Any changes to the HID protocol must be mirrored in `qtdial`.

## Debugging
- GPIO25 = TCK
- GPIO26 = TDI
- GPIO24 = TDO
- GPIO27 = TMS
- GPIO22 = nTRST
- GND = GND
- For Pi 4 bare-metal JTAG, `config.txt` needs:
  - `enable_jtag_gpio=1`
  - `gpio=22-27=a4`
  - `gpio=22-27=pn`
- The working OpenOCD target is `target/bcm2711.cfg`.
- The working FTDI probe interface on this host is `interface/ftdi/esp32_devkitj_v1.cfg`.
- A known-good attach command on this host is:
  - `openocd -s /home/adam/.local/share/Trash/files/packages/tool-openocd-rp2040-earlephilhower/share/openocd/scripts -f interface/ftdi/esp32_devkitj_v1.cfg -c 'transport select jtag' -c 'adapter speed 100' -f target/bcm2711.cfg`
- A software reboot can be triggered over JTAG using the Pi 4 power manager watchdog registers:
  - `ARM_PM_RSTS = 0xFE100020`
  - `ARM_PM_WDOG = 0xFE100024`
  - `ARM_PM_RSTC = 0xFE10001C`
  - write sequence:
    - `mww 0xFE100020 0x5A000000`
    - `mww 0xFE100024 0x5A00000A`
    - `mww 0xFE10001C 0x5A000020`
  - after this write, the DAP/JTAG link will usually drop immediately and OpenOCD must be reattached after the Pi restarts
  - one-line command: `printf 'targets bcm2711.cpu0\nhalt\nmww 0xFE100020 0x5A000000\nmww 0xFE100024 0x5A00000A\nmww 0xFE10001C 0x5A000020\n' | nc 127.0.0.1 4444`
- A previous Pi freeze was debugged to framebuffer-console writes from the RPi `Serial` shim during `UICommon::createMainUI()`.
