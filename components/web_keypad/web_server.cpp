#include "web_server.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "ArduinoJson.h"

#if defined(USE_DSC_PANEL)
#include "esphome/components/dsc_alarm_panel/dscAlarm.h"
#endif
#if defined(USE_VISTA_PANEL)
#include "esphome/components/vista_alarm_panel/vistaalarm.h"
#endif

#include <cstdlib>

#ifdef USE_LIGHT
#include "esphome/components/light/light_json_schema.h"
#endif

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif

#ifdef USE_ARDUINO
#include <StreamString.h>
#include <Update.h>
#endif
namespace esphome {
namespace web_server {



static const char *const TAG = "web_server";
void * webServerPtr;

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
static const char *const HEADER_PNA_NAME = "Private-Network-Access-Name";
static const char *const HEADER_PNA_ID = "Private-Network-Access-ID";
static const char *const HEADER_CORS_REQ_PNA = "Access-Control-Request-Private-Network";
static const char *const HEADER_CORS_ALLOW_PNA = "Access-Control-Allow-Private-Network";
#endif

#if USE_WEBSERVER_VERSION == 1
/*
void write_row(AsyncResponseStream *stream, EntityBase *obj, const std::string &klass, const std::string &action,
               const std::function<void(AsyncResponseStream &stream, EntityBase *obj)> &action_func = nullptr) {
  stream->print("<tr class=\"");
  stream->print(klass.c_str());
  if (obj->is_internal())
    stream->print(" internal");
  stream->print("\" id=\"");
  stream->print(klass.c_str());
  stream->print("-");
  stream->print(obj->get_object_id().c_str());
  stream->print("\"><td>");
  stream->print(obj->get_name().c_str());
  stream->print("</td><td></td><td>");
  stream->print(action.c_str());
  if (action_func) {
    action_func(*stream, obj);
  }
  stream->print("</td>");
  stream->print("</tr>");
}
*/
#endif

UrlMatch match_url(const struct mg_str *urlstr, bool only_domain = false) {
   char buf[urlstr->len + 1];
   strncpy(buf,urlstr->ptr,urlstr->len);
   buf[urlstr->len]=0;
  std::string url=buf;
  
  UrlMatch match;
  match.valid = false;
  size_t domain_end = url.find('/', 1);
  if (domain_end == std::string::npos)
    return match;
  match.domain = url.substr(1, domain_end - 1);
  if (only_domain) {
    match.valid = true;
    return match;
  }
  if (url.length() == domain_end - 1)
    return match;
  size_t id_begin = domain_end + 1;
  size_t id_end = url.find('/', id_begin);
  match.valid = true;
  if (id_end == std::string::npos) {
    match.id = url.substr(id_begin, url.length() - id_begin);
    return match;
  }
  match.id = url.substr(id_begin, id_end - id_begin);
  size_t method_begin = id_end + 1;
  match.method = url.substr(method_begin, url.length() - method_begin);
  return match;
}

WebServer::WebServer()
    :  entities_iterator_(ListEntitiesIterator(this)) {
#ifdef USE_ESP32
  to_schedule_lock_ = xSemaphoreCreateMutex();
#endif
   webServerPtr=this;
}

#if USE_WEBSERVER_VERSION == 1
void WebServer::set_css_url(const char *css_url) { this->css_url_ = css_url; }
void WebServer::set_js_url(const char *js_url) { this->js_url_ = js_url; }
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
void WebServer::set_css_include(const char *css_include) { this->css_include_ = css_include; }
#endif
#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::set_js_include(const char *js_include) { this->js_include_ = js_include; }
#endif

void WebServer::set_keypad_config(const char * json_keypad_config) {
    _json_keypad_config=json_keypad_config;
}

std::string WebServer::get_config_json() {
  return json::build_json([this](JsonObject root) {
    root["title"] = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
    root["comment"] = App.get_comment();
    root["ota"] = this->allow_ota_;
    root["log"] = this->expose_log_;
    root["lang"] = "en";
    root["partitions"]=this->partitions_;
  });
}

std::string WebServer::escape_json(const char * input) {
    std::string output;
    output.reserve(strlen(input));

    for (int i=0;i<strlen(input);i++)
    {
        
        switch (input[i]) {
            case 27 : 
                    output+="\x27";
                    break;
            case '"':
                output += "\\\"";
                break;
            case '/':
                output += "\\/";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            case '\\':
                output += "\\\\";
                break;
            default:
                output += input[i];
                break;
        }

    }

    return output;
}
/*
#ifdef ASYNCWEB

void WebServer::webPollTask(void * args) {

  WebServer * _this = (WebServer * ) args;
  static unsigned long checkTime = millis();  
  for (;;) { 
     if (network::is_connected() && _this->c != NULL)  
        mg_mgr_poll(&_this->mgr, 1000) ;
        if (millis() - checkTime > 30000) {
            UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            printf("\nTaskupdates free memory: %5d\n", (uint16_t) uxHighWaterMark);
            checkTime=millis();
        }
  }
  vTaskDelete(NULL);
}

#endif
*/

void WebServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up web server...");
  this->setup_controller(this->include_internal_);

   mg_mgr_init(&mgr); 
#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->expose_log_) {
    logger::global_logger->add_on_log_callback(
        [this](int level, const char *tag, const char *message) { 
        std::string msg=escape_json(message);
        this->push(LOG,msg.c_str()); 
        });
  }
#endif

  this->set_interval(10000, [this]() {
      this->push(PING,"",millis(),30000);
      });
      /*
#ifdef ASYNCWEB   
      
    int ASYNC_CORE=0;
    xTaskCreatePinnedToCore(
    this -> webPollTask, //Function to implement the task
    "webPollTask", //Name of the task
    6200, //Stack size in words
    (void * ) this, //Task input parameter
    10, //Priority of the task
    NULL, //Task handle.
    ASYNC_CORE //Core where the task should run
  );  
#endif     
*/      
}
void WebServer::loop() {
#ifdef USE_ESP32
  if (xSemaphoreTake(this->to_schedule_lock_, 0L)) {
    std::function<void()> fn;
    if (!to_schedule_.empty()) {
      // scheduler execute things out of order which may lead to incorrect state
      // this->defer(std::move(to_schedule_.front()));
      // let's execute it directly from the loop
      fn = std::move(to_schedule_.front());
      to_schedule_.pop_front();
    }
    xSemaphoreGive(this->to_schedule_lock_);
    if (fn) {
      fn();
    }
  }
#endif
  this->entities_iterator_.advance();
     
  if (firstrun_ && network::is_connected()) {
    mg_mgr_init(&mgr);        // Initialise event manager
    char addr[50];
    sprintf(addr,"http://0.0.0.0:%d",port_);
    printf("Starting web server on %s:%d\n", network::get_use_address().c_str(),port_);
   if ((c = mg_http_listen(&mgr, addr, ev_handler, &mgr)) == NULL) {
     printf("Cannot listen on address..");
      return;
    }
    firstrun_=false;
   }
   
//#if not defined(ASYNCWEB)   
   if (network::is_connected() && c != NULL)
      mg_mgr_poll(&mgr, 0);
//#endif  

}
void WebServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Web Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), port_);
}
float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

#if USE_WEBSERVER_VERSION == 1
/*
void WebServer::handle_index_request(struct mg_connection *c) {
  AsyncResponseStream *stream = request->beginResponseStream("text/html");
  const std::string &title = App.get_name();
  stream->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                  "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\"><title>"));
  stream->print(title.c_str());
  stream->print(F("</title>"));
#ifdef USE_WEBSERVER_CSS_INCLUDE
  stream->print(F("<link rel=\"stylesheet\" href=\"/0.css\">"));
#endif
  if (strlen(this->css_url_) > 0) {
    stream->print(F(R"(<link rel="stylesheet" href=")"));
    stream->print(this->css_url_);
    stream->print(F("\">"));
  }
  stream->print(F("</head><body>"));
  stream->print(F("<article class=\"markdown-body\"><h1>"));
  stream->print(title.c_str());
  stream->print(F("</h1>"));
  stream->print(F("<h2>States</h2><table id=\"states\"><thead><tr><th>Name<th>State<th>Actions<tbody>"));

#ifdef USE_SENSOR
  for (auto *obj : App.get_sensors()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "sensor", "");
  }
#endif

#ifdef USE_SWITCH
  for (auto *obj : App.get_switches()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "switch", "<button>Toggle</button>");
  }
#endif

#ifdef USE_BUTTON
  for (auto *obj : App.get_buttons())
    write_row(stream, obj, "button", "<button>Press</button>");
#endif

#ifdef USE_BINARY_SENSOR
  for (auto *obj : App.get_binary_sensors()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "binary_sensor", "");
  }
#endif

#ifdef USE_FAN
  for (auto *obj : App.get_fans()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "fan", "<button>Toggle</button>");
  }
#endif

#ifdef USE_LIGHT
  for (auto *obj : App.get_lights()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "light", "<button>Toggle</button>");
  }
#endif

#ifdef USE_TEXT_SENSOR
  for (auto *obj : App.get_text_sensors()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "text_sensor", "");
  }
#endif

#ifdef USE_COVER
  for (auto *obj : App.get_covers()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "cover", "<button>Open</button><button>Close</button>");
  }
#endif

#ifdef USE_NUMBER
  for (auto *obj : App.get_numbers()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "number", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        number::Number *number = (number::Number *) obj;
        stream.print(R"(<input type="number" min=")");
        stream.print(number->traits.get_min_value());
        stream.print(R"(" max=")");
        stream.print(number->traits.get_max_value());
        stream.print(R"(" step=")");
        stream.print(number->traits.get_step());
        stream.print(R"(" value=")");
        stream.print(number->state);
        stream.print(R"("/>)");
      });
    }
  }
#endif

#ifdef USE_TEXT
  for (auto *obj : App.get_texts()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "text", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        text::Text *text = (text::Text *) obj;
        auto mode = (int) text->traits.get_mode();
        stream.print(R"(<input type=")");
        if (mode == 2) {
          stream.print(R"(password)");
        } else {  // default
          stream.print(R"(text)");
        }
        stream.print(R"(" minlength=")");
        stream.print(text->traits.get_min_length());
        stream.print(R"(" maxlength=")");
        stream.print(text->traits.get_max_length());
        stream.print(R"(" pattern=")");
        stream.print(text->traits.get_pattern().c_str());
        stream.print(R"(" value=")");
        stream.print(text->state.c_str());
        stream.print(R"("/>)");
      });
    }
  }
#endif

#ifdef USE_SELECT
  for (auto *obj : App.get_selects()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "select", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        select::Select *select = (select::Select *) obj;
        stream.print("<select>");
        stream.print("<option></option>");
        for (auto const &option : select->traits.get_options()) {
          stream.print("<option>");
          stream.print(option.c_str());
          stream.print("</option>");
        }
        stream.print("</select>");
      });
    }
  }
#endif

#ifdef USE_LOCK
  for (auto *obj : App.get_locks()) {
    if (this->include_internal_ || !obj->is_internal()) {
      write_row(stream, obj, "lock", "", [](AsyncResponseStream &stream, EntityBase *obj) {
        lock::Lock *lock = (lock::Lock *) obj;
        stream.print("<button>Lock</button><button>Unlock</button>");
        if (lock->traits.get_supports_open()) {
          stream.print("<button>Open</button>");
        }
      });
    }
  }
#endif

#ifdef USE_CLIMATE
  for (auto *obj : App.get_climates()) {
    if (this->include_internal_ || !obj->is_internal())
      write_row(stream, obj, "climate", "");
  }
#endif

  stream->print(F("</tbody></table><p>See <a href=\"https://esphome.io/web-api/index.html\">ESPHome Web API</a> for "
                  "REST API documentation.</p>"));
  if (this->allow_ota_) {
    stream->print(
        F("<h2>OTA Update</h2><form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\"><input "
          "type=\"file\" name=\"update\"><input type=\"submit\" value=\"Update\"></form>"));
  }
  stream->print(F("<h2>Debug Log</h2><pre id=\"log\"></pre>"));
#ifdef USE_WEBSERVER_JS_INCLUDE
  if (this->js_include_ != nullptr) {
    stream->print(F("<script type=\"module\" src=\"/0.js\"></script>"));
  }
#endif
  if (strlen(this->js_url_) > 0) {
    stream->print(F("<script src=\""));
    stream->print(this->js_url_);
    stream->print(F("\"></script>"));
  }
  stream->print(F("</article></body></html>"));
  request->send(stream);
  */
}
#elif USE_WEBSERVER_VERSION == 2
void WebServer::handle_index_request(struct mg_connection *c) {
  
          const char * buf= (const char *) ESPHOME_WEBSERVER_INDEX_HTML;  
          mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n", ESPHOME_WEBSERVER_INDEX_HTML_SIZE );
          mg_send(c,buf,ESPHOME_WEBSERVER_INDEX_HTML_SIZE);
          c->is_resp = 0;  

}
#endif

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
void WebServer::handle_pna_cors_request(struct mg_connection *c) {
    /*
  AsyncWebServerResponse *response = request->beginResponse(200, "");
  response->addHeader(HEADER_CORS_ALLOW_PNA, "true");
  response->addHeader(HEADER_PNA_NAME, App.get_name().c_str());
  std::string mac = get_mac_address_pretty();
  response->addHeader(HEADER_PNA_ID, mac.c_str());
  request->send(response);
  */
 std::string mac = get_mac_address_pretty();  
  mg_printf(c,"HTTP/1.1 200 OK\r\n%s:%s\r\n%s:%s\r\n%s:%s\r\n\r\n",HEADER_CORS_ALLOW_PNA,"true",HEADER_PNA_NAME,App.get_name().c_str(),HEADER_PNA_ID,mac.c_str());
  c->is_resp=0;

}
#endif

#ifdef USE_WEBSERVER_CSS_INCLUDE
void WebServer::handle_css_request(struct mg_connection *c) {
   
          const char * buf= (const char *) ESPHOME_WEBSERVER_CSS_INCLUDE;
          mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\nContent-Encoding: gzip\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n", ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE );
          mg_send(c,buf,ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE);
          c->is_resp = 0;    
     
}
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
void WebServer::handle_js_request(struct mg_connection *c) {

         
          const char * buf= (const char *) ESPHOME_WEBSERVER_JS_INCLUDE;
          mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/javascript; charset=utf-8\r\nContent-Encoding: gzip\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n", ESPHOME_WEBSERVER_JS_INCLUDE_SIZE );
          mg_send(c,buf,ESPHOME_WEBSERVER_JS_INCLUDE_SIZE);
          c->is_resp = 0;      
            
}
#endif

#define set_json_id(root, obj, sensor, start_config) \
  (root)["id"] = sensor; \
  if (((start_config) == DETAIL_ALL)) \
    (root)["name"] = (obj)->get_name();

#define set_json_value(root, obj, sensor, value, start_config) \
  set_json_id((root), (obj), sensor, start_config)(root)["value"] = value;

#define set_json_state_value(root, obj, sensor, state, value, start_config) \
  set_json_value(root, obj, sensor, value, start_config)(root)["state"] = state;

#define set_json_icon_state_value(root, obj, sensor, state, value, start_config) \
  set_json_value(root, obj, sensor, value, start_config)(root)["state"] = state; \
  if (((start_config) == DETAIL_ALL)) \
    (root)["icon"] = (obj)->get_icon();

#ifdef USE_SENSOR
void WebServer::on_sensor_update(sensor::Sensor *obj, float state) {
  //this->events_.send(this->sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->sensor_json(obj, state, DETAIL_STATE).c_str());  
}
void WebServer::handle_sensor_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
  for (sensor::Sensor *obj : App.get_sensors()) {
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->sensor_json(obj, obj->state, DETAIL_STATE);
    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n", "%s", data.c_str());          
    //request->send(200, "application/json", data.c_str());
    return;
  }
   mg_http_reply(c,404,"","");
}
std::string WebServer::sensor_json(sensor::Sensor *obj, float value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    std::string state;
    if (std::isnan(value)) {
      state = "NA";
    } else {
      state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
      if (!obj->get_unit_of_measurement().empty())
        state += " " + obj->get_unit_of_measurement();
    }
    set_json_icon_state_value(root, obj, "sensor-" + obj->get_object_id(), state, value, start_config);
  });
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
  //this->events_.send(this->text_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->text_sensor_json(obj, state, DETAIL_STATE).c_str());   
}
void WebServer::handle_text_sensor_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
  for (text_sensor::TextSensor *obj : App.get_text_sensors()) {
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->text_sensor_json(obj, obj->state, DETAIL_STATE);
    //request->send(200, "application/json", data.c_str());
    mg_http_reply(c, 200, "Content-Type: application/jsonAccess-Control-Allow-Origin: *\r\n\r\n", "%s", data.c_str());          
    return;
  }
   mg_http_reply(c,404,"","");
}
#if defined(USE_CUSTOM_ID)
std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value,
                                        JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "text_sensor-" + obj->get_object_id() + "-" + obj->get_type_id(), value, value, start_config);
  });
}
#else
std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value,
                                        JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "text_sensor-" + obj->get_object_id() + "-" + obj->get_object_id(), value, value, start_config);
  });
}    

#endif
#endif

#ifdef USE_SWITCH
void WebServer::on_switch_update(switch_::Switch *obj, bool state) {
  //this->events_.send(this->switch_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->switch_json(obj, state, DETAIL_STATE).c_str());   
}
std::string WebServer::switch_json(switch_::Switch *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "switch-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
    if (start_config == DETAIL_ALL) {
      root["assumed_state"] = obj->assumed_state();
    }
  });
}
void WebServer::handle_switch_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
  for (switch_::Switch *obj : App.get_switches()) {
    if (obj->get_object_id() != match.id)
      continue;
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;  
   // if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {       
      std::string data = this->switch_json(obj, obj->state, DETAIL_STATE);
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());      
    } else if (match.method == "toggle") {
      this->schedule_([obj]() { obj->toggle(); });
        mg_http_reply(c,200,"","");
    } else if (match.method == "turn_on") {
      this->schedule_([obj]() { obj->turn_on(); });
        mg_http_reply(c,200,"","");
    } else if (match.method == "turn_off") {
      this->schedule_([obj]() { obj->turn_off(); });
        mg_http_reply(c,200,"","");
    } else {
       mg_http_reply(c,404,"","");
    }
    return;
  }
   mg_http_reply(c,404,"","");
}
#endif

#ifdef USE_BUTTON
std::string WebServer::button_json(button::Button *obj, JsonDetail start_config) {
  return json::build_json(
      [obj, start_config](JsonObject root) { set_json_id(root, obj, "button-" + obj->get_object_id(), start_config); });
}

void WebServer::handle_button_requeststruct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;       
  for (button::Button *obj : App.get_buttons()) {
    if (obj->get_object_id() != match.id)
      continue;
      if (mg_vcasecmp(&hm->method, "GET") == 0 && match.method == "press") {  
   // if (request->method() == HTTP_POST && match.method == "press") {
      this->schedule_([obj]() { obj->press(); });
        mg_http_reply(c,200,"","");
      return;
    } else {
       mg_http_reply(c,404,"","");
    }
    return;
  }
   mg_http_reply(c,404,"","");
}
#endif

#ifdef USE_BINARY_SENSOR
void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  //this->events_.send(this->binary_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->binary_sensor_json(obj, state, DETAIL_STATE).c_str());   
}
#if defined(USE_CUSTOM_ID)
std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_state_value(root, obj, "binary_sensor-" + obj->get_object_id() + "-" + obj->get_type_id(), value ? "ON" : "OFF", value, start_config);
  
  });
}
#else
std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_state_value(root, obj, "binary_sensor-" + obj->get_object_id() + "-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
  
  });
}
#endif

void WebServer::handle_binary_sensor_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
  for (binary_sensor::BinarySensor *obj : App.get_binary_sensors()) {
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->binary_sensor_json(obj, obj->state, DETAIL_STATE);
    //request->send(200, "application/json", data.c_str());
    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());      
    return;
  }
   mg_http_reply(c,404,"","");
}
#endif

#ifdef USE_FAN
void WebServer::on_fan_update(fan::Fan *obj) {
    //this->p.send(this->fan_json(obj, DETAIL_STATE).c_str(), "state"); 
  this->push(STATE,this->fan_json(obj, DETAIL_STATE).c_str());     
    }
std::string WebServer::fan_json(fan::Fan *obj, JsonDetail start_config) {
    
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_state_value(root, obj, "fan-" + obj->get_object_id(), obj->state ? "ON" : "OFF", obj->state, start_config);
    const auto traits = obj->get_traits();
    if (traits.supports_speed()) {
      root["speed_level"] = obj->speed;
      root["speed_count"] = traits.supported_speed_count();
    }
    if (obj->get_traits().supports_oscillation())
      root["oscillation"] = obj->oscillating;
  });
}
void WebServer::handle_fan_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
  
  for (fan::Fan *obj : App.get_fans()) {
    if (obj->get_object_id() != match.id)
      continue;

   // if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {         
      std::string data = this->fan_json(obj, DETAIL_STATE);
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());        
    } else if (match.method == "toggle") {
      this->schedule_([obj]() { obj->toggle().perform(); });
        mg_http_reply(c,200,"","");
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
     // if (request->hasParam("speed_level")) {
       char buf[100];
        if (mg_http_get_var(&hm->body,"speed_level",buf,sizeof(buf)) > 0) {         
        auto speed_level = buf;
        auto val = parse_number<int>(speed_level.c_str());
        if (!val.has_value()) {
          ESP_LOGW(TAG, "Can't convert '%s' to number!", speed_level.c_str());
          return;
        }
        call.set_speed(*val);
      }
     // if (request->hasParam("oscillation")) {
    char buf[100];
    if (mg_http_get_var(&hm->body,"oscillation",buf,sizeof(buf)) > 0) {         
        auto speed = buf;
        auto val = parse_on_off(speed.c_str());
        switch (val) {
          case PARSE_ON:
            call.set_oscillating(true);
            break;
          case PARSE_OFF:
            call.set_oscillating(false);
            break;
          case PARSE_TOGGLE:
            call.set_oscillating(!obj->oscillating);
            break;
          case PARSE_NONE:
             mg_http_reply(c,404,"","");
            return;
        }
      }
      this->schedule_([call]() mutable { call.perform(); });
        mg_http_reply(c,200,"","");
    } else if (match.method == "turn_off") {
      this->schedule_([obj]() { obj->turn_off().perform(); });
        mg_http_reply(c,200,"","");
    } else {
       mg_http_reply(c,404,"","");
    }
    return;
  }
  
   mg_http_reply(c,404,"","");


}
#endif

#ifdef USE_LIGHT
void WebServer::on_light_update(light::LightState *obj) {
 // this->events_.send(this->light_json(obj, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->light_json(obj, DETAIL_STATE).c_str());  
}
void WebServer::handle_light_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
    
  for (light::LightState *obj : App.get_lights()) {
    if (obj->get_object_id() != match.id)
      continue;

    //if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) { 
      std::string data = this->light_json(obj, DETAIL_STATE);
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());       
    } else if (match.method == "toggle") {
      this->schedule_([obj]() { obj->toggle().perform(); });
        mg_http_reply(c,200,"","");
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      //if (request->hasParam("brightness")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"brightness",buf,sizeof(buf)) > 0) {          
        auto brightness = parse_number<float>(buf);
        if (brightness.has_value()) {
          call.set_brightness(*brightness / 255.0f);
        }
      }
     // if (request->hasParam("r")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"r",buf,sizeof(buf)) > 0) {          
        auto r = parse_number<float>(buf);
        if (r.has_value()) {
          call.set_red(*r / 255.0f);
        }
      }
     // if (request->hasParam("g")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"g",buf,sizeof(buf)) > 0) {          
        auto g = parse_number<float>(buf);
        if (g.has_value()) {
          call.set_green(*g / 255.0f);
        }
      }
     // if (request->hasParam("b")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"b",buf,sizeof(buf)) > 0) {          
        auto b = parse_number<float>(buf);
        if (b.has_value()) {
          call.set_blue(*b / 255.0f);
        }
      }
     // if (request->hasParam("white_value")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"white_value",buf,sizeof(buf)) > 0) {          
        auto white_value = parse_number<float>(buf);
        if (white_value.has_value()) {
          call.set_white(*white_value / 255.0f);
        }
      }
     // if (request->hasParam("color_temp")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"color_temp",buf,sizeof(buf)) > 0) {          
        auto color_temp = parse_number<float>(buf);
        if (color_temp.has_value()) {
          call.set_color_temperature(*color_temp);
        }
      }
      //if (request->hasParam("flash")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"flash",buf,sizeof(buf)) > 0) {           
        auto flash = parse_number<uint32_t>(buf);
        if (flash.has_value()) {
          call.set_flash_length(*flash * 1000);
        }
      }
      //if (request->hasParam("transition")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"transition",buf,sizeof(buf)) > 0) {           
        auto transition = parse_number<uint32_t>(buf);
        if (transition.has_value()) {
          call.set_transition_length(*transition * 1000);
        }
      }
     // if (request->hasParam("effect")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"effect",buf,sizeof(buf)) > 0) {          
        const char *effect = buf;
        call.set_effect(effect);
      }

      this->schedule_([call]() mutable { call.perform(); });
        mg_http_reply(c,200,"","");
    } else if (match.method == "turn_off") {
      auto call = obj->turn_off();
     // if (request->hasParam("transition")) {
       char buf[100];
       if (mg_http_get_var(&hm->body,"transition",buf,sizeof(buf)) > 0) {          
        auto transition = parse_number<uint32_t>(buf);
        if (transition.has_value()) {
          call.set_transition_length(*transition * 1000);
        }
      }
      this->schedule_([call]() mutable { call.perform(); });
        mg_http_reply(c,200,"","");
    } else {
       mg_http_reply(c,404,"","");
    }
    return;
  }
  
   mg_http_reply(c,404,"","");
  
}
std::string WebServer::light_json(light::LightState *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "light-" + obj->get_object_id(), start_config);
    root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

    light::LightJSONSchema::dump_json(*obj, root);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("effects");
      opt.add("None");
      for (auto const &option : obj->get_effects()) {
        opt.add(option->get_name());
      }
    }
  });
}
#endif

#ifdef USE_COVER
void WebServer::on_cover_update(cover::Cover *obj) {
 // this->events_.send(this->cover_json(obj, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->cover_json(obj, DETAIL_STATE).c_str());  
}
void WebServer::handle_cover_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;      
  for (cover::Cover *obj : App.get_covers()) {
    if (obj->get_object_id() != match.id)
      continue;

    //if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {        
      std::string data = this->cover_json(obj, DETAIL_STATE);
     // request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());     
      continue;
    }

    auto call = obj->make_call();
    if (match.method == "open") {
      call.set_command_open();
    } else if (match.method == "close") {
      call.set_command_close();
    } else if (match.method == "stop") {
      call.set_command_stop();
    } else if (match.method != "set") {
       mg_http_reply(c,404,"","");
      return;
    }

    auto traits = obj->get_traits();

   // if ((request->hasParam("position") && !traits.get_supports_position()) ||
        //(request->hasParam("tilt") && !traits.get_supports_tilt())) {
       char buf[50];
       bool p= (mg_http_get_var(&hm->body,"position",buf,sizeof(buf)) > 0);
       bool t= (mg_http_get_var(&hm->body,"tilt",buf,sizeof(buf)) > 0); 
       if (p && !traits.get_supports_position()) || t && !traits.get_supports_tilt())) {
            //request->send(409);
        mg_http_reply(c,409,"","");            
        return;
        }

    char buf[100];
    if (mg_http_get_var(&hm->body,"position",buf,sizeof(buf)) > 0) {
      auto position = parse_number<float>(buf);
      if (position.has_value()) {
        call.set_position(*position);
      }
    }
    if (mg_http_get_var(&hm->body,"tilt",buf,sizeof(buf)) > 0) {
      auto position = parse_number<float>(buf);
      auto tilt = parse_number<float>(buf);
      if (tilt.has_value()) {
        call.set_tilt(*tilt);
      }
    }

    this->schedule_([call]() mutable { call.perform(); });
      mg_http_reply(c,200,"","");
    return;
  }
   mg_http_reply(c,404,"","");
}
std::string WebServer::cover_json(cover::Cover *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_state_value(root, obj, "cover-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
                         obj->position, start_config);
    root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_tilt())
      root["tilt"] = obj->tilt;
  });
}
#endif

#ifdef USE_NUMBER
void WebServer::on_number_update(number::Number *obj, float state) {
  //this->events_.send(this->number_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->number_json(obj, state, DETAIL_STATE).c_str());  
}
void WebServer::handle_number_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;      
  for (auto *obj : App.get_numbers()) {
    if (obj->get_object_id() != match.id)
      continue;

    //if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {        
      std::string data = this->number_json(obj, obj->state, DETAIL_STATE);
     // request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());      
      return;
    }
    if (match.method != "set") {
       mg_http_reply(c,404,"","");
      return;
    }

    auto call = obj->make_call();
    char buf[100];
    if (mg_http_get_var(&hm->body,"value",buf,sizeof(buf)) > 0) {
   // if (request->hasParam("value")) {
      auto value = parse_number<float>(buf);
      if (value.has_value())
        call.set_value(*value);
    }

    this->schedule_([call]() mutable { call.perform(); });
      mg_http_reply(c,200,"","");
    return;
  }
   mg_http_reply(c,404,"","");
}

std::string WebServer::number_json(number::Number *obj, float value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_id(root, obj, "number-" + obj->get_object_id(), start_config);
    if (start_config == DETAIL_ALL) {
      root["min_value"] = obj->traits.get_min_value();
      root["max_value"] = obj->traits.get_max_value();
      root["step"] = obj->traits.get_step();
      root["mode"] = (int) obj->traits.get_mode();
    }
    if (std::isnan(value)) {
      root["value"] = "\"NaN\"";
      root["state"] = "NA";
    } else {
      root["value"] = value;
      std::string state = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      if (!obj->traits.get_unit_of_measurement().empty())
        state += " " + obj->traits.get_unit_of_measurement();
      root["state"] = state;
    }
  });
}
#endif

#ifdef USE_TEXT
void WebServer::on_text_update(text::Text *obj, const std::string &state) {
  //this->events_.send(this->text_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->text_json(obj, state, DETAIL_STATE).c_str());   
}
void WebServer::handle_text_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;      
  for (auto *obj : App.get_texts()) {
    if (obj->get_object_id() != match.id)
      continue;

    //if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {        
      std::string data = this->text_json(obj, obj->state, DETAIL_STATE);
     // request->send(200, "text/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());      
      return;
    }
    if (match.method != "set") {
       mg_http_reply(c,404,"","");
      return;
    }

    auto call = obj->make_call();
   // if (request->hasParam("value")) {
    char buf[100];
    if (mg_http_get_var(&hm->body,"value",buf,sizeof(buf)) > 0) {
      auto value=buf;       
      //String value = request->getParam("value")->value();
      call.set_value(value);
    }

    this->defer([call]() mutable { call.perform(); });
      mg_http_reply(c,200,"","");
    return;
  }
   mg_http_reply(c,404,"","");
}

std::string WebServer::text_json(text::Text *obj, const std::string &value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_id(root, obj, "text-" + obj->get_object_id(), start_config);
    if (start_config == DETAIL_ALL) {
      root["mode"] = (int) obj->traits.get_mode();
    }
    root["min_length"] = obj->traits.get_min_length();
    root["max_length"] = obj->traits.get_max_length();
    root["pattern"] = obj->traits.get_pattern();
    if (obj->traits.get_mode() == text::TextMode::TEXT_MODE_PASSWORD) {
      root["state"] = "********";
    } else {
      root["state"] = value;
    }
    root["value"] = value;
  });
}
#endif


    long int WebServer::toInt(std::string s, int base) {
      if (s.empty() || std::isspace(s[0])) return 0;
      char * p;
      long int li = strtol(s.c_str(), & p, base);
      return li;
    }
    
bool WebServer::callKeyService(const char *buf,int partition) {
#if defined(USE_DSC_PANEL) || defined (USE_VISTA_PANEL)

#ifdef USE_DSC_PANEL
     auto * alarmPanel=static_cast< alarm_panel::DSCkeybushome*>(alarm_panel::alarmPanelPtr);
   #else     
     auto * alarmPanel=static_cast<alarm_panel::vistaECPHome*>(alarm_panel::alarmPanelPtr);
#endif 
      std::string keys=buf;
      if (this->key_service_func_.has_value()) {
          (*this->key_service_func_)(keys,partition);          
          return true;
      } else if (alarmPanel != NULL) {
          alarmPanel->alarm_keypress_partition(keys,partition);
          return true;
      } 
      return false;
}    

void WebServer::handle_alarm_panel_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
    
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;      

    // if (request->method() == HTTP_GET) {
     if (mg_vcasecmp(&hm->method, "GET") == 0 && match.method=="getconfig") {        
      std::string data = _json_keypad_config;
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s\n", data.c_str());      
      return;
    }
    
    if (match.method != "set") {
       mg_http_reply(c,404,"","");
      return;
    }    

    int partition=1; //get default partition
    //if (request->hasParam("partition",true)) {
     //  auto p = request->getParam("partition",true)->value(); 
    char buf[100];
    if (mg_http_get_var(&hm->body,"partition",buf,sizeof(buf)) > 0) {
      partition = toInt(buf,10);
    }
   // if (request->hasParam("keys",true)) {
    //  std::string keys = request->getParam("keys",true)->value().c_str();
    if (mg_http_get_var(&hm->body,"keys",buf,sizeof(buf)) > 0) {
       if (callKeyService(buf,partition)) 
        mg_http_reply(c,200,"Access-Control-Allow-Origin: *\r\n","");
       else
        mg_http_reply(c,404,"","");
      return;      
    }
   mg_http_reply(c,404,"","");
}

#endif



#ifdef USE_SELECT
void WebServer::on_select_update(select::Select *obj, const std::string &state, size_t index) {
  //this->events_.send(this->select_json(obj, state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->select_json(obj, state, DETAIL_STATE).c_str());   
}
void WebServer::handle_select_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;        
  for (auto *obj : App.get_selects()) {
    if (obj->get_object_id() != match.id)
      continue;

    //if (request->method() == HTTP_GET) {
      if (mg_vcasecmp(&hm->method, "GET") == 0) {        
      auto detail = DETAIL_STATE;
    //  auto *param = request->getParam("detail");
     char buf[100];    
    if (mg_http_get_var(&hm->body,"detail",buf,sizeof(buf)) > 0) {
     std::string detailstr=buf;    
      if (detailstr == "all") {
        detail = DETAIL_ALL;
      }
      std::string data = this->select_json(obj, obj->state, detail);
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());      
      return;
    }

    if (match.method != "set") {
   mg_http_reply(c,404,"","");
      return;
    }

    auto call = obj->make_call();

    if (mg_http_get_var(&hm->body,"option",buf,sizeof(buf)) > 0) {
     auto option=buf;
    //if (request->hasParam("option")) {
     // auto option = request->getParam("option")->value();
      call.set_option(option.c_str());  // NOLINT(clang-diagnostic-deprecated-declarations)
    }

    this->schedule_([call]() mutable { call.perform(); });
   mg_http_reply(c,200,"","");
    return;
  }
   mg_http_reply(c,404,"","");
}
std::string WebServer::select_json(select::Select *obj, const std::string &value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_state_value(root, obj, "select-" + obj->get_object_id(), value, value, start_config);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("option");
      for (auto &option : obj->traits.get_options()) {
        opt.add(option);
      }
    }
  });
}
#endif

// Longest: HORIZONTAL
#define PSTR_LOCAL(mode_s) strncpy_P(buf, (PGM_P) ((mode_s)), 15)

#ifdef USE_CLIMATE
void WebServer::on_climate_update(climate::Climate *obj) {
  //this->events_.send(this->climate_json(obj, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->climate_json(obj, DETAIL_STATE).c_str());   
}

void WebServer::handle_climate_requeststruct mg_connection *c, void *ev_data, const UrlMatch &match) {
  struct mg_http_message *hm = (struct mg_http_message *) ev_data;    
  for (auto *obj : App.get_climates()) {
    if (obj->get_object_id() != match.id)
      continue;

   // if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {        
      std::string data = this->climate_json(obj, DETAIL_STATE);
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());      
      return;
    }

    if (match.method != "set") {
   mg_http_reply(c,404,"","");
      return;
    }

    auto call = obj->make_call();
    char buf[100];
    if (mg_http_get_var(&hm->body,"mode",buf,sizeof(buf)) > 0) {
    //if (request->hasParam("mode")) {
     // auto mode = request->getParam("mode")->value();
      auto mode=buf;
      call.set_mode(mode);
    }
    if (mg_http_get_var(&hm->body,"target_temperature_high",buf,sizeof(buf)) > 0) {
     auto target_temperature_high=buf;
   // if (request->hasParam("target_temperature_high")) {
     // auto target_temperature_high = parse_number<float>(request->getParam("target_temperature_high")->value().c_str());
      if (target_temperature_high.has_value())
        call.set_target_temperature_high(*target_temperature_high);
    }

   // if (request->hasParam("target_temperature_low")) {
      //auto target_temperature_low = parse_number<float>(request->getParam("target_temperature_low")->value().c_str());
    if (mg_http_get_var(&hm->body,"target_temperature_low",buf,sizeof(buf)) > 0) {
     auto target_temperature_low=buf;      
      if (target_temperature_low.has_value())
        call.set_target_temperature_low(*target_temperature_low);
    }

   // if (request->hasParam("target_temperature")) {
    //  auto target_temperature = parse_number<float>(request->getParam("target_temperature")->value().c_str());
    if (mg_http_get_var(&hm->body,"target_temperature",buf,sizeof(buf)) > 0) {
     auto target_temperature=buf;    
      if (target_temperature.has_value())
        call.set_target_temperature(*target_temperature);
    }

    this->schedule_([call]() mutable { call.perform(); });
   mg_http_reply(c,200,"","");
    return;
  }
   mg_http_reply(c,404,"","");
}

std::string WebServer::climate_json(climate::Climate *obj, JsonDetail start_config) {
  return json::build_json([obj, start_config](JsonObject root) {
    set_json_id(root, obj, "climate-" + obj->get_object_id(), start_config);
    const auto traits = obj->get_traits();
    int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
    int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
    char buf[16];

    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("modes");
      for (climate::ClimateMode m : traits.get_supported_modes())
        opt.add(PSTR_LOCAL(climate::climate_mode_to_string(m)));
      if (!traits.get_supported_custom_fan_modes().empty()) {
        JsonArray opt = root.createNestedArray("fan_modes");
        for (climate::ClimateFanMode m : traits.get_supported_fan_modes())
          opt.add(PSTR_LOCAL(climate::climate_fan_mode_to_string(m)));
      }

      if (!traits.get_supported_custom_fan_modes().empty()) {
        JsonArray opt = root.createNestedArray("custom_fan_modes");
        for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
          opt.add(custom_fan_mode);
      }
      if (traits.get_supports_swing_modes()) {
        JsonArray opt = root.createNestedArray("swing_modes");
        for (auto swing_mode : traits.get_supported_swing_modes())
          opt.add(PSTR_LOCAL(climate::climate_swing_mode_to_string(swing_mode)));
      }
      if (traits.get_supports_presets() && obj->preset.has_value()) {
        JsonArray opt = root.createNestedArray("presets");
        for (climate::ClimatePreset m : traits.get_supported_presets())
          opt.add(PSTR_LOCAL(climate::climate_preset_to_string(m)));
      }
      if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
        JsonArray opt = root.createNestedArray("custom_presets");
        for (auto const &custom_preset : traits.get_supported_custom_presets())
          opt.add(custom_preset);
      }
    }

    bool has_state = false;
    root["mode"] = PSTR_LOCAL(climate_mode_to_string(obj->mode));
    root["max_temp"] = value_accuracy_to_string(traits.get_visual_max_temperature(), target_accuracy);
    root["min_temp"] = value_accuracy_to_string(traits.get_visual_min_temperature(), target_accuracy);
    root["step"] = traits.get_visual_target_temperature_step();
    if (traits.get_supports_action()) {
      root["action"] = PSTR_LOCAL(climate_action_to_string(obj->action));
      root["state"] = root["action"];
      has_state = true;
    }
    if (traits.get_supports_fan_modes() && obj->fan_mode.has_value()) {
      root["fan_mode"] = PSTR_LOCAL(climate_fan_mode_to_string(obj->fan_mode.value()));
    }
    if (!traits.get_supported_custom_fan_modes().empty() && obj->custom_fan_mode.has_value()) {
      root["custom_fan_mode"] = obj->custom_fan_mode.value().c_str();
    }
    if (traits.get_supports_presets() && obj->preset.has_value()) {
      root["preset"] = PSTR_LOCAL(climate_preset_to_string(obj->preset.value()));
    }
    if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
      root["custom_preset"] = obj->custom_preset.value().c_str();
    }
    if (traits.get_supports_swing_modes()) {
      root["swing_mode"] = PSTR_LOCAL(climate_swing_mode_to_string(obj->swing_mode));
    }
    if (traits.get_supports_current_temperature()) {
      if (!std::isnan(obj->current_temperature)) {
        root["current_temperature"] = value_accuracy_to_string(obj->current_temperature, current_accuracy);
      } else {
        root["current_temperature"] = "NA";
      }
    }
    if (traits.get_supports_two_point_target_temperature()) {
      root["target_temperature_low"] = value_accuracy_to_string(obj->target_temperature_low, target_accuracy);
      root["target_temperature_high"] = value_accuracy_to_string(obj->target_temperature_high, target_accuracy);
      if (!has_state) {
        root["state"] = value_accuracy_to_string((obj->target_temperature_high + obj->target_temperature_low) / 2.0f,
                                                 target_accuracy);
      }
    } else {
      root["target_temperature"] = value_accuracy_to_string(obj->target_temperature, target_accuracy);
      if (!has_state)
        root["state"] = root["target_temperature"];
    }
  });
}
#endif

#ifdef USE_LOCK
void WebServer::on_lock_update(lock::Lock *obj) {
 // this->events_.send(this->lock_json(obj, obj->state, DETAIL_STATE).c_str(), "state");
this->push(STATE,this->lock_json(obj, obj->state, DETAIL_STATE).c_str());  
}
std::string WebServer::lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    set_json_icon_state_value(root, obj, "lock-" + obj->get_object_id(), lock::lock_state_to_string(value), value,
                              start_config);
  });
}
void WebServer::handle_lock_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;        
  for (lock::Lock *obj : App.get_locks()) {
    if (obj->get_object_id() != match.id)
      continue;

    //if (request->method() == HTTP_GET) {
    if (mg_vcasecmp(&hm->method, "GET") == 0) {        
      std::string data = this->lock_json(obj, obj->state, DETAIL_STATE);
      //request->send(200, "application/json", data.c_str());
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
    } else if (match.method == "lock") {
      this->schedule_([obj]() { obj->lock(); });
       mg_http_reply(c,200,"","");
    } else if (match.method == "unlock") {
      this->schedule_([obj]() { obj->unlock(); });
       mg_http_reply(c,200,"","");
    } else if (match.method == "open") {
      this->schedule_([obj]() { obj->open(); });
       mg_http_reply(c,200,"","");
    } else {
       mg_http_reply(c,404,"","");
    }
    return;
  }
   mg_http_reply(c,404,"","");
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
void WebServer::on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) {
 // this->events_.send(this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str(), "state");
this->push(STATE,this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str());  
}
std::string WebServer::alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                                alarm_control_panel::AlarmControlPanelState value,
                                                JsonDetail start_config) {
  return json::build_json([obj, value, start_config](JsonObject root) {
    char buf[16];
    set_json_icon_state_value(root, obj, "alarm-control-panel-" + obj->get_object_id(),
                              PSTR_LOCAL(alarm_control_panel_state_to_string(value)), value, start_config);
  });
}
void WebServer::handle_alarm_control_panel_request(struct mg_connection *c, void *ev_data, const UrlMatch &match) {
 struct mg_http_message *hm = (struct mg_http_message *) ev_data;     
  for (alarm_control_panel::AlarmControlPanel *obj : App.get_alarm_control_panels()) {
    if (obj->get_object_id() != match.id)
      continue;
   // if (request->method() == HTTP_GET) {
      if (mg_vcasecmp(&hm->method, "GET") == 0) {
      std::string data = this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE);
      mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
      //request->send(200, "application/json", data.c_str());
      return;
    }
  }
  //request->send(404);
   mg_http_reply(c,404,"","");
}
#endif

void WebServer::push(msgType mt, const char *data,uint32_t id,uint32_t reconnect) {
  struct mg_connection *c;
  std::string msg=data;
  std::string type;
  switch (mt) {
      case PING: type="ping";break;
      case STATE: type="state";break;
      case LOG:   type="log";break;
      case CONFIG: type="config";break;
      case OTA:    type="ota";break;
      default: type="";break;
  }
  for (c = mgr.conns; c != NULL; c = c->next) {
    if (c->data[0] =='E') {
         if (id && reconnect)
           mg_printf(c,"id: %d\r\nretry: %d\r\nevent: %s\r\ndata: %s\r\n\r\n",id,reconnect,type.c_str(), msg.c_str());
        else
           mg_printf(c,"event: %s\r\ndata: %s\r\n\r\n",type.c_str(), msg.c_str());
   }       
      
    if (c->data[0] != 'W') continue;     
    if (mt==PING) 
         mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":\"%d\"}", "type",type.c_str(),"data",id); 
    else if (mt==STATE ) 
        mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":%s}", "type",type.c_str(),"data", msg.c_str());    
    else if(mt==LOG )
         mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":\"%s\"}", "type",type.c_str(),"data", msg.c_str());
    else if (mt==CONFIG )
         mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":%s}", "type",type.c_str(),"data", msg.c_str());
    else if (mt==OTA )
         mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":\"%s\"}", "type",type.c_str(),"data", msg.c_str());     
  }
}
   

size_t getMultipart(struct mg_str body, size_t ofs,
                              struct mg_http_part *part) {
  struct mg_str cd = mg_str_n("Content-Disposition", 19);
  const char *s = body.ptr;
  size_t b = ofs, h1, h2, b1, b2, max = body.len;

  // Init part params
  if (part != NULL) part->name = part->filename = part->body = mg_str_n(0, 0);

  while ((b + 2) < max && s[b] != '\r' && s[b + 1] != '\n') b++;
  if (b <= ofs || ( b + 2) >= max || (s[0] != '-' && s[1] !='-')) return 0;
 //  MG_INFO(("B: %lu %lu [%.*s]", ofs, b - ofs, (int) (b - ofs), s));

  // Skip headers
  h1 = h2 = b + 2;
  for (;;) {
    while (h2 + 2 < max && s[h2] != '\r' && s[h2 + 1] != '\n') h2++;
    if (h2 == h1) break;
    if (h2 + 2 >= max) return 0;
   //  MG_INFO(("Header: [%.*s]", (int) (h2 - h1), &s[h1]));
    if (part != NULL && h1 + cd.len + 2 < h2 && s[h1 + cd.len] == ':' &&
        mg_ncasecmp(&s[h1], cd.ptr, cd.len) == 0) {
      struct mg_str v = mg_str_n(&s[h1 + cd.len + 2], h2 - (h1 + cd.len + 2));
      part->name = mg_http_get_header_var(v, mg_str_n("name", 4));
      part->filename = mg_http_get_header_var(v, mg_str_n("filename", 8));
    }
    h1 = h2 = h2 + 2;
  }
  b1 = b2 = h2 + 2;
  //MG_INFO(("B2: %lu [%.*s]",b1,  10, &s[b2]));  
 
 return b2;

}        

char WebServer::matchBuf[MATCH_BUF_SIZE];
uint8_t WebServer::matchIndex=0;

void WebServer::ev_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    WebServer * srv=static_cast<WebServer*>(webServerPtr);
    bool final=false;
    if (ev == MG_EV_WS_MSG) {
        // Got websocket frame. Received data is wm->data. Echo it back!
            struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
            DynamicJsonDocument doc(100);
            char buf[wm->data.len + 1];
            strncpy(buf,wm->data.ptr,wm->data.len);
            buf[wm->data.len]=0;
            deserializeJson(doc,buf);
            std::string key=doc["key"];
            int partition=doc["partition"];
            srv->callKeyService(key.c_str(),partition);
            
            //mg_ws_send(c, wm->data.ptr, wm->data.len, WEBSOCKET_OP_TEXT);
    } else if (ev == MG_EV_READ && !c->is_websocket && c->data[0] != 'E') {
    // Parse the incoming data ourselves. If we can parse the request,
    // store two size_t variables in the c->data: expected len and recv len.
    size_t *data = (size_t *) c->data;
    if (data[0]  ) {  // Already parsed, simply print received data
     if (data[2] >= c->recv.len) {
         data[0]=0;
         c->recv.len = 0;          
          srv->push(OTA,"OTA update failed... Please try again.");
          mg_http_reply(c, 200, "Content-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\n\r\n", "FAIL\r");         
         return;
     }
     size_t ml=strlen(matchBuf);
     char *b = (char *) c->recv.buf + data[2];
     size_t bl = c->recv.len - data[2];
 

     size_t x=0;
     for (x;x<bl;x++) {
         bool firstLoop=1;
         for (size_t y=matchIndex;y<ml;y++) {
            
           if ((y+x)>= bl) 
               break;

           if (b[y+x] ==  matchBuf[y]) {
              matchIndex++;
           } else {
               if (x==0 && matchIndex>0 && firstLoop) {
                   srv->handleUpload(data[0],"upload.bin",data[1],(uint8_t*)matchBuf,matchIndex,final);
                   data[1]+=matchIndex;
                   MG_INFO(("***** save partial matched data bindex=%lu, index=%lu -  [%.*s]",matchIndex,data[1],matchIndex,matchBuf));
                }
               matchIndex=0;               
               break;
           }
           firstLoop=false;
         }
         if (matchIndex) break;
     }

      if (matchIndex==ml && ml > 0) {
             matchIndex=0;
             final=true;
        MG_INFO(("**********Final  bytes %lu, final=%d, data0=%lu, total=%lu, data1=%lu",x,final,data[0],x+data[1],data[1]));              
        } 
      
      data[2]=0;  
      c->recv.len = 0;  // And cleanup the receive buffer. Streaming!
      bool res=true;

       res=srv->handleUpload(data[0],"upload.bin",data[1],(uint8_t*)b,x,final); 
  
      if (!res) {
        data[0]=0;
        mg_http_reply(c, 200, "Content-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\n\r\n", "FAIL\r"); 
        return;
      }
      data[1] += x;      

      if (final || data[1] >= data[0]) {
        data[0]=0;
        data[1]=0;
        mg_http_reply(c, 200, "Content-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\n\r\n", "OK\r"); 
        return;
      }
     
    } else {
      struct mg_http_message hm;
      int n = mg_http_parse((char *) c->recv.buf, c->recv.len, &hm);
      if (n < 0) mg_error(c, "Bad response");
      if (n > 0) {
        if (mg_http_match_uri(&hm, "/update")) {
          struct mg_str *ct = mg_http_get_header(&hm, "Content-Type");
          struct mg_str boundary = mg_str("");           
          if (ct != NULL) {
            boundary = mg_http_get_header_var(*ct, mg_str("boundary"));  
            MG_INFO(("before boundary check"));
            if (boundary.ptr != NULL && boundary.len > 0) {
             matchBuf[0]=13;
             matchBuf[1]=10;
             matchBuf[2]='-';
             matchBuf[3]='-';             
             memcpy(matchBuf+4,(char*) boundary.ptr, boundary.len);
             //matchBuf[boundary.len+4]='-';
            // matchBuf[boundary.len+5]='-';
             matchBuf[boundary.len+4]=0;
             MG_INFO(("datasize=%lu,len=%lu,boundary=[%.*s],sizeofdata=%lu,st=%lu",MG_DATA_SIZE,strlen(matchBuf),strlen(matchBuf),matchBuf,sizeof(c->data),sizeof(c->data[0])));
             matchIndex=0;
            } else
                return;
         } else 
            return;   
          struct mg_http_part part;
          size_t ofs = 0; 
          ofs=getMultipart(hm.body, ofs, &part);
          if (ofs > 0  && part.filename.len) { 
         MG_INFO(("Chunk name: [%.*s] filename: [%.*s] length: %lu bytes,ofs=%lu",
                 (int) part.name.len, part.name.ptr, (int) part.filename.len,
                 part.filename.ptr, (unsigned long) hm.body.len,ofs));          

          data[0] = hm.body.len  - ofs -  (strlen(matchBuf)+4);//total len + 2 boundary headers including terminating --\r\n
          data[1] = 0;//byte counter
          data[2] = n + ofs; //initial offset
          MG_INFO(("Got chunk len %lu,data0=%lu,data1=%lu,data2=%lu", 0,data[0],data[1],data[2]));          
          }
  
        } 
      }
    }

  }  else if (ev == MG_EV_HTTP_MSG) { 
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        if (mg_http_match_uri(hm, "/ws") && c->data[0] != 'E') {
      // Upgrade to websocket. From now on, a connection is a full-duplex
      // Websocket connection, which will receive MG_EV_WS_MSG events.
            mg_ws_upgrade(c, hm, NULL);
            c->data[0] = 'W'; 
            mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":%s}", "type","app_config","data", srv->get_config_json().c_str());
            mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"%s\":\"%s\",\"%s\":%s}", "type","key_config","data", srv->_json_keypad_config);        
            srv->entities_iterator_.begin(srv->include_internal_);
        
        }  else  if (mg_http_match_uri(hm, "/events") && !c->is_websocket) {
            mg_str *hdr =mg_http_get_header(hm, "Accept"); 
            if (hdr != NULL && mg_strstr(*hdr, mg_str("text/event-stream")) != NULL)  {
              c->data[0]='E';
              mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\ndata: %s\r\n\r\n", "Subscribed to Events");
              c->is_resp=0;
              mg_printf(c,"id: %d\r\nretry: %d\r\nevent: %s\r\ndata: %s\r\n\r\n",millis(),30000,"ping", srv->get_config_json().c_str());   
              mg_printf(c,"event: %s\r\ndata: %s\r\n\r\n","key_config", srv-> _json_keypad_config);   
               srv->entities_iterator_.begin(srv->include_internal_);
            } else
                mg_http_reply(c, 404,"", ""); 
        
        } else {
            srv->handleRequest(c,hm);
        }
  } 
       (void) fn_data, (void) ev_data;
}


void WebServer::handleRequest(struct mg_connection *c,void *ev_data) {
   struct mg_http_message *hm = (struct mg_http_message *) ev_data;    
  //if (request->url() == "/")
  if (mg_http_match_uri(hm, "/")) { 
    this->handle_index_request(c);
    return;
  }

#ifdef USE_WEBSERVER_CSS_INCLUDE
  //if (request->url() == "/0.css") {
  if (mg_http_match_uri(hm, "/0.css")) {       
    this->handle_css_request(c);
    return;
  }
#endif

#ifdef USE_WEBSERVER_JS_INCLUDE
  if (mg_http_match_uri(hm, "/0.js")) { 
  //if (request->url() == "/0.js") {
    this->handle_js_request(c);
    return;
  }
#endif

#ifdef USE_WEBSERVER_PRIVATE_NETWORK_ACCESS
  //if (request->method() == HTTP_OPTIONS && request->hasHeader(HEADER_CORS_REQ_PNA)) {
   // this->handle_pna_cors_request(c);
   // return;
  //}
   mg_str *hdr =mg_http_get_header(hm, HEADER_CORS_REQ_PNA); 
   if (mg_vcasecmp(&hm->method, "OPTIONS") == 0 && hdr != NULL) {  
    this->handle_pna_cors_request(c);
    return;
   }   

#endif
 UrlMatch match = match_url(&hm->uri);

#ifdef USE_SENSOR
  if (match.domain == "sensor") {
    this->handle_sensor_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_SWITCH
  if (match.domain == "switch") {
    this->handle_switch_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_BUTTON
  if (match.domain == "button") {
    this->handle_button_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (match.domain == "binary_sensor") {
    this->handle_binary_sensor_request(c, ev_data, match);
    return;
  }
#endif
#ifdef USE_FAN
  if (match.domain == "fan") {
    this->handle_fan_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_LIGHT
  if (match.domain == "light") {
    this->handle_light_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (match.domain == "text_sensor") {
    this->handle_text_sensor_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_COVER
  if (match.domain == "cover") {
    this->handle_cover_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_NUMBER
  if (match.domain == "number") {
    this->handle_number_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_TEXT
  if (match.domain == "text") {
    this->handle_text_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_SELECT
  if (match.domain == "select") {
    this->handle_select_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_CLIMATE
  if (match.domain == "climate") {
    this->handle_climate_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_LOCK
  if (match.domain == "lock") {
    this->handle_lock_request(c, ev_data, match);

    return;
  }
#endif


#if defined(USE_DSC_PANEL) || defined(USE_VISTA_PANEL)
  if (match.domain == "alarm_panel") {
    this->handle_alarm_panel_request(c, ev_data, match);
    return;
  }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  if (match.domain == "alarm_control_panel") {
    this->handle_alarm_control_panel_request(c, ev_data, match);
    return;
  }
#endif

   mg_http_reply(c,404,"","");
}

void WebServer::schedule_(std::function<void()> &&f) {
#ifdef USE_ESP32
  xSemaphoreTake(this->to_schedule_lock_, portMAX_DELAY);
  to_schedule_.push_back(std::move(f));
  xSemaphoreGive(this->to_schedule_lock_);
#else
  this->defer(std::move(f));
#endif
}

void WebServer::report_ota_error() {
#ifdef USE_ARDUINO
  StreamString ss;
  Update.printError(ss);
  char buf[100];
  ESP_LOGW(TAG, "OTA Update failed! Error: %s", ss.c_str());  
  snprintf(buf,100,"OTA Update failed! Error: %s", ss.c_str());
  std::string ebuf=escape_json(buf);
  this->push(OTA,ebuf.c_str());
#endif
}

bool WebServer::handleUpload(size_t bodylen,  const String &filename, size_t index,uint8_t *data, size_t len, bool final) {
    char buf[100];                                    
#ifdef USE_ARDUINO
  bool success;
  if (index == 0) {
    snprintf(buf,100,"OTA Update Start: %s", filename.c_str());
    this->push(OTA,buf);
    ESP_LOGI(TAG, "OTA Update Start: %s", filename.c_str());

    this->ota_read_length_ = 0;

    if (Update.isRunning()) {
      Update.abort();
      return false;
    }
    success = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);

    if (!success) {
      report_ota_error();
      return false;
    }
  } else if (Update.hasError()) {
     this->push(OTA,"OTA Update has error");      
    // don't spam logs with errors if something failed at start
    return false;
  }

  success = Update.write(data, len) == len;
  if (!success) {
    report_ota_error();
    return false;
  }
  this->ota_read_length_ += len;

  const uint32_t now = millis();
  if (now - this->last_ota_progress_ > 1000) {
    if (bodylen != 0) {
      float percentage = (this->ota_read_length_ * 100.0f) / bodylen;
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
      snprintf(buf,100,"OTA in progress: %0.1f%%", percentage);
      this->push(OTA,buf);
    } else {
      ESP_LOGD(TAG, "OTA in progress: %u bytes read", this->ota_read_length_);
      snprintf(buf,100, "OTA in progress: %u bytes read", this->ota_read_length_); 
      this->push(OTA,buf);      
    }

    this->last_ota_progress_ = now;
  }

  if (final) {
    if (Update.end(true)) {
      ESP_LOGI(TAG, "OTA update successful!");
      this->push(OTA,"OTA Update successful. Press F5 to reload this page.");      
      this->set_timeout(1000, []() { App.safe_reboot(); });
      return true;
    } else {
      report_ota_error();
      return false;
    }
  }
#endif
return true;
}

}  // namespace web_server
}  // namespace esphome
