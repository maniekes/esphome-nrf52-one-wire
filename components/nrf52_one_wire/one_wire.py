from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.one_wire import OneWireBus
from esphome.const import CONF_ID, CONF_PIN

DEPENDENCIES = ["nrf52"]
CONF_INTERNAL_PULLUP = "internal_pullup"
nrf52_one_wire_ns = cg.esphome_ns.namespace("nrf52_one_wire")
NRF52OneWireBus = nrf52_one_wire_ns.class_("NRF52OneWireBus", OneWireBus, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NRF52OneWireBus),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_INTERNAL_PULLUP, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_internal_pullup(config[CONF_INTERNAL_PULLUP]))
