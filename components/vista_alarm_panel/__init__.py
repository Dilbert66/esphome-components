import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
import os
import logging

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
CONF_TYPE_ID="code"


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
  
    }
)

async def to_code(config):

    cg.add_define("USE_VISTA_PANEL")  
    if CORE.is_esp8266:
        cg.add_define("INPUT_PULLDOWN INPUT")
    old_dir = CORE.relative_build_path("src")    
    if config[CONF_CLEAN] or os.path.exists(old_dir+'/vistaalarm.h'):
        real_clean_build()
    
    if  config[CONF_USEASYNC]:
        cg.add_define("USETASK")
    var = cg.new_Pvariable(config[CONF_ID],config[CONF_KEYPAD1],config[CONF_RXPIN],config[CONF_TXPIN],config[CONF_MONITORPIN],config[CONF_MAXZONES],config[CONF_MAXPARTITIONS],config[CONF_INVERT_RX],config[CONF_INVERT_TX],config[CONF_INVERT_MON],cg.RawExpression(config[CONF_INPUT_RX]),cg.RawExpression(config[CONF_INPUT_MON]))
    
    if CONF_ACCESSCODE in config:
        cg.add(var.set_accessCode(config[CONF_ACCESSCODE]));
    if CONF_MAXZONES in config:
        cg.add(var.set_maxZones(config[CONF_MAXZONES]));
    if CONF_MAXPARTITIONS in config:
        cg.add(var.set_maxPartitions(config[CONF_MAXPARTITIONS]));
    if CONF_RFSERIAL in config:
        cg.add(var.set_rfSerialLookup(config[CONF_RFSERIAL]));
    if CONF_DEFAULTPARTITION in config:
        cg.add(var.set_defaultPartition(config[CONF_DEFAULTPARTITION]));
    if CONF_DEBUGLEVEL in config:
        cg.add(var.set_debug(config[CONF_DEBUGLEVEL]));
    if CONF_KEYPAD1 in config:
        cg.add(var.set_partitionKeypad(1,config[CONF_KEYPAD1]));
    if CONF_KEYPAD2 in config:
        cg.add(var.set_partitionKeypad(2,config[CONF_KEYPAD2]));
    if CONF_KEYPAD3 in config:
        cg.add(var.set_partitionKeypad(3,config[CONF_KEYPAD3]));
    if CONF_EXPANDER1 in config:
        cg.add(var.set_expanderAddr(config[CONF_EXPANDER1]));
    if CONF_EXPANDER2 in config:
        cg.add(var.set_expanderAddr(config[CONF_EXPANDER2]));
    if CONF_RELAY1 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY1]));
    if CONF_RELAY2 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY2]));
    if CONF_RELAY3 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY3]));
    if CONF_RELAY4 in config:
        cg.add(var.set_expanderAddr(config[CONF_RELAY4]));
    if CONF_TTL in config:
        cg.add(var.set_ttl(config[CONF_TTL]));        
    if CONF_QUICKARM in config:
        cg.add(var.set_quickArm(config[CONF_QUICKARM]));        
    if CONF_LRR in config:
        cg.add(var.set_lrrSupervisor(config[CONF_LRR]));      
    if CONF_AUIADDR in config:
        cg.add(var.set_auiaddr(config[CONF_AUIADDR]));
    if CONF_FAULT in config:
        cg.add(var.set_text(1,config[CONF_FAULT])); 
    if CONF_BYPASS in config:
        cg.add(var.set_text(2,config[CONF_BYPASS])); 
    if CONF_ALARM in config:
        cg.add(var.set_text(3,config[CONF_ALARM])); 
    if CONF_FIRE in config:
        cg.add(var.set_text(4,config[CONF_FIRE])); 
    if CONF_CHECK in config:
        cg.add(var.set_text(5,config[CONF_CHECK])); 
    if CONF_TRBL in config:
        cg.add(var.set_text(6,config[CONF_TRBL]));  
    if CONF_HITSTAR in config:
        cg.add(var.set_text(7,config[CONF_HITSTAR]));  

  
    await cg.register_component(var, config)
 
def real_clean_build():
    import shutil
    build_dir = CORE.relative_build_path("")
    if os.path.isdir(build_dir):
        _LOGGER.info("Deleting %s", build_dir)
        shutil.rmtree(build_dir)

        
            
    