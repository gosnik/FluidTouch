#include "core/comm_manager.h"
#include "core/grbl_comm.h"
#include "network/fluidnc_client.h"

ConnectionType CommManager::currentType = CONN_WIRELESS;
MachineConfig CommManager::currentConfig;
bool CommManager::initialized = false;
MessageCallback CommManager::messageCallback = nullptr;
MessageCallback CommManager::terminalCallback = nullptr;
CommManager::EventCallback CommManager::eventCallback = nullptr;
bool CommManager::jog_soft_limit_x_enabled = false;
bool CommManager::jog_soft_limit_y_enabled = false;
bool CommManager::jog_soft_limit_z_enabled = false;
float CommManager::jog_soft_limit_x_min = 0.0f;
float CommManager::jog_soft_limit_x_max = 0.0f;
float CommManager::jog_soft_limit_y_min = 0.0f;
float CommManager::jog_soft_limit_y_max = 0.0f;
float CommManager::jog_soft_limit_z_min = 0.0f;
float CommManager::jog_soft_limit_z_max = 0.0f;
bool CommManager::jog_pending_valid = false;
float CommManager::jog_pending_x = 0.0f;
float CommManager::jog_pending_y = 0.0f;
float CommManager::jog_pending_z = 0.0f;
float CommManager::jog_last_reported_x = 0.0f;
float CommManager::jog_last_reported_y = 0.0f;
float CommManager::jog_last_reported_z = 0.0f;

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
        target_x = clampJogTarget(target_x, jog_soft_limit_x_enabled, jog_soft_limit_x_min, jog_soft_limit_x_max);
        delta_x = target_x - predicted_x;
        if (delta_x == 0.0f) include_x = false;
    }
    if (include_y) {
        target_y = cmd.absolute ? cmd.y : (predicted_y + cmd.y);
        target_y = clampJogTarget(target_y, jog_soft_limit_y_enabled, jog_soft_limit_y_min, jog_soft_limit_y_max);
        delta_y = target_y - predicted_y;
        if (delta_y == 0.0f) include_y = false;
    }
    if (include_z) {
        target_z = cmd.absolute ? cmd.z : (predicted_z + cmd.z);
        target_z = clampJogTarget(target_z, jog_soft_limit_z_enabled, jog_soft_limit_z_min, jog_soft_limit_z_max);
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
    messageCallback = callback;
    applyCallbacks();
}

void CommManager::clearMessageCallback() {
    messageCallback = nullptr;
    applyCallbacks();
}

void CommManager::setTerminalCallback(MessageCallback callback) {
    terminalCallback = callback;
    applyCallbacks();
}

void CommManager::clearTerminalCallback() {
    terminalCallback = nullptr;
    applyCallbacks();
}

void CommManager::setEventCallback(EventCallback callback) {
    eventCallback = callback;
}

void CommManager::clearEventCallback() {
    eventCallback = nullptr;
}

void CommManager::emitConnected() {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::CONNECTED;
    event.message = nullptr;
    eventCallback(event);
}

void CommManager::emitDisconnected(const char *message) {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::DISCONNECTED;
    event.message = message;
    eventCallback(event);
}

void CommManager::emitConnectionError(const char *message) {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::CONNECTION_ERROR;
    event.message = message;
    eventCallback(event);
}

void CommManager::emitProbeResult(float x, float y, float z, bool success) {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::PROBE_RESULT;
    event.message = nullptr;
    event.probe = {x, y, z, success};
    eventCallback(event);
}

ConnectionType CommManager::getConnectionType() {
    return currentType;
}

bool CommManager::useGrbl() {
    return currentType == CONN_UART;
}

void CommManager::applyCallbacks() {
    if (messageCallback) {
        FluidNCClient::setMessageCallback(messageCallback);
        GrblComm::setMessageCallback(messageCallback);
    } else {
        FluidNCClient::clearMessageCallback();
        GrblComm::clearMessageCallback();
    }

    if (terminalCallback) {
        FluidNCClient::setTerminalCallback(terminalCallback);
        GrblComm::setTerminalCallback(terminalCallback);
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
    jog_last_reported_x = 0.0f;
    jog_last_reported_y = 0.0f;
    jog_last_reported_z = 0.0f;
}

void CommManager::updateJogTrackingFromStatus(const FluidNCStatus &status) {
    if (!status.is_connected) {
        jog_pending_valid = false;
        return;
    }
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

float CommManager::clampJogTarget(float target, bool enabled, float min_limit, float max_limit) {
    if (!enabled) {
        return target;
    }
    if (min_limit > max_limit) {
        float tmp = min_limit;
        min_limit = max_limit;
        max_limit = tmp;
    }
    if (target < min_limit) return min_limit;
    if (target > max_limit) return max_limit;
    return target;
}
