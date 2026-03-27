#include "core/comm_manager.h"
#include "core/grbl_comm.h"
#include "network/fluidnc_client.h"

namespace {

struct CommManagerState {
    ConnectionType currentType = CONN_WIRELESS;
    MachineConfig currentConfig{};
    bool initialized = false;
    bool jog_soft_limit_x_enabled = false;
    bool jog_soft_limit_y_enabled = false;
    bool jog_soft_limit_z_enabled = false;
    float jog_soft_limit_x_min = 0.0f;
    float jog_soft_limit_x_max = 0.0f;
    float jog_soft_limit_y_min = 0.0f;
    float jog_soft_limit_y_max = 0.0f;
    float jog_soft_limit_z_min = 0.0f;
    float jog_soft_limit_z_max = 0.0f;
    bool jog_pending_valid = false;
    float jog_pending_x = 0.0f;
    float jog_pending_y = 0.0f;
    float jog_pending_z = 0.0f;
    int32_t jog_pending_command_count = 0;
    float jog_last_reported_x = 0.0f;
    float jog_last_reported_y = 0.0f;
    float jog_last_reported_z = 0.0f;
    MachineState jog_last_state = STATE_DISCONNECTED;
};

CommManagerState &commManagerState() {
    static CommManagerState state;
    return state;
}

MessageCallback &messageCallbackStorage() {
    static MessageCallback callback;
    return callback;
}

MessageCallback &terminalCallbackStorage() {
    static MessageCallback callback;
    return callback;
}

CommManager::CommandTap &commandTapStorage() {
    static CommManager::CommandTap callback;
    return callback;
}

CommManager::EventCallback &eventCallbackStorage() {
    static CommManager::EventCallback callback;
    return callback;
}

}  // namespace

#define currentType commManagerState().currentType
#define currentConfig commManagerState().currentConfig
#define initialized commManagerState().initialized
#define jog_soft_limit_x_enabled commManagerState().jog_soft_limit_x_enabled
#define jog_soft_limit_y_enabled commManagerState().jog_soft_limit_y_enabled
#define jog_soft_limit_z_enabled commManagerState().jog_soft_limit_z_enabled
#define jog_soft_limit_x_min commManagerState().jog_soft_limit_x_min
#define jog_soft_limit_x_max commManagerState().jog_soft_limit_x_max
#define jog_soft_limit_y_min commManagerState().jog_soft_limit_y_min
#define jog_soft_limit_y_max commManagerState().jog_soft_limit_y_max
#define jog_soft_limit_z_min commManagerState().jog_soft_limit_z_min
#define jog_soft_limit_z_max commManagerState().jog_soft_limit_z_max
#define jog_pending_valid commManagerState().jog_pending_valid
#define jog_pending_x commManagerState().jog_pending_x
#define jog_pending_y commManagerState().jog_pending_y
#define jog_pending_z commManagerState().jog_pending_z
#define jog_pending_command_count commManagerState().jog_pending_command_count
#define jog_last_reported_x commManagerState().jog_last_reported_x
#define jog_last_reported_y commManagerState().jog_last_reported_y
#define jog_last_reported_z commManagerState().jog_last_reported_z
#define jog_last_state commManagerState().jog_last_state

void CommManager::init() {
    if (initialized) {
        return;
    }
    FluidNCClient::init();
    GrblComm::init();
    initialized = true;
}

bool CommManager::connect(const MachineConfig &config) {
    currentConfig = config;
    currentType = config.connection_type;
    setJogSoftLimits(config.soft_limit_x_enabled,
                     config.soft_limit_y_enabled,
                     config.soft_limit_z_enabled,
                     config.soft_limit_x_min,
                     config.soft_limit_x_max,
                     config.soft_limit_y_min,
                     config.soft_limit_y_max,
                     config.soft_limit_z_min,
                     config.soft_limit_z_max);
    resetJogTracking();
    bool ok = false;
    if (useGrbl()) {
        ok = GrblComm::connect(config);
    } else {
        ok = FluidNCClient::connect(config);
    }
    applyCallbacks();
    return ok;
}

void CommManager::disconnect() {
    resetJogTracking();
    if (useGrbl()) {
        GrblComm::disconnect();
        return;
    }
    FluidNCClient::disconnect();
}

void CommManager::stopReconnectionAttempts() {
    if (useGrbl()) {
        return;
    }
    FluidNCClient::stopReconnectionAttempts();
}

bool CommManager::isConnected() {
    return useGrbl() ? GrblComm::isConnected() : FluidNCClient::isConnected();
}

bool CommManager::isAutoReporting() {
    return useGrbl() ? GrblComm::isAutoReporting() : FluidNCClient::isAutoReporting();
}

void CommManager::loop() {
    if (useGrbl()) {
        GrblComm::loop();
    } else {
        FluidNCClient::loop();
    }
    updateJogTrackingFromStatus(getStatus());
}

const FluidNCStatus& CommManager::getStatus() {
    return useGrbl() ? GrblComm::getStatus() : FluidNCClient::getStatus();
}

void CommManager::sendCommand(const char* command) {
    if (commandTapStorage()) {
        commandTapStorage()(command);
    }
    if (useGrbl()) {
        GrblComm::sendCommand(command);
        return;
    }
    FluidNCClient::sendCommand(command);
}

bool CommManager::sendJog(const JogCommand &cmd) {
    if (!isConnected()) {
        return false;
    }

    const FluidNCStatus &status = getStatus();
    float predicted_x, predicted_y, predicted_z;
    getPredictedWpos(status, predicted_x, predicted_y, predicted_z);

    bool include_x = cmd.has_x;
    bool include_y = cmd.has_y;
    bool include_z = cmd.has_z;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    float delta_z = 0.0f;

    if (include_x) {
        target_x = cmd.absolute ? cmd.x : (predicted_x + cmd.x);
        target_x = clampJogTarget(predicted_x, target_x, jog_soft_limit_x_enabled, jog_soft_limit_x_min, jog_soft_limit_x_max);
        delta_x = target_x - predicted_x;
        if (delta_x == 0.0f) include_x = false;
    }
    if (include_y) {
        target_y = cmd.absolute ? cmd.y : (predicted_y + cmd.y);
        target_y = clampJogTarget(predicted_y, target_y, jog_soft_limit_y_enabled, jog_soft_limit_y_min, jog_soft_limit_y_max);
        delta_y = target_y - predicted_y;
        if (delta_y == 0.0f) include_y = false;
    }
    if (include_z) {
        target_z = cmd.absolute ? cmd.z : (predicted_z + cmd.z);
        target_z = clampJogTarget(predicted_z, target_z, jog_soft_limit_z_enabled, jog_soft_limit_z_min, jog_soft_limit_z_max);
        delta_z = target_z - predicted_z;
        if (delta_z == 0.0f) include_z = false;
    }

    if (!include_x && !include_y && !include_z) {
        return false;
    }

    char jog_cmd[128];
    size_t len = 0;
    len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, "$J=");
    if (cmd.units && cmd.units[0] != '\0') {
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, "%s", cmd.units);
    }
    len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, "%s", cmd.absolute ? "G90" : "G91");
    if (include_x) {
        float value = cmd.absolute ? target_x : delta_x;
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " X%.4f", value);
    }
    if (include_y) {
        float value = cmd.absolute ? target_y : delta_y;
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " Y%.4f", value);
    }
    if (include_z) {
        float value = cmd.absolute ? target_z : delta_z;
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " Z%.4f", value);
    }
    snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " F%.0f\n", cmd.feedrate);

    sendCommand(jog_cmd);

    if (jog_pending_valid) {
        if (include_x) jog_pending_x += (cmd.absolute ? (target_x - predicted_x) : delta_x);
        if (include_y) jog_pending_y += (cmd.absolute ? (target_y - predicted_y) : delta_y);
        if (include_z) jog_pending_z += (cmd.absolute ? (target_z - predicted_z) : delta_z);
        jog_pending_command_count++;
    }

    return true;
}

bool CommManager::sendJogRelative(float x, float y, float z, float feedrate) {
    JogCommand cmd{};
    cmd.absolute = false;
    cmd.has_x = (x != 0.0f);
    cmd.has_y = (y != 0.0f);
    cmd.has_z = (z != 0.0f);
    cmd.x = x;
    cmd.y = y;
    cmd.z = z;
    cmd.feedrate = feedrate;
    cmd.units = nullptr;
    return sendJog(cmd);
}

bool CommManager::sendJogRelativeAxis(char axis, float delta, float feedrate) {
    JogCommand cmd{};
    cmd.absolute = false;
    cmd.has_x = (axis == 'X' || axis == 'x');
    cmd.has_y = (axis == 'Y' || axis == 'y');
    cmd.has_z = (axis == 'Z' || axis == 'z');
    cmd.x = cmd.has_x ? delta : 0.0f;
    cmd.y = cmd.has_y ? delta : 0.0f;
    cmd.z = cmd.has_z ? delta : 0.0f;
    cmd.feedrate = feedrate;
    cmd.units = nullptr;
    return sendJog(cmd);
}

bool CommManager::getJogPredictedWpos(float &x, float &y, float &z) {
    const FluidNCStatus &status = getStatus();
    getPredictedWpos(status, x, y, z);
    return jog_pending_valid && status.is_connected;
}

int32_t CommManager::getJogPendingCommandCount() {
    return jog_pending_command_count;
}

void CommManager::setJogSoftLimits(bool x_enabled, bool y_enabled, bool z_enabled,
                                   float x_min, float x_max,
                                   float y_min, float y_max,
                                   float z_min, float z_max) {
    jog_soft_limit_x_enabled = x_enabled;
    jog_soft_limit_y_enabled = y_enabled;
    jog_soft_limit_z_enabled = z_enabled;
    jog_soft_limit_x_min = x_min;
    jog_soft_limit_x_max = x_max;
    jog_soft_limit_y_min = y_min;
    jog_soft_limit_y_max = y_max;
    jog_soft_limit_z_min = z_min;
    jog_soft_limit_z_max = z_max;

    // Soft-limit changes can invalidate pending predicted offsets.
    // Re-anchor prediction to the latest reported machine status.
    const FluidNCStatus &status = getStatus();
    resetJogTracking();
    updateJogTrackingFromStatus(status);
}

void CommManager::requestStatusReport() {
    if (useGrbl()) {
        GrblComm::requestStatusReport();
        return;
    }
    FluidNCClient::requestStatusReport();
}

String CommManager::getMachineIP() {
    return useGrbl() ? String() : FluidNCClient::getMachineIP();
}

void CommManager::setMessageCallback(MessageCallback callback) {
    messageCallbackStorage() = callback;
    applyCallbacks();
}

void CommManager::clearMessageCallback() {
    messageCallbackStorage() = nullptr;
    applyCallbacks();
}

void CommManager::setTerminalCallback(MessageCallback callback) {
    terminalCallbackStorage() = callback;
    applyCallbacks();
}

void CommManager::clearTerminalCallback() {
    terminalCallbackStorage() = nullptr;
    applyCallbacks();
}

void CommManager::setCommandTap(CommandTap tap) {
    commandTapStorage() = tap;
}

void CommManager::clearCommandTap() {
    commandTapStorage() = nullptr;
}

void CommManager::setEventCallback(EventCallback callback) {
    eventCallbackStorage() = callback;
}

void CommManager::clearEventCallback() {
    eventCallbackStorage() = nullptr;
}

void CommManager::emitConnected() {
    if (!eventCallbackStorage()) {
        return;
    }
    Event event{};
    event.type = EventType::CONNECTED;
    event.message = nullptr;
    eventCallbackStorage()(event);
}

void CommManager::emitDisconnected(const char *message) {
    if (!eventCallbackStorage()) {
        return;
    }
    Event event{};
    event.type = EventType::DISCONNECTED;
    event.message = message;
    eventCallbackStorage()(event);
}

void CommManager::emitConnectionError(const char *message) {
    if (!eventCallbackStorage()) {
        return;
    }
    Event event{};
    event.type = EventType::CONNECTION_ERROR;
    event.message = message;
    eventCallbackStorage()(event);
}

void CommManager::emitProbeResult(float x, float y, float z, bool success) {
    if (!eventCallbackStorage()) {
        return;
    }
    Event event{};
    event.type = EventType::PROBE_RESULT;
    event.message = nullptr;
    event.probe = {x, y, z, success};
    eventCallbackStorage()(event);
}

ConnectionType CommManager::getConnectionType() {
    return currentType;
}

bool CommManager::useGrbl() {
    if (currentType == CONN_UART) {
        return true;
    }
    // Treat explicit wired-serial targets as pendant/MPG transport too.
    return (currentType == CONN_WIRED && currentConfig.serial_port[0] != '\0');
}

void CommManager::applyCallbacks() {
    if (messageCallbackStorage()) {
        FluidNCClient::setMessageCallback(messageCallbackStorage());
        GrblComm::setMessageCallback(messageCallbackStorage());
    } else {
        FluidNCClient::clearMessageCallback();
        GrblComm::clearMessageCallback();
    }

    if (terminalCallbackStorage()) {
        FluidNCClient::setTerminalCallback(terminalCallbackStorage());
        GrblComm::setTerminalCallback(terminalCallbackStorage());
    } else {
        FluidNCClient::clearTerminalCallback();
        GrblComm::clearTerminalCallback();
    }
}

void CommManager::resetJogTracking() {
    jog_pending_valid = false;
    jog_pending_x = 0.0f;
    jog_pending_y = 0.0f;
    jog_pending_z = 0.0f;
    jog_pending_command_count = 0;
    jog_last_reported_x = 0.0f;
    jog_last_reported_y = 0.0f;
    jog_last_reported_z = 0.0f;
    jog_last_state = STATE_DISCONNECTED;
}

void CommManager::updateJogTrackingFromStatus(const FluidNCStatus &status) {
    if (!status.is_connected) {
        jog_pending_valid = false;
        jog_last_state = STATE_DISCONNECTED;
        return;
    }

    const bool jog_to_idle = (jog_last_state == STATE_JOG && status.state == STATE_IDLE);
    jog_last_state = status.state;

    if (!jog_pending_valid) {
        jog_last_reported_x = status.wpos_x;
        jog_last_reported_y = status.wpos_y;
        jog_last_reported_z = status.wpos_z;
        jog_pending_x = 0.0f;
        jog_pending_y = 0.0f;
        jog_pending_z = 0.0f;
        jog_pending_valid = true;
        return;
    }

    if (jog_to_idle) {
        jog_last_reported_x = status.wpos_x;
        jog_last_reported_y = status.wpos_y;
        jog_last_reported_z = status.wpos_z;
        jog_pending_x = 0.0f;
        jog_pending_y = 0.0f;
        jog_pending_z = 0.0f;
        jog_pending_command_count = 0;
        return;
    }

    bool changed = (status.wpos_x != jog_last_reported_x) ||
                   (status.wpos_y != jog_last_reported_y) ||
                   (status.wpos_z != jog_last_reported_z);
    if (!changed) {
        return;
    }

    float target_x = jog_last_reported_x + jog_pending_x;
    float target_y = jog_last_reported_y + jog_pending_y;
    float target_z = jog_last_reported_z + jog_pending_z;

    jog_last_reported_x = status.wpos_x;
    jog_last_reported_y = status.wpos_y;
    jog_last_reported_z = status.wpos_z;

    jog_pending_x = target_x - jog_last_reported_x;
    jog_pending_y = target_y - jog_last_reported_y;
    jog_pending_z = target_z - jog_last_reported_z;
    if (jog_pending_command_count > 0) {
        jog_pending_command_count--;
    }
}

void CommManager::getPredictedWpos(const FluidNCStatus &status, float &x, float &y, float &z) {
    updateJogTrackingFromStatus(status);
    if (!jog_pending_valid) {
        x = status.wpos_x;
        y = status.wpos_y;
        z = status.wpos_z;
        return;
    }
    x = jog_last_reported_x + jog_pending_x;
    y = jog_last_reported_y + jog_pending_y;
    z = jog_last_reported_z + jog_pending_z;
}

float CommManager::clampJogTarget(float current, float target, bool enabled, float min_limit, float max_limit) {
    if (!enabled) {
        return target;
    }
    if (min_limit > max_limit) {
        float tmp = min_limit;
        min_limit = max_limit;
        max_limit = tmp;
    }

    // If already below min, allow only recovery moves in the positive direction.
    if (current < min_limit) {
        if (target <= current) {
            return current;
        }
        if (target > max_limit) {
            return max_limit;
        }
        return target;
    }

    // If already above max, allow only recovery moves in the negative direction.
    if (current > max_limit) {
        if (target >= current) {
            return current;
        }
        if (target < min_limit) {
            return min_limit;
        }
        return target;
    }

    // Normal in-range clamping.
    if (target < min_limit) return min_limit;
    if (target > max_limit) return max_limit;
    return target;
}
