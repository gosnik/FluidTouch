#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <fstream>
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
        saveToDisk();
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
            saveToDisk();
            return;
        }
        store()[namespace_][key ? key : ""] =
            std::string(static_cast<const char *>(buf), length);
        saveToDisk();
    }

private:
    static std::string prefsFilePath() {
        return ".fluidtouch_prefs.db";
    }

    static std::string hexEncode(const std::string &input) {
        static const char *kHex = "0123456789ABCDEF";
        std::string out;
        out.reserve(input.size() * 2);
        for (unsigned char c : input) {
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
        return out;
    }

    static bool hexValue(char c, uint8_t &out) {
        if (c >= '0' && c <= '9') {
            out = static_cast<uint8_t>(c - '0');
            return true;
        }
        if (c >= 'A' && c <= 'F') {
            out = static_cast<uint8_t>(10 + (c - 'A'));
            return true;
        }
        if (c >= 'a' && c <= 'f') {
            out = static_cast<uint8_t>(10 + (c - 'a'));
            return true;
        }
        return false;
    }

    static std::string hexDecode(const std::string &input) {
        if (input.size() % 2 != 0) {
            return std::string();
        }
        std::string out;
        out.reserve(input.size() / 2);
        for (size_t i = 0; i < input.size(); i += 2) {
            uint8_t hi = 0;
            uint8_t lo = 0;
            if (!hexValue(input[i], hi) || !hexValue(input[i + 1], lo)) {
                return std::string();
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
        }
        return out;
    }

    static void loadFromDisk() {
        if (loaded_) {
            return;
        }
        loaded_ = true;

        std::ifstream in(prefsFilePath(), std::ios::in);
        if (!in.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(in, line)) {
            const size_t p1 = line.find('|');
            if (p1 == std::string::npos) continue;
            const size_t p2 = line.find('|', p1 + 1);
            if (p2 == std::string::npos) continue;

            const std::string ns = hexDecode(line.substr(0, p1));
            const std::string key = hexDecode(line.substr(p1 + 1, p2 - (p1 + 1)));
            const std::string value = hexDecode(line.substr(p2 + 1));
            store_[ns][key] = value;
        }
    }

    static void saveToDisk() {
        std::ofstream out(prefsFilePath(), std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return;
        }

        for (const auto &ns_pair : store_) {
            const std::string ns_hex = hexEncode(ns_pair.first);
            for (const auto &kv : ns_pair.second) {
                out << ns_hex << '|'
                    << hexEncode(kv.first) << '|'
                    << hexEncode(kv.second) << '\n';
            }
        }
    }

    static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &store() {
        loadFromDisk();
        return store_;
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
        saveToDisk();
    }

    std::string namespace_;
    bool read_only_ = false;
    static bool loaded_;
    static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> store_;
};

inline bool Preferences::loaded_ = false;
inline std::unordered_map<std::string, std::unordered_map<std::string, std::string>> Preferences::store_;
