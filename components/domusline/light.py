import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light, spi
from esphome.const import CONF_CS_PIN, CONF_OUTPUT_ID

DEPENDENCIES = ["esp32", "spi"]
AUTO_LOAD = []

CONF_READY_PIN = "ready_pin"
CONF_GDO0_PIN = "gdo0_pin"

domusline_ns = cg.esphome_ns.namespace("domusline")
DomusLine = domusline_ns.class_(
    "DomusLine", cg.PollingComponent, light.LightOutput, spi.SPIDevice
)

CONFIG_SCHEMA = (
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DomusLine),
            cv.Optional(CONF_READY_PIN, default=19): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_GDO0_PIN): pins.internal_gpio_input_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        spi.spi_device_schema(
            cs_pin_required=False,
            default_data_rate="250kHz",
            default_mode="mode0",
        )
    )
    .extend(
        {
            cv.Optional(CONF_CS_PIN, default=5): pins.internal_gpio_output_pin_schema,
        }
    )
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "Domus Line", require_mosi=True, require_miso=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)
    await spi.register_spi_device(var, config)
    await cg.add_pin(var.set_ready_pin, config[CONF_READY_PIN])
    if CONF_GDO0_PIN in config:
        await cg.add_pin(var.set_gdo0_pin, config[CONF_GDO0_PIN])
