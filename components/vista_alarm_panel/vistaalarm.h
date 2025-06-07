#pragma once

#if !defined(ARDUINO_MQTT)
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/time/real_time_clock.h"

// #if defined(AUTOPOPULATE)
// #if defined(TEMPLATE_ALARM)
// #include "esphome/components/template_alarm/binary_sensor/template_binary_sensor.h"
// #else
// #include "esphome/components/template/binary_sensor/template_binary_sensor.h"
// #endif
// #endif

#if defined(USE_MQTT)
#define ESPHOME_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif
#if defined(USE_API)
#include "esphome/components/api/custom_api_device.h"
#endif
#include "esphome/core/defines.h"
#endif

#include "vista.h"
#include <string>
#include "paneltext.h"
#include "Regexp.h"

#if defined(USE_RP2040) && not defined(ESP)
#define ESP rp2040
#endif

// for documentation see project at https://github.com/Dilbert66/esphome-vistaecp

#define KP_ADDR 17 // only used as a default if not set in the yaml
#define MAX_ZONES 48
#define MAX_PARTITIONS 3
#define DEFAULTPARTITION 1

#define SFIRE "fire"
#define SALARM "alm"
#define STROUBLE "trbl"
#define SARMEDSTAY "arms"
#define SARMEDAWAY "arma"
#define SINSTANT "armi"
#define SREADY "rdy"
#define SAC "ac"
#define SBYPASS "byp"
#define SCHIME "chm"
#define SBAT "bat"
#define SARMEDNIGHT "armn"
#define SARMED "arm"
#define SSYSTEMSTATE "ss"
#define SLRR "lrr"
#define SRF "rf"
#define SLINE1 "ln1"
#define SLINE2 "ln2"
#define SBEEP "bp"
#define SZONESTATUS "zs"
#define SZONE "z"
#define SRELAY "r"

#define ASYNC_CORE 1
// #define USETASK

// default pins to use for serial comms to the panel
// The pinouts below are only examples. You can choose any other gpio pin that is available and not needed for boot.
// These have proven to work fine.
#ifdef ESP32
// esp32 use gpio pins 4,13,16-39
#define RX_PIN 22
#define TX_PIN 21
#define MONITOR_PIN 18 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#endif
#ifdef ESP8266
#define RX_PIN 5
#define TX_PIN 4
#define MONITOR_PIN 14 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#endif
#ifdef USE_RP2040
#define RX_PIN 21
#define TX_PIN 20
#define MONITOR_PIN 18 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#endif

#if !defined(ARDUINO_MQTT)
namespace esphome
{
  namespace alarm_panel
  {

    extern Vista vista;
#if defined(ESPHOME_MQTT)
    extern std::function<void(const std::string &, JsonObject)> mqtt_callback;
    extern const char *setalarmcommandtopic;
#endif

#endif

    enum sysState
    {
      sarmedaway,
      sarmedstay,
      sarmednight,
      sdisarmed,
      striggered,
      sunavailable,
    };

    enum reqStates
    {
      rsidle,
      rsopenzones,
      rsbypasszones,
      rszonecount,
      rspartitionlist,
      rspartitionid,
      rszoneinfo,
      rsicode,
      rsdate,
    };

#if defined(ESPHOME_MQTT) && !defined(USE_API)
    class vistaECPHome : public time::RealTimeClock
    {
#elif defined(ARDUINO_MQTT)
class vistaECPHome
{
#elif defined(USE_API)
class vistaECPHome : public api::CustomAPIDevice, public time::RealTimeClock
{
#else
class vistaECPHome : public time::RealTimeClock
{
#endif

    public:
      vistaECPHome(char kpaddr = KP_ADDR, int receivePin = RX_PIN, int transmitPin = TX_PIN, int monitorTxPin = MONITOR_PIN, int maxzones = MAX_ZONES, int maxpartitions = MAX_PARTITIONS, bool invertrx = true, bool inverttx = true, bool invertmon = true, uint8_t inputrx = INPUT, uint8_t inputmon = INPUT);

      void set_accessCode(const char *ac) { accessCode = ac; }
      void set_rfSerialLookup(const char *rf) { rfSerialLookup = rf; }
      void set_quickArm(bool qa) { quickArm = qa; }
      void set_displaySystemMsg(bool dsm) { displaySystemMsg = dsm; }
      void set_lrrSupervisor(bool ls) { lrrSupervisor = ls; }
      void set_auiaddr(uint8_t addr) { auiAddr = addr; };
      void set_expanderAddr(uint8_t addr)
      {
        vista.addModule(addr);
      }
      void set_maxZones(int mz) { maxZones = mz; }
      void set_maxPartitions(uint8_t mp) { maxPartitions = mp; }
      void set_partitionKeypad(uint8_t idx, uint8_t addr)
      {
        if (idx && idx < 4)
          partitionKeypads[idx] = addr;
      }
      void set_defaultPartition(uint8_t dp) { defaultPartition = dp; }
      void set_debug(uint8_t db) { debug = db; }
      void set_ttl(uint32_t t) { TTL = t; };
      void set_text(uint8_t text_idx, const char *text)
      {
        switch (text_idx)
        {
        case 1:
          FAULT = text;
          break;
        case 2:
          BYPAS = text;
          break;
        case 3:
          ALARM = text;
          break;
        case 4:
          FIRE = text;
          break;
        case 5:
          CHECK = text;
          break;
        case 6:
          TRBL = text;
          break;
        case 7:
          HITSTAR = text;
          break;
        default:
          break;
        }
      }
#if !defined(ARDUINO_MQTT)
      std::vector<binary_sensor::BinarySensor *> bMap;
      std::vector<text_sensor::TextSensor *> tMap;
#endif

      void publishBinaryState(const std::string &cstr, uint8_t partition, bool open);
      void publishTextState(const std::string &cstr, uint8_t partition, std::string *text);

      bool displaySystemMsg = false;
      bool forceRefreshGlobal, forceRefreshZones, forceRefresh;

      sysState currentSystemState,
          previousSystemState;
      void stop();

    private:
      struct zoneType
      {
#if !defined(ARDUINO_MQTT)
        binary_sensor::BinarySensor *binarysensor;
        text_sensor::TextSensor *textsensor;
#endif
        uint16_t zone;
        unsigned long time;
        uint8_t partition : 7;
        uint8_t open : 1;
        uint8_t bypass : 1;
        uint8_t alarm : 1;
        uint8_t check : 1;
        uint8_t fire : 1;
        uint8_t panic : 1;
        uint8_t trouble : 1;
        uint8_t lowbat : 1;
        uint8_t active : 1;
        bool rflowbat ;
        bool external;
        uint32_t rfserial;
        uint8_t loopmask;

      };

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

      void publishZoneStatus(zoneType *zt, const char *open)
      {
        if (zt == NULL)
          return;
#if defined(ARDUINO_MQTT)
        publishTextState(SZONE, zt->zone, open);
#else
      std::string s = open;
      if (zt->textsensor != NULL && s != zt->textsensor->state)
        zt->textsensor->publish_state(s);
   #endif
      }

      void publishZoneStatus(zoneType *zt, bool open)
      {
        if (zt == NULL)
          return;
#if defined(ARDUINO_MQTT)
        publishBinaryState(SZONE, zt->zone, open);
#else
        if (zt->binarysensor != NULL && open != zt->binarysensor->state)
          zt->binarysensor->publish_state(open);
#endif
      }

      void publishStatus(const char *sensor, bool open, uint8_t partition = 0)
      {
        publishBinaryState(sensor, partition, open);
      }

      void publishSystemStatus(std::string statusCode, uint8_t partition)
      {
        publishTextState(SSYSTEMSTATE, partition, &statusCode);
      }

      void publishLrrMsg(std::string msg)
      {
        publishTextState(SLRR, 0, &msg);
      }
      void publishRfMsg(std::string msg)
      {
        publishTextState(SRF, 0, &msg);
      }
      void publishLine1(std::string msg, uint8_t partition)
      {
        publishTextState(SLINE1, partition, &msg);
      }
      void publishLine2(std::string msg, uint8_t partition)
      {
        publishTextState(SLINE2, partition, &msg);
      }
      void publishBeeps(std::string beeps, uint8_t partition)
      {
        publishTextState(SBEEP, partition, &beeps);
      }
      void publishZoneExtendedStatus(std::string msg)
      {
        publishTextState(SZONESTATUS, 0, &msg);
      }
      void publishRelayStatus(uint8_t addr, int channel, bool state)
      {
        std::string sensor = SRELAY + std::to_string(addr) + std::to_string(channel);
        publishBinaryState(sensor, 0, open);
      }
      uint8_t getLoopMask(uint8_t loop)
      {
        switch (loop)
        {
        case 1:
          return 0x80;
        case 2:
          return 0x20;
        case 3:
          return 0x10;
        case 4:
          return 0x40;
        default:
          return 0x80;
        }
      }

      int TTL = 30000;
      uint8_t debug = 0;
      char keypadAddr1 = 0;
      int rxPin = 0;
      int txPin = 0;
      int monitorPin = 0;
      int maxZones = 0;
      int maxPartitions = 0;
      bool invertRx;
      bool invertTx;
      bool invertMon;
      uint8_t inputRx = 0;
      uint8_t inputMon = 0;
      uint8_t auiAddr = 0;
      // bool activeAuiAddr=false;
      bool sendAuiTime();
      // bool sendAuiTime(int year, int month, int day, int hour, int minute,int seconds,int dow);
      char auiSeq = 8;
      void processAuiQueue();
      // int8_t dateReqStatus=0;

      struct auiCmdType
      {
        reqStates state = rsidle;
        unsigned long time = 0;
        uint8_t partition = 0;
        uint8_t records = 0;
        uint8_t record = 0;
        bool pending = false;
      };

      const char *accessCode;
      const char *rfSerialLookup;
      bool quickArm;

      bool lrrSupervisor, vh;
      char *partitionKeypads;
      int defaultPartition = DEFAULTPARTITION;
      char expanderAddr[9] = {};

      uint8_t *partitions;
      std::string topic_prefix, topic;

#if defined(AUTOPOPULATE)
      // struct zoneNameType
      // {
      //   std::string name;
      //   uint8_t zone;
      //   uint8_t zone_type;
      //   uint8_t device_type;
      // };
#endif

      zoneType zonetype_INIT = {
          .binarysensor = NULL,
          .textsensor = NULL,
          .zone = 0,
          .time = 0,
          .partition = 0,
          .open = 0,
          .bypass = 0,
          .alarm = 0,
          .check = 0,
          .fire = 0,
          .panic = 0,
          .trouble = 0,
          .lowbat = 0,
          .active = 0,
          .rfserial = 0,
          .loopmask = 0x80};

      struct
      {
        uint8_t bell : 1;
        uint8_t wrx1 : 1;
        uint8_t wrx2 : 1;
        uint8_t loop : 1;
        uint8_t duress : 1;
        uint8_t panic1 : 1;
        uint8_t panic2 : 1;
        uint8_t panic3 : 1;
      } otherSup;

      unsigned long lowBatteryTime;

      struct alarmStatusType
      {
        unsigned long time;
        bool state;
        uint16_t zone;
        char prompt[17];
      };

      struct lrrType
      {
        int code;
        uint8_t qual;
        uint16_t zone;
        uint8_t user;
      };

      struct lightStates
      {
        uint8_t away : 1;
        uint8_t stay : 1;
        uint8_t night : 1;
        uint8_t instant : 1;
        uint8_t bypass : 1;
        uint8_t ready : 1;
        uint8_t ac : 1;
        uint8_t chime : 1;
        uint8_t bat : 1;
        uint8_t alarm : 1;
        uint8_t check : 1;
        uint8_t fire : 1;
        uint8_t canceled : 1;
        uint8_t trouble : 1;
        uint8_t armed : 1;
      };

      lightStates currentLightState,
          previousLightState;
      enum lrrtype
      {
        user_t,
        zone_t
      };

      struct partitionStateType
      {
        sysState previousSystemState;
        lightStates previousLightState;
        int lastbeeps;
        bool refreshStatus;
        bool refreshLights;
      };

      void updateZoneState(zoneType *zt, int p, bool state, unsigned long t);
      char *parseAUIMessage(char *cmd);
      void processZoneList(char *list);
      void sendZoneRequest();
      void loadZones();
      void loadZone(int zone, std::string &&name, uint8_t zonetype, uint8_t devicetype);
      void getZoneCount();
      void getZoneRecord();
      void processZoneInfo(char *list);
      void getRFSerial(zoneType *zt);

    public:
      partitionStateType *partitionStates;

      void disconnectVista()
      {
        vista.stop();
      }
      bool connected()
      {
        vista.keybusConnected = vista.connected;
        return vista.connected;
      }

      void setExpFault(int zone, bool fault)
      {
        vista.setExpFault(zone, fault);
      }

#if !defined(ARDUINO_MQTT)
      void createZoneFromObj(binary_sensor::BinarySensor *obj, uint8_t p = 0, uint32_t rfSerial = 0, uint8_t loop = 0);
      void createZoneFromObj(text_sensor::TextSensor *obj, uint8_t p = 0, uint32_t rfSerial = 0, uint8_t loop = 0);
#endif

    private:
      std::string previousMsg,
          previousZoneStatusMsg;

      alarmStatusType fireStatus,
          panicStatus,
          alarmStatus;
      uint8_t partitionTargets;
      bool firstRun;

      struct serialType
      {
        uint16_t zone;
        int mask;
      };
      struct cmdQueueItem vistaCmd;
#ifdef ESP32
      TaskHandle_t xHandle;
      static void cmdQueueTask(void *args);
#endif
      void createZone(uint16_t z, uint8_t p = 0);
      int getZoneNumber(char *zid);

      auiCmdType auiCmd;
      std::vector<zoneType> extZones{};
      std::queue<auiCmdType> auiQueue{};

#if defined(AUTOPOPULATE)
      // std::vector<zoneNameType> autoZones{};
      // void fetchPanelZones();
#endif

      zoneType nz;

      zoneType *getZone(uint16_t z);
      std::string getZoneName(uint16_t zone, bool append = false);

      zoneType *getZoneFromRFSerial(uint32_t serialCode);

      void zoneStatusUpdate(zoneType *zt);
      void assignPartitionToZone(zoneType *zt);

#if defined(ESPHOME_MQTT)

      void on_json_message(const std::string &topic, JsonObject payload);
      static void on_json_message_callback(const std::string &topic, JsonObject payload);

#endif
    public:
#if defined(ARDUINO_MQTT)
      void begin()
      {
#else
  void setup() override;
#endif

        void set_panel_time();
        //  void set_panel_time_manual(int year, int month, int day, int hour, int minute, int second, int dow);
        void alarm_disarm(std::string code, int32_t partition);

        void alarm_arm_home(int32_t partition);

        void alarm_arm_night(int32_t partition);

        void alarm_arm_away(int32_t partition);

        void alarm_trigger_fire(std::string code, int32_t partition);

        void alarm_trigger_panic(std::string code, int32_t partition);

        void set_zone_fault(int32_t zone, bool fault);

        void set_keypad_address(int32_t addr)
        {
          // if (addr > 0 and addr < 24)
          ///  vista.setKpAddr(addr); //disabled for now
        }

        void alarm_keypress(std::string keystring);

        void alarm_keypress_partition(std::string keystring, int32_t partition);
        void send_cmd_bytes(int32_t addr, std::string hexbytes);
        void setDefaultKpAddr(uint8_t p);

      private:
        bool isInt(std::string s, int base);

        long int toInt(std::string s, int base);

        bool areEqual(char *a1, char *a2, uint8_t len);

        int getZoneFromPrompt(char *p1);
        std::string getNameFromPrompt(char *p1, char *p2);

        // bool promptContains(char * p1, const char * msg, int & zone);

        void printPacket(const char *label, char cbuf[], int len);

        //  std::string getF7Lookup(char cbuf[]) ;

        void updateDisplayLines(uint8_t partition);

      public:
        void set_alarm_state(std::string const &state, std::string code = "", int partition = DEFAULTPARTITION);

      private:
        int getZoneFromChannel(uint8_t deviceAddress, uint8_t channel);

        //   void translatePrompt(char * cbuf) ;

        void getPartitionsFromMask();

      public:
#if defined(ARDUINO_MQTT)
        void loop();
#else
  void update() override;
#endif

      private:
        const __FlashStringHelper *statusText(int statusCode);
      };

      extern vistaECPHome *alarmPanelPtr;
#if !defined(ARDUINO_MQTT)
    }
  } // namespaces
#endif
