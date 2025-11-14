# FluidTouch Screenshot Reference Guide

> Complete list of screenshots needed for documentation

## Required Screenshots (26 total)

Save location: `docs/images/`

---

### Startup & Selection (2 screenshots)

#### 1. splash-screen.png
- **What to capture:** FluidNC logo splash screen at startup
- **When:** Displays for 2.5 seconds on boot
- **Notes:** Centered FluidNC logo on dark background

#### 2. machine-selection.png
- **What to capture:** Machine selection screen (normal mode)
- **Show:**
  - FluidNC logo at top
  - List of configured machines (up to 4 visible)
  - Each machine shows name, WiFi network, and connection status
  - "⚙️ Manage" button in upper right corner
- **Notes:** Show at least 2-3 configured machines for visual variety

#### 3. machine-selection-edit.png
- **What to capture:** Machine selection screen in edit mode
- **Show:**
  - Wider machine buttons (458px) on left
  - Control buttons on right (60×60px): up/down arrows, edit (✏️), delete (🗑️)
  - "➕ Add" button in upper right corner (green, 120×45px)
  - Consistent 5px gaps between all elements
  - At least 2-3 machines to show reordering controls
- **Trigger:** Click "⚙️ Manage" button from normal mode
- **Notes:** Show mix of machines to demonstrate up/down functionality

#### 4. machine-dialog.png
- **What to capture:** Machine Add/Edit dialog overlay
- **Show:**
  - 780×460px modal dialog with semi-transparent background
  - Left column: Name, SSID, FluidNC URL text areas
  - Right column: Connection dropdown, Password, Port
  - Title: "ADD MACHINE" or "EDIT MACHINE" in uppercase gray
  - Save and Cancel buttons at bottom
- **Notes:** Capture in "Add" mode with empty fields or "Edit" mode with populated data

---

### Main Interface (1 screenshot)

#### 5. status-bar.png
- **What to capture:** Top status bar only (60px height)
- **Left section:** Machine state (IDLE/RUN/etc.) in 32pt uppercase, color-coded
- **Center section:** Work Position (orange labels) and Machine Position (cyan labels)
- **Right section:** Machine name + WiFi network
- **Notes:** Try to capture during RUN state for color variety

---

### Status Tab (1 screenshot)

#### 6. status-tab.png
- **What to capture:** Full status tab with all four columns
- **Column 1:** Work Position (X/Y/Z with orange labels)
- **Column 2:** Machine Position (X/Y/Z with cyan labels)
- **Column 3:** Feed Rate + Spindle Speed (with override percentages)
- **Column 4:** 9 modal state fields (WCS, Plane, Distance, Units, Motion, Feed Mode, Spindle, Coolant, Tool)
- **Bottom:** Message display area
- **Notes:** Show with actual machine data, non-zero coordinates preferred

---

### Control Tab - Sub-tabs (5 screenshots)

#### 7. control-actions.png
- **What to capture:** Actions sub-tab of Control tab
- **Show:** Grid layout with buttons:
  - Top row: Home All, Home X, Home Y, Home Z
  - Middle row: Zero All, Zero X, Zero Y, Zero Z
  - Bottom row: Unlock, Reset
- **Notes:** Clean, centered button layout

#### 8. control-jog.png
- **What to capture:** Jog sub-tab of Control tab
- **Left side (XY):**
  - Step selection buttons (0.1, 1, 10, 50, 100)
  - 3×3 jog pad with diagonal buttons
  - Feed rate display and adjustment buttons
- **Right side (Z):**
  - Step selection buttons (0.1, 1, 10, 25, 50)
  - Z+ button, step display, Z- button
  - Feed rate display and adjustment buttons
- **Center:** Octagon STOP button
- **Notes:** Show with a step size selected

#### 9. control-joystick.png
- **What to capture:** Joystick sub-tab of Control tab
- **Left:** 220×220 circular XY joystick with crosshairs
- **Center:** Info display (percentage, feed rate, max feed rate)
- **Right:** 80×220 vertical Z slider
- **Notes:** Capture with joystick/slider in neutral position

#### 10. control-probe.png
- **What to capture:** Probe sub-tab of Control tab
- **Left side:** Probe buttons (X-, Y-, Z-) in axis colors
- **Right side:** Parameter inputs:
  - Feed Rate (mm/min)
  - Max Distance (mm)
  - Retract (mm)
  - Probe Thickness (mm)
- **Bottom:** 2-line result display textarea
- **Notes:** Show with default values, empty result field

#### 11. control-overrides.png
- **What to capture:** Overrides sub-tab of Control tab
- **Three sections:**
  - Feed Override: percentage display with -10%, -1%, Reset, +1%, +10% buttons
  - Rapid Override: 25%, 50%, 100% preset buttons
  - Spindle Override: percentage display with -10%, -1%, Reset, +1%, +10% buttons
- **Notes:** Show at 100% default values

---

### Other Main Tabs (3 screenshots)

#### 12. files-tab.png
- **What to capture:** Files tab showing file browser
- **Show:**
  - Folder/file list from FluidNC SD card
  - Mix of folders and files
  - File sizes and icons
  - Upload and back navigation buttons
- **Notes:** If "Folders on Top" is enabled, folders should appear first

#### 13. file-upload-dialog.png
- **What to capture:** File upload dialog overlay
- **Show:**
  - Modal dialog with upload instructions
  - Current directory path
  - Web URL for upload interface
  - QR code for mobile access
- **Notes:** Capture before upload starts (no progress bar yet)

#### 14. file-delete-dialog.png
- **What to capture:** Delete confirmation dialog
- **Show:**
  - Small modal dialog with filename
  - "Delete" button (red) and "Cancel" button
  - Semi-transparent background overlay
- **Notes:** Simple confirmation dialog, centered on screen

#### 15. macros-tab.png
- **What to capture:** Macros tab in normal mode
- **Show:**
  - 3×3 grid of macro buttons
  - Mix of configured macros (with colors and names) and empty slots
  - Gear icon in upper right for edit mode
- **Notes:** Show at least 3-4 configured macros with different colors

#### 15. macros-edit.png
- **What to capture:** Macros tab in edit mode
- **Show:**
  - 3×3 grid of macro slots
  - Each slot showing macro button + edit button + delete button
  - "Done" button in upper right
  - Mix of configured and empty slots
- **Notes:** Shows all 9 slots with their controls visible

#### 16. macro-dialog.png
- **What to capture:** Macro configuration dialog overlay
- **Show:**
  - Modal dialog (600×450px)
  - Name input field
  - File dropdown (showing available macro files)
  - Color picker buttons in grid
  - Save and Cancel buttons
- **Notes:** Capture with dropdown open if possible to show file options

#### 17. terminal-tab.png
- **What to capture:** Terminal tab with message stream
- **Show:**
  - Several lines of WebSocket messages
  - Auto-scroll toggle at top
  - Mix of message types (commands, responses, feedback)
- **Notes:** Messages should show variety of FluidNC communication

---

### Settings Tab - Sub-tabs (5 screenshots)

#### 18. settings-general.png
- **What to capture:** General sub-tab of Settings tab
- **Show:**
  - "Show Machine Selection" toggle
  - "Folders on Top" toggle with description
- **Notes:** Two-column layout

#### 19. settings-jog.png
- **What to capture:** Jog sub-tab of Settings tab
- **Show:**
  - XY Max Feed Rate input field with value
  - Z Max Feed Rate input field with value
- **Notes:** Show with typical values like 3000 for XY, 500 for Z

#### 20. settings-probe.png
- **What to capture:** Probe sub-tab of Settings tab
- **Show:**
  - Feed Rate input
  - Max Distance input
  - Retract Distance input
  - Probe Thickness input
- **Notes:** Show with default values (100, 50, 2, 0)

#### 21. settings-power.png
- **What to capture:** Power sub-tab of Settings tab
- **Show:**
  - Power Management section:
    - Enable toggle
    - Dim Timeout dropdown
    - Sleep Timeout dropdown
    - Deep Sleep dropdown
  - Brightness section:
    - Normal Brightness dropdown
    - Dim Brightness dropdown
- **Notes:** Show with reasonable settings enabled

#### 22. settings-about.png
- **What to capture:** About sub-tab of Settings tab
- **Show:**
  - FluidTouch version number
  - GitHub link with QR code
  - Screenshot Server link with QR code (if WiFi connected)
- **Notes:** QR codes should be visible and scannable

---

### Optional State Popups (3 screenshots - capture opportunistically)

#### 23. popup-hold.png
- **What to capture:** HOLD state popup overlay
- **Show:**
  - Modal dialog with cyan/teal border
  - "HOLD" title (32pt)
  - Last FluidNC message (24pt)
  - "Close" button (left)
  - "Resume" button (right)
- **Trigger:** Press feed hold on FluidNC or send `!` command during a job
- **Notes:** Appears over any tab

#### 24. popup-alarm.png
- **What to capture:** ALARM state popup overlay
- **Show:**
  - Modal dialog with red border
  - "ALARM" title (32pt)
  - Alarm message (24pt)
  - "Close" button (left)
  - "Clear Alarm" button (right)
- **Trigger:** Trigger a soft limit or force an alarm condition
- **Notes:** Appears over any tab

#### 25. sd-file-progress.png
- **What to capture:** Status tab during SD card file printing
- **Show:**
  - File progress bar spanning columns 2-4 in status tab header
  - Filename (truncated)
  - Progress bar with percentage
  - Elapsed time (H:MM format)
  - Estimated completion time (Est: H:MM)
- **Trigger:** Start printing a file from SD card
- **Notes:** Capture during active print job, ideally 30-70% complete

---

## Screenshot Capture Tips

### Method 1: Screenshot Server (Recommended)
1. Connect FluidTouch to WiFi
2. Go to Settings → About tab
3. Note the Screenshot Server URL (or scan QR code)
4. Open URL in browser on computer/phone
5. Click to download BMP screenshot
6. Convert BMP to PNG if needed

### Method 2: USB Serial Screenshot Tool
- Use esptool or custom script to capture framebuffer over serial
- Requires additional development tools

### Method 3: Phone Camera
- Take photo of display
- Crop to 800×480 if possible
- Ensure good lighting and no glare

### Image Processing
- **Format:** PNG preferred (smaller file size than BMP)
- **Resolution:** 800×480 native resolution
- **Naming:** Use exact filenames listed above
- **Optimization:** Compress PNGs to reduce repo size
- **Location:** Save all to `docs/images/` directory

---

## Priority Levels

### High Priority (Required for basic documentation)
1-2, 5-11 (startup, main interface, control sub-tabs), 12, 15, 18-23 (files, macros, terminal, all settings tabs)

### Medium Priority (Enhance documentation)
3 (machine edit mode), 4 (machine dialog), 13-14 (file upload/delete dialogs), 16-17 (macro edit/dialog), 24-25 (state popups)

### Low Priority (Nice to have)
26 (SD file progress - requires active print job)

---

## Checklist

- [ ] 1. splash-screen.png
- [ ] 2. machine-selection.png
- [ ] 3. machine-selection-edit.png
- [ ] 4. machine-dialog.png
- [ ] 5. status-bar.png
- [ ] 6. status-tab.png
- [ ] 7. control-actions.png
- [ ] 8. control-jog.png
- [ ] 9. control-joystick.png
- [ ] 10. control-probe.png
- [ ] 11. control-overrides.png
- [ ] 12. files-tab.png
- [ ] 13. file-upload-dialog.png
- [ ] 14. file-delete-dialog.png
- [ ] 15. macros-tab.png
- [ ] 16. macros-edit.png
- [ ] 17. macro-dialog.png
- [ ] 18. terminal-tab.png
- [ ] 19. settings-general.png
- [ ] 20. settings-jog.png
- [ ] 21. settings-probe.png
- [ ] 22. settings-power.png
- [ ] 23. settings-about.png
- [ ] 24. popup-hold.png (optional)
- [ ] 25. popup-alarm.png (optional)
- [ ] 26. sd-file-progress.png (optional)
