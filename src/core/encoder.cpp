
#include "core/encoder.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

Encoder::Encoder(pcnt_unit_t unit) : unit_(unit), a_pin_(-1), b_pin_(-1) {}

bool Encoder::init(int a_pin, int b_pin) {
  a_pin_ = a_pin;
  b_pin_ = b_pin;

  pcnt_config_t enc_config = {
      .pulse_gpio_num = a_pin_, // Rotary Encoder Chan A
      .ctrl_gpio_num = b_pin_,  // Rotary Encoder Chan B

      .lctrl_mode = PCNT_MODE_KEEP,    // Rising A on HIGH B = CW Step
      .hctrl_mode = PCNT_MODE_REVERSE, // Rising A on LOW B = CCW Step
      .pos_mode = PCNT_COUNT_INC,      // Count Only On Rising-Edges
      .neg_mode = PCNT_COUNT_DEC,      // Discard Falling-Edge

      .counter_h_lim = INT16_MAX,
      .counter_l_lim = INT16_MIN,

      .unit = unit_,
      .channel = PCNT_CHANNEL_0,
  };
  if (pcnt_unit_config(&enc_config) != ESP_OK) {
    return false;
  }

  enc_config.pulse_gpio_num = b_pin_;
  enc_config.ctrl_gpio_num = a_pin_;
  enc_config.channel = PCNT_CHANNEL_1;
  enc_config.pos_mode = PCNT_COUNT_DEC; // Count Only On Falling-Edges
  enc_config.neg_mode = PCNT_COUNT_INC; // Discard Rising-Edge
  if (pcnt_unit_config(&enc_config) != ESP_OK) {
    return false;
  }

  pcnt_set_filter_value(unit_, 250); // Filter runt pulses
  pcnt_filter_enable(unit_);

  gpio_pullup_en((gpio_num_t)a_pin_);
  gpio_pullup_en((gpio_num_t)b_pin_);

  pcnt_counter_pause(unit_);
  pcnt_counter_clear(unit_);
  pcnt_counter_resume(unit_);
  return true;
}

int16_t Encoder::get() const {
  int16_t count = 0;
  pcnt_get_counter_value(unit_, &count);
  return count;
}

void Encoder::clear() const { pcnt_counter_clear(unit_); }

pcnt_unit_t Encoder::unit() const { return unit_; }

namespace {
constexpr size_t kEncoderCount = 3;
Encoder encoders[kEncoderCount] = {Encoder(PCNT_UNIT_0), Encoder(PCNT_UNIT_1), Encoder(PCNT_UNIT_2)};
bool encoder_ready[kEncoderCount] = {false, false, false};
}  // namespace

bool init_encoders(const EncoderPins *pins, size_t count) {
  if (!pins || count == 0) {
    return false;
  }

  bool ok = true;
  size_t limit = (count < kEncoderCount) ? count : kEncoderCount;
  for (size_t i = 0; i < limit; ++i) {
    if (pins[i].a_pin < 0 || pins[i].b_pin < 0) {
      encoder_ready[i] = false;
      continue;
    }
    encoder_ready[i] = encoders[i].init(pins[i].a_pin, pins[i].b_pin);
    ok = ok && encoder_ready[i];
  }
  return ok;
}

int16_t get_encoder_value(size_t index) {
  if (index >= kEncoderCount || !encoder_ready[index]) {
    return 0;
  }
  return encoders[index].get();
}

void clear_encoder_value(size_t index) {
  if (index >= kEncoderCount || !encoder_ready[index]) {
    return;
  }
  encoders[index].clear();
}

void init_encoder(int a_pin, int b_pin) {
  EncoderPins pins = {.a_pin = a_pin, .b_pin = b_pin};
  init_encoders(&pins, 1);
}

int16_t get_encoder() { return get_encoder_value(0); }
