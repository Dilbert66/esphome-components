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
AlarmComponent = component_ns.class_('vistaECPHome', cg.PollingComponent)


CONF_ACCESSCODE="accesscode"
CONF_MAXZONES="maxzones"
CONF_MAXPARTITIONS="maxpartitions"
CONF_RFSERIAL="rfseriallookup"
CONF_DEFAULTPARTITION="defaultpartition"
CONF_DEBUGLEVEL="vistadebuglevel"
CONF_KEYPAD1="keypadaddr1"
CONF_KEYPAD2="keypadaddr2"
CONF_KEYPAD3="keypadaddr3"
CONF_AUIADDR="auiaddr"
CONF_RXPIN="rxpin"
CONF_TXPIN="txpin"
CONF_MONITORPIN="monitorpin"
CONF_EXPANDER1="expanderaddr1"
CONF_EXPANDER2="expanderaddr2"
CONF_RELAY1="relayaddr1"
CONF_RELAY2="relayaddr2"
CONF_RELAY3="relayaddr3"
CONF_RELAY4="relayaddr4"
CONF_TTL="ttl"
CONF_QUICKARM="quickarm"
CONF_LRR="lrrsupervisor"
CONF_CLEAN="clean_build"
CONF_FAULT="fault_text"
CONF_BYPASS="bypas_text"
CONF_ALARM="alarm_text"
CONF_FIRE="fire_text"
CONF_CHECK="check_text"
CONF_TRBL="trbl_text"
CONF_HITSTAR="hitstar_text"
CONF_INVERT_RX="invert_rx"
CONF_INVERT_TX="invert_tx"
CONF_INVERT_MON="invert_mon"
CONF_INPUT_MON="input_mode_mon"
CONF_INPUT_RX="input_mode_rx"
CONF_AUTOPOPULATE="autopopulate"
CONF_USEASYNC="use_async_polling"
CONF_STACK_SIZE="stack_size"
CONF_TYPE_ID = "id_code"
CONF_PARTITION="partition"
CONF_ALARM_ID = "alarm_id"
CONF_SORTING_GROUP_ID = "sorting_group_id"
CONF_SORTING_WEIGHT = "sorting_weight"
CONF_WEB_KEYPAD_ID="web_keypad_id"
CONF_WEB_KEYPAD="web_keypad"
CONF_DEVICE_SERIAL="device_serial"
CONF_DEVICE_LOOP="device_loop"
CONF_DEVICE_TYPE="device_type"
CONF_RF_ADDR="rf_receiver_addr"
CONF_EMULATE_RF_RECEIVER="emulate_rf_receiver"
CONF_EMULATED="emulated"

BINARY_SENSOR_TYPE_ID_REGEX = r"^(ac|bat|trbl_\d+|byp_\d+|rdy_\d+|arm_\d+|arma_\d+|arms_\d+|armi_\d+|armn_\d+|alm_\d+|fire_\d+|chm_\d+|r\d+|z\d+)$"
BINARY_SENSOR_TYPE_ID_DESCRIPTION = "ac, bat, trbl_<digits>,byp_<digits>, rdy_<digits>, arm_<digits>, arma_<digits>, arms_<digits>, arma_<digits>, armn_<digits>,alm_<digits>, fire_<digits>, chm_<digits>, r<digits>, z<digits>"
TEXT_SENSOR_TYPE_ID_REGEX = r"^(zs|lrr|rf|ss_\d+|ln1_\d+|ln2_\d+|bp_\d+|z\d+)$"
TEXT_SENSOR_TYPE_ID_DESCRIPTION = "zs, lrr, rf, ss_<digits>, ln1_<digits>, ln2_<digits>, bp_<digits>, z<digits>"

CONFIG_SCHEMA = cv.Schema(
    {
    cv.GenerateID(): cv.declare_id(AlarmComponent),
    cv.Optional(CONF_ACCESSCODE,default=""): cv.string  ,
    cv.Optional(CONF_MAXZONES,default=32): cv.int_, 
    cv.Optional(CONF_MAXPARTITIONS,default=1): cv.int_, 
    cv.Optional(CONF_RFSERIAL,default=""): cv.string, 
    cv.Optional(CONF_DEFAULTPARTITION): cv.int_, 
    cv.Optional(CONF_DEBUGLEVEL): cv.int_, 
    cv.Optional(CONF_KEYPAD1,default=17): cv.int_, 
    cv.Optional(CONF_KEYPAD2,default=0): cv.int_, 
    cv.Optional(CONF_KEYPAD3,default=0): cv.int_, 
    cv.Optional(CONF_AUIADDR,default=0): cv.int_,
    cv.Optional(CONF_RXPIN): cv.int_, 
    cv.Optional(CONF_TXPIN): cv.int_, 
    cv.Optional(CONF_MONITORPIN): cv.int_, 
    cv.Optional(CONF_EXPANDER1): cv.int_, 
    cv.Optional(CONF_EXPANDER2): cv.int_, 
    cv.Optional(CONF_RELAY1): cv.int_, 
    cv.Optional(CONF_RELAY2): cv.int_, 
    cv.Optional(CONF_RELAY3): cv.int_, 
    cv.Optional(CONF_RELAY4): cv.int_, 
    cv.Optional(CONF_TTL): cv.int_, 
    cv.Optional(CONF_QUICKARM): cv.boolean, 
    cv.Optional(CONF_LRR): cv.boolean, 
    cv.Optional(CONF_CLEAN,default='false'): cv.boolean,     
    cv.Optional(CONF_FAULT): cv.string  ,
    cv.Optional(CONF_BYPASS): cv.string  ,
    cv.Optional(CONF_ALARM): cv.string  ,
    cv.Optional(CONF_FIRE): cv.string  ,  
    cv.Optional(CONF_CHECK): cv.string  ,
    cv.Optional(CONF_TRBL): cv.string  ,   
    cv.Optional(CONF_HITSTAR): cv.string  ,
    cv.Optional(CONF_USEASYNC,default= 'true'): cv.boolean,
    cv.Optional(CONF_INVERT_RX, default='true'): cv.boolean, 
    cv.Optional(CONF_INVERT_TX, default='true'): cv.boolean,   
    cv.Optional(CONF_INVERT_MON, default='true'): cv.boolean,  
    cv.Optional(CONF_AUTOPOPULATE,default='false'): cv.boolean,  
    cv.Optional(CONF_INPUT_RX,default='INPUT'): cv.one_of('INPUT_PULLUP','INPUT_PULLDOWN','INPUT',upper=True),
    cv.Optional(CONF_INPUT_MON,default='INPUT'): cv.one_of('INPUT_PULLUP','INPUT_PULLDOWN','INPUT',upper=True),
    cv.Optional(CONF_STACK_SIZE):cv.int_,
    cv.Optional(CONF_EMULATE_RF_RECEIVER,default= 'false'): cv.boolean,
    cv.Optional(CONF_RF_ADDR,default=0xff): cv.int_, 
    }
)


web_keypad_ns = cg.esphome_ns.namespace("web_keypad")
WebKeypad = web_keypad_ns.class_("WebServer", cg.Component, cg.Controller)

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

ALARM_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALARM_ID): cv.use_id(AlarmComponent),
        cv.Optional(CONF_TYPE_ID, default=""): cv.Any(cv.string_strict, validate_id_code),
        cv.Optional(CONF_PARTITION,default=0): cv.int_,
        cv.Optional(CONF_DEVICE_SERIAL,default=0):cv.int_,
        cv.Optional(CONF_DEVICE_LOOP,default=0):cv.int_,
        cv.Optional(CONF_DEVICE_TYPE,default="WIRED"): cv.one_of("WIRED","RF","LOOP",upper=True),
        cv.Optional(CONF_EMULATED,default=False):cv.boolean
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

    cg.add_define("USE_VISTA_PANEL")  
    if CORE.is_esp8266:
        cg.add_define("INPUT_PULLDOWN INPUT")
    old_dir = CORE.relative_build_path("src")    
    # if config[CONF_CLEAN] or os.path.exists(old_dir / 'vistaalarm.h'):
    #     real_clean_build()

    if  config[CONF_USEASYNC]:
        cg.add_define("USETASK")
    var = cg.new_Pvariable(config[CONF_ID],config[CONF_KEYPAD1],config[CONF_RXPIN],config[CONF_TXPIN],config[CONF_MONITORPIN],config[CONF_MAXZONES],config[CONF_MAXPARTITIONS],config[CONF_INVERT_RX],config[CONF_INVERT_TX],config[CONF_INVERT_MON],cg.RawExpression(config[CONF_INPUT_RX]),cg.RawExpression(config[CONF_INPUT_MON]))
    
    if CONF_ACCESSCODE in config:
        cg.add(var.set_accessCode(config[CONF_ACCESSCODE]));
    if CONF_MAXZONES in config:
        cg.add(var.set_maxZones(config[CONF_MAXZONES]))
    if CONF_RFSERIAL in config:
        cg.add(var.set_rfSerialLookup(config[CONF_RFSERIAL]))
    if CONF_DEFAULTPARTITION in config:
        cg.add(var.set_defaultPartition(config[CONF_DEFAULTPARTITION]))
    if CONF_DEBUGLEVEL in config:
        cg.add(var.set_debug(config[CONF_DEBUGLEVEL]))
    if CONF_KEYPAD1 in config:
        cg.add(var.set_partitionKeypad(1,config[CONF_KEYPAD1]))
    if CONF_KEYPAD2 in config:
        cg.add(var.set_partitionKeypad(2,config[CONF_KEYPAD2]))
    if CONF_KEYPAD3 in config:
        cg.add(var.set_partitionKeypad(3,config[CONF_KEYPAD3]))
    if CONF_EXPANDER1 in config:
        cg.add(var.set_expanderAddr(config[CONF_EXPANDER1]))
    if CONF_EXPANDER2 in config:
        cg.add(var.set_expanderAddr(config[CONF_EXPANDER2]))
    if CONF_RELAY1 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY1]))
    if CONF_RELAY2 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY2]))
    if CONF_RELAY3 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY3]))
    if CONF_RELAY4 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY4]))
    if CONF_TTL in config:
        cg.add(var.set_ttl(config[CONF_TTL]))        
    if CONF_QUICKARM in config:
        cg.add(var.set_quickArm(config[CONF_QUICKARM]))       
    if CONF_LRR in config:
        cg.add(var.set_lrrSupervisor(config[CONF_LRR]))     
    if CONF_AUIADDR in config:
        cg.add(var.set_auiaddr(config[CONF_AUIADDR]))
    if CONF_FAULT in config:
        cg.add(var.set_text(1,config[CONF_FAULT]))
    if CONF_BYPASS in config:
        cg.add(var.set_text(2,config[CONF_BYPASS]))
    if CONF_ALARM in config:
        cg.add(var.set_text(3,config[CONF_ALARM]))
    if CONF_FIRE in config:
        cg.add(var.set_text(4,config[CONF_FIRE]))
    if CONF_CHECK in config:
        cg.add(var.set_text(5,config[CONF_CHECK]))
    if CONF_TRBL in config:
        cg.add(var.set_text(6,config[CONF_TRBL]))  
    if CONF_HITSTAR in config:
        cg.add(var.set_text(7,config[CONF_HITSTAR])) 
    if config[CONF_EMULATE_RF_RECEIVER]:
        cg.add(var.set_rf_emulation(config[CONF_EMULATE_RF_RECEIVER]))
    cg.add(var.set_rf_addr(config[CONF_RF_ADDR]))
  
    await cg.register_component(var, config)
 
# def real_clean_build():
#     import shutil
#     build_dir = CORE.relative_build_path("")
#     if os.path.isdir(build_dir):
#         _LOGGER.info("Deleting %s", build_dir)
#         shutil.rmtree(build_dir)

async def setup_alarm_sensor(var, config,is_binary_sensor=True):
    """Set up custom properties for an alarm sensor"""
    paren = await cg.get_variable(config[CONF_ALARM_ID])
    device_type=0
    if config[CONF_DEVICE_TYPE] =="RF":
        device_type=1
    if config[CONF_DEVICE_TYPE] =="LOOP":
        device_type=2
    if config.get(CONF_TYPE_ID):
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_TYPE_ID]))))
        cg.add(paren.createZoneFromObj(var,config[CONF_PARTITION],config[CONF_DEVICE_SERIAL],config[CONF_DEVICE_LOOP],device_type,config[CONF_EMULATED]))
    elif config[CONF_ID] and config[CONF_ID].is_manual:
        cg.add(var.set_object_id(sanitize(snake_case(config[CONF_ID].id))))
        cg.add(paren.createZoneFromObj(var,config[CONF_PARTITION],config[CONF_DEVICE_SERIAL],config[CONF_DEVICE_LOOP],device_type,config[CONF_EMULATED]))
    if is_binary_sensor:
        cg.add(var.publish_state(False))
        cg.add(var.set_trigger_on_initial_state(True))
    else:
        cg.add(var.publish_state(" "))
    # cg.register_component(var,config)
    if web_keypad_config := config.get(CONF_WEB_KEYPAD):
        from esphome.components import web_keypad
        await web_keypad.add_entity_config(var, web_keypad_config)
        
def generate_validate_sensor_config(is_binary_sensor=True):
    """Generate a validation function for sensor configuration."""
    def validate_sensor_config(config):
        if config[CONF_DEVICE_SERIAL] > 0:
            if (config[CONF_DEVICE_TYPE]=="WIRED"):
                raise cv.Invalid(f"Option 'device_type' required for RF/LOOP zones. Allowed values: RF, LOOP")
            if config[CONF_DEVICE_LOOP]==0 or config[CONF_DEVICE_LOOP]>3:
                raise cv.Invalid(f"Option 'device_loop' required for RF/LOOP zones. Allowed values: 1,2 or 3")
        id_val = config.get(CONF_TYPE_ID) or config[CONF_ID].id
        validate_id_code(id_val, is_binary_sensor)
        return config


    return validate_sensor_config            
    