import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import generate_validate_sensor_config, setup_alarm_sensor
from .. import WEBKEYPAD_SORTING_SCHEMA, ALARM_SENSOR_SCHEMA

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema()
    .extend(ALARM_SENSOR_SCHEMA)      
    .extend(cv.COMPONENT_SCHEMA)
    .extend(WEBKEYPAD_SORTING_SCHEMA),
    generate_validate_sensor_config(True) # True indicates this is a binary sensor
)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await setup_alarm_sensor(var,config,True)

