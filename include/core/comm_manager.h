#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include <functional>

#include "core/comm_types.h"
#include "ui/machine_config.h"

class CommManager {
public:
    using CommandTap = std::function<void(const char *command)>;
    enum class EventType {
        CONNECTED,
        DISCONNECTED,
        CONNECTION_ERROR,
        PROBE_RESULT
    };

    struct ProbeResult {
        float x;
        float y;
        float z;
        bool success;
    };

    struct Event {
        EventType type;
        const char *message;
        ProbeResult probe;
    };

    struct JogCommand {
        bool absolute;
        bool has_x;
        bool has_y;
        bool has_z;
        float x;
        float y;
        float z;
        float feedrate;
        const char *units;
    };

    using EventCallback = std::function<void(const Event &event)>;

    static void init();
    static bool connect(const MachineConfig &config);
    static void disconnect();
    static void stopReconnectionAttempts();
    static bool isConnected();
    static bool isAutoReporting();
    static void loop();
    static const FluidNCStatus& getStatus();
    static void sendCommand(const char* command);
    static void setCommandTap(CommandTap tap);
    static void clearCommandTap();
    static bool sendJog(const JogCommand &cmd);
    static bool sendJogRelative(float x, float y, float z, float feedrate);
    static bool sendJogRelativeAxis(char axis, float delta, float feedrate);
    static bool getJogPredictedWpos(float &x, float &y, float &z);
    static int32_t getJogPendingCommandCount();
    static void setJogSoftLimits(bool x_enabled, bool y_enabled, bool z_enabled,
                                 float x_min, float x_max,
                                 float y_min, float y_max,
                                 float z_min, float z_max);
    static void requestStatusReport();
    static String getMachineIP();
    static void setMessageCallback(MessageCallback callback);
    static void clearMessageCallback();
    static void setTerminalCallback(MessageCallback callback);
    static void clearTerminalCallback();
    static void setEventCallback(EventCallback callback);
    static void clearEventCallback();
    static void emitConnected();
    static void emitDisconnected(const char *message);
    static void emitConnectionError(const char *message);
    static void emitProbeResult(float x, float y, float z, bool success);
    static ConnectionType getConnectionType();

private:
    static ConnectionType currentType;
    static MachineConfig currentConfig;
    static bool initialized;
    static bool jog_soft_limit_x_enabled;
    static bool jog_soft_limit_y_enabled;
    static bool jog_soft_limit_z_enabled;
    static float jog_soft_limit_x_min;
    static float jog_soft_limit_x_max;
    static float jog_soft_limit_y_min;
    static float jog_soft_limit_y_max;
    static float jog_soft_limit_z_min;
    static float jog_soft_limit_z_max;
    static bool jog_pending_valid;
    static float jog_pending_x;
    static float jog_pending_y;
    static float jog_pending_z;
    static int32_t jog_pending_command_count;
    static float jog_last_reported_x;
    static float jog_last_reported_y;
    static float jog_last_reported_z;
    static MachineState jog_last_state;

    static bool useGrbl();
    static void applyCallbacks();
    static void resetJogTracking();
    static void updateJogTrackingFromStatus(const FluidNCStatus &status);
    static void getPredictedWpos(const FluidNCStatus &status, float &x, float &y, float &z);
    static float clampJogTarget(float current, float target, bool enabled, float min_limit, float max_limit);
};

#endif // COMM_MANAGER_H
