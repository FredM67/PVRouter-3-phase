import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_FLASH, UNIT_WATT_HOURS

from .. import CONF_TAG_NAME, CONF_MK2PVROUTER_ID, MK2PVROUTER_LISTENER_SCHEMA, mk2pvrouter_ns

Mk2PVRouterSensor = mk2pvrouter_ns.class_("Mk2PVRouterSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(
    Mk2PVRouterSensor,
    unit_of_measurement=UNIT_WATT_HOURS,
    icon=ICON_FLASH,
    accuracy_decimals=0,
).extend(MK2PVROUTER_LISTENER_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    cg.add(mk2pvrouter.register_mk2pvrouter_listener(var))
