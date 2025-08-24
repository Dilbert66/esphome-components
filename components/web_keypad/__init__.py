import gzip
import json
import yaml
import requests
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CSS_INCLUDE,
    CONF_CSS_URL,
    CONF_ID,
    CONF_JS_INCLUDE,
    CONF_JS_URL,
    CONF_ENABLE_PRIVATE_NETWORK_ACCESS,
    CONF_PORT,
    CONF_AUTH,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_INCLUDE_INTERNAL,
    CONF_OTA,
    CONF_LOG,
    CONF_VERSION,
    CONF_LOCAL,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
    PLATFORM_RTL87XX,
    CONF_NAME,
)
import os
import pathlib
import logging
from esphome.helpers import copy_file_if_changed
from esphome.core import CORE, coroutine_with_priority

_LOGGER = logging.getLogger(__name__)
DEPENDENCIES = ["network"]
AUTO_LOAD = ["json","mg_lib"]

CONF_CONFIG ="config_local"
CONF_KEYPAD_URL="config_url"
CONF_PARTITIONS="partitions"
CONF_SERVICE_LAMBDA="service_lambda"
CONF_KEYPAD="show_keypad"
CONF_CERTIFICATE="certificate"
CONF_CERTIFICATE_KEY="certificate_key"
CONF_ENCRYPTION="encryption"
CONF_JSLOCAL="js_local"


CONF_SORTING_GROUP_ID = "sorting_group_id"
CONF_SORTING_GROUPS = "sorting_groups"
CONF_SORTING_WEIGHT = "sorting_weight"
CONF_WEB_KEYPAD_ID="web_keypad_id"
CONF_WEB_KEYPAD="web_keypad"
CONF_STACK_SIZE="stack_size"

web_keypad_ns = cg.esphome_ns.namespace("web_keypad")
WebKeypad = web_keypad_ns.class_("WebServer", cg.Component, cg.Controller)

sorting_groups = {}

def default_url(config):
    config = config.copy()
    if config[CONF_VERSION] == 2:
        if not (CONF_JSLOCAL in config):
            config[CONF_JSLOCAL]=""
        if not (CONF_CSS_URL in config):
            config[CONF_CSS_URL] = ""
        if not (CONF_JS_URL in config):
            config[CONF_JS_URL] = "https://dilbert66.github.io/js_files/www.js"
    if config[CONF_VERSION] == 3:
        if not (CONF_JSLOCAL in config):
            config[CONF_JSLOCAL]=""
        if not (CONF_CSS_URL in config):
            config[CONF_CSS_URL] = ""
        if not (CONF_JS_URL in config):
            config[CONF_JS_URL] = "https://dilbert66.github.io/js_files/www_v3.js"   
    return config


def validate_sorting_groups(config):
    if CONF_SORTING_GROUPS in config and config[CONF_VERSION] != 3:
        raise cv.Invalid(
            f"'{CONF_SORTING_GROUPS}' is only supported in 'web_server' version 3"
        )
    return config


sorting_group = {
    cv.Required(CONF_ID): cv.declare_id(cg.int_),
    cv.Required(CONF_NAME): cv.string,
    cv.Optional(CONF_SORTING_WEIGHT): cv.float_,
}

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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebKeypad),
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_VERSION, default=2): cv.one_of(2,3, int=True),
            cv.Optional(CONF_CSS_URL): cv.string,
            cv.Optional(CONF_CSS_INCLUDE): cv.file_,
            cv.Optional(CONF_JS_URL): cv.string,
            cv.Optional(CONF_JSLOCAL): cv.string,            
            cv.Optional(CONF_PARTITIONS): cv.int_,            
            cv.Optional(CONF_JS_INCLUDE): cv.file_,
            cv.Optional(CONF_CONFIG):cv.file_,
            cv.Optional(CONF_KEYPAD, default=True):cv.boolean,
            cv.Optional(CONF_KEYPAD_URL):cv.string,  
            cv.Optional(CONF_SERVICE_LAMBDA): cv.lambda_,
            cv.Optional(CONF_ENABLE_PRIVATE_NETWORK_ACCESS, default=True): cv.boolean,
            cv.Optional(CONF_CERTIFICATE): cv.All(
                cv.string
            ),
            cv.Optional(CONF_CERTIFICATE_KEY): cv.All(
                cv.string
            ),
            cv.Optional(CONF_AUTH): cv.Schema(
                {
                    cv.Required(CONF_USERNAME): cv.All(
                        cv.string_strict, cv.Length(min=1)
                    ),
                    cv.Required(CONF_PASSWORD): cv.All(
                        cv.string_strict, cv.Length(min=1)
                    ),
                    cv.Optional(CONF_ENCRYPTION, default=False):cv.boolean,                     
                    },
            ),
            cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
            cv.SplitDefault(
                CONF_OTA,
                esp8266=False,
                esp32_arduino=False,
                esp32_idf=False,
                bk72xx=False,
                rtl87xx=False,
            ): cv.boolean,
            cv.Optional(CONF_LOG, default=False): cv.boolean,
            cv.Optional(CONF_LOCAL, default=True): cv.boolean,
            cv.Optional(CONF_SORTING_GROUPS): cv.ensure_list(sorting_group),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    #cv.only_on([PLATFORM_ESP32]),
    default_url,
    validate_sorting_groups,

)

def add_sorting_groups(web_server_var, config):
    for group in config:
        sorting_groups[group[CONF_ID]] = group[CONF_NAME]
        group_sorting_weight = group.get(CONF_SORTING_WEIGHT, 50)
        cg.add(
            web_server_var.add_sorting_group(
                hash(group[CONF_ID]), group[CONF_NAME], group_sorting_weight
            )
        )


async def add_entity_config(entity, config):
    web_keypad = await cg.get_variable(config[CONF_WEB_KEYPAD_ID])
    sorting_weight = config.get(CONF_SORTING_WEIGHT, 50)
    sorting_group_hash = hash(config.get(CONF_SORTING_GROUP_ID))

    cg.add(
        web_keypad.add_entity_config(
            entity,
            sorting_weight,
            sorting_group_hash,
        )
    )


def build_index_html(config) -> str:
    html = "<!DOCTYPE html><html><head><meta charset=UTF-8><link rel=icon href=data:>"
    css_include = config.get(CONF_CSS_INCLUDE)
    js_include = config.get(CONF_JS_INCLUDE)
    js_url = config.get(CONF_JS_URL)  
    js_local=config.get(CONF_LOCAL);  
    if config[CONF_JSLOCAL]:
        js_include=config.get(CONF_JSLOCAL)
        js_url=""
        js_local=True

    if css_include:
        html += "<link rel=stylesheet href=/0.css>"
    if config[CONF_CSS_URL]:
        html += f'<link rel=stylesheet href="{config[CONF_CSS_URL]}">'
    html += "</head><body>"
    html += "<esp-app></esp-app>"  
    if js_include or (js_url and js_local):
        html += "<script src=/0.js></script>"
    if js_url and not js_local:
        html += f'<script src="{config[CONF_JS_URL]}"></script>'
    html += "</body></html>"
    return html


def add_resource_as_progmem(
    resource_name: str, content: str, compress: bool = True
) -> None:
    """Add a resource to progmem."""
    content_encoded = content.encode("utf-8")
    if compress:
        content_encoded = gzip.compress(content_encoded)
    content_encoded_size = len(content_encoded)
    bytes_as_int = ", ".join(str(x) for x in content_encoded)
    uint8_t = f"const uint8_t ESPHOME_WEBKEYPAD_{resource_name}[{content_encoded_size}] PROGMEM = {{{bytes_as_int}}}"
    size_t = (
        f"const size_t ESPHOME_WEBKEYPAD_{resource_name}_SIZE = {content_encoded_size}"
    )
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(size_t))


@coroutine_with_priority(40.0)
async def to_code(config):
    if CORE.using_arduino:
        stack =f"SET_LOOP_TASK_STACK_SIZE(16 * 1024);"
        if CONF_STACK_SIZE in config and config[CONF_STACK_SIZE]:
            stack =f"SET_LOOP_TASK_STACK_SIZE({config[CONF_STACK_SIZE]} * 1024);"
        cg.add_global(cg.RawStatement(stack))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library("intrbiz/Crypto",None)
    version = config[CONF_VERSION]

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add_define("USE_WEBKEYPAD")
    cg.add_define("USE_WEBKEYPAD_PORT", config[CONF_PORT])
    cg.add_define("USE_WEBKEYPAD_VERSION", version)
    
    if lambda_config := config.get(CONF_SERVICE_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config, [(cg.std_string, "keys"),(cg.uint8, "partition")], return_type=None
        )
        cg.add(var.set_service_lambda(lambda_))    
    if version >= 2:
        # Don't compress the index HTML as the data sizes are almost the same.
        add_resource_as_progmem("INDEX_HTML", build_index_html(config), compress=False)
    else:
        cg.add(var.set_css_url(config[CONF_CSS_URL]))
        cg.add(var.set_js_url(config[CONF_JS_URL]))
    cg.add(var.set_allow_ota(config[CONF_OTA]))
    cg.add(var.set_expose_log(config[CONF_LOG]))
    cg.add(var.set_show_keypad(config[CONF_KEYPAD]))   

    
    if CONF_PARTITIONS in config:
        cg.add(var.set_partitions(config[CONF_PARTITIONS]))   
    if config[CONF_ENABLE_PRIVATE_NETWORK_ACCESS]:
        cg.add_define("USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS")

    if CONF_CERTIFICATE in config:
        cg.add(var.set_certificate(config[CONF_CERTIFICATE]))
        cg.add(var.set_certificate_key(config[CONF_CERTIFICATE_KEY]))
        
    if CONF_AUTH in config:
        cg.add(var.set_auth(config[CONF_AUTH][CONF_USERNAME],config[CONF_AUTH][CONF_PASSWORD],config[CONF_AUTH][CONF_ENCRYPTION]));    
               
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_WEBKEYPAD_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        with open(file=path, encoding="utf-8") as css_file:
            add_resource_as_progmem("CSS_INCLUDE", css_file.read())
            
    if  config[CONF_JSLOCAL]:
        cg.add_define("USE_WEBKEYPAD_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JSLOCAL])
        with open(file=path, encoding="utf-8") as js_file:
            add_resource_as_progmem("JS_INCLUDE", js_file.read())  
            
    if (CONF_JS_URL in config and config[CONF_JS_URL] and config[CONF_LOCAL]) and not config[CONF_JSLOCAL]:
        cg.add_define("USE_WEBKEYPAD_JS_INCLUDE")
        response = requests.get(config[CONF_JS_URL])
        add_resource_as_progmem("JS_INCLUDE", response.text)   
        
    if CONF_JS_INCLUDE in config and not config[CONF_JSLOCAL]:
        cg.add_define("USE_WEBKEYPAD_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        with open(file=path, encoding="utf-8") as js_file:
            add_resource_as_progmem("JS_INCLUDE", js_file.read())
           
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
       
    if CONF_KEYPAD_URL in config and config[CONF_KEYPAD_URL]:
        response = requests.get(config[CONF_KEYPAD_URL])
        configuration = yaml.safe_load(response.text)
        output = json.dumps(configuration)
        cg.add(var.set_keypad_config(output))        
       
    if CONF_CONFIG in config and config[CONF_CONFIG]:
        with open( CORE.relative_config_path(config[CONF_CONFIG]),'r') as file:
            configuration = yaml.safe_load(file)
        output = json.dumps(configuration)
        cg.add(var.set_keypad_config(output))
    if CORE.using_arduino:
        if CORE.is_esp32:        
            cg.add_library("Update", None)     
            
    if (sorting_group_config := config.get(CONF_SORTING_GROUPS)) is not None:
        add_sorting_groups(var, sorting_group_config)

    # src=os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose/mongoose.h")
    # dst=CORE.relative_build_path("src/esphome/components/mg_lib/mongoose.h")
    # if os.path.isfile(src) and not os.path.isfile(dst):
    #     copy_file_if_changed(src,dst)
    # src=os.path.join(pathlib.Path(__file__).parent.resolve(),"mongoose/mongoose.c")
    
    #remove old version file
    dst=CORE.relative_build_path("src/mongoose.c")
    if os.path.isfile(dst):
          os.remove(dst)