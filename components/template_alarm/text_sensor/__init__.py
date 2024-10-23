import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt, web_server,text_sensor
from esphome.helpers import sanitize, snake_case
from esphome.components.text_sensor import TextSensorPublishAction
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_FILTERS,
    CONF_ICON,
    CONF_ID,
    CONF_ON_VALUE,
    CONF_ON_RAW_VALUE,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
    CONF_WEB_SERVER_ID,
    CONF_NAME,
    CONF_STATE,
    CONF_LAMBDA,
    CONF_STATE,
    CONF_FROM,
    CONF_TO,
)


from esphome.core import CORE, coroutine_with_priority
from .. import template_alarm_ns

CONF_TYPE_ID = "id_code"
ALARM_PTR="alarm_panel::alarmPanelPtr"

TemplateTextSensor = template_alarm_ns.class_(
    "TemplateTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.Optional(CONF_TYPE_ID, default=""): cv.string_strict,  
            cv.GenerateID(): cv.declare_id(TemplateTextSensor),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def setup_text_sensor_core_(var, config):
    await setup_entity(var, config)
    
    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, var, config)


async def register_text_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text_sensor(var))
    await setup_text_sensor_core_(var, config)


async def new_text_sensor(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_text_sensor(var, config)
    return var

async def setup_entity(var, config):
    """Set up generic properties of an Entity"""
    cg.add(var.set_name(config[CONF_NAME]))
    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(config[CONF_TYPE_ID]))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(config[CONF_ID].id))
    else:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_NAME]))))

async def to_code(config):
    #cg.add_define("USE_TEMPLATE_ALARM_SENSORS")
    var = await new_text_sensor(config)
    await cg.register_component(var, config)

        
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_template(template_))


@automation.register_action(
    "text_sensor.template_alarm.publish",
    TextSensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
        }
    ),
)
async def text_sensor_template_alarm_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, cg.std_string)
    cg.add(var.set_state(template_))
    return var
