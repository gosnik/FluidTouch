#pragma once

#include <cstdint>
#include <cstddef>
#include "driver/pcnt.h"

class Encoder {
public:
    explicit Encoder(pcnt_unit_t unit);

    bool init(int a_pin, int b_pin);
    int16_t get() const;
    void clear() const;
    pcnt_unit_t unit() const;

private:
    pcnt_unit_t unit_;
    int a_pin_;
    int b_pin_;
};

struct EncoderPins {
    int a_pin;
    int b_pin;
};

// Multi-encoder helpers.
bool init_encoders(const EncoderPins *pins, size_t count);
int16_t get_encoder_value(size_t index);
void clear_encoder_value(size_t index);

// Legacy single-encoder helpers (PCNT unit 0).
int16_t get_encoder();
void init_encoder(int a_pin, int b_pin);
