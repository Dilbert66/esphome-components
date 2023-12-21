import gzip
import json
import yaml
import requests
from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_USERNAME,
    CONF_PASSWORD,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
    PLATFORM_RTL87XX,
    CONF_ON_MESSAGE, 
    CONF_TRIGGER_ID,
    CONF_PAYLOAD
)
from esphome.core import CORE, coroutine_with_priority

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json"]
CONF_API_HOST="telegram_host"
CONF_USER_CHAT_ID="chat_id"
CONF_BOT_ID="bot_id"
CONF_ENABLEBOT="bot_enable"
CONF_ENABLESEND="send_enable"
CONF_ALLOWED_IDS="allowed_chat_ids"
CONF_CMD="command"

web_notify_ns = cg.esphome_ns.namespace("web_notify")

WebNotify = web_notify_ns.class_("WebNotify", cg.Component, cg.Controller)
TelegramPublishAction = web_notify_ns.class_("TelegramPublishAction", automation.Action)

TelegramMessageTrigger = web_notify_ns.class_(
    "TelegramMessageTrigger", automation.Trigger.template(cg.std_string)
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebNotify),
            #cv.Optional(CONF_API_HOST, default="api.telegram.org"): cv.string_strict ,
            cv.Required(CONF_USER_CHAT_ID): cv.string_strict, 
            cv.Required(CONF_BOT_ID): cv.string_strict, 
            cv.Optional(CONF_ENABLEBOT,default=False): cv.boolean,
            cv.Optional(CONF_ENABLESEND,default=True): cv.boolean,
            cv.Optional(CONF_ALLOWED_IDS):cv.ensure_list(cv.string_strict),
            
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TelegramMessageTrigger),
                    cv.Required(CONF_CMD): cv.string_strict,                    
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
)
CONF_TO="to"
CONF_MESSAGE="message"

TELEGRAM_PUBLISH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WebNotify),
        cv.Required(CONF_TO): cv.templatable(cv.string),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)

@automation.register_action(
    "telegram.publish", TelegramPublishAction, TELEGRAM_PUBLISH_ACTION_SCHEMA
)
async def telegram_publish_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_TO], args, cg.std_string)
    cg.add(var.set_to(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    return var



def add_resource_as_progmem(
    resource_name: str, content: str, compress: bool = True
) -> None:
    """Add a resource to progmem."""
    content_encoded = content.encode("utf-8")
    if compress:
        content_encoded = gzip.compress(content_encoded)
    content_encoded_size = len(content_encoded)
    bytes_as_int = ", ".join(str(x) for x in content_encoded)
    uint8_t = f"const uint8_t ESPHOME_WEBSERVER_{resource_name}[{content_encoded_size}] PROGMEM = {{{bytes_as_int}}}"
    size_t = (
        f"const size_t ESPHOME_WEBSERVER_{resource_name}_SIZE = {content_encoded_size}"
    )
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(size_t))


@coroutine_with_priority(40.0)
async def to_code(config):
    cg.add_build_flag("-DMG_TLS=MG_TLS_MBED")
    cg.add_library("https://github.com/Dilbert66/esphome-mongoose.git", ">=1.0.0")    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_ALLOWED_IDS in config:
        for anid in config[CONF_ALLOWED_IDS]:
            cg.add(var.add_chatid(anid))
    #if CONF_API_HOST in config and config[CONF_API_HOST]:
        #cg.add(var.set_api_host(config[CONF_API_HOST]));
    if CONF_BOT_ID in config and config[CONF_BOT_ID]:
        cg.add(var.set_bot_id(config[CONF_BOT_ID])); 
    if CONF_API_HOST in config and config[CONF_API_HOST]:
        cg.add(var.set_api_host(config[CONF_API_HOST]));        
    if CONF_USER_CHAT_ID in config and config[CONF_USER_CHAT_ID]:
        cg.add(var.set_chat_id(config[CONF_USER_CHAT_ID]));   
    if CONF_ENABLEBOT in config and config[CONF_ENABLEBOT]:
        cg.add(var.set_bot_enable(config[CONF_ENABLEBOT])); 
    if CONF_ENABLESEND in config and config[CONF_ENABLESEND]:
        cg.add(var.set_send_enable(config[CONF_ENABLESEND]));   

    for conf in config.get(CONF_ON_MESSAGE, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID],conf[CONF_CMD])
        await automation.build_automation(trig, [(cg.std_string, "cmd"),(cg.std_string,"args"),(cg.std_string,"text")], conf)


