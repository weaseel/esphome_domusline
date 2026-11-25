#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace domusline {

class DomusLine : public PollingComponent, public light::LightOutput {
 public:
  DomusLine();

  void set_cs_pin(uint8_t pin);
  void set_ready_pin(uint8_t pin);

  void setup() override;
  void update() override;
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 private:
  void get_current_values(light::LightState *state, uint8_t &current_temperature, uint8_t &current_brightness);
  void send_spi_package(uint8_t *data, uint8_t length);
  void send_spi_package_CRC(uint8_t *data, uint8_t length);
  void wait_send_spi_package(uint8_t *data, uint8_t length);
  void init_cc2500();
  void cc2500_on(uint8_t current_temperature);
  void cc2500_off();
  void cc2500_increaseBrightness();
  void cc2500_decreaseBrightness();
  void cc2500_temperature(uint8_t current_temperature);
  void cc2500_lowPower();
  uint8_t cc2500_crc(uint8_t *data, std::size_t length);

  uint8_t CS = 5;
  uint8_t readyPin = 19;
  uint8_t actualBrightness = 10;
  uint8_t setBrightness = 10;
  uint8_t lastTemperature = 0;
  bool lightIsOn = false;
  bool writeCallbackIsRunning = false;
};

}
}