# FluidTouch Usage Guide

> Step-by-step instructions for operating your CNC machine with FluidTouch

## Table of Contents

- [First-Time Setup](#first-time-setup)
- [Connecting to Your Machine](#connecting-to-your-machine)
- [Basic Operations](#basic-operations)
- [Jogging](#jogging)
- [Probing](#probing)
- [Running Jobs](#running-jobs)
- [Troubleshooting](#troubleshooting)

---

## First-Time Setup

### 1. Power On

Connect the display to power via USB-C or external power supply. The device will boot and show the FluidTouch splash screen.

### 2. Configure WiFi

1. After splash screen, you'll see the machine selection screen
2. If no machines are configured, add your first machine
3. Go to **Settings → General**
4. Enter your **WiFi SSID** and **Password**
5. Tap **Connect**
6. Wait for "Connected" status (shows IP address)

### 3. Add Your Machine

1. Return to machine selection (restart if needed)
2. Tap the **green "Add Machine"** button
3. Enter machine details:
   - **Name:** Friendly name (e.g., "CNC Router", "Laser")
   - **Hostname/IP:** FluidNC IP address (e.g., `192.168.1.100`)
   - **Port:** WebSocket port (default: `81`)
4. Tap **Save**
5. Select the machine to connect

### 4. Verify Connection

Check the status bar:
- **Left:** Should show machine state (IDLE, ALARM, etc.)
- **Center:** Shows position coordinates
- **Right:** Shows machine name and WiFi network

If showing ALARM, clear it: **Control → Actions → Unlock**

---

## Connecting to Your Machine

### Machine Selection

On startup, FluidTouch shows saved machines:
1. **Tap a machine** to connect
2. **Edit** button - Modify machine settings
3. **Delete** button - Remove machine
4. **Up/Down** buttons - Reorder machines
5. **Add** button (green) - Add new machine

### Switching Machines

1. Tap **right side of status bar** (machine name area)
2. Confirm restart dialog
3. Device restarts and shows machine selection
4. Select different machine

---

## Basic Operations

### Understanding Machine States

FluidTouch displays the current machine state in the status bar (left side):

- **IDLE** (Green) - Ready for commands
- **RUN** (Cyan) - Executing G-code
- **JOG** (Cyan) - Manual jogging active
- **HOLD** (Yellow) - Feed hold (paused)
- **ALARM** (Red) - Error state, requires clearing
- **CHECK** (Orange) - G-code check mode
- **HOME** (Blue) - Homing cycle active
- **SLEEP** (Gray) - Sleep mode

### Clearing Alarms

If the machine enters ALARM state:

**Method 1 - Use Popup:**
1. ALARM popup appears automatically
2. Tap **Clear Alarm** button
3. Wait for state to change to IDLE

**Method 2 - Use Actions Tab:**
1. Go to **Control → Actions**
2. Tap **Unlock** button
3. If needed, tap **Reset** to fully restart

### Homing the Machine

Before first use, home your machine:

1. Go to **Control → Actions**
2. Tap **Home All** to home all axes
3. Or tap individual **Home X/Y/Z** buttons
4. Wait for homing cycle to complete
5. Status bar shows "IDLE" when done

---

## Jogging

### Button-Based Jogging (Jog Tab)

**XY Jogging:**
1. Go to **Control → Jog**
2. Select **step size** (100, 50, 10, 1, 0.1 mm)
3. Tap **direction buttons** on jog pad
   - Diagonal buttons move both axes
   - N/S buttons move Y only
   - E/W buttons move X only
4. Adjust **feed rate** with ±100 / ±1000 buttons
5. Tap **STOP** to cancel movement

**Z Jogging:**
1. Select **Z step size** (50, 25, 10, 1, 0.1 mm)
2. Tap **Z+** or **Z-** buttons
3. Adjust **feed rate** independently
4. Tap **STOP** to cancel

### Analog Jogging (Joystick Tab)

**XY Joystick:**
1. Go to **Control → Joystick**
2. Drag the **circular joystick** knob
3. Distance from center = speed (quadratic response)
4. Release to stop

**Z Slider:**
1. Drag the **vertical slider** knob
2. Up = Z+, Down = Z-
3. Distance from center = speed
4. Release to stop

**Feed Rate:**
- Set max feed rates in **Settings → Jog**
- Joystick/slider position scales from 0% to 100% of max

---

## Probing

### Setting Up Touch Probe

1. Attach touch probe to spindle/tool holder
2. Connect probe wire to FluidNC
3. Go to **Control → Probe**
4. Configure parameters:
   - **Feed Rate:** Probe speed (default: 100 mm/min)
   - **Max Distance:** How far to search (default: 50 mm)
   - **Retract:** Pull-off distance after contact (default: 2 mm)
   - **Probe Thickness:** Probe plate/puck thickness (default: 0 mm)

### Probing Operations

**Z-Axis Probing (Tool Length):**
1. Position tool above probe plate/work surface
2. Ensure within Max Distance
3. Tap **Z-** probe button
4. Wait for probe cycle to complete
5. Check results display:
   - "SUCCESS" - Shows Z position found
   - "FAILED" - No contact detected

**Offset Adjustment:**
- If using probe plate, enter thickness in **Probe Thickness**
- FluidTouch automatically accounts for thickness in result

### Probe Safety

⚠️ **Important Safety Notes:**
- Always test probe connection before use
- Start with conservative Max Distance
- Ensure probe is properly grounded
- Never probe faster than recommended
- Use retract distance to clear chips

---

## Running Jobs

### Before Starting

1. **Home machine** (Control → Actions → Home All)
2. **Load material** and secure workpiece
3. **Zero work coordinates:**
   - Jog to work zero position
   - Control → Actions → Zero All (or individual axes)
4. **Load G-code file** to FluidNC (via SD card or WebUI)

### Starting a Job

1. In FluidNC WebUI, start the G-code file
2. FluidTouch Status tab shows:
   - File name
   - Progress bar (0-100%)
   - Elapsed time
   - Estimated completion time
3. Monitor machine state and positions

### During a Job

**Feed Hold (Pause):**
- Press FluidNC feed hold button or send `!` command
- FluidTouch shows HOLD popup
- Tap **Resume** to continue

**Emergency Stop:**
- Press FluidNC reset or panic button
- Or send Ctrl+X via Terminal
- Machine enters ALARM state
- Clear alarm and restart job from beginning

### Job Monitoring

Status tab displays:
- **Real-time positions** - Work and machine coordinates
- **Feed rate** - Current vs. programmed
- **Spindle speed** - Current vs. programmed
- **File progress** - Percentage and time remaining
- **Messages** - FluidNC feedback

---

## Troubleshooting

### WiFi Won't Connect

**Check:**
- SSID and password are correct (case-sensitive)
- WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Router is in range and operational
- No special characters in password causing issues

**Try:**
- Restart FluidTouch device
- Forget and re-add WiFi credentials
- Check router logs for connection attempts
- Use static IP instead of DHCP if available

### Can't Connect to FluidNC

**Check:**
- FluidNC is powered on and responsive
- IP address/hostname is correct
- Port is correct (default: 81)
- Both devices on same WiFi network
- Firewall not blocking WebSocket connections

**Try:**
- Ping FluidNC IP from computer
- Access FluidNC WebUI from browser
- Check FluidNC YAML configuration for WebSocket port
- Restart both FluidTouch and FluidNC

### Machine Won't Move

**Check:**
- Machine state is IDLE (not ALARM or HOLD)
- Limits switches not triggered
- Stepper drivers enabled
- FluidNC configuration correct
- Emergency stop not engaged

**Try:**
- Clear any alarms (Control → Actions → Unlock)
- Home machine if required
- Check FluidNC terminal for error messages
- Verify motor cables and connections

### Touch Screen Not Responding

**Check:**
- Touch controller properly initialized (check serial log)
- No physical damage to screen
- Firmware uploaded successfully

**Try:**
- Restart device
- Re-upload firmware
- Check for loose ribbon cable connections
- Test with screenshot server to verify display works

### Position Values Wrong

**Check:**
- Machine has been homed (required for machine coordinates)
- Work coordinate system set correctly (G54-G59)
- No lost steps (check mechanical issues)
- FluidNC steps/mm configured correctly

**Try:**
- Home machine (Control → Actions → Home All)
- Zero work coordinates at known position
- Check FluidNC YAML for axis configuration
- Verify mechanical backlash/binding

### Screenshot Server Not Working

**Check:**
- WiFi is connected
- IP address shown in About tab
- Port 80 not blocked by firewall
- Browser can access FluidTouch IP

**Try:**
- Visit http://[FluidTouch-IP]/screenshot
- Scan QR code in Settings → About
- Check router DHCP assignment
- Restart WiFi connection

---

## Advanced Tips

### Multi-Machine Workflow

Save different machine profiles for:
- Different physical machines (router, laser, mill)
- Different FluidNC configurations
- Testing vs. production setups
- Multiple locations/shops

### Settings Optimization

**Jog Settings:**
- Set max feed rates based on machine capabilities
- Lower rates for heavy/delicate materials
- Higher rates for rapid positioning

**Probe Settings:**
- Save common probe setups
- Adjust feed rate for material hardness
- Use generous max distance for first contact

### Position Monitoring

Watch for:
- Unexpected position changes (lost steps)
- Work vs. machine coordinate differences
- Override percentages affecting feed/speed
- Modal state changes during operations

---

*For UI details and screenshots, see [User Interface Guide](./ui-guide.md)*
