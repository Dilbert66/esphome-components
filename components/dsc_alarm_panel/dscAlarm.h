// for project documenation visit https://github.com/Dilbert66/esphome-dsckeybus

#pragma once

#if !defined(ARDUINO_MQTT)
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include <string.h>



#if defined(USE_MQTT)
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#if defined(USE_API) || defined(USE_API_SERVICES)
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
#define dscReadPinDefault 21  // esp32: GPIO21
#define dscWritePinDefault 18 // esp32: GPIO18

#else

#define dscClockPinDefault 5  // esp8266: GPIO5
#define dscReadPinDefault 4   // esp8266: GPIO4
#define dscWritePinDefault 15 // esp8266: GPIO15

#endif

#define maxZonesDefault 32 // set to 64 if your system supports it
#define maxRelays 8
#include "dscKeybusInterface.h"
#include "Regexp.h"
#include <cstring> //esp-idf

extern dscKeybusInterface dsc;
extern bool forceDisconnect;
extern void disconnectKeybus();

#if !defined(ARDUINO_MQTT)
namespace esphome
{
  namespace alarm_panel
  {
#endif


#if defined(ESPHOME_MQTT)
    extern std::function<void(const std::string &, JsonObject)> mqtt_callback;
    const char setalarmcommandtopic[] PROGMEM = "/alarm/set";
#endif



    typedef struct
    {
      byte lbcount = 0;
      byte bcount = 0;
      byte ccount = 0;
      byte ecount = 0;
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
    const char ml1[] PROGMEM = "Secure System: Before Arming <>";

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
    const char STATUS_NOT_READY[] PROGMEM = "not_ready"; // ha alarm panel likes to see "unavailable" instead of not_ready when the system can't be armed

    const char *const mainMenu[] PROGMEM = {
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
        mm10};
#define mmsize 11

    const char *const outputMenu[] PROGMEM = {
        om0,
        om1,
        om2};
#define omsize 3

    const char *const troubleMenu[] PROGMEM = {
        tm0,
        tm1,
        tm2,
        tm3,
        tm4,
        tm5,
        tm6,
        tm7,
        tm8};
#define tmsize 9
    const char *const serviceMenu[] PROGMEM = {
        sm0,
        sm1,
        sm2,
        sm3,
        sm4,
        sm5,
        sm6,
        sm7,
        sm8};
#define smsize 9

    const char *const userMenu[] PROGMEM = {
        um0,
        um1,
        um2,
        um3,
        um4,
        um5,
        um6};
#define umsize 7

    const char *const statusMenu[] PROGMEM = {
        am0,
        am1,
        am2,
        am3,
        am4,
        am5};
#define amsize 6

    const char *const statusMenuLabels[] PROGMEM = {
        ml0,
        ml1};

#define mlsize 2

// binary, no zone
#define TRSTATUS "tr"
#define BATSTATUS "bat"
#define ACSTATUS "ac"
#define RDYSTATUS "rdy"
#define ARMSTATUS "arm"
#define ALARMSTATUS "al"
#define FIRE "fa"
#define CHIMESTATUS "chm"

// text, no zone
#define SYSTEMSTATUS "ss"
#define ZONESTATUS "zs"
#define TROUBLE "tr_msg"
#define EVENT "evt"

// text, has numeric suffix with "_" prefix
#define PARTITIONSTATUS "ps"
#define ALARMSTATUS "al"
#define PARTITIONMSG "msg"
#define LINE1 "ln1"
#define LINE2 "ln2"
#define BEEP "bp"
#define ZONEALARM "za"
#define USER "user"

// binary, has numeric suffix without "_"
#define RELAY "r"
#define ZONE "z"
#define CHIMESTATUS "chm"

//misc
#define TRIGGERED "triggered"

#if !defined(USE_API)

#if defined(USE_TIME)
    class DSCkeybushome : public time::RealTimeClock
    {
#else
    class DSCkeybushome : public PollingComponent
    {
#endif

#elif defined(ARDUINO_MQTT)
class DSCkeybushome
{
#else

#if defined(USE_TIME)
class DSCkeybushome : public api::CustomAPIDevice, public time::RealTimeClock
{
#else
class DSCkeybushome : public api::CustomAPIDevice, public PollingComponent
{
#endif

#endif

    public:
      DSCkeybushome(byte dscClockPin, byte dscReadPin, byte dscWritePin, bool setInvertWrite = true);

#if defined(ARDUINO_MQTT)
      std::function<void(const std::string &, uint8_t, std::string *)> textSensorCallback;
      std::function<void(const std::string &, uint8_t, bool)> binarySensorCallback;
      void setBinarySensorCallback(std::function<void(const std::string &cstr, uint8_t partition, bool open)> callback)
      {
        binarySensorCallback = callback;
      }
      void setTextSensorCallback(std::function<void(const std::string &cstr, uint8_t partition, std::string *text)> callback)
      {
        textSensorCallback = callback;
      }
#endif

#if defined (USE_ESP_IDF)
 unsigned long millis() {
     return esp_timer_get_time() / 1000;
 }

unsigned long micros() {
     return esp_timer_get_time() ;
 }

 #endif



      struct zoneType
      {
#if !defined(ARDUINO_MQTT)
        binary_sensor::BinarySensor *binary_sensor;
#endif
        byte partition;
        byte zone;
        byte tamper : 1;
        byte battery_low : 1;
        byte open : 1;
        byte alarm : 1;
        byte enabled : 1;
        byte bypassed : 1;
      };

      void publishZoneStatus(zoneType *zt)
      {
        if (zt == NULL)
          return;
#if defined(ARDUINO_MQTT)
        publishBinaryState(ZONE, zt->zone, zt->open);
#else
    if (zt->binary_sensor != NULL)
      zt->binary_sensor->publish_state(zt->open);
   
#endif
      }

      void publishPanelStatus(const char *sensor, bool open, uint8_t partition)
      {
        publishBinaryState(sensor, partition, open);
      }

      void publishSystemStatus(std::string statuscode)
      {
        publishTextState(SYSTEMSTATUS, 0, &statuscode);
      }
      void publishPartitionStatus(std::string statuscode, uint8_t partition)
      {
        publishTextState(PARTITIONSTATUS, partition, &statuscode);
        publishBinaryState(ALARMSTATUS, partition, (statuscode.compare(TRIGGERED) == 0));
      }
      void publishPartitionMsg(std::string msg, uint8_t partition)
      {
        publishTextState(PARTITIONMSG, partition, &msg);
      }

      void publishLine1(std::string msg, uint8_t partition)
      {
        publishTextState(LINE1, partition, &msg);
      }
      void publishLine2(std::string msg, uint8_t partition)
      {
        publishTextState(LINE2, partition, &msg);
      }

      void publishBeeps(std::string beeps, uint8_t partition)
      {
        publishTextState(BEEP, partition, &beeps);
      }
      void publishZoneAlarm(std::string zone, uint8_t partition)
      {
        publishTextState(ZONEALARM, partition, &zone);
      }
      void publishUserArmingDisarming(std::string user, uint8_t partition)
      {
        publishTextState(USER, partition, &user);
      }

      void publishZoneMsgStatus(std::string msg)
      {
        publishTextState(ZONESTATUS, 0, &msg);
      }
      void publishTroubleMsgStatus(std::string msg)
      {
        publishTextState(TROUBLE, 0, &msg);
      }
      void publishEventInfo(std::string msg)
      {
        publishTextState(EVENT, 0, &msg);
      }

      void publishFireStatus(bool open, uint8_t partition)
      {
        publishBinaryState(FIRE, partition, open);
      }
      void publishRelayStatus(int channel, bool open)
      {
        std::string sensor = RELAY + std::to_string(channel);
        publishBinaryState(sensor, 0, open);
      }

      void set_panel_time();

      void publishBinaryState(const std::string &cstr, uint8_t partition, bool open);
      void publishTextState(const std::string &cstr, uint8_t partition, std::string *text);

      void set_panel_time_manual(int year, int month, int day, int hour, int minute);

      void set_accessCode(const char *ac);
      void set_maxZones(int mz);
      void set_userCodes(const char *uc);
      void set_defaultPartition(uint8_t dp);
      void set_debug(uint8_t db);
      void set_expanderAddr(uint8_t addr);
      void set_refresh_time(uint8_t rt);
      void set_trouble_fetch(bool fetch);
      void set_trouble_fetch_cmd(const char *cmd);
      zoneType* createZone(uint16_t z, uint8_t p = 0);
#if !defined(ARDUINO_MQTT)
      void createZoneFromObj(binary_sensor::BinarySensor *obj, uint8_t p = 0);
      void createZoneFromObj(text_sensor::TextSensor *obj, uint8_t p = 0);
#endif
      void stop();

    private:

      int activePartition = 1;
      unsigned long cmdWaitTime;
      bool extendedBufferFlag = false;

      uint32_t refreshTimeSetting = 10 * 60 * 1000; // milliseconds - 10 minutes
      unsigned long lastTroubleLightTime;
      bool troubleFetch = false;
      byte debug;
      const char *laststatus;
      const char *accessCode;
      const char *fetchCmd = "*21#7##";
      const char *userCodes;
      byte maxZones = maxZonesDefault;
      int defaultPartition = 1;

      uint8_t expanderAddr1 = 0;
      uint8_t expanderAddr2 = 0;
      uint8_t zone;
      byte dscClockPin,
          dscReadPin,
          dscWritePin;
      bool invertWrite;
      bool firstrun;
      bool options;
      unsigned long beepTime,
          eventTime;

      struct partitionType
      {
        unsigned long keyPressTime;
        byte lastStatus;
        const char *lastPartitionStatus;
        byte status;
        byte digits;
        byte editIdx;
        byte currentSelection;
        byte selectedZone;
        byte locked : 1;
        byte inprogram : 1;
        byte decimalInput : 1;
        byte hex : 1;
        byte eventViewer : 1;
        byte submitted : 1;
        byte newData : 1;
        byte hexMode : 1;
        byte chime : 1;
      };

      zoneType zonetype_INIT = {
          .binary_sensor = NULL,
          .partition = 0,
          .zone = 0,
          .tamper = false,
          .battery_low = false,
          .open = false,
          .alarm = false,
          .enabled = false,
          .bypassed = false};

      zoneType *getZone(byte z,bool create=false);
      partitionType partitionStatus[dscPartitions];
      bool forceRefresh;
      std::string previousZoneStatusMsg, eventStatusMsg;
      std::vector<zoneType> zoneStatus{};
      byte lastStatus[dscPartitions];
      bool relayStatus[16],
          previousRelayStatus[16];
      bool sendCmd, system0Changed, system1Changed;
      byte system0,
          system1, previousSystem0, previousSystem1;
      byte programZones[dscZones];
      char decimalInputBuffer[6];
      byte line2Digit;
      byte beeps,
          previousBeeps;
      bool refresh;
#if defined(ESPHOME_MQTT)
      std::string topic, topic_prefix;
#endif
      bool zoneActive(byte zone);
      long int toInt(std::string s, int base);
      byte maxPartitions();

#if defined(ARDUINO_MQTT)
    public:
      void begin();
#else
  void setup() override;
#endif

    private:
      std::string getUserName(int usercode, bool append = false, bool returncode = false);
      void toLower(std::string *s);
      std::string partitionStatusGlobal;
      const char * getPartitionStatus(byte partition,std::string & status);
      #ifdef USE_ESP_IDF
      static void setupTask(void *args);
      #endif

    public:
      void set_default_partition(int32_t partition);

      void set_zone_fault(int32_t zone, bool fault);

      void alarm_disarm(std::string code);

      void alarm_arm_home();

      void alarm_arm_night(std::string code);

      void alarm_arm_away();

      void alarm_trigger_fire();

      void alarm_trigger_panic();

    private:
      void loadZones();

      void processMenu(byte key, byte partition = -1);

      void getPrevIdx(const char *tpl, byte partition);

      void getNextIdx(const char *tpl, byte partition);

    public:
      void alarm_keypress(std::string keystring);

      void alarm_keypress_partition(std::string keystring, int32_t partition);

    private:
#if defined(ESPHOME_MQTT)
      static void on_json_message_callback(const std::string &topic, JsonObject payload);
      void on_json_message(const std::string &topic, JsonObject payload);

#endif

      bool isInt(std::string s, int base);

    public:
      void set_alarm_state(std::string state, std::string code = "", int32_t partition = 0);

    private:
      bool check051bCmd();

      void printPacket(const char *label, char cmd, volatile byte cbuf[], int len);

      byte getPartitionE6(byte panelByte);

      bool getEnabledZonesB1(byte inputByte, byte startZone, byte partition);

      bool getEnabledZonesE6(byte inputByte, byte startZone, byte partitionByte);

      std::string getOptionsString();

      bool checkUserCode(byte code);

      byte getNextOption(byte start);

      byte getPreviousOption(byte start);

      byte getNextUserCode(byte start);

      byte getPreviousUserCode(byte start);

      void getPreviousMainOption(byte partition);

      void clearZoneAlarms(byte partition);

      void clearZoneBypass(byte partition);

      byte getNextOpenZone(byte start, byte partition);

      byte getPreviousOpenZone(byte start, byte partition);

      void getNextMainOption(byte partition);

      byte getNextEnabledZone(byte start, byte partition);

      byte getPreviousEnabledZone(byte start, byte partition);

      byte getNextAlarmedZone(byte start, byte partition);

      byte getPreviousAlarmedZone(byte start, byte partition);

      void getBypassZones(byte partition);
#if defined(ARDUINO_MQTT)
    public:
      void loop();
#else
  void update() override;
  float get_loop_priority() const override {
  return 800.0f ; 
}
#endif

      std::string getZoneName(int zone, bool append = false);

      void setStatus(byte partition, bool force = false, bool skip = false);

      // Processes status data not natively handled within the library
      void processStatus();

      void processPanelTone(byte panelByte);

      void processBeeps(byte panelByte,byte partition);
      void processBeeps19(byte panelByte,byte beepbyte);

      void printPanel_0x6E();

      void processLowBatteryZones();

      void processRelayCmd();

      void processProgramZones(byte startByte, byte zoneStart);

      void processEventBufferAA(bool showEvent = false);

      void processEventBufferEC(bool showEvent = false);

      void printPanelStatus0(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus1(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus2(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus3(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus4(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus5(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus14(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus16(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus17(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus18(byte panelByte, byte partition, bool showEvent = false);

      void printPanelStatus1B(byte panelByte, byte partition, bool showEvent = false);

      const __FlashStringHelper *statusText(uint8_t statusCode);
    };

    extern DSCkeybushome *alarmPanelPtr;

#if !defined(ARDUINO_MQTT)
  }
} // namespaces
#endif