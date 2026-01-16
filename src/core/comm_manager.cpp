#include "core/comm_manager.h"
#include "core/grbl_comm.h"
#include "network/fluidnc_client.h"

ConnectionType CommManager::currentType = CONN_WIRELESS;
MachineConfig CommManager::currentConfig;
bool CommManager::initialized = false;
MessageCallback CommManager::messageCallback = nullptr;
MessageCallback CommManager::terminalCallback = nullptr;
CommManager::EventCallback CommManager::eventCallback = nullptr;

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
        return;
    }
    FluidNCClient::loop();
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
