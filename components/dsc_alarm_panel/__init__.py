import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
import os
import logging
    
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
CONF_TYPE_ID="code"

CONFIG_SCHEMA = cv.Schema(
    {
    cv.GenerateID(): cv.declare_id(AlarmComponent),
    cv.Optional(CONF_ACCESSCODE, default=""): cv.string  ,
    cv.Optional(CONF_MAXZONES, default=""): cv.int_, 
    cv.Optional(CONF_USERCODES, default=""): cv.string, 
    cv.Optional(CONF_DEFAULTPARTITION, default=""): cv.int_, 
    cv.Optional(CONF_DEBUGLEVEL, default=""): cv.int_, 
    cv.Optional(CONF_READPIN, default=""): cv.int_, 
    cv.Optional(CONF_WRITEPIN, default=""): cv.int_, 
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
    }
)

async def to_code(config):

    if config[CONF_EVENTFORMAT]=="json":
        cg.add_define("USE_JSON_EVENT")
    cg.add_define("USE_DSC_PANEL")   
    if config[CONF_DETAILEDPARTITIONSTATE]:
        cg.add_define("DETAILED_PARTITION_STATE")
    old_dir = CORE.relative_build_path("src")
    if config[CONF_CLEAN] or os.path.exists(old_dir+'/dscAlarm.h'):
        real_clean_build()
    # if config[CONF_DEBOUNCE]:
    #    cg.add_build_flag("-DDEBOUNCE")  
    if not config[CONF_EXPANDER1] and not config[CONF_EXPANDER2]:
        cg.add_define("DISABLE_EXPANDER")

    var = cg.new_Pvariable(config[CONF_ID],config[CONF_CLOCKPIN],config[CONF_READPIN],config[CONF_WRITEPIN],config[CONF_INVERT_WRITE])

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
   

    
def real_clean_build():
    import shutil
    build_dir = CORE.relative_build_path("")
    if os.path.isdir(build_dir):
        _LOGGER.info("Deleting %s", build_dir)
        shutil.rmtree(build_dir)
    