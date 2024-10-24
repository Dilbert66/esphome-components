#pragma once

#if !defined(ARDUINO_MQTT)
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/time/real_time_clock.h"
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
//#include <regex>
#include "paneltext.h"
#include "Regexp.h"

// for documentation see project at https://github.com/Dilbert66/esphome-vistaecp

#define KP_ADDR 17 // only used as a default if not set in the yaml
#define MAX_ZONES 48
#define MAX_PARTITIONS 3
#define DEFAULTPARTITION 1

#define ASYNC_CORE 1
#define USETASK

// default pins to use for serial comms to the panel
// The pinouts below are only examples. You can choose any other gpio pin that is available and not needed for boot.
// These have proven to work fine.
#ifdef ESP32
// esp32 use gpio pins 4,13,16-39
#define RX_PIN 22
#define TX_PIN 21
#define MONITOR_PIN 18 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#else
#define RX_PIN 5
#define TX_PIN 4
#define MONITOR_PIN 14 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#endif



#if !defined(ARDUINO_MQTT)
namespace esphome
{
  namespace alarm_panel
  {

extern Vista * vista;
#if defined(ESPHOME_MQTT)
    extern std::function<void(const std::string &, JsonObject)> mqtt_callback;
    const char setalarmcommandtopic[] PROGMEM = "/alarm/set";
#endif

#endif

    enum sysState
    {
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
      sarmed,
      sarming,
      spending
    };

    enum reqStates
    {
      sidle,
      sopenzones,
      sbypasszones,
      szonelist,
      sdevicelist,
      spartitionlist,
      spartitionid,
      sicode

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

      std::function<void(int, std::string)> zoneStatusChangeCallback;
      std::function<void(int, bool)> zoneStatusChangeBinaryCallback;
      std::function<void(std::string, uint8_t)> systemStatusChangeCallback;
      std::function<void(sysState, bool, uint8_t)> statusChangeCallback;
      std::function<void(std::string, uint8_t)> systemMsgChangeCallback;
      std::function<void(std::string)> lrrMsgChangeCallback;
      std::function<void(std::string)> rfMsgChangeCallback;
      std::function<void(std::string, uint8_t)> line1DisplayCallback;
      std::function<void(std::string, uint8_t)> line2DisplayCallback;
      std::function<void(std::string, uint8_t)> beepsCallback;
      std::function<void(std::string)> zoneExtendedStatusCallback;
      std::function<void(uint8_t, int, bool)> relayStatusChangeCallback;


      void onZoneStatusChange(std::function<void(int zone,
                                                 std::string msg)>
                                  callback)
      {
        zoneStatusChangeCallback = callback;
      }
      void onZoneStatusChangeBinarySensor(std::function<void(int zone,
                                                             bool open)>
                                              callback)
      {
        zoneStatusChangeBinaryCallback = callback;
      }
      void onSystemStatusChange(std::function<void(std::string status, uint8_t partition)> callback)
      {
        systemStatusChangeCallback = callback;
      }
      void onStatusChange(std::function<void(sysState led, bool isOpen, uint8_t partition)> callback)
      {
        statusChangeCallback = callback;
      }
      void onSystemMsgChange(std::function<void(std::string msg, uint8_t partition)> callback)
      {
        systemMsgChangeCallback = callback;
      }
      void onLrrMsgChange(std::function<void(std::string msg)> callback)
      {
        lrrMsgChangeCallback = callback;
      }
      void onLine1DisplayChange(std::function<void(std::string msg, uint8_t partition)> callback)
      {
        line1DisplayCallback = callback;
      }
      void onLine2DisplayChange(std::function<void(std::string msg, uint8_t partition)> callback)
      {
        line2DisplayCallback = callback;
      }
      void onBeepsChange(std::function<void(std::string beeps, uint8_t partition)> callback)
      {
        beepsCallback = callback;
      }
      void onZoneExtendedStatusChange(std::function<void(std::string zoneExtendedStatus)> callback)
      {
        zoneExtendedStatusCallback = callback;
      }
      void onRelayStatusChange(std::function<void(uint8_t addr, int channel, bool state)> callback)
      {
        relayStatusChangeCallback = callback;
      }
      void onRfMsgChange(std::function<void(std::string msg)> callback)
      {
        rfMsgChangeCallback = callback;
      }

      void set_accessCode(const char *ac) { accessCode = ac; }
      void set_rfSerialLookup(const char *rf) { rfSerialLookup = rf; }
      void set_quickArm(bool qa) { quickArm = qa; }
      void set_displaySystemMsg(bool dsm) { displaySystemMsg = dsm; }
      void set_lrrSupervisor(bool ls) { lrrSupervisor = ls; }
      void set_auiaddr(uint8_t addr) { auiAddr = addr; };
      void set_expanderAddr(uint8_t idx, uint8_t addr)
      {
        if (idx && idx < 10)
          expanderAddr[idx - 1] = addr;
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

      std::vector<binary_sensor::BinarySensor *> bMap;
      std::vector<text_sensor::TextSensor *> tMap;
      
      void publishStatusChange(sysState led,bool open,uint8_t partition);
      void publishBinaryState(const std::string &cstr, uint8_t partition, bool open);
      void publishTextState(const std::string &cstr, uint8_t partition, std::string *text);

      bool displaySystemMsg = false;
      bool forceRefreshGlobal, forceRefreshZones, forceRefresh;

      sysState currentSystemState,
          previousSystemState;
      void stop();

    private:

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


      const char *accessCode;
      const char *rfSerialLookup;
      bool quickArm;

      bool lrrSupervisor, vh;
      char *partitionKeypads;
      int defaultPartition = DEFAULTPARTITION;
      char expanderAddr[9] = {};

      uint8_t *partitions;
      std::string topic_prefix, topic;

      struct zoneType
      {
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
      };
      zoneType zonetype_INIT = {
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
          .active = 0
          };

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

      void updateZoneState(zoneType *zt, int p, reqStates r, bool state, unsigned long t);
      char *parseAUIMessage(char *cmd, reqStates request);
      void processZoneList(uint8_t partition, reqStates request, char *list);
      void sendZoneRequest(uint8_t partition, reqStates request);
      void loadZones();


    public:
      partitionStateType *partitionStates;

      void disconnectVista()
      {
        vista->stop();
      }
      bool connected() {
        return vista->connected;
      }

      void setExpFault(int zone,bool fault) {
         vista->setExpFault(zone,fault);
      }



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


      std::vector<zoneType> extZones{};

      zoneType nz;

      zoneType *getZone(uint16_t z);
      zoneType *createZone(uint16_t z);

      serialType getRfSerialLookup(char *serialCode);

      void zoneStatusUpdate(zoneType *zt);
      void assignPartitionToZone(zoneType *zt);

#if defined(ESPHOME_MQTT)

      void on_json_message(const std::string &topic, JsonObject payload);
      static void on_json_message_callback(const std::string &topic, JsonObject payload);

#endif
    public:
      void set_panel_time();
      void set_panel_time_manual(int year, int month, int day, int hour, int minute);

#if defined(ARDUINO_MQTT)
      void begin()
      {
#else
  void setup() override;
#endif

        void alarm_disarm(std::string code, int partition);

        void alarm_arm_home(int partition);

        void alarm_arm_night(int partition);

        void alarm_arm_away(int partition);

        void alarm_trigger_fire(std::string code, int partition);

        void alarm_trigger_panic(std::string code, int partition);

        void set_zone_fault(int zone, bool fault);

        void set_keypad_address(int addr)
        {
          // if (addr > 0 and addr < 24)
          ///  vista.setKpAddr(addr); //disabled for now
        }

        void alarm_keypress(std::string keystring);

        void alarm_keypress_partition(std::string keystring, int partition);
        void send_cmd_bytes(int addr, std::string hexbytes);
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
