#pragma once
#include "list_entities.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/mg_lib/mongoose.h"
#include <vector>
#include <Crypto.h>

#ifdef USE_ESP32
#include <deque>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif


#if USE_WEBKEYPAD_VERSION >= 2
extern const uint8_t ESPHOME_WEBKEYPAD_INDEX_HTML[] PROGMEM;
extern const size_t ESPHOME_WEBKEYPAD_INDEX_HTML_SIZE;
#endif

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
extern const uint8_t ESPHOME_WEBKEYPAD_CSS_INCLUDE[] PROGMEM;
extern const size_t ESPHOME_WEBKEYPAD_CSS_INCLUDE_SIZE;
#endif

#ifdef USE_WEBKEYPAD_JS_INCLUDE
extern const uint8_t ESPHOME_WEBKEYPAD_JS_INCLUDE[] PROGMEM;
extern const size_t ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE;
#endif



namespace esphome {
namespace web_keypad {

#define KEYSIZE 32

extern void * webServerPtr;
/// Internal helper struct that is used to parse incoming URLs
/*
struct UrlMatch {
  std::string domain;  ///< The domain of the component, for example "sensor"
  std::string id;      ///< The id of the device that's being accessed, for example "living_room_fan"
  std::string method;  ///< The method that's being called, for example "turn_on"
  bool valid;          ///< Whether this match is valid
};
*/
enum msgType {
  STATE = 0,
  LOG,
  CONFIG,
  PING ,
  OTA
};

struct SortingComponents {
  float weight;
  uint64_t group_id;
};

struct SortingGroup {
  std::string name;
  float weight;
};

struct Credentials {
  std::string username="";
  std::string password="";
  uint8_t token[KEYSIZE];
  uint8_t hmackey[KEYSIZE];
  bool crypt=false;
};

struct upload_state {
  size_t expected;  // POST data length, bytes
  size_t received;  // Already received bytes
  String fn;
};

#define SALT "77992288"
enum JsonDetail { DETAIL_ALL, DETAIL_STATE };

/** This class allows users to create a web server with their ESP nodes.
 *
 * Behind the scenes it's using AsyncWebServer to set up the server. It exposes 3 things:
 * an index page under '/' that's used to show a simple web interface (the css/js is hosted
 * by esphome.io by default), an event source under '/events' that automatically sends
 * all state updates in real time + the debug log. Lastly, there's an REST API available
 * under the '/light/...', '/sensor/...', ... URLs. A full documentation for this API
 * can be found under https://esphome.io/web-api/index.html.
 */
class WebServer : public Controller, public Component {
 public:
  WebServer();


#ifdef USE_WEBKEYPAD_CSS_INCLUDE
  /** Set local path to the script that's embedded in the index page. Defaults to
   *
   * @param css_include Local path to web server script.
   */
  void set_css_include(const char *css_include);
#endif

#ifdef USE_WEBKEYPAD_JS_INCLUDE
  /** Set local path to the script that's embedded in the index page. Defaults to
   *
   * @param js_include Local path to web server script.
   */
  void set_js_include(const char *js_include);
#endif

  /** Determine whether internal components should be displayed on the web server.
   * Defaults to false.
   *
   * @param include_internal Whether internal components should be displayed.
   */
  void set_include_internal(bool include_internal) { include_internal_ = include_internal; }
  /** Set whether or not the webserver should expose the OTA form and handler.
   *
   * @param allow_ota.
   */
  void set_allow_ota(bool allow_ota) { this->allow_ota_ = allow_ota; }
  /** Set whether or not the webserver should expose the Log.
   *
   * @param expose_log.
   */

 // std::map<mg_connection *,std::string> sessionTokens;
  Credentials credentials_;
  using key_service_t = std::function<void(std::string,int)>;
  optional<key_service_t> key_service_func_{}; 
  
  void set_partitions(uint8_t partitions) { this->partitions_=partitions;}
  void set_expose_log(bool expose_log) { this->expose_log_ = expose_log; }
  void set_show_keypad(bool show_keypad) { this->show_keypad_ = show_keypad; }  
  void set_keypad_config(const char *  json_keypad_config);
  void set_port(uint8_t port) { this->port_=port;}
  
  void set_certificate(const char * cert) { certificate_ = cert;
      // certificate_.erase(std::remove(certificate_.begin(),certificate_.end(),'\n'),certificate_.end());
  } 
  
  void set_certificate_key(const char * cert_key) { certificate_key_ = cert_key; 
         //certificate_key_.erase(std::remove(certificate_key_.begin(),certificate_key_.end(),'\n'),certificate_key_.end());
  } 
  const char * get_certificate() { return certificate_; } 
  const char *get_certificate_key() { return certificate_key_; } 
  void set_service_lambda(const key_service_t & lambda) { 
   this->key_service_func_ = lambda;
  }

  void set_auth(const std::string & auth_username,const std::string & auth_password,bool use_encryption) { 
  credentials_.username = auth_username;   
  credentials_.password = auth_password;
  
  const char * keystr=(credentials_.username + SALT + credentials_.password).c_str();

    SHA256HMAC aeskey((const byte*)keystr,strlen(keystr));
    aeskey.doUpdate("aeskey");
    aeskey.doFinal(credentials_.token);
    
    SHA256HMAC hmac((const byte*)keystr,strlen(keystr));
    hmac.doUpdate("hmackey");
    hmac.doFinal(credentials_.hmackey);
   
    
   this->crypt_ = use_encryption;  
   credentials_.crypt=use_encryption;

   }  
   
  Credentials * get_credentials() { return &credentials_;}
  bool handleUpload(size_t bodylen,  const String &filename, size_t index,uint8_t *data, size_t len, bool final);
  const std::string encrypt(const char * message);
  const std::string decrypt(DynamicJsonDocument&  doc);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup the internal web server and register handlers.
  void setup() override;
  void loop() override;

  void dump_config() override;

  /// MQTT setup priority.
  float get_setup_priority() const override;

  /// Handle an index request under '/'.
  void handle_index_request(struct mg_connection *c);

  /// Return the webserver configuration as JSON.
  const std::string get_config_json(unsigned long c=0);
  const std::string escape_json(const char *s);
  
  long int toInt(const std::string &s, int base); 

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
  /// Handle included css request under '/0.css'.
  void handle_css_request(struct mg_connection *c);
#endif

#ifdef USE_WEBKEYPAD_JS_INCLUDE
  /// Handle included js request under '/0.js'.
  void handle_js_request(struct mg_connection *c);
#endif

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
  // Handle Private Network Access CORS OPTIONS request
  void handle_pna_cors_request(struct mg_connection *c);
#endif

#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj, float state) override;
  /// Handle a sensor request under '/sensor/<id>'.
  void handle_sensor_request(struct mg_connection *c, JsonObject doc);

  /// Dump the sensor state with its value as a JSON string.
  std::string sensor_json(sensor::Sensor *obj, float value, JsonDetail start_config);
#endif

#ifdef USE_EVENT
  void on_event(event::Event *obj, const std::string &event_type) override;

  /// Handle a event request under '/event<id>'.
  void handle_event_request(struct mg_connection *c, JsonObject doc);

  /// Dump the event details with its value as a JSON string.
  std::string event_json(event::Event *obj, const std::string &event_type, JsonDetail start_config);
#endif

#ifdef USE_UPDATE
  void on_update(update::UpdateEntity *obj) override;

  /// Handle a update request under '/update/<id>'.
  void handle_update_request(struct mg_connection *c, JsonObject doc);

  /// Dump the update state with its value as a JSON string.
  std::string update_json(update::UpdateEntity *obj, JsonDetail start_config);
#endif

#ifdef USE_VALVE
  void on_valve_update(valve::Valve *obj) override;

  /// Handle a valve request under '/valve/<id>/<open/close/stop/set>'.
  void handle_valve_request(struct mg_connection *c, JsonObject doc);

  /// Dump the valve state as a JSON string.
  std::string valve_json(valve::Valve *obj, JsonDetail start_config);
#endif

#ifdef USE_DATETIME_DATE
  void on_date_update(datetime::DateEntity *obj) override;
  /// Handle a date request under '/date/<id>'.
  void handle_date_request(struct mg_connection *c, JsonObject doc);

  /// Dump the date state with its value as a JSON string.
  std::string date_json(datetime::DateEntity *obj, JsonDetail start_config);
#endif

#ifdef USE_DATETIME_TIME
  void on_time_update(datetime::TimeEntity *obj) override;
  /// Handle a time request under '/time/<id>'.
  void handle_time_request(struct mg_connection *c, JsonObject doc);

  /// Dump the time state with its value as a JSON string.
  std::string time_json(datetime::TimeEntity *obj, JsonDetail start_config);
#endif

#ifdef USE_DATETIME_DATETIME
  void on_datetime_update(datetime::DateTimeEntity *obj) override;
  /// Handle a datetime request under '/datetime/<id>'.
  void handle_datetime_request(struct mg_connection *c, JsonObject doc);

  /// Dump the datetime state with its value as a JSON string.
  std::string datetime_json(datetime::DateTimeEntity *obj, JsonDetail start_config);
#endif


#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj, bool state) override;

  /// Handle a switch request under '/switch/<id>/</turn_on/turn_off/toggle>'.
  void handle_switch_request(struct mg_connection *c, JsonObject doc);

  /// Dump the switch state with its value as a JSON string.
  std::string switch_json(switch_::Switch *obj, bool value, JsonDetail start_config);
#endif

#ifdef USE_BUTTON
  /// Handle a button request under '/button/<id>/press'.
  void handle_button_request(struct mg_connection *c, JsonObject doc);

  /// Dump the button details with its value as a JSON string.
  std::string button_json(button::Button *obj, JsonDetail start_config);
#endif

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) override;

  /// Handle a binary sensor request under '/binary_sensor/<id>'.
  void handle_binary_sensor_request(struct mg_connection *c, JsonObject doc);

  /// Dump the binary sensor state with its value as a JSON string.
  std::string binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config);
#endif

#ifdef USE_FAN
  void on_fan_update(fan::Fan *obj) override;

  /// Handle a fan request under '/fan/<id>/</turn_on/turn_off/toggle>'.
  void handle_fan_request(struct mg_connection *c, JsonObject doc);

  /// Dump the fan state as a JSON string.
  std::string fan_json(fan::Fan *obj, JsonDetail start_config);
#endif

#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;

  /// Handle a light request under '/light/<id>/</turn_on/turn_off/toggle>'.
  void handle_light_request(struct mg_connection *c, JsonObject doc);

  /// Dump the light state as a JSON string.
  std::string light_json(light::LightState *obj, JsonDetail start_config);
#endif

#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) override;

  /// Handle a text sensor request under '/text_sensor/<id>'.
  void handle_text_sensor_request(struct mg_connection *c, JsonObject doc);

  /// Dump the text sensor state with its value as a JSON string.
  std::string text_sensor_json(text_sensor::TextSensor *obj, const std::string &value, JsonDetail start_config);
#endif

#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;

  /// Handle a cover request under '/cover/<id>/<open/close/stop/set>'.
  void handle_cover_request(struct mg_connection *c, JsonObject doc);

  /// Dump the cover state as a JSON string.
  std::string cover_json(cover::Cover *obj, JsonDetail start_config);
#endif

#ifdef USE_NUMBER
  void on_number_update(number::Number *obj, float state) override;
  /// Handle a number request under '/number/<id>'.
  void handle_number_request(struct mg_connection *c, JsonObject doc);

  /// Dump the number state with its value as a JSON string.
  std::string number_json(number::Number *obj, float value, JsonDetail start_config);
#endif



void handle_auth_request(mg_connection *c,JsonObject doc);
void handle_alarm_panel_request(struct mg_connection *c, JsonObject doc);


#ifdef USE_TEXT
  void on_text_update(text::Text *obj, const std::string &state) override;
  /// Handle a text input request under '/text/<id>'.
  void handle_text_request(struct mg_connection *c, JsonObject doc);

  /// Dump the text state with its value as a JSON string.
  std::string text_json(text::Text *obj, const std::string &value, JsonDetail start_config);
#endif

#ifdef USE_SELECT
  void on_select_update(select::Select *obj, const std::string &state, size_t index) override;
  /// Handle a select request under '/select/<id>'.
  void handle_select_request(struct mg_connection *c, JsonObject doc);

  /// Dump the select state with its value as a JSON string.
  std::string select_json(select::Select *obj, const std::string &value, JsonDetail start_config);
#endif

#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
  /// Handle a climate request under '/climate/<id>'.
  void handle_climate_request(struct mg_connection *c, JsonObject doc);

  /// Dump the climate details
  std::string climate_json(climate::Climate *obj, JsonDetail start_config);
#endif

#ifdef USE_LOCK
  void on_lock_update(lock::Lock *obj) override;

  /// Handle a lock request under '/lock/<id>/</lock/unlock/open>'.
  void handle_lock_request(struct mg_connection *c, JsonObject doc);

  /// Dump the lock state with its value as a JSON string.
  std::string lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config);
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) override;

  /// Handle a alarm_control_panel request under '/alarm_control_panel/<id>'.
  void handle_alarm_control_panel_request(struct mg_connection *c, JsonObject doc);

  /// Dump the alarm_control_panel state with its value as a JSON string.
  std::string alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                       alarm_control_panel::AlarmControlPanelState value, JsonDetail start_config);
#endif
  struct mg_mgr mgr;
  struct mg_connection *c;
  friend ListEntitiesIterator;  
  ListEntitiesIterator entities_iterator_;
void push(msgType mt, const char *data,uint32_t id = 0,uint32_t reconnect = 0);
bool callKeyService(const char *buf,int partition);
void report_ota_error();
#define MATCH_BUF_SIZE 60
static char matchBuf[MATCH_BUF_SIZE];
static uint8_t matchIndex;
void handleRequest(struct mg_connection *c,JsonObject doc) ;
void handleWebRequest(struct mg_connection *c,mg_http_message *hm);
static void ev_handler(struct mg_connection *nc, int ev, void *p);
void parseUrl(mg_http_message *hm,JsonObject doc) ;
void parseUrlParams(char *queryString, int resultsMaxCt, boolean decodeUrl,JsonObject doc);
void ws_reply(mg_connection *c,const char * data,bool ok);
void add_entity_config(EntityBase *entity, float weight, uint64_t group);
void add_sorting_group(uint64_t group_id, const std::string &group_name, float weight);

 protected:
 
  const char * certificate_;
  const char * certificate_key_;
  uint32_t last_ota_progress_{0};
  uint32_t ota_read_length_{0}; 
  void schedule_(std::function<void()> &&f);
#ifdef ASYNCWEB 
static void webPollTask(void * args);
#endif
  bool firstrun_{true};
  const char * _json_keypad_config;

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
  const char *css_include_{nullptr};
#endif
#ifdef USE_WEBKEYPAD_JS_INCLUDE
  const char *js_include_{nullptr};
#endif
  bool include_internal_{false};
  bool allow_ota_{false};
  bool expose_log_{false};
  uint8_t partitions_{1};
  uint8_t port_{80};
  bool show_keypad_{true};
  bool crypt_{false};
  void percentDecode(char *src);
#ifdef USE_ESP32
  std::deque<std::function<void()>> to_schedule_;
  SemaphoreHandle_t to_schedule_lock_;
#endif
  std::map<EntityBase *, SortingComponents> sorting_entitys_;
  std::map<uint64_t, SortingGroup> sorting_groups_;
};

}  // namespace web_server
}  // namespace esphome
