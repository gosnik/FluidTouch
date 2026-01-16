#ifndef GRBL_COMM_H
#define GRBL_COMM_H

#include <Arduino.h>
#include "core/comm_types.h"
#include "ui/machine_config.h"

class GrblComm {
public:
    static void init();
    static bool connect(const MachineConfig &config);
    static void disconnect();
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

private:
    static HardwareSerial *serialPort;
    static FluidNCStatus currentStatus;
    static uint32_t lastStatusRequestMs;
    static uint32_t lastStatusRxMs;
    static bool initialized;
    static MessageCallback messageCallback;
    static MessageCallback terminalCallback;
    static char lineBuffer[256];
    static size_t lineLen;

    static void handleLine(const char* line);
    static void parseStatusReport(const char* message);
    static void parseGCodeState(const char* message);
    static void parseRealtimeFeedback(const char* message);
};

#endif // GRBL_COMM_H
