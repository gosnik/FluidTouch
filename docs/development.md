# FluidTouch Development Guide

> Building, debugging, and contributing to FluidTouch

## Table of Contents

- [Development Environment](#development-environment)
- [Building from Source](#building-from-source)
- [Project Architecture](#project-architecture)
- [Code Style Guidelines](#code-style-guidelines)
- [Debugging](#debugging)
- [Contributing](#contributing)

---

## Development Environment

### Prerequisites

**Required:**
- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE Extension](https://platformio.org/install/ide?install=vscode)
- [Git](https://git-scm.com/)
- USB-C cable with data support

**Optional:**
- Elecrow CrowPanel 7" ESP32-S3 HMI Display:
  - [Basic version](https://www.awin1.com/cread.php?awinmid=82721&awinaffid=2663106&ued=https%3A%2F%2Fwww.elecrow.com%2Fesp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html) (4MB flash) *(affiliate link)*
    - 800×480 TN RGB TFT LCD
    - PWM backlight control
    - External battery connector
  - [Advance version](https://www.awin1.com/cread.php?awinmid=82721&awinaffid=2663106&ued=https%3A%2F%2Fwww.elecrow.com%2Fcrowpanel-advance-7-0-hmi-esp32-ai-display-800x480-artificial-intelligent-ips-touch-screen-support-meshtastic-and-arduino-lvgl-micropython.html) (16MB flash) *(affiliate link)*
    - 800×480 IPS RGB LCD (superior image quality vs Basic)
    - I2C backlight control
    - Internal battery connector with JST PH 2.0mm 2-pin connector
    - Optional acrylic case includes space for battery (e.g., 1200mAh lithium polymer battery)
    - ⚠️ **Advance Version Compatibility:** Currently only supports hardware Version 1.3
    - **Version 1.3 Configuration:** DIP switches S0 and S1 must both be set to position 1
    - If you have an earlier version (1.0, 1.1, 1.2), please [create a GitHub issue](https://github.com/jeyeager65/FluidTouch/issues) for assistance
- Serial terminal (PlatformIO includes one)
- Chrome/Edge browser (for ESP Web Tools testing)

### Hardware Notes

**Advance Display with Acrylic Case:**
- Boot and reset buttons are accessible through holes in the acrylic case
- ⚠️ **Important:** If placed on a flat surface, the device may press the reset button and restart
- **Solution:** Install 6mm M3 screws in the mounting inserts on the bottom to create clearance

### Repository Setup

```bash
# Clone repository
git clone https://github.com/jeyeager65/FluidTouch.git
cd FluidTouch

# Checkout development branch
git checkout dev

# Open in VS Code
code .
```

---

## Building from Source

### PlatformIO Commands

**Build Environments:**
- `elecrow-crowpanel-7-basic` - Basic hardware (4MB flash, PWM backlight)
- `elecrow-crowpanel-7-advance` - Advance hardware (16MB flash, I2C backlight)

**Windows PowerShell:**
```powershell
# Build only (Basic)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e elecrow-crowpanel-7-basic

# Build only (Advance)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e elecrow-crowpanel-7-advance

# Build and upload (Basic)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload -e elecrow-crowpanel-7-basic

# Build and upload (Advance)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload -e elecrow-crowpanel-7-advance

# Clean build
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t clean

# Serial monitor
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

**Linux/macOS:**
```bash
# Build only (Basic)
platformio run -e elecrow-crowpanel-7-basic

# Build only (Advance)
platformio run -e elecrow-crowpanel-7-advance

# Build and upload (Basic)
platformio run --target upload -e elecrow-crowpanel-7-basic

# Build and upload (Advance)
platformio run --target upload -e elecrow-crowpanel-7-advance

# Clean build
platformio run -t clean

# Serial monitor
platformio device monitor -b 115200
```

### Build Output

**Flash Usage:** 
- Basic: ~57% (1.86MB of 3.25MB)
- Advance: ~28% (1.86MB of 6.5MB)

**RAM Usage:** ~1.0% (82KB of 8.5MB)

**Binary Locations:**
- Basic Firmware: `.pio/build/elecrow-crowpanel-7-basic/firmware.bin`
- Advance Firmware: `.pio/build/elecrow-crowpanel-7-advance/firmware.bin`

---

## PC Emulator

FluidTouch can run as a native desktop app using the `emulator_64bits` PlatformIO
environment. This uses SDL2 and the LVGL SDL driver (mirroring `lv_platformio`).

### Requirements

- SDL2 development libraries
  - macOS (Homebrew): `brew install sdl2`
  - Ubuntu/Debian: `sudo apt-get install libsdl2-dev`
  - Windows: install SDL2 dev packages and ensure the include/lib paths are visible

### Build/Run

```bash
# Build
platformio run -e emulator_64bits

# Run (output binary name varies by platform)
./.pio/build/emulator_64bits/program
```

Notes:
- WiFi and hardware-specific drivers are stubbed for the emulator build.
- If no machines are configured, a default `Simulator` machine is created automatically.

### Raspberry Pi Cross-Compile

The `emulator_rpi` environment cross-compiles the SDL emulator for Raspberry Pi
using a GNU cross toolchain.

Requirements:
- Cross compiler toolchain (`arm-linux-gnueabihf-*` by default)
- SDL2 development package for the target rootfs/sysroot
- `pkg-config` for the target metadata
  (the build prefers `<triplet>-pkg-config` when present and otherwise falls back
  to `pkg-config` with target-aware search paths)
  or manual SDL2 flags

Build examples:

```bash
# Raspberry Pi OS 32-bit (default)
platformio run -e emulator_rpi

# Raspberry Pi OS 64-bit
FT_RPI_TRIPLET=aarch64-linux-gnu platformio run -e emulator_rpi

# Optional: explicit sysroot for headers/libs
FT_RPI_SYSROOT=/opt/rpi-sysroot platformio run -e emulator_rpi
```

Useful environment variables:
- `FT_RPI_TRIPLET`: toolchain prefix (default: `arm-linux-gnueabihf`)
- `FT_RPI_SYSROOT`: optional sysroot path passed as `--sysroot`
- `FT_RPI_PKG_CONFIG`: optional pkg-config binary override
- `FT_RPI_SDL_CFLAGS`: optional manual SDL2 compile flags (for example `-I/opt/rpi-sysroot/usr/include/SDL2`)
- `FT_RPI_SDL_LIBS`: optional manual SDL2 link flags (for example `-L/opt/rpi-sysroot/usr/lib/arm-linux-gnueabihf -lSDL2`)

### Raspberry Pi Circle Bare-Metal Target

The `rpi_circle` environment builds a Raspberry Pi bare-metal bootstrap target
using the [Circle](https://github.com/rsta2/circle) programming environment,
the `circle-stdlib` integration layer, and Circle's LVGL addon. These
dependencies are tracked in-repo as Git submodules under `third_party/`. This
is intentionally a bring-up target for the porting work, not a full replacement
for the ESP32 firmware.

Requirements:
- Repository submodules initialized
- `make`
- For 32-bit builds: `arm-none-eabi-*`
- For local Raspberry Pi 4 / 64-bit builds: `aarch64-none-elf-*`

Repository setup:

```bash
git submodule update --init --recursive
```

Build example:

```bash
platformio run -e rpi_circle
```

By default the env targets Raspberry Pi 4 in 64-bit mode and uses
`aarch64-none-elf-*`. If that toolchain is not already installed, the PlatformIO
hook bootstraps a repo-local copy under `.tools/` using Arm GNU Toolchain
14.3.Rel1. You can override the target:

```bash
# Raspberry Pi 3, 64-bit
FT_CIRCLE_AARCH=64 FT_CIRCLE_RASPPI=3 \
  FT_CIRCLE_PREFIX64=aarch64-none-elf- \
  platformio run -e rpi_circle
```

Useful environment variables:
- `FT_CIRCLE_STDLIB_HOME`: optional override for the `circle-stdlib` checkout path
- `FT_CIRCLE_HOME`: optional override for a plain Circle checkout path
- `FT_CIRCLE_AARCH`: `32` or `64`
- `FT_CIRCLE_RASPPI`: Circle target selector (`1`, `2`, `3`, `4`, or `5` depending on architecture)
- `FT_CIRCLE_PREFIX32`: 32-bit toolchain prefix (default: `arm-none-eabi-`)
- `FT_CIRCLE_PREFIX64`: 64-bit toolchain prefix (default: `aarch64-none-elf-`)

Output:
- Circle artifacts: `.pio/build/rpi_circle/circle/`
- Bootable SD payload: `.pio/build/rpi_circle/circle/boot/`

### `rpi_circle_dev` Netboot Workflow

`rpi_circle_dev` builds the same Pi 4 Circle app but packages it for an Ethernet netboot workflow from the local build folder.

Build it with:

```sh
platformio run -e rpi_circle_dev
```

Output:
- Circle artifacts: `.pio/build/rpi_circle_dev/circle/`
- Netboot payload root: `.pio/build/rpi_circle_dev/circle/boot/`
- Netboot manifest: `.pio/build/rpi_circle_dev/circle/boot/netboot.json`

Serve that payload with the built-in Node server:

```sh
sudo node scripts/rpi_circle_netboot_server.mjs \
  --boot-dir .pio/build/rpi_circle_dev/circle/boot \
  --iface eth0 \
  --client-ip 10.42.0.9
```

Notes:
- This server uses DHCP on UDP `67` and TFTP on UDP `69`, so it normally needs root privileges or equivalent capabilities.
- By default it also exports the same build directory over NFS and advertises that export via DHCP root-path.
- Host NFS tools are required for that mode: `exportfs`, `rpc.nfsd`, and `rpc.mountd`.
- Pass `--no-nfs` if you want DHCP/TFTP only.
- Use an isolated Ethernet link or isolated network segment for development booting.
- The script auto-detects the host IPv4 from `--iface` and serves the Pi boot files directly from the build output.
- The Raspberry Pi 4 EEPROM must already be configured for network boot.
- The default initial boot file is `start4.elf`, which then loads `config.txt`, `cmdline.txt`, the Pi 4 DTB, and `kernel8-rpi4.img` from the same build folder.

---

## Project Architecture

### Directory Structure

```
FluidTouch/
├── include/                 # Header files
│   ├── config.h            # Configuration constants
│   ├── core/               # Hardware drivers
│   ├── network/            # Network modules
│   └── ui/                 # UI headers
│       ├── ui_theme.h      # Centralized colors/constants
│       └── tabs/           # Tab UI headers
├── src/                    # Implementation files
│   ├── main.cpp           # Entry point
│   ├── core/              # Driver implementations
│   ├── network/           # Network implementations
│   └── ui/                # UI implementations
├── web/                    # ESP Web Tools installer
├── docs/                   # Documentation
└── platformio.ini         # Build configuration
```

### Module Organization

See `.github/copilot-instructions.md` for detailed architecture documentation.

**Key Principles:**
1. **Hardware Platform** - ESP32-S3 with 4MB flash + 8MB PSRAM
2. **Driver Modules** - DisplayDriver, TouchDriver (in `core/`)
3. **Network Modules** - ScreenshotServer, FluidNCClient (in `network/`)
4. **UI Hierarchy** - Modular tab system with static factory methods

---

## Code Style Guidelines

### Naming Conventions

**Classes:**
```cpp
class UITabControl {          // PascalCase
    static void create();      // camelCase methods
private:
    static lv_obj_t *button;   // snake_case for LVGL objects
};
```

**Files:**
- Headers: `ui_tab_control.h` (snake_case)
- Implementation: `ui_tab_control.cpp`
- Match class name pattern

**Constants:**
```cpp
// config.h
#define SCREEN_WIDTH 800

// ui_theme.h
namespace UITheme {
    static constexpr lv_color_t ACCENT_PRIMARY = ...;
}
```

### Memory Management

**Critical Rules:**
1. **Large buffers (>10KB):** Use `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`
2. **LVGL objects:** Use LVGL's allocator (automatic PSRAM via `lv_conf.h`)
3. **Delta checking:** Only update UI when values change
4. **Static pointers:** Required for UI elements needing live updates

### UI Development

**Theme Colors:**
```cpp
// NEVER use lv_color_hex() directly
lv_obj_set_style_bg_color(obj, UITheme::BG_MEDIUM, 0);  // ✅ Good
lv_obj_set_style_text_color(obj, lv_color_hex(0x00FF00), 0);  // ❌ Bad
```

**Event Handling:**
```cpp
// Use LV_EVENT_CLICKED for touch interactions
lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, nullptr);
```

**Label Updates:**
```cpp
// Delta checking to prevent unnecessary redraws
static float last_value = -9999.0f;
if (current_value != last_value) {
    lv_label_set_text_fmt(label, "%.3f", current_value);
    last_value = current_value;
}
```

### Module Structure

**Header Pattern:**
```cpp
#ifndef UI_TAB_EXAMPLE_H
#define UI_TAB_EXAMPLE_H

#include <lvgl.h>

class UITabExample {
public:
    static void create(lv_obj_t *parent);
    static void update();  // Optional, for dynamic updates
    
private:
    static lv_obj_t *label;
    static void event_handler(lv_event_t *e);
};

#endif
```

**Implementation Pattern:**
```cpp
#include "ui/tabs/ui_tab_example.h"
#include "ui/ui_theme.h"

// Static member initialization
lv_obj_t *UITabExample::label = nullptr;

void UITabExample::create(lv_obj_t *parent) {
    // Clear scrolling if fixed layout
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create UI elements
    label = lv_label_create(parent);
    lv_label_set_text(label, "Example");
    lv_obj_set_style_text_color(label, UITheme::TEXT_LIGHT, 0);
}
```

---

## Debugging

### Serial Debugging

**Monitor Output:**
```bash
platformio device monitor -b 115200
```

**Debug Macros:**
```cpp
Serial.println("Debug message");
Serial.printf("Value: %d\n", value);
Serial.printf("Free heap: %d, Free PSRAM: %d\n", 
    ESP.getFreeHeap(), ESP.getFreePsram());
```

### Screenshot Server

Access live display at `http://[ESP32-IP]`

**Benefits:**
- No serial connection needed
- See actual rendered UI
- Document UI states
- Debug layout issues
- Share visual bugs

### Memory Monitoring

**Check Heap:**
```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
```

**LVGL Memory:**
```cpp
lv_mem_monitor_t mon;
lv_mem_monitor(&mon);
Serial.printf("LVGL used: %d KB\n", mon.total_size - mon.free_size);
```

### Common Issues

**Out of Memory:**
- Check large buffers use PSRAM (`heap_caps_malloc`)
- Monitor LVGL heap usage
- Verify display buffers allocated correctly

**UI Not Updating:**
- Ensure `lv_tick_inc()` and `lv_timer_handler()` called in main loop
- Check static pointers initialized
- Verify event callbacks registered

**Touch Not Working:**
- Check serial log for GT911 initialization
- Verify I2C pins (SDA=19, SCL=20, RST=38)
- Test with screenshot server

---

## Contributing

### Development Workflow

1. **Fork** the repository
2. **Create branch** from `dev`:
   ```bash
   git checkout dev
   git pull origin dev
   git checkout -b feature/my-feature
   ```
3. **Make changes** following code style guidelines
4. **Test thoroughly** on hardware
5. **Commit** with clear messages:
   ```bash
   git commit -m "Add feature: description"
   ```
6. **Push** to your fork:
   ```bash
   git push origin feature/my-feature
   ```
7. **Create Pull Request** to `dev` branch

### Pull Request Guidelines

**Include:**
- Clear description of changes
- Screenshots/videos for UI changes
- Test results on hardware
- Any breaking changes noted
- Updated documentation if needed

**Requirements:**
- Code follows style guidelines
- No compiler warnings
- Flash usage remains reasonable (<70%)
- Tested on actual hardware preferred

### Branching Strategy

- **`main`:** Stable releases only
- **`dev`:** Active development
- **Feature branches:** `feature/description`
- **Bug fixes:** `fix/description`

### Testing Checklist

- [ ] Compiles without warnings
- [ ] Flash usage acceptable
- [ ] UI responsive on hardware
- [ ] Touch input works correctly
- [ ] FluidNC communication stable
- [ ] WiFi connects reliably
- [ ] No memory leaks
- [ ] Screenshot server accessible
- [ ] State transitions handled
- [ ] Error conditions tested

---

## Resources

**FluidTouch:**
- [GitHub Repository](https://github.com/jeyeager65/FluidTouch)
- [Issues](https://github.com/jeyeager65/FluidTouch/issues)

**Dependencies:**
- [LVGL Documentation](https://docs.lvgl.io/)
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [FluidNC](https://github.com/bdring/FluidNC)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)

**Hardware:**
- [Elecrow CrowPanel 7" Basic](https://www.awin1.com/cread.php?awinmid=82721&awinaffid=2663106&ued=https%3A%2F%2Fwww.elecrow.com%2Fesp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html) *(affiliate link)*
- [Elecrow CrowPanel 7" Advance](https://www.awin1.com/cread.php?awinmid=82721&awinaffid=2663106&ued=https%3A%2F%2Fwww.elecrow.com%2Fcrowpanel-advance-7-0-hmi-esp32-ai-display-800x480-artificial-intelligent-ips-touch-screen-support-meshtastic-and-arduino-lvgl-micropython.html) *(affiliate link)*
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

---

*For architecture details, see `.github/copilot-instructions.md`*
