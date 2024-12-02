import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
#from esphome.components import mqtt
from esphome.core import CORE
import os
import logging
import pathlib
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import ( VARIANT_ESP32C3 )
from esphome.helpers import copy_file_if_changed
    
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


systemstatus= '''[&](std::string statusCode) {
      alarm_panel::alarmPanelPtr->publishTextState("ss",0,&statusCode); 
    }'''
partitionstatus= '''[&](std::string statusCode,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishTextState("ps_",partition,&statusCode); 
      alarm_panel::alarmPanelPtr->publishBinaryState("al_",partition,(statusCode.compare("triggered")==0));        
    }'''    
partitionmsg= '''[&](std::string msg,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishTextState("msg_",partition,&msg); 
    }'''    
panelstatus= '''[&](alarm_panel::panelStatus ps,bool open,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishPanelStatus(ps,open,partition);
    }'''      
line1 ='''[&](std::string msg,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishTextState("ln1_",partition,&msg);
    }'''
line2='''[&](std::string msg,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishTextState("ln2_",partition,&msg);
    }'''
beeps='''[&](std::string  beeps,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishTextState("bp_",partition,&beeps); 
    }'''
zonemsg='''[&](std::string msg) {
      alarm_panel::alarmPanelPtr->publishTextState("zs",0,&msg);  
    }'''
troublemsg='''[&](std::string msg) {
      alarm_panel::alarmPanelPtr->publishTextState("tr_msg",0,&msg);  
    }'''    
eventinfo='''[&](std::string msg) {
      alarm_panel::alarmPanelPtr->publishTextState("evt",0,&msg);  
    }''' 
    
firestatus='''[&]( bool open,uint8_t partition) {
      alarm_panel::alarmPanelPtr->publishBinaryState("fa_",partition,open);    
    }'''
zonebinary='''[&](int zone, bool open) {
      std::string sensor = "z" + std::to_string(zone) ;
      alarm_panel::alarmPanelPtr->publishBinaryState(sensor,0,open);    
    }'''
relay='''[&](uint8_t channel,bool open) {
      std::string sensor = "r"+ std::to_string(channel);
      alarm_panel::alarmPanelPtr->publishBinaryState(sensor,0,open);       
    }'''

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
    if config[CONF_AUTOPOPULATE]:
        cg.add_define("AUTOPOPULATE")
    if config[CONF_DETAILEDPARTITIONSTATE]:
        cg.add_define("DETAILED_PARTITION_STATE")
    old_dir = CORE.relative_build_path("src")
    if config[CONF_CLEAN] or os.path.exists(old_dir+'/dscAlarm.h'):
        real_clean_build()
    if config[CONF_DEBOUNCE]:
       cg.add_build_flag("-DDEBOUNCE")  
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
        cg.add(var.set_expanderAddr(1,config[CONF_EXPANDER1]));
    if CONF_EXPANDER2 in config:
        cg.add(var.set_expanderAddr(2,config[CONF_EXPANDER2]));

       
    cg.add(var.onSystemStatusChange(cg.RawExpression(systemstatus)))  
    cg.add(var.onPartitionStatusChange(cg.RawExpression(partitionstatus)))  
    cg.add(var.onPartitionMsgChange(cg.RawExpression(partitionmsg)))  
    cg.add(var.onPanelStatusChange(cg.RawExpression(panelstatus)))  
    cg.add(var.onLine1Display(cg.RawExpression(line1))) 
    cg.add(var.onLine2Display(cg.RawExpression(line2)))    
    cg.add(var.onBeeps(cg.RawExpression(beeps)))    
    cg.add(var.onZoneMsgStatus(cg.RawExpression(zonemsg)))   
    cg.add(var.onEventInfo(cg.RawExpression(eventinfo)))    
    cg.add(var.onZoneStatusChange(cg.RawExpression(zonebinary)))    
    cg.add(var.onFireStatusChange(cg.RawExpression(firestatus)))   
    cg.add(var.onTroubleMsgStatus(cg.RawExpression(troublemsg)))    
    cg.add(var.onRelayChannelChange(cg.RawExpression(relay)))      
    await cg.register_component(var, config)
    
def real_clean_build():
    import shutil
    build_dir = CORE.relative_build_path("")
    if os.path.isdir(build_dir):
        _LOGGER.info("Deleting %s", build_dir)
        shutil.rmtree(build_dir)
    