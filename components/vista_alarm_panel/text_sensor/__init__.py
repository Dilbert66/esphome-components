import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import generate_validate_sensor_config, setup_alarm_sensor
from .. import WEBKEYPAD_SORTING_SCHEMA, ALARM_SENSOR_SCHEMA

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema()
    .extend(ALARM_SENSOR_SCHEMA)      
    .extend(cv.COMPONENT_SCHEMA)
    .extend(WEBKEYPAD_SORTING_SCHEMA),
    generate_validate_sensor_config(False) # True indicates this is a binary sensor
)

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await setup_alarm_sensor(var,config,False)

