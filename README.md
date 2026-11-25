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
