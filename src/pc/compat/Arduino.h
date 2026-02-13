#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>
#include <cctype>
#include <cmath>

#define HEX 16

class String {
public:
    String() = default;
    String(const char *value) : data_(value ? value : "") {}
    String(const std::string &value) : data_(value) {}
    String(int value) : data_(std::to_string(value)) {}
    String(unsigned int value) : data_(std::to_string(value)) {}
    String(long value) : data_(std::to_string(value)) {}
    String(unsigned long value) : data_(std::to_string(value)) {}
    String(float value) : data_(std::to_string(value)) {}
    String(double value) : data_(std::to_string(value)) {}
    String(unsigned long value, int base) { data_ = toBase(value, base); }

    const char *c_str() const { return data_.c_str(); }
    size_t length() const { return data_.length(); }
    void reserve(size_t size) { data_.reserve(size); }
    void clear() { data_.clear(); }

    void remove(size_t index) {
        if (index < data_.size()) {
            data_.erase(index);
        }
    }

    void remove(size_t index, size_t count) {
        if (index < data_.size()) {
            data_.erase(index, count);
        }
    }

    int indexOf(char value, size_t fromIndex = 0) const {
        if (fromIndex >= data_.size()) {
            return -1;
        }
        size_t pos = data_.find(value, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    String substring(size_t from) const {
        if (from >= data_.size()) {
            return String();
        }
        return String(data_.substr(from));
    }

    String substring(size_t from, size_t to) const {
        if (from >= data_.size() || to <= from) {
            return String();
        }
        size_t count = to - from;
        return String(data_.substr(from, count));
    }

    bool equals(const String &other) const { return data_ == other.data_; }
    bool equals(const char *other) const { return data_ == (other ? other : ""); }

    char operator[](size_t index) const { return data_[index]; }

    String &operator=(const String &other) = default;

    String &operator+=(const String &other) {
        data_ += other.data_;
        return *this;
    }

    String &operator+=(const char *other) {
        data_ += (other ? other : "");
        return *this;
    }

    friend String operator+(const String &lhs, const String &rhs) {
        return String(lhs.data_ + rhs.data_);
    }

    friend String operator+(const String &lhs, const char *rhs) {
        return String(lhs.data_ + (rhs ? rhs : ""));
    }

    friend String operator+(const char *lhs, const String &rhs) {
        return String(std::string(lhs ? lhs : "") + rhs.data_);
    }

    operator std::string() const { return data_; }

    bool startsWith(const char *prefix) const {
        if (!prefix) {
            return false;
        }
        size_t prefix_len = std::strlen(prefix);
        if (prefix_len > data_.size()) {
            return false;
        }
        return data_.compare(0, prefix_len, prefix) == 0;
    }

    bool endsWith(const char *suffix) const {
        if (!suffix) {
            return false;
        }
        size_t suffix_len = std::strlen(suffix);
        if (suffix_len > data_.size()) {
            return false;
        }
        return data_.compare(data_.size() - suffix_len, suffix_len, suffix) == 0;
    }

    bool equalsIgnoreCase(const char *other) const {
        if (!other) {
            return false;
        }
        size_t other_len = std::strlen(other);
        if (other_len != data_.size()) {
            return false;
        }
        for (size_t i = 0; i < data_.size(); i++) {
            if (std::tolower(static_cast<unsigned char>(data_[i])) !=
                std::tolower(static_cast<unsigned char>(other[i]))) {
                return false;
            }
        }
        return true;
    }

    bool isEmpty() const { return data_.empty(); }

    void replace(const char *find, const char *replace_with) {
        if (!find || !replace_with) {
            return;
        }
        std::string from(find);
        if (from.empty()) {
            return;
        }
        std::string to(replace_with);
        size_t pos = 0;
        while ((pos = data_.find(from, pos)) != std::string::npos) {
            data_.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    int read() {
        if (read_pos_ >= data_.size()) {
            return -1;
        }
        return static_cast<unsigned char>(data_[read_pos_++]);
    }

    int peek() const {
        if (read_pos_ >= data_.size()) {
            return -1;
        }
        return static_cast<unsigned char>(data_[read_pos_]);
    }

    void trim() {
        if (data_.empty()) {
            return;
        }
        size_t start = 0;
        while (start < data_.size() && std::isspace(static_cast<unsigned char>(data_[start]))) {
            start++;
        }
        size_t end = data_.size();
        while (end > start && std::isspace(static_cast<unsigned char>(data_[end - 1]))) {
            end--;
        }
        data_ = data_.substr(start, end - start);
        read_pos_ = 0;
    }

    void resetReadPos() { read_pos_ = 0; }

private:
    static std::string toBase(unsigned long value, int base) {
        if (base < 2 || base > 36) {
            return std::to_string(value);
        }
        std::string digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string result;
        do {
            result.insert(result.begin(), digits[value % base]);
            value /= base;
        } while (value > 0);
        return result;
    }

    std::string data_;
    mutable size_t read_pos_ = 0;
};

inline uint32_t millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

inline void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline long random(long max) {
    if (max <= 0) {
        return 0;
    }
    return std::rand() % max;
}

inline long random(long min, long max) {
    if (max <= min) {
        return min;
    }
    return min + (std::rand() % (max - min));
}

class SerialClass {
public:
    void begin(unsigned long) {}

    void print(const char *msg) { std::fputs(msg ? msg : "", stdout); }
    void print(const String &msg) { std::fputs(msg.c_str(), stdout); }

    void println(const char *msg = "") {
        std::fputs(msg ? msg : "", stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
    void println(const String &msg) { println(msg.c_str()); }

    void printf(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::vprintf(fmt, args);
        va_end(args);
        std::fflush(stdout);
    }

    void flush() { std::fflush(stdout); }
};

inline SerialClass Serial;

class ESPClass {
public:
    void restart() {}
    uint32_t getFreeHeap() const { return 0; }
    uint32_t getPsramSize() const { return 0; }
    uint32_t getFreePsram() const { return 0; }
};

inline ESPClass ESP;
