# FluidTouch - AI Coding Agent Instructions

## Project Overview
FluidTouch is an ESP32-S3 embedded touchscreen CNC controller for FluidNC machines, running on the Elecrow CrowPanel 7" display (800x480). The project uses PlatformIO, LVGL 9.3 for UI, and LovyanGFX for hardware-accelerated display rendering.

**Status**: Active development - core architecture complete, features in progress.

## Architecture & Design Patterns

### Hardware Platform (ESP32-S3-WROOM-1-N4R8)
- **Flash**: 4MB DIO mode
- **PSRAM**: 8MB Octal PSRAM (`dio_opi` memory type)
- **Display**: 800x480 RGB parallel interface (16-bit RGB565)
- **Touch**: GT911 I2C controller (address 0x5D, pins SDA=19, SCL=20, RST=38)
- **Critical**: ALL large buffers MUST use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` to allocate in PSRAM, never regular heap

### Module Organization Pattern
The codebase follows a **strict modular pattern** with clear separation:

1. **Driver Modules** (`src/` + `include/`):
   - `DisplayDriver` - LovyanGFX RGB parallel display with LVGL integration
   - `TouchDriver` - GT911 I2C touch controller with LVGL input device
   - `ScreenshotServer` - WiFi web server for remote screenshots via LovyanGFX `readRect()`
   - `FluidNCClient` - WebSocket client for FluidNC communication with automatic status reporting

2. **UI Module Hierarchy** (all under `ui/` subdirectory):
   - `UICommon` - Shared status bar with machine/WiFi info and position displays
   - `UIMachineSelect` - Machine selection screen (appears after splash, before main UI)
   - `UISplash` - Startup splash screen (2.5s duration)
   - `UITabs` - Main tabview orchestrator, delegates to tab modules
   - `UITab*` - Individual tab modules (`UITabStatus`, `UITabControl`, etc.)
   - **Nested tabs**: `UITabControl` contains sub-modules in `tabs/control/` (Actions, Jog, Joystick, Probe, Overrides)

3. **Module Naming Convention**:
   - Class files: `ui_tab_control.h/cpp` → class `UITabControl`
   - All UI classes use static `create(lv_obj_t *parent)` factory methods
   - Headers in `include/`, implementations in `src/` (matching structure)

### LVGL 9.4 Specifics
- **Color depth**: RGB565 (16-bit) via `LV_COLOR_DEPTH 16`
- **Memory**: 256KB LVGL heap allocated in PSRAM (see `lv_conf.h`)
- **Display buffers**: Dual full-screen buffers (800×480 lines each) in PSRAM for smooth rendering
- **Tick handling**: Manual `lv_tick_inc()` + `lv_timer_handler()` in main loop every 5ms
- **No scrolling**: Most tabs disable `LV_OBJ_FLAG_SCROLLABLE` for fixed layouts
- **Font**: Montserrat 18pt for tab buttons and primary UI text
- **Event handling**: Use `LV_EVENT_CLICKED` (standard press+release) for all UI interactions - more forgiving of natural finger movement than `LV_EVENT_SHORT_CLICKED`
- **Label updates**: Use delta checking to prevent unnecessary redraws - only call `lv_label_set_text()` when values change

### Critical Integration Points
1. **Main loop sequence** (`main.cpp`):
   ```cpp
   DisplayDriver::init() → TouchDriver::init() → ScreenshotServer::init()
   → FluidNCClient::init() → UISplash::show() → UIMachineSelect::show() 
   → [user selects machine] → UICommon::createMainUI() → FluidNCClient::connect()
   → UICommon::createStatusBar() → UITabs::createTabs()
   ```

2. **FluidNC Communication**:
   - WebSocket client connects to FluidNC using machine configuration (IP/hostname + port, default 81)
   - Uses **automatic reporting** (`$Report/Interval=250`) instead of polling - FluidNC pushes updates every 250ms
   - Parses three message types:
     - Status reports (binary frames): `<Idle|MPos:x,y,z|FS:feed,spindle|Ov:feed,rapid,spindle|WCO:x,y,z|SD:percent,filename>`
     - GCode parser state: `[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]`
     - Realtime feedback: `[MSG:...]`, `[G92:...]`, etc.
   - **Work Position Calculation**: WPos = MPos - WCO (Work Coordinate Offset)
     - FluidNC sends MPos in every status report
     - WCO is sent periodically (not every report) to save bandwidth
     - WCO values are cached and used for continuous WPos calculation
   - **Feed/Spindle Values**: Parsed from both status reports (FS:) and GCode state (F/S)
     - Status report FS: values are current/actual feed and spindle
     - GCode state F/S values are programmed/commanded values (used as fallback)
   - **SD Card File Progress**: Parsed from status reports (SD:percent,filename)
     - Tracks elapsed time from file start using millis()
     - Calculates estimated completion time based on percentage and elapsed time
     - Displayed in Status tab header spanning columns 2-4 when file is running
     - Format: filename (truncated), progress bar, elapsed time (H:MM), estimated time (Est: H:MM)
   - **Delta Checking**: UI updates use cached values to prevent unnecessary redraws
     - Only updates labels when values actually change
     - Eliminates visual glitches from constant LVGL label redraws
     - Cached values initialized to sentinel values (-9999 for positions, -1 for rates)
   - UI updates every 250ms from FluidNC status in main loop
   - Connection initiated after machine selection in `UICommon::createMainUI()`

3. **Machine Selection**:
   - `UIMachineSelect` supports up to 5 machines with reordering (up/down buttons), edit, delete, and add functionality
   - Machines stored in Preferences as array under "machines" key (MachineConfig struct)
   - Selected machine stored in Preferences under "machine" key
   - Machine name displayed in status bar with connection symbol
   - **Layout**: 450px machine buttons + 60px up/down buttons + 70px edit/delete buttons
   - **Add button**: Single button in upper right corner (green, 120x40px)
   - **Machine switching**: Clicking right side of status bar shows confirmation dialog, then restarts ESP32 to switch machines cleanly

3. **Status Bar Layout** (60px height, 18pt font, split into clickable areas):
   - **Left area** (550px): Machine state (IDLE/RUN/ALARM) - 32pt uppercase, vertically centered, color-coded
     - Clicking navigates to Status tab (LV_EVENT_CLICKED)
   - **Center**: Work Position (top line, orange label) and Machine Position (bottom line, cyan label)
     - Separate labels for each axis (X/Y/Z) with axis-specific colors (X=cyan, Y=green, Z=magenta)
     - Format: "WPos:" then "X:0000.000 Y:0000.000 Z:0000.000" (4 digits before decimal)
   - **Right area** (240px): Machine name with symbol (top line, blue) and WiFi network (bottom line, cyan)
     - Clicking shows confirmation dialog "This will restart the controller. Continue?" with "⚡ Restart" button
     - Confirmation triggers ESP32 restart to cleanly reload with new machine selection (LV_EVENT_CLICKED)

4. **Tab creation delegation**:
   - `UITabs` creates tabview structure, delegates content to `UITab*::create()`
   - Each tab module is responsible for its own layout and event handlers
   - Nested tabviews use `LV_DIR_LEFT` for vertical tabs (see `UITabControl`)

5. **Serial debugging**: All modules use `Serial.println/printf` at 115200 baud with heap/PSRAM monitoring

## Development Workflows

### Building & Flashing
**CRITICAL**: On Windows, the `pio` shortcut often doesn't work. Use the full PlatformIO path with proper PowerShell quoting.

**ALWAYS upload after successful builds** - use `--target upload` instead of just building:

```powershell
# Windows: Full path to platformio.exe - MUST use quotes and call operator (&)
# ALWAYS use --target upload to flash after building
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload

# Or use the explicit path with quotes:
& "C:\Users\USERNAME\.platformio\penv\Scripts\platformio.exe" run --target upload

# Build and upload to ESP32-S3 (via USB) - DEFAULT COMMAND
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload

# Serial monitor (115200 baud)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200

# Clean build
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t clean
```

**Windows PowerShell Note**: The call operator `&` and quotes around the path are REQUIRED because the path contains environment variables and backslashes. Without quotes, PowerShell will throw a parser error. If `platformio.exe` shortcut works in your PATH, you can use it directly without the full path.

### Screenshot Debugging
- WiFi credentials stored in ESP32 Preferences (`PREFS_NAMESPACE "fluidtouch"`)
- When connected, access via browser at `http://<ESP32-IP>/screenshot`
- Returns live screen as BMP (800×480 RGB888, ~1.2MB)
- Uses persistent PSRAM buffer to avoid allocation failures

### Memory Management Rules
- **Display buffers**: 2× 256KB in PSRAM (dual buffering)
- **Screenshot buffer**: 768KB in PSRAM (lazy allocated on first request)
- **LVGL heap**: 256KB in PSRAM via custom allocator (`LV_MEM_POOL_ALLOC`)
- **Heap monitoring**: Check logs for "Free heap" and "Free PSRAM" at startup and every 5s

## Project-Specific Conventions

### UI Theme System (`include/ui/ui_theme.h`)
**CRITICAL**: All colors MUST use the centralized `UITheme` namespace - NO hardcoded `lv_color_hex()` calls allowed.

Theme uses semantic naming organized by category:
```cpp
// Accent colors - use for primary/secondary selections
UITheme::ACCENT_PRIMARY          // Main tab selected (blue)
UITheme::ACCENT_SECONDARY        // Sub-tab selected (teal)

// Machine states - use for CNC machine status display
UITheme::STATE_IDLE              // Idle state (green)
UITheme::STATE_RUN               // Running state (cyan)
UITheme::STATE_ALARM             // Alarm/Error state (red)

// Axis colors - use for X/Y/Z axis displays
UITheme::AXIS_X                  // X-axis (cyan)
UITheme::AXIS_Y                  // Y-axis (green)
UITheme::AXIS_Z                  // Z-axis (magenta)

// Position displays
UITheme::POS_MACHINE             // Machine position label (cyan)
UITheme::POS_WORK                // Work position label (orange)

// Buttons - specific action colors
UITheme::BTN_PLAY                // Play/Success buttons (green)
UITheme::BTN_ESTOP               // Emergency stop (dark red)
UITheme::BTN_CONNECT             // Connect button (blue)

// UI feedback
UITheme::UI_SUCCESS              // Success messages (green)
UITheme::UI_WARNING              // Warning messages (orange)
UITheme::UI_INFO                 // Info text (cyan)

// Backgrounds, text, borders - standard grays
UITheme::BG_DARK, BG_MEDIUM, TEXT_LIGHT, BORDER_MEDIUM
```

**Adding new colors**: Update `ui_theme.h` with semantic naming (purpose-based, not color-based).

### Configuration Constants
All other hardcoded values live in `include/config.h`:
- Screen dimensions, pin mappings, I2C addresses
- UI layout constants (`STATUS_BAR_HEIGHT`, `TAB_BUTTON_HEIGHT`)
- Buffer sizes, timing values, feature flags

### Adding New UI Tabs
1. Create header in `include/ui/tabs/ui_tab_<name>.h`
2. Create implementation in `src/ui/tabs/ui_tab_<name>.cpp`
3. Add `static void create(lv_obj_t *tab)` factory method
4. Register in `UITabs::createTabs()` with `lv_tabview_add_tab()`
5. Delegate creation via `UITabs::create<Name>Tab()` private method

## Key Files & Patterns

- **`platformio.ini`**: Flash/PSRAM config (`dio_opi`), partition tables, build flags
- **`include/lv_conf.h`**: LVGL configuration (color depth, memory, features) - 1400+ lines
- **`src/main.cpp`**: Entry point, initialization sequence, main loop with LVGL tick handling
- **`src/display_driver.cpp`**: LovyanGFX RGB parallel setup (lines 11-63 are pin mappings)
- **`include/config.h`**: Central configuration for ALL hardcoded values (BUFFER_LINES=480 for full-screen buffering)
- **`src/screenshot_server.cpp`**: WiFi setup, BMP conversion from RGB565 frame buffer
- **`src/fluidnc_client.cpp`**: FluidNC WebSocket client with automatic reporting, status parsing, WCO handling, F/S parsing from both status reports and GCode state, and SD card file progress tracking
- **`src/ui/ui_common.cpp`**: Status bar implementation with separate axis labels, delta checking for smooth updates, and clickable left/right areas for navigation and machine switching
- **`src/ui/ui_machine_select.cpp`**: Machine selection screen with reordering, edit, delete, and add functionality (up to 5 machines stored in Preferences)
- **`src/ui/tabs/ui_tab_status.cpp`**: Status tab with delta-checked position displays, feed/spindle rates with overrides, 8 modal state fields, message display, and SD card file progress (filename, progress bar, elapsed/estimated time)

### Status Tab Layout (ui_tab_status.cpp)
- **Top Section**: STATE (left, x=0) + FILE PROGRESS (spans columns 2-4, x=230-780, hidden when not printing)
  - File progress shows: filename (truncated to 350px), progress bar (0-100%), elapsed time (H:MM), estimated completion time (Est: H:MM)
  - Progress container has dark gray background with border, appears only when `is_sd_printing` is true
- **Column Layout** (after separator line at y=60):
  - Column 1 (x=0): Work Position (X/Y/Z)
  - Column 2 (x=225): Machine Position (X/Y/Z)
  - Column 3 (x=475): Feed Rate + Override, Spindle + Override
  - Column 4 (x=615/735): Modal states (9 fields: WCS, PLANE, DIST, UNITS, MOTION, FEED, SPINDLE, COOLANT, TOOL)
- **Bottom Section** (y=325): MESSAGE label spanning columns 1-3 (x=0, width=550px)
- **Modal States**: 26px vertical spacing, 20pt font, teal labels (ACCENT_SECONDARY), semantic value colors
- **Position Format**: "X  0000.000" (4 digits before decimal, 3 after, double-space after axis letter)
- **Padding**: 10px tab padding for consistent margins

## Common Pitfalls

1. **Memory allocation**: Never use `malloc()` for buffers >10KB - always use `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`
2. **LVGL tick**: Forgetting `lv_tick_inc()` breaks timers and input devices - must call every loop
3. **File structure**: UI files MUST be in `ui/` subdirectory (both `include/ui/` and `src/ui/`)
4. **Tab scrolling**: Most tabs have scrolling disabled - re-enable only if content exceeds screen height
5. **Display buffer size**: BUFFER_LINES=480 for full-screen buffering - provides smooth rendering with 8MB PSRAM available
6. **RGB565 byte order**: LovyanGFX returns byte-swapped RGB565 - swap before decoding (see `screenshot_server.cpp:19-29`)
7. **Color usage**: NEVER use `lv_color_hex()` directly - always use `UITheme::*` constants for maintainability and consistency
8. **Event types**: Always use `LV_EVENT_CLICKED` for touch interactions - provides better UX than `LV_EVENT_SHORT_CLICKED` by being more tolerant of slight finger movement
9. **Label updates**: Always use delta checking - only call `lv_label_set_text()` when values actually change to prevent visual glitches
10. **Static label pointers**: UI update methods require static member pointers to labels - never use local variables for labels that need live updates
11. **Machine switching**: Use `ESP.restart()` to switch between machines - cleanly avoids LVGL memory fragmentation issues that can occur when rebuilding entire UI trees
12. **WCO caching**: Work position requires cached WCO values since FluidNC only sends WCO periodically - calculate WPos = MPos - WCO on every status update
13. **SD file progress**: FluidNC sends `SD:percent,filename` in status reports - track start time on first detection, calculate elapsed/estimated times based on percentage and elapsed duration

## External Dependencies

- **FluidNC**: CNC controller firmware (WebSocket connection on port 81/82, automatic reporting enabled)
- **LVGL 9.4.0+**: UI library via PlatformIO lib_deps
- **LovyanGFX 1.2.7+**: Hardware display driver with RGB parallel support
- **WebSockets 2.5.4+**: WebSocket client library for FluidNC communication
- **ESP32 Arduino Framework**: Core platform APIs (Wire, WiFi, WebServer, Preferences)
