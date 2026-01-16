#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include "core/comm_types.h"
#include "ui/machine_config.h"

class CommManager {
public:
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
    static MessageCallback messageCallback;
    static MessageCallback terminalCallback;
    static EventCallback eventCallback;

    static bool useGrbl();
    static void applyCallbacks();
};

#endif // COMM_MANAGER_H
#include <functional>
