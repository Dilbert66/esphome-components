//for project documenation visit https://github.com/Dilbert66/esphome-dsckeybus

#pragma once

#if !defined(ARDUINO_MQTT)
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#if defined(USE_MQTT)
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#if defined(USE_API)
#include "esphome/components/api/custom_api_device.h"
#endif

#if defined(USE_TIME)
#include "esphome/components/time/real_time_clock.h"
#endif

#if defined(USE_MQTT)
#define ESPHOME_MQTT
#endif

#endif

#ifdef ESP32

#define dscClockPinDefault 22 // esp32: GPIO22
#define dscReadPinDefault 21 // esp32: GPIO21
#define dscWritePinDefault 18 // esp32: GPIO18

#else

#define dscClockPinDefault 5 // esp8266: GPIO5 
#define dscReadPinDefault 4 // esp8266: GPIO4 
#define dscWritePinDefault 15 // esp8266: GPIO15 

#endif

#define maxZonesDefault 32 //set to 64 if your system supports it
#define maxRelays 8
#include "dscKeybusInterface.h"
#include "Regexp.h"

extern dscKeybusInterface dsc;
extern bool forceDisconnect;
extern void disconnectKeybus();


#if !defined(ARDUINO_MQTT)
namespace esphome {
namespace alarm_panel {
#endif


#if defined(ESPHOME_MQTT)
extern std::function<void(const std::string &, JsonObject)> mqtt_callback;
const char setalarmcommandtopic[] PROGMEM = "/alarm/set"; 
#endif

typedef struct  {
    byte lbcount=0;
    byte bcount=0;
    byte ccount=0;
    byte ecount=0;
} cmdCountType;



const char mm0[] PROGMEM = "Press # to exit";
const char mm1[] PROGMEM = "Zone Bypass";
const char mm2[] PROGMEM = "System Troubles";
const char mm3[] PROGMEM = "Alarm Memory";
const char mm4[] PROGMEM = "Door Chime";
const char mm5[] PROGMEM = "Access Codes";
const char mm6[] PROGMEM = "User Functions";
const char mm7[] PROGMEM = "Output Contact";
const char mm8[] PROGMEM = " ";
const char mm9[] PROGMEM = "No-Entry Arm";
const char mm10[] PROGMEM = "Quick Arming";

const char om0[] PROGMEM = "Press # to exit";
const char om1[] PROGMEM = "O/P 1";
const char om2[] PROGMEM = "O/P 2";

const char tm0[] PROGMEM = "Press # to exit";
const char tm1[] PROGMEM = "Service Required *";
const char tm2[] PROGMEM = "AC Failure";
const char tm3[] PROGMEM = "Tel Line Trouble";
const char tm4[] PROGMEM = "Comm Failure";
const char tm5[] PROGMEM = "Zone Fault *";
const char tm6[] PROGMEM = "Zone Tamper *";
const char tm7[] PROGMEM = "Low Battery *";
const char tm8[] PROGMEM = "System Time";

const char sm0[] PROGMEM = "Press # to exit";
const char sm1[] PROGMEM = "Low Battery";
const char sm2[] PROGMEM = "Bell Circuit";
const char sm3[] PROGMEM = "System Trouble";
const char sm4[] PROGMEM = "System Tamper";
const char sm5[] PROGMEM = "Mod Supervision";
const char sm6[] PROGMEM = "RF Jam detected";
const char sm7[] PROGMEM = "PC5204 Low Battery";
const char sm8[] PROGMEM = "PC5204 AC Fail";

const char um0[] PROGMEM = "Press # to exit";
const char um1[] PROGMEM = "Time and Date";
const char um2[] PROGMEM = "Auto Arm/Disarm";
const char um3[] PROGMEM = "Auto Arm Time";
const char um4[] PROGMEM = "System Test";
const char um5[] PROGMEM = "System Serv/DLS";
const char um6[] PROGMEM = "Event Buffer";

const char am0[] PROGMEM = " ";
const char am1[] PROGMEM = " ";
const char am2[] PROGMEM = "System Trouble:(*2) to view";
const char am3[] PROGMEM = "Bypass Active:(*1) to view";
const char am4[] PROGMEM = "Alarm Memory: (*3) to view";
const char am5[] PROGMEM = "Open Zones:Scroll to view <>";

const char ml0[] PROGMEM = "System is Ready:Ready to Arm <>";
const char ml1[] PROGMEM = "Secure System:Before Arming <>";

const char STATUS_EXIT[] PROGMEM = "arming";
const char STATUS_ENTRY[] PROGMEM = "pending";
const char STATUS_ARM[] PROGMEM = "armed_away";
const char STATUS_STAY[] PROGMEM = "armed_home";
const char STATUS_NIGHT[] PROGMEM = "armed_night";
const char STATUS_ONLINE[] PROGMEM = "online";
const char STATUS_OFFLINE[] PROGMEM = "offline";
const char STATUS_TRIGGERED[] PROGMEM = "triggered";
const char STATUS_READY[] PROGMEM = "ready";
const char STATUS_DISARMED[] PROGMEM = "disarmed";
const char STATUS_NOT_READY[] PROGMEM = "not_ready"; //ha alarm panel likes to see "unavailable" instead of not_ready when the system can't be armed

const char *
  const mainMenu[] PROGMEM = {
    mm0,
    mm1,
    mm2,
    mm3,
    mm4,
    mm5,
    mm6,
    mm7,
    mm8,
    mm9,
    mm10
  };
#define mmsize 11

const char *
  const outputMenu[] PROGMEM = {
    om0,
    om1,
    om2
  };
#define omsize 3

const char *
  const troubleMenu[] PROGMEM = {
    tm0,
    tm1,
    tm2,
    tm3,
    tm4,
    tm5,
    tm6,
    tm7,
    tm8
  };
#define tmsize 9
const char *
  const serviceMenu[] PROGMEM = {
    sm0,
    sm1,
    sm2,
    sm3,
    sm4,
    sm5,
    sm6,
    sm7,
    sm8
  };
#define smsize 9

const char *
  const userMenu[] PROGMEM = {
    um0,
    um1,
    um2,
    um3,
    um4,
    um5,
    um6
  };
#define umsize 7

const char *
  const statusMenu[] PROGMEM = {
    am0,
    am1,
    am2,
    am3,
    am4,
    am5
  };
#define amsize 6

const char *
  const statusMenuLabels[] PROGMEM = {
    ml0,
    ml1
  };

#define mlsize 2


enum panelStatus {
  acStatus,
  batStatus,
  trStatus,
  fireStatus,
  panicStatus,
  rdyStatus,
  armStatus,
  chimeStatus
};




#if !defined(USE_API) 

#if defined(USE_TIME)
class DSCkeybushome:  public time::RealTimeClock {
#else
class DSCkeybushome:  public PollingComponent {
#endif

#elif defined(ARDUINO_MQTT) 
class DSCkeybushome { 
#else
    
#if defined(USE_TIME)
class DSCkeybushome: public api::CustomAPIDevice, public time::RealTimeClock {
#else 
class DSCkeybushome: public api::CustomAPIDevice, public PollingComponent {
#endif

#endif

public:
DSCkeybushome(byte dscClockPin , byte dscReadPin , byte dscWritePin ,bool setInvertWrite=true);

  std:: function < void(uint8_t, bool) > zoneStatusChangeCallback;
  std:: function < void(std::string ) > systemStatusChangeCallback;
  std:: function < void(panelStatus, bool, int) > panelStatusChangeCallback;
  std:: function < void(bool, int) > fireStatusChangeCallback;
  std::function  <void (std::string,int)> partitionStatusChangeCallback;   
  std:: function < void(std::string, int) > partitionMsgChangeCallback;
  std:: function < void(std::string) > zoneMsgStatusCallback;
  std:: function < void(std::string) > troubleMsgStatusCallback;
  std:: function < void(uint8_t, bool) > relayChannelChangeCallback;
  std:: function < void(std::string, int) > line1DisplayCallback;
  std:: function < void(std::string, int) > line2DisplayCallback;
  std:: function < void(std::string) > eventInfoCallback;
  std:: function < void(std::string, int) > lightsCallback;
  std:: function < void(std::string, int) > beepsCallback;

  void onZoneStatusChange(std:: function < void(uint8_t zone, bool isOpen) > callback) {
    zoneStatusChangeCallback = callback;
  }
  void onSystemStatusChange(std:: function < void(std::string  status) > callback) {
    systemStatusChangeCallback = callback;
  }
  void onFireStatusChange(std:: function < void(bool isOpen,int partition) > callback) {
    fireStatusChangeCallback = callback;
  }
  void onPanelStatusChange(std:: function < void(panelStatus ts, bool isOpen, int partition) > callback) {
    panelStatusChangeCallback = callback;
  }
  void onPartitionStatusChange(std:: function < void(std::string status, int partition) > callback) {
    partitionStatusChangeCallback = callback;
  }
  void onPartitionMsgChange(std::function<void (std::string msg,uint8_t partition)> callback) {
    partitionMsgChangeCallback = callback; 
  }  
  void onZoneMsgStatus(std:: function < void(std::string msg) > callback) {
    zoneMsgStatusCallback = callback;
  }
  void onTroubleMsgStatus(std:: function < void(std::string msg) > callback) {
    troubleMsgStatusCallback = callback;
  }
  void onRelayChannelChange(std:: function < void(uint8_t channel, bool state) > callback) {
    relayChannelChangeCallback = callback;
  }

  void onLine1Display(std:: function < void(std::string msg, int partition) > callback) {
    line1DisplayCallback = callback;
  }
  void onLine2Display(std:: function < void(std::string msg, int partition) > callback) {
    line2DisplayCallback = callback;
  }
  void onEventInfo(std:: function < void(std::string msg) > callback) {
    eventInfoCallback = callback;
  }
  void onLights(std:: function < void(std::string msg, int partition) > callback) {
    lightsCallback = callback;
  }
  void onBeeps(std:: function < void(std::string beep, int partition) > callback) {
    beepsCallback = callback;
  }
  
void set_panel_time();


std::vector<binary_sensor::BinarySensor *> bMap;
std::vector<text_sensor::TextSensor *> tMap;

void publishPanelStatus(panelStatus ps,bool open,uint8_t partition);
void publishBinaryState(const std::string& cstr,uint8_t partition,bool open);
void publishTextState(const std::string & cstr,uint8_t partition,std::string * text);

void set_panel_time_manual(int year,int month,int day,int hour,int minute); 

  void set_accessCode(const char * ac);
  void set_maxZones(int mz); 
  void set_userCodes(const char * uc);
  void set_defaultPartition(uint8_t dp);
  void set_debug(uint8_t db);
  void set_expanderAddr(uint8_t idx,uint8_t addr);
  void set_refresh_time(uint8_t  rt);
  void set_trouble_fetch(bool fetch);
  void set_trouble_fetch_cmd(const char * cmd);


  private: 

  int activePartition = 1;
  unsigned long cmdWaitTime;
  bool extendedBufferFlag=false;
 
  uint32_t refreshTimeSetting=5 * 60 * 1000; // milliseconds - 5 minutes
  bool troubleFetch=true;
  byte debug;
  const char * laststatus;
  const char * accessCode;
  const char * fetchCmd="*21#7#";
  const char * userCodes;
  byte maxZones = maxZonesDefault; 
  int defaultPartition = 1;
  
  uint8_t expanderAddr1=0;
  uint8_t expanderAddr2=0;
  uint8_t zone;
  byte dscClockPin,
  dscReadPin,
  dscWritePin;
  bool invertWrite;
  bool firstrun;
  bool options;
  unsigned long beepTime,
  eventTime;

  struct partitionType {
    unsigned long keyPressTime;
    byte lastStatus;
    const char * lastPartitionStatus;
    byte status;
    byte digits;
    byte editIdx;
    byte currentSelection;
    byte selectedZone;
    byte locked:1;    
    byte inprogram:1;    
    byte decimalInput:1;
    byte hex:1;
    byte eventViewer:1;
    byte submitted:1;  
    byte newData:1;
    byte hexMode:1;
    byte chime:1;

  };

  struct zoneType {
    byte partition;
    byte zone;    
    byte tamper:1;
    byte batteryLow:1;
    byte open:1;
    byte alarm:1;
    byte enabled:1;
    byte bypassed:1;
  };
  
  zoneType zonetype_INIT={ 
     .partition=0,     
     .zone=0,
     .tamper=false, 
     .batteryLow=false, 
      .open=false,
     .alarm=false,
     .enabled=false,
     .bypassed=false
       };

  public:
  std::vector<zoneType> zoneStatus{};  
  zoneType * createZone(byte z);  
  zoneType * getZone(byte z);  
  partitionType partitionStatus[dscPartitions];
  bool forceRefresh;
  std::string previousZoneStatusMsg,eventStatusMsg; 
  
  private:
  byte  lastStatus[dscPartitions];  
  bool relayStatus[16],
  previousRelayStatus[16];
  bool sendCmd,system0Changed,system1Changed;
  byte system0,
  system1,previousSystem0,previousSystem1;
  byte programZones[dscZones];
  char decimalInputBuffer[6];
  byte line2Digit;
  byte beeps,
  previousBeeps;
  bool refresh;
  #if defined(ESPHOME_MQTT)
  std::string topic,topic_prefix;
  #endif
  bool zoneActive(byte zone);
  long int toInt(std::string s, int base); 
  
#if defined(ARDUINO_MQTT)
public:
void begin() ;
#else
  void setup() override ;
#endif      

 private:  
  std::string getUserName(char * code) ;
public:
  void set_default_partition(int partition);

  void set_zone_fault(int zone, bool fault) ;

  void alarm_disarm(std::string code) ;

 
  void alarm_arm_home() ;

  void alarm_arm_night(std::string code) ;

  void alarm_arm_away() ;

  void alarm_trigger_fire() ;

  void alarm_trigger_panic() ;

private:

  void loadZones();
 
  void processMenu(byte key, byte partition = -1) ;

  void getPrevIdx(const char * tpl, byte partition) ;

  void getNextIdx(const char * tpl, byte partition) ;

public:
  void alarm_keypress(std::string keystring) ;

  void alarm_keypress_partition(std::string keystring, int partition) ;
private:
#if defined(ESPHOME_MQTT) 
static void on_json_message_callback(const std::string &topic, JsonObject payload) ;
void on_json_message(const std::string &topic, JsonObject payload) ;

#endif

  bool isInt(std::string s, int base) ;
public:
  void set_alarm_state(std::string state, std::string code = "", int partition = 0) ;

private:

  bool check051bCmd();
  
  void printPacket(const char * label, char cmd, volatile byte cbuf[], int len) ;

  byte getPanelBitNumber(byte panelByte, byte startNumber) ;

  bool getEnabledZonesB1(byte inputByte, byte startZone, byte partition) ;

  bool getEnabledZonesE6(byte inputByte, byte startZone, byte partitionByte) ;

  String getOptionsString() ;

  bool checkUserCode(byte code) ;

  byte getNextOption(byte start) ;

  byte getPreviousOption(byte start) ;

  byte getNextUserCode(byte start) ;

  byte getPreviousUserCode(byte start) ;

  void getPreviousMainOption(byte partition) ;

  void clearZoneAlarms(byte partition) ;

  void clearZoneBypass(byte partition) ;

  byte getNextOpenZone(byte start, byte partition) ;

  byte getPreviousOpenZone(byte start, byte partition) ;

  void getNextMainOption(byte partition) ;

  byte getNextEnabledZone(byte start, byte partition) ;

  byte getPreviousEnabledZone(byte start, byte partition) ;

  byte getNextAlarmedZone(byte start, byte partition) ;

  byte getPreviousAlarmedZone(byte start, byte partition) ;

  void getBypassZones(byte partition) ;
#if defined(ARDUINO_MQTT)
  public:
  void loop();
#else   
void update() override; 
#endif


 std::string getZoneName(int zone); 

  void setStatus(byte partition, bool force = false, bool skip = false);


  // Processes status data not natively handled within the library
  void processStatus();

  void printPanelTone(byte panelByte) ;

  void printBeeps(byte panelByte) ;


  void printPanel_0x6E() ;

  void processLowBatteryZones() ;

  void processRelayCmd() ;

  void processProgramZones(byte startByte,byte zoneStart) ;

  void processEventBufferAA(bool showEvent = false) ;

  void processEventBufferEC(bool showEvent = false) ;

  void printPanelStatus0(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus1(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus2(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus3(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus4(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus5(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus14(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus16(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus17(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus18(byte panelByte, byte partition, bool showEvent = false) ;

  void printPanelStatus1B(byte panelByte, byte partition, bool showEvent = false) ;
  
  const __FlashStringHelper *statusText(uint8_t statusCode) ;  

};

extern DSCkeybushome* alarmPanelPtr;


#if !defined(ARDUINO_MQTT)
}}//namespaces
#endif