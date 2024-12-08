import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE,CONF_PUBLISH_INITIAL_STATE

from esphome.core import CORE, coroutine_with_priority
from esphome.helpers import sanitize, snake_case
from esphome.components import mqtt, web_server, binary_sensor
from esphome.const import (
    CONF_DELAY,
    CONF_NAME,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_FILTERS,
    CONF_ICON,
    CONF_ID,
    CONF_INVALID_COOLDOWN,
    CONF_INVERTED,
    CONF_MAX_LENGTH,
    CONF_MIN_LENGTH,
    CONF_ON_CLICK,
    CONF_ON_DOUBLE_CLICK,
    CONF_ON_MULTI_CLICK,
    CONF_ON_PRESS,
    CONF_ON_RELEASE,
    CONF_ON_STATE,
    CONF_PUBLISH_INITIAL_STATE,
    CONF_STATE,
    CONF_TIMING,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
    CONF_WEB_SERVER_ID,
    CONF_INTERNAL,
    CONF_DISABLED_BY_DEFAULT,
    CONF_WEB_SERVER,

)
from .. import component_ns

CONF_TYPE_ID = "id_code"

AlarmBinarySensor = component_ns.class_(
    "AlarmBinarySensor", binary_sensor.BinarySensor, cg.Component
)


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(AlarmBinarySensor)
    .extend(
        {
            cv.Optional(CONF_TYPE_ID, default=""): cv.string_strict,  
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def setup_entity_alarm(var, config):
    """Set up custom properties for an alarm sensor"""
    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_TYPE_ID]))))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_ID].id))))

    cg.add(var.set_publish_initial_state(True))



async def to_code(config):
    cg.add_define("TEMPLATE_ALARM")
    var = await binary_sensor.new_binary_sensor(config)
    await setup_entity_alarm(var,config)

    await cg.register_component(var, config)
       
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(bool)
        )
        cg.add(var.set_template(template_))


@automation.register_action(
    "binary_sensor.alarm.publish",
    binary_sensor.BinarySensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
)
async def binary_sensor_alarm_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var
