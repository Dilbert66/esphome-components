import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import text_sensor
from esphome.components.text_sensor import TextSensorPublishAction
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE
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


async def to_code(config):
    cg.add_define("USE_TEMPLATE_ALARM_SENSORS")
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    if config.get(CONF_TYPE_ID):
        cg.add(cg.RawExpression(f"{ALARM_PTR}->add_text_sensor({var},\"{config[CONF_TYPE_ID]}\");"))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(cg.RawExpression(f"{ALARM_PTR}->add_text_sensor({var},\"{config[CONF_ID].id}\");"))
        
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
