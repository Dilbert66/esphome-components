import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.helpers import sanitize, snake_case
from esphome.const import CONF_ID
from .. import component_ns, AlarmComponent, validate_id_code, generate_validate_sensor_config, CONF_TYPE_ID
from .. import CONF_TYPE_ID, CONF_ALARM_ID, CONF_WEB_KEYPAD, CONF_PARTITION, WEBKEYPAD_SORTING_SCHEMA

AlarmTextSensor = component_ns.class_(
    "AlarmTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.Optional(CONF_TYPE_ID, default=""): cv.Any(cv.string_strict, validate_id_code),
            cv.Optional(CONF_PARTITION,default=0): cv.int_,
            cv.GenerateID(CONF_ALARM_ID): cv.use_id(AlarmComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(WEBKEYPAD_SORTING_SCHEMA),
    generate_validate_sensor_config(False)  # False indicates this is not a binary sensor, but a text sensor
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
    
    if web_keypad_config := config.get(CONF_WEB_KEYPAD):
        from esphome.components import web_keypad
        await web_keypad.add_entity_config(var, web_keypad_config)
        
async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await setup_entity_alarm(var,config)

