#pragma once

#include "config.h"

#if FT_WIFI_ENABLED
#include <Arduino.h>
#include "esp_err.h"

namespace WifiManager {
bool init();
bool connect(const char *ssid, const char *password);
void disconnect(bool stop_wifi);
bool isConnected();
String getLocalIpString();
String getSsid();
bool resolveHostname(const char *hostname, String &out_ip);
}
#endif
