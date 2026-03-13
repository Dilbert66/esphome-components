import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_COMPONENTS, CONF_ID, CONF_LAMBDA
import logging
_LOGGER = logging.getLogger(__name__)

custom_component_ns = cg.esphome_ns.namespace("custom_component")
CustomComponentConstructor = custom_component_ns.class_("CustomComponentConstructor",cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomComponentConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_COMPONENTS): cv.ensure_list(
            cv.Schema({cv.GenerateID(): cv.declare_id(cg.Component)}).extend(
                cv.COMPONENT_SCHEMA
            )
        ),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.std_vector.template(cg.ComponentPtr)
    )
    var = cg.new_Pvariable(config[CONF_ID],template_)


