#include "web_server.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif


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

#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif

#ifdef USE_WEBKEYPAD_OTA
#include "esphome/components/ota/ota_backend.h"
#endif

#ifdef USE_ARDUINO
#if defined(USE_ESP8266) || defined(USE_RP2040)
#include <Updater.h>
#elif defined(USE_ESP32) || defined(USE_LIBRETINY)
#include <Update.h>
#endif
#endif  // USE_ARDUINO

#ifdef USE_RP2040
char * strchrnul(const char * s, int c)
{
   while(*s)
   {
      if (c == *s) break;
      s++;
   }
   return const_cast<char *>(s);
}
#endif

namespace esphome
{
    namespace web_keypad
    {

#if defined(ESP8266)
#define FC(s) (String(PSTR(s)).c_str())
#define FCS(s) (String(PSTR(s)).c_str()) 
#else
#define FC(s) ((const char*)(s))
#define FCS(s) ((const char*)(s))
#endif
 
void ev_handler_cb(struct mg_connection *c, int ev, void *ev_data) {
    WebServer *srv = reinterpret_cast<WebServer *>(c->fn_data);
    if (srv != NULL)
            srv->ev_handler(c,ev,ev_data);

}


        static const char *const TAG = "web_server";
   
        void WebServer::parseUrlParams(char *queryString, int resultsMaxCt, bool decodeUrl, JsonObject doc)
        {
            int ct = 0;
            char *name;
            char *value;
            if (decodeUrl)
                percentDecode(queryString);
            while (queryString && *queryString && ct < resultsMaxCt)
            {
                name = strsep(&queryString, "&");
                value = strchrnul(name, '=');
                if (*value)
                    *value++ = '\0';
                doc[name] = value;
                ct++;
            }
        }

        /**
         * Perform URL percent decoding.
         * Decoding is done in-place and will modify the parameter.
         */

        void WebServer::percentDecode(char *src)
        {
            char *dst = src;

            while (*src)
            {
                if (*src == '+')
                {
                    src++;
                    *dst++ = ' ';
                }
                else if (*src == '%')
                {
                    // handle percent escape

                    *dst = '\0';
                    src++;

                    if (*src >= '0' && *src <= '9')
                    {
                        *dst = *src++ - '0';
                    }
                    else if (*src >= 'A' && *src <= 'F')
                    {
                        *dst = 10 + *src++ - 'A';
                    }
                    else if (*src >= 'a' && *src <= 'f')
                    {
                        *dst = 10 + *src++ - 'a';
                    }

                    // this will cause %4 to be decoded to ascii @, but %4 is invalid
                    // and we can't be expected to decode it properly anyway

                    *dst <<= 4;

                    if (*src >= '0' && *src <= '9')
                    {
                        *dst |= *src++ - '0';
                    }
                    else if (*src >= 'A' && *src <= 'F')
                    {
                        *dst |= 10 + *src++ - 'A';
                    }
                    else if (*src >= 'a' && *src <= 'f')
                    {
                        *dst |= 10 + *src++ - 'a';
                    }

                    dst++;
                }
                else
                {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';
        }

        void WebServer::parseUrl(mg_http_message *hm, JsonObject doc)
        {
            std::string url = std::string(hm->uri.buf, hm->uri.len);
            std::string query;
            if (hm->query.len > 0)
                query = std::string(hm->query.buf, hm->query.len);
            else
                query = std::string(hm->body.buf, hm->body.len);

            // MG_INFO(("url=%s,query=%s",url.c_str(),query.c_str()));
            parseUrlParams((char *)query.c_str(), 15, true, doc);
            doc["method"] = std::string(hm->method.buf, hm->method.len);
            size_t domain_end = url.find('/', 1);
            if (domain_end == std::string::npos)
                return;
           doc[FC("domain")] = url.substr(1, domain_end - 1);
            if (url.length() == domain_end - 1)
                return;
            size_t id_begin = domain_end + 1;
            size_t id_end = url.find('/', id_begin);
            if (id_end == std::string::npos)
            {
                doc[FC("oid")] = url.substr(id_begin, url.length() - id_begin);
            }
            doc[FC("oid")] = url.substr(id_begin, id_end - id_begin);
            size_t method_begin = id_end + 1;
            doc[FC("action")] = url.substr(method_begin, url.length() - method_begin);
        }
       
        void WebServer::ws_reply(mg_connection *c, const char *data, bool ok)
        {
            std::string newdata=std::string(data);
            if (!c->is_websocket)
            {
                if (ok)
                {
                    if (strlen(data) == 0)
                    {
                        mg_http_reply(c, 204, FC("Access-Control-Allow-Origin: *\r\n"), "");
                    }
                    else
                    {
                #ifdef USE_WEBKEYPAD_ENCRYPTION
                        if (get_credentials()->crypt)
                        {
                            if (c->is_authenticated)
                            {
                                mg_http_reply(c, 404, "", "");
                                return;
                            }
                            else
                                encrypt(newdata);
                        }
                #endif
                        //  ESP_LOGD(TAG,"sending %s",data);
                        mg_http_reply(c, 200, FC("Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n"), "%s", newdata.c_str());
                    }
                }
                else
                    mg_http_reply(c, 404, "", "");
            }
        }

std::string WebServer::get_object_id(EntityBase * entity) {
     const StringRef &name = entity->get_name();
 #if defined(USE_DSC_PANEL) || defined(USE_VISTA_PANEL)
  //ESP_LOGD("test","checking  name: %s,hash: %d",entity->get_name().c_str(),entity->get_object_id_hash());
  const char *  oid = alarm_panel::alarmPanelPtr->getIdType(entity->get_object_id_hash());
  if (strcmp(oid,"") != 0)  return std::string(oid);
 #endif
 
  #if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 1, 0)
  return  entity->get_object_id();
  #else
  char object_id_buf[128];
  return std::string(entity->get_object_id_to(object_id_buf));
  #endif
 }


       WebServer::WebServer()
            : entities_iterator_(ListEntitiesIterator(this))
        {
#ifdef USE_ESP32
            to_schedule_lock_ = xSemaphoreCreateMutex();
#endif
            credentials_ = new Credentials;
        }


        WebServer::~WebServer() {
            delete credentials_;
        }

// #ifdef USE_WEBKEYPAD_CSS_INCLUDE
//         void WebServer::set_css_include(const char *css_include) { this->css_include_ = css_include; }
// #endif
// #ifdef USE_WEBKEYPAD_JS_INCLUDE
//         void WebServer::set_js_include(const char *js_include) { this->js_include_ = js_include; }
// #endif

        void WebServer::get_keypad_config(std::string &out)
        {
            #ifdef ESP8266
           char buf[ESPHOME_WEBKEYPAD_CONFIG_INCLUDE_SIZE];
           memcpy_P(buf,ESPHOME_WEBKEYPAD_CONFIG_INCLUDE,ESPHOME_WEBKEYPAD_CONFIG_INCLUDE_SIZE);
           #else
            const char *buf = (const char *)ESPHOME_WEBKEYPAD_CONFIG_INCLUDE;
           #endif

           out = std::string(buf,ESPHOME_WEBKEYPAD_CONFIG_INCLUDE_SIZE);
        }

        void WebServer::get_config_json(unsigned long cid,std::string & out)
        {

            uint8_t key[16];
            random_bytes(key, 16);
            c_data cd;
            std::string token = base64_encode(key, 16);
            cd.token = token;
            cd.lastseq = 0;
            tokens_[cid] = cd;
            json::JsonBuilder builder;
            JsonObject root = builder.root(); 
            root[FC("title")] = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
            root[FC("comment")] = App.get_comment();
            root[FC("ota")] = this->allow_ota_;
            root[FC("log")] = this->expose_log_;
            root[FC("lang")] = "en";
            root[FC("partitions")] = this->partitions_;
            root[FC("keypad")] = this->show_keypad_;
            root[FC("crypt")] = this->crypt_;
            root[FC("cid")] = cid;
            root[FC("token")] = token;
            out=builder.serialize();
        }

        void WebServer::escape_json(const char *input,std::string & output)
        {
            for (int i = 0; i < strlen(input); i++)
            {

                switch (input[i])
                {
                case 0x1b:
                    output += "\x1b";
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

        }

        

void WebServer::setup()
{

    ControllerRegistry::register_controller(this);
    mg_log_set(MG_LL_ERROR); //MG_LL_NONE, MG_LL_ERROR, MG_LL_INFO, MG_LL_DEBUG, MG_LL_VERBOSE
    mg_mgr_init(&mgr);
#if ESPHOME_VERSION_CODE < VERSION_CODE(2025, 12, 0)
#ifdef USE_LOGGER
    if (logger::global_logger != nullptr && this->expose_log_)
    {
        logger::global_logger->add_on_log_callback(
            [this](int level, const char *tag, const char *message, size_t message_len)
            {
                (void) message_len;
                std::string msg;
                escape_json(message,msg);
                this->push(LOG, msg.c_str());
            });
    }
#endif
#else
#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->expose_log_) {
    logger::global_logger->add_log_listener(this);
  }
#endif
#endif
  this->set_interval(10000, [this](){ this->push(PING, "", millis(), 30000); });

  ESP_LOGD(TAG,"Web keypad setup completed");
}



void WebServer::loop()
{
#ifdef USE_ESP32
    if (xSemaphoreTake(this->to_schedule_lock_, 0L))
    {
        std::function<void()> fn;
        if (!to_schedule_.empty())
        {
            // scheduler execute things out of order which may lead to incorrect state
            // let's execute it directly from the loop
            fn = std::move(to_schedule_.front());
            to_schedule_.pop_front();
        }
        xSemaphoreGive(this->to_schedule_lock_);
        if (fn)
        {
            fn();
        }
    }
#endif
    if (!this->entities_iterator_.completed())
        this->entities_iterator_.advance();

   #if defined(USE_WIFI) && !defined(USE_CAPTIVE_PORTAL)
   is_ap_active_ = wifi::global_wifi_component->is_ap_active();
   #endif

    if (firstrun_ && ( network::is_connected() || is_ap_active_ ))
    {
        char addr[30];
        snprintf(addr,30, "http://0.0.0.0:%d", port_);
        ESP_LOGD(TAG, "Starting web server on %s:%d", network::get_use_address(), port_);
        struct mg_connection *c = mg_http_listen(&mgr, addr,&ev_handler_cb,this);
        firstrun_ = false;
        
    }
    
    mg_mgr_poll(&mgr, 0);

}

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 12, 0)
 #ifdef USE_LOGGER
void WebServer::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  (void) level;
  (void) tag;
  (void) message_len;
                std::string msg;
                escape_json(message,msg);
                this->push(LOG, msg.c_str());
   }
#endif
#endif
void WebServer::dump_config()
{
    ESP_LOGCONFIG(TAG, "Web Server:");
    ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address(), port_);
}
float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }


void WebServer::handle_index_request(struct mg_connection *c)
{
    const char *buf = (const char *)ESPHOME_WEBKEYPAD_INDEX_HTML;
    mg_printf(c, FC("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n"), ESPHOME_WEBKEYPAD_INDEX_HTML_SIZE);
    mg_send(c, buf, ESPHOME_WEBKEYPAD_INDEX_HTML_SIZE);
    c->is_resp = 0;
    //c->is_draining=1;

}

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
void WebServer::handle_pna_cors_request(struct mg_connection *c)
{

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
        const char* const HEADER_PNA_NAME  = FCS("Private-Network-Access-Name");
        const char* const HEADER_PNA_ID = FCS("Private-Network-Access-ID");
        const char* const HEADER_CORS_ALLOW_PNA =  FCS("Access-Control-Allow-Private-Network");
#endif
    std::string mac = get_mac_address_pretty();
    mg_printf(c, FC("HTTP/1.1 200 OK\r\n%s:%s\r\n%s:%s\r\n%s:%s\r\n\r\n"), HEADER_CORS_ALLOW_PNA, "true", HEADER_PNA_NAME, App.get_name().c_str(), HEADER_PNA_ID, mac.c_str());
    c->is_resp = 0;
   // c->is_draining=1;
}
#endif

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
void WebServer::handle_css_request(struct mg_connection *c)
{
    const char *buf = (const char *)ESPHOME_WEBKEYPAD_CSS_INCLUDE;
    mg_printf(c, FC("HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\nContent-Encoding: gzip\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n"), ESPHOME_WEBKEYPAD_CSS_INCLUDE_SIZE);
    mg_send(c, buf, ESPHOME_WEBKEYPAD_CSS_INCLUDE_SIZE);
    c->is_resp = 0;
    //c->is_draining=1;
}
#endif


#ifdef USE_WEBKEYPAD_JS_INCLUDE

//large file so we send in 1k blocks on every loop iteration
void WebServer::send_js_include(mg_connection *c){
    const size_t BS=1024;       
    uint32_t  index = *(uint32_t *) c->data;  //use connection data array to store curent index
    const char *buf = (const char *)ESPHOME_WEBKEYPAD_JS_INCLUDE;
    size_t blocksize = index + BS <= ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE ? BS : ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE - index;
    if (c->send.len < blocksize  && mg_send(c, &buf[index],blocksize)) {
        index=index+blocksize;
        *(uint32_t *) c->data=(uint32_t) index;
    }

    if (index >= ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE) {
        c->is_resp = 0;
        c->is_sending = 0; 
        *(uint32_t *) c->data = (uint32_t)0;
        //c->is_draining = 1;
    }
                
}


void WebServer::handle_js_request(struct mg_connection *c)
{
        mg_printf(c,FC("HTTP/1.1 200 OK\r\nContent-Type: text/javascript; charset=utf-8\r\nContent-Encoding: gzip\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n"), ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE);
        c->is_sending  =1; 
        *(uint32_t *) c->data = (uint32_t)0;
        send_js_include(c);
}
#endif

#define set_json_id(root, obj, sensor, start_config)                            \
    (root)["id"] = std::string(sensor) + "-" + get_object_id(obj);              \
    if (((start_config) == DETAIL_ALL))                                         \
    {                                                                           \
        (root)[FC("name")] = (obj)->get_name();                                     \
        (root)[FC("icon")] = (obj)->get_icon_ref();                                 \
        (root)[FC("entity_category")] = (obj)->get_entity_category();               \
        (root)[FC("domain")] = sensor;                                              \
        if ((obj)->is_disabled_by_default())                                    \
            (root)[FC("is_disabled_by_default")] = (obj)->is_disabled_by_default(); \
    }


#define set_json_value(root, obj, sensor, value, start_config) \
    set_json_id((root), (obj), sensor, start_config);          \
    (root)[FC("value")] = value;

#define set_json_icon_state_value(root, obj, sensor, state, value, start_config) \
    set_json_value(root, obj, sensor, value, start_config);                      \
    (root)[FC("state")] = state;

#ifdef USE_SENSOR
        void WebServer::on_sensor_update(sensor::Sensor *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            this->push(STATE, this->sensor_json(obj, obj->state, DETAIL_STATE).c_str());
        }

        void WebServer::handle_sensor_request(mg_connection *c, JsonObject doc)
        {
            for (sensor::Sensor *obj : App.get_sensors())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;
                auto detail = DETAIL_STATE;
                if (doc[FC("detail")].is<JsonVariant>())
                {
                    if (doc[FC("detail")] == "all")
                    {
                        detail = DETAIL_ALL;
                    }
                }
                std::string data = this->sensor_json(obj, obj->state, detail);
                ws_reply(c, data.c_str(), true);
                return;
            }
            ws_reply(c, "", false);
        }

    std::string WebServer::sensor_json(sensor::Sensor *obj, float value, JsonDetail start_config)
    {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {

    const auto uom_ref = obj->get_unit_of_measurement_ref();
    #if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 1, 0)  
    std::string state;
    if (std::isnan(value)) {
      state = "NA";
    } else {
      state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
      if (!uom_ref.empty())
        state += " " + uom_ref;
    }
    #else

  char buf[VALUE_ACCURACY_MAX_LEN];
  const char *state = std::isnan(value)
                          ? "NA"
                          : (value_accuracy_with_uom_to_buf(buf, value, obj->get_accuracy_decimals(), uom_ref), buf);
    #endif
    set_json_icon_state_value(root, obj, "sensor", state, value, start_config);
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
      if (!uom_ref.empty())
        root["uom"] = uom_ref;
    } });
        }
#endif



void WebServer::handle_wifisave(struct mg_connection *c, JsonObject doc) {
 #ifdef USE_WIFI
        std::string ssid="";
        std::string psk="";
        if (doc["ssid"].is<JsonVariant>()) 
            ssid=std::string(doc["ssid"]);
        if (doc["psk"].is<JsonVariant>())
            psk=std::string(doc["psk"]);
      ESP_LOGI(TAG,
           "Requested WiFi Settings Change:\n"
           "  SSID='%s'\n"
           "  Password=" LOG_SECRET("'%s'"),
           ssid.c_str(), psk.c_str());

   if (ssid !="") {
   wifi::global_wifi_component->save_wifi_sta(ssid, psk); 
    ws_reply(c, "", true);
    return;
   }
#endif
ws_reply(c, "", false);
   //request->redirect(ESPHOME_F("/?save"));
}

#ifdef USE_TEXT_SENSOR

         void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            this->push(STATE, text_sensor_json(obj,obj->state,DETAIL_STATE).c_str());
        }

std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value,
                                        JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();
  root[FC("id_code")] = get_object_id(obj);
  set_json_icon_state_value(root, obj, "text_sensor", value, value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}

        void WebServer::handle_text_sensor_request(mg_connection *c, JsonObject doc)
        {
            for (text_sensor::TextSensor *obj : App.get_text_sensors())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;
                auto detail = DETAIL_STATE;
                if (doc[FC("detail")].is<JsonVariant>() && doc[FC("detail")] == "all")
                {
                        detail = DETAIL_ALL;
                }
                std::string data = this->text_sensor_json(obj, obj->state, detail);

                ws_reply(c, data.c_str(), true);
                return;
            }
            ws_reply(c, "", false);
        }



#endif

#ifdef USE_SWITCH
        void WebServer::on_switch_update(switch_::Switch *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->switch_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->switch_json(obj, obj->state, DETAIL_STATE).c_str());
        }

        std::string WebServer::switch_json(switch_::Switch *obj, bool value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
        set_json_icon_state_value(root, obj, "switch", value ? "ON" : "OFF", value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  } });
        }
        void WebServer::handle_switch_request(mg_connection *c, JsonObject doc)
        {
            for (switch_::Switch *obj : App.get_switches())
            {

                if (get_object_id(obj) != doc[FC("oid")])
                             continue;
                if (doc[FC("action")] == "get")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->switch_json(obj, obj->state, detail);
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc[FC("action")] == "toggle")
                {
                    this->schedule_([obj]()
                                    { obj->toggle(); });
                    // mg_http_reply(c,200,"","");
                    ws_reply(c, "", true);
                }
                else if (doc[FC("action")] == "turn_on")
                {
                    this->schedule_([obj]()
                                    { obj->turn_on(); });
                    // mg_http_reply(c,200,"","");
                    ws_reply(c, "", true);
                }
                else if (doc[FC("action")] == "turn_off")
                {
                    this->schedule_([obj]()
                                    { obj->turn_off(); });
                    ws_reply(c, "", true);
                }
                else
                {
                    ws_reply(c, "", false);
                }
                return;
            }
            ws_reply(c, "", false);
        }
#endif

#ifdef USE_BUTTON
        std::string WebServer::button_json(button::Button *obj, JsonDetail start_config)
        {
            return json::build_json(
                [this, obj, start_config](JsonObject root)
                {
                    set_json_id(root, obj, "button-" + get_object_id(obj), start_config);
                    if (start_config == DETAIL_ALL)
                    {
                    this->add_sorting_info_(root, obj);
                    }
                });
        }

        void WebServer::handle_button_request(mg_connection *c, JsonObject doc)
        {
            for (button::Button *obj : App.get_buttons())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;
                if (doc["method"] == "GET" && doc["method"] == "")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->button_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["method"] == "POST" && doc[FC("action")] == "press")
                {
                    this->schedule_([obj]()
                                    { obj->press(); });
                    ws_reply(c, "", true);
                }
                else
                {
                    ws_reply(c, "", false);
                }
                return;
            }
            ws_reply(c, "", false);
        }
#endif

#ifdef USE_BINARY_SENSOR

  

        void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj)
        {
          if (!this->include_internal_ && obj->is_internal())
                 return;
            this->push(STATE, binary_sensor_json(obj,obj->state,DETAIL_STATE).c_str());
         }

std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config) {
  json::JsonBuilder builder;
  JsonObject root = builder.root();
  root[FC("id_code")] = get_object_id(obj);
  set_json_icon_state_value(root, obj, "binary_sensor" , value ? "ON" : "OFF", value, start_config);
  if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
  }

  return builder.serialize();
}

        void WebServer::handle_binary_sensor_request(mg_connection *c, JsonObject doc)
        {
            for (binary_sensor::BinarySensor *obj : App.get_binary_sensors())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;
                auto detail = DETAIL_STATE;
                if (doc[FC("detail")].is<JsonVariant>())
                {
                    if (doc[FC("detail")] == "all")
                    {
                        detail = DETAIL_ALL;
                    }
                }
                std::string data = this->binary_sensor_json(obj, obj->state, detail);
                ws_reply(c, data.c_str(), true);
                return;
            }
            ws_reply(c, "", false);
        }
#endif

#ifdef USE_FAN
        void WebServer::on_fan_update(fan::Fan *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->p.send(this->fan_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->fan_json(obj, DETAIL_STATE).c_str());
        }
        std::string WebServer::fan_json(fan::Fan *obj, JsonDetail start_config)
        {

            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    // set_json_state_value(root, obj, "fan-" + get_object_id(obj), obj->state ? "ON" : "OFF", obj->state, start_config);
    // const auto traits = obj->get_traits();
    // if (traits.supports_speed()) {
    //   root["speed_level"] = obj->speed;
    //   root["speed_count"] = traits.supported_speed_count();
    // }
    // if (obj->get_traits().supports_oscillation())
    //   root["oscillation"] = obj->oscillating; });
        set_json_icon_state_value(root, obj, "fan", obj->state ? "ON" : "OFF", obj->state,
                              start_config);
    const auto traits = obj->get_traits();
    if (traits.supports_speed()) {
      root["speed_level"] = obj->speed;
      root["speed_count"] = traits.supported_speed_count();
    }
    if (obj->get_traits().supports_oscillation())
      root["oscillation"] = obj->oscillating;
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
        void WebServer::handle_fan_request(mg_connection *c, JsonObject doc)
        {

            for (fan::Fan *obj : App.get_fans())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->fan_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc[FC("action")] == "toggle")
                {
                    this->schedule_([obj]()
                                    { obj->toggle().perform(); });
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc[FC("action")] == "turn_on")
                {
                    auto call = obj->turn_on();
                    // if (request->hasParam("speed_level")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"speed_level",buf,sizeof(buf)) > 0) {
       if (doc["speed_level"].is<JsonVariant>()) {
                        // auto speed_level = buf;
                        auto val = parse_number<int>(doc["speed_level"]);
                        if (!val.has_value())
                        {
                            ESP_LOGW(TAG, "Can't convert '%s' to number!", speed_level.c_str());
                            return;
                        }
                        call.set_speed(*val);
      }
     // if (request->hasParam("oscillation")) {
    //char buf[100];
    //if (mg_http_get_var(&hm->body,"oscillation",buf,sizeof(buf)) > 0) {         
     //   auto speed = buf;
        if (doc["oscillation"].is<JsonVariant>()) {
                        auto val = parse_on_off(doc["oscillation"]);
                        switch (val)
                        {
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
                            ws_reply(c, "", false);
                            return;
                        }
      }
      this->schedule_([call]() mutable {
                        call.perform(); });
            ws_reply(c,"",true);
                }
                else if (doc[FC("action")] == "turn_off")
                {
                    this->schedule_([obj]()
                                    { obj->turn_off().perform(); });
                    ws_reply(c, "", true);
                }
                else
                {
                    ws_reply(c, "", false);
                }
                return;
            }

            ws_reply(c, "", false);
        }
#endif

#ifdef USE_LIGHT
        void WebServer::on_light_update(light::LightState *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->light_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->light_json(obj, DETAIL_STATE).c_str());
        }
        void WebServer::handle_light_request(mg_connection *c, JsonObject doc)
        {

            for (light::LightState *obj : App.get_lights())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->light_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc[FC("action")] == "toggle")
                {
                    this->schedule_([obj]()
                                    { obj->toggle().perform(); });
                    // mg_http_reply(c,200,"","");
                    ws_reply(c, "", true);
                }
                else if (doc[FC("action")] == "turn_on")
                {
                    auto call = obj->turn_on();
                    // if (request->hasParam("brightness")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"brightness",buf,sizeof(buf)) > 0) {
                    if (doc["brightness"].is<JsonVariant>())
                    {
                        std::string num = doc["brightness"];
                        auto brightness = parse_number<float>(num);
                        if (brightness.has_value())
                        {
                            call.set_brightness(*brightness / 255.0f);
                        }
                    }
                    // if (request->hasParam("r")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"r",buf,sizeof(buf)) > 0) {
                    if (doc["r"].is<JsonVariant>())
                    {
                        std::string num = doc["r"];
                        auto r = parse_number<float>(num);
                        if (r.has_value())
                        {
                            call.set_red(*r / 255.0f);
                        }
                    }
                    // if (request->hasParam("g")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"g",buf,sizeof(buf)) > 0) {
                    if (doc["g"].is<JsonVariant>())
                    {
                        std::string num = doc["g"];
                        auto g = parse_number<float>(num);
                        if (g.has_value())
                        {
                            call.set_green(*g / 255.0f);
                        }
                    }
                    // if (request->hasParam("b")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"b",buf,sizeof(buf)) > 0) {
                    if (doc["b"].is<JsonVariant>())
                    {
                        std::string num = doc["b"];
                        auto b = parse_number<float>(num);
                        if (b.has_value())
                        {
                            call.set_blue(*b / 255.0f);
                        }
                    }
                    // if (request->hasParam("white_value")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"white_value",buf,sizeof(buf)) > 0) {
                    if (doc["white_value"].is<JsonVariant>())
                    {
                        std::string num = doc["white_value"];
                        auto white_value = parse_number<float>(num);
                        if (white_value.has_value())
                        {
                            call.set_white(*white_value / 255.0f);
                        }
                    }
                    // if (request->hasParam("color_temp")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"color_temp",buf,sizeof(buf)) > 0) {
                    if (doc["color_temp"].is<JsonVariant>())
                    {
                        std::string num = doc["color_temp"];
                        auto color_temp = parse_number<float>(num);
                        if (color_temp.has_value())
                        {
                            call.set_color_temperature(*color_temp);
                        }
                    }
                    // if (request->hasParam("flash")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"flash",buf,sizeof(buf)) > 0) {
                    if (doc["flash"].is<JsonVariant>())
                    {
                        std::string num = doc["flash"];
                        auto flash = parse_number<uint32_t>(num);
                        if (flash.has_value())
                        {
                            call.set_flash_length(*flash * 1000);
                        }
                    }
                    // if (request->hasParam("transition")) {
                    //  char buf[100];
                    // if (mg_http_get_var(&hm->body,"transition",buf,sizeof(buf)) > 0) {
                    if (doc["transition"].is<JsonVariant>())
                    {
                        std::string num = doc["transition"];
                        auto transition = parse_number<uint32_t>(num);
                        if (transition.has_value())
                        {
                            call.set_transition_length(*transition * 1000);
                        }
                    }
                    // if (request->hasParam("effect")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"effect",buf,sizeof(buf)) > 0) {
                    if (doc["effect"].is<JsonVariant>())
                    {
                        const char *effect = doc["effect"];
                        call.set_effect(effect);
                    }

                    this->schedule_([call]() mutable
                                    { call.perform(); });
                    ws_reply(c, "", true);
                } // else if (match.method == "turn_off") {
                else if (doc[FC("action")] == "turn_off")
                {
                    auto call = obj->turn_off();
                    // if (request->hasParam("transition")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"transition",buf,sizeof(buf)) > 0) {
                    if (doc["transition"].is<JsonVariant>())
                    {
                        std::string num = doc["transition"];
                        auto transition = parse_number<uint32_t>(num);
                        if (transition.has_value())
                        {
                            call.set_transition_length(*transition * 1000);
                        }
                    }
                    this->schedule_([call]() mutable
                                    { call.perform(); });
                    ws_reply(c, "", true);
                }
                else
                {
                    ws_reply(c, "", false);
                }
                return;
            }

            ws_reply(c, "", false);
        }
        std::string WebServer::light_json(light::LightState *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    // set_json_id(root, obj, "light-" + get_object_id(obj), start_config);
    // root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

    // light::LightJSONSchema::dump_json(*obj, root);
    // if (start_config == DETAIL_ALL) {
    //   JsonArray opt = root.createNestedArray("effects");
    //   opt.add("None");
    //   for (auto const &option : obj->get_effects()) {
    //     opt.add(option->get_name());
    //   }
    // } });
        set_json_id(root, obj, "light-" + get_object_id(obj), start_config);
    root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

    light::LightJSONSchema::dump_json(*obj, root);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("effects");
      opt.add("None");
      for (auto const &option : obj->get_effects()) {
        opt.add(option->get_name());
      }
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

#ifdef USE_COVER
        void WebServer::on_cover_update(cover::Cover *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->cover_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->cover_json(obj, DETAIL_STATE).c_str());
        }
        void WebServer::handle_cover_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (cover::Cover *obj : App.get_covers())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->cover_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                    continue;
                }

                auto call = obj->make_call();
                // if (match.method == "open") {
                if (doc[FC("action")] == "open")
                {
                    call.set_command_open();
                    // } else if (match.method == "close") {
                }
                else if (doc[FC("action")] == "close")
                {
                    call.set_command_close();
                    //} else if (match.method == "stop") {
                }
                else if (doc[FC("action")] == "stop")
                {
                    call.set_command_stop();
                    // } else if (match.method != "set") {
                }
                else if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto traits = obj->get_traits();

                // if ((request->hasParam("position") && !traits.get_supports_position()) ||
                //(request->hasParam("tilt") && !traits.get_supports_tilt())) {
                // char buf[50];
                // bool p= (mg_http_get_var(&hm->body,"position",buf,sizeof(buf)) > 0);
                if ((doc["position"].is<JsonVariant>() && !traits.get_supports_position()) || (doc["tilt"].is<JsonVariant>() && !traits.get_supports_tilt()))
                {

                    // bool t= (mg_http_get_var(&hm->body,"tilt",buf,sizeof(buf)) > 0);
                    //  if (p && !traits.get_supports_position()) || t && !traits.get_supports_tilt())) {
                    // request->send(409);
                    ws_reply(c, "", false);
                    return;
                }

                // char buf[100];
                // if (mg_http_get_var(&hm->body,"position",buf,sizeof(buf)) > 0) {
                if (doc["position"].is<JsonVariant>())
                {
                    auto position = parse_number<float>(doc["position"]);
                    if (position.has_value())
                    {
                        call.set_position(*position);
                    }
                }
                // if (mg_http_get_var(&hm->body,"tilt",buf,sizeof(buf)) > 0) {
                if (doc["tilt"].is<JsonVariant>())
                {
                    auto tilt = parse_number<float>(doc["tilt"]);
                    if (tilt.has_value())
                    {
                        call.set_tilt(*tilt);
                    }
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
        }
        std::string WebServer::cover_json(cover::Cover *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    // set_json_state_value(root, obj, "cover-" + get_object_id(obj), obj->is_fully_closed() ? "CLOSED" : "OPEN",
    //                      obj->position, start_config);
    // root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    // if (obj->get_traits().get_supports_tilt())
    //   root["tilt"] = obj->tilt; });
      set_json_icon_state_value(root, obj, "cover" , obj->is_fully_closed() ? "CLOSED" : "OPEN",
                              obj->position, start_config);
    root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_position())
      root["position"] = obj->position;
    if (obj->get_traits().get_supports_tilt())
      root["tilt"] = obj->tilt;
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

#ifdef USE_NUMBER
        void WebServer::on_number_update(number::Number *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->number_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->number_json(obj, obj->state, DETAIL_STATE).c_str());
        }
        void WebServer::handle_number_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (auto *obj : App.get_numbers())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->number_json(obj, obj->state, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();

                if (doc[FC("value")].is<JsonVariant>())
                {
                    std::string value = doc[FC("value")];
                    auto value = parse_number<float>(value);
                    if (value.has_value())
                        call.set_value(*value);
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }            ws_reply(c, "", false);
        }

        std::string WebServer::number_json(number::Number *obj, float value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
       set_json_id(root, obj, "number-" + get_object_id(obj), start_config);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 1, 0)  
    if (start_config == DETAIL_ALL) {
      root["min_value"] =
          value_accuracy_to_string(obj->traits.get_min_value(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["max_value"] =
          value_accuracy_to_string(obj->traits.get_max_value(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["step"] =
          value_accuracy_to_string(obj->traits.get_step(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["mode"] = (int) obj->traits.get_mode();
      if (!obj->traits.get_unit_of_measurement_ref().empty())
        root["uom"] = obj->traits.get_unit_of_measurement_ref();
            this->add_sorting_info_(root, obj);
    }
#else 
 char val_buf[VALUE_ACCURACY_MAX_LEN];
  char state_buf[VALUE_ACCURACY_MAX_LEN];
  const char *val_str = std::isnan(value) ? "\"NaN\"" : (value_accuracy_to_buf(val_buf, value, accuracy), val_buf);
  const char *state_str =
      std::isnan(value) ? "NA" : (value_accuracy_with_uom_to_buf(state_buf, value, accuracy, uom_ref), state_buf);
  set_json_icon_state_value(root, obj, "number", state_str, val_str, start_config);
  if (start_config == DETAIL_ALL) {
    // ArduinoJson copies the string immediately, so we can reuse val_buf
    root[ESPHOME_F("min_value")] = (value_accuracy_to_buf(val_buf, obj->traits.get_min_value(), accuracy), val_buf);
    root[ESPHOME_F("max_value")] = (value_accuracy_to_buf(val_buf, obj->traits.get_max_value(), accuracy), val_buf);
    root[ESPHOME_F("step")] = (value_accuracy_to_buf(val_buf, obj->traits.get_step(), accuracy), val_buf);
    root[ESPHOME_F("mode")] = (int) obj->traits.get_mode();
    if (!uom_ref.empty())
      root[ESPHOME_F("uom")] = uom_ref;
          this->add_sorting_info_(root, obj);
    }

#endif


    if (std::isnan(value)) {
      root["value"] = "\"NaN\"";
      root["state"] = "NA";
    } else {
      root["value"] = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      std::string state = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      if (!obj->traits.get_unit_of_measurement_ref().empty())
        state += " " + obj->traits.get_unit_of_measurement_ref();
      root["state"] = state;
    } });
        }
#endif

#ifdef USE_DATETIME_DATE
        void WebServer::on_date_update(datetime::DateEntity *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->date_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->date_json(obj, DETAIL_STATE).c_str());
        }

        void WebServer::handle_date_request(mg_connection *c, JsonObject doc)
        {
            for (auto *obj : App.get_dates())
            {

                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->date_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                // if (get_object_id(obj) != match.id)
                //   continue;
                // if (request->method() == HTTP_GET && match.method.empty()) {
                //   auto detail = DETAIL_STATE;
                //   auto *param = request->getParam("detail");
                //   if (param && param->value() == "all") {
                //     detail = DETAIL_ALL;
                //   }
                //   std::string data = this->date_json(obj, detail);
                //   request->send(200, "application/json", data.c_str());
                //   return;
                // }
                // if (match.method != "set") {
                //   request->send(404);
                //   return;
                // }

                auto call = obj->make_call();
                if (doc[FC("value")].is<JsonVariant>())
                {
                    std::string value = doc[FC("value")];
                    call.set_date(value);
                }
                else
                {
                    ws_reply(c, "", false);
                    return;
                }

                //   if (!request->hasParam("value")) {
                //     request->send(409);
                //     return;
                //   }

                //   if (request->hasParam("value")) {
                //     std::string value = request->getParam("value")->value().c_str();
                //     call.set_date(value);
                //   }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                //   request->send(200);
                return;
            }
            ws_reply(c, "", false);
            // request->send(404);
        }

        std::string WebServer::date_json(datetime::DateEntity *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    set_json_id(root, obj, "date-" + get_object_id(obj), start_config);
    std::string value = str_sprintf("%d-%02d-%02d", obj->year, obj->month, obj->day);
    root["value"] = value;
    root["state"] = value;
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
#endif // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
        void WebServer::on_time_update(datetime::TimeEntity *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            this->push(STATE, this->time_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->time_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_time_request(mg_connection *c, JsonObject doc)
        {
            for (auto *obj : App.get_times())
            {

                // if (get_object_id(obj) != match.id)
                //   continue;
                // if (request->method() == HTTP_GET && match.method.empty()) {
                //   auto detail = DETAIL_STATE;
                //   auto *param = request->getParam("detail");
                //   if (param && param->value() == "all") {
                //     detail = DETAIL_ALL;
                //   }
                //   std::string data = this->time_json(obj, detail);
                //   request->send(200, "application/json", data.c_str());
                //   return;
                // }
                // if (match.method != "set") {
                //   request->send(404);
                //   return;
                // }

                // auto call = obj->make_call();

                // if (!request->hasParam("value")) {
                //   request->send(409);
                //   return;
                // }

                // if (request->hasParam("value")) {
                //   std::string value = request->getParam("value")->value().c_str();
                //   call.set_time(value);
                // }

                // this->schedule_([call]() mutable { call.perform(); });
                // request->send(200);
                // return;
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->time_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();
                if (doc[FC("value")].is<JsonVariant>())
                {
                    std::string value = doc[FC("value")];
                    call.set_time(value);
                }
                else
                {
                    ws_reply(c, "", false);
                    return;
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
            // request->send(404);
        }
        std::string WebServer::time_json(datetime::TimeEntity *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    set_json_id(root, obj, "time-" + get_object_id(obj), start_config);
    std::string value = str_sprintf("%02d:%02d:%02d", obj->hour, obj->minute, obj->second);
    root["value"] = value;
    root["state"] = value;
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
#endif // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
        void WebServer::on_datetime_update(datetime::DateTimeEntity *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            this->push(STATE, this->datetime_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->datetime_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_datetime_request(mg_connection *c, JsonObject doc)
        {
            for (auto *obj : App.get_datetimes())
            {

                // if (get_object_id(obj) != match.id)
                //   continue;
                // if (request->method() == HTTP_GET && match.method.empty())
                // {
                //   auto detail = DETAIL_STATE;
                //   auto *param = request->getParam("detail");
                //   if (param && param->value() == "all")
                //   {
                //     detail = DETAIL_ALL;
                //   }
                //   std::string data = this->datetime_json(obj, detail);
                //   request->send(200, "application/json", data.c_str());
                //   return;
                // }
                // if (match.method != "set")
                // {
                //   request->send(404);
                //   return;
                // }

                // auto call = obj->make_call();

                // if (!request->hasParam("value"))
                // {
                //   request->send(409);
                //   return;
                // }

                // if (request->hasParam("value"))
                // {
                //   std::string value = request->getParam("value")->value().c_str();
                //   call.set_datetime(value);
                // }

                // this->schedule_([call]() mutable
                //                 { call.perform(); });
                // request->send(200);
                // return;

                if (get_object_id(obj) != doc[FC("oid")]
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->datetime_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();
                if (doc[FC("value")].is<JsonVariant>())
                {
                    std::string value = doc[FC("value")];
                    call.set_datetime(value);
                }
                else
                {
                    ws_reply(c, "", false);
                    return;
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
            // request->send(404);
        }

        std::string WebServer::datetime_json(datetime::DateTimeEntity *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    set_json_id(root, obj, "datetime-" + get_object_id(obj), start_config);
    std::string value = str_sprintf("%d-%02d-%02d %02d:%02d:%02d", obj->year, obj->month, obj->day, obj->hour,
                                    obj->minute, obj->second);
    root["value"] = value;
    root["state"] = value;
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
#endif // USE_DATETIME_DATETIME

#ifdef USE_EVENT
        void WebServer::on_event(event::Event *obj, const std::string &event_type)
        {
            this->push(STATE, this->event_json(obj, event_type, DETAIL_STATE).c_str());
            // this->events_.send(this->event_json(obj, event_type, DETAIL_STATE).c_str(), "state");
        }

        void WebServer::handle_event_request(mg_connection *c, JsonObject doc)
        {
            for (event::Event *obj : App.get_events())
            {
                // if (get_object_id(obj) != match.id)
                //   continue;

                // if (request->method() == HTTP_GET && match.method.empty()) {
                //   auto detail = DETAIL_STATE;
                //   auto *param = request->getParam("detail");
                //   if (param && param->value() == "all") {
                //     detail = DETAIL_ALL;
                //   }
                //   std::string data = this->event_json(obj, "", detail);
                //   request->send(200, "application/json", data.c_str());
                //   return;

                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->event_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }
            }
            // request->send(404);
            ws_reply(c, "", false);
        }
        std::string WebServer::event_json(event::Event *obj, const std::string &event_type, JsonDetail start_config)
        {
            return json::build_json([this, obj, event_type, start_config](JsonObject root)
                                    {
    set_json_id(root, obj, "event-" + get_object_id(obj), start_config);
    if (!event_type.empty()) {
      root["event_type"] = event_type;
    }
    if (start_config == DETAIL_ALL) {
      JsonArray event_types = root.createNestedArray("event_types");
      for (auto const &event_type : obj->get_event_types()) {
        event_types.add(event_type);
      }
      root["device_class"] = obj->get_device_class();
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

#ifdef USE_UPDATE
        void WebServer::on_update(update::UpdateEntity *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
            this->push(STATE, this->update_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->update_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_update_request(mg_connection *c, JsonObject doc)
        {
            for (update::UpdateEntity *obj : App.get_updates())
            {

                // if (get_object_id(obj) != match.id)
                //   continue;

                // if (request->method() == HTTP_GET && match.method.empty()) {
                //   auto detail = DETAIL_STATE;
                //   auto *param = request->getParam("detail");
                //   if (param && param->value() == "all") {
                //     detail = DETAIL_ALL;
                //   }
                //   std::string data = this->update_json(obj, detail);
                //   request->send(200, "application/json", data.c_str());
                //   return;
                // }

                // if (match.method != "install") {
                //   request->send(404);
                //   return;
                // }

                // this->schedule_([obj]() mutable { obj->perform(); });
                // request->send(200);
                // return;
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->update_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc[FC("action")] != "install")
                {
                    ws_reply(c, "", false);
                    return;
                }
                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            // request->send(404);
            ws_reply(c, "", false);
        }
        std::string WebServer::update_json(update::UpdateEntity *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    set_json_id(root, obj, "update-" + get_object_id(obj), start_config);
    root["value"] = obj->update_info.latest_version;
    switch (obj->state) {
      case update::UPDATE_STATE_NO_UPDATE:
        root["state"] = "NO UPDATE";
        break;
      case update::UPDATE_STATE_AVAILABLE:
        root["state"] = "UPDATE AVAILABLE";
        break;
      case update::UPDATE_STATE_INSTALLING:
        root["state"] = "INSTALLING";
        break;
      default:
        root["state"] = "UNKNOWN";
        break;
    }
    if (start_config == DETAIL_ALL) {
      root["current_version"] = obj->update_info.current_version;
      root["title"] = obj->update_info.title;
      root["summary"] = obj->update_info.summary;
      root["release_url"] = obj->update_info.release_url;
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

#ifdef USE_VALVE
        void WebServer::on_valve_update(valve::Valve *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            this->push(STATE, this->valve_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->valve_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_valve_request(mg_connection *c, JsonObject doc)
        {
            for (valve::Valve *obj : App.get_valves())
            {

                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->valve_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }
                auto call = obj->make_call();
                if (doc[FC("action")] == "open")
                {
                    call.set_command_open();
                }
                else if (doc[FC("action")] == "close")
                {
                    call.set_command_close();
                }
                else if (doc[FC("action")] == "stop")
                {
                    call.set_command_stop();
                }
                else if (doc[FC("action")] == "toggle")
                {
                    call.set_command_toggle();
                }
                else if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                }

                if (doc["position"].is<JsonVariant>())
                {
                    std::string value = doc["position"];

                    auto position = parse_number<float>(value);
                    if (position.has_value())
                    {
                        call.set_position(*position);
                    }
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
        }
        std::string WebServer::valve_json(valve::Valve *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    set_json_icon_state_value(root, obj, "valve" , obj->is_fully_closed() ? "CLOSED" : "OPEN",
                              obj->position, start_config);
    root["current_operation"] = valve::valve_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_position())
      root["position"] = obj->position;
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

#ifdef USE_TEXT
        void WebServer::on_text_update(text::Text *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->text_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->text_json(obj, obj->state, DETAIL_STATE).c_str());
        }

        void WebServer::handle_text_request(mg_connection *c, JsonObject doc)
        {

            for (auto *obj : App.get_texts())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->text_json(obj, obj->state, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }
                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }
                auto call = obj->make_call();
                if (doc[FC("value")].is<JsonVariant>())
                {
                    call.set_value(doc[FC("value")]);
                    this->defer([call]() mutable
                                { call.perform(); });
                }

                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
        }

        std::string WebServer::text_json(text::Text *obj, const std::string &value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
    // set_json_id(root, obj, "text-" + get_object_id(obj), start_config);
    // if (start_config == DETAIL_ALL) {
    //   root["mode"] = (int) obj->traits.get_mode();
    // }
    // root["min_length"] = obj->traits.get_min_length();
    // root["max_length"] = obj->traits.get_max_length();
    // root["pattern"] = obj->traits.get_pattern();
    // if (obj->traits.get_mode() == text::TextMode::TEXT_MODE_PASSWORD) {
    //   root["state"] = "********";
    // } else {
    //   root["state"] = value;
    // }
    // root["value"] = value; });
        set_json_id(root, obj, "text-" + get_object_id(obj), start_config);
    root["min_length"] = obj->traits.get_min_length();
    root["max_length"] = obj->traits.get_max_length();
    root["pattern"] = obj->traits.get_pattern();
    if (obj->traits.get_mode() == text::TextMode::TEXT_MODE_PASSWORD) {
      root["state"] = "********";
    } else {
      root["state"] = value;
    }
    root["value"] = value;
    if (start_config == DETAIL_ALL) {
      root["mode"] = (int) obj->traits.get_mode();
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

        long int WebServer::toInt(const std::string &s, int base)
        {
            if (s.empty() || std::isspace(s[0]))
                return 0;
            char *p;
            long int li = strtol(s.c_str(), &p, base);
            return li;
        }

        void WebServer::handle_auth_request(mg_connection *c, JsonObject doc)
        {

            if (doc[FC("action")] != "set")
            {
                ws_reply(c, "", false);
                return;
            }
            if (doc["cid"].is<JsonVariant>())
            {
                // cid = toInt(doc["partition"],10);
                unsigned long ul = (unsigned long)doc["cid"];
              for (mg_connection * cl=mgr.conns ;cl != NULL; cl = cl->next)
                 {
                    if (cl->id == ul)
                    {
                        cl->is_authenticated=1;
                        entities_iterator_.begin(this->include_internal_); //ok authenticated so we can start sending data
                        ESP_LOGD(TAG, "Set auth conn %d as authenticated", cl->id);
                        break;
                    }
                }
                ws_reply(c, "", true);
               // c->is_draining = 1;
                return;
            }
            ws_reply(c, "", false);
            //c->is_draining=1;

        }

        bool WebServer::callKeyService(const char *buf, int partition)
        {

            std::string keys = buf;
            if (this->key_service_func_.has_value())
            {
                (*this->key_service_func_)(keys, partition);
                return true;
            }
#if defined(USE_DSC_PANEL) || defined(USE_VISTA_PANEL)
            else if (alarm_panel::alarmPanelPtr != NULL)
            {
                alarm_panel::alarmPanelPtr->alarm_keypress_partition(keys, partition);
                return true;
            }
#endif

            return false;
        }

        void WebServer::handle_alarm_panel_request(mg_connection *c, JsonObject doc)
        {

            if (doc["method"] == "GET")
            {

                if (doc[FC("action")] == "getconfig")
                {
                    std::string enc;
                    get_keypad_config(enc);
                    ws_reply(c, enc.c_str(), true);
                    return;
                }
                // ws_reply(c,"",true);
                // return;
            }
            if (doc[FC("action")] != "set")
            {
                ws_reply(c, "", false);
                return;
            }

            int partition = 1; // get default partition
            if (doc["partition"].is<JsonVariant>())
            {
                partition = toInt(doc["partition"], 10);
            }
            if (doc["keys"].is<JsonVariant>())
            {
                if (callKeyService(doc["keys"], partition))
                    ws_reply(c, "", true);
                else
                    ws_reply(c, "", false);
                return;
            }
            ws_reply(c, "", false);
        }

#ifdef USE_SELECT
        void WebServer::on_select_update(select::Select *obj)
        {
            f (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->select_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->select_json(obj, obj->state, DETAIL_STATE).c_str());
        }
        void WebServer::handle_select_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (auto *obj : App.get_selects())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                //   if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->select_json(obj, obj->state, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                //  if (match.method != "set") {
                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();

                // if (mg_http_get_var(&hm->body,"option",buf,sizeof(buf)) > 0) {
                if (doc["option"].is<JsonVariant>())
                {
                    auto option = doc["option"];
                    // if (request->hasParam("option")) {
                    //  auto option = request->getParam("option")->value();
                    call.set_option(option.c_str()); // NOLINT(clang-diagnostic-deprecated-declarations)
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
        }
        std::string WebServer::select_json(select::Select *obj, const std::string &value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {

        set_json_icon_state_value(root, obj, "select", value, value, start_config);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("option");
      for (auto &option : obj->traits.get_options()) {
        opt.add(option);
      }
    this->add_sorting_info_(root, obj);
    } });
        }
#endif

// Longest: HORIZONTAL
#define PSTR_LOCAL(mode_s) strncpy_P(buf, (PGM_P)((mode_s)), 15)
#ifdef USE_CLIMATE_XX //not supported
        void WebServer::on_climate_update(climate::Climate *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->climate_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->climate_json(obj, DETAIL_STATE).c_str());
        }

        void WebServer::handle_climate_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (auto *obj : App.get_climates())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->climate_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                // if (match.method != "set") {
                if (doc[FC("action")] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();
                // char buf[100];
                // if (mg_http_get_var(&hm->body,"mode",buf,sizeof(buf)) > 0) {
                if (doc["mode"].is<JsonVariant>())
                {
                    // if (request->hasParam("mode")) {
                    //  auto mode = request->getParam("mode")->value();
                    //  auto mode=buf;
                    call.set_mode(doc["mode"]);
                }
                // if (mg_http_get_var(&hm->body,"target_temperature_high",buf,sizeof(buf)) > 0) {
                // auto target_temperature_high=buf;
                if (doc["target_temperature_high"].is<JsonVariant>())
                {

                    // if (request->hasParam("target_temperature_high")) {
                    auto target_temperature_high = parse_number<float>(doc["target_temperature_high"]);
                    if (target_temperature_high.has_value())
                        call.set_target_temperature_high(*target_temperature_high);
                }

                // if (request->hasParam("target_temperature_low")) {
                // auto target_temperature_low = parse_number<float>(request->getParam("target_temperature_low")->value().c_str());
                // if (mg_http_get_var(&hm->body,"target_temperature_low",buf,sizeof(buf)) > 0) {
                if (doc["target_temperature_low"].is<JsonVariant>())
                {
                    auto target_temperature_low = parse_number<float>(doc["target_temperature_low"]);
                    if (target_temperature_low.has_value())
                        call.set_target_temperature_low(*target_temperature_low);
                }

                // if (request->hasParam("target_temperature")) {
                //  auto target_temperature = parse_number<float>(request->getParam("target_temperature")->value().c_str());
                // if (mg_http_get_var(&hm->body,"target_temperature",buf,sizeof(buf)) > 0) {
                if (doc["target_temperature"].is<JsonVariant>())
                {
                    auto target_temperature = parse_number<float>(doc["target_temperature"]);
                    if (target_temperature.has_value())
                        call.set_target_temperature(*target_temperature);
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
        }

        std::string WebServer::climate_json(climate::Climate *obj, JsonDetail start_config)
        {
            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    // set_json_id(root, obj, "climate-" + get_object_id(obj), start_config);
    // const auto traits = obj->get_traits();
    // int8_t target_accuracy = traits.get_target_temperature_accuracy_decimals();
    // int8_t current_accuracy = traits.get_current_temperature_accuracy_decimals();
    // char buf[16];


    // if (start_config == DETAIL_ALL) {
    //   JsonArray opt = root.createNestedArray("modes");
    //   for (climate::ClimateMode m : traits.get_supported_modes())
    //     opt.add(PSTR_LOCAL(climate::climate_mode_to_string(m)));
    //   if (!traits.get_supported_custom_fan_modes().empty()) {
    //     JsonArray opt = root.createNestedArray("fan_modes");
    //     for (climate::ClimateFanMode m : traits.get_supported_fan_modes())
    //       opt.add(PSTR_LOCAL(climate::climate_fan_mode_to_string(m)));
    //   }

    //   if (!traits.get_supported_custom_fan_modes().empty()) {
    //     JsonArray opt = root.createNestedArray("custom_fan_modes");
    //     for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
    //       opt.add(custom_fan_mode);
    //   }
    //   if (traits.get_supports_swing_modes()) {
    //     JsonArray opt = root.createNestedArray("swing_modes");
    //     for (auto swing_mode : traits.get_supported_swing_modes())
    //       opt.add(PSTR_LOCAL(climate::climate_swing_mode_to_string(swing_mode)));
    //   }
    //   if (traits.get_supports_presets() && obj->preset.has_value()) {
    //     JsonArray opt = root.createNestedArray("presets");
    //     for (climate::ClimatePreset m : traits.get_supported_presets())
    //       opt.add(PSTR_LOCAL(climate::climate_preset_to_string(m)));
    //   }
    //   if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
    //     JsonArray opt = root.createNestedArray("custom_presets");
    //     for (auto const &custom_preset : traits.get_supported_custom_presets())
    //       opt.add(custom_preset);
    //   }
    // }

    // bool has_state = false;
    // root["mode"] = PSTR_LOCAL(climate_mode_to_string(obj->mode));
    // root["max_temp"] = value_accuracy_to_string(traits.get_visual_max_temperature(), target_accuracy);
    // root["min_temp"] = value_accuracy_to_string(traits.get_visual_min_temperature(), target_accuracy);
    // root["step"] = traits.get_visual_target_temperature_step();
    // if (traits.get_supports_action()) {
    //   root["action"] = PSTR_LOCAL(climate_action_to_string(obj->action));
    //   root["state"] = root["action"];
    //   has_state = true;
    // }
    // if (traits.get_supports_fan_modes() && obj->fan_mode.has_value()) {
    //   root["fan_mode"] = PSTR_LOCAL(climate_fan_mode_to_string(obj->fan_mode.value()));
    // }
    // if (!traits.get_supported_custom_fan_modes().empty() && obj->custom_fan_mode.has_value()) {
    //   root["custom_fan_mode"] = obj->custom_fan_mode.value().c_str();
    // }
    // if (traits.get_supports_presets() && obj->preset.has_value()) {
    //   root["preset"] = PSTR_LOCAL(climate_preset_to_string(obj->preset.value()));
    // }
    // if (!traits.get_supported_custom_presets().empty() && obj->custom_preset.has_value()) {
    //   root["custom_preset"] = obj->custom_preset.value().c_str();
    // }
    // if (traits.get_supports_swing_modes()) {
    //   root["swing_mode"] = PSTR_LOCAL(climate_swing_mode_to_string(obj->swing_mode));
    // }
    // if (traits.get_supports_current_temperature()) {
    //   if (!std::isnan(obj->current_temperature)) {
    //     root["current_temperature"] = value_accuracy_to_string(obj->current_temperature, current_accuracy);
    //   } else {
    //     root["current_temperature"] = "NA";
    //   }
    // }
    // if (traits.get_supports_two_point_target_temperature()) {
    //   root["target_temperature_low"] = value_accuracy_to_string(obj->target_temperature_low, target_accuracy);
    //   root["target_temperature_high"] = value_accuracy_to_string(obj->target_temperature_high, target_accuracy);
    //   if (!has_state) {
    //     root["state"] = value_accuracy_to_string((obj->target_temperature_high + obj->target_temperature_low) / 2.0f,
    //                                              target_accuracy);
    //   }
    // } else {
    //   root["target_temperature"] = value_accuracy_to_string(obj->target_temperature, target_accuracy);
    //   if (!has_state)
    //     root["state"] = root["target_temperature"];
    // } });
     set_json_id(root, obj, "climate-" + get_object_id(obj), start_config);
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
    this->add_sorting_info_(root, obj);
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
    } });
        }
#endif

#ifdef USE_LOCK
        void WebServer::on_lock_update(lock::Lock *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->lock_json(obj, obj->state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->lock_json(obj, obj->state, DETAIL_STATE).c_str());
        }
        std::string WebServer::lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
                                    set_json_icon_state_value(root, obj, "lock", lock::lock_state_to_string(value), value,
                              start_config);
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
        void WebServer::handle_lock_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (lock::Lock *obj : App.get_locks())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->lock_json(obj, obj->state, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc[FC("action")] == "lock")
                {
                    this->schedule_([obj]()
                                    { obj->lock(); });
                    ws_reply(c, "", true);
                }
                else if (doc[FC("action")] == "unlock")
                {
                    this->schedule_([obj]()
                                    { obj->unlock(); });
                    ws_reply(c, "", true);
                }
                else if (doc[FC("action")] == "open")
                {
                    this->schedule_([obj]()
                                    { obj->open(); });
                    ws_reply(c, "", true);
                }
                else
                {
                    ws_reply(c, "", false);
                }
                return;
            }
            ws_reply(c, "", false);
            ;
        }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
        void WebServer::on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj)
        {
            if (!this->include_internal_ && obj->is_internal())
                 return;
            // this->events_.send(this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str());
        }
        std::string WebServer::alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                                        alarm_control_panel::AlarmControlPanelState value,
                                                        JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {


        char buf[16];
    set_json_icon_state_value(root, obj, "alarm-control-panel",PSTR_LOCAL(alarm_control_panel_state_to_string(value)), value, start_config);
    if (start_config == DETAIL_ALL) {
    this->add_sorting_info_(root, obj);
    } });
        }
        void WebServer::handle_alarm_control_panel_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (alarm_control_panel::AlarmControlPanel *obj : App.get_alarm_control_panels())
            {
                if (get_object_id(obj) != doc[FC("oid")])
                    continue;
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc[FC("detail")].is<JsonVariant>())
                    {
                        if (doc[FC("detail")] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->alarm_control_panel_json(obj, obj->get_state(), detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }
            }
            ws_reply(c, "", false);
        }
#endif

        void WebServer::push(msgType mt, const char *data, uint32_t id, uint32_t reconnect)
        {


           const char * type;
            switch (mt)
            {
            case PING:
                type = "ping";
                break;
            case STATE:
                type = "state";
                break;
            case LOG:
                type = "log";
                break;
            case CONFIG:
                type = "config";
                break;
            case OTA:
                type = "ota";
                break;
            default:
                return;
            }


#ifdef USE_WEBKEYPAD_ENCRYPTION
            std::string newdata;
            if (get_credentials()->crypt && strlen(data) > 0) {
                newdata=std::string(data);
                encrypt(newdata);
                data=newdata.c_str();
            }
#endif

            for (  struct mg_connection *c = mgr.conns; c != NULL; c = c->next)
            {
               // printf("id %d recv size=%d, send size=%d\n",(int)c->id,c->recv.size,c->send.size);
#ifdef USE_WEBKEYPAD_ENCRYPTION
                if (get_credentials()->crypt && !c->is_authenticated)
                    continue; // not authenticated with encrypted response
#endif

                if (c->is_event && !c->is_closing)
                {
                    // ESP_LOGD(TAG,"type=%s,len=%d,data=%s",type.c_str(),strlen(data),data);
                    if (id && reconnect)
                        mg_printf(c, FC("id: %d\r\nretry: %d\r\nevent: %s\r\ndata: %s\r\n\r\n"), id, reconnect, type, data);
                    else
                        mg_printf(c, FC("event: %s\r\ndata: %s\r\n\r\n"), type, data);

                    if (c->send.len > 10000) {
                        ESP_LOGD(TAG,"Non responsive event connection. Closing %d",c->id);
                        c->is_closing = 1; // dead connection. kill it.
                    }
                    continue;
                }
#ifdef USE_WEBKEYPAD_WEBSOCKET
                if (!c->is_websocket)
                    continue;

                if (mt == PING)
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, FC("{\"%s\":\"%s\",\"%s\":\"%d\"}"), "type", type, "data", id);
                else if ((mt == LOG || mt == OTA) && !get_credentials()->crypt)
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, FC("{\"%s\":\"%s\",\"%s\":\"%s\"}"), "type", type, "data", data);
                else
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, FC("{\"%s\":\"%s\",\"%s\":%s}"), "type", type, "data", data);

                if (c->send.len > 15000)
                    c->is_closing = 1; // dead connection. kill it.
#endif
            }
        }

        /*
        static const char *s_tls_cert =
            "-----BEGIN CERTIFICATE-----\n"
            "MIIBhzCCASygAwIBAgIUbnMoVd8TtWH1T09dANkK2LU6IUswCgYIKoZIzj0EAwIw\n"
            "RDELMAkGA1UEBhMCSUUxDzANBgNVBAcMBkR1YmxpbjEQMA4GA1UECgwHQ2VzYW50\n"
            "YTESMBAGA1UEAwwJVGVzdCBSb290MB4XDTIwMDUwOTIxNTE0OVoXDTMwMDUwOTIx\n"
            "NTE0OVowETEPMA0GA1UEAwwGc2VydmVyMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcD\n"
            "QgAEkuBGnInDN6l06zVVQ1VcrOvH5FDu9MC6FwJc2e201P8hEpq0Q/SJS2nkbSuW\n"
            "H/wBTTBaeXN2uhlBzMUWK790KKMvMC0wCQYDVR0TBAIwADALBgNVHQ8EBAMCA6gw\n"
            "EwYDVR0lBAwwCgYIKwYBBQUHAwEwCgYIKoZIzj0EAwIDSQAwRgIhAPo6xx7LjCdZ\n"
            "QY133XvLjAgVFrlucOZHONFVQuDXZsjwAiEAzHBNligA08c5U3SySYcnkhurGg50\n"
            "BllCI0eYQ9ggp/o=\n"
            "-----END CERTIFICATE-----\n";

        static const char *s_tls_key =
            "-----BEGIN PRIVATE KEY-----\n"
            "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQglNni0t9Dg9icgG8w\n"
            "kbfxWSS+TuNgbtNybIQXcm3NHpmhRANCAASS4EacicM3qXTrNVVDVVys68fkUO70\n"
            "wLoXAlzZ7bTU/yESmrRD9IlLaeRtK5Yf/AFNMFp5c3a6GUHMxRYrv3Qo\n"
            "-----END PRIVATE KEY-----\n";
        */

        char *mg_md5(char buf[33], ...)
        {
            unsigned char hash[16];
            const unsigned char *p;
            va_list ap;
            mg_md5_ctx ctx;
            mg_md5_init(&ctx);
            va_start(ap, buf);
            // MG_INFO(("start"));
            while ((p = va_arg(ap, const unsigned char *)) != NULL)
            {

                size_t len = va_arg(ap, size_t);
                //  MG_INFO(("md5 update %.*s",len,p));
                mg_md5_update(&ctx, p, len);
            }
            // MG_INFO(("end"));
            va_end(ap);
            mg_md5_final(&ctx, hash);
            mg_hex(hash, sizeof(hash), buf);
            return buf;
        }

        static void mg_send_digest_auth_request(struct mg_connection *c,
                                                const char *realm)
        {
            char nonce[16];
            mg_random(nonce, sizeof(nonce));

            mg_printf(c,
                      FC("HTTP/1.1 401 Unauthorized\r\n"
                           "WWW-Authenticate: Digest qop=\"auth\", "
                           "realm=\"%s\", nonce=\"%lu\"\r\n"
                           "Content-Length: 0\r\n\r\n"),
                      realm, nonce);
        }

        static void mg_mkmd5resp(const char *method, size_t method_len, const char *uri,
                                 size_t uri_len, mg_str *username, const char *password, mg_str *realm, mg_str *nonce, mg_str *nc,
                                 mg_str *cnonce, mg_str *qop, char *resp)
        {
            static const char colon[] = ":";
            static const size_t one = 1;
            char ha1[33];
            char ha2[33];
            mg_md5(ha1, username->buf, username->len, colon, one, realm->buf, realm->len, colon, one, password, strlen(password), NULL);
            mg_md5(ha2, method, method_len, colon, one, uri, uri_len, NULL);
            mg_md5(resp, ha1, sizeof(ha1) - 1, colon, one, nonce->buf, nonce->len, colon, one, nc->buf, nc->len, colon, one, cnonce->buf, cnonce->len, colon, one, qop->buf, qop->len, colon, one, ha2, sizeof(ha2) - 1, NULL);
        }

        /*
         * Authenticate HTTP request against opened passwords file.
         * Returns 1 if authenticated, 0 otherwise.
         */
        static int mg_http_check_digest_auth(struct mg_http_message *hm,
                                             const char *auth_domain, Credentials *creds)
        {
            mg_str *hdr;
            char expected_response[33];
            mg_str username, cnonce, response, uri, qop, nc, nonce;
            /* Parse "Authorization:" header, fail fast on parse error */
            if (hm == NULL || creds == NULL ||
                (hdr = mg_http_get_header(hm, "Authorization")) == 0 ||

                (username = mg_http_get_header_var(*hdr, mg_str_n("username", 8))).len == 0 ||
                (cnonce = mg_http_get_header_var(*hdr, mg_str_n("cnonce", 6))).len == 0 ||
                (response = mg_http_get_header_var(*hdr, mg_str_n("response", 8))).len == 0 ||
                (uri = mg_http_get_header_var(*hdr, mg_str_n("uri", 3))).len == 0 ||
                (qop = mg_http_get_header_var(*hdr, mg_str_n("qop", 3))).len == 0 ||
                (nc = mg_http_get_header_var(*hdr, mg_str_n("nc", 2))).len == 0 ||
                (nonce = mg_http_get_header_var(*hdr, mg_str_n("nonce", 5))).len == 0
                //||
                // mg_http_parse_header(hdr, "nonce", nonce, sizeof(nonce)) == 0 ||
                // check_nonce(nonce.buf) == 0
            )
            {
                return 0;
            }
            // if (hdr != NULL) {
            //         std::string s=std::string(hdr->buf,hdr->len);
            //         Serial.printf("HDR2 is %s\n",s.c_str());
            //         }
            mg_str realm = mg_str(auth_domain);
            std::string u = std::string(username.buf, username.len);
            std::string r = std::string(response.buf, response.len);
            // if (strcmp(creds->username.c_str(),u.c_str())) {
            // MG_INFO(("password=%s",creds->password.c_str()));

            mg_mkmd5resp(
                hm->method.buf, hm->method.len, hm->uri.buf,
                hm->uri.len,
                &username, creds->password, &realm, &nonce, &nc, &cnonce,
                &qop, expected_response);
            //Serial.printf("response =%s, expected=%s, cusername=%s,u=%s\n",r.c_str(),expected_response,creds->username.c_str(),u.c_str());
            return mg_casecmp(r.c_str(), expected_response) == 0;
            // }

            /* None of the entries in the passwords file matched - return failure */
            return 0;
        }

#ifdef USE_WEBKEYPAD_ENCRYPTION
        void WebServer::encrypt(std::string &data)
        {
            const char * message=data.c_str();
            int ml = strlen(message);

            if (!ml)
                return ;
   
            uint8_t iv[AES_IV_SIZE +1];
            random_bytes(iv, AES_IV_SIZE );
            std::string eiv = base64_encode(iv, AES_IV_SIZE ); 
            AES aes(get_credentials()->token, iv, AES::AES_MODE_256, AES::CIPHER_ENCRYPT);
            int length = aes.calcSizeAndPad(ml);
            std::string em="";
            if (length < 1024) {
                uint8_t encrypted[length+1]; //use stack for small messages
                aes.padPlaintext((uint8_t *)message, encrypted);
                aes.processNoPad((uint8_t *)encrypted, encrypted, length);
                em = base64_encode(encrypted, length);
            } else {
                uint8_t * encrypted = new uint8_t[length+1]; //use heap
                aes.padPlaintext((uint8_t *)message, encrypted);
                aes.processNoPad((uint8_t *)encrypted, encrypted, length);
                em = base64_encode(encrypted, length);
                delete[] encrypted;
            }
            
            SHA256HMAC hmac((const char*) get_credentials()->hmackey, SHA256HMAC_SIZE);

            hmac.doUpdate(eiv.c_str(), eiv.length());
 
            hmac.doUpdate(em.c_str(), em.length());
  
            uint8_t authCode[SHA256HMAC_SIZE+1];
            hmac.doFinal((char*)authCode);

            data = "{\"iv\":\"" + eiv + "\",\"data\":\"";
            data.append(em);
            data.append("\",\"hash\":\"" + base64_encode(authCode, SHA256HMAC_SIZE) + "\"}");
        }

        bool WebServer::decrypt(JsonObject doc, uint8_t *err,std::string & out)
        {
            const char *iv = doc["iv"];
            const char *data = doc["data"];
            std::string hash = doc["hash"];
            unsigned long cid = 0;

            std::string token = "";
            const char * seqstr = "";
            int seq = 0;
            int *lastseq = NULL;
            // right now we don't force a seq/cid field in the encrypted packet.
            if (doc["seq"].is<JsonVariant>())
            {
                seqstr = doc["seq"];
                seq = (unsigned long)doc["seq"];
            }
            if (doc["cid"].is<JsonVariant>())
            {
                // ensure packet is associated with an active session token and the sequence is newer to prevent replay attacks
                cid = (unsigned long)doc["cid"];
                if (cid > 0)
                {
                    auto it = tokens_.find(cid);
                    if (it != tokens_.end())
                    {
                        token = std::string(it->second.token);
                        lastseq = &it->second.lastseq;
                        if (seq > 0 && seq <= *lastseq)
                        {
                            *err = 1;
                            return 0;
                        }
                    }
                    else
                    {
                        *err = 1;
                        return 0;
                    }
                }
                else
                {
                    *err = 1;
                    return 0;
                }
            }

            uint8_t *key = get_credentials()->token;
            uint8_t *hmackey = get_credentials()->hmackey;
            uint8_t data_decoded[strlen(data)+1];
            uint8_t iv_decoded[strlen(iv)+1];

            SHA256HMAC hmac((const char*) get_credentials()->hmackey, SHA256HMAC_SIZE);
            hmac.doUpdate(iv, strlen(iv));
            if (token != "")
            {
                hmac.doUpdate(token.c_str(), token.length());
            }
            if (strlen(seqstr) > 0)
            {
                hmac.doUpdate(seqstr, strlen(seqstr));
            }

            hmac.doUpdate(data, strlen(data));
            uint8_t authCode[SHA256HMAC_SIZE];
            hmac.doFinal((char*)authCode);
             std::string ehm = base64_encode(authCode, SHA256HMAC_SIZE);
            if (ehm != hash)
            {
                ESP_LOGD(TAG, "ehm [%s] does not match hash [%s]", ehm.c_str(), hash.c_str());
                *err = 1;
                return 0;
            }
            if (seq > 0 && lastseq != NULL)
                *lastseq = seq;

            int encrypted_length = base64_decode(std::string(data), data_decoded, strlen(data));
            base64_decode(std::string(iv), iv_decoded, strlen(iv));
            AES aes(key, iv_decoded, AES::AES_MODE_256, AES::CIPHER_DECRYPT);
            aes.process((uint8_t *)data_decoded, data_decoded, encrypted_length);
            out = std::string((char *)data_decoded);
             //ESP_LOGD(TAG,"decryption: %s,%s,len=%d\r\nhash=%s, data=%s",data,iv,strlen(iv),ehm.c_str(),out.c_str());
            // return std::string((char*)data_decoded);
            return 1;
        }
#endif

#ifdef USE_WEBKEYPAD_OTA
        void WebServer::handle_uploads(struct mg_connection *c, int ev, void *ev_data)
        {

            // Catch /update requests early, without buffering whole body
            // When we receive MG_EV_HTTP_HDRS event, that means we've received all
            // HTTP headers but not necessarily full HTTP body
            if (!c->is_ota && ev == MG_EV_HTTP_HDRS)
            {
                struct mg_http_message *hm = (struct mg_http_message *)ev_data;
                if (hm == nullptr) return;
                if (mg_match(hm->uri, mg_str("/update*"), NULL))
                {

                    if (strcmp(get_credentials()->password,"") != 0 )
                    {
                        if (!mg_http_check_digest_auth(hm, "webkeypad", get_credentials()))
                        {
                            mg_send_digest_auth_request(c, "webkeypad");
                            c->is_draining = 1;
                            c->recv.len = 0;
                            return;
                        }
                    }
                    c->is_ota=true;

                    struct mg_str *fs = mg_http_get_header(hm, "x-filesize");
                    struct mg_str *fn = mg_http_get_header(hm, "x-filename");
                    if (fn != nullptr)
                        upl.filename=PlatformString(std::string(fn->buf,fn->len).c_str());
                    if (fs != nullptr)
                        upl.filesize=std::stoi(std::string(fs->buf,fs->len));
                    if (upl.filesize)
                        upl.expected = upl.filesize;             // Store number of bytes we expect
                    else
                        upl.expected = hm->body.len;             // Store number of bytes we expect
                   

                    upl.received=0;
                    mg_iobuf_del(&c->recv, 0, hm->head.len); // Delete HTTP headers
                    c->pfn = NULL;                           // Silence HTTP protocol handler, we'll use MG_EV_READ
                    ESP_LOGI(TAG,"Performing OTA update... file: %s, size: %d,expected: %d",upl.filename.c_str(),upl.filesize,upl.expected);
                   
                }
             }


            if (!c->is_ota) return;  //not an ota update so return

            // Catch uploaded file data for both MG_EV_READ and MG_EV_HTTP_HDRS
            if (upl.filesize > 0 && c->recv.len > 0)
            {
                // MG_INFO(("Expected bytes: %d, got: %d, received: %d",upl.expected,c->recv.len,upl.received));
                if ((upl.received + c->recv.len) >= upl.expected)
                {
                    // Uploaded everything. Send response back
                    ESP_LOGI(TAG,"OTA uploaded %lu bytes from file %s", upl.received + c->recv.len, ota_filename_.c_str());
                    mg_http_reply(c, 200, NULL, "%lu ok\n", upl.received);
                    handleUpload(upl.expected,( PlatformString) upl.filename, upl.received, c->recv.buf, c->recv.len, true);
                   // memset(us, 0, sizeof(*us)); // Cleanup upload state
                   upl.expected=0;
                   c->is_ota=false;
                   //c->is_draining = 1;         // Close connection when response gets sent
                }
                
                handleUpload(upl.expected,(PlatformString)upl.filename, upl.received, c->recv.buf, c->recv.len, false);
                upl.received += c->recv.len;
                c->recv.len = 0; // Delete received data
            }
        }
#endif

      void WebServer::ev_handler(struct mg_connection *c, int ev, void *ev_data)
        {
            #ifdef USE_WEBKEYPAD_OTA
            if (!c->is_sending) handle_uploads(c, ev, ev_data);
            #endif
            if (ev == MG_EV_POLL) {
                if (c->is_sending ) {
                    //printf("Sending js data to connection %d\n",c->id);
                    send_js_include(c); // process pending file send
                }
                if (c->recv.len == 0 && !c->is_ota) mg_iobuf_resize(&c->recv,0); //keep receive buffer low as we don't get much data in and saves ram
                if (c->send.len == 0 && c->send.size  > 1024)
                {
                   //printf("Resized send buf for id:%d size: %d\n",(int)c->id,c->send.size);
                    mg_iobuf_resize(&c->send,1024);
                }
             }  
            else if (ev == MG_EV_CLOSE)
            {
                ESP_LOGD(TAG, "Connection %d closed", c->id);
                tokens_.erase(c->id);

#if defined(ESP32)
                ESP_LOGD(TAG, "Current Heap values: freeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(8));
#endif
            }
            else if (ev == MG_EV_ACCEPT)
            {
                /*
                const char *cert=get_certificate();
                const char *key=get_certificate_key();
                MG_INFO(("certificate len=%d,%s,%s",strlen(cert),cert,key));

                if (strlen(cert)==0) return;
                struct mg_tls_opts opts = {
                .cert = mg_str(s_tls_cert),
                .key = mg_str(s_tls_key),
                };

                mg_tls_init(c, &opts);
            */
                c->is_authenticated=0; // ensure we set these to  default
                c->is_sending=0;
                c->is_ota=0;
                c->is_event=0;
                ESP_LOGD(TAG, "New connection %d accepted", c->id);
#if defined(ESP32)
                ESP_LOGD(TAG, "Current Heap values: freeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(8));
#endif
            }

#ifdef USE_WEBKEYPAD_WEBSOCKET
            else if (ev == MG_EV_WS_MSG)
            {
                // Got websocket frame. Received data is wm->data. Echo it back!
                struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;

                JsonDocument doc=json::parse_json((const uint8_t*) wm->data.buf,wm->data.len);
                JsonObject obj = doc.as<JsonObject>();
                uint8_t err = 0;
#ifdef USE_WEBKEYPAD_ENCRYPTION
                if (obj["iv"].is<JsonVariant>() && get_credentials()->crypt)
                {
                    std::string buf="";
                    decrypt(obj, &err,buf);
                   
                    if (buf == "" || err)
                        err = 1;
                    if (!err)
                        doc=json::parse_json((const uint8_t *)buf.c_str(), buf.length());
                       // deserializeJson(doc, buf.c_str());
                }
#endif
                if (!err)
                {
                    handleRequest(c, obj);
                }
                else
                    mg_http_reply(c, 403, "", "");

            } else
#endif //websocket
            if (ev == MG_EV_HTTP_MSG && !c->is_ota)
            {
                struct mg_http_message *hm = (struct mg_http_message *)ev_data;
               // std::string u=std::string(hm->uri.buf, hm->uri.len);
                if (strcmp(get_credentials()->password,"") != 0 && !get_credentials()->crypt)
                {
                    if (!mg_http_check_digest_auth(hm, FC("webkeypad"), get_credentials()))
                    {
                        mg_send_digest_auth_request(c, FC("webkeypad"));
                       // c->is_draining = 1;
                        c->recv.len = 0;
                        return;
                    }
                }


#ifdef USE_WEBKEYPAD_WEBSOCKET
                if (mg_match(hm->uri, mg_str("/ws"), NULL) && !c->is_event)
                {

                    // Upgrade to websocket. From now on, a connection is a full-duplex
                    // Websocket connection, which will receive MG_EV_WS_MSG events.
                    mg_ws_upgrade(c, hm, NULL);
                    c->is_websocket=1;
                    std::string enc;
                    bool crypt = get_credentials()->crypt;
                    get_config_json(c->id,enc);
                    #ifdef USE_WEBKEYPAD_ENCRYPTION
                    if (crypt)
                        encrypt(enc);
                    #endif
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, FC("{\"%s\":\"%s\",\"%s\":%ul,\"%s\":%s}"), "type", "app_config", "data", enc.c_str());
                    get_keypad_config(enc);
                    if (enc.length() > 0)
                    {
                       #ifdef USE_WEBKEYPAD_ENCRYPTION
                        if (crypt)
                            encrypt(enc);
                        #endif
                        mg_ws_printf(c, WEBSOCKET_OP_TEXT, FC("{\"%s\":\"%s\",\"%s\":%s}"), "type", "key_config", "data", enc.c_str());
                    }
                    for (auto &group : sorting_groups_)
                    {

                    json::JsonBuilder builder;
                    JsonObject root = builder.root(); 

                   root["name"] = group.second.name;
                   root["sorting_weight"] = group.second.weight; 
                   enc=builder.serialize();

                   #ifdef USE_WEBKEYPAD_ENCRYPTION
                        if (crypt)
                            encrypt(enc);
                    #endif
                        mg_ws_printf(c, WEBSOCKET_OP_TEXT, FC("{\"%s\":\"%s\",\"%s\":%s}"), "type", "sorting_group", "data", enc.c_str());
                    }
                      if (!crypt)
                            entities_iterator_.begin(this->include_internal_);
                      c->pfn = NULL; 
                      return;
                   
                } else
#endif //websocket
                if (mg_match(hm->uri, mg_str("/events"), NULL) && !c->is_websocket)
                {
                  //  mg_str *hdr = mg_http_get_header(hm, "Accept");
                    // if (hdr != NULL && mg_strstr(*hdr, mg_str("text/event-stream")) != NULL)  {
                    c->is_event=1;
                    c->send.c = c;
                    mg_printf(c, FC("HTTP/2 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\n"));
                    c->send.c = c;
                    bool crypt = get_credentials()->crypt;
                    std::string enc;
                    get_config_json(c->id,enc);
                    #ifdef USE_WEBKEYPAD_ENCRYPTION
                    if (crypt)
                        encrypt(enc);
                    #endif
                    mg_printf(c, FC("id: %d\r\nretry: %d\r\nevent: %s\r\ndata: %s\r\n\r\n"), millis(), 30000, "ping", enc.c_str());
                    for (auto &group : sorting_groups_)
                    {
                      json::JsonBuilder builder;
                      JsonObject root = builder.root(); 
                       root["name"] = group.second.name;
                       root["sorting_weight"] = group.second.weight;
                       enc=builder.serialize();
                     #ifdef USE_WEBKEYPAD_ENCRYPTION
                        // if (crypt) //no need to encrypt it
                        //     encrypt(enc);
                     #endif
                        mg_printf(c, FC("event: %s\r\ndata: %s\r\n\r\n"), "sorting_group", enc.c_str());
                    }
                    mg_mgr_poll(&mgr,0);
                    get_keypad_config(enc);
                    if (enc.length() > 0)
                    {
                        #ifdef USE_WEBKEYPAD_ENCRYPTION
                        //  if (crypt)  //again no need to encrypt this.  Saves some heap hashing
                        //     encrypt(enc);
                        #endif
                        mg_printf(c, FC("event: %s\r\ndata: %s\r\n\r\n"), "key_config", enc.c_str());
                    }
                       if (!crypt) //if we don't need encryption or authentication, we can start the iterator
                        entities_iterator_.begin(this->include_internal_);
                    c->pfn = NULL; 
                    return;
                }
                 else
                {
                    if (!mg_match(hm->uri, mg_str("/update*"), NULL))
                    {
                        c->send.c = c;
                        c->recv.c = c;
                        handleWebRequest(c, hm);

                    }
                }


            } else if (ev == MG_EV_OPEN) {

                ESP_LOGD(TAG,"New connection open with client id %d\n",c->id);
            } else if (ev == MG_EV_READ){
                // ESP_LOGD(TAG,"reading from client %d,%d\n",c->fd,c->id);
            }
            else if (ev == MG_EV_ERROR)
            {
                ESP_LOGE(TAG, "MG_EV_ERROR %lu %ld %s.", c->id, c->fd, (char *)ev_data);

            }
           
           
        }



        void WebServer::handleWebRequest(struct mg_connection *c, mg_http_message *hm)
        {

            if (mg_match(hm->uri, mg_str("/"), NULL))
            {
                this->handle_index_request(c);
                return;
            }

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
            if (mg_match(hm->uri, mg_str("/0.css"), NULL))
            {
                this->handle_css_request(c);
                return;
            }
#endif

#ifdef USE_WEBKEYPAD_JS_INCLUDE
            if (mg_match(hm->uri, mg_str("/0.js"), NULL))
            {
                this->handle_js_request(c);
                return;
            }
#endif

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
            // if (request->method() == HTTP_OPTIONS && request->hasHeader(HEADER_CORS_REQ_PNA)) {
            //  this->handle_pna_cors_request(c);
            //  return;
            //}
             const char* const HEADER_CORS_REQ_PNA = FCS("Access-Control-Request-Private-Network");
            mg_str *hdr = mg_http_get_header(hm, HEADER_CORS_REQ_PNA);
            if (mg_vcasecmp(&hm->method, "OPTIONS") == 0 && hdr != NULL)
            {
                this->handle_pna_cors_request(c);
                return;
            }

#endif

            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            if (mg_match(hm->uri, mg_str("/api"), NULL))
            {
                doc=json::parse_json((const uint8_t *)hm->body.buf, hm->body.len);
   #ifdef USE_WEBKEYPAD_ENCRYPTION
                if (obj["iv"].is<JsonVariant>() && strcmp(get_credentials()->password,"") != 0)
                {
                    uint8_t e = 0;
                    std::string buf="";
                    decrypt(obj, &e,buf);
                    if (buf != "") {
                         doc=json::parse_json((const uint8_t *)buf.c_str(), buf.length());
                     }  
                     else
                        mg_http_reply(c, 403, "", "");
                }
#endif
            }

            else 
            {
                parseUrl(hm, obj);
            }

            handleRequest(c, obj);
           // c->is_draining=1;


        }

        void WebServer::handleRequest(mg_connection *c, JsonObject doc)
        {
         //   std::string d =doc[FC("domain")];
          
             if (doc[FC("domain")] == "wifisave") {
                this->handle_wifisave(c,doc);
                return;
             }
#ifdef USE_SENSOR

            if (doc[FC("domain")] == "sensor")
            {
                this->handle_sensor_request(c, doc);
                return;
            }
#endif

#ifdef USE_SWITCH
            if (doc[FC("domain")] == "switch")
            {
                this->handle_switch_request(c, doc);
                return;
            }
#endif

#ifdef USE_BUTTON
            if (doc[FC("domain")] == "button")
            {
                this->handle_button_request(c, doc);
                return;
            }
#endif

#ifdef USE_BINARY_SENSOR
            if (doc[FC("domain")] == "binary_sensor")
            {
                this->handle_binary_sensor_request(c, doc);
                return;
            }
#endif
#ifdef USE_FAN
            if (doc[FC("domain")] == "fan")
            {
                this->handle_fan_request(c, doc);
                return;
            }
#endif

#ifdef USE_LIGHT
            if (doc[FC("domain")] == "light")
            {
                this->handle_light_request(c, doc);
                return;
            }
#endif

#ifdef USE_TEXT_SENSOR
            if (doc[FC("domain")] == "text_sensor")
            {
                this->handle_text_sensor_request(c, doc);
                return;
            }
#endif

#ifdef USE_COVER
            if (doc[FC("domain")] == "cover")
            {
                this->handle_cover_request(c, doc);
                return;
            }
#endif

#ifdef USE_NUMBER
            if (doc[FC("domain")] == "number")
            {
                this->handle_number_request(c, doc);
                return;
            }
#endif

#ifdef USE_DATETIME_DATE
            if (doc[FC("domain")] == "date")
            {
                this->handle_date_request(c, doc);
                return;
            }
#endif

#ifdef USE_DATETIME_TIME
            if (doc[FC("domain")] == "time")
            {
                this->handle_time_request(c, doc);
                return;
            }
#endif

#ifdef USE_DATETIME_DATETIME
            if (doc[FC("domain")] == "datetime")
            {
                this->handle_datetime_request(c, doc);
                return;
            }
#endif

#ifdef USE_VALVE
            if (doc[FC("domain")] == "valve")
            {
                this->handle_valve_request(c, doc);
                return;
            }
#endif

#ifdef USE_UPDATE
            if (doc[FC("domain")] == "update")
            {
                this->handle_update_request(c, doc);
                return;
            }
#endif

#ifdef USE_EVENT
            if (doc[FC("domain")] == "event")
            {
                this->handle_event_request(c, doc);
                return;
            }
#endif

#ifdef USE_TEXT
            if (doc[FC("domain")] == "text")
            {
                this->handle_text_request(c, doc);
                return;
            }
#endif

#ifdef USE_SELECT
            if (doc[FC("domain")] == "select")
            {
                this->handle_select_request(c, doc);
                return;
            }
#endif

#ifdef USE_CLIMATE
            if (doc[FC("domain")] == "climate")
            {
                this->handle_climate_request(c, doc);
                return;
            }
#endif

#ifdef USE_LOCK
            if (doc[FC("domain")] == "lock")
            {
                this->handle_lock_request(c, doc);
                return;
            }
#endif


            if (doc[FC("domain")] == "auth")
            {
                this->handle_auth_request(c, doc);
                return;
            }
            if (doc[FC("domain")] == "alarm_panel")
            {
                  this->handle_alarm_panel_request(c, doc);
                return;
            }


#ifdef USE_ALARM_CONTROL_PANEL
            if (doc[FC("domain")] == "alarm_control_panel")
            {
                this->handle_alarm_control_panel_request(c, doc);
                return;
            }
#endif
            // mg_http_reply(c,404,"","");
            ws_reply(c, "", false);

        }

        void WebServer::add_sorting_info_(JsonObject &root, EntityBase *entity) {
#ifdef USE_WEBSERVER_SORTING
  if (this->sorting_entitys_.find(entity) != this->sorting_entitys_.end()) {
    root["sorting_weight"] = this->sorting_entitys_[entity].weight;
    if (this->sorting_groups_.find(this->sorting_entitys_[entity].group_id) != this->sorting_groups_.end()) {
      root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[entity].group_id].name;
    }
  }
#endif
}
        void WebServer::add_entity_config(EntityBase *entity, float weight, uint64_t group)
        {
            this->sorting_entitys_[entity] = SortingComponents{weight, group};
        }

        void WebServer::add_sorting_group(uint64_t group_id, const std::string &group_name, float weight)
        {
            this->sorting_groups_[group_id] = SortingGroup{group_name, weight};
        }

        void WebServer::schedule_(std::function<void()> &&f)
        {
#ifdef USE_ESP32
            xSemaphoreTake(this->to_schedule_lock_, portMAX_DELAY);
            to_schedule_.push_back(std::move(f));
            xSemaphoreGive(this->to_schedule_lock_);
#else
            this->defer(std::move(f));
#endif
        }

#ifdef USE_WEBKEYPAD_OTA

  void WebServer::report_ota_progress_() {
  const uint32_t now = millis();
  if (now - this->last_ota_progress_ > 1000) {
    float percentage = 0.0f;
      char buf[100];
     
//    if (request->contentLength() != 0) {
//      // Note: Using contentLength() for progress calculation is technically wrong as it includes
//      // multipart headers/boundaries, but it's only off by a small amount and we don't have
//      // access to the actual firmware size until the upload is complete. This is intentional
//      // as it still gives the user a reasonable progress indication.
//      percentage = (this->ota_read_length_ * 100.0f) / request->contentLength();
//      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
//    } else {

      snprintf(buf,100,"OTA in progress: %" PRIu32 " bytes written of %d", this->ota_read_length_,upl.filesize);
     #ifdef ESP8266
      ESP_LOGD(TAG,"OTA in progress: %" PRIu32 " bytes written of %d", this->ota_read_length_,upl.filesize);
     #else
      ESP_LOGD(TAG,buf);
      #endif
      this->push(OTA,buf);
//    }

    this->last_ota_progress_ = now;
  }
}

void WebServer::schedule_ota_reboot_() {
  ESP_LOGI(TAG, "OTA update successful!");
  this->set_timeout(2000, []() {
    ESP_LOGI(TAG, "Performing OTA reboot now");
    App.safe_reboot();
  });
}

void WebServer::ota_init_(const char *filename) {
  ESP_LOGI(TAG, "OTA Update Start: %s", filename);
  this->ota_read_length_ = 0;
  this->ota_success_ = false;
}



bool WebServer::handleUpload(size_t bodylen, const PlatformString &filename, size_t index, uint8_t *data, size_t len, bool final) {
                                      
ota::OTAResponseTypes error_code = ota::OTA_RESPONSE_OK;
char buf[100];
//ESP_LOGD("test", "before index");
  if (index == 0 && !this->ota_backend_) {
 //   ESP_LOGD("test", "after index");
    // Initialize OTA on first call
    this->ota_init_(filename.c_str());

 snprintf(buf, 100, "OTA Update Started: %s", filename.c_str());
                this->push(OTA, buf);

    // Platform-specific pre-initialization
#ifdef USE_ARDUINO
#ifdef USE_ESP8266
  //  Update.runAsync(false);
#endif
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
    if (Update.isRunning()) {
      Update.abort();
    }
#endif
#endif  // USE_ARDUINO

    this->ota_backend_ = ota::make_ota_backend();
    if (!this->ota_backend_) {
      snprintf(buf,100, "Failed to create OTA backend");
     #ifdef ESP8266
      ESP_LOGE(TAG,"Failed to create OTA backend");
     #else
      ESP_LOGE(TAG,buf);
     #endif

      this->push(OTA, buf);
      return false;
    }

    error_code = this->ota_backend_->begin(bodylen);
    if (error_code != ota::OTA_RESPONSE_OK) {
      snprintf(buf, 100,"OTA begin failed: %d", error_code);
     #ifdef ESP8266
      ESP_LOGE(TAG,"OTA begin failed: %d", error_code);
     #else
      ESP_LOGE(TAG,buf);
      #endif
      this->push(OTA, buf);
      this->ota_backend_.reset();
      return false;
    }
  }

  if (!this->ota_backend_) {
    return false;
  }

  // Process data
  if (len > 0) {
   // ESP_LOGD("test", "in process data");
    error_code = this->ota_backend_->write(data, len);
    if (error_code != ota::OTA_RESPONSE_OK) {
      snprintf(buf,100, "OTA write failed: %d", error_code);
     #ifdef ESP8266
      ESP_LOGE(TAG,"OTA write failed: %d", error_code);
     #else
      ESP_LOGE(TAG,buf);
      #endif
      this->push(OTA, buf);
      this->ota_backend_->abort();
      this->ota_backend_.reset();
      return false;
    }
    this->ota_read_length_ += len;
    this->report_ota_progress_();
  }
  

  // Finalize
  if (final) {
    ESP_LOGD(TAG, "OTA final chunk: index=%zu, len=%zu, total_read=%" PRIu32 ", contentLength=%zu", index, len,
             this->ota_read_length_, bodylen);

    // For Arduino framework, the Update library tracks expected size from firmware header
    // If we haven't received enough data, calling end() will fail
    // This can happen if the upload is interrupted or the client disconnects
    error_code = this->ota_backend_->end();
    if (error_code == ota::OTA_RESPONSE_OK) {
      this->ota_success_ = true;
      snprintf(buf, 100,"OTA completed");
      this->push(OTA, buf);
      this->schedule_ota_reboot_();
    } else {
      snprintf(buf, 100,"OTA end failed: %d", error_code);
     #ifdef ESP8266
      ESP_LOGE(TAG,"OTA end failed: %d", error_code);
     #else
      ESP_LOGE(TAG,buf);
      #endif
      this->push(OTA, buf);
      this->ota_backend_.reset();
      return false;
    }
    this->ota_backend_.reset();
    
  }
   return true;
 }
#endif //web_keypad_ota

    } // namespace web_server
} // namespace esphome
