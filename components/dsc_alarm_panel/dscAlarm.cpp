// for project documentation visit https://github.com/Dilbert66/esphome-dsckeybus

#include "dscAlarm.h"
#if !defined(ARDUINO_MQTT)
#include "esphome/components/network/util.h"
#endif

#if defined(ESP32) 
#include <esp_chip_info.h>
#include <esp_task_wdt.h>
#endif

static const char *const TAG = "dscalarm";

dscKeybusInterface dsc(dscClockPinDefault, dscReadPinDefault, dscWritePinDefault);

bool forceDisconnect;

void disconnectKeybus()
{
  dsc.stop();
  dsc.keybusConnected = false;
  dsc.statusChanged = false;
  forceDisconnect = true;
}

#if !defined(ARDUINO_MQTT)
namespace esphome
{
  namespace alarm_panel
  {


#endif
    DSCkeybushome *alarmPanelPtr;
#if defined(ESPHOME_MQTT)
    std::function<void(const std::string &, JsonObject)> mqtt_callback;
#endif
    void DSCkeybushome::stop()
    {
      disconnectKeybus();
    }

#if defined(ARDUINO_MQTT)
    void DSCkeybushome::publishBinaryState(const std::string *idstr, uint8_t num, bool open)
    {
      if (binarySensorCallback != NULL)
        binarySensorCallback(idstr, num, open);
    }

    void DSCkeybushome::publishTextState(const std::string *idstr, uint8_t num, std::string *text)
    {
      if (textSensorCallback != NULL)
        textSensorCallback(idstr, num, text);
    }
#else
void DSCkeybushome::publishBinaryState(const std::string &idstr, uint8_t num, bool open)
{
  std::string id = idstr;
  if (num)
  {
    id += "_" + std::to_string(num);
  }
  auto bMap=App.get_binary_sensors();
  auto it = std::find_if(bMap.begin(), bMap.end(), [id](binary_sensor::BinarySensor *bs)
                         { return bs->get_object_id() == id; });

  if (it != bMap.end() && (*it)->state != open)
    (*it)->publish_state(open);
}

void DSCkeybushome::publishTextState(const std::string &idstr, uint8_t num, std::string *text)
{
  std::string id = idstr;
  if (num)
  {
    id += "_" + std::to_string(num);
  }
  auto tMap=App.get_text_sensors();
  auto it = std::find_if(tMap.begin(), tMap.end(), [id](text_sensor::TextSensor *ts)
                         { return ts->get_object_id() == id; });
  if (it != tMap.end() && (*it)->state != *text)
    (*it)->publish_state(*text);
}
#endif

    DSCkeybushome::DSCkeybushome(byte dscClockPin, byte dscReadPin, byte dscWritePin, bool invertWrite)
        : dscClockPin(dscClockPin),
          dscReadPin(dscReadPin),
          dscWritePin(dscWritePin),
          invertWrite(invertWrite)
    {

      alarmPanelPtr = this;

#if defined(USE_MQTT)
      mqtt_callback = on_json_message_callback;
#endif
    }

    std::string DSCkeybushome::getZoneName(int zone, bool append)
    {
#if !defined(ARDUINO_MQTT)
      std::string c = "z" + std::to_string(zone);
      auto bMap=App.get_binary_sensors();
      auto it = std::find_if(bMap.begin(), bMap.end(), [c](binary_sensor::BinarySensor *bs)
                             { return bs->get_object_id() == c; });
      if (it != bMap.end())
      {
        if (append)
          // return std::to_string(zone).append(" (").append((*it)->get_name()).append(")");
          return std::string((*it)->get_name()).append(" (").append(std::to_string(zone)).append(")");
        else
          return (*it)->get_name();
      }
#endif
      return "";
    }

    void DSCkeybushome::set_panel_time()
    {
#if defined(USE_TIME)
      ESPTime rtc = now();
      if (!rtc.is_valid())
        return;
      ESP_LOGI(TAG, "Setting panel time...");
      dsc.setDateTime(rtc.year, rtc.month, rtc.day_of_month, rtc.hour, rtc.minute);
#endif
    }

    void DSCkeybushome::set_panel_time_manual(int year, int month, int day, int hour, int minute)
    {
#if defined(ARDUINO_MQTT)
      Serial.printf("Setting panel time...\n");
#else
  ESP_LOGI(TAG, "Setting panel time...");
#endif
      dsc.setDateTime(year, month, day, hour, minute);
    }

#if defined(USE_MQTT)
    // void set_mqtt_id(mqtt::MQTTClientComponent *mqtt_id) { mqttId = mqtt_id; }
#endif
    void DSCkeybushome::set_accessCode(const char *ac) { accessCode = ac; }
    void DSCkeybushome::set_maxZones(int mz) { maxZones = mz; }
    void DSCkeybushome::set_userCodes(const char *uc) { userCodes = uc; }
    void DSCkeybushome::set_defaultPartition(uint8_t dp) { defaultPartition = dp; }
    void DSCkeybushome::set_debug(uint8_t db) { debug = db; }
    void DSCkeybushome::set_trouble_fetch(bool fetch) { troubleFetch = fetch; }
    void DSCkeybushome::set_trouble_fetch_cmd(const char *cmd) { fetchCmd = cmd; }
    void DSCkeybushome::set_expanderAddr(uint8_t addr)
    {
      #if not defined(DISABLE_EXPANDER)
      dsc.addModule(addr);
      #endif
    }
    void DSCkeybushome::set_refresh_time(uint8_t rt)
    {
      if (rt >= 60)
        refreshTimeSetting = rt * 1000; // convert seconds to ms
      if (rt == 0)
        troubleFetch = false;
    }

    DSCkeybushome::zoneType *DSCkeybushome::getZone(byte z,bool create)
    {
      // zone=0 to maxZones-1
      auto it = std::find_if(zoneStatus.begin(), zoneStatus.end(), [&z](zoneType &f)
                             { return f.zone == z + 1; });
      if (it != zoneStatus.end())
        return &(*it);
      else {
        return create?createZone(z+1):&zonetype_INIT;
      }

    }

#if defined(ARDUINO_MQTT)
  public:
    void DSCkeybushome::begin()
    {
#else
void DSCkeybushome::setup()
{
#endif
      eventStatusMsg.reserve(64);
      #if defined(ARDUINO_MQTT)
       if (debug > 2)
         Serial.begin(115200);
      #endif
#if !defined(ARDUINO_MQTT)
      loadZones();
#endif
#if defined(ESPHOME_MQTT)

      topic_prefix = mqtt::global_mqtt_client->get_topic_prefix();
      mqtt::MQTTDiscoveryInfo mqttDiscInfo = mqtt::global_mqtt_client->get_discovery_info();
      std::string discovery_prefix = mqttDiscInfo.prefix;
      topic = discovery_prefix + "/alarm_control_panel/" + topic_prefix + "/config";
      mqtt::global_mqtt_client->subscribe_json(topic_prefix + setalarmcommandtopic, mqtt_callback);
#endif
#if defined(USE_API)
 #if defined(USE_API_SERVICES)
      register_service(&DSCkeybushome::set_alarm_state, "set_alarm_state", {"state", "code", "partition"});
      register_service(&DSCkeybushome::alarm_disarm, "alarm_disarm", {"code"});
      #if defined(USE32)
#if !defined(ARDUINO_MQTT) && defined(USE_TIME)
      register_service(&DSCkeybushome::set_panel_time, "set_panel_time", {});
#else
      register_service(&DSCkeybushome::set_panel_time_manual, "set_panel_time_manual", {"year", "month", "day", "hour", "minute"});
#endif
#endif
      register_service(&DSCkeybushome::alarm_arm_home, "alarm_arm_home");
      register_service(&DSCkeybushome::alarm_arm_night, "alarm_arm_night", {"code"});
      register_service(&DSCkeybushome::alarm_arm_away, "alarm_arm_away");
      register_service(&DSCkeybushome::alarm_trigger_panic, "alarm_trigger_panic");
      register_service(&DSCkeybushome::alarm_trigger_fire, "alarm_trigger_fire");
      register_service(&DSCkeybushome::alarm_keypress, "alarm_keypress", {"keys"});
     register_service(&DSCkeybushome::alarm_keypress_partition, "alarm_keypress_partition", {"keys", "partition"});
      register_service(&DSCkeybushome::set_zone_fault, "set_zone_fault", {"zone", "fault"});
     // register_service(&DSCkeybushome::set_default_partition, "set_default_partition", {"partition"});
      #else
      #error "Missing "custom_services: true" line in the api: section"
      #endif
#endif

      publishSystemStatus(FC(STATUS_OFFLINE));
      forceDisconnect = false;
#ifdef MODULESUPERVISION
      dsc.enableModuleSupervision = 1;
#endif
#if not defined(DISABLE_EXPANDER)
      dsc.addModule(expanderAddr1);
      dsc.addModule(expanderAddr2);
#endif
      dsc.maxZones = maxZones;
      ESP_LOGD("test","ste4c");
      dsc.resetStatus();
      dsc.processModuleData = true;

#ifdef USE_ESP_IDF
      esp_chip_info_t info;
      esp_chip_info(&info);
      ESP_LOGE(TAG, "Cores: %d", info.cores);
      ESP_LOGE(TAG,"Running on core %d",xPortGetCoreID());
      uint8_t core = info.cores > 1 ? 1 : 0;
      TaskHandle_t setupHandle;
      xTaskCreatePinnedToCore(
      this->setupTask, // setup task
      "setupTask",     // Name of the task
        3000,               // Stack size in words
      (void *)this,       // Task input parameter
      10,                 // Priority of the task
      &setupHandle            // Task handle.
      ,
      core // Core where the task should run. If only one core, core will be ignored
  );

#else
      if (dscClockPin && dscReadPin )
        dsc.begin(dscClockPin, dscReadPin, dscWritePin, invertWrite);
      else
        dsc.begin();
#endif
      for (int p = 0; p < dscPartitions; p++)
      {

        partitionStatus[p]={0,0,NULL,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        publishBeeps("0", p + 1);
        publishPartitionMsg("No messages", p + 1);
        publishPartitionStatus("No messages", p + 1);
        publishLine1("ESP Module Start", p + 1);
        publishLine2(" ", p + 1);
        publishUserArmingDisarming(" ", p + 1);
        publishZoneAlarm(" ", p + 1);
      }
      for (int x = 0; x < dscZones; x++)
        programZones[x] = 0;

      system1 = 0;
      system0 = 0;
      publishTroubleMsgStatus("No messages");
      publishEventInfo("ESP module start");
      publishZoneMsgStatus("No messages");
    }

#ifdef USE_ESP_IDF
    void DSCkeybushome::setupTask(void *args)
    {
      //ensure we run vista setup on correct core.  This task is only setup to init the interrupts and pins, otherwise it's idle
      DSCkeybushome *_this = (DSCkeybushome *)args;
      if (_this->dscClockPin && _this->dscReadPin )
        dsc.begin(_this->dscClockPin, _this->dscReadPin, _this->dscWritePin, _this->invertWrite);
      else
        dsc.begin();
       unsigned long checkTime=_this->millis();
      for (;;)
      {
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          if (_this->millis() - checkTime > 300000)
          {
            checkTime = _this->millis();
#if not defined(ARDUINO_MQTT)
            UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGD(TAG, "High water stack level: %5d", (uint16_t)uxHighWaterMark);
#endif
          }
      }

    }
#endif

    std::string DSCkeybushome::getUserName(int usercode, bool append, bool returncode)
    {
      std::string name = "";
      std::string code = std::to_string(usercode);

      if (userCodes && *userCodes)
      {
        std::string s = userCodes;
        std::string token1, token2, token3;
        size_t pos, pos1;
        s.append(",");
        while ((pos = s.find(',')) != std::string::npos)
        {
          token1 = s.substr(0, pos); // store the substring
          pos1 = token1.find(':');
          token2 = token1.substr(0, pos1);
          token3 = token1.substr(pos1 + 1);
          if (token2 == code)
          {
            name = token3;
            if (append)
              return name.substr(0, 24).append(" (").append(code).append(")");
            else
              return name.substr(0, 24);
          }
          s.erase(0, pos + 1); /* erase() function store the current positon and move to next token. */
        }
      }
      if (returncode)
        return code;
      else
        return "";
    }

    void DSCkeybushome::set_default_partition(int32_t partition)
    {
      if (partition > 0 && partition < dscPartitions)
      {
        defaultPartition = partition;
        dsc.currentDefaultPartition = partition;
      }
    }

    void DSCkeybushome::set_zone_fault(int32_t zone, bool fault)
    {
#if not defined(DISABLE_EXPANDER)
#if !defined(ARDUINO_MQTT)
      ESP_LOGI(TAG, "Setting Zone Fault: %d,%d", zone, fault);
#else
      Serial.printf("Setting Zone Fault: %d,%d\n", zone, fault); 
#endif
      dsc.setZoneFault(zone, fault);

#endif
    }

    void DSCkeybushome::alarm_disarm(std::string code)
    {

      set_alarm_state("D", code, defaultPartition);
    }

    void DSCkeybushome::alarm_arm_home()
    {

      set_alarm_state("S", "", defaultPartition);
    }

    void DSCkeybushome::alarm_arm_night(std::string code)
    {

      set_alarm_state("N", code, defaultPartition);
    }

    void DSCkeybushome::alarm_arm_away()
    {

      set_alarm_state("W", "", defaultPartition);
    }

    void DSCkeybushome::alarm_trigger_fire()
    {

      set_alarm_state("F", "", defaultPartition);
    }

    void DSCkeybushome::alarm_trigger_panic()
    {

      set_alarm_state("P", "", defaultPartition);
    }

    void DSCkeybushome::processMenu(byte key, byte partition)
    {

      if (partition < 1)
        partition = defaultPartition;
      byte *currentSelection = &partitionStatus[partition - 1].currentSelection;
      byte *selectedZone = &partitionStatus[partition - 1].selectedZone;

      if (partitionStatus[partition - 1].locked)
      {
        publishLine1("System", partition);
        publishLine2("not available", partition);
        return;
      }
      /*
      if (partitionStatus[partition - 1].sectionDigits > 0) { //program mode data input
        if (key > 47 && key < 58) {
            decimalInputBuffer[partitionStatus[partition - 1].editIdx] = key;
            partitionStatus[partition - 1].editIdx = partitionStatus[partition - 1].editIdx+1
        }
       dsc.write(key, partition);
       return;
      }
      */
      if (partitionStatus[partition - 1].digits > 0)
      { // program mode data input
        std::string tpl =FC("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
        if (dsc.status[partition - 1] == 0xAA)
        { // time entry
          tpl = FC("XXXX    XXXXXX  XXXXXXXXXXXXXXXX");
        }
        if (dsc.status[partition - 1] == 0xAB)
        { // time entry
          tpl = FC("XXXX                            ");
        }

        if (key == '#')
        {
          partitionStatus[partition - 1].newData = false;
          if ( partitionStatus[partition - 1].hex && partitionStatus[partition - 1].digits > 2)
          {
            dsc.setLCDSend(partition, (partitionStatus[partition - 1].digits > 8));
          }
          else
          {
            dsc.write(key, partition);
          }
          return;
        }
        else if (key == '>')
        {
          getNextIdx(tpl.c_str(), partition);
          if (partitionStatus[partition - 1].editIdx == 0 && (!partitionStatus[partition - 1].hex || partitionStatus[partition - 1].digits < 3))
          {
            dsc.setLCDSend(partition);
            partitionStatus[partition - 1].newData = false;
            return;
          }
        }
        else if (key == '<')
        {
          getPrevIdx(tpl.c_str(), partition);
        }
        else if (key != '*')
        {
          if (partitionStatus[partition - 1].decimalInput)
          {
            int num;
            decimalInputBuffer[partitionStatus[partition - 1].editIdx] = key;
            sscanf(decimalInputBuffer, "%d", &num);
            dsc.pgmBuffer.data[0] = num;
            // convert data to int and store it to data[0];
          }
          else
          {
            // skip every 5th byte since it's a checksum
            byte y = (partitionStatus[partition - 1].editIdx / 2) + (partitionStatus[partition - 1].editIdx / 8);
            char k = key - 48;
            if (partitionStatus[partition - 1].hexMode)
            {
              if (k < 6)
                k += 10;
              else
                return;
            }

            if (partitionStatus[partition - 1].editIdx % 2)
              dsc.pgmBuffer.data[y] = (dsc.pgmBuffer.data[y] & 0xF0) | k;
            else
              dsc.pgmBuffer.data[y] = (dsc.pgmBuffer.data[y] & 0x0F) | (k << 4);
          }
          getNextIdx(tpl.c_str(), partition);
          if (partitionStatus[partition - 1].editIdx == 0 && partitionStatus[partition - 1].digits < 32)
          {
            dsc.setLCDSend(partition);
            partitionStatus[partition - 1].newData = false;
          
            return;
          }
        }
        else if (key == '*')
        {
          if (partitionStatus[partition - 1].hex)
          {
            partitionStatus[partition - 1].hexMode = partitionStatus[partition - 1].hexMode ? false : true;
          }
          else
          {
            dsc.write(key, partition);
            return;
          }
        }
        dsc.write("$", partition); // place holder character .
        setStatus(partition - 1, true);
        return;
      }

      if (key == '#')
      {
        *currentSelection = 0xFF;
        *selectedZone = 0;
        partitionStatus[partition - 1].eventViewer = false;
        activePartition = defaultPartition;
        dsc.write(key, partition);
        setStatus(partition - 1, true);
        return;
      }

      if (dsc.status[partition - 1] < 0x04)
      {
        if (dsc.keybusVersion1)
        { // older versions don't support top level navigation
          if (key == '<' || key == '>')
            return;
        }
        else
        {

          if (key == '<')
          {
            getPreviousMainOption(partition);
          }
          else if (key == '>')
          {
            getNextMainOption(partition);
          };
        }
        dsc.write(key, partition);
        setStatus(partition - 1, true);
      }
      else if (dsc.status[partition - 1] == 0x9E)
      { // * mainmenu
        if (key == '<')
        {
          *currentSelection = *currentSelection >= mmsize ? mmsize - 1 : (*currentSelection > 0 ? *currentSelection - 1 : mmsize - 1);
          if (*currentSelection == 8)
            *currentSelection -= 1; // skip empty item
          if (*currentSelection < mmsize)
            publishLine2(FC(mainMenu[*currentSelection]), partition);
        }
        else if (key == '>')
        {
          *currentSelection = *currentSelection >= (mmsize - 1) ? 0 : *currentSelection + 1;
          if (*currentSelection == 8)
            *currentSelection += 1;
          if (*currentSelection < mmsize)
            publishLine2(FC(mainMenu[*currentSelection]), partition);
        }
        else if (key == '*' && *currentSelection > 0)
        {
          dsc.write(key, partition);
          char s[5];
          sprintf(s, "%d", *currentSelection % (mmsize - 1));
          // const char * out = strcpy(new char[3], s);
          *currentSelection = 0;
          dsc.write(s, partition);
        }
        else
        {
          dsc.write(key, partition);
          *currentSelection = 0;
        }
      }
      else if (dsc.status[partition - 1] == 0xA1)
      { // trouble
        if (key == '*' && *currentSelection > 0)
        {
          char s[5];
          sprintf(s, "%d", *currentSelection);
          // const char * out = strcpy(new char[3], s);
          *currentSelection = 0;
          dsc.write(s, partition);
        }
        else if (key == '>')
        {
          *currentSelection = getNextOption(*currentSelection);
          if (*currentSelection < tmsize)
            publishLine2(FC(troubleMenu[*currentSelection]), partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousOption(*currentSelection);
          if (*currentSelection < tmsize)
            publishLine2(FC(troubleMenu[*currentSelection]), partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
      }
      else if (dsc.status[partition - 1] == 0xC8)
      { // service
        if (key == '*' && *currentSelection > 0)
        {
          char s[5];
          sprintf(s, "%d", *currentSelection);
          // const char * out = strcpy(new char[3], s);
          *currentSelection = 0;
          dsc.write(s, partition);
        }
        else if (key == '>')
        {
          *currentSelection = getNextOption(*currentSelection);
          if (*currentSelection < smsize)
            publishLine2(FC(serviceMenu[*currentSelection]), partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousOption(*currentSelection);
          if (*currentSelection < smsize)
            publishLine2(FC(serviceMenu[*currentSelection]), partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
      }
      else if (dsc.status[partition - 1] == 0xA9 && !partitionStatus[partition - 1].eventViewer)
      { // * user functions
        if (key == '<')
        {
          *currentSelection = *currentSelection >= umsize ? (umsize - 1) : (*currentSelection > 0 ? *currentSelection - 1 : (umsize - 1));
          if (*currentSelection < umsize)
            publishLine2(FC(userMenu[*currentSelection]), partition);
        }
        else if (key == '>')
        {
          *currentSelection = *currentSelection >= (umsize - 1) ? 0 : *currentSelection + 1;
          if (*currentSelection < umsize)
            publishLine2(FC(userMenu[*currentSelection]), partition);
        }
        else if (key == '*' && *currentSelection > 0)
        {
          char s[5];
          if (*currentSelection == 6)
          { // event viewer.
            partitionStatus[partition - 1].eventViewer = true;
            activePartition = partition;
            dsc.write('b', partition);
          }
          else
          {
            sprintf(s, "%d", *currentSelection % (umsize - 1));
            // const char * out = strcpy(new char[3], s);
            *currentSelection = 0;
            dsc.write(s, partition);
          }
        }
        else
        {
          dsc.write(key, partition);
          *currentSelection = 0;
        }
      }
      else if (dsc.status[partition - 1] == 0xA2)
      { // alarm memory
        if (key == '>')
        {
          *currentSelection = getNextOption(*currentSelection);
          dsc.write(key, partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousOption(*currentSelection);
          dsc.write(key, partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
        setStatus(partition - 1, true);
      }
      else if (dsc.status[partition - 1] == 0xBA)
      { // low battery zones
        if (key == '>')
        {
          *currentSelection = getNextOption(*currentSelection);
          dsc.write(key, partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousOption(*currentSelection);
          dsc.write(key, partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
        setStatus(partition - 1, true);
      }
      else if (dsc.status[partition - 1] == 0xA6)
      { // user codes
        if (key == '*')
        {
          char s[5];
          sprintf(s, "%02d", *currentSelection);
          // const char * out = strcpy(new char[3], s);
          dsc.write(s, partition);
        }
        else if (key == '>')
        {
          *currentSelection = getNextUserCode(*currentSelection);
          dsc.write(key, partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousUserCode(*currentSelection);
          dsc.write(key, partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
        setStatus(partition - 1, true);
      }
      else if (dsc.status[partition - 1] == 0x11)
      { // alarms
        if (key == '>')
        {
          *currentSelection = getNextAlarmedZone(*currentSelection, partition);
          dsc.write(key, partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousAlarmedZone(*currentSelection, partition);
          dsc.write(key, partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
        setStatus(partition - 1, true);
      }
      else if (dsc.status[partition - 1] == 0xA0)
      { // bypass
        if (key == '*')
        {
          char s[5];
          sprintf(s, "%02d", *currentSelection);
          // const char * out = strcpy(new char[3], s);
          dsc.write(s, partition);
        }
        else if (key == '>')
        {
          *currentSelection = getNextEnabledZone(*currentSelection, partition);
          dsc.write(key, partition);
        }
        else if (key == '<')
        {
          *currentSelection = getPreviousEnabledZone(*currentSelection, partition);
          dsc.write(key, partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
        setStatus(partition - 1, true);
      }
      else if (dsc.status[partition - 1] == 0xB2)
      { // output control
        if (key == '<')
        {
          *currentSelection = *currentSelection >= omsize ? (omsize - 1) : (*currentSelection > 0 ? *currentSelection - 1 : (omsize - 1));
          if (*currentSelection < omsize)
            publishLine2(FC(outputMenu[*currentSelection]), partition);
          dsc.write(key, partition);
        }
        else if (key == '>')
        {
          *currentSelection = *currentSelection >= (omsize - 1) ? 0 : *currentSelection + 1;
          if (*currentSelection < omsize)
            publishLine2(FC(outputMenu[*currentSelection]), partition);
          dsc.write(key, partition);
        }
        else if (key == '*' && *currentSelection > 0)
        {
          char s[5];
          sprintf(s, "%d", *currentSelection);
          // const char * out = strcpy(new char[3], s);
          *currentSelection = 0;
          dsc.write(s, partition);
        }
        else
        {
          *currentSelection = 0;
          dsc.write(key, partition);
        }
        setStatus(partition - 1, true);
      }
      else
      {
        dsc.write(key, partition);
        setStatus(partition - 1, false);
      }
    }

    void DSCkeybushome::getPrevIdx(const char *tpl, byte partition)
    {
      int count = 0;
      do
      {
        partitionStatus[partition - 1].editIdx = partitionStatus[partition - 1].editIdx > 0 ? partitionStatus[partition - 1].editIdx - 1 : partitionStatus[partition - 1].digits - 1;
        count++;
      } while (tpl[partitionStatus[partition - 1].editIdx] != 'X' && count <= partitionStatus[partition - 1].digits);
    }

    void DSCkeybushome::getNextIdx(const char *tpl, byte partition)
    {
      int count = 0;
      do
      {
        partitionStatus[partition - 1].editIdx = partitionStatus[partition - 1].editIdx + 1 < partitionStatus[partition - 1].digits ? partitionStatus[partition - 1].editIdx + 1 : 0;
        count++;
      } while (tpl[partitionStatus[partition - 1].editIdx] != 'X' && count <= partitionStatus[partition - 1].digits);
    }

    void DSCkeybushome::alarm_keypress(std::string keystring)
    {
      alarm_keypress_partition(keystring, defaultPartition);
    }

    void DSCkeybushome::alarm_keypress_partition(std::string keystring, int32_t partition)
    {
      if (!partition)
        partition = defaultPartition;
      if (keystring == "R")
      {
        forceRefresh = true;
        return;
      }
#if !defined(ARDUINO_MQTT)
      if (debug > 0)
        ESP_LOGI(TAG, "Writing keys: %s to partition %d, partition disabled: %d , partition locked: %d", keystring.c_str(), partition, dsc.disabled[partition - 1], partitionStatus[partition - 1].locked);
#else
  if (debug > 0)
    Serial.printf("Writing keys: %s to partition %d, partition disabled: %d , partition locked: %d\n", keystring.c_str(), partition, dsc.disabled[partition - 1], partitionStatus[partition - 1].locked); 
#endif
     // if (dsc.disabled[partition - 1])
      //  return;
      partitionStatus[partition - 1].keyPressTime = millis();
      if (keystring.length() == 1)
      {
        processMenu(keystring.c_str()[0], partition);
      }
      else
      {
        if (!partitionStatus[partition - 1].locked)
          dsc.write(keystring.c_str(), partition);
        setStatus(partition - 1, true);
      }
    }

#if defined(ESPHOME_MQTT)
    void DSCkeybushome::on_json_message_callback(const std::string &topic, JsonObject payload)
    {
      alarmPanelPtr->on_json_message(topic, payload);
    }

    void DSCkeybushome::on_json_message(const std::string &topic, JsonObject payload)
    {
      int p = 0;
      if (topic.find(setalarmcommandtopic) != std::string::npos)
      {
        if (payload.containsKey("partition"))
          p = payload["partition"];

        if (payload.containsKey("state"))
        {
          const char *c = "";
          if (payload.containsKey("code"))
            c = payload["code"];
          std::string code = c;
          std::string s = payload["state"];
          set_alarm_state(s, code, p);
        }
        else if (payload.containsKey("keys"))
        {
          std::string s = payload["keys"];
          alarm_keypress_partition(s, p);
        }
        else if (payload.containsKey("fault") && payload.containsKey("zone"))
        {
          bool b = false;
          std::string s1 = payload["fault"];
          if (s1 == "ON" || s1 == "on" || s1 == "1")
            b = true;
          p = payload["zone"];
          // ESP_LOGI(TAG,"set zone fault %s,%s,%d,%d",s2.c_str(),c,b,p);
          set_zone_fault(p, b);
        }
      }
    }

#endif

    bool DSCkeybushome::isInt(std::string s, int base)
    {
      if (s.empty() || std::isspace(s[0]))
        return false;
      char *p;
      strtol(s.c_str(), &p, base);
      return (*p == 0);
    }

    void DSCkeybushome::set_alarm_state(std::string state, std::string code, int32_t partition)
    {

      if (code.length() != 4 || !isInt(code, 10))
        code = ""; // ensure we get a numeric 4 digit code
      // const char * alarmCode = strcpy(new char[code.length() + 1], code.c_str());
      if (!partition)
        partition = defaultPartition;

#if !defined(ARDUINO_MQTT)
      ESP_LOGI(TAG, "Setting Alarm state: %s to partition %d", state.c_str(), partition);
#else
  Serial.printf("Setting Alarm state: %s to partition %d\n", state.c_str(), partition);
#endif
      if (partitionStatus[partition - 1].locked)
        return;

      // Arm stay
      if (state.compare("S") == 0 && !dsc.armed[partition - 1] && !dsc.exitDelay[partition - 1])
      {
#if !defined(ARDUINO_MQTT)
        if (debug > 1)
          ESP_LOGI(TAG, "Arming stay");
#endif
        dsc.write('s', partition); // Virtual keypad arm stay
      }
      // Arm away
      else if ((state.compare("A") == 0 || state.compare("W") == 0) && !dsc.armed[partition - 1] && !dsc.exitDelay[partition - 1])
      {
#if !defined(ARDUINO_MQTT)
        if (debug > 1)
          ESP_LOGI(TAG, "Arming away");
#endif
        dsc.write('w', partition); // Virtual keypad arm away
      }
      // Arm night  ** this depends on the accessCode setup in the yaml
      else if (state.compare("N") == 0 && !dsc.armed[partition - 1] && !dsc.exitDelay[partition - 1])
      {
#if !defined(ARDUINO_MQTT)
        if (debug > 1)
          ESP_LOGI(TAG, "Arming night");
#endif
        dsc.write('n', partition); // Virtual keypad arm away
      }
      // Fire command
      else if (state.compare("F") == 0)
      {
        dsc.write('f', partition); // Virtual keypad arm away
      }
      // Panic command
      else if (state.compare("P") == 0)
      {
        dsc.write('p', partition); // Virtual keypad arm away
      }
      // Disarm
      else if (state.compare("D") == 0 && (dsc.armed[partition - 1] || dsc.exitDelay[partition - 1]))
      {
        if (code.length() == 4)
        { // ensure we get 4 digit code
#if !defined(ARDUINO_MQTT)
          if (debug > 1)
            ESP_LOGI(TAG, "Disarming ... ");
#endif
          dsc.write(code.c_str(), partition);
        }
      }
      partitionStatus[partition - 1].keyPressTime = millis();
    }

    void DSCkeybushome::printPacket(const char *label, char cmd, volatile byte cbuf[], int len)
    {

      char s1[4];

      std::string s = "";

#if !defined(ARDUINO_MQTT)
      char s2[25];
      ESPTime rtc = now();
      sprintf(s2, "[%02d-%02d-%02d %02d:%02d]", rtc.year, rtc.month, rtc.day_of_month, rtc.hour, rtc.minute);
#endif
      for (int c = 0; c < len; c++)
      {
        sprintf(s1, "%02X ", cbuf[c]);
        s = s.append(s1);
      }
#if defined(ARDUINO_MQTT)
      Serial.printf("%s: %02X: %s(%d)\n", label, cmd, s.c_str(), dsc.panelBitCount);
#else
  ESP_LOGI(label, "%s %02X: %s(%d)", s2, cmd, s.c_str(), dsc.panelBitCount);
#endif
    }

    byte DSCkeybushome::getPartitionE6(byte partitionByte)
    {

      for (byte bit = 0; bit <= 7; bit++)
      {
        if (bitRead(dsc.panelData[partitionByte], bit))
        {
          return bit + 1;
        }
  
      }
      return defaultPartition;
    }

    bool DSCkeybushome::getEnabledZonesB1(byte inputByte, byte startZone, byte partition)
    {
      bool zonesEnabled = false;
      byte zone;
      for (byte panelByte = inputByte; panelByte <= inputByte + 3; panelByte++)
      {
        if (dsc.panelData[panelByte] != 0)
        {
          zonesEnabled = true;
          for (byte zoneBit = 0; zoneBit < 8; zoneBit++)
          {
            zone = (zoneBit + startZone) + ((panelByte - inputByte) * 8) - 1;
            if (zone >= maxZones)
              continue;
            if (bitRead(dsc.panelData[panelByte], zoneBit))
            {
              getZone(zone,true)->partition = partition;
              getZone(zone)->enabled = true;
             // if (debug > 1) ESP_LOGD(TAG,"B1: Enabled zone %d on partition %d",zone+1,partition);
            }
            else if (getZone(zone)->partition == partition)
            {
              getZone(zone)->enabled = false;
              if (debug > 2) ESP_LOGD(TAG,"B1: Disabled zone %d on partition %d",zone+1,partition);
            }
          }
        }
      }
      return zonesEnabled;
    }

    bool DSCkeybushome::getEnabledZonesE6(byte inputByte, byte startZone, byte partitionByte)
    {
      bool zonesEnabled = false;
      byte zone;

      byte partition = getPartitionE6(partitionByte) ;
      for (byte panelByte = inputByte; panelByte <= inputByte + 3; panelByte++)
      {
        if (dsc.panelData[panelByte] != 0)
        {
          zonesEnabled = true;
          for (byte zoneBit = 0; zoneBit < 8; zoneBit++)
          {
            zone = (zoneBit + startZone) + ((panelByte - inputByte) * 8) - 1;
            if (zone >= maxZones)
              continue;
            if (bitRead(dsc.panelData[panelByte], zoneBit))
            {
              getZone(zone,true)->partition = partition;
              getZone(zone)->enabled = true;
              if (debug > 2) ESP_LOGD(TAG,"E6: Enabled zone %d on partition %d",zone+1,partition);
            }
            else if (getZone(zone)->partition == partition)
            {
              getZone(zone)->enabled = false;
              if (debug > 2) ESP_LOGD(TAG,"E6: Disabled zone %d on partition %d",zone+1,partition);
            }
          }
        }

      }
      return zonesEnabled;
    }

    std::string DSCkeybushome::getOptionsString()
    {

      char s1[4];
      std::string options;
      options = " ";
      byte option;
      for (byte optionGroup = 0; optionGroup < dscZones; optionGroup++)
      {
        for (byte optionBit = 0; optionBit < 8; optionBit++)
        {
          option = optionBit + 1 + (optionGroup * 8);
          if (bitRead(programZones[optionGroup], optionBit))
          {
            sprintf(s1, "%d", option);
            options.append(s1);
          }
        }
      }
      return options.c_str();
    }

    bool DSCkeybushome::checkUserCode(byte code)
    {
      byte option, optionGroup;
      for (optionGroup = 0; optionGroup < dscZones; optionGroup++)
      {
        for (byte optionBit = 0; optionBit < 8; optionBit++)
        {
          option = optionBit + 1 + (optionGroup * 8);
          if (bitRead(programZones[optionGroup], optionBit) && option == code)
          {
            return true;
          }
        }
      }
      return false;
    }

    byte DSCkeybushome::getNextOption(byte start)
    {
      byte option, optionGroup, optionBit;
      byte s = start >= maxZones ? 0 : start;
      for (optionGroup = 0; optionGroup < dscZones; optionGroup++)
      {
        for (optionBit = 0; optionBit < 8; optionBit++)
        {
          option = optionBit + 1 + (optionGroup * 8);
          if (bitRead(programZones[optionGroup], optionBit) && option > s)
          {
            // ESP_LOGD("test","returning %d",option);
            return option;
          }
        }
      }
      if (start > 0)
        return getNextOption(0);
      else
        return 0;
    }

    byte DSCkeybushome::getPreviousOption(byte start)
    {
      byte option, optionGroup, optionBit;
      byte s = start > 0 ? start : maxZones;

      for (optionGroup = dscZones - 1; optionGroup >= 0 && optionGroup < dscZones; optionGroup--)
      {
        for (optionBit = 7; optionBit >= 0 && optionBit < 8; optionBit--)
        {
          option = optionBit + 1 + (optionGroup * 8);
          if (bitRead(programZones[optionGroup], optionBit) && option < s)
          {
            // ESP_LOGD("test","returning %d",option);
            return option;
          }
        }
      }
      if (start > 0)
        return getPreviousOption(0);
      else
        return 0;
    }

    byte DSCkeybushome::getNextUserCode(byte start)
    {
      if (start < 95)
        return start + 1;
      else
        return 1;
    }

    byte DSCkeybushome::getPreviousUserCode(byte start)
    {
      if (start > 1)
        return (byte)start - 1;
      else
        return 95;
    }

    void DSCkeybushome::getPreviousMainOption(byte partition)
    {

      byte menu = 0;
      byte zone = 0;

      if (partitionStatus[partition - 1].currentSelection > 5)
      {
        partitionStatus[partition - 1].currentSelection = 0;
        partitionStatus[partition - 1].selectedZone = 0;
        return;
      }
      else if (partitionStatus[partition - 1].currentSelection == 5)
      { // open zones
        partitionStatus[partition - 1].selectedZone = getPreviousOpenZone(partitionStatus[partition - 1].selectedZone, partition);
        if (partitionStatus[partition - 1].selectedZone)
          return;
      }
      else if (partitionStatus[partition - 1].currentSelection < 2)
        partitionStatus[partition - 1].currentSelection = 6;

      for (int x = partitionStatus[partition - 1].currentSelection; x >= 0; x--)
      {

        if ((x == 6))
        { // openzones
          menu = 5;
          zone = 0;
          break;
        }
        else if (x == 3 && dsc.lights[partition - 1] & 0x10)
        { // trouble
          menu = 2;
          zone = 0;
          break;
        }
        else if (x == 4 && dsc.lights[partition - 1] & 0x08)
        { // bypass
          menu = 3;
          zone = 0;
          break;
        }
        else if (x == 5 && getNextAlarmedZone(0, partition))
        { // alarms
          menu = 4;
          zone = 0;
          break;
        }
      }
      partitionStatus[partition - 1].currentSelection = menu;
      partitionStatus[partition - 1].selectedZone = zone;
    }

    void DSCkeybushome::clearZoneAlarms(byte partition)
    {
      for (int zone = 0; zone < maxZones; zone++)
      {
        uint8_t p = getZone(zone)->partition;
        if (p == partition || p == 0)
        {
          getZone(zone)->alarm = false;
        }
      }
      publishZoneAlarm(" ", partition);
    }

    void DSCkeybushome::clearZoneBypass(byte partition)
    {
      for (int zone = 0; zone < maxZones; zone++)
      {
        uint8_t p = getZone(zone)->partition;
        if (p == partition || p == 0)
          getZone(zone)->bypassed = false;
      }
    }

    byte DSCkeybushome::getNextOpenZone(byte start, byte partition)
    {
      if (start >= maxZones)
        start = 0;
      for (int zone = start; zone < maxZones; zone++)
      {
        if (getZone(zone)->enabled && getZone(zone)->partition == partition && getZone(zone)->open)
        {
          return (byte)zone + 1;
        }
      }
      return 0;
    }

    byte DSCkeybushome::getPreviousOpenZone(byte start, byte partition)
    {
      if (start == 1)
        return 0;
      if (start == 0 || start > maxZones)
        start = maxZones;
      for (int zone = start - 2; zone >= 0; zone--)
      {
        if (getZone(zone)->enabled && getZone(zone)->partition == partition && getZone(zone)->open)
        {
          return (byte)zone + 1;
        }
      }
      return 0;
    }

    void DSCkeybushome::getNextMainOption(byte partition)
    {

      byte menu = 0;
      byte zone = 0;

      if (partitionStatus[partition - 1].currentSelection > 5)
      {
        partitionStatus[partition - 1].currentSelection = 0;
        return;
      }
      else if (partitionStatus[partition - 1].currentSelection == 5)
      { // open zones
        partitionStatus[partition - 1].selectedZone = getNextOpenZone(partitionStatus[partition - 1].selectedZone, partition);
        if (partitionStatus[partition - 1].selectedZone)
          return;
      }

      for (int x = partitionStatus[partition - 1].currentSelection; x < 6; x++)
      {
        if ((x == 0 || x == 1) && dsc.lights[partition - 1] & 0x10)
        { // trouble
          menu = 2;
          zone = 0;
          break;
        }
        else if (x == 2 && dsc.lights[partition - 1] & 0x08)
        { // bypass
          menu = 3;
          zone = 0;
          break;
        }
        else if (x == 3 && getNextAlarmedZone(0, partition))
        { // alarm
          menu = 4;
          zone = 0;
          break;
        }
        else if (x == 4)
        { // open
          menu = 5;
          zone = 0;
          break;
        }
      }
      partitionStatus[partition - 1].currentSelection = menu;
      partitionStatus[partition - 1].selectedZone = zone;
    }

    byte DSCkeybushome::getNextEnabledZone(byte start, byte partition)
    {
      if (start >= maxZones)
        start = 0;
      for (int zone = start; zone < maxZones; zone++)
      {
        if (getZone(zone)->partition == partition && getZone(zone)->enabled)
        {
          return (byte)zone + 1;
        }
      }
      return 0;
    }

    byte DSCkeybushome::getPreviousEnabledZone(byte start, byte partition)
    {

      if (start < 2 || start > maxZones)
        start = maxZones;
      int zone;
      for (zone = start - 2; zone >= 0; zone--)
      {
        if (getZone(zone)->partition == partition && getZone(zone)->enabled)
        {
          return (byte)zone + 1;
        }
      }
      if (zone < 0)
        start = maxZones;
      for (zone = start - 2; zone >= 0; zone--)
      {
        if (getZone(zone)->partition == partition && getZone(zone)->enabled)
        {
          return (byte)zone + 1;
        }
      }

      return 0;
    }

    byte DSCkeybushome::getNextAlarmedZone(byte start, byte partition)
    {
      if (start >= maxZones)
        start = 0;
      for (int zone = start; zone < maxZones; zone++)
      {
        if (getZone(zone)->partition == partition && getZone(zone)->alarm)
        {
          return (byte)zone + 1;
        }
      }
      return 0;
    }

    byte DSCkeybushome::getPreviousAlarmedZone(byte start, byte partition)
    {
      if (start < 2 || start > maxZones)
        start = maxZones;
      int zone;
      for (zone = start - 2; zone >= 0; zone--)
      {
        if (getZone(zone)->partition == partition && getZone(zone)->alarm)
        {
          return (byte)zone + 1;
        }
      }
      if (zone < 0)
        start = maxZones;
      for (zone = start - 2; zone >= 0 && zone < maxZones; zone--)
      {
        if (getZone(zone)->partition == partition && getZone(zone)->alarm)
        {
          return (byte)zone + 1;
        }
      }
      return 0;
    }
    void DSCkeybushome::getBypassZones(byte partition)
    {

      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++)
      {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++)
        {
          zone = zoneBit + (zoneGroup * 8);
          if (!(getZone(zone)->partition == partition && getZone(zone)->enabled) || zone >= maxZones)
            continue;
          if (bitRead(programZones[zoneGroup], zoneBit))
          {
            getZone(zone)->bypassed = true;
          }
          else
          {
            getZone(zone)->bypassed = false;
          }
        }
      }
    }

    // bool DSCkeybushome::check051bCmd()
    // {

    //   cmdCountType *cmdCount;
    //   static cmdCountType count05;
    //   static cmdCountType count1b;


    //   switch (dsc.panelData[0])
    //   {
    //   case 0x05:
    //     cmdCount = &count05;
    //     break;
    //   case 0x1b:
    //     cmdCount = &count1b;
    //     break;
    //   default:
    //     return true;
    //   }

    //   if (cmdCount->bcount > 0 && cmdCount->bcount != dsc.panelBitCount)
    //   {
    //     dsc.panelData[11] = 0x77;
    //     cmdCount->ecount = cmdCount->ecount + 1;
    //     if (cmdCount->ecount > 1)
    //     {
    //       cmdCount->ccount = 0; // get a new updated bit count since we have more than 1 error count in a row
    //       cmdCount->bcount = 0;
    //       cmdCount->lbcount = 0;
    //       cmdCount->ecount = 0;
    //     }
    //     return false;
    //   }
    //   else
    //   {
    //     cmdCount->ecount = 0; // matching bit count reset error count
    //     if (cmdCount->ccount < 3)
    //     {
    //       if (cmdCount->lbcount == dsc.panelBitCount || cmdCount->lbcount == 0)
    //       {
    //         cmdCount->ccount = cmdCount->ccount + 1;
    //         if (cmdCount->ccount >= 3)
    //           cmdCount->bcount = dsc.panelBitCount;
    //       }
    //       else
    //       {
    //         dsc.panelData[11] = 0x76;
    //         cmdCount->ccount = 0; // reset since we did not get 3 in a row
    //         cmdCount->bcount = 0;
    //         cmdCount->lbcount = 0;
    //         return false;
    //       }
    //       cmdCount->lbcount = dsc.panelBitCount;
    //     }
    //   }

    //   return true;
    // }

#if defined(ARDUINO_MQTT)

    void DSCkeybushome::loop()
    {
#else
void DSCkeybushome::update()
{
#endif

      if (forceDisconnect) {
        return;
      }
        static bool firstRun=false;
        static bool lastConnectState=false;
        bool is_connected=network::is_connected();
        if (is_connected && is_connected != lastConnectState) firstRun=true;
        lastConnectState=is_connected;

#if defined(ESPHOME_MQTT)
      static bool firstrunmqtt = true;
      if (firstrunmqtt && mqtt::global_mqtt_client->is_connected())
      {
        mqtt::global_mqtt_client->publish(topic, "{\"name\":\"command\", \"cmd_t\":\"" + topic_prefix + setalarmcommandtopic + "\"}", 0, 1);
        firstrunmqtt = false;
      }
#endif

      if (beeps > 0 && millis() - beepTime > 2000)
      {
        beeps = 0;
        for (byte partition = 1; partition <= dscPartitions; partition++)
        {
          publishBeeps("0", partition);
        }
        beepTime = millis();
      }
      /*
      if (millis() - eventTime > 30000) {
        publishEventInfo("");
        eventTime = millis();
      }
      */

      static unsigned long refreshTime;
      if (!firstrun && refreshTimeSetting > 0 && millis() - refreshTime > refreshTimeSetting)
      {
        refreshTime = millis();
        forceRefresh = true;
        if (dsc.trouble && !partitionStatus[defaultPartition - 1].inprogram && !dsc.armed[defaultPartition - 1] && !dsc.alarm[defaultPartition - 1] && !dsc.disabled[defaultPartition - 1] && !partitionStatus[defaultPartition - 1].locked && troubleFetch)

        {
          partitionStatus[defaultPartition - 1].keyPressTime = millis();
          ESP_LOGD(TAG, "Periodic trouble flag fetch...");
          dsc.write("*2##", defaultPartition); // fetch panel troubles /zone module low battery
        }
        /*
    if (debug > 1)   {
#ifdef ESP32
      UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
      ESP_LOGD(TAG,"Free memory: %5d,freeheap: %5d,minheap: %5d,maxfree:%5d\n", (uint16_t) uxHighWaterMark,esp_get_free_heap_size(),esp_get_minimum_free_heap_size(),heap_caps_get_largest_free_block(8));
#endif
    }
    */
      }
      //  gptr->call();
      //&& dsc.panelData[0]
      if ((dsc.loop() || forceRefresh))
      { // Processes data only when a valid Keybus command has been read

#if !defined(ARDUINO_MQTT)
        if (dsc.bufferOverflow)
        {
          ESP_LOGE(TAG, "Keybus buffer overflow");
#else
    if (dsc.bufferOverflow)
    {
        Serial.printf("Keybus buffer overflow\n"); 
#endif
          dsc.bufferOverflow = false;
        }
        static unsigned long errorTime = millis();

        // Checks if the interface is connected to the Keybus
        if (dsc.keybusChanged || forceRefresh)
        {
          dsc.keybusChanged = false; // Resets the Keybus data status flag
          if (dsc.keybusConnected)
          {
            publishSystemStatus(FC(STATUS_ONLINE));
          }
          else
          {

            if (millis() - errorTime > 15000)
            {
              errorTime = millis();
              ESP_LOGE(TAG, "Panel keybus connect timeout! Is the panel connected?");
              publishSystemStatus(FC(STATUS_OFFLINE));
              return;
            }
          }
        }
        if (!dsc.panelData[0])
          return; // not valid data

        static uint8_t delayedStart = 1;
        static unsigned long startWait = millis();
        if (delayedStart == 1 && millis() - startWait > 10000)
        {
          forceRefresh = true;
          delayedStart++;
        }
        else if (delayedStart == 2 && millis() - startWait > 60000)
        {
          delayedStart++;
          if (!dsc.disabled[defaultPartition - 1] && !partitionStatus[defaultPartition - 1].locked && !dsc.armed[defaultPartition - 1] && !partitionStatus[defaultPartition - 1].inprogram && troubleFetch)
          {
            partitionStatus[defaultPartition - 1].keyPressTime = millis();
            dsc.write(fetchCmd, defaultPartition); // fetch panel troubles /zone module low battery
          }
        }

     //   bool valid05 = check051bCmd();
       #if defined(ESP8266) || defined(ESP32)
  #if defined(USE_ESP_IDF)
  taskYIELD ();
  #else
  yield();
  #endif
  #endif
        if (debug > 1)
          printPacket("Panel ", dsc.panelData[0], dsc.panelData, 16);
        // if (!valid05) {
        //   if (debug > 1)
        //     ESP_LOGW(TAG,"Bit count mismatch. Ignoring cmd");
        //   return;
        // }
#ifdef SERIALDEBUGCOMMANDS
        if (debug > 2)
        {
        #if defined(ARDUINO_MQTT)
          Serial.print(" ");
          dsc.printPanelBinary(); // Optionally prints without spaces: printPanelBinary(false);
          Serial.print(" [");
          dsc.printPanelCommand(); // Prints the panel command as hex
          Serial.print("] ");
          dsc.printPanelMessage(); // Prints the decoded message
          Serial.println();
        #else
          ESP_LOGD(TAG," ");
          dsc.printPanelBinary(); // Optionally prints without spaces: printPanelBinary(false);
          Serial.print(" [");
          dsc.printPanelCommand(); // Prints the panel command as hex
          Serial.print("] ");
          dsc.printPanelMessage(); // Prints the decoded message
          Serial.println();
        #endif
        }
#endif

        processStatus();

        for (byte partition = 0; partition < dscPartitions; partition++)
        {
           if ( dsc.status[partition] != 0xA0)
            continue;
          getBypassZones(partition + 1);
          setStatus(partition, true);
        }
      }

      if (!forceDisconnect && (dsc.statusChanged || forceRefresh) && dsc.panelData[0])
      {                            // Processes data only when a valid Keybus command has been read and statuses were changed
        dsc.statusChanged = false; // Reset the status tracking flag

        // if (debug  > 0)
        //  printPacket("Paneldata", dsc.panelData[0], dsc.panelData, 16);

        for (byte partition = 0; partition < dscPartitions; partition++)
        {
          if (firstrun)
          {
            publishBeeps("0", partition + 1);
            partitionStatus[partition].chime = 0;
            for (int x = 1; x <= maxRelays; x++)
              publishRelayStatus(x, false);
          }

          setStatus(partition, forceRefresh || dsc.status[partition] == 0xEE || dsc.status[partition] == 0xA0);
        }

        if (dsc.powerChanged || forceRefresh)
        {
          dsc.powerChanged = false;
          if (dsc.powerTrouble)
          {
            publishPanelStatus(FC(ACSTATUS), false, 0); // no ac
          }
          else
          {
            publishPanelStatus(FC(ACSTATUS), true, 0);
          }
        }

        if (dsc.batteryChanged || forceRefresh)
        {
          dsc.batteryChanged = false;
          if (dsc.batteryTrouble)
          {
            publishPanelStatus(FC(BATSTATUS), true, 0);
          }
          else
          {
            publishPanelStatus(FC(BATSTATUS), false, 0);
          }
        }

        if (dsc.keypadFireAlarm)
        {
          dsc.keypadFireAlarm = false;
          // partitionMsgChangeCallback("Keypad Fire Alarm",defaultPartition);
        }

        if (dsc.keypadPanicAlarm)
        {
          dsc.keypadPanicAlarm = false;
          // partitionMsgChangeCallback("Keypad Panic Alarm",defaultPartition);
        }

        // Publishes trouble status
        if (dsc.troubleChanged || forceRefresh)
        {
          dsc.troubleChanged = false; // Resets the trouble status flag

          if (dsc.trouble)
          {
            publishPanelStatus(FC(TRSTATUS), true, 0); // Trouble alarm tripped

          }
          else
          {
            publishPanelStatus(FC(TRSTATUS), false, 0); // Trouble alarm restored
          }

          if (!forceRefresh && !partitionStatus[defaultPartition - 1].inprogram && !dsc.armed[defaultPartition - 1] && !dsc.alarm[defaultPartition - 1] && !dsc.disabled[defaultPartition - 1] && !partitionStatus[defaultPartition - 1].locked && troubleFetch && (millis() - lastTroubleLightTime) > 20000)
          {
            partitionStatus[defaultPartition - 1].keyPressTime = millis();
            ESP_LOGD(TAG, "Fetching troubles..");
            dsc.write(fetchCmd, defaultPartition); // fetch panel troubles /zone module low battery
          }
          lastTroubleLightTime=millis();

        }

        // Publishes status per partition
        for (byte partition = 0; partition < dscPartitions; partition++)
        {

   

          if (lastStatus[partition] != dsc.status[partition])
          {
            lastStatus[partition] = dsc.status[partition];
            char msg[50];
            sprintf(msg, "%02x %s", dsc.status[partition],FC(statusText(dsc.status[partition])));
            publishPartitionMsg(msg, partition + 1);
          }

          // Publishes alarm status
          if (dsc.alarmChanged[partition] || forceRefresh)
          {
            dsc.alarmChanged[partition] = false; // Resets the partition alarm status flag
            if (dsc.alarm[partition])
            {
              dsc.readyChanged[partition] = false; // if we are triggered no need to trigger a ready state change
              dsc.armedChanged[partition] = false; // no need to display armed changed
            }
          }

          // Publishes armed/disarmed status
        //  ESP_LOGD("test","partition=%d,armed=%d,armedstay=%d,armedAway=%d,noentrydelay=%d,forced=%d",partition+1,dsc.armed[partition],dsc.armedStay[partition],dsc.armedAway[partition],dsc.noEntryDelay[partition],forceRefresh);
          if (dsc.armedChanged[partition] || forceRefresh)
          {
            dsc.armedChanged[partition] = false; // Resets the partition armed status flag

            if (dsc.armed[partition] && !dsc.alarm[partition])
            {
              clearZoneAlarms(partition + 1);
              publishPanelStatus(FC(ARMSTATUS), true, partition + 1);
            }
            else if (!dsc.exitDelay[partition] && !dsc.alarm[partition])
            {
              if (!forceRefresh)
              {
                clearZoneBypass(partition + 1);
              }
              publishPanelStatus(FC(ARMSTATUS), false, partition + 1);
            }
          }
          // Publishes exit delay status
          if (dsc.exitDelayChanged[partition] || forceRefresh)
          {
            clearZoneAlarms(partition + 1);
            dsc.exitDelayChanged[partition] = false; // Resets the exit delay status flag
          }

          // Publishes ready status

          if (dsc.readyChanged[partition] || forceRefresh)
          {
            dsc.readyChanged[partition] = false; // Resets the partition alarm status flag
            if (dsc.ready[partition] && !dsc.exitDelay[partition])
            {
              publishPanelStatus(FC(RDYSTATUS), true, partition + 1);
            }
            else if (!dsc.exitDelay[partition])
            {
              if (!dsc.armed[partition])
              {
                publishPanelStatus(FC(RDYSTATUS), false, partition + 1);
              }
              publishPanelStatus(FC(RDYSTATUS), false, partition + 1);
            }
          }

          // Publishes fire alarm status
          if (dsc.fireChanged[partition] || forceRefresh)
          {
            dsc.fireChanged[partition] = false; // Resets the fire status flag
            if (dsc.fire[partition])
            {
              publishFireStatus(true, partition + 1); // Fire alarm tripped
            }
            else
            {
              publishFireStatus(false, partition + 1); // Fire alarm restored
            }
          }
          if (forceRefresh)
          {
            publishPanelStatus(FC(CHIMESTATUS), partitionStatus[partition].chime, partition + 1);
          }

          std::string ps;
          const char * status=getPartitionStatus(partition,ps);
          publishPartitionStatus(status, partition + 1);


         // partitionStatus[partition].lastPartitionStatus = status;

        }

        // Publishes zones 1-64 status in a separate topic per zone
        // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
        //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
        //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
        //   ...
        //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64

        if (dsc.openZonesStatusChanged || forceRefresh)
        {
          for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++)
          {
            for (byte zoneBit = 0; zoneBit < 8; zoneBit++)
            {
              if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit) || forceRefresh)
              {                                                        // Checks an individual open zone status flag
                bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0); // Resets the individual open zone status flag
                zone = zoneBit + (zoneGroup * 8);
                if (zone >= maxZones)
                  continue;
                zoneType *zt = getZone(zone);
                if (bitRead(dsc.openZones[zoneGroup], zoneBit))
                {

                  zt->open = true;
                }
                else
                {
                  zt->open = false;
                }
                publishZoneStatus(zt);
              }
            }

          }
        }

        std::string zoneStatusMsg;
        zoneStatusMsg = "";
        char s1[7];
        for (auto &x : zoneStatus)
        {

          if (x.open)
          {
            if (zoneStatusMsg != "")
              sprintf(s1, FC(",OP:%d"), x.zone);
            else
              sprintf(s1, FC("OP:%d"), x.zone);
            zoneStatusMsg.append(s1);
          }

          if (x.alarm)
          {
            if (zoneStatusMsg != "")
              sprintf(s1, FC(",AL:%d"), x.zone);
            else
              sprintf(s1, FC("AL:%d"), x.zone);
            zoneStatusMsg.append(s1);
          }
          if (x.bypassed)
          {
            if (zoneStatusMsg != "")
              sprintf(s1, FC(",BY:%d"), x.zone);
            else
              sprintf(s1, FC("BY:%d"), x.zone);
            zoneStatusMsg.append(s1);
          }

          if (x.tamper)
          {
            if (zoneStatusMsg != "")
              sprintf(s1, FC(",TA:%d"), x.zone);
            else
              sprintf(s1, FC("TA:%d"), x.zone);
            zoneStatusMsg.append(s1);
          }

          if (x.battery_low)
          {
            if (zoneStatusMsg != "")
              sprintf(s1, FC(",LB:%d"), x.zone);
            else
              sprintf(s1, FC("LB:%d"), x.zone);
            zoneStatusMsg.append(s1);
          }

        }
        // if (zoneStatusMsg == "")
        // zoneStatusMsg = "Pending";

        if (zoneStatusMsg != previousZoneStatusMsg || forceRefresh)
          publishZoneMsgStatus(zoneStatusMsg);

        previousZoneStatusMsg = zoneStatusMsg;

        std::string system0Msg = "";
        std::string system1Msg = "";

        if (system0Changed || system1Changed || forceRefresh)
        {

          if (system1Changed)
            previousSystem1 = system1;
          else
            system1 = previousSystem1;

          if (bitRead(system1, 0))
          {
            system1Msg.append(FC("BAT "));
            if (system1Changed)
            {
              dsc.batteryTrouble = true;
              publishPanelStatus(FC(BATSTATUS), true, 0);
            }
          }
          else
          {
            if (system1Changed)
            {
              dsc.batteryTrouble = false;
              publishPanelStatus(FC(BATSTATUS), false, 0);
            }
          }

          if (bitRead(system1, 1))
          {
            system1Msg.append(FC("BELL "));
          }
          if (bitRead(system1, 2))
          {
            system1Msg.append(FC("SYS "));
          }
          if (bitRead(system1, 3))
          {
            system1Msg.append(FC("TAMP "));
          }
          if (bitRead(system1, 4))
          {
            system1Msg.append(FC("SUP "));
          }
          if (bitRead(system1, 5))
          {
            system1Msg.append(FC("RF "));
          }
          /*
          if (bitRead(system1, 6)) {
            system1Msg.append(FC("B4 "));
          }
          if (bitRead(system1, 7)) {
            system1Msg.append(FC("A4 "));
          }
    */

          if (system0Changed)
            previousSystem0 = system0;
          else
            system0 = previousSystem0;

          if (bitRead(system0, 1))
          {
            system0Msg.append(FC("AC "));
          }
          if (bitRead(system0, 2))
          {
            system0Msg.append(FC("TEL "));
          }
          if (bitRead(system0, 3))
          {
            system0Msg.append(FC("COM "));
          }
          if (bitRead(system0, 4))
          {
            system0Msg.append(FC("ZF "));
          }
          if (bitRead(system0, 5))
          {
            system0Msg.append(FC("ZT "));
          }
          if (bitRead(system0, 6))
          {
            system0Msg.append(FC("DBAT "));
          }
          if (bitRead(system0, 7))
          {
            system0Msg.append(FC("TIME "));
          }
          publishTroubleMsgStatus(system0Msg.append(system1Msg));
        }
        system0Changed = false;
        system1Changed = false;
      }

      if (!forceDisconnect && dsc.handleModule() && dsc.moduleCmd)
      {
        if (dsc.panelData[0] == 0x41)
        {
          for (byte zoneByte = 0; zoneByte < 4; zoneByte++)
          {
            byte zoneBit = 0;
            for (int x = 7; x >= 0; x--)
            {
              zone = zoneBit + (zoneByte * 8);
              if (zone >= maxZones)
                continue;
              if (!bitRead(dsc.moduleData[zoneByte + 2], x))
              { // Checks an individual zone battery status flag for low
                getZone(zone)->battery_low = true;
              }
              else if (!bitRead(dsc.moduleData[zoneByte + 6], x))
              { // Checks an individual zone battery status flag for restore
                getZone(zone)->battery_low = false;
              }
              zoneBit++;
            }


          }
        }
  #if defined(ESP8266) || defined(ESP32)
  #if defined(USE_ESP_IDF)
  taskYIELD ();
  #else
  yield();
  #endif
  #endif
        if (debug > 1)
          printPacket("Module", dsc.moduleCmd, dsc.moduleData, 16);

#ifdef DEBUGCOMMANDS
        if (debug > 2) //esp-idf
        {
          #if defined (ARDUINO_MQTT)
         Serial.print("[MODULE] ");
          Serial.print(dsc.panelData[0], HEX);
          Serial.print(": ");
          dsc.printModuleBinary(); // Optionally prints without spaces: printKeybusBinary(false);
          Serial.print(" ");
          dsc.printModuleMessage(); // Prints the decoded message
          Serial.println();
        #endif

        }
          
#endif
      }

      forceRefresh = false;
      firstrun = false;
    }

     const char * DSCkeybushome::getPartitionStatus(byte partition,std::string & status)
    {

      status="";
      if (dsc.status[partition] == 0x3e)
        status = FC(STATUS_DISARMED);
      else if (dsc.alarm[partition])
        status = FC(STATUS_TRIGGERED);
      else if (dsc.exitDelay[partition])
       status = FC(STATUS_EXIT);
      else if (dsc.entryDelay[partition])
        status = FC(STATUS_ENTRY);
      else if (dsc.noEntryDelay[partition])
        status = FC(STATUS_NIGHT);
      else if (dsc.armedStay[partition])
        status = FC(STATUS_STAY);
      else if (dsc.armedAway[partition])
        status = FC(STATUS_ARM);
#ifdef DETAILED_PARTITION_STATE
      else if (dsc.ready[partition])
        status = FC(STATUS_READY);
      else if (dsc.status[partition] != 0x9f && !(dsc.status[partition] > 0x03 && dsc.status[partition] < 0x0e))
        status = FC(STATUS_NOT_READY);
#else
  else
    status = FC(STATUS_DISARMED);
#endif
        return status.c_str();
    }

    void DSCkeybushome::setStatus(byte partition, bool force, bool skip)
    {
      if (partition >= dscPartitions)
        return;
      if (dsc.status[partition] == partitionStatus[partition].lastStatus && beeps == 0 && !force)
        return;
      byte *currentSelection = &partitionStatus[partition].currentSelection;
      byte *selectedOpenZone = &partitionStatus[partition].selectedZone;
      std::string lcdLine1;
      std::string lcdLine2;
      options = false;
      partitionStatus[partition].digits = 0;
      partitionStatus[partition].hex = false;
      partitionStatus[partition].decimalInput = false;
#if !defined(ARDUINO_MQTT)
      if (debug > 1)
        ESP_LOGI(TAG, "status %02X, last status %02X,selection %02X,partition=%d,skip=%d,force=%d", dsc.status[partition], partitionStatus[partition].lastStatus, *currentSelection, partition + 1, skip, force);
#else
  if (debug > 1)
    Serial.printf("status %02X, last status %02X,selection %02X,partition=%d,skip=%d,force=%d\n", dsc.status[partition], partitionStatus[partition].lastStatus, *currentSelection, partition + 1, skip, force); 
#endif
      switch (dsc.status[partition])
      {
      case 0x01:
        lcdLine1 = FC("Partition ready");
        lcdLine2 = FC(" ");
        break;
      case 0x02:
        lcdLine1 = FC("Stay");
        lcdLine2 = FC("zones open");
        break;
      case 0x03:
        lcdLine1 = FC("Zones open  <>");
        lcdLine2 = FC(" ");
        break;
      case 0x04:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Stay");
        break;
      case 0x05:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Away");
        break;
      case 0x06:
        lcdLine1 = FC("Armed Stay");
        lcdLine2 = FC("No entry delay");
        break;
      case 0x07:
        lcdLine1 = FC("Failed");
        lcdLine2 = FC("to arm");
        break;
      case 0x08:
        lcdLine1 = FC("Exit delay");
        lcdLine2 = FC("in progress");
        break;
      case 0x09:
        lcdLine1 = FC("Arming");
        lcdLine2 = FC("No entry delay");
        break;
      case 0x0B:
        lcdLine1 = FC("Quick exit");
        lcdLine2 = FC("in progress");
        break;
      case 0x0C:
        lcdLine1 = FC("Entry delay");
        lcdLine2 = FC("in progress");
        break;
      case 0x0D:
        lcdLine1 = FC("Entry delay");
        lcdLine2 = FC("after alarm");
        break;
      case 0x0E:
        lcdLine1 = FC("Not");
        lcdLine2 = FC("available");
        break;
      case 0x10:
        lcdLine1 = FC("Keypad");
        lcdLine2 = FC("lockout");
        break;
      case 0x11:
        lcdLine1 = FC("Partition in alarm");
        lcdLine2 = FC("  ");
        break;
      case 0x12:
        lcdLine1 = FC("Battery check");
        lcdLine2 = FC("in progress");
        break;
      case 0x14:
        lcdLine1 = FC("Auto-arm");
        lcdLine2 = FC("in progress");
        break;
      case 0x15:
        lcdLine1 = FC("Arming with");
        lcdLine2 = FC("bypass zones");
        break;
      case 0x16:
        lcdLine1 = FC("Armed away");
        lcdLine2 = FC("No entry delay");
        break;
      case 0x17:
        lcdLine1 = FC("Keypad blanked");
        lcdLine2 = FC("Enter access code");
        break;
      case 0x19:
        lcdLine1 = FC("Alarm");
        lcdLine2 = FC("occurred");
        break;
      case 0x22:
        lcdLine1 = FC("Alarms occurred");
        lcdLine2 = FC("Press # to exit");
        break;
      case 0x2F:
        lcdLine1 = FC("Keypad LCD");
        lcdLine2 = FC("test");
        break;
      case 0x33:
        lcdLine1 = FC("Command");
        lcdLine2 = FC("output active");
        break;
      case 0x3D:
        lcdLine1 = FC("Alarm");
        lcdLine2 = FC("occurred");
        break;
      case 0x3E:
        lcdLine1 = FC("Disarmed");
        lcdLine2 = FC(" ");
        break;
      case 0x40:
        lcdLine1 = FC("Keypad blanked");
        lcdLine2 = FC("Enter access code");
        break;
      case 0x80:
        lcdLine1 = FC("Invalid entry");
        lcdLine2 = FC(" ");
        break;
      case 0x8A:
        lcdLine1 = FC("Activate");
        lcdLine2 = FC("stay/away zones ");
        break;
      case 0x8B:
        lcdLine1 = FC("Quick exit");
        lcdLine2 = FC(" ");
        break;
      case 0x8E:
        lcdLine1 = FC("Invalid");
        lcdLine2 = FC("option");
        break;
      case 0x8F:
        lcdLine1 = FC("Invalid");
        lcdLine2 = FC("Access_code");
        break;
      case 0x9E:
        lcdLine1 = FC("Press (*) for <>");
        lcdLine2 = FC(" ");
        break;
      case 0x9F:
        lcdLine1 = FC("Enter");
        lcdLine2 = FC("Access_code");
        break;
      case 0xA0:
        lcdLine1 = FC("Zone bypass <>");
        lcdLine2 = FC(" ");
        break;
      case 0xA1:
        lcdLine1 = FC("Trouble menu <>");
        lcdLine2 = FC(" ");
        break;
      case 0xA2:
        lcdLine1 = FC("Alarm memory <>");
        lcdLine2 = FC(" ");
        break;
      case 0xA3:
        lcdLine1 = FC("Door");
        lcdLine2 = FC("chime enabled");
        partitionStatus[partition].chime = true;
        publishPanelStatus(FC(CHIMESTATUS), true, partition + 1);
        break;
      case 0xA4:
        lcdLine1 = FC("Door");
        lcdLine2 = FC("chime disabled  ");
        partitionStatus[partition].chime = false;
        publishPanelStatus(FC(CHIMESTATUS), false, partition + 1);
        break;
      case 0xA5:
        lcdLine1 = FC("Enter");
        lcdLine2 = FC("Master code");
        break;
      case 0xA6:
        lcdLine1 = FC("*5:  Access code");
        lcdLine2 = FC("code? (2 digits)");
        // digits = 2;
        break;
      case 0xA7:
        lcdLine1 = FC("*5 Enter new");
        lcdLine2 = FC("4-digit code");
        partitionStatus[partition].digits = 4;
        break;
      case 0xA9:
        lcdLine1 = FC("*6: User functions");
        lcdLine2 = FC("function?");
        break;
      case 0xAA:
        lcdLine1 = FC(" HHMM    MMDDYY");
        lcdLine2 = FC("");
        partitionStatus[partition].digits = 16;
        break;
      case 0xAB:
        lcdLine1 = FC(" HHMM");
        lcdLine2 = FC("");
        partitionStatus[partition].digits = 4;
        break;
      case 0xAC:
        lcdLine1 = FC("*6");
        lcdLine2 = FC("Auto-arm on");
        break;
      case 0xAD:
        lcdLine1 = FC("*6");
        lcdLine2 = FC("Auto-arm off");
        break;
      case 0xAF:
        lcdLine1 = FC("*6");
        lcdLine2 = FC("System test");
        break;
      case 0xB0:
        lcdLine1 = FC("*6");
        lcdLine2 = FC("Enable DLS");
        break;
      case 0xB1:
        lcdLine1 = FC("*6");
        lcdLine2 = FC("b1 command");
        break;
      case 0xB2:
      case 0xB3:
        lcdLine1 = FC("*7");
        lcdLine2 = FC("Command output");
        break;
      case 0xB7:
        lcdLine1 = FC("Enter");
        lcdLine2 = FC("installer code");
        break;
      case 0xB8:
        lcdLine1 = FC("Enter *");
        lcdLine2 = FC("function code");
        break;
      case 0xB9:
        lcdLine1 = FC("Zone Tamper <>");
        lcdLine2 = FC(" ");
        break;
      case 0xBA:
        lcdLine1 = FC("Zones low battery <>");
        lcdLine2 = FC(" ");
        break;
      case 0xBC:
        lcdLine1 = FC("*5 Enter new");
        lcdLine2 = FC("6-digit code");
        partitionStatus[partition].digits = 6;
        break;
      case 0xBF:
        lcdLine1 = FC("Select day:");
        lcdLine2 = FC("Sun=1,Tue=2,Sat=7");
        break;
      case 0xC6:
        lcdLine1 = FC(" Zone faults  <>");
        lcdLine2 = FC(" ");
        break;
      case 0xC7:
        lcdLine1 = FC("Partition");
        lcdLine2 = FC("disabled        ");
        break;
      case 0xC8:
        lcdLine1 = FC("Service req. <>");
        lcdLine2 = FC(" ");
        break;
      case 0xCD:
        lcdLine1 = FC("Downloading in progress");
        lcdLine2 = FC(" ");
        break;
      case 0xCE:
        lcdLine1 = FC("Active camera");
        lcdLine2 = FC("monitor select. ");
        break;
      case 0xD0:
        lcdLine1 = FC("*2: Keypads");
        lcdLine2 = FC("low battery");
        break;
      case 0xD1:
        lcdLine1 = FC("*2: Keyfobs");
        lcdLine2 = FC("low battery");
        break;
      case 0xD4:
        lcdLine1 = FC("*2: Sensors");
        lcdLine2 = FC("RF Delinquency");
        break;
      case 0xE4:
        lcdLine1 = FC("Section:");
        lcdLine2 = FC("(3 digits)");
        break;
      case 0xE5:
        lcdLine1 = FC("Keypad");
        lcdLine2 = FC("slot assignmen");
        break;
      case 0xE6:
        lcdLine1 = FC("Input:");
        lcdLine2 = FC("(2 digits)");
        partitionStatus[partition].digits = 2;
        break;
      case 0xE7:
        lcdLine1 = FC("Input:");
        partitionStatus[partition].digits = 3;
        lcdLine2 = FC("(3 digits)");
        partitionStatus[partition].decimalInput = true;
        break;
      case 0xE8:
        lcdLine1 = FC("Input:");
        partitionStatus[partition].digits = 4;
        lcdLine2 = FC("(4 digits)");
        break;
      case 0xE9:
        lcdLine1 = FC("Input:");
        partitionStatus[partition].digits = 5;
        lcdLine2 = FC("(5 digits)");
        break;
      case 0xEA:
        lcdLine1 = FC("Input hex:");
        partitionStatus[partition].digits = 2;
        partitionStatus[partition].hex = true;
        lcdLine2 = FC("(2 digits)");
        break;
      case 0xEB:
        lcdLine1 = FC("Input hex:");
        partitionStatus[partition].digits = 4;
        partitionStatus[partition].hex = true;
        lcdLine2 = FC("(4 digits)");
        break;
      case 0xEC:
        lcdLine1 = FC("Input hex:");
        partitionStatus[partition].digits = 6;
        partitionStatus[partition].hex = true;
        lcdLine2 = FC("(6 digits)");
        break;
      case 0xED:
        lcdLine1 = FC("Input hex:");
        partitionStatus[partition].digits = 32;
        partitionStatus[partition].hex = true;
        lcdLine2 = FC("(32 digits)  ");
        break;
      case 0xEE:
        lcdLine1 = FC("options:");
        options = true;
        lcdLine2 = FC("option per zone ");
        break;
      case 0xEF:
        lcdLine1 = FC("Module");
        lcdLine2 = FC("supervision");
        break;
      case 0xF0:
        lcdLine1 = FC("Function");
        lcdLine2 = FC("key 1");
        break;
      case 0xF1:
        lcdLine1 = FC("Function");
        lcdLine2 = FC("key 2");
        break;
      case 0xF2:
        lcdLine1 = FC("Function");
        lcdLine2 = FC("key 3");
        break;
      case 0xF3:
        lcdLine1 = FC("Function");
        lcdLine2 = FC("key 4");
        break;
      case 0xF4:
        lcdLine1 = FC("Function");
        lcdLine2 = FC("key 5");
        break;
      case 0xF5:
        lcdLine1 = FC("Wireless mod.");
        lcdLine2 = FC("placement test");
        break;
      case 0xF6:
        lcdLine1 = FC("Activate");
        lcdLine2 = FC("device for test");
        break;
      case 0xF7:
        lcdLine1 = FC("Sub-section:");
        lcdLine2 = FC("(2 digits)");
        break;
      case 0xF8:
        lcdLine1 = FC("Keypad");
        lcdLine2 = FC("programming");
        break;
      case 0xFA:
        lcdLine1 = FC("Input:");
        partitionStatus[partition].digits = 6;
        lcdLine2 = FC("(6 digits) ");
        break;
      default:
        lcdLine2 = std::to_string(dsc.status[partition]);
        partitionStatus[partition].digits = 0;
      }

      if (dsc.status[partition] != 0xA9)
        partitionStatus[partition].eventViewer = false;

      if (partitionStatus[partition].digits == 0)
        partitionStatus[partition].newData = false;

      if (millis() - partitionStatus[partition].keyPressTime > 3000 && dsc.status[partition] > 0x8B)
      {
        if (!partitionStatus[partition].inprogram)
        {
          partitionStatus[partition].locked = true;
          partitionStatus[partition].lastStatus = dsc.status[partition];
          return;
        }
        else
          partitionStatus[partition].locked = false;
      }
      else if (dsc.status[partition] > 0x8B && !partitionStatus[partition].locked)
      {
        partitionStatus[partition].inprogram = true;
      }
      if (dsc.status[partition] < 0x8B)
      {
        partitionStatus[partition].locked = false;
        partitionStatus[partition].inprogram = false;
        activePartition = defaultPartition;
      }

      if (!skip)
      {

        // ESP_LOGI("test", "digits = %d,status=%02X,previoustatus=%02X,newdata=%d,locked=%d,partition=%d,selection=%d", partitionStatus[partition].digits, dsc.status[partition], partitionStatus[partition].lastStatus, partitionStatus[partition].newData, partitionStatus[partition].locked, partition + 1, *currentSelection);

        // if multi digit field, setup for 6E request to panel
        if (dsc.status[partition] != partitionStatus[partition].lastStatus && !partitionStatus[partition].locked && partitionStatus[partition].digits && !partitionStatus[partition].newData)
        {

          // ESP_LOGI("test", "in setlcd: digits = %d,status=%02X,previoustatus=%02X,newdata=%d,locked=%d", partitionStatus[partition].digits, dsc.status[partition], partitionStatus[partition].lastStatus, partitionStatus[partition].newData, partitionStatus[partition].locked);

          dsc.setLCDReceive(partitionStatus[partition].digits, partition + 1);
          partitionStatus[partition].editIdx = 0;
          partitionStatus[partition].hexMode = false;
          partitionStatus[partition].newData = true;
          lcdLine1 = FC("");
          lcdLine2 = FC("");

          // ok, we should have the data now so display it
        }
        else if (partitionStatus[partition].digits && partitionStatus[partition].newData && dsc.pgmBuffer.dataPending)
        {
          char s[8];
          if (partitionStatus[partition].digits > 16)
            lcdLine1 = " ";
          lcdLine2 = " ";
          int y;
          char c;
          if (partitionStatus[partition].hexMode)
            lcdLine1 = FC("*");

          if (partitionStatus[partition].decimalInput)
          {
            if (partitionStatus[partition].digits == 2)
              sprintf(decimalInputBuffer, FC("%2d"), dsc.pgmBuffer.data[0]);
            else
              sprintf(decimalInputBuffer, FC("%03d"), dsc.pgmBuffer.data[0]);
          }
          for (int x = 0; x < partitionStatus[partition].digits; x++)
          {
            y = (x / 2) + (x / 8); // skip every 5th byte since it's a checksum
            if (partitionStatus[partition].decimalInput)
              c = decimalInputBuffer[x] - 48;
            else
              c = x % 2 ? dsc.pgmBuffer.data[y] & 0x0F : (dsc.pgmBuffer.data[y] & 0xF0) >> 4;
            std::string tpl = FC("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");

            if (dsc.status[partition] == 0xAA)
            {
              tpl = FC("XXXX    XXXXXX  XXXXXXXXXXXXXXXX");
            }
            if (dsc.status[partition] == 0xAB)
            {
              tpl = FC("XXXX");
            }
            if (tpl[x] == 'X')
            {
              if (x == partitionStatus[partition].editIdx)
                sprintf(s, FC("[%1X]"), c);
              else
                sprintf(s, FC("%1X"), c);
            }
            else
              sprintf(s, " ");

            if (partitionStatus[partition].digits < 17)
              lcdLine2 += s;
            else
            {
              if (x < 16)
                lcdLine1 += s;
              else
                lcdLine2 += s;
            }
          }
        }
        else if (partitionStatus[partition].digits)
        {
          lcdLine1 = "";
          lcdLine2 = "";
        }

        if (dsc.status[partition] < 0x04)
        { // status menu/open zones
          if (*currentSelection > 1 && *currentSelection < amsize)
          {
            std::string s = FC(statusMenu[*currentSelection]);
            int pos = s.find(":");
            lcdLine1 = s.substr(0, pos);
            lcdLine2 = s.substr(pos + 1);
          }
          else
          {
            byte c = dsc.ready[partition] ? 0 : 1;
            std::string s = FC(statusMenuLabels[c]);
            int pos = s.find(":");
            lcdLine1 = s.substr(0, pos);
            lcdLine2 = s.substr(pos + 1);
            *currentSelection = 1;
          }
          if (*selectedOpenZone && *selectedOpenZone < maxZones)
          { // open zones
            char s[51];
            std::string name = getZoneName(*selectedOpenZone);

            if (name != "")
              snprintf(s, 50, FC("%02d %s"), *selectedOpenZone, name.c_str());
            else
              snprintf(s, 50, FC("%02d"), *selectedOpenZone);
            lcdLine2 = s;
          }
        }
        else if (dsc.status[partition] == 0xA0)
        { // bypass
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
            *currentSelection = getNextEnabledZone(0, partition + 1);
          if (*currentSelection < maxZones && *currentSelection > 0)
          {
            char s[51];
            char bypassStatus = ' ';
            if (getZone(*currentSelection - 1)->bypassed)
              bypassStatus = 'B';
            else if (getZone(*currentSelection - 1)->open)
              bypassStatus = 'O';
            std::string name = getZoneName(*currentSelection);
            if (name != "")
                if (bypassStatus==' ')
                  snprintf(s, 50, FC("%02d %s"), *currentSelection, name.c_str());
                else
                  snprintf(s, 50, FC("%02d %s %c"), *currentSelection, name.c_str(), bypassStatus);
            else if (bypassStatus==' ')
                snprintf(s, 50, FC("%02d"), *currentSelection);
              else
                snprintf(s, 50, FC("%02d %c"), *currentSelection, bypassStatus);
            lcdLine2 = s;
          }
        }
        else if (dsc.status[partition] == 0x11)
        { // alarms
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
            *currentSelection = getNextAlarmedZone(0, partition + 1);
          if (*currentSelection < maxZones && *currentSelection > 0)
          {
            char s[51];
            std::string name = getZoneName(*currentSelection);
            if (name != "")
              snprintf(s, 50, FC("%02d %s"), *currentSelection, name.c_str());
            else
              snprintf(s, 50, FC("%02d "), *currentSelection);
            lcdLine2 = s;
          }
          else
            lcdLine2 = " ";
        }
        else if (dsc.status[partition] == 0xA2)
        { // alarm memory
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
            *currentSelection = getNextOption(0);
          if (*currentSelection < maxZones && *currentSelection > 0)
          {
            char s[51];
            std::string name = getZoneName(*currentSelection);
            if (name != "")
              snprintf(s, 50, FC("%02d (%s)"), *currentSelection, name.c_str());
            else
              snprintf(s, 50, FC("%02d"), *currentSelection);

            lcdLine2 = s;
          }
          else if (!force)
          {
            lcdLine1 = FC("No alarms");
            lcdLine2 = FC("in memory");
          }
        }
        else if (dsc.status[partition] == 0xBA)
        { // low battery zones
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
            *currentSelection = getNextOption(0);

          if (*currentSelection < maxZones && *currentSelection > 0)
          {
            char s[51];
            std::string name = getZoneName(*currentSelection);
            if (name != "")
              snprintf(s, 50, FC("%02d (%s)"), *currentSelection, name.c_str());
            else
              snprintf(s, 50, FC("%02d"), *currentSelection);

            lcdLine2 = s;
          }
          else
          {
            lcdLine1 = FC("There are no");
            lcdLine2 = FC("low battery zones");
          }
        }
        else if (dsc.status[partition] == 0x9E)
        { // main menu
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
          {
            *currentSelection = 1;
          }
          if (*currentSelection < mmsize)
            lcdLine2 =FC(mainMenu[*currentSelection]);
        }
        else if (dsc.status[partition] == 0xB2)
        { // output menu
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
          {
            *currentSelection = 1;
          }
          if (*currentSelection < omsize)
            lcdLine2 = FC(outputMenu[*currentSelection]);
        }
        else if (dsc.status[partition] == 0xA9 && !partitionStatus[partition].eventViewer)
        { // user menu
          if (*currentSelection == 0 || dsc.status[partition] != partitionStatus[partition].lastStatus)
          {
            *currentSelection = 1;
          }
          if (*currentSelection < umsize)
            lcdLine2 = FC(userMenu[*currentSelection]);
        }
        else if (dsc.status[partition] == 0xA1)
        { // trouble
          if (*currentSelection == 0)
          {
            *currentSelection = getNextOption(0);
          }
          if (*currentSelection < tmsize)
          {
            lcdLine2 = FC(troubleMenu[*currentSelection]);
          }
        }
        else if (dsc.status[partition] == 0xC8)
        { // service

          if (*currentSelection == 0)
            *currentSelection = getNextOption(0);
          if (*currentSelection < smsize)
          {
            lcdLine2 = FC(serviceMenu[*currentSelection]);
          }
        }
        else if (dsc.status[partition] == 0xA6)
        { // user code
          if (*currentSelection == 0)
            *currentSelection = getNextUserCode(0);
          if (*currentSelection < 96)
          {
            char s[51];
            char programmed = ' ';
            if (checkUserCode(*currentSelection))
              programmed = 'P';
            std::string name = getUserName(*currentSelection);
            if (name != "")
              snprintf(s, 50, FC("%02d (%s)   %c"), *currentSelection, name.c_str(), programmed);
            else
              snprintf(s, 50, FC("%02d   %c"), *currentSelection, programmed);
            lcdLine2 = s;
          }
        }
        else // Sends the Access_code when needed by the panel for arming
          if (dsc.status[partition] == 0x9F && dsc.accessCodePrompt && isInt(accessCode, 10))
          {
            dsc.accessCodePrompt = false;
            dsc.write(accessCode, partition + 1);
            if (debug > 0)
              ESP_LOGI(TAG, "got Access code prompt for partition %d", partition + 1);
          }

        if (options)
        {
          lcdLine2 = getOptionsString();
        }

      } // if skip

      if (lcdLine1 != "")
        publishLine1(lcdLine1.c_str(), partition + 1);
      if (lcdLine2 != "")
        publishLine2(lcdLine2.c_str(), partition + 1);

      partitionStatus[partition].lastStatus = dsc.status[partition];
    }

    // Processes status data not natively handled within the library
    void DSCkeybushome::processStatus()
    {
      switch (dsc.panelData[0])
      {
      case 0x0F:
      case 0x0A:

        processProgramZones(4, 0);
        // 0A: 0A 00 80 BA 00 10 00 00 00 54 00 00 00 00 00 00 (74)
        if (dsc.panelData[3] == 0xBA)
          processLowBatteryZones();
        if (dsc.panelData[3] == 0xA1)
        { // system status
          system0 = dsc.panelData[4];
          system0Changed = true;
        }
        if (dsc.panelData[3] == 0xC8)
        { // service required menu
          system1 = dsc.panelData[4];
          system1Changed = true;
        }

        break;
      case 0x5D:
      case 0x63:

        if ((dsc.panelData[2] & 0x04) == 0x04)
        { // Alarm memory zones 1-32
          processProgramZones(3, 0);
        }
        break;

      case 0xA5:

        processEventBufferAA(true);
        break;
      case 0xAA:

        processEventBufferAA();
        break;
      case 0x6E:
        printPanel_0x6E();
        break;
      case 0x69:
          printBeeps(2,1); //partition 2
          break;
      case 0x64:
        printBeeps(2,0); //partition 1
        break;
      case 0x75: // tones 1
      case 0x7D:
        // ESP_LOGI(TAG, "Sent tones cmd %02X,%02X", dsc.panelData[0], dsc.panelData[3]);
        break;   // tones 2
      case 0x87: // relay cmd
        processRelayCmd();
        break;
      case 0xB1:
        getEnabledZonesB1(2, 1, 1);
        getEnabledZonesB1(6, 1, 2);
        break;
      case 0xE6:

        switch (dsc.panelData[2])
        {
        case 0x01:
          if (!(dsc.panelData[9] & 0x80))
            processProgramZones(5, 0);
          else
            processProgramZones(5, 4);
          break;
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x1D: // ESP_LOGI(TAG, "Sent tones cmd %02X,%02X", dsc.panelData[0], dsc.panelData[4]);
          break;   // tones 3-8
        case 0x19:
          printBeeps19(4,3);
          break;
        case 0x1A:
          break;
        case 0x20:
        case 0x21:
          processProgramZones(5, 4);
          break; // Programming zone lights 33-64 //bypass?
        case 0x18:
          // ESP_LOGI(TAG, "zone lights 33");
          if ((dsc.panelData[4] & 0x04) == 0x04)
            processProgramZones(5, 4);
          break; // Alarm memory zones 33-64
        case 0x2B:
          getEnabledZonesE6(4, 1, 3);
          break;
        case 0x2C:
          getEnabledZonesE6(4, 33, 3);
          break;
        };

        break;
      case 0xEB:

        if (dsc.panelData[7] == 1)
          switch (dsc.panelData[8])
          {
          case 0xAE:
            publishLine2("Walk test end", activePartition);
            break;
          case 0xAF:
            publishLine2("Walk test beging", activePartition);
            break;
          };
        processEventBufferEC(true);
        break;
      case 0xEC:
        processEventBufferEC();
        break;
      }
    }

    void DSCkeybushome::printPanelTone(byte panelByte)
    {

      if (dsc.panelData[panelByte] == 0)
      {
        // stream->print(F("none"));

        return;
      }

      if ((dsc.panelData[panelByte] & 0x80) == 0x80)
      {
        // stream->print(F("constant tone "));
      }

      if ((dsc.panelData[panelByte] & 0x70) != 0)
      {
        // stream->print((panelData[panelByte] & 0x70) >> 4);
        // stream->print(F(" beep "));
      }

      if ((dsc.panelData[panelByte] & 0x0F) != 0)
      {
        //  stream->print(panelData[panelByte] & 0x0F);
        //  stream->print(F("s interval"));
      }
    }

    void DSCkeybushome::printBeeps(byte beepByte,byte partition)
    {
      dsc.statusChanged = true;
      beeps = dsc.panelData[beepByte] / 2;
      char s[4];
      sprintf(s, "%d", beeps);
        publishBeeps(s, partition + 1);
        if (beeps == 2 && partitionStatus[partition].digits)
        {
          dsc.setLCDReceive(partitionStatus[partition].digits, partition);
          partitionStatus[partition].editIdx = 0;
          partitionStatus[partition].hexMode = false;
          partitionStatus[partition].newData = true;
        }

      beepTime = millis();
    }

    void DSCkeybushome::printBeeps19(byte beepByte,byte partitionByte)
    {
      dsc.statusChanged = true;
      beeps = dsc.panelData[beepByte] / 2;
      char s[4];
      sprintf(s, "%d", beeps);
      byte bitCount = 0;
      for (byte bit = 0; bit <= 7; bit++) {
       if (bitRead(dsc.panelData[partitionByte], bit)) {
        byte partition=bitCount;
        publishBeeps(s,partition+1);
        if (beeps == 2 && partitionStatus[partition].digits)
        {
          dsc.setLCDReceive(partitionStatus[partition].digits, partition);
          partitionStatus[partition].editIdx = 0;
          partitionStatus[partition].hexMode = false;
          partitionStatus[partition].newData = true;
        }

      }
      bitCount++;
     }
      beepTime = millis();
    }

    void DSCkeybushome::printPanel_0x6E()
    {
      if (dsc.pgmBuffer.partition && dsc.pgmBuffer.partition <= dscPartitions)
      {
        if (dsc.pgmBuffer.idx == dsc.pgmBuffer.len)
          setStatus(dsc.pgmBuffer.partition - 1, true);
      }
    }
    // 0A: 0A 00 80 BA 00 10 00 00 00 54 00 00 00 00 00 00 (74)
    void DSCkeybushome::processLowBatteryZones()
    {
      for (byte panelByte = 4; panelByte < 8; panelByte++)
      {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++)
        {
          zone = zoneBit + ((panelByte - 4) * 8);
          if (zone >= maxZones)
            continue;
          if (bitRead(dsc.panelData[panelByte], zoneBit))
          {
            getZone(zone)->battery_low = true;
          }
          else
            getZone(zone)->battery_low = false;
        }
      }
    }

    void DSCkeybushome::processRelayCmd()
    {
      byte rchan;
      byte pgm = 2;
      for (byte relayByte = 2; relayByte < 4; relayByte++)
      {
        for (byte x = 0; x < 8; x++)
        {

          if (relayByte == 3)
          {
            if (x < 2)
              pgm = 0;
            else if (x == 2 || x == 3)
              continue;
            else if (x > 3)
              pgm = 6;
          }
          rchan = pgm + x;
          if (bitRead(dsc.panelData[relayByte], x))
          {
            relayStatus[rchan] = true;
          }
          else
          {
            relayStatus[rchan] = false;
          }
          if (previousRelayStatus[rchan] != relayStatus[rchan])
            publishRelayStatus(rchan + 1, relayStatus[rchan]);
          previousRelayStatus[rchan] = relayStatus[rchan];
        }
      }
    }

    void DSCkeybushome::processProgramZones(byte startByte, byte zoneStart)
    {
      byte byteCount = 0;
      byte zone;

      for (byte zoneGroup = zoneStart; zoneGroup < zoneStart + 4; zoneGroup++)
      {
        programZones[zoneGroup] = dsc.panelData[startByte + byteCount];
        byteCount++;
      }
      if (zoneStart == 0)
      {
        // clear upper group
        for (int x = 4; x < dscZones; x++)
        {
          programZones[x] = 0;
        }
      }

      if (options)
        dsc.statusChanged = true;
    }

    void DSCkeybushome::processEventBufferAA(bool showEvent)
    {
#ifndef dscClassicSeries
      if (extendedBufferFlag)
        return; // Skips 0xAA data when 0xEC extended event buffer data is available
      eventStatusMsg = "";
      char eventInfo[45] = "";
      char charBuffer[5];
      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        strcat_P(eventInfo, FC("'time':'"));
#else
        strcat_P(eventInfo, FC("["));
#endif
      }
      if (!showEvent)
      {
        strcat_P(eventInfo, FC("E:"));
        itoa(dsc.panelData[7], charBuffer, 10);
        if (dsc.panelData[7] < 10)
          strcat_P(eventInfo, FC("00"));
        else if (dsc.panelData[7] < 100)
          strcat(eventInfo, "0");
        strcat(eventInfo, charBuffer);
        strcat(eventInfo, " ");
      }
      byte dscYear3 = dsc.panelData[2] >> 4;
      byte dscYear4 = dsc.panelData[2] & 0x0F;
      byte dscMonth = dsc.panelData[2 + 1] << 2;
      dscMonth >>= 4;
      byte dscDay1 = dsc.panelData[2 + 1] << 6;
      dscDay1 >>= 3;
      byte dscDay2 = dsc.panelData[2 + 2] >> 5;
      byte dscDay = dscDay1 | dscDay2;
      byte dscHour = dsc.panelData[2 + 2] & 0x1F;
      byte dscMinute = dsc.panelData[2 + 3] >> 2;

      if (dscYear3 >= 7)
        strcat_P(eventInfo, FC("19"));
      else
        strcat_P(eventInfo, FC("20"));
      itoa(dscYear3, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      itoa(dscYear4, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, ".");
      if (dscMonth < 10)
        strcat(eventInfo, "0");
      itoa(dscMonth, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, ".");
      if (dscDay < 10)
        strcat(eventInfo, "0");
      itoa(dscDay, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, " ");
      if (dscHour < 10)
        strcat(eventInfo, "0");
      itoa(dscHour, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, ":");
      if (dscMinute < 10)
        strcat(eventInfo, "0");
      itoa(dscMinute, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        strcat(eventInfo, "'");
#else
        strcat(eventInfo, "] ");
#endif
      }
      if (dsc.panelData[6] == 0 && dsc.panelData[7] == 0)
      {
        // timestamp
        return;
      }
      byte partition = dsc.panelData[3] >> 6;

      if (!partition)
        partition = activePartition;

      if (!showEvent)
      {
        strcat_P(eventInfo, FC(", p:"));
        itoa(partition, charBuffer, 10);
        strcat(eventInfo, charBuffer);
      }

      if (showEvent)
        eventStatusMsg = eventInfo;
      else
        publishLine1(eventInfo, activePartition);

      switch (dsc.panelData[5] & 0x03)
      {
      case 0x00:
        printPanelStatus0(6, partition, showEvent);
        break;
      case 0x01:
        printPanelStatus1(6, partition, showEvent);
        break;
      case 0x02:
        printPanelStatus2(6, partition, showEvent);
        break;
      case 0x03:
        printPanelStatus3(6, partition, showEvent);
        break;
      }
      if (showEvent && eventStatusMsg != "")
      {
       std::string ps;
#ifdef USE_JSON_EVENT
        eventStatusMsg =("{") + eventStatusMsg.append(FC(",'status':'")).append(getPartitionStatus(partition - 1,ps)).append(FC(",'partition':")).append(partition).append("}");
#else
        if (maxPartitions() > 1)
          eventStatusMsg.append(FC(", Partition ")).append(std::to_string(partition)).append(FC(" status is ")).append(getPartitionStatus(partition - 1,ps));
        else
          eventStatusMsg.append(FC(", Partition status is ")).append(getPartitionStatus(partition - 1,ps));
#endif
        publishEventInfo(eventStatusMsg);
        eventTime = millis();
        eventTime = millis();
      }
#endif
    }

    void DSCkeybushome::processEventBufferEC(bool showEvent)
    {
#ifndef dscClassicSeries
      if (!extendedBufferFlag)
        extendedBufferFlag = true;
      eventStatusMsg = "";
      char eventInfo[45] = "";
      char charBuffer[5];
      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        strcat_P(eventInfo, FC("'time':'"));
#else
        strcat_P(eventInfo, FC("["));
#endif
      }

      if (!showEvent)
      {
        strcat(eventInfo, "E:");
        int eventNumber = dsc.panelData[9] + ((dsc.panelData[4] >> 6) * 256);
        itoa(eventNumber, charBuffer, 10);
        if (eventNumber < 10)
          strcat_P(eventInfo, FC("00"));
        else if (eventNumber < 100)
          strcat(eventInfo, "0");
        strcat(eventInfo, charBuffer);
        strcat(eventInfo, " ");
      }

      byte dscYear3 = dsc.panelData[3] >> 4;
      byte dscYear4 = dsc.panelData[3] & 0x0F;
      byte dscMonth = dsc.panelData[4] << 2;
      dscMonth >>= 4;
      byte dscDay1 = dsc.panelData[4] << 6;
      dscDay1 >>= 3;
      byte dscDay2 = dsc.panelData[5] >> 5;
      byte dscDay = dscDay1 | dscDay2;
      byte dscHour = dsc.panelData[5] & 0x1F;
      byte dscMinute = dsc.panelData[6] >> 2;

      if (dscYear3 >= 7)
        strcat_P(eventInfo, FC("19"));
      else
        strcat_P(eventInfo, FC("20"));
      itoa(dscYear3, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      itoa(dscYear4, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, ".");
      if (dscMonth < 10)
        strcat(eventInfo, "0");
      itoa(dscMonth, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, ".");
      if (dscDay < 10)
        strcat(eventInfo, "0");
      itoa(dscDay, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, " ");
      if (dscHour < 10)
        strcat(eventInfo, "0");
      itoa(dscHour, charBuffer, 10);
      strcat(eventInfo, charBuffer);
      strcat(eventInfo, ":");
      if (dscMinute < 10)
        strcat(eventInfo, "0");
      itoa(dscMinute, charBuffer, 10);
      strcat(eventInfo, charBuffer);

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        strcat(eventInfo, "'");
#else
        strcat_P(eventInfo, FC("] "));
#endif
      }

      byte partition = activePartition;

      if (dsc.panelData[2] != 0)
      {

        if (!showEvent)
        {
          strcat_P(eventInfo, FC(", p:"));
        }

        byte bitCount = 0;
        for (byte bit = 0; bit <= 7; bit++)
        {
          if (bitRead(dsc.panelData[2], bit))
          {
            partition = bitCount + 1;
            break;
          }
          bitCount++;
        }
        if (!showEvent)
        {
          itoa(partition, charBuffer, 10);
          strcat(eventInfo, charBuffer);
        }
      }
      if (showEvent)
        eventStatusMsg = eventInfo;
      else
        publishLine1(eventInfo, activePartition);

      switch (dsc.panelData[7])
      {
      case 0x00:
        printPanelStatus0(8, partition, showEvent);
        break;
      case 0x01:
        printPanelStatus1(8, partition, showEvent);
        break;
      case 0x02:
        printPanelStatus2(8, partition, showEvent);
        break;
      case 0x03:
        printPanelStatus3(8, partition, showEvent);
        break;
      case 0x04:
        printPanelStatus4(8, partition, showEvent);
        break;
      case 0x05:
        printPanelStatus5(8, partition, showEvent);
        break;
      case 0x14:
        printPanelStatus14(8, partition, showEvent);
        break;
      case 0x16:
        printPanelStatus16(8, partition, showEvent);
        break;
      case 0x17:
        printPanelStatus17(8, partition, showEvent);
        break;
      case 0x18:
        printPanelStatus18(8, partition, showEvent);
        break;
      case 0x1B:
        printPanelStatus1B(8, partition, showEvent);
        break;
      }
      if (showEvent && eventStatusMsg != "")
      {
       std::string ps;
#ifdef USE_JSON_EVENT
        eventStatusMsg = ("{") + eventStatusMsg.append(FC(",'status':'")).append(getPartitionStatus(partition - 1,ps)).append(FC(",'partition':")).append(partition).append("}");
#else
        if (maxPartitions() > 1)
          eventStatusMsg.append(FC(", Partition ")).append(std::to_string(partition)).append(FC(" status is ")).append(getPartitionStatus(partition - 1,ps));
        else
          eventStatusMsg.append(FC(", Partition status is ")).append(getPartitionStatus(partition - 1,ps));
#endif

        publishEventInfo(eventStatusMsg);
        eventTime = millis();
      }
#endif
    }

    void DSCkeybushome::printPanelStatus0(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;

      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;
      switch (dsc.panelData[panelByte])
      {
      case 0x49:
        lcdLine1 = FC("Duress alarm");
        lcdLine2 = FC(" ");
        break;
      case 0x4A:
        lcdLine1 = FC("Disarmed");
        lcdLine2 = FC("Alarm mem");
        break;
      case 0x4B:
        lcdLine1 = FC("Recent");
        lcdLine2 = FC("closing alarm");
        break;
      case 0x4C:
        lcdLine1 = FC("Zone exp");
        lcdLine2 = FC("suprvis. alarm");
        break;
      case 0x4D:
        lcdLine1 = FC("Zone exp");
        lcdLine2 = FC("suprvis. rest");
        break;
      case 0x4E:
        lcdLine1 = FC("Keypad Fire");
        lcdLine2 = FC("alarm");
        break;
      case 0x4F:
        lcdLine1 = FC("Keypad Aux");
        lcdLine2 = FC("alarm");
        break;
      case 0x50:
        lcdLine1 = FC("Keypad Panic");
        lcdLine2 = FC("alarm");
        break;
      case 0x51:
        lcdLine1 = FC("Aux input");
        lcdLine2 = FC("alarm");
        break;
      case 0x52:
        lcdLine1 = FC("Keypad Fire");
        lcdLine2 = FC("alarm rest");
        break;
      case 0x53:
        lcdLine1 = FC("Keypad Aux");
        lcdLine2 = FC("alarm rest");
        break;
      case 0x54:
        lcdLine1 = FC("Keypad Panic");
        lcdLine2 = FC("alarm rest");
        break;
      case 0x55:
        lcdLine1 = FC("Aux input");
        lcdLine2 = FC("alarm rest");
        break;
        // 0x56 - 0x75: Zone tamper, zones 1-32
        // 0x76 - 0x95: Zone tamper restored, zones 1-32
      case 0x98:
        lcdLine1 = FC("Keypad");
        lcdLine2 = FC("lockout");
        break;
        // 0x99 - 0xBD: Armed: Access_codes 1-34, 40-42
      case 0xBE:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Partial");
        break;
      case 0xBF:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Special");
        publishUserArmingDisarming("<quick arm>", partition);
        break;
        // 0xC0 - 0xE4: Disarmed: Access_codes 1-34, 40-42
      case 0xE5:
        lcdLine1 = FC("Auto-arm");
        lcdLine2 = FC("canc");
        break;
      case 0xE6:
        lcdLine1 = FC("Disarmed");
        lcdLine2 = FC("Special");
        break;
      case 0xE7:
        lcdLine1 = FC("Panel bat");
        lcdLine2 = FC("trble");
        break;
      case 0xE8:
        lcdLine1 = FC("Panel AC");
        lcdLine2 = FC("trble");
        break;
      case 0xE9:
        lcdLine1 = FC("Bell trble");
        lcdLine2 = FC(" ");
        break;
      case 0xEA:
        lcdLine1 = FC("Fire zone");
        lcdLine2 = FC("trble");
        break;
      case 0xEB:
        lcdLine1 = FC("Panel aux sup");
        lcdLine2 = FC("trble");
        break;
      case 0xEC:
        lcdLine1 = FC("Tel line");
        lcdLine2 = FC("trble");
        break;
      case 0xEF:
        lcdLine1 = FC("Panel bat");
        lcdLine2 = FC("rest");
        break;
      case 0xF0:
        lcdLine1 = FC("Panel AC");
        lcdLine2 = FC("rest");
        break;
      case 0xF1:
        lcdLine1 = FC("Bell rest");
        lcdLine2 = FC(" ");
        break;
      case 0xF2:
        lcdLine1 = FC("Fire zone");
        lcdLine2 = FC("trble rest");
        break;
      case 0xF3:
        lcdLine1 = FC("Panel aux sup");
        lcdLine2 = FC("rest");
        break;
      case 0xF4:
        lcdLine1 = FC("Tel line");
        lcdLine2 = FC("rest");
        break;
      case 0xF7:
        lcdLine1 = FC("Phone 1 FTC");
        lcdLine2 = FC(" ");
        break;
      case 0xF8:
        lcdLine1 = FC("Phone 2 FTC");
        lcdLine2 = FC(" ");
        break;
      case 0xF9:
        lcdLine1 = FC("Event buffer");
        lcdLine2 = FC("threshold");
        break;
      case 0xFA:
        lcdLine1 = FC("DLS lead-in");
        lcdLine2 = FC(" ");
        break;
      case 0xFB:
        lcdLine1 = FC("DLS lead-out");
        lcdLine2 = FC(" ");
        break;
      case 0xFE:
        lcdLine1 = FC("Periodic test");
        lcdLine2 = FC("trans");
        break;
      case 0xFF:
        lcdLine1 = FC("System test");
        lcdLine2 = FC(" ");
        break;
      default:
        decoded = false;
      }
      if (decoded)
        eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      if (dsc.panelData[panelByte] >= 0x09 && dsc.panelData[panelByte] <= 0x28)
      {
        lcdLine1 = FC("Zone alarm on");
        byte zone = dsc.panelData[panelByte] - 8;
        if (zone > 0 && zone < maxZones)
          getZone(zone - 1)->alarm = true;
        decoded = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        eventstr = lcdLine1.c_str();
        publishZoneAlarm(zonestr, partition);
        dsc.statusChanged = true;
      }

      if (dsc.panelData[panelByte] >= 0x29 && dsc.panelData[panelByte] <= 0x48)
      {
        lcdLine1 = FC("Zone alarm off");
        byte zone = dsc.panelData[panelByte] - 40;
        // if (zone > 0 && zone < maxZones)
        // zoneStatus[zone-1].alarm=false;
        decoded = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        eventstr = lcdLine1.c_str();
        dsc.statusChanged = true;
      }

      if (dsc.panelData[panelByte] >= 0x56 && dsc.panelData[panelByte] <= 0x75)
      {
        lcdLine1 = FC("Zone tamper ON");
        byte zone = dsc.panelData[panelByte] - 0x55;
        // if (zone > 0 && zone < maxZones)
        //   getZone(zone - 1)->tamper = true;
        decoded = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        eventstr = lcdLine1.c_str();
        dsc.statusChanged = true;
      }

      if (dsc.panelData[panelByte] >= 0x76 && dsc.panelData[panelByte] <= 0x95)
      {
        lcdLine1 = FC("Zone tamper OFF");
        byte zone = dsc.panelData[panelByte] - 0x75;
        // if (zone > 0 && zone < maxZones)
        //   getZone(zone - 1)->tamper = false;
        decoded = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        eventstr = lcdLine1.c_str();
        dsc.statusChanged = true;
      }

      if (dsc.panelData[panelByte] >= 0x99 && dsc.panelData[panelByte] <= 0xBD)
      {
        lcdLine1 = FC("Armed by");
        byte dscCode = dsc.panelData[panelByte] - 0x98;
        if (dscCode >= 35)
          dscCode += 5;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        publishUserArmingDisarming(userstr, partition);
      }

      if (dsc.panelData[panelByte] >= 0xC0 && dsc.panelData[panelByte] <= 0xE4)
      {
        lcdLine1 = FC("Disarmed by");
        byte dscCode = dsc.panelData[panelByte] - 0xBF;
        if (dscCode >= 35)
          dscCode += 5;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        publishUserArmingDisarming(userstr, partition);
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown_data0");
        lcdLine2 = " ";
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus1(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      switch (dsc.panelData[panelByte])
      {
      case 0x03:
        lcdLine1 = FC("Cross zone");
        lcdLine2 = FC("alarm");
        break;
      case 0x04:
        lcdLine1 = FC("Delinquency");
        lcdLine2 = FC("alarm");
        break;
      case 0x05:
        lcdLine1 = FC("Late to close");
        lcdLine2 = FC(" ");
        break;
        // 0x24 - 0x28: Access codes 33-34, 40-42
      case 0x29:
        lcdLine1 = FC("Download");
        lcdLine2 = FC("forced ans");
        break;
      case 0x2B:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Auto-arm");
        break;
        // 0x2C - 0x4B: Zone battery restored, zones 1-32
        // 0x4C - 0x6B: Zone battery low, zones 1-32
        // 0x6C - 0x8B: Zone fault restored, zones 1-32
        // 0x8C - 0xAB: Zone fault, zones 1-32
      case 0xAC:
        lcdLine1 = FC("Exit inst");
        lcdLine2 = FC("prog");
        break;
      case 0xAD:
        lcdLine1 = FC("Enter inst");
        lcdLine2 = FC("prog");
        break;
      case 0xAE:
        lcdLine1 = FC("Walk test");
        lcdLine2 = FC("end");
        break;
      case 0xAF:
        lcdLine1 = FC("Walk test");
        lcdLine2 = FC("begin");
        break;
        // 0xB0 - 0xCF: Zones bypassed, zones 1-32
      case 0xD0:
        lcdLine1 = FC("Command");
        lcdLine2 = FC("output 4");
        break;
      case 0xD1:
        lcdLine1 = FC("Exit fault");
        lcdLine2 = FC("pre-alert");
        break;
      case 0xD2:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Entry delay");
        break;
      case 0xD3:
        lcdLine1 = FC("Downlook rem");
        lcdLine2 = FC("trig");
        break;
      default:
        decoded = false;
      }
      if (decoded)
        eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      if (dsc.panelData[panelByte] >= 0x24 && dsc.panelData[panelByte] <= 0x28)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x03;
        if (dscCode >= 35)
          dscCode += 5;
        lcdLine1 = FC("User ");
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        ;
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x2C && dsc.panelData[panelByte] <= 0x4B)
      {
        lcdLine1 = FC("Zone bat OK");
        byte zone = dsc.panelData[panelByte] - 43;
        getZone(zone - 1)->battery_low = false;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        dsc.statusChanged = true;
      }

      if (dsc.panelData[panelByte] >= 0x4C && dsc.panelData[panelByte] <= 0x6B)
      {
        lcdLine1 = FC("Zone bat LOW");
        byte zone = dsc.panelData[panelByte] - 75;
        getZone(zone - 1)->battery_low = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        dsc.statusChanged = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x6C && dsc.panelData[panelByte] <= 0x8B)
      {
        lcdLine1 = FC("Zone fault OFF");
        byte zone = dsc.panelData[panelByte] - 107;
        // zoneStatus[zone-1].open=false;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        dsc.statusChanged = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x8C && dsc.panelData[panelByte] <= 0xAB)
      {
        lcdLine1 = FC("Zone fault ON");
        byte zone = dsc.panelData[panelByte] - 139;
        // zoneStatus[zone-1].open=true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        dsc.statusChanged = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xB0 && dsc.panelData[panelByte] <= 0xCF)
      {
        lcdLine1 = FC("Zone bypass ON");
        byte zone = dsc.panelData[panelByte] - 175;
        // zoneStatus[zone-1].bypassed=true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        dsc.statusChanged = true;
        eventstr = lcdLine1.c_str();
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data1");
        lcdLine2 = FC(" ");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus2(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;
      switch (dsc.panelData[panelByte])
      {
      case 0x2A:
        lcdLine1 = FC("Quick exit");
        lcdLine2 = FC(" ");
        break;
      case 0x63:
        lcdLine1 = FC("Keybus fault");
        lcdLine2 = FC("rest");
        break;
      case 0x64:
        lcdLine1 = FC("Keybus fault");
        lcdLine2 = FC(" ");
        break;
      case 0x66:
        lcdLine1 = FC("Zone bypass");
        lcdLine2 = FC("program");
        break;
      case 0x67:
        lcdLine1 = FC("Command");
        lcdLine2 = FC("output 1");
        break;
      case 0x68:
        lcdLine1 = FC("Command");
        lcdLine2 = FC("output 2");
        break;
      case 0x69:
        lcdLine1 = FC("Command");
        lcdLine2 = FC("output 3");
        break;
      case 0x8C:
        lcdLine1 = FC("Cold start");
        lcdLine2 = FC(" ");
        break;
      case 0x8D:
        lcdLine1 = FC("Warm start");
        lcdLine2 = FC(" ");
        break;
      case 0x8E:
        lcdLine1 = FC("Panel factory");
        lcdLine2 = FC("default");
        break;
      case 0x91:
        lcdLine1 = FC("Swinger shutdown");
        lcdLine2 = FC(" ");
        break;
      case 0x93:
        lcdLine1 = FC("Disarmed");
        lcdLine2 = FC("Keyswitch");
        break;
      case 0x96:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Keyswitch");
        break;
      case 0x97:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Keypad away");
        break;
      case 0x98:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Quick-arm");
        break;
      case 0x99:
        lcdLine1 = FC("Activate");
        lcdLine2 = FC("stay/away zones");
        break;
      case 0x9A:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Stay");
        break;
      case 0x9B:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("Away");
        break;
      case 0x9C:
        lcdLine1 = FC("Armed");
        lcdLine2 = FC("No ent del");
        break;
        // 0x9E - 0xC2: *1: Access_codes 1-34, 40-42
        // 0xC3 - 0xC5: *5: Access_codes 40-42
        // 0xC6 - 0xE5: Access_codes 1-34, 40-42
        // 0xE6 - 0xE8: *6: Access_codes 40-42
        // 0xE9 - 0xF0: Keypad restored: Slots 1-8
        // 0xF1 - 0xF8: Keypad trouble: Slots 1-8
        // 0xF9 - 0xFE: Zone expander restored: 1-6
      case 0xFF:
        lcdLine1 = FC("Zone exp");
        lcdLine2 = FC("trble:1");
        break;
      default:
        decoded = false;
      }
      if (decoded)
        eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      char charBuffer[5];
      if (dsc.panelData[panelByte] >= 0x67 && dsc.panelData[panelByte] <= 0x69)
      {
        lcdLine1 = FC("Cmd O/P");
        byte zone = dsc.panelData[panelByte] - 0x66;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x9E && dsc.panelData[panelByte] <= 0xC2)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x9D;
        lcdLine1 = FC("[*1] by");
        if (dscCode >= 35)
          dscCode += 5;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xC3 && dsc.panelData[panelByte] <= 0xC5)
      {
        byte dscCode = dsc.panelData[panelByte] - 0xA0;
        lcdLine1 = FC("[*5] by");
        if (dscCode >= 35)
          dscCode += 5;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xC6 && dsc.panelData[panelByte] <= 0xE5)
      {
        byte dscCode = dsc.panelData[panelByte] - 0xC5;
        if (dscCode >= 35)
          dscCode += 5;
        lcdLine1 = FC("User");
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xE6 && dsc.panelData[panelByte] <= 0xE8)
      {
        byte dscCode = dsc.panelData[panelByte] - 0xC3;
        lcdLine1 = FC("[*6] by");
        if (dscCode >= 35)
          dscCode += 5;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xE9 && dsc.panelData[panelByte] <= 0xF0)
      {
        lcdLine1 = FC("Keypad slot ok");
        itoa(dsc.panelData[panelByte] - 232, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xF1 && dsc.panelData[panelByte] <= 0xF8)
      {
        lcdLine1 = FC("Keypad slot trbl");
        itoa(dsc.panelData[panelByte] - 240, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xF9 && dsc.panelData[panelByte] <= 0xFE)
      {
        lcdLine1 = FC("Zone exp ok");
        itoa(dsc.panelData[panelByte] - 248, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data2");
        lcdLine2 = " ";
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus3(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;
      switch (dsc.panelData[panelByte])
      {
      case 0x05:
        lcdLine1 = FC("PC/RF5132:");
        lcdLine2 = FC("Suprvis. rest");
        break;
      case 0x06:
        lcdLine1 = FC("PC/RF5132:");
        lcdLine2 = FC("Suprvis. trble");
        break;
      case 0x09:
        lcdLine1 = FC("PC5204:");
        lcdLine2 = FC("Suprvis. rest");
        break;
      case 0x0A:
        lcdLine1 = FC("PC5204:");
        lcdLine2 = FC("Suprvis. trble");
        break;
      case 0x17:
        lcdLine1 = FC("Zone exp 7");
        lcdLine2 = FC("rest");
        break;
      case 0x18:
        lcdLine1 = FC("Zone exp 7");
        lcdLine2 = FC("trble");
        break;
        // 0x25 - 0x2C: Keypad tamper restored, slots 1-8
        // 0x2D - 0x34: Keypad tamper, slots 1-8
        // 0x35 - 0x3A: Module tamper restored, slots 9-14
        // 0x3B - 0x40: Module tamper, slots 9-14
      case 0x41:
        lcdLine1 = FC("PC/RF5132:");
        lcdLine2 = FC("Tamper rest");
        break;
      case 0x42:
        lcdLine1 = FC("PC/RF5132: Tamper");
        lcdLine2 = FC(" ");
        break;
      case 0x43:
        lcdLine1 = FC("PC5208: Tamper");
        lcdLine2 = FC("rest");
        break;
      case 0x44:
        lcdLine1 = FC("PC5208: Tamper");
        lcdLine2 = FC(" ");
        break;
      case 0x45:
        lcdLine1 = FC("PC5204: Tamper");
        lcdLine2 = FC("rest");
        break;
      case 0x46:
        lcdLine1 = FC("PC5204: Tamper");
        lcdLine2 = FC(" ");
        break;
      case 0x51:
        lcdLine1 = FC("Zone exp 7");
        lcdLine2 = FC("tamper rest");
        break;
      case 0x52:
        lcdLine1 = FC("Zone exp 7");
        lcdLine2 = FC("tamper");
        break;
      case 0xB3:
        lcdLine1 = FC("PC5204:");
        lcdLine2 = FC("Bat rest");
        break;
      case 0xB4:
        lcdLine1 = FC("PC5204:");
        lcdLine2 = FC("Bat trble");
        break;
      case 0xB5:
        lcdLine1 = FC("PC5204: Aux");
        lcdLine2 = FC("sup rest");
        break;
      case 0xB6:
        lcdLine1 = FC("PC5204: Aux");
        lcdLine2 = FC("sup trble");
        break;
      case 0xB7:
        lcdLine1 = FC("PC5204: Out 1");
        lcdLine2 = FC("rest");
        break;
      case 0xB8:
        lcdLine1 = FC("PC5204: Out 1");
        lcdLine2 = FC("trble");
        break;
      case 0xFF:
        lcdLine1 = FC("Ext status");
        lcdLine2 = FC(" ");
        break;
      default:
        decoded = false;
      }

      char charBuffer[5];
      if (decoded)
        eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      if (dsc.panelData[panelByte] <= 0x04)
      {
        lcdLine1 = FC("Zone exp trouble");
        itoa(dsc.panelData[panelByte] + 2, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }
      if (dsc.panelData[panelByte] >= 0x25 && dsc.panelData[panelByte] <= 0x2C)
      {
        lcdLine1 = FC("keypad tamper off");
        itoa(dsc.panelData[panelByte] - 0x24, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }
      if (dsc.panelData[panelByte] >= 0x2D && dsc.panelData[panelByte] <= 0x34)
      {
        lcdLine1 = FC("keypad tamper on");
        itoa(dsc.panelData[panelByte] - 0x2c, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = dsc.panelData[panelByte] - 0x2c;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x35 && dsc.panelData[panelByte] <= 0x3A)
      {
        lcdLine1 = FC("Zone exp tamper off");
        itoa(dsc.panelData[panelByte] - 52, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x3B && dsc.panelData[panelByte] <= 0x40)
      {
        lcdLine1 = FC("Zone exp tamper on");
        itoa(dsc.panelData[panelByte] - 58, charBuffer, 10);
        lcdLine2 = charBuffer;
        decoded = true;
        zonestr = charBuffer;
        eventstr = lcdLine1.c_str();
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data3");
        lcdLine2 = FC(" ");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus4(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;
      switch (dsc.panelData[panelByte])
      {
      case 0x86:
        lcdLine1 = FC("Period test");
        lcdLine2 = FC("trble");
        break;
      case 0x87:
        lcdLine1 = FC("Exit fault");
        lcdLine2 = FC(" ");
        break;
      case 0x89:
        lcdLine1 = FC("Alarm cancel");
        lcdLine2 = FC(" ");
        break;
      default:
        decoded = false;
      }

      if (decoded)
        eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      if (dsc.panelData[panelByte] <= 0x1F)
      {
        lcdLine1 = FC("Zone alarm on");
        byte zone = dsc.panelData[panelByte] + 33;
        if (zone > 0 && zone < maxZones)
          getZone(zone - 1)->alarm = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        publishZoneAlarm(zonestr, partition);
        dsc.statusChanged = true;
      }
      else if (dsc.panelData[panelByte] >= 0x20 && dsc.panelData[panelByte] <= 0x3F)
      {
        lcdLine1 = FC("Zone alarm off");
        byte zone = dsc.panelData[panelByte] + 1;
        //   if (zone > 0 && zone < maxZones)
        //    zoneStatus[zone-1].alarm=false;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }
      else if (dsc.panelData[panelByte] >= 0x40 && dsc.panelData[panelByte] <= 0x5F)
      {
        lcdLine1 = FC("Zone tamper on");
        byte zone = dsc.panelData[panelByte] - 31;
        // if (zone > 0 && zone < maxZones)
        // getZone(zone - 1)->tamper = true;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        dsc.statusChanged = true;
      }
      else if (dsc.panelData[panelByte] >= 0x60 && dsc.panelData[panelByte] <= 0x7F)
      {
        lcdLine1 = FC("Tamper zone off");
        byte zone = dsc.panelData[panelByte] - 63;
        //   if (zone > 0 && zone < maxZones)
        // getZone(zone - 1)->tamper = false;
        zonestr = getZoneName(zone, false);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        dsc.statusChanged = true;
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data4");
        lcdLine2 = FC(" ");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus5(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      if (dsc.panelData[panelByte] <= 0x39)
      {
        byte dscCode = dsc.panelData[panelByte] + 0x23;
        lcdLine1 = FC("Armed by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        publishUserArmingDisarming(userstr, partition);
      }

      if (dsc.panelData[panelByte] >= 0x3A && dsc.panelData[panelByte] <= 0x73)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x17;
        lcdLine1 = FC("Disarmed by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        publishUserArmingDisarming(userstr, partition);
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data5");
        lcdLine2 = FC(" ");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus14(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = false;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      switch (dsc.panelData[panelByte])
      {
      // 0x40 - 0x5F: Zone fault restored, zones 33-64
      // 0x60 - 0x7F: Zone fault, zones 33-64
      // 0x80 - 0x9F: Zone bypassed, zones 33-64
      case 0xC0:
        lcdLine1 = FC("TLink");
        lcdLine2 = FC("com fault");
        decoded = true;
        break;
      case 0xC2:
        lcdLine1 = FC("Tlink");
        lcdLine2 = FC("net fault");
        decoded = true;
        break;
      case 0xC4:
        lcdLine1 = FC("TLink rec");
        lcdLine2 = FC("trouble");
        decoded = true;
        break;
      case 0xC5:
        lcdLine1 = FC("TLink receiver");
        lcdLine2 = FC("restored");
        decoded = true;
        break;
      default:
        break;
      }

      eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      if (dsc.panelData[panelByte] >= 0x40 && dsc.panelData[panelByte] <= 0x5F)
      {
        byte dscCode = dsc.panelData[panelByte] - 31;
        lcdLine1 = FC("Zone restored");
        zonestr = getZoneName(dscCode, true);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }
      /*
       *  Zone fault, zones 33-64
       *
       *  Command   Partition YYY1YYY2   MMMMDD DDDHHHHH MMMMMM             Status             CRC
       *  11101011 0 00000000 00100001 00010110 11000000 00100100 00010100 01111111 11111111 10011000 [0xEB] 2021.05.22 00:09 | Zone fault: 64
       *  11101100 0 00000000 00100001 00010110 11000000 00100100 00010100 01111111 00001011 10100101 [0xEC] Event: 011 | 2021.05.22 00:09 | Zone fault: 64
       *  Byte 0   1    2        3        4        5        6        7        8        9        10
       */
      if (dsc.panelData[panelByte] >= 0x60 && dsc.panelData[panelByte] <= 0x7F)
      {
        lcdLine1 = FC("Zone fault on");
        byte dscCode = dsc.panelData[panelByte] - 63;
        zonestr = getZoneName(dscCode, true);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      /*
       *  Zones bypassed, zones 33-64
       *
       *  Command   Partition YYY1YYY2   MMMMDD DDDHHHHH MMMMMM             Status             CRC
       *  11101011 0 00000001 00100001 00010110 11000000 00011000 00010100 10011111 00000000 10101110 [0xEB] 2021.05.22 00:06 | Partition 1 | Zone bypassed: 64
       *  11101100 0 00000001 00100001 00010110 11000000 00011000 00010100 10011111 00010011 11000010 [0xEC] Event: 019 | 2021.05.22 00:06 | Partition 1 | Zone bypassed: 64
       *  Byte 0   1    2        3        4        5        6        7        8        9        10
       */
      if (dsc.panelData[panelByte] >= 0x80 && dsc.panelData[panelByte] <= 0x9F)
      {
        lcdLine1 = FC("Zone bypass on");
        byte dscCode = dsc.panelData[panelByte] - 95;
        zonestr = getZoneName(dscCode, true);
        lcdLine2 = zonestr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        ;
      }
      if (!decoded)
      {
        lcdLine1 = FC("Unknown data14");
        lcdLine2 = FC(" ");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }
    long int DSCkeybushome::toInt(std::string s, int base)
    {
      if (s.empty() || std::isspace(s[0]))
        return 0;
      char *p;
      long int li = strtol(s.c_str(), &p, base);
      return li;
    }

    void DSCkeybushome::printPanelStatus16(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      switch (dsc.panelData[panelByte])
      {
      case 0x80:
        lcdLine1 = FC("Trouble");
        lcdLine2 = FC("ack");
        break;
      case 0x81:
        lcdLine1 = FC("RF delin");
        lcdLine2 = FC("trouble");

        break;
      case 0x82:
        lcdLine1 = FC("RF delin");
        lcdLine2 = FC("rest");

        break;
      default:
        decoded = false;
        break;
      }
      eventstr = std::string(lcdLine1.c_str()) + std::string(" ") + std::string(lcdLine2.c_str());

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data16");
        lcdLine2 = FC("");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus17(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      if (dsc.panelData[panelByte] >= 0x4A && dsc.panelData[panelByte] <= 0x83)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x27;
        lcdLine1 = FC("[*1] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] <= 0x24)
      {
        byte dscCode = dsc.panelData[panelByte] + 1;
        lcdLine1 = FC("[*2] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x84 && dsc.panelData[panelByte] <= 0xBD)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x61;
        lcdLine1 = FC("[*2] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x25 && dsc.panelData[panelByte] <= 0x49)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x24;
        lcdLine1 = FC("[*3] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0xBE && dsc.panelData[panelByte] <= 0xF7)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x9B;
        lcdLine1 = FC("[*3] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data17");
        lcdLine2 = FC("");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus18(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;

      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      if (dsc.panelData[panelByte] <= 0x39)
      {
        byte dscCode = dsc.panelData[panelByte] + 0x23;
        if (dscCode >= 40)
          dscCode += 3;
        lcdLine1 = FC("User ");
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x3A && dsc.panelData[panelByte] <= 0x95)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x39;
        lcdLine1 = FC("[*5] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (dsc.panelData[panelByte] >= 0x96 && dsc.panelData[panelByte] <= 0xF1)
      {
        byte dscCode = dsc.panelData[panelByte] - 0x95;
        lcdLine1 = FC("[*6] by");
        if (dscCode >= 40)
          dscCode += 3;
        userstr = getUserName(dscCode, false, true);
        lcdLine2 = userstr.c_str();
        decoded = true;
        eventstr = lcdLine1.c_str();
      }

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data18");
        lcdLine2 = FC("");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::printPanelStatus1B(byte panelByte, byte partition, bool showEvent)
    {
      bool decoded = true;
      std::string lcdLine1;
      std::string lcdLine2;
      std::string eventstr, userstr, zonestr, statusstr, zonenamestr;

      switch (dsc.panelData[panelByte])
      {
      case 0xF1:
        lcdLine1 = FC("System reset ");
        lcdLine2 = FC("trans");
        break;
      default:
        decoded = false;
      }
      eventstr = lcdLine1 + " " + lcdLine2;

      if (!decoded)
      {
        lcdLine1 = FC("Unknown data1b");
        lcdLine2 = FC("");
        eventstr = "unknown data";
      }

      if (showEvent)
      {
#ifdef USE_JSON_EVENT
        toLower(&eventstr);
        eventStatusMsg.append(FC("'event':'")).append(eventstr).append(FC("','user':'")).append(userstr).append(FC("','zone':'")).append(zonestr).append("'");
#else
    eventStatusMsg.append(eventstr);
    if (userstr != "")
      eventStatusMsg.append(" ").append(userstr);
    if (zonestr != "")
      eventStatusMsg.append(FC(" for ")).append(zonestr);
#endif
      }
      else
        publishLine2(lcdLine1 + " " + lcdLine2, activePartition);
    }

    void DSCkeybushome::toLower(std::string *s)
    {
      for (auto &c : *s)
      {
        c = tolower(c);
      }
    }

    byte DSCkeybushome::maxPartitions()
    {
      byte p = 0;
      for (byte partition = 0; partition < dscPartitions; partition++)
      {
        if (!dsc.disabled[partition])
          p++;
      }
      return p;
    }

#if !defined(ARDUINO_MQTT)
    void DSCkeybushome::loadZones()
    {

      for (binary_sensor::BinarySensor *obj : App.get_binary_sensors())
      {
        createZoneFromObj(obj);
      }
    }

    void DSCkeybushome::createZoneFromObj(binary_sensor::BinarySensor *obj, uint8_t p)
    {
      MatchState ms;
      char buf[20];
      char res;
      ms.Target((char *)obj->get_object_id().c_str());
      res = ms.Match("^[zZ](%d+)$");
      if (res == REGEXP_MATCHED)
      {
        ms.GetCapture(buf, 0);
        int z = toInt(buf, 10);
        if (!z)
          return;
        zoneType *zt = getZone(z - 1);
        if (zt->zone == z) {
          if (zt->binary_sensor==NULL)
            zt->binary_sensor=obj;
          if (!zt->partition)
            zt->partition=p;
          return;
        }
        zoneType n = zonetype_INIT;
        n.zone = z;
        n.binary_sensor = obj;
        n.enabled = true;
        n.partition = p;
        zoneStatus.push_back(n);
        ESP_LOGD(TAG, "CreatefromOjb: added zone %d", zoneStatus.back().zone);
      }

    }

#endif

 DSCkeybushome::zoneType * DSCkeybushome::createZone(uint16_t z, uint8_t p)
{

  if (!z)
    return &zonetype_INIT;
  zoneType n = zonetype_INIT;
  n.zone = z;
  n.enabled = true;
  n.partition = p;

  zoneStatus.push_back(n);
  ESP_LOGD(TAG, "createzone: added zone %d", zoneStatus.back().zone);
  return  &zoneStatus.back();
}


    const __FlashStringHelper *DSCkeybushome::statusText(uint8_t statusCode)
    {
      switch (statusCode)
      {
      case 0x01:
        return F("Ready");
      case 0x02:
        return F("Stay zones open");
      case 0x03:
        return F("Zones open");
      case 0x04:
        return F("Armed stay");
      case 0x05:
        return F("Armed away");
      case 0x06:
        return F("No entry delay");
      case 0x07:
        return F("Failed to arm");
      case 0x08:
        return F("Exit delay");
      case 0x09:
        return F("No entry delay");
      case 0x0B:
        return F("Quick exit");
      case 0x0C:
        return F("Entry delay");
      case 0x0D:
        return F("Alarm memory");
      case 0x10:
        return F("Keypad lockout");
      case 0x11:
        return F("Alarm");
      case 0x14:
        return F("Auto-arm");
      case 0x15:
        return F("Arm with bypass");
      case 0x16:
        return F("No entry delay");
      case 0x17:
        return F("Power failure"); //??? not sure
      case 0x22:
        return F("Alarm memory");
      case 0x33:
        return F("Busy");
      case 0x3D:
        return F("Disarmed");
      case 0x3E:
        return F("Disarmed");
      case 0x40:
        return F("Keypad blanked");
      case 0x8A:
        return F("Activate zones");
      case 0x8B:
        return F("Quick exit");
      case 0x8E:
        return F("Invalid option");
      case 0x8F:
        return F("Invalid code");
      case 0x9E:
        return F("Enter * code");
      case 0x9F:
        return F("Access_code");
      case 0xA0:
        return F("Zone bypass");
      case 0xA1:
        return F("Trouble menu");
      case 0xA2:
        return F("Alarm memory");
      case 0xA3:
        return F("Door chime on");
      case 0xA4:
        return F("Door chime off");
      case 0xA5:
        return F("Master code");
      case 0xA6:
        return F("Access codes");
      case 0xA7:
        return F("Enter new code");
      case 0xA9:
        return F("User function");
      case 0xAA:
        return F("Time and Date");
      case 0xAB:
        return F("Auto-arm time");
      case 0xAC:
        return F("Auto-arm on");
      case 0xAD:
        return F("Auto-arm off");
      case 0xAF:
        return F("System test");
      case 0xB0:
        return F("Enable DLS");
      case 0xB2:
        return F("Command output");
      case 0xB7:
        return F("Installer code");
      case 0xB8:
        return F("Enter * code");
      case 0xB9:
        return F("Zone tamper");
      case 0xBA:
        return F("Zones low batt.");
      case 0xC6:
        return F("Zone fault menu");
      case 0xC8:
        return F("Service required");
      case 0xD0:
        return F("Keypads low batt");
      case 0xD1:
        return F("Wireless low bat");
      case 0xE4:
        return F("Installer menu");
      case 0xE5:
        return F("Keypad slot");
      case 0xE6:
        return F("Input: 2 digits");
      case 0xE7:
        return F("Input: 3 digits");
      case 0xE8:
        return F("Input: 4 digits");
      case 0xEA:
        return F("Code: 2 digits");
      case 0xEB:
        return F("Code: 4 digits");
      case 0xEC:
        return F("Input: 6 digits");
      case 0xED:
        return F("Input: 32 digits");
      case 0xEE:
        return F("Input: option");
      case 0xF0:
        return F("Function key 1");
      case 0xF1:
        return F("Function key 2");
      case 0xF2:
        return F("Function key 3");
      case 0xF3:
        return F("Function key 4");
      case 0xF4:
        return F("Function key 5");
      case 0xF8:
        return F("Keypad program");
      case 0xFF:
        return F("Disabled");
      default:
        return F("Unknown");
      }
    }

    /*
    #if defined(AUTOPOPULATE)
    void DSCkeybushome::loadZone(int z) {
        std::string n=std::to_string(z);
        std::string type_id="z" + n;
        std::string name="Zone " + n;
        auto it = std::find_if(bMap.begin(), bMap.end(),  [&type_id](binary_sensor::BinarySensor* f){ return f->get_object_id() == type_id; } );
        if (it != bMap.end()) return;

        template_alarm_::TemplateBinarySensor * ptr = new template_alarm_::TemplateBinarySensor();
        App.register_binary_sensor(ptr);

        ptr->set_name("Zone 19");
        ptr->set_object_id("z19");

    //ESP_LOGD(TAG,"get name=%s,get object_id=%s, get typeid=%s,",ptr->get_name().c_str(),ptr->get_object_id().c_str(),ptr->get_type_id().c_str());

       // bst->ptr->set_device_class("window");
        ptr->set_publish_initial_state(true);
        ptr->set_disabled_by_default(false);
    #if defined(ESPHOME_MQTT)
        mqtt::MQTTBinarySensorComponent * mqptr=new mqtt::MQTTBinarySensorComponent(ptr);
        mqptr->set_component_source("mqtt");
        App.register_component(mqptr);
        mqptr->call();
    #endif
        ptr->set_component_source("template_alarm.binary_sensor");
        App.register_component(ptr);
        ptr->call();
        gptr=ptr;

    }
    #endif
    */

#if !defined(ARDUINO_MQTT)
  }
} // namespaces
#endif