import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt,text_sensor
from esphome.helpers import sanitize, snake_case
from esphome.components.text_sensor import TextSensorPublishAction
from esphome.const import (
    CONF_ID,
    CONF_STATE,
    CONF_LAMBDA,
    CONF_STATE,
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
            cv.GenerateID(): cv.declare_id(AlarmTextSensor),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_PARTITION,default=0): cv.int_,
            cv.GenerateID(CONF_ALARM_ID): cv.use_id(AlarmComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)

async def setup_entity_alarm(var, config):
    """Set up custom properties for an alarm sensor"""
    paren = await cg.get_variable(config[CONF_ALARM_ID])
    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_TYPE_ID]))))
        # cg.add(paren.createZoneFromId(var.get_object_id().c_str(),config[CONF_PARTITION]))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_ID].id))))
        # cg.add(paren.createZoneFromId(var.get_object_id().c_str(),config[CONF_PARTITION]))

        
async def to_code(config):
    cg.add_define("TEMPLATE_ALARM")
    var = await text_sensor.new_text_sensor(config)
    await setup_entity_alarm(var,config)

    await cg.register_component(var, config)

        
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_template(template_))


@automation.register_action(
    "text_sensor.template.publish",
    TextSensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
        }
    ),
)
async def text_sensor_alarm_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, cg.std_string)
    cg.add(var.set_state(template_))
    return var
