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

light:
  - platform: domusline
    name: "Domus Line"
    cs_pin: 5
    ready_pin: 19
```

The component defaults to `cs_pin: 5` and `ready_pin: 19`, matching the original implementation. `ready_pin` is the CC2500 MISO/ready signal used while sending packets. Connect the CC2500 to the board's hardware SPI pins; this component uses the Arduino SPI bus and initializes it with the platform defaults.

Use an ESP32 board with the Arduino framework and ensure the pin numbers match your wiring. The light supports on/off, brightness, and tunable white from 175 to 333 mireds.

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
