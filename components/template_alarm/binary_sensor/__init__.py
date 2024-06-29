import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE,CONF_PUBLISH_INITIAL_STATE
from .. import template_alarm_ns


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


async def to_code(config):
    cg.add_define("USE_TEMPLATE_ALARM_SENSORS")
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
   
    if not config.get(CONF_PUBLISH_INITIAL_STATE):
        cg.add(var.set_publish_initial_state(True))
    if config.get(CONF_TYPE_ID):
        cg.add(cg.RawExpression(f"{ALARM_PTR}->add_binary_sensor({var},\"{config[CONF_TYPE_ID]}\");"))

    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(cg.RawExpression(f"{ALARM_PTR}->add_binary_sensor({var},\"{config[CONF_ID].id}\");"))
        
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
