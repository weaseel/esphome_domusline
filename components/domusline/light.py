import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

DEPENDENCIES = ["esp32"]
AUTO_LOAD = []

CONF_CS_PIN = "cs_pin"
CONF_READY_PIN = "ready_pin"
CONF_GDO0_PIN = "gdo0_pin"

domusline_ns = cg.esphome_ns.namespace("domusline")
DomusLine = domusline_ns.class_("DomusLine", cg.PollingComponent, light.LightOutput)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DomusLine),
        cv.Optional(CONF_CS_PIN, default=5): cv.int_range(min=0, max=33),
        cv.Optional(CONF_READY_PIN, default=19): cv.int_range(min=0, max=39),
        cv.Optional(CONF_GDO0_PIN): cv.int_range(min=0, max=39),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)
    cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
    cg.add(var.set_ready_pin(config[CONF_READY_PIN]))
    if CONF_GDO0_PIN in config:
        cg.add(var.set_gdo0_pin(config[CONF_GDO0_PIN]))
