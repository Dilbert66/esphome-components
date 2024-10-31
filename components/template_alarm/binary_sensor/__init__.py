import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE,CONF_PUBLISH_INITIAL_STATE
from .. import template_alarm_ns
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
)

CONF_TYPE_ID = "id_code"
ALARM_PTR="alarm_panel::alarmPanelPtr"



TemplateBinarySensor = template_alarm_ns.class_(
    "TemplateBinarySensor", binary_sensor.BinarySensor, cg.Component
)


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(TemplateBinarySensor)
    .extend(
        {
            cv.Optional(CONF_TYPE_ID, default=""): cv.string_strict,  
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def new_binary_sensor(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_binary_sensor(var, config)
    return var

async def register_binary_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_binary_sensor(var))
    await setup_binary_sensor_core_(var, config)

async def setup_binary_sensor_core_(var, config):
    await setup_entity(var, config)
    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, var, config)

async def setup_entity(var, config):
    """Set up generic properties of an Entity"""
    cg.add(var.set_name(config[CONF_NAME]))
    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(config[CONF_TYPE_ID]))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(config[CONF_ID].id))
    else:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_NAME]))))
    cg.add(var.set_publish_initial_state(True))

async def to_code(config):
    #cg.add_define("USE_TEMPLATE_ALARM_SENSORS")
    var = await new_binary_sensor(config)

    await cg.register_component(var, config)
       
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(bool)
        )
        cg.add(var.set_template(template_))


@automation.register_action(
    "binary_sensor.template_alarm.publish",
    binary_sensor.BinarySensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
)
async def binary_sensor_template_alarm_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var
