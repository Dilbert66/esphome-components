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
)
import os
import pathlib
import logging
from esphome.helpers import copy_file_if_changed
from esphome.core import CORE, coroutine_with_priority
from esphome.components import (
    template,text
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json","mg_lib"]

CONF_API_HOST="telegram_host"
CONF_CHAT_ID="chat_id"
CONF_BOT_ID="bot_id"
CONF_BOT_NAME="bot_name"
CONF_ENABLEBOT="bot_enable"
CONF_ENABLESEND="send_enable"
CONF_ALLOWED_IDS="allowed_chat_ids"
CONF_CMD="command"
CONF_TEXT="text"
CONF_CALLBACK="callback"
CONF_KEYBOARD="keyboard"
CONF_INLINE_KEYBOARD="inline_keyboard"
CONF_MARKUP="reply_markup"
CONF_TO="to"
CONF_MESSAGE="message"
CONF_PARSE_MODE="parse_mode"
CONF_DISABLE_NOTIFICATION="disable_notification"
CONF_DISABLE_WEB_PREVIEW="disable_web_page_preview"
CONF_RESIZE_KEYBOARD="resize_keyboard"
CONF_ONE_TIME_KEYBOARD="one_time_keyboard"
CONF_CALLBACK_ID="callback_query_id"
CONF_SHOW_ALERT="show_alert"
CONF_MESSAGE_ID="message_id"
CONF_TITLE="title"
CONF_SELECTIVE="selective"
CONF_FORCE="force"
CONF_URL="url"
CONF_CACHE_TIME="cache_time"
CONF_SKIP_FIRST="skip_first"
CONF_STACK_SIZE="stack_size"
KEY_ESP32 = "esp32"
KEY_SDKCONFIG_OPTIONS = "sdkconfig_options"
SDK_STACK_SIZE="CONFIG_ESP_MAIN_TASK_STACK_SIZE"


web_notify_ns = cg.esphome_ns.namespace("web_notify")
WebNotify = web_notify_ns.class_("WebNotify", cg.Component, cg.Controller)
TelegramPublishAction = web_notify_ns.class_("TelegramPublishAction", automation.Action)
TelegramEditMessageAction = web_notify_ns.class_("TelegramEditMessageAction", automation.Action)
TelegramAnswerCallBackAction = web_notify_ns.class_("TelegramAnswerCallBackAction", automation.Action)
TelegramEditReplyMarkupAction = web_notify_ns.class_("TelegramEditReplyMarkupAction", automation.Action)
TelegramDeleteMessageAction = web_notify_ns.class_("TelegramDeleteMessageAction", automation.Action)

TelegramMessageTrigger = web_notify_ns.class_(
    "TelegramMessageTrigger", automation.Trigger.template(cg.std_string)
)

RemoteData= web_notify_ns.struct(f"RemoteData")
SendData= web_notify_ns.struct(f"SendData")

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebNotify),
            #cv.Optional(CONF_API_HOST, default="api.telegram.org"): cv.string_strict ,
            cv.Required(CONF_CHAT_ID): cv.templatable(cv.string_strict),
            cv.Required(CONF_BOT_ID): cv.templatable(cv.string_strict),
            cv.Optional(CONF_ENABLEBOT,default=False): cv.boolean,
            cv.Optional(CONF_ENABLESEND,default=True): cv.boolean,
            cv.Optional(CONF_ALLOWED_IDS):cv.ensure_list(cv.string_strict),
            cv.Optional(CONF_BOT_NAME,default=""):cv.string_strict,
            cv.Optional(CONF_SKIP_FIRST,default=False):cv.boolean,
            cv.Optional(CONF_STACK_SIZE):cv.int_,
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TelegramMessageTrigger),
                    cv.Optional(CONF_CMD): cv.string_strict,
                    cv.Optional(CONF_TEXT): cv.string_strict,
                    cv.Optional(CONF_CALLBACK): cv.string_strict,

                },
                cv.has_at_least_one_key(CONF_CMD, CONF_TEXT, CONF_CALLBACK),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
)


TELEGRAM_PUBLISH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WebNotify),
        cv.Optional(CONF_TO): cv.templatable(cv.string),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
        cv.Optional(CONF_MARKUP): cv.templatable(cv.string),
        cv.Optional(CONF_KEYBOARD): cv.templatable(cv.string),
        cv.Optional(CONF_INLINE_KEYBOARD): cv.templatable(cv.string),
        cv.Optional(CONF_PARSE_MODE): cv.templatable(cv.string),
        cv.Optional(CONF_TITLE): cv.templatable(cv.string),
        cv.Optional(CONF_DISABLE_NOTIFICATION): cv.templatable(cv.boolean),
        cv.Optional(CONF_DISABLE_WEB_PREVIEW): cv.templatable(cv.boolean),
        cv.Optional(CONF_RESIZE_KEYBOARD): cv.templatable(cv.boolean),
        cv.Optional(CONF_ONE_TIME_KEYBOARD): cv.templatable(cv.boolean),
        cv.Optional(CONF_FORCE): cv.templatable(cv.boolean),

    }
)

@automation.register_action(
    "telegram.publish", TelegramPublishAction, TELEGRAM_PUBLISH_ACTION_SCHEMA
)
async def telegram_publish_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    #args_ = args + [(cg.JsonObject, "root")]
    if CONF_TO in config:
        template_ = await cg.templatable(config[CONF_TO], args, cg.std_string)
        cg.add(var.set_to(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    if CONF_KEYBOARD in config:
        template_ = await cg.templatable(config[CONF_KEYBOARD], args, cg.std_string)
        cg.add(var.set_keyboard(template_))
    if CONF_INLINE_KEYBOARD in config:
        template_ = await cg.templatable(config[CONF_INLINE_KEYBOARD], args, cg.std_string)
        cg.add(var.set_inline_keyboard(template_))
    if CONF_MARKUP in config:
        template_ = await cg.templatable(config[CONF_MARKUP], args, cg.std_string)
        cg.add(var.set_reply_markup(template_))
    if CONF_PARSE_MODE in config:
        template_ = await cg.templatable(config[CONF_PARSE_MODE], args, cg.std_string)
        cg.add(var.set_parse_mode(template_))
    if CONF_TITLE in config:
        template_ = await cg.templatable(config[CONF_TITLE], args, cg.std_string)
        cg.add(var.set_title(template_))
    if CONF_DISABLE_NOTIFICATION in config:
        template_ = await cg.templatable(config[CONF_DISABLE_NOTIFICATION], args, cg.bool_)
        cg.add(var.set_disable_notification(template_))
    if CONF_DISABLE_WEB_PREVIEW in config:
        template_ = await cg.templatable(config[CONF_DISABLE_WEB_PREVIEW], args, cg.bool_)
        cg.add(var.set_disable_web_page_preview(template_))
    if CONF_RESIZE_KEYBOARD in config:
        template_ = await cg.templatable(config[CONF_RESIZE_KEYBOARD], args, cg.bool_)
        cg.add(var.set_resize_keyboard(template_))
    if CONF_ONE_TIME_KEYBOARD in config:
        template_ = await cg.templatable(config[CONF_ONE_TIME_KEYBOARD], args, cg.bool_)
        cg.add(var.set_one_time_keyboard(template_))
    if CONF_FORCE in config:
        template_ = await cg.templatable(config[CONF_FORCE], args, cg.bool_)
        cg.add(var.set_force(template_))
    return var


TELEGRAM_ANSWER_CALLBACK_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WebNotify),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
        cv.Required(CONF_CALLBACK_ID): cv.templatable(cv.string),
        cv.Optional(CONF_SHOW_ALERT,default=False): cv.templatable(cv.boolean),
        cv.Optional(CONF_URL,default=""):cv.templatable(cv.string),
        cv.Optional(CONF_CACHE_TIME,default=0):cv.templatable(cv.int_)
    }
)

@automation.register_action(
    "telegram.answer_callback", TelegramAnswerCallBackAction, TELEGRAM_ANSWER_CALLBACK_ACTION_SCHEMA
)
async def telegram_answer_callback_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    #args_ = args + [(cg.JsonObject, "root")]
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    template_ = await cg.templatable(config[CONF_CALLBACK_ID], args, cg.std_string)
    cg.add(var.set_callback_id(template_))
    if CONF_SHOW_ALERT in config:
        template_ = await cg.templatable(config[CONF_SHOW_ALERT], args, cg.bool_)
        cg.add(var.set_show_alert(template_))
    if CONF_URL in config:
        template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
        cg.add(var.set_url(template_))
    if CONF_CACHE_TIME in config:
        template_ = await cg.templatable(config[CONF_CACHE_TIME], args, cg.int32)
        cg.add(var.set_cache_time(template_))

    return var


TELEGRAM_DELETE_MESSAGE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WebNotify),
        cv.Required(CONF_CHAT_ID): cv.templatable(cv.string),
        cv.Required(CONF_MESSAGE_ID): cv.templatable(cv.string),
    }
)

@automation.register_action(
    "telegram.delete_message", TelegramDeleteMessageAction, TELEGRAM_DELETE_MESSAGE_ACTION_SCHEMA
)
async def telegram_delete_message_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_CHAT_ID], args, cg.std_string)
    cg.add(var.set_chat_id(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE_ID], args, cg.std_string)
    cg.add(var.set_message_id(template_))
    return var


TELEGRAM_EDIT_REPLY_MARKUP_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WebNotify),
        cv.Required(CONF_CHAT_ID): cv.templatable(cv.string),
        cv.Required(CONF_MESSAGE_ID): cv.templatable(cv.string),
        cv.Optional(CONF_INLINE_KEYBOARD): cv.templatable(cv.string),
        cv.Optional(CONF_DISABLE_WEB_PREVIEW): cv.templatable(cv.boolean),

    }
)

@automation.register_action(
    "telegram.edit_reply_markup", TelegramEditReplyMarkupAction, TELEGRAM_EDIT_REPLY_MARKUP_ACTION_SCHEMA
)
async def telegram_edit_reply_markup_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    #args_ = args + [(cg.JsonObject, "root")]
    template_ = await cg.templatable(config[CONF_CHAT_ID], args, cg.std_string)
    cg.add(var.set_chat_id(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE_ID], args, cg.std_string)
    cg.add(var.set_message_id(template_))
    if CONF_INLINE_KEYBOARD in config:
        template_ = await cg.templatable(config[CONF_INLINE_KEYBOARD], args, cg.std_string)
        cg.add(var.set_inline_keyboard(template_))
    if CONF_DISABLE_WEB_PREVIEW in config:
        template_ = await cg.templatable(config[CONF_DISABLE_WEB_PREVIEW], args, cg.bool_)
        cg.add(var.set_disable_web_page_preview(template_))
    return var


TELEGRAM_EDIT_MESSAGE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(WebNotify),
        cv.Required(CONF_CHAT_ID): cv.templatable(cv.string),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
        cv.Optional(CONF_INLINE_KEYBOARD): cv.templatable(cv.string),
        cv.Optional(CONF_PARSE_MODE): cv.templatable(cv.string),
        cv.Optional(CONF_TITLE): cv.templatable(cv.string),
        cv.Required(CONF_MESSAGE_ID): cv.templatable(cv.string),
        cv.Optional(CONF_DISABLE_WEB_PREVIEW): cv.templatable(cv.boolean),

    }
)

@automation.register_action(
    "telegram.edit_message", TelegramEditMessageAction, TELEGRAM_EDIT_MESSAGE_ACTION_SCHEMA
)
async def telegram_edit_message_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    #args_ = args + [(cg.JsonObject, "root")]
    template_ = await cg.templatable(config[CONF_CHAT_ID], args, cg.std_string)
    cg.add(var.set_chat_id(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE_ID], args, cg.std_string)
    cg.add(var.set_message_id(template_))
    if CONF_PARSE_MODE in config:
        template_ = await cg.templatable(config[CONF_PARSE_MODE], args, cg.std_string)
        cg.add(var.set_parse_mode(template_))
    if CONF_INLINE_KEYBOARD in config:
        template_ = await cg.templatable(config[CONF_INLINE_KEYBOARD], args, cg.std_string)
        cg.add(var.set_inline_keyboard(template_))
    if CONF_TITLE in config:
        template_ = await cg.templatable(config[CONF_TITLE], args, cg.std_string)
        cg.add(var.set_title(template_))
    if CONF_DISABLE_WEB_PREVIEW in config:
        template_ = await cg.templatable(config[CONF_DISABLE_WEB_PREVIEW], args, cg.bool_)
        cg.add(var.set_disable_web_page_preview(template_))
    return var


@coroutine_with_priority(40.0)
async def to_code(config):
    if CORE.using_arduino:
        stack =f"SET_LOOP_TASK_STACK_SIZE(16 * 1024);"
        if CONF_STACK_SIZE in config and config[CONF_STACK_SIZE]:
            stack =f"SET_LOOP_TASK_STACK_SIZE({config[CONF_STACK_SIZE]} * 1024);"
        cg.add_global(cg.RawStatement("#if not defined(USE_STACK_SIZE)"))
        cg.add_global(cg.RawStatement(stack))
        cg.add_global(cg.RawStatement("#define USE_STACK_SIZE"))
        cg.add_global(cg.RawStatement("#endif"))
    if CORE.using_esp_idf: 
        stack=6
        if CONF_STACK_SIZE in config and config[CONF_STACK_SIZE]:
            stack=config[CONF_STACK_SIZE]
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][SDK_STACK_SIZE] = stack * 1024
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_ALLOWED_IDS in config:
        for anid in config[CONF_ALLOWED_IDS]:
            cg.add(var.add_chatid(anid))
    if CONF_API_HOST in config and config[CONF_API_HOST]:
        cg.add(var.set_api_host(config[CONF_API_HOST]))
    if CONF_ENABLEBOT in config and config[CONF_ENABLEBOT]:
        cg.add(var.set_bot_enable(config[CONF_ENABLEBOT]))
    if CONF_ENABLESEND in config and config[CONF_ENABLESEND]:
        cg.add(var.set_send_enable(config[CONF_ENABLESEND]))
    cg.add(var.set_bot_name(config[CONF_BOT_NAME]))
    cg.add(var.set_skip_first(config[CONF_SKIP_FIRST]))
    
    if CONF_CHAT_ID in config and config[CONF_CHAT_ID]:
        if (cg.is_template(config[CONF_CHAT_ID])):
            template_ = await cg.process_lambda(config[CONF_CHAT_ID], "", return_type=cg.std_string)
            cg.add(var.set_chat_id_f(template_))
        else:
            cg.add(var.set_chat_id(config[CONF_CHAT_ID]))
    if CONF_BOT_ID in config and config[CONF_BOT_ID]:
        if (cg.is_template(config[CONF_BOT_ID])):
            template_ = await cg.process_lambda(config[CONF_BOT_ID], "", return_type=cg.std_string)
            cg.add(var.set_bot_id_f(template_))
        else:
            cg.add(var.set_bot_id(config[CONF_BOT_ID]))

    for conf in config.get(CONF_ON_MESSAGE, []):
        if CONF_CMD in conf:
            trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID],","+conf[CONF_CMD]+",","cmd")
            await automation.build_automation(trig, [(RemoteData,'x')], conf)
        if CONF_TEXT in conf:
            trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID],","+conf[CONF_TEXT]+",","text")
            await automation.build_automation(trig, [(RemoteData,'x')], conf)
        if CONF_CALLBACK in conf:
            trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID],","+conf[CONF_CALLBACK]+",","callback")
            await automation.build_automation(trig, [(RemoteData,'x')], conf)

    # src=os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose/mongoose.h")
    # dst=CORE.relative_build_path("src/esphome/components/mg_lib/mongoose.h")
    # if os.path.isfile(src) and not os.path.isfile(dst):
    #     copy_file_if_changed(src,dst)
    # src=os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose/mongoose.c")
    # dst=CORE.relative_build_path("src/esphome/components/mg_lib/mongoose.c")
    # if os.path.isfile(src) and not os.path.isfile(dst):
    #      copy_file_if_changed(src,dst)
    
    #remove old version file
    dst=CORE.relative_build_path("src/mongoose.c")
    if os.path.isfile(dst):
          os.remove(dst)

