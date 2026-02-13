#include "core/comm_manager.h"
#include "Arduino.h"
#include <cstring>

ConnectionType CommManager::currentType = CONN_WIRED;
MachineConfig CommManager::currentConfig = {};
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

namespace {
bool g_connected = false;
bool g_auto_reporting = false;
FluidNCStatus g_status;
}

void CommManager::init() {
    initialized = true;
    g_connected = false;
    g_auto_reporting = false;
    g_status = FluidNCStatus();
}

bool CommManager::connect(const MachineConfig &config) {
    currentConfig = config;
    currentType = config.connection_type;
    g_connected = true;
    g_status.is_connected = true;
    g_status.state = STATE_IDLE;
    g_status.last_update_ms = millis();
    emitConnected();
    return true;
}

void CommManager::disconnect() {
    g_connected = false;
    g_status.is_connected = false;
    g_status.state = STATE_DISCONNECTED;
    emitDisconnected("Simulator disconnect");
}

void CommManager::stopReconnectionAttempts() {}

bool CommManager::isConnected() {
    return g_connected;
}

bool CommManager::isAutoReporting() {
    return g_auto_reporting;
}

void CommManager::loop() {
    if (!g_connected) {
        return;
    }
    g_status.last_update_ms = millis();
}

const FluidNCStatus &CommManager::getStatus() {
    return g_status;
}

void CommManager::sendCommand(const char *command) {
    if (terminalCallback && command) {
        terminalCallback(command);
    }
}

bool CommManager::sendJog(const JogCommand &) {
    return g_connected;
}

bool CommManager::sendJogRelative(float, float, float, float) {
    return g_connected;
}

bool CommManager::sendJogRelativeAxis(char, float, float) {
    return g_connected;
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

void CommManager::requestStatusReport() {}

String CommManager::getMachineIP() {
    return String();
}

void CommManager::setMessageCallback(MessageCallback callback) {
    messageCallback = callback;
}

void CommManager::clearMessageCallback() {
    messageCallback = nullptr;
}

void CommManager::setTerminalCallback(MessageCallback callback) {
    terminalCallback = callback;
}

void CommManager::clearTerminalCallback() {
    terminalCallback = nullptr;
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
    event.message = "Simulator connected";
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
    event.probe = {x, y, z, success};
    eventCallback(event);
}

ConnectionType CommManager::getConnectionType() {
    return currentType;
}
