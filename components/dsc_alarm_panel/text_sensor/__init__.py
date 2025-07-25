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
CONF_SORTING_GROUP_ID = "sorting_group_id"
CONF_SORTING_WEIGHT = "sorting_weight"
CONF_WEB_KEYPAD_ID="web_keypad_id"
CONF_WEB_KEYPAD="web_keypad"

AlarmTextSensor = component_ns.class_(
    "AlarmTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

web_keypad_ns = cg.esphome_ns.namespace("web_keypad")
WebKeypad = web_keypad_ns.class_("WebServer", cg.Component, cg.Controller)

WEBKEYPAD_SORTING_SCHEMA = cv.Schema(
    {
         cv.Optional(CONF_WEB_KEYPAD): cv.Schema(
             {
                cv.OnlyWith(CONF_WEB_KEYPAD_ID, "web_keypad"): cv.use_id(WebKeypad),
                cv.Optional(CONF_SORTING_WEIGHT): cv.All(
                    cv.requires_component("web_keypad"),
                    cv.float_,
                ),
                cv.Optional(CONF_SORTING_GROUP_ID): cv.All(
                    cv.requires_component("web_keypad"),
                    cv.use_id(cg.int_),
                ),
             }
         )
    }
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
    .extend(WEBKEYPAD_SORTING_SCHEMA)
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

