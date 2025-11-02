# FluidTouch Documentation Images

This folder contains screenshots and images for the FluidTouch documentation.

## Image Guidelines

### Naming Convention

Use descriptive, kebab-case names:
- `splash-screen.png`
- `machine-selection.png`
- `status-tab.png`
- `control-actions.png`
- `settings-general.png`

### Image Requirements

**Screenshots:**
- **Resolution:** 800×480 pixels (native display resolution)
- **Format:** PNG (lossless)
- **Source:** Screenshot server at `http://[ESP32-IP]/screenshot`
- **Processing:** Crop/annotate as needed, maintain aspect ratio

**Diagrams/Annotations:**
- **Format:** PNG or SVG
- **Background:** Transparent or white
- **Colors:** Match FluidTouch theme (blues, cyans, teals)

### Capture Method

1. **Start FluidTouch** on hardware
2. **Navigate to screen** you want to capture
3. **Open browser** to `http://[ESP32-IP]/screenshot`
4. **Right-click image** → "Save image as..."
5. **Rename file** following naming convention
6. **Place in this folder**

### Optimization

Before committing images:
- Remove unnecessary metadata
- Optimize PNG size (TinyPNG, ImageOptim, etc.)
- Target <500KB per image for reasonable file sizes

## Required Screenshots

### Core UI
- [ ] `splash-screen.png` - Startup splash with FluidNC logo
- [ ] `machine-selection.png` - Machine selection screen
- [ ] `status-bar.png` - Status bar showing all sections

### Tabs
- [ ] `status-tab.png` - Status tab with all columns
- [ ] `control-actions.png` - Control → Actions sub-tab
- [ ] `control-jog.png` - Control → Jog sub-tab
- [ ] `control-joystick.png` - Control → Joystick sub-tab
- [ ] `control-probe.png` - Control → Probe sub-tab
- [ ] `control-overrides.png` - Control → Overrides sub-tab
- [ ] `files-tab.png` - Files tab (placeholder)
- [ ] `macros-tab.png` - Macros tab (placeholder)
- [ ] `terminal-tab.png` - Terminal tab with messages
- [ ] `settings-general.png` - Settings → General (WiFi)
- [ ] `settings-jog.png` - Settings → Jog settings
- [ ] `settings-probe.png` - Settings → Probe settings
- [ ] `settings-about.png` - Settings → About with QR codes

### States/Dialogs
- [ ] `state-hold-popup.png` - HOLD state popup
- [ ] `state-alarm-popup.png` - ALARM state popup
- [ ] `machine-restart-dialog.png` - Machine switch confirmation

### Special Screens
- [ ] `sd-file-progress.png` - Status tab showing file progress
- [ ] `machine-edit-dialog.png` - Edit machine dialog
- [ ] `machine-add-dialog.png` - Add machine dialog

## Usage in Documentation

Reference images in Markdown:
```markdown
![Alt Text](./images/filename.png)
```

Example:
```markdown
![Splash Screen](./images/splash-screen.png)
```

The alt text helps with:
- Accessibility
- Search engine optimization
- Graceful degradation if image fails to load

## Image Status

Check off images as they're added to track progress.
