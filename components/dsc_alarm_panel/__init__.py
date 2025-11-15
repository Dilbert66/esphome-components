import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
import re
import os
import logging
from esphome.helpers import sanitize, snake_case

    
_LOGGER = logging.getLogger(__name__)
component_ns = cg.esphome_ns.namespace('alarm_panel')
AlarmComponent = component_ns.class_('DSCkeybushome', cg.PollingComponent)

CONF_ACCESSCODE="accesscode"
CONF_MAXZONES="maxzones"
CONF_USERCODES="usercodes"
CONF_DEFAULTPARTITION="defaultpartition"
CONF_DEBUGLEVEL="dscdebuglevel"
CONF_READPIN="dscreadpin"
CONF_WRITEPIN="dscwritepin"
CONF_CLOCKPIN="dscclockpin"
CONF_INVERT_WRITE="invert_write"
CONF_EXPANDER1="expanderaddr1"
CONF_EXPANDER2="expanderaddr2"
CONF_DEBOUNCE="debounce"
CONF_CLEAN="clean_build"
CONF_AUTOPOPULATE="autopopulate"
CONF_DETAILEDPARTITIONSTATE="detailed_partition_state"
CONF_REFRESHTIME="trouble_fetch_update_time"
CONF_TROUBLEFETCH="trouble_fetch"
CONF_TROUBLEFETCHCMD="trouble_fetch_cmd"
CONF_EVENTFORMAT="event_format"
CONF_STACK_SIZE="stack_size"
CONF_USE_ESP_IDF_TIMER="use_esp_idf_timer"
CONF_TYPE_ID = "id_code"
CONF_PARTITION="partition"
CONF_ALARM_ID = "alarm_id"
CONF_SORTING_GROUP_ID = "sorting_group_id"
CONF_SORTING_WEIGHT = "sorting_weight"
CONF_WEB_KEYPAD_ID="web_keypad_id"
CONF_WEB_KEYPAD="web_keypad"

BINARY_SENSOR_TYPE_ID_REGEX = r"^(tr|bat|ac|rdy_\d+|arm_\d+|al_\d+|fa_\d+|chm_\d+|r\d+|z\d+)$"
BINARY_SENSOR_TYPE_ID_DESCRIPTION = "tr, bat, ac, rdy_<digits>, arm_<digits>, al_<digits>, fa_<digits>, chm_<digits>, r<digits>, z<digits>"
TEXT_SENSOR_TYPE_ID_REGEX = r"^(ss|zs|tr_msg|evt|ps_\d+|msg_\d+|ln1_\d+|ln2_\d+|bp_\d+|za_\d+|user_\d+)$"
TEXT_SENSOR_TYPE_ID_DESCRIPTION = "ss, zs, tr_msg, evt, ps_<digits>, msg_<digits>, ln1_<digits>, ln2_<digits>, bp_<digits>, za_<digits>, user_<digits>"

CONFIG_SCHEMA = cv.Schema(
    {
    cv.GenerateID(): cv.declare_id(AlarmComponent),
    cv.Optional(CONF_ACCESSCODE, default=""): cv.string  ,
    cv.Optional(CONF_MAXZONES, default=""): cv.int_, 
    cv.Optional(CONF_USERCODES, default=""): cv.string, 
    cv.Optional(CONF_DEFAULTPARTITION, default=""): cv.int_, 
    cv.Optional(CONF_DEBUGLEVEL, default=""): cv.int_, 
    cv.Optional(CONF_READPIN, default=""): cv.int_, 
    cv.Optional(CONF_WRITEPIN, default="255"): cv.int_, 
    cv.Optional(CONF_CLOCKPIN, default=""): cv.int_,
    cv.Optional(CONF_INVERT_WRITE, default='true'): cv.boolean,
    cv.Optional(CONF_EXPANDER1, default=0): cv.int_, 
    cv.Optional(CONF_EXPANDER2, default=0): cv.int_, 
    cv.Optional(CONF_REFRESHTIME):cv.int_,
    cv.Optional(CONF_TROUBLEFETCHCMD):cv.string,
    cv.Optional(CONF_TROUBLEFETCH):cv.boolean,
    cv.Optional(CONF_DEBOUNCE,default='false'): cv.boolean,      
    cv.Optional(CONF_CLEAN,default='false'): cv.boolean,  
    cv.Optional(CONF_AUTOPOPULATE,default='false'): cv.boolean,
    cv.Optional(CONF_DETAILEDPARTITIONSTATE,default='false'):cv.boolean,
    cv.Optional(CONF_EVENTFORMAT,default='plain'): cv.one_of('json','plain',lower=True),  
    cv.Optional(CONF_STACK_SIZE):cv.int_, 
    cv.Optional(CONF_USE_ESP_IDF_TIMER,default='true'):cv.boolean, 
    }
)

web_keypad_ns = cg.esphome_ns.namespace("web_keypad")
WebKeypad = web_keypad_ns.class_("WebServer", cg.Component, cg.Controller)

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


def validate_id_code(value, is_binary_sensor=True):
    """Validate the type_id for binary or text sensors."""
    if is_binary_sensor:
        regex = BINARY_SENSOR_TYPE_ID_REGEX
        description = BINARY_SENSOR_TYPE_ID_DESCRIPTION
    else:
        regex = TEXT_SENSOR_TYPE_ID_REGEX
        description = TEXT_SENSOR_TYPE_ID_DESCRIPTION

    if not value or not re.fullmatch(regex, value):
        raise cv.Invalid(f"Invalid type_id '{value}'. Allowed formats: {description}")

    return value

ALARM_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TYPE_ID, default=""): cv.Any(cv.string_strict, validate_id_code),
        cv.Optional(CONF_PARTITION,default=0): cv.int_,
        cv.GenerateID(CONF_ALARM_ID): cv.use_id(AlarmComponent),
    }
)

async def to_code(config):
    if CORE.using_arduino and CORE.is_esp32:
        #we double usual stack size
        stack =f"SET_LOOP_TASK_STACK_SIZE(16 * 1024);"
        if CONF_STACK_SIZE in config and config[CONF_STACK_SIZE]:
            stack =f"SET_LOOP_TASK_STACK_SIZE({config[CONF_STACK_SIZE]} * 1024);"
        cg.add_global(cg.RawStatement("#if not defined(USE_STACK_SIZE)"))    
        cg.add_global(cg.RawStatement(stack))
        cg.add_global(cg.RawStatement("#define USE_STACK_SIZE"))
        cg.add_global(cg.RawStatement("#endif"))
    if config[CONF_EVENTFORMAT]=="json":
        cg.add_define("USE_JSON_EVENT")
    cg.add_define("USE_DSC_PANEL")   
    if config[CONF_DETAILEDPARTITIONSTATE]:
        cg.add_define("DETAILED_PARTITION_STATE")
    old_dir = CORE.relative_build_path("src")
#    if config[CONF_CLEAN] or os.path.exists(old_dir+'/dscAlarm.h'):
#        real_clean_build()
    if not config[CONF_EXPANDER1] and not config[CONF_EXPANDER2]:
        cg.add_define("DISABLE_EXPANDER")

    var = cg.new_Pvariable(config[CONF_ID],config[CONF_CLOCKPIN],config[CONF_READPIN],config[CONF_WRITEPIN],config[CONF_INVERT_WRITE])
    if config[CONF_USE_ESP_IDF_TIMER] and CORE.is_esp32:
        cg.add_define("USE_ESP_IDF_TIMER")
    if CONF_ACCESSCODE in config:
        cg.add(var.set_accessCode(config[CONF_ACCESSCODE]));
    if CONF_MAXZONES in config:
        cg.add(var.set_maxZones(config[CONF_MAXZONES]));
    if CONF_REFRESHTIME in config:
        cg.add(var.set_refresh_time(config[CONF_REFRESHTIME]));
    if CONF_TROUBLEFETCH in config:
        cg.add(var.set_trouble_fetch(config[CONF_TROUBLEFETCH]));
    if CONF_TROUBLEFETCHCMD in config:
        cg.add(var.set_trouble_fetch_cmd(config[CONF_TROUBLEFETCHCMD]));
    if CONF_USERCODES in config:
        cg.add(var.set_userCodes(config[CONF_USERCODES]));
    if CONF_DEFAULTPARTITION in config:
        cg.add(var.set_defaultPartition(config[CONF_DEFAULTPARTITION]));
    if CONF_DEBUGLEVEL in config:
        cg.add(var.set_debug(config[CONF_DEBUGLEVEL]));
    if CONF_EXPANDER1 in config:
        cg.add(var.set_expanderAddr(config[CONF_EXPANDER1]));
    if CONF_EXPANDER2 in config:
        cg.add(var.set_expanderAddr(config[CONF_EXPANDER2]));
       
    await cg.register_component(var, config)

#def real_clean_build():
#    import shutil
#    build_dir = CORE.relative_build_path("")
#    if os.path.isdir(build_dir):
#        _LOGGER.info("Deleting %s", build_dir)
#        shutil.rmtree(build_dir)

async def setup_alarm_sensor(var, config,is_binary_sensor=True):
    """Set up custom properties for an alarm sensor"""
    paren = await cg.get_variable(config[CONF_ALARM_ID])

    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_TYPE_ID]))))
        if is_binary_sensor:
            cg.add(paren.createZoneFromObj(var,config[CONF_PARTITION]))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_ID].id))))
        if is_binary_sensor:
            cg.add(paren.createZoneFromObj(var,config[CONF_PARTITION]))
    if is_binary_sensor:
        cg.add(var.publish_state(False))
        cg.add(var.set_trigger_on_initial_state(True))
    else:
        cg.add(var.publish_state(" "))
    if web_keypad_config := config.get(CONF_WEB_KEYPAD):
        from esphome.components import web_keypad
        await web_keypad.add_entity_config(var, web_keypad_config)

def generate_validate_sensor_config(is_binary_sensor=True):
    """Generate a validation function for sensor configuration."""
    def validate_sensor_config(config):
        id_val = config.get(CONF_TYPE_ID) or config[CONF_ID].id
        validate_id_code(id_val, is_binary_sensor)
        return config

    return validate_sensor_config
