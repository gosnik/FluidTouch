#ifndef COMM_TYPES_H
#define COMM_TYPES_H

#include <Arduino.h>
#include <cstring>
#include <functional>

// Callback type for receiving CNC messages.
using MessageCallback = std::function<void(const char* message)>;

// CNC machine states.
enum MachineState {
    STATE_IDLE = 0,
    STATE_RUN = 1,
    STATE_HOLD = 2,
    STATE_JOG = 3,
    STATE_ALARM = 4,
    STATE_DOOR = 5,
    STATE_CHECK = 6,
    STATE_HOME = 7,
    STATE_SLEEP = 8,
    STATE_DISCONNECTED = 9
};

// CNC status report structure (shared across comm backends).
struct FluidNCStatus {
    // Machine state
    MachineState state;

    // Positions (X, Y, Z) in mm
    float mpos_x, mpos_y, mpos_z;  // Machine position
    float wpos_x, wpos_y, wpos_z;  // Work position
    float wco_x, wco_y, wco_z;     // Work coordinate offset (WPos = MPos - WCO)

    // Feed and spindle
    float feed_rate;        // Current feed rate (mm/min)
    float feed_override;    // Feed override percentage (0-200)
    float rapid_override;   // Rapid override percentage (0-200)
    float spindle_speed;    // Current spindle speed (RPM)
    float spindle_override; // Spindle override percentage (0-200)

    // Modal states (Grbl parser state)
    char modal_motion[8];    // G0, G1, G2, G3, etc.
    char modal_wcs[8];       // G54, G55, G56, etc.
    char modal_plane[8];     // G17, G18, G19
    char modal_units[8];     // G20 (inches), G21 (mm)
    char modal_distance[8];  // G90 (absolute), G91 (relative)
    char modal_feedrate[8];  // G93 (inverse time), G94 (units/min), G95 (units/rev)
    char modal_spindle[8];   // M3, M4, M5
    char modal_coolant[8];   // M7, M8, M9
    char modal_tool[8];      // T0, T1, T2, etc.

    // Last message from CNC
    char last_message[128]; // Store last [MSG:...] or feedback message

    // SD card file progress (when running from SD)
    bool is_sd_printing;        // True if running a file from SD card
    float sd_percent;           // Progress percentage (0.0-100.0)
    char sd_filename[64];       // Current file being run
    uint32_t sd_start_time_ms;  // Timestamp when file started (millis())
    uint32_t sd_elapsed_ms;     // Elapsed time since file started

    // Connection status
    bool is_connected;
    uint32_t last_update_ms;

};

inline void resetFluidNCStatus(FluidNCStatus &status) {
    std::memset(&status, 0, sizeof(status));
    status.state = STATE_DISCONNECTED;
    status.feed_override = 100;
    status.rapid_override = 100;
    status.spindle_override = 100;
    std::strcpy(status.modal_motion, "G0");
    std::strcpy(status.modal_wcs, "G54");
    std::strcpy(status.modal_plane, "G17");
    std::strcpy(status.modal_units, "G21");
    std::strcpy(status.modal_distance, "G90");
    std::strcpy(status.modal_feedrate, "G94");
    std::strcpy(status.modal_spindle, "M5");
    std::strcpy(status.modal_coolant, "M9");
    std::strcpy(status.modal_tool, "T0");
}

#endif // COMM_TYPES_H
