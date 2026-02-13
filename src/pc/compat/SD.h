#pragma once

#include <cstdint>
#include <string>
#include "SPI.h"

enum {
    CARD_NONE = 0,
    CARD_MMC = 1,
    CARD_SD = 2,
    CARD_SDHC = 3
};

#ifndef FILE_READ
#define FILE_READ "r"
#endif
#ifndef FILE_WRITE
#define FILE_WRITE "w"
#endif

class File {
public:
    File() = default;
    explicit File(const std::string &name, bool is_dir = false, size_t size = 0, bool valid = false)
        : name_(name), is_dir_(is_dir), size_(size), valid_(valid) {}

    operator bool() const { return valid_; }

    const char *name() const { return name_.c_str(); }
    bool isDirectory() const { return is_dir_; }
    size_t size() const { return size_; }

    int available() const { return 0; }
    int read() { return -1; }
    size_t read(uint8_t *, size_t) { return 0; }
    size_t write(const uint8_t *, size_t) { return 0; }
    size_t write(uint8_t) { return 0; }
    void close() {}

    File openNextFile() { return File(); }

private:
    std::string name_;
    bool is_dir_ = false;
    size_t size_ = 0;
    bool valid_ = false;
};

class SDClass {
public:
    bool begin(int, SPIClass &, uint32_t = 0, const char * = nullptr, uint8_t = 0, bool = false) {
        return false;
    }

    bool begin(uint8_t) { return false; }

    uint8_t cardType() { return CARD_NONE; }
    uint64_t cardSize() { return 0; }

    File open(const char *, const char * = "r") { return File(); }
    bool exists(const char *) { return false; }
};

inline SDClass SD;
