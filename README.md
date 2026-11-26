# ESPHome Domus Line light

ESPHome external light component for controlling Domus Line tunable-white lights through a CC2500 radio module. This packages the former custom component as a normal ESPHome light platform.

## Installation

Add the external component to your ESPHome device configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/weaseel/esphome_domusline
      ref: main
    components: [domusline]

spi:
  # Example pins for a classic ESP32 DevKit; choose pins supported by your board.
  clk_pin: 18
  mosi_pin: 23
  miso_pin: 19

light:
  - platform: domusline
    name: "Domus Line"
    cs_pin: 5
    ready_pin: 19
    # Optional: connect CC2500 GDO0 to ESP32 GPIO4 for interrupt-driven RX
    gdo0_pin: 4
```

The component defaults to `cs_pin: 5` and `ready_pin: 19`, matching the original implementation. `ready_pin` must be connected to the same signal as the SPI bus `miso_pin`; it is used as the CC2500 ready signal while sending packets. `gdo0_pin` is optional: when configured, the CC2500 signals a CRC-valid received packet on GDO0 and the ESP32 handles it by interrupt; without it, the component polls the receive FIFO every 5 ms.

### Wiring

Use the SPI pins configured in the ESPHome `spi:` block; their numbers depend on the selected board. For a common ESP32 DevKit, wire:

| CC2500 pin | ESP32 DevKit pin | Purpose |
| --- | --- | --- |
| VCC | 3V3 | 3.3 V supply; do not connect to 5 V. |
| GND | GND | Common ground. |
| SCLK | GPIO18 | SPI clock. |
| SI / MOSI | GPIO23 | SPI controller-to-radio data. |
| SO / MISO | GPIO19 | SPI radio-to-controller data and the configured ready signal (`ready_pin`). |
| CSn | GPIO5 | Chip select (`cs_pin`). |
| GDO0 | GPIO4 | Optional receive interrupt (`gdo0_pin`). Leave unconnected when using polling. |

If your board uses different SPI pins, configure `clk_pin`, `mosi_pin`, and `miso_pin` for those pins, and set `ready_pin` to the same GPIO as `miso_pin`. Configure `cs_pin` and `gdo0_pin` to match your wiring. ESP32-C6 requires the ESP-IDF framework; this component uses ESPHome's SPI and GPIO interfaces and works with ESP-IDF. The ESP32 and CC2500 must share ground and use 3.3 V logic.

Use an ESP32 board and ensure the SPI and GPIO pin numbers match your wiring. ESP32-C6 requires the ESP-IDF framework. The light supports on/off, brightness, and tunable white from 175 to 333 mireds.

### ESP32-C6-DevKitC-1 example

See [example-esp32-c6.yaml](./example-esp32-c6.yaml) for a complete ESP-IDF configuration. Its sample pin assignments are SCLK GPIO6, MOSI GPIO7, MISO/ready GPIO3, CSn GPIO0, and optional GDO0 GPIO2. The MISO and `ready_pin` entries intentionally use the same GPIO because the CC2500 uses its MISO line as the ready signal. Check your board revision and existing peripherals before using these pins.

## CC2500 data format

The component uses the CC2500 SPI interface to load each Domus Line command into the transmit FIFO, then issues the `STX` command strobe. The leading `0x7F` below is the CC2500 SPI burst-write header for its TX FIFO; it is not part of the FIFO contents or the Domus Line command:

```text
SPI write:  7F | 0E 68 05 7A EF OP ARG 00 00 00 00 00 00 XOR 16 FF
             ^    `-----------------------------------------------'
             |                  CC2500 TX FIFO data
             +-- TX FIFO burst-write header
```

The FIFO bytes are laid out as follows:

| FIFO offset | Value | Meaning |
| --- | --- | --- |
| 0 | `0x0E` | CC2500 variable packet length: 14 bytes follow in the RF payload, through the `0x16` trailer. |
| 1 | `0x68` | Fixed protocol framing byte. |
| 2-4 | `0x05 0x7A 0xEF` | Fixed Domus Line command prefix. |
| 5 | `OP` | Domus Line operation code. |
| 6 | `ARG` | Operation argument; color-temperature commands use the current temperature value. |
| 7-12 | six `0x00` bytes | Reserved/padding bytes included in the checksum. |
| 13 | `XOR` | XOR of FIFO payload offsets 2-12, i.e. the 11-byte Domus Line command payload. |
| 14 | `0x16` | Fixed protocol trailer; included in the 14 bytes counted by the length field. |
| 15 | `0xFF` | Additional byte written to the FIFO by the original implementation, beyond the length-delimited RF payload. |

The operation values and their apparent meanings, based on how this component calls them, are:

| `OP` | Apparent operation |
| --- | --- |
| `0x01` | Turn light on |
| `0x02` | Turn light off |
| `0x11` | Increase brightness |
| `0x21` | Decrease brightness |
| `0x50` | Set color temperature |

These operation codes and the XOR checksum belong to the Domus Line protocol, not the CC2500. Their meanings are inferred from the existing call sites; the CC2500 itself only handles the SPI strobes, FIFO, and radio transmission. Commands are repeated by the component to match the original behavior.

### Testing candidate opcodes from Home Assistant

The example exposes an `api.actions` action named `test_domusline_command`. After flashing the example, call `esphome.domus_line_example_test_domusline_command` from Home Assistant's Developer Tools or an automation with integer fields `opcode`, `argument`, and `repetitions`. The component sends the candidate using the normal Domus Line frame and checksum, then returns to receive mode. Values are limited to 0-255, and repetitions to 1-6.

For example, to test opcode `0x50` with argument `0x80` once:

```yaml
action: esphome.domus_line_example_test_domusline_command
data:
  opcode: 80
  argument: 128
  repetitions: 1
```

To look for an absolute-brightness command, first use the remote to set several known levels and compare the received opcode/argument logs. If the remote only emits `0x11`/`0x21` with no level argument, those are relative step commands. Then test candidate opcodes and arguments at low repetition counts, changing one value at a time and recording the resulting lamp level. A command that consistently produces the requested level from different starting levels is evidence of an absolute set command. Test only while you can observe the lamp; unknown opcodes may have undocumented effects.

## Receiving remote commands

After initialization and after each transmitted command, the CC2500 returns to receive mode. The component polls the RX FIFO every 5 ms, waits for the variable-length packet and the two appended CC2500 status bytes, then checks the radio CRC, Domus Line prefix/trailer, and XOR checksum. Valid frames are logged at INFO level, for example:

```text
[I][domusline]: Received Domus Line RF command: brightness up (opcode 0x11, argument 0x00)
```

Unknown opcodes with otherwise valid framing are logged as `unknown`. Invalid radio CRCs and malformed Domus Line checksums are reported as warnings. Valid remote on/off and color-temperature commands are also mirrored into the ESPHome light state without retransmitting them. Relative brightness-up/down commands are logged but do not change the Home Assistant brightness value. Recognition assumes the remote uses the same RF profile and Domus Line frame format described above. If an ESPHome light transition is in progress, an external state update may be rejected to avoid interrupting that transition.

## Local development

For local testing, place this repository next to the ESPHome YAML file and point `external_components` at its `components` directory:

```yaml
external_components:
  - source:
      type: local
      path: ./esphome_domusline/components
    components: [domusline]
```

Then use the same `light:` configuration shown above.
