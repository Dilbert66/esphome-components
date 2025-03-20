import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.helpers import sanitize, snake_case
from esphome.const import (
    CONF_ID,

)
from .. import component_ns,AlarmComponent

CONF_TYPE_ID = "id_code"
CONF_PARTITION="partition"
CONF_ALARM_ID = "alarm_id"

AlarmTextSensor = component_ns.class_(
    "AlarmTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.Optional(CONF_TYPE_ID, default=""): cv.string_strict,  
            cv.Optional(CONF_PARTITION,default=0): cv.int_,
            cv.GenerateID(CONF_ALARM_ID): cv.use_id(AlarmComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def setup_entity_alarm(var, config):
    """Set up custom properties for an alarm sensor"""
    paren = await cg.get_variable(config[CONF_ALARM_ID])
    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_TYPE_ID]))))
        # cg.add(paren.createZoneFromObj(var.get_object_id().c_str(),config[CONF_PARTITION]))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_ID].id))))
        # cg.add(paren.createZoneFromObj(var.get_object_id().c_str(),config[CONF_PARTITION]))
    cg.add(var.publish_state(" "))
        
async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await setup_entity_alarm(var,config)

