# FluidTouch Configuration Guide

> Detailed configuration options and settings

## Table of Contents

- [WiFi Configuration](#wifi-configuration)
- [Machine Configuration](#machine-configuration)
- [Jog Settings](#jog-settings)
- [Probe Settings](#probe-settings)
- [Preferences Storage](#preferences-storage)

---

## WiFi Configuration

### Connecting to WiFi

**Settings → General → WiFi**

1. Enter **SSID** (network name)
2. Enter **Password** (case-sensitive)
3. Tap **Connect** button
4. Wait for connection status:
   - "Connecting..." - Attempting connection
   - "Connected: [IP Address]" - Success
   - "Failed" - Check credentials and try again

### WiFi Requirements

- **Frequency:** 2.4GHz only (ESP32 limitation)
- **Security:** WPA2-PSK or open networks
- **DHCP:** Automatic IP assignment recommended
- **Signal:** Strong signal required for reliable WebSocket connection

### Troubleshooting WiFi

**Connection Fails:**
- Verify SSID and password (case-sensitive)
- Ensure 2.4GHz network (not 5GHz)
- Check router settings (WPA2-PSK, SSID broadcast enabled)
- Try open network for testing
- Check for MAC address filtering on router

**Connection Drops:**
- Improve signal strength (move closer to router/AP)
- Reduce WiFi interference (other devices, microwaves)
- Check router stability and logs
- Consider static IP assignment

**Can't Access Screenshot Server:**
- Verify IP address in Settings → About
- Check firewall settings
- Ensure devices on same subnet
- Try http://[IP]/screenshot directly

---

## Machine Configuration

### Adding a Machine

**Machine Selection Screen → Add Machine**

1. **Name:** Descriptive name (e.g., "CNC Router", "Laser Cutter")
   - Up to 32 characters
   - Used for identification in status bar

2. **Hostname/IP:** FluidNC network address
   - IP address format: `192.168.1.100`
   - Hostname format: `fluidnc.local` (mDNS)
   - Must be reachable from FluidTouch WiFi network

3. **Port:** WebSocket port number
   - Default: `81`
   - Must match FluidNC WebSocket port
   - Check FluidNC YAML: `WebUI/Port`

### Editing a Machine

1. Select machine in list
2. Tap **Edit** button
3. Modify any settings
4. Tap **Save**
5. Restart to apply changes

### Deleting a Machine

1. Select machine in list
2. Tap **Delete** button
3. Confirm deletion
4. Machine removed from storage

### Reordering Machines

Use **Up/Down** buttons to:
- Move frequently used machines to top
- Organize by usage or location
- Group similar machines together

### Machine Storage

- Maximum: 4 machines
- Stored in ESP32 NVS (non-volatile storage)
- Survives power cycles and firmware updates
- Stored data:
  - Machine name
  - Hostname/IP address
  - WebSocket port

---

## Jog Settings

**Settings → Jog**

Configure default jogging behavior:

### XY Max Feed Rate

- **Range:** 1-10000 mm/min
- **Default:** 3000 mm/min
- **Usage:**
  - Maximum speed for XY jogging
  - Used by joystick (0-100% scaling)
  - Used by jog buttons with selected step size
  
**Recommendations:**
- Start conservatively (1000-2000 mm/min)
- Increase based on machine capability
- Consider material and tooling
- Lower for heavy gantry/delicate setups

### Z Max Feed Rate

- **Range:** 1-5000 mm/min
- **Default:** 500 mm/min
- **Usage:**
  - Maximum speed for Z-axis jogging
  - Used by joystick/slider (0-100% scaling)
  - Used by Z jog buttons
  
**Recommendations:**
- Slower than XY (typically 25-50% of XY rate)
- Account for screw lead and motor torque
- Consider tool weight and spindle mass
- Safer to err on slow side for Z

### Step Size Presets

**XY Steps:** 100, 50, 10, 1, 0.1 mm
**Z Steps:** 50, 25, 10, 1, 0.1 mm

Customization via `UITheme::XY_STEP_VALUES` and `UITheme::Z_STEP_VALUES` in code.

---

## Probe Settings

**Settings → Probe**

Default values for touch probe operations:

### Feed Rate

- **Range:** 1-1000 mm/min
- **Default:** 100 mm/min
- **Usage:** Probing speed toward target
  
**Recommendations:**
- Slower = more accurate
- Faster = quicker but less precise
- 50-150 mm/min typical for touch probes
- Adjust based on probe sensitivity

### Max Distance

- **Range:** 1-500 mm
- **Default:** 50 mm
- **Usage:** Maximum travel distance while probing
  
**Safety:**
- Set conservatively for first use
- Prevents runaway if probe doesn't trigger
- Should exceed expected probe distance
- Typical: 25-100 mm depending on setup

### Retract Distance

- **Range:** 0-50 mm
- **Default:** 2 mm
- **Usage:** Pull-off distance after probe contact
  
**Recommendations:**
- 1-5 mm typical
- Enough to clear chips/swarf
- Not too large (wastes time)
- Consider second slower probe if needed

### Probe Thickness

- **Range:** 0-50 mm
- **Default:** 0 mm
- **Usage:** Thickness of probe plate/puck
  
**Usage:**
- Enter actual probe plate thickness
- FluidTouch compensates automatically
- Zero value for direct surface probing
- Typical probe plates: 10-25 mm

---

## Preferences Storage

FluidTouch uses ESP32 NVS (Non-Volatile Storage) for persistent data:

### Stored Data

**Machine Configurations:**
- Up to 4 machine profiles
- Name, hostname/IP, port
- Selected machine index

**WiFi Credentials:**
- SSID (network name)
- Password (encrypted in NVS)
- Auto-reconnect on boot

**Jog Settings:**
- XY max feed rate
- Z max feed rate

**Probe Settings:**
- Feed rate
- Max distance
- Retract distance
- Probe thickness

### Storage Characteristics

- **Persistence:** Survives power loss and restarts
- **Wear leveling:** NVS manages flash wear automatically
- **Size limit:** ~15KB available for preferences
- **Namespace:** "fluidtouch" isolates from other apps
- **Backup:** No automatic backup - note settings before full flash

### Clearing Preferences

To reset to factory defaults:

1. Flash new firmware with "Erase Flash" option
2. Or send erase command via esptool:
   ```bash
   esptool.py --chip esp32s3 erase_flash
   ```
3. Then re-flash firmware

⚠️ **Warning:** This erases all stored configurations!

---

## Advanced Configuration

### Static IP Assignment

FluidTouch uses DHCP by default. For static IP:

1. Configure DHCP reservation in router for FluidTouch MAC
2. Or modify `ScreenshotServer::init()` to use static IP
3. Rebuild and flash firmware

### WebSocket Port Configuration

Default port 81 works for most FluidNC setups. If changed:

1. Check FluidNC YAML configuration:
   ```yaml
   WebUI:
     port: 81  # Change to match FluidNC
   ```
2. Update machine configuration in FluidTouch
3. Ensure firewall allows port

### Screenshot Server Port

Fixed at port 80 (HTTP). To change:

1. Modify `SCREENSHOT_SERVER_PORT` in `screenshot_server.cpp`
2. Rebuild firmware
3. Update documentation/QR codes

---

*For usage instructions, see [Usage Guide](./usage.md)*
