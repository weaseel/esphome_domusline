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
  void send_test_command(int opcode, int argument, int repetitions);

  void setup() override;
  void loop() override;
  void update() override;
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 private:
  enum class Cc2500Strobe : uint8_t {
    SRES = 0x30,
    SCAL = 0x33,
    SRX = 0x34,
    STX = 0x35,
    SIDLE = 0x36,
    SPWD = 0x39,
    SFRX = 0x3A,
    SFTX = 0x3B,
  };

  enum class Cc2500StatusRegister : uint8_t {
    RXBYTES = 0x3B,
  };

  enum class Cc2500Register : uint8_t {
    IOCFG2 = 0x00,
    IOCFG0 = 0x02,
    FIFOTHR = 0x03,
    SYNC1 = 0x04,
    SYNC0 = 0x05,
    PKTLEN = 0x06,
    PKTCTRL1 = 0x07,
    PKTCTRL0 = 0x08,
    ADDR = 0x09,
    CHANNR = 0x0A,
    FSCTRL1 = 0x0B,
    FSCTRL0 = 0x0C,
    FREQ2 = 0x0D,
    FREQ1 = 0x0E,
    FREQ0 = 0x0F,
    MDMCFG4 = 0x10,
    MDMCFG3 = 0x11,
    MDMCFG2 = 0x12,
    MDMCFG1 = 0x13,
    MDMCFG0 = 0x14,
    DEVIATN = 0x15,
    MCSM2 = 0x16,
    MCSM1 = 0x17,
    MCSM0 = 0x18,
    FOCCFG = 0x19,
    BSCFG = 0x1A,
    AGCCTRL2 = 0x1B,
    AGCCTRL1 = 0x1C,
    AGCCTRL0 = 0x1D,
    WOREVT1 = 0x1E,
    WOREVT0 = 0x1F,
    WORCTRL = 0x20,
    FREND1 = 0x21,
    FREND0 = 0x22,
    FSCAL3 = 0x23,
    FSCAL2 = 0x24,
    FSCAL1 = 0x25,
    FSCAL0 = 0x26,
    RCCTRL1 = 0x27,
    RCCTRL0 = 0x28,
    FSTEST = 0x29,
    TEST2 = 0x2C,
    TEST1 = 0x2D,
    TEST0 = 0x2E,
    PATABLE = 0x3E,
    TX_FIFO = 0x3F,
  };

  // These are Domus Line payload opcodes, not CC2500 command strobes.
  enum class DomusLineCommand : uint8_t {
    LIGHT_ON = 0x01,
    LIGHT_OFF = 0x02,
    BRIGHTNESS_UP = 0x11,
    BRIGHTNESS_DOWN = 0x21,
    SET_COLOR_TEMPERATURE = 0x50,
  };

  void get_current_values(light::LightState *state, uint8_t &current_temperature, uint8_t &current_brightness);
  void transfer_cc2500_bytes(const uint8_t *data, uint8_t length);
  void send_cc2500_strobe(Cc2500Strobe strobe);
  void write_cc2500_register(Cc2500Register reg, uint8_t value);
  uint8_t read_cc2500_register(Cc2500Register reg);
  uint8_t read_cc2500_status_register(Cc2500StatusRegister reg);
  void read_cc2500_rx_fifo(uint8_t *data, uint8_t length);
  void write_cc2500_burst(Cc2500Register address, const uint8_t *data, uint8_t length);
  void poll_cc2500_rx_fifo();
  void process_received_packet(const uint8_t *packet, uint8_t length, bool crc_valid);
  void synchronize_remote_command(DomusLineCommand command, uint8_t argument);
  void flush_cc2500_rx_fifo();
  void initialize_cc2500();
  void send_domusline_command(DomusLineCommand command, uint8_t argument);
  void send_domusline_command_raw(uint8_t opcode, uint8_t argument);
  void send_domusline_command_repeatedly(DomusLineCommand command, uint8_t argument, uint8_t repeat_count,
                                         uint16_t repeat_delay_ms);
  void send_domusline_command_raw_repeatedly(uint8_t opcode, uint8_t argument, uint8_t repeat_count,
                                             uint16_t repeat_delay_ms);
  void turn_light_on(uint8_t current_temperature);
  void turn_light_off();
  void increase_light_brightness();
  void decrease_light_brightness();
  void set_light_color_temperature(uint8_t current_temperature);
  void enter_cc2500_receive_mode();
  uint8_t calculate_domusline_xor_checksum(const uint8_t *data, std::size_t length);

  uint8_t CS = 5;
  uint8_t readyPin = 19;
  uint8_t actualBrightness = 10;
  uint8_t setBrightness = 10;
  uint8_t lastTemperature = 0;
  bool lightIsOn = false;
  bool writeCallbackIsRunning = false;
  uint32_t last_receive_poll_ms = 0;
  uint8_t pending_rx_packet_length = 0;
  light::LightState *light_state_{nullptr};
};

}
}