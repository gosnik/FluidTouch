#ifndef FLUIDNC_CLIENT_H
#define FLUIDNC_CLIENT_H

#include <Arduino.h>
#include "esp_event.h"
#include "esp_websocket_client.h"
#include "ui/machine_config.h"
#include "core/comm_types.h"

class FluidNCClient {
public:
    // Initialize the client
    static void init();
    
    // Connect to FluidNC using machine config
    static bool connect(const MachineConfig &config);
    
    // Disconnect from FluidNC
    static void disconnect();
    
    // Stop reconnection attempts (sets reconnect interval to 24 hours)
    static void stopReconnectionAttempts();
    
    // Check if connected
    static bool isConnected();
    
    // Check if using auto-reporting (true) or fallback polling (false)
    static bool isAutoReporting();
    
    // Main loop - call regularly to handle WebSocket events
    static void loop();
    
    // Get current status
    static const FluidNCStatus& getStatus();
    
    // Send a command to FluidNC (e.g., "G0 X10", "$H", "!")
    static void sendCommand(const char* command);
    
    // Request status report (sends "?")
    static void requestStatusReport();
    
    // Get machine IP address (extracted from WebSocket URL)
    static String getMachineIP();
    
    // Set callback for receiving raw messages (for file list, etc.)
    static void setMessageCallback(MessageCallback callback);
    
    // Clear message callback
    static void clearMessageCallback();
    
    // Set callback for terminal display (excludes status reports starting with '<')
    static void setTerminalCallback(MessageCallback callback);
    
    // Clear terminal callback
    static void clearTerminalCallback();
    
private:
    static esp_websocket_client_handle_t webSocket;
    static FluidNCStatus currentStatus;
    static MachineConfig currentConfig;
    static uint32_t lastStatusRequestMs;
    static bool initialized;
    static MessageCallback messageCallback;  // Optional callback for raw messages
    static MessageCallback terminalCallback; // Optional callback for terminal display
    
    // Auto-reporting and fallback polling
    static bool autoReportingEnabled;     // True if auto-reporting is active
    static bool autoReportingAttempted;   // True if we tried to enable auto-reporting
    static uint32_t lastPollingMs;        // Last time we sent "?" for fallback polling
    static uint32_t lastGCodePollMs;      // Last time we sent "$G" for GCode state
    static uint32_t lastAutoReportAttemptMs; // Last time we tried to enable auto-reporting
    
    // Connection tracking
    static bool everConnectedSuccessfully; // True once first status report received, never reset
    static bool reconnectEnabled;
    static uint32_t reconnectIntervalMs;
    static uint32_t lastReconnectAttemptMs;
    
    // WebSocket event handler
    static void onWebSocketEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    
    // Parse status report message
    static void parseStatusReport(const char* message);
    
    // Parse GCode parser state message
    static void parseGCodeState(const char* message);
    
    // Parse realtime feedback
    static void parseRealtimeFeedback(const char* message);
    
    // Auto-reporting and polling helpers
    static void attemptEnableAutoReporting();
    static void performFallbackPolling();
    
    // Helper to extract float value from status report
    static float extractFloat(const char* str, const char* key);
    
    // Helper to extract string value from status report
    static void extractString(const char* str, const char* key, char* dest, size_t maxLen);
};

#endif // FLUIDNC_CLIENT_H
