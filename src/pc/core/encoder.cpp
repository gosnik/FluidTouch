#include "core/encoder.h"

Encoder::Encoder(pcnt_unit_t unit) : unit_(unit), a_pin_(-1), b_pin_(-1) {}

bool Encoder::init(int a_pin, int b_pin) {
    a_pin_ = a_pin;
    b_pin_ = b_pin;
    return true;
}

int16_t Encoder::get() const { return 0; }
void Encoder::clear() const {}
pcnt_unit_t Encoder::unit() const { return unit_; }

bool init_encoders(const EncoderPins *, size_t) { return true; }
int16_t get_encoder_value(size_t) { return 0; }
void clear_encoder_value(size_t) {}

int16_t get_encoder() { return 0; }
void init_encoder(int, int) {}
