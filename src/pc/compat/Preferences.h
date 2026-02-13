#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include "Arduino.h"

class Preferences {
public:
    Preferences() = default;

    bool begin(const char *ns, bool readOnly = false) {
        namespace_ = ns ? ns : "";
        read_only_ = readOnly;
        return true;
    }

    void end() {}

    void clear() {
        if (read_only_) {
            return;
        }
        store()[namespace_].clear();
    }

    bool getBool(const char *key, bool defaultValue = false) {
        return getValue(key, defaultValue ? "1" : "0") == "1";
    }

    int getInt(const char *key, int defaultValue = 0) {
        return std::atoi(getValue(key, std::to_string(defaultValue)).c_str());
    }

    uint8_t getUChar(const char *key, uint8_t defaultValue = 0) {
        return static_cast<uint8_t>(std::atoi(getValue(key, std::to_string(defaultValue)).c_str()));
    }

    uint16_t getUShort(const char *key, uint16_t defaultValue = 0) {
        return static_cast<uint16_t>(std::atoi(getValue(key, std::to_string(defaultValue)).c_str()));
    }

    float getFloat(const char *key, float defaultValue = 0.0f) {
        return std::strtof(getValue(key, std::to_string(defaultValue)).c_str(), nullptr);
    }

    String getString(const char *key, const char *defaultValue = "") {
        return String(getValue(key, defaultValue ? defaultValue : ""));
    }

    size_t getString(const char *key, char *value, size_t maxLen) {
        std::string stored = getValue(key, "");
        if (maxLen == 0 || value == nullptr) {
            return 0;
        }
        size_t copyLen = std::min(maxLen - 1, stored.size());
        std::memcpy(value, stored.data(), copyLen);
        value[copyLen] = '\0';
        return copyLen;
    }

    void putBool(const char *key, bool value) {
        setValue(key, value ? "1" : "0");
    }

    void putInt(const char *key, int value) {
        setValue(key, std::to_string(value));
    }

    void putUChar(const char *key, uint8_t value) {
        setValue(key, std::to_string(value));
    }

    void putUShort(const char *key, uint16_t value) {
        setValue(key, std::to_string(value));
    }

    void putFloat(const char *key, float value) {
        setValue(key, std::to_string(value));
    }

    void putString(const char *key, const char *value) {
        setValue(key, value ? value : "");
    }

    size_t getBytesLength(const char *key) {
        auto nsIt = store().find(namespace_);
        if (nsIt == store().end()) {
            return 0;
        }
        auto keyIt = nsIt->second.find(key ? key : "");
        if (keyIt == nsIt->second.end()) {
            return 0;
        }
        return keyIt->second.size();
    }

    size_t getBytes(const char *key, void *buf, size_t maxLen) {
        if (!buf || maxLen == 0) {
            return 0;
        }
        auto nsIt = store().find(namespace_);
        if (nsIt == store().end()) {
            return 0;
        }
        auto keyIt = nsIt->second.find(key ? key : "");
        if (keyIt == nsIt->second.end()) {
            return 0;
        }
        size_t copyLen = std::min(maxLen, keyIt->second.size());
        std::memcpy(buf, keyIt->second.data(), copyLen);
        return copyLen;
    }

    void putBytes(const char *key, const void *buf, size_t length) {
        if (read_only_) {
            return;
        }
        if (!buf || length == 0) {
            store()[namespace_][key ? key : ""].clear();
            return;
        }
        store()[namespace_][key ? key : ""] =
            std::string(static_cast<const char *>(buf), length);
    }

private:
    static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &store() {
        static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> s_store;
        return s_store;
    }

    std::string getValue(const char *key, const std::string &defaultValue) const {
        auto nsIt = store().find(namespace_);
        if (nsIt == store().end()) {
            return defaultValue;
        }
        auto keyIt = nsIt->second.find(key ? key : "");
        if (keyIt == nsIt->second.end()) {
            return defaultValue;
        }
        return keyIt->second;
    }

    void setValue(const char *key, const std::string &value) {
        if (read_only_) {
            return;
        }
        store()[namespace_][key ? key : ""] = value;
    }

    std::string namespace_;
    bool read_only_ = false;
};
