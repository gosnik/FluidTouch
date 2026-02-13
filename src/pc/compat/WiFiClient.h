#pragma once

#include <cstdint>
#include <cstddef>

class WiFiClient {
public:
    bool connect(const char *, uint16_t) { return false; }
    void stop() {}
    bool connected() { return false; }

    size_t print(const char *) { return 0; }
    size_t write(const uint8_t *, size_t) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
};
