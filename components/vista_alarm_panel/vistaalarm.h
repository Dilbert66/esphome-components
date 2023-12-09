#pragma once
#if !defined(ARDUINO_MQTT)
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/time/real_time_clock.h"
#if defined(USE_MQTT)
#include "esphome/components/mqtt/mqtt_client.h"
#else
#include "esphome/components/api/custom_api_device.h"
#endif
#include "esphome/core/defines.h"
#include "paneltext.h"

#if defined(USE_MQTT)
#define ESPHOME_MQTT
#endif
#endif

#include "vista.h"
#include <string>


 //for documentation see project at https://github.com/Dilbert66/esphome-vistaecp

#define KP_ADDR 17 //only used as a default if not set in the yaml
#define MAX_ZONES 48
#define MAX_PARTITIONS 3  
#define DEFAULTPARTITION 1

//default pins to use for serial comms to the panel
//The pinouts below are only examples. You can choose any other gpio pin that is available and not needed for boot.
//These have proven to work fine.
#ifdef ESP32
//esp32 use gpio pins 4,13,16-39 
#define RX_PIN 22
#define TX_PIN 21
#define MONITOR_PIN 18 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#else
#define RX_PIN 5
#define TX_PIN 4
#define MONITOR_PIN 14 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#endif



extern Vista vista;
extern Stream * OutputStream;
extern void disconnectVista();

#if !defined(ARDUINO_MQTT)
namespace esphome {
namespace alarm_panel {
 
extern void * alarmPanelPtr;    
#if defined(ESPHOME_MQTT)
extern std::function<void(const std::string &, JsonObject)> mqtt_callback;
const char setalarmcommandtopic[] PROGMEM = "/alarm/set"; 
#endif  
  
#endif   
 
enum sysState {
  soffline,
  sarmedaway,
  sarmedstay,
  sbypass,
  sac,
  schime,
  sbat,
  scheck,
  scanceled,
  sarmednight,
  sdisarmed,
  striggered,
  sunavailable,
  strouble,
  salarm,
  sfire,
  sinstant,
  sready,
  sarmed
};



#if !defined(ARDUINO_MQTT)
extern void publishBinaryState(const char * cstr,uint8_t partition,bool open);
extern void publishTextState(const char * cstr,uint8_t partition,std::string * text);
#endif

#if defined(ESPHOME_MQTT) 
class vistaECPHome:  public time::RealTimeClock {
#elif defined(ARDUINO_MQTT)
class vistaECPHome { 
#else
class vistaECPHome: public api::CustomAPIDevice, public time::RealTimeClock {
#endif



    public: 
    
    vistaECPHome(char kpaddr = KP_ADDR, int receivePin = RX_PIN, int transmitPin = TX_PIN, int monitorTxPin = MONITOR_PIN,int maxzones=MAX_ZONES,int maxpartitions=MAX_PARTITIONS);
    

    std:: function < void(int, std::string) > zoneStatusChangeCallback;
    std:: function < void(int, bool) > zoneStatusChangeBinaryCallback;    
    std:: function < void(std::string, uint8_t) > systemStatusChangeCallback;
    std:: function < void(sysState, bool, uint8_t) > statusChangeCallback;
    std:: function < void(std::string , uint8_t) > systemMsgChangeCallback;
    std:: function < void(std::string) > lrrMsgChangeCallback;
    std:: function < void(std::string ) > rfMsgChangeCallback;
    std:: function < void(std::string , uint8_t) > line1DisplayCallback;
    std:: function < void(std::string , uint8_t) > line2DisplayCallback;
    std:: function < void(std::string , uint8_t) > beepsCallback;
    std:: function < void(std::string ) > zoneExtendedStatusCallback;
    std:: function < void(uint8_t, int, bool) > relayStatusChangeCallback;

    void onZoneStatusChange(std:: function < void(int zone,
     std::string msg) > callback) {
      zoneStatusChangeCallback = callback;
    }
    void onZoneStatusChangeBinarySensor(std:: function < void(int zone,
      bool open) > callback) {
      zoneStatusChangeBinaryCallback = callback;
    }    
    void onSystemStatusChange(std:: function < void(std::string status, uint8_t partition) > callback) {
      systemStatusChangeCallback = callback;
    }
    void onStatusChange(std:: function < void(sysState led, bool isOpen, uint8_t partition) > callback) {
      statusChangeCallback = callback;
    }
    void onSystemMsgChange(std:: function < void(std::string msg, uint8_t partition) > callback) {
      systemMsgChangeCallback = callback;
    }
    void onLrrMsgChange(std:: function < void(std::string msg) > callback) {
      lrrMsgChangeCallback = callback;
    }
    void onLine1DisplayChange(std:: function < void(std::string msg, uint8_t partition) > callback) {
      line1DisplayCallback = callback;
    }
    void onLine2DisplayChange(std:: function < void(std::string msg, uint8_t partition) > callback) {
      line2DisplayCallback = callback;
    }
    void onBeepsChange(std:: function < void(std::string beeps, uint8_t partition) > callback) {
      beepsCallback = callback;
    }
    void onZoneExtendedStatusChange(std:: function < void(std::string zoneExtendedStatus) > callback) {
      zoneExtendedStatusCallback = callback;
    }
    void onRelayStatusChange(std:: function < void(uint8_t addr, int channel, bool state) > callback) {
      relayStatusChangeCallback = callback;
    }
    void onRfMsgChange(std:: function < void(std::string msg) > callback) {
      rfMsgChangeCallback = callback;
    }
    
    void zoneStatusUpdate(int zone);
    void set_accessCode(const char * ac) { accessCode=ac; }
    void set_rfSerialLookup(const char * rf) { rfSerialLookup=rf;}
    void set_quickArm(bool qa) { quickArm=qa;}
    void set_displaySystemMsg(bool dsm) {displaySystemMsg=dsm;}
    void set_lrrSupervisor(bool ls) { lrrSupervisor=ls;}
    void set_expanderAddr(uint8_t idx,uint8_t addr) { if (idx && idx < 10) expanderAddr[idx-1] = addr;}
    void set_maxZones(int mz) {maxZones=mz;}
    void set_maxPartitions(uint8_t mp) { maxPartitions=mp;}
    void set_partitionKeypad(uint8_t idx,uint8_t addr) {if (idx && idx<4) partitionKeypads[idx]=addr;}
    void set_defaultPartition(uint8_t dp) {defaultPartition=dp;}
    void set_debug(uint8_t db) {debug=db;}
    void set_ttl(uint32_t t) { TTL=t;};
    void set_text(uint8_t text_idx,const char * text) {
        switch (text_idx) {
            case 1: FAULT = text;break;
            case 2: BYPAS = text;break;
            case 3: ALARM = text;break;
            case 4: FIRE = text;break;
            case 5: CHECK = text;break;
            case 6: TRBL = text;break;
            case 7: HITSTAR = text;break;
            default: break;
        }
        
    }
    
    bool displaySystemMsg = false;
    bool forceRefreshGlobal,forceRefreshZones,forceRefresh;

    sysState currentSystemState,
    previousSystemState;

  private:
    
    int TTL = 30000;
    uint8_t debug=0;
    char keypadAddr1=0;
    int rxPin=0;
    int txPin=0;
    int monitorPin=0;    
    
    const char * accessCode;
    const char * rfSerialLookup;    
    bool quickArm;

    bool lrrSupervisor,    vh;
    int maxZones;
    int maxPartitions;
    char * partitionKeypads;
    int defaultPartition=DEFAULTPARTITION;    
    char expanderAddr[9]={};
    int zone;
    bool sent;
    char p1[18];
    char p2[18];

    uint8_t * partitions;
    std::string topic_prefix,topic;
    char msg[50];
    

    struct zoneType {
      unsigned long time;
      uint8_t partition;
      uint8_t open:1;
      uint8_t bypass:1;
      uint8_t alarm:1;
      uint8_t check:1;
      uint8_t fire:1;
      uint8_t panic:1;
      uint8_t trouble:1;
      uint8_t lowbat:1;
    };
    struct {
        uint8_t bell:1;
        uint8_t wrx1:1;
        uint8_t wrx2:1;
        uint8_t loop:1;
        uint8_t duress:1;
        uint8_t panic1:1;
        uint8_t panic2:1;
        uint8_t panic3:1;
    } otherSup;
        

private:    
    unsigned long lowBatteryTime;

    struct alarmStatusType {
      unsigned long time;
      bool state;
      int zone;
      char prompt[17];
    };

    struct lrrType {
      int code;
      uint8_t qual;
      int zone;
      uint8_t user;
    };

    struct lightStates {
      bool away;
      bool stay;
      bool night;
      bool instant;
      bool bypass;
      bool ready;
      bool ac;
      bool chime;
      bool bat;
      bool alarm;
      bool check;
      bool fire;
      bool canceled;
      bool trouble;
      bool armed;
    };

    lightStates currentLightState,
    previousLightState;
    enum lrrtype {
      user_t,
      zone_t
    };

    struct partitionStateType {
      sysState previousSystemState;
      lightStates previousLightState;
      std::string lastp1=" ";
      std::string lastp2=" ";
      int lastbeeps;
      bool refreshStatus;
      bool refreshLights;
    } ;
    
public:
    partitionStateType * partitionStates;
    
private:
    std::string previousMsg,
    previousZoneStatusMsg;

    alarmStatusType fireStatus,
    panicStatus,
    alarmStatus;
    uint8_t partitionTargets;
    unsigned long sendWaitTime;
    bool firstRun;
    
    struct serialType {
       int zone;
       int mask;
    };

bool zoneActive(uint32_t zone);


std::map<uint32_t,zoneType> extZones;
zoneType nz;

zoneType * getZone(uint32_t z);

serialType getRfSerialLookup(char * serialCode);


#if defined(ESPHOME_MQTT) 
static void on_json_message(const std::string &topic, JsonObject payload);
#endif
 public:
 
 void set_panel_time();
  void set_panel_time_manual(int year,int month,int day,int hour,int minute);

#if defined(ARDUINO_MQTT)
void begin() {
#else
void setup() override;
#endif
    void alarm_disarm(std::string code,int partition);

    void alarm_arm_home(int partition);

    void alarm_arm_night(int partition);

    void alarm_arm_away(int partition);

    void alarm_trigger_fire(std::string code,int partition);

    void alarm_trigger_panic(std::string code,int partition);

    void set_zone_fault(int zone, bool fault);

    void set_keypad_address(int addr) {
     // if (addr > 0 and addr < 24) 
      ///  vista.setKpAddr(addr); //disabled for now 
    }
    
    void alarm_keypress(std::string keystring);

    void alarm_keypress_partition(std::string keystring, int partition);
    
    void setDefaultKpAddr(uint8_t p);
private:
    bool isInt(std::string s, int base);

    long int toInt(std::string s, int base);
    
 

    bool areEqual(char * a1, char * a2, uint8_t len);
    
  int getZoneFromPrompt(char *p1);
   
  bool promptContains(char * p1, const char * msg, int & zone);

  void printPacket(const char * label, char cbuf[], int len) ;

   
    std::string getF7Lookup(char cbuf[]) ;
public:
    void set_alarm_state(std::string const& state, std::string code = "",int partition=DEFAULTPARTITION) ;
private:
    int getZoneFromChannel(uint8_t deviceAddress, uint8_t channel) ;
    
    void translatePrompt(char * cbuf) ;

    void assignPartitionToZone(int zone) ;

    void getPartitionsFromMask() ;

public:
#if defined(ARDUINO_MQTT)
void loop();
#else   
void update() override;
#endif 
   
private:
    const __FlashStringHelper * statusText(int statusCode) ;
  };
#if !defined(ARDUINO_MQTT)
}} //namespaces
#endif


