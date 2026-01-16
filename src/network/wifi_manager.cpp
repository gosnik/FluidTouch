#include "network/wifi_manager.h"

#if FT_WIFI_ENABLED
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include <cstring>

#if CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED
#include "esp_wifi_remote.h"
#endif

#if CONFIG_ESP_HOSTED_ENABLED
#include "esp_hosted.h"
#endif

namespace {
constexpr const char *kIpUnavailable = "Not connected";

bool s_initialized = false;
bool s_started = false;
bool s_connected = false;
esp_netif_t *s_netif = nullptr;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void)arg;
    (void)event_data;

#if CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED
    const esp_event_base_t wifi_base = WIFI_REMOTE_EVENT;
#else
    const esp_event_base_t wifi_base = WIFI_EVENT;
#endif

    if (event_base == wifi_base) {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
        }
    }

    if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            s_connected = true;
        } else if (event_id == IP_EVENT_STA_LOST_IP) {
            s_connected = false;
        }
    }
}

static void ensure_event_loop() {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WifiManager] esp_netif_init failed: %s\n", esp_err_to_name(err));
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WifiManager] esp_event_loop_create_default failed: %s\n", esp_err_to_name(err));
    }
}

static void ensure_netif() {
    if (s_netif) {
        return;
    }

    s_netif = esp_netif_create_default_wifi_sta();

    if (!s_netif) {
        Serial.println("[WifiManager] Failed to create default STA netif");
    }
}
}  // namespace

bool WifiManager::init() {
    if (s_initialized) {
        return true;
    }

    ensure_event_loop();
    ensure_netif();

#if CONFIG_ESP_HOSTED_ENABLED
    int hosted_err = esp_hosted_init();
    if (hosted_err != ESP_OK && hosted_err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WifiManager] esp_hosted_init failed: %s\n", esp_err_to_name(hosted_err));
        return false;
    }
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE && err != ESP_ERR_WIFI_INIT_STATE) {
        Serial.printf("[WifiManager] esp_wifi_init failed: %s\n", esp_err_to_name(err));
        return false;
    }

#if CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED
    esp_event_base_t wifi_base = WIFI_REMOTE_EVENT;
#else
    esp_event_base_t wifi_base = WIFI_EVENT;
#endif

    esp_event_handler_instance_t wifi_instance;
    esp_event_handler_instance_t ip_instance;
    err = esp_event_handler_instance_register(wifi_base, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, &wifi_instance);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WifiManager] WiFi event handler register failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, &ip_instance);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WifiManager] IP event handler register failed: %s\n", esp_err_to_name(err));
        return false;
    }

    s_initialized = true;
    return true;
}

bool WifiManager::connect(const char *ssid, const char *password) {
    if (!init()) {
        return false;
    }

    if (!ssid || ssid[0] == '\0') {
        Serial.println("[WifiManager] Missing SSID");
        return false;
    }

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy(reinterpret_cast<char *>(wifi_config.sta.password), password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        Serial.printf("[WifiManager] esp_wifi_set_mode failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        Serial.printf("[WifiManager] esp_wifi_set_config failed: %s\n", esp_err_to_name(err));
        return false;
    }

    s_connected = false;

    if (!s_started) {
        err = esp_wifi_start();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            Serial.printf("[WifiManager] esp_wifi_start failed: %s\n", esp_err_to_name(err));
            return false;
        }
        s_started = true;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WifiManager] esp_wifi_connect failed: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

void WifiManager::disconnect(bool stop_wifi) {
    if (!s_initialized) {
        return;
    }

    esp_wifi_disconnect();
    s_connected = false;

    if (stop_wifi) {
        esp_wifi_stop();
        s_started = false;
    }
}

bool WifiManager::isConnected() {
    return s_connected;
}

String WifiManager::getLocalIpString() {
    if (!s_netif || !s_connected) {
        return String(kIpUnavailable);
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_netif, &ip_info) != ESP_OK) {
        return String(kIpUnavailable);
    }

    char buf[IP4ADDR_STRLEN_MAX] = {};
    snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip_info.ip));
    return String(buf);
}

String WifiManager::getSsid() {
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return String();
    }
    return String(reinterpret_cast<const char *>(ap_info.ssid));
}

bool WifiManager::resolveHostname(const char *hostname, String &out_ip) {
    if (!hostname || hostname[0] == '\0') {
        return false;
    }

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = nullptr;
    int err = getaddrinfo(hostname, nullptr, &hints, &result);
    if (err != 0 || !result) {
        return false;
    }

    char buf[INET_ADDRSTRLEN] = {};
    struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(result->ai_addr);
    if (!inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf))) {
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);
    out_ip = String(buf);
    return true;
}
#endif
