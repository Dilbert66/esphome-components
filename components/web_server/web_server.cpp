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
#if not defined(ESP8266)
#include <Update.h>
#endif
#endif

namespace esphome
{
    namespace web_keypad
    {

#define KEYSIZE 32

        static const char *const TAG = "web_server";
        void *webServerPtr;

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
        static const char *const HEADER_PNA_NAME = "Private-Network-Access-Name";
        static const char *const HEADER_PNA_ID = "Private-Network-Access-ID";
        static const char *const HEADER_CORS_REQ_PNA = "Access-Control-Request-Private-Network";
        static const char *const HEADER_CORS_ALLOW_PNA = "Access-Control-Allow-Private-Network";
#endif

        void WebServer::parseUrlParams(char *queryString, int resultsMaxCt, boolean decodeUrl, JsonObject doc)
        {
            int ct = 0;
            char *name;
            char *value;
            if (decodeUrl)
                percentDecode(queryString);
            // MG_INFO(("query=%s",queryString));
            while (queryString && *queryString && ct < resultsMaxCt)
            {
                name = strsep(&queryString, "&");
                value = strchrnul(name, '=');

                if (*value)
                    *value++ = '\0';
                std::string n = std::string(name);
                std::string v = std::string(value);
                doc[n] = v;
                // MG_INFO(("parameter %s = %s",n.c_str(),v.c_str()));
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
            doc["domain"] = url.substr(1, domain_end - 1);
            if (url.length() == domain_end - 1)
                return;
            size_t id_begin = domain_end + 1;
            size_t id_end = url.find('/', id_begin);
            if (id_end == std::string::npos)
            {
                doc["oid"] = url.substr(id_begin, url.length() - id_begin);
            }
            doc["oid"] = url.substr(id_begin, id_end - id_begin);
            size_t method_begin = id_end + 1;
            doc["action"] = url.substr(method_begin, url.length() - method_begin);
            std::string a = doc["domain"];
            // MG_INFO(("in parseurl domain=%s",a.c_str()));
        }

        void WebServer::ws_reply(mg_connection *c, const char *data, bool ok)
        {
            if (c->data[0] != 'W')
            {
                if (ok)
                {
                    if (strlen(data) == 0)
                    {
                        mg_http_reply(c, 204, PSTR("Access-Control-Allow-Origin: *\r\n"), "");
                    }
                    else
                    {
                        if (credentials_.crypt)
                        {
                            if (c->data[1])
                            {
                                mg_http_reply(c, 404, "", "");
                                return;
                            }
                            else
                                data = encrypt(data).c_str();
                        }
                        //  ESP_LOGD(TAG,"sending %s",data);
                        mg_http_reply(c, 200, PSTR("Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n"), "%s", data);
                    }
                }
                else
                    mg_http_reply(c, 404, "", "");
            }
        }

        WebServer::WebServer()
            : entities_iterator_(ListEntitiesIterator(this))
        {
#ifdef USE_ESP32
            to_schedule_lock_ = xSemaphoreCreateMutex();
#endif
            webServerPtr = this;
        }

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
        void WebServer::set_css_include(const char *css_include) { this->css_include_ = css_include; }
#endif
#ifdef USE_WEBKEYPAD_JS_INCLUDE
        void WebServer::set_js_include(const char *js_include) { this->js_include_ = js_include; }
#endif

        void WebServer::set_keypad_config(const char *json_keypad_config)
        {
            _json_keypad_config = json_keypad_config;
        }

        const std::string WebServer::get_config_json(unsigned long cid)
        {

            uint8_t key[16];
            random_bytes(key, 16);
            c_data cd;
            std::string token = base64_encode(key, 16);
            cd.token = token;
            cd.lastseq = 0;
            tokens_[cid] = cd;
            return json::build_json([this, cid, token](JsonObject root)
                                    {
                                        root["title"] = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
                                        root["comment"] = App.get_comment();
                                        root["ota"] = this->allow_ota_;
                                        root["log"] = this->expose_log_;
                                        root["lang"] = "en";
                                        root["partitions"] = this->partitions_;
                                        root["keypad"] = this->show_keypad_;
                                        root["crypt"] = this->crypt_;
                                        root["cid"] = cid;
                                        root["token"] = token; });
        }

        const std::string WebServer::escape_json(const char *input)
        {
            std::string output;

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

            return output;
        }

        void WebServer::setup()
        {
            // ESP_LOGCONFIG(TAG, PSTR("Setting up web server..."));
            this->setup_controller(this->include_internal_);
            mg_mgr_init(&mgr);
#ifdef USE_LOGGER
            if (logger::global_logger != nullptr && this->expose_log_)
            {
                logger::global_logger->add_on_log_callback(
                    [this](int level, const char *tag, const char *message)
                    {
                        std::string msg = escape_json(message);
                        this->push(LOG, msg.c_str());
                    });
            }
#endif
            this->set_interval(10000, [this]()
                               { this->push(PING, "", millis(), 30000); });
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
                    // this->defer(std::move(to_schedule_.front()));
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
            this->entities_iterator_.advance();

            if (firstrun_ && network::is_connected())
            {
                char addr[50];
                sprintf(addr, "http://0.0.0.0:%d", port_);
                ESP_LOGD(TAG, "Starting web server on %s:%d", network::get_use_address().c_str(), port_);
                if ((c = mg_http_listen(&mgr, addr, ev_handler, this)) == NULL)
                {
                    printf("Cannot listen on address..");
                    return;
                }
                firstrun_ = false;
            }
            mg_mgr_poll(&mgr, 0);
        }
        void WebServer::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Web Server:");
            ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), port_);
        }
        float WebServer::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

        void WebServer::handle_index_request(struct mg_connection *c)
        {

            const char *buf = (const char *)ESPHOME_WEBKEYPAD_INDEX_HTML;
            mg_printf(c, PSTR("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n"), ESPHOME_WEBKEYPAD_INDEX_HTML_SIZE);
            mg_send(c, buf, ESPHOME_WEBKEYPAD_INDEX_HTML_SIZE);
            c->is_resp = 0;
        }

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
        void WebServer::handle_pna_cors_request(struct mg_connection *c)
        {

            std::string mac = get_mac_address_pretty();
            mg_printf(c, PSTR("HTTP/1.1 200 OK\r\n%s:%s\r\n%s:%s\r\n%s:%s\r\n\r\n"), HEADER_CORS_ALLOW_PNA, "true", HEADER_PNA_NAME, App.get_name().c_str(), HEADER_PNA_ID, mac.c_str());
            c->is_resp = 0;
            // MG_INFO((" in cors header %s",HEADER_CORS_ALLOW_PNA));
        }
#endif

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
        void WebServer::handle_css_request(struct mg_connection *c)
        {

            const char *buf = (const char *)ESPHOME_WEBKEYPAD_CSS_INCLUDE;
            mg_printf(c, PSTR("HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\nContent-Encoding: gzip\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n"), ESPHOME_WEBKEYPAD_CSS_INCLUDE_SIZE);
            mg_send(c, buf, ESPHOME_WEBKEYPAD_CSS_INCLUDE_SIZE);
            c->is_resp = 0;
        }
#endif

#ifdef USE_WEBKEYPAD_JS_INCLUDE
        void WebServer::handle_js_request(struct mg_connection *c)
        {

            const char *buf = (const char *)ESPHOME_WEBKEYPAD_JS_INCLUDE;
            mg_printf(c, PSTR("HTTP/1.1 200 OK\r\nContent-Type: text/javascript; charset=utf-8\r\nContent-Encoding: gzip\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\n\r\n"), ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE);
            for (int s = 0; s < ESPHOME_WEBKEYPAD_JS_INCLUDE_SIZE; s = s + 1024)
            { // we send the file in blocks of 1024 then run poll to purge the buffer out in order to keep io buffer size small
                mg_send(c, &buf[s], 1024);
                mg_mgr_poll(&mgr, 0);
            }
            c->is_resp = 0;
        }
#endif

#define set_json_id(root, obj, sensor, start_config)                            \
    (root)["id"] = sensor;                                                      \
    if (((start_config) == DETAIL_ALL))                                         \
    {                                                                           \
        (root)["name"] = (obj)->get_name();                                     \
        (root)["icon"] = (obj)->get_icon();                                     \
        (root)["entity_category"] = (obj)->get_entity_category();               \
        if ((obj)->is_disabled_by_default())                                    \
            (root)["is_disabled_by_default"] = (obj)->is_disabled_by_default(); \
    }

#define set_json_value(root, obj, sensor, value, start_config) \
    set_json_id((root), (obj), sensor, start_config);          \
    (root)["value"] = value;

#define set_json_icon_state_value(root, obj, sensor, state, value, start_config) \
    set_json_value(root, obj, sensor, value, start_config);                      \
    (root)["state"] = state;

#ifdef USE_SENSOR
        void WebServer::on_sensor_update(sensor::Sensor *obj, float state)
        {
            this->push(STATE, this->sensor_json(obj, state, DETAIL_STATE).c_str());
        }

        void WebServer::handle_sensor_request(mg_connection *c, JsonObject doc)
        {
            for (sensor::Sensor *obj : App.get_sensors())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;
                auto detail = DETAIL_STATE;
                if (doc.containsKey("detail"))
                {
                    if (doc["detail"] == "all")
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
    std::string state;
    if (std::isnan(value)) {
      state = "NA";
    } else {
      state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
      if (!obj->get_unit_of_measurement().empty())
        state += " " + obj->get_unit_of_measurement();
    }
    //set_json_icon_state_value(root, obj, "sensor-" + obj->get_object_id(), state, value, start_config); });

    set_json_icon_state_value(root, obj, "sensor-" + obj->get_object_id(), state, value, start_config);
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
      if (!obj->get_unit_of_measurement().empty())
        root["uom"] = obj->get_unit_of_measurement();
    } });
        }
#endif

#ifdef USE_TEXT_SENSOR
        void WebServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state)
        {
            // this->events_.send(this->text_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
            std::string data = this->text_sensor_json(obj, state, DETAIL_STATE);

#if defined(USE_CUSTOM_ID) || defined(USE_TEMPLATE_ALARM_SENSORS)
            // std::string id =obj->get_type_id();
            // if (id.substr(0,2)=="ln" && get_credentials()->crypt) //encrypt display lines
            //  data=encrypt(data.c_str());
#endif

            this->push(STATE, data.c_str());
        }

        void WebServer::handle_text_sensor_request(mg_connection *c, JsonObject doc)
        {
            for (text_sensor::TextSensor *obj : App.get_text_sensors())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;
                auto detail = DETAIL_STATE;
                if (doc.containsKey("detail"))
                {
                    if (doc["detail"] == "all")
                    {
                        detail = DETAIL_ALL;
                    }
                }
                std::string data = this->text_sensor_json(obj, obj->state, detail);

                // request->send(200, "application/json", data.c_str());
                // mg_http_reply(c, 200, "Content-Type: application/jsonAccess-Control-Allow-Origin: *\r\n\r\n", "%s", data.c_str());
                ws_reply(c, data.c_str(), true);
                return;
            }
            ws_reply(c, "", false);
        }

        std::string WebServer::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value,
                                                JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
   // set_json_icon_state_value(root, obj, "text_sensor-" +  obj->get_object_id(), value, value, start_config);
  //root["id_code"]=obj->get_object_id(); });
      set_json_icon_state_value(root, obj, "text_sensor-" + obj->get_object_id(), value, value, start_config);
    root["id_code"] = obj->get_object_id();
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;

        }
      }
    } });
        }

#endif

#ifdef USE_SWITCH
        void WebServer::on_switch_update(switch_::Switch *obj, bool state)
        {
            // this->events_.send(this->switch_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->switch_json(obj, state, DETAIL_STATE).c_str());
        }

        std::string WebServer::switch_json(switch_::Switch *obj, bool value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
    // set_json_icon_state_value(root, obj, "switch-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
    // if (start_config == DETAIL_ALL) {
    //   root["assumed_state"] = obj->assumed_state();
    // } });
        set_json_icon_state_value(root, obj, "switch-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
    if (start_config == DETAIL_ALL) {
      root["assumed_state"] = obj->assumed_state();
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
        void WebServer::handle_switch_request(mg_connection *c, JsonObject doc)
        {
            for (switch_::Switch *obj : App.get_switches())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;
                // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
                //  if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["action"] == "get")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->switch_json(obj, obj->state, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["action"] == "toggle")
                {
                    this->schedule_([obj]()
                                    { obj->toggle(); });
                    // mg_http_reply(c,200,"","");
                    ws_reply(c, "", true);
                }
                else if (doc["action"] == "turn_on")
                {
                    this->schedule_([obj]()
                                    { obj->turn_on(); });
                    // mg_http_reply(c,200,"","");
                    ws_reply(c, "", true);
                }
                else if (doc["action"] == "turn_off")
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
                    // set_json_id(root, obj, "button-" + obj->get_object_id(), start_config); });
                    set_json_id(root, obj, "button-" + obj->get_object_id(), start_config);
                    if (start_config == DETAIL_ALL)
                    {
                        if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end())
                        {
                            root["sorting_weight"] = this->sorting_entitys_[obj].weight;
                            if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end())
                            {
                                root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
                            }
                        }
                    }
                });
        }

        void WebServer::handle_button_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (button::Button *obj : App.get_buttons())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;
                if (doc["method"] == "GET" && doc["method"] == "")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->button_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["method"] == "POST" && doc["action"] == "press")
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
        void WebServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state)
        {
            // this->events_.send(this->binary_sensor_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->binary_sensor_json(obj, state, DETAIL_STATE).c_str());
        }

        std::string WebServer::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
                         //       set_json_state_value(root, obj, "binary_sensor-" + obj->get_object_id(), value ? "ON" : "OFF", value, start_config);
                            //    root["id_code"] = obj->get_object_id(); });
    set_json_icon_state_value(root, obj, "binary_sensor-" + obj->get_object_id(), value ? "ON" : "OFF", value,
                              start_config);
    root["id_code"] = obj->get_object_id();
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;

        }
      }
    } });
        }

        void WebServer::handle_binary_sensor_request(mg_connection *c, JsonObject doc)
        {
            for (binary_sensor::BinarySensor *obj : App.get_binary_sensors())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;
                auto detail = DETAIL_STATE;
                if (doc.containsKey("detail"))
                {
                    if (doc["detail"] == "all")
                    {
                        detail = DETAIL_ALL;
                    }
                }
                std::string data = this->binary_sensor_json(obj, obj->state, detail);
                // request->send(200, "application/json", data.c_str());
                // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                ws_reply(c, data.c_str(), true);
                return;
            }
            ws_reply(c, "", false);
        }
#endif

#ifdef USE_FAN
        void WebServer::on_fan_update(fan::Fan *obj)
        {
            // this->p.send(this->fan_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->fan_json(obj, DETAIL_STATE).c_str());
        }
        std::string WebServer::fan_json(fan::Fan *obj, JsonDetail start_config)
        {

            return json::build_json([this, obj, start_config](JsonObject root)
                                    {
    // set_json_state_value(root, obj, "fan-" + obj->get_object_id(), obj->state ? "ON" : "OFF", obj->state, start_config);
    // const auto traits = obj->get_traits();
    // if (traits.supports_speed()) {
    //   root["speed_level"] = obj->speed;
    //   root["speed_count"] = traits.supported_speed_count();
    // }
    // if (obj->get_traits().supports_oscillation())
    //   root["oscillation"] = obj->oscillating; });
        set_json_icon_state_value(root, obj, "fan-" + obj->get_object_id(), obj->state ? "ON" : "OFF", obj->state,
                              start_config);
    const auto traits = obj->get_traits();
    if (traits.supports_speed()) {
      root["speed_level"] = obj->speed;
      root["speed_count"] = traits.supported_speed_count();
    }
    if (obj->get_traits().supports_oscillation())
      root["oscillation"] = obj->oscillating;
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;

        }
      }
    } });
        }
        void WebServer::handle_fan_request(mg_connection *c, JsonObject doc)
        {

            for (fan::Fan *obj : App.get_fans())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->fan_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["action"] == "toggle")
                {
                    this->schedule_([obj]()
                                    { obj->toggle().perform(); });
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["action"] == "turn_on")
                {
                    auto call = obj->turn_on();
                    // if (request->hasParam("speed_level")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"speed_level",buf,sizeof(buf)) > 0) {
       if (doc.containsKey("speed_level") {
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
        if (doc.containsKey("oscillation")) {
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
                else if (doc["action"] == "turn_off")
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
            // this->events_.send(this->light_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->light_json(obj, DETAIL_STATE).c_str());
        }
        void WebServer::handle_light_request(mg_connection *c, JsonObject doc)
        {

            for (light::LightState *obj : App.get_lights())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->light_json(obj, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["action"] == "toggle")
                {
                    this->schedule_([obj]()
                                    { obj->toggle().perform(); });
                    // mg_http_reply(c,200,"","");
                    ws_reply(c, "", true);
                }
                else if (doc["action"] == "turn_on")
                {
                    auto call = obj->turn_on();
                    // if (request->hasParam("brightness")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"brightness",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("brightness"))
                    {
                        auto brightness = parse_number<float>(doc["brightness"]);
                        if (brightness.has_value())
                        {
                            call.set_brightness(*brightness / 255.0f);
                        }
                    }
                    // if (request->hasParam("r")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"r",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("r"))
                    {
                        auto r = parse_number<float>(doc["r"]);
                        if (r.has_value())
                        {
                            call.set_red(*r / 255.0f);
                        }
                    }
                    // if (request->hasParam("g")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"g",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("g"))
                    {
                        auto g = parse_number<float>(doc["g"]);
                        if (g.has_value())
                        {
                            call.set_green(*g / 255.0f);
                        }
                    }
                    // if (request->hasParam("b")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"b",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("b"))
                    {
                        auto b = parse_number<float>(doc["b"]);
                        if (b.has_value())
                        {
                            call.set_blue(*b / 255.0f);
                        }
                    }
                    // if (request->hasParam("white_value")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"white_value",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("white_value"))
                    {
                        auto white_value = parse_number<float>(doc["white_value"]);
                        if (white_value.has_value())
                        {
                            call.set_white(*white_value / 255.0f);
                        }
                    }
                    // if (request->hasParam("color_temp")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"color_temp",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("color_temp"))
                    {
                        auto color_temp = parse_number<float>(doc["color_temp"]);
                        if (color_temp.has_value())
                        {
                            call.set_color_temperature(*color_temp);
                        }
                    }
                    // if (request->hasParam("flash")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"flash",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("flash"))
                    {
                        auto flash = parse_number<uint32_t>(doc["flash"]);
                        if (flash.has_value())
                        {
                            call.set_flash_length(*flash * 1000);
                        }
                    }
                    // if (request->hasParam("transition")) {
                    //  char buf[100];
                    // if (mg_http_get_var(&hm->body,"transition",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("transition"))
                    {
                        auto transition = parse_number<uint32_t>(doc["transition"]);
                        if (transition.has_value())
                        {
                            call.set_transition_length(*transition * 1000);
                        }
                    }
                    // if (request->hasParam("effect")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"effect",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("effect"))
                    {
                        const char *effect = doc["effect"];
                        call.set_effect(effect);
                    }

                    this->schedule_([call]() mutable
                                    { call.perform(); });
                    ws_reply(c, "", true);
                } // else if (match.method == "turn_off") {
                else if (doc["action"] == "turn_off")
                {
                    auto call = obj->turn_off();
                    // if (request->hasParam("transition")) {
                    // char buf[100];
                    // if (mg_http_get_var(&hm->body,"transition",buf,sizeof(buf)) > 0) {
                    if (doc.containsKey("transition"))
                    {
                        auto transition = parse_number<uint32_t>(doc["transition"]);
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
    // set_json_id(root, obj, "light-" + obj->get_object_id(), start_config);
    // root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

    // light::LightJSONSchema::dump_json(*obj, root);
    // if (start_config == DETAIL_ALL) {
    //   JsonArray opt = root.createNestedArray("effects");
    //   opt.add("None");
    //   for (auto const &option : obj->get_effects()) {
    //     opt.add(option->get_name());
    //   }
    // } });
        set_json_id(root, obj, "light-" + obj->get_object_id(), start_config);
    root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";

    light::LightJSONSchema::dump_json(*obj, root);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("effects");
      opt.add("None");
      for (auto const &option : obj->get_effects()) {
        opt.add(option->get_name());
      }
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif

#ifdef USE_COVER
        void WebServer::on_cover_update(cover::Cover *obj)
        {
            // this->events_.send(this->cover_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->cover_json(obj, DETAIL_STATE).c_str());
        }
        void WebServer::handle_cover_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (cover::Cover *obj : App.get_covers())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
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
                if (doc["action"] == "open")
                {
                    call.set_command_open();
                    // } else if (match.method == "close") {
                }
                else if (doc["action"] == "close")
                {
                    call.set_command_close();
                    //} else if (match.method == "stop") {
                }
                else if (doc["action"] == "stop")
                {
                    call.set_command_stop();
                    // } else if (match.method != "set") {
                }
                else if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto traits = obj->get_traits();

                // if ((request->hasParam("position") && !traits.get_supports_position()) ||
                //(request->hasParam("tilt") && !traits.get_supports_tilt())) {
                // char buf[50];
                // bool p= (mg_http_get_var(&hm->body,"position",buf,sizeof(buf)) > 0);
                if ((doc.containsKey("position") && !traits.get_supports_position()) || (doc.containsKey("tilt") && !traits.get_supports_tilt()))
                {

                    // bool t= (mg_http_get_var(&hm->body,"tilt",buf,sizeof(buf)) > 0);
                    //  if (p && !traits.get_supports_position()) || t && !traits.get_supports_tilt())) {
                    // request->send(409);
                    ws_reply(c, "", false);
                    return;
                }

                // char buf[100];
                // if (mg_http_get_var(&hm->body,"position",buf,sizeof(buf)) > 0) {
                if (doc.containsKey("position"))
                {
                    auto position = parse_number<float>(doc["position"]);
                    if (position.has_value())
                    {
                        call.set_position(*position);
                    }
                }
                // if (mg_http_get_var(&hm->body,"tilt",buf,sizeof(buf)) > 0) {
                if (doc.containsKey("tilt"))
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
    // set_json_state_value(root, obj, "cover-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
    //                      obj->position, start_config);
    // root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    // if (obj->get_traits().get_supports_tilt())
    //   root["tilt"] = obj->tilt; });
      set_json_icon_state_value(root, obj, "cover-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
                              obj->position, start_config);
    root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_position())
      root["position"] = obj->position;
    if (obj->get_traits().get_supports_tilt())
      root["tilt"] = obj->tilt;
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif

#ifdef USE_NUMBER
        void WebServer::on_number_update(number::Number *obj, float state)
        {
            // this->events_.send(this->number_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->number_json(obj, state, DETAIL_STATE).c_str());
        }
        void WebServer::handle_number_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (auto *obj : App.get_numbers())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->number_json(obj, obj->state, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();

                if (doc.containsKey("value"))
                {
                    std::string value = doc["value"];
                    auto value = parse_number<float>(value);
                    if (value.has_value())
                        call.set_value(*value);
                }

                this->schedule_([call]() mutable
                                { call.perform(); });
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
        }

        std::string WebServer::number_json(number::Number *obj, float value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
    // set_json_id(root, obj, "number-" + obj->get_object_id(), start_config);
    // if (start_config == DETAIL_ALL) {
    //   root["min_value"] = obj->traits.get_min_value();
    //   root["max_value"] = obj->traits.get_max_value();
    //   root["step"] = obj->traits.get_step();
    //   root["mode"] = (int) obj->traits.get_mode();
    // }
    // if (std::isnan(value)) {
    //   root["value"] = "\"NaN\"";
    //   root["state"] = "NA";
    // } else {
    //   root["value"] = value;
    //   std::string state = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
    //   if (!obj->traits.get_unit_of_measurement().empty())
    //     state += " " + obj->traits.get_unit_of_measurement();
    //   root["state"] = state;
    // } });
       set_json_id(root, obj, "number-" + obj->get_object_id(), start_config);
    if (start_config == DETAIL_ALL) {
      root["min_value"] =
          value_accuracy_to_string(obj->traits.get_min_value(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["max_value"] =
          value_accuracy_to_string(obj->traits.get_max_value(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["step"] =
          value_accuracy_to_string(obj->traits.get_step(), step_to_accuracy_decimals(obj->traits.get_step()));
      root["mode"] = (int) obj->traits.get_mode();
      if (!obj->traits.get_unit_of_measurement().empty())
        root["uom"] = obj->traits.get_unit_of_measurement();
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    }
    if (std::isnan(value)) {
      root["value"] = "\"NaN\"";
      root["state"] = "NA";
    } else {
      root["value"] = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      std::string state = value_accuracy_to_string(value, step_to_accuracy_decimals(obj->traits.get_step()));
      if (!obj->traits.get_unit_of_measurement().empty())
        state += " " + obj->traits.get_unit_of_measurement();
      root["state"] = state;
    } });
        }
#endif

#ifdef USE_DATETIME_DATE
        void WebServer::on_date_update(datetime::DateEntity *obj)
        {
            // this->events_.send(this->date_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->date_json(obj, DETAIL_STATE).c_str());
        }

        void WebServer::handle_date_request(mg_connection *c, JsonObject doc)
        {
            for (auto *obj : App.get_dates())
            {

                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->date_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                // if (obj->get_object_id() != match.id)
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
                if (doc.containsKey("value"))
                {
                    std::string value = doc["value"];
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
    set_json_id(root, obj, "date-" + obj->get_object_id(), start_config);
    std::string value = str_sprintf("%d-%02d-%02d", obj->year, obj->month, obj->day);
    root["value"] = value;
    root["state"] = value;
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
        void WebServer::on_time_update(datetime::TimeEntity *obj)
        {
            this->push(STATE, this->time_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->time_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_time_request(mg_connection *c, JsonObject doc)
        {
            for (auto *obj : App.get_times())
            {

                // if (obj->get_object_id() != match.id)
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
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->time_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();
                if (doc.containsKey("value"))
                {
                    std::string value = doc["value"];
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
    set_json_id(root, obj, "time-" + obj->get_object_id(), start_config);
    std::string value = str_sprintf("%02d:%02d:%02d", obj->hour, obj->minute, obj->second);
    root["value"] = value;
    root["state"] = value;
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
        void WebServer::on_datetime_update(datetime::DateTimeEntity *obj)
        {
            this->push(STATE, this->datetime_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->datetime_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_datetime_request(mg_connection *c, JsonObject doc)
        {
            for (auto *obj : App.get_datetimes())
            {

                // if (obj->get_object_id() != match.id)
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

                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->datetime_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();
                if (doc.containsKey("value"))
                {
                    std::string value = doc["value"];
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
    set_json_id(root, obj, "datetime-" + obj->get_object_id(), start_config);
    std::string value = str_sprintf("%d-%02d-%02d %02d:%02d:%02d", obj->year, obj->month, obj->day, obj->hour,
                                    obj->minute, obj->second);
    root["value"] = value;
    root["state"] = value;
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
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
                // if (obj->get_object_id() != match.id)
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

                if (obj->get_object_id() != doc["oid"])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
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
    set_json_id(root, obj, "event-" + obj->get_object_id(), start_config);
    if (!event_type.empty()) {
      root["event_type"] = event_type;
    }
    if (start_config == DETAIL_ALL) {
      JsonArray event_types = root.createNestedArray("event_types");
      for (auto const &event_type : obj->get_event_types()) {
        event_types.add(event_type);
      }
      root["device_class"] = obj->get_device_class();
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif

#ifdef USE_UPDATE
        void WebServer::on_update(update::UpdateEntity *obj)
        {
            this->push(STATE, this->update_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->update_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_update_request(mg_connection *c, JsonObject doc)
        {
            for (update::UpdateEntity *obj : App.get_updates())
            {

                // if (obj->get_object_id() != match.id)
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
                if (obj->get_object_id() != doc["oid"])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->update_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }

                if (doc["action"] != "install")
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
    set_json_id(root, obj, "update-" + obj->get_object_id(), start_config);
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
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif

#ifdef USE_VALVE
        void WebServer::on_valve_update(valve::Valve *obj)
        {
            this->push(STATE, this->valve_json(obj, DETAIL_STATE).c_str());
            // this->events_.send(this->valve_json(obj, DETAIL_STATE).c_str(), "state");
        }
        void WebServer::handle_valve_request(mg_connection *c, JsonObject doc)
        {
            for (valve::Valve *obj : App.get_valves())
            {

                if (obj->get_object_id() != doc["oid"])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->valve_json(obj, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }
                auto call = obj->make_call();
                if (doc["action"] == "open")
                {
                    call.set_command_open();
                }
                else if (doc["action"] == "close")
                {
                    call.set_command_close();
                }
                else if (doc["action"] == "stop")
                {
                    call.set_command_stop();
                }
                else if (doc["action"] == "toggle")
                {
                    call.set_command_toggle();
                }
                else if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                }

                if (doc.containsKey("position"))
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
    set_json_icon_state_value(root, obj, "valve-" + obj->get_object_id(), obj->is_fully_closed() ? "CLOSED" : "OPEN",
                              obj->position, start_config);
    root["current_operation"] = valve::valve_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_position())
      root["position"] = obj->position;
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif

#ifdef USE_TEXT
        void WebServer::on_text_update(text::Text *obj, const std::string &state)
        {
            // this->events_.send(this->text_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->text_json(obj, state, DETAIL_STATE).c_str());
        }

        void WebServer::handle_text_request(mg_connection *c, JsonObject doc)
        {

            for (auto *obj : App.get_texts())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->text_json(obj, obj->state, detail);
                    ws_reply(c, data.c_str(), true);
                    return;
                }
                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }
                auto call = obj->make_call();
                if (doc.containsKey("value"))
                {
                    call.set_value(doc["value"]);
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
    // set_json_id(root, obj, "text-" + obj->get_object_id(), start_config);
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
        set_json_id(root, obj, "text-" + obj->get_object_id(), start_config);
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
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
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

            if (doc["action"] != "set")
            {
                ws_reply(c, "", false);
                return;
            }
            if (doc.containsKey("cid"))
            {
                // cid = toInt(doc["partition"],10);
                unsigned long ul = (unsigned long)doc["cid"];
                for (struct mg_connection *cl = mgr.conns; cl != NULL; cl = cl->next)
                {
                    if (cl->id == ul)
                    {
                        cl->data[1] = 1;
                        ESP_LOGD(TAG, "Set auth conn %d as 1", cl->id);
                        entities_iterator_.begin(this->include_internal_);
                        break;
                    }
                }
                ws_reply(c, "", true);
                return;
            }
            ws_reply(c, "", false);
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
                if (doc["action"] == "getconfig" && strlen(_json_keypad_config) > 0)
                {
                    ws_reply(c, _json_keypad_config, true);
                    return;
                }
                // ws_reply(c,"",true);
                // return;
            }
            if (doc["action"] != "set")
            {
                ws_reply(c, "", false);
                return;
            }
            int partition = 1; // get default partition
            if (doc.containsKey("partition"))
            {
                partition = toInt(doc["partition"], 10);
            }
            if (doc.containsKey("keys"))
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
        void WebServer::on_select_update(select::Select *obj, const std::string &state, size_t index)
        {
            // this->events_.send(this->select_json(obj, state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->select_json(obj, state, DETAIL_STATE).c_str());
        }
        void WebServer::handle_select_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (auto *obj : App.get_selects())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                //   if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
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
                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();

                // if (mg_http_get_var(&hm->body,"option",buf,sizeof(buf)) > 0) {
                if (doc.containsKey("option"))
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
    // set_json_state_value(root, obj, "select-" + obj->get_object_id(), value, value, start_config);
    // if (start_config == DETAIL_ALL) {
    //   JsonArray opt = root.createNestedArray("option");
    //   for (auto &option : obj->traits.get_options()) {
    //     opt.add(option);
    //   }
    // } });
        set_json_icon_state_value(root, obj, "select-" + obj->get_object_id(), value, value, start_config);
    if (start_config == DETAIL_ALL) {
      JsonArray opt = root.createNestedArray("option");
      for (auto &option : obj->traits.get_options()) {
        opt.add(option);
      }
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
#endif

// Longest: HORIZONTAL
#define PSTR_LOCAL(mode_s) strncpy_P(buf, (PGM_P)((mode_s)), 15)

#ifdef USE_CLIMATE
        void WebServer::on_climate_update(climate::Climate *obj)
        {
            // this->events_.send(this->climate_json(obj, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->climate_json(obj, DETAIL_STATE).c_str());
        }

        void WebServer::handle_climate_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (auto *obj : App.get_climates())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
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
                if (doc["action"] != "set")
                {
                    ws_reply(c, "", false);
                    return;
                }

                auto call = obj->make_call();
                // char buf[100];
                // if (mg_http_get_var(&hm->body,"mode",buf,sizeof(buf)) > 0) {
                if (doc.containsKey("mode"))
                {
                    // if (request->hasParam("mode")) {
                    //  auto mode = request->getParam("mode")->value();
                    //  auto mode=buf;
                    call.set_mode(doc["mode"]);
                }
                // if (mg_http_get_var(&hm->body,"target_temperature_high",buf,sizeof(buf)) > 0) {
                // auto target_temperature_high=buf;
                if (doc.containsKey("target_temperature_high"))
                {

                    // if (request->hasParam("target_temperature_high")) {
                    auto target_temperature_high = parse_number<float>(doc["target_temperature_high"]);
                    if (target_temperature_high.has_value())
                        call.set_target_temperature_high(*target_temperature_high);
                }

                // if (request->hasParam("target_temperature_low")) {
                // auto target_temperature_low = parse_number<float>(request->getParam("target_temperature_low")->value().c_str());
                // if (mg_http_get_var(&hm->body,"target_temperature_low",buf,sizeof(buf)) > 0) {
                if (doc.containsKey("target_temperature_low"))
                {
                    auto target_temperature_low = parse_number<float>(doc["target_temperature_low"]);
                    if (target_temperature_low.has_value())
                        call.set_target_temperature_low(*target_temperature_low);
                }

                // if (request->hasParam("target_temperature")) {
                //  auto target_temperature = parse_number<float>(request->getParam("target_temperature")->value().c_str());
                // if (mg_http_get_var(&hm->body,"target_temperature",buf,sizeof(buf)) > 0) {
                if (doc.contains("target_temperature"))
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
    // set_json_id(root, obj, "climate-" + obj->get_object_id(), start_config);
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
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
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
    } });
        }
#endif

#ifdef USE_LOCK
        void WebServer::on_lock_update(lock::Lock *obj)
        {
            // this->events_.send(this->lock_json(obj, obj->state, DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->lock_json(obj, obj->state, DETAIL_STATE).c_str());
        }
        std::string WebServer::lock_json(lock::Lock *obj, lock::LockState value, JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {
                                //  set_json_icon_state_value(root, obj, "lock-" + obj->get_object_id(), lock::lock_state_to_string(value), value,
                                //                           start_config); });
                                    set_json_icon_state_value(root, obj, "lock-" + obj->get_object_id(), lock::lock_state_to_string(value), value,
                              start_config);
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
        void WebServer::handle_lock_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (lock::Lock *obj : App.get_locks())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;

                // if (request->method() == HTTP_GET) {
                // if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
                        {
                            detail = DETAIL_ALL;
                        }
                    }
                    std::string data = this->lock_json(obj, obj->state, detail);
                    // request->send(200, "application/json", data.c_str());
                    // mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", data.c_str());
                    ws_reply(c, data.c_str(), true);
                }
                else if (doc["action"] == "lock")
                {
                    this->schedule_([obj]()
                                    { obj->lock(); });
                    ws_reply(c, "", true);
                }
                else if (doc["action"] == "unlock")
                {
                    this->schedule_([obj]()
                                    { obj->unlock(); });
                    ws_reply(c, "", true);
                }
                else if (doc["action"] == "open")
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
            // this->events_.send(this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str(), "state");
            this->push(STATE, this->alarm_control_panel_json(obj, obj->get_state(), DETAIL_STATE).c_str());
        }
        std::string WebServer::alarm_control_panel_json(alarm_control_panel::AlarmControlPanel *obj,
                                                        alarm_control_panel::AlarmControlPanelState value,
                                                        JsonDetail start_config)
        {
            return json::build_json([this, obj, value, start_config](JsonObject root)
                                    {

    //  char buf[16];
    // set_json_icon_state_value(root, obj, "alarm-control-panel-" + obj->get_object_id(),
    //                           PSTR_LOCAL(alarm_control_panel_state_to_string(value)), value, start_config); });
        char buf[16];
    set_json_icon_state_value(root, obj, "alarm-control-panel-" + obj->get_object_id(),
                              PSTR_LOCAL(alarm_control_panel_state_to_string(value)), value, start_config);
    if (start_config == DETAIL_ALL) {
      if (this->sorting_entitys_.find(obj) != this->sorting_entitys_.end()) {
        root["sorting_weight"] = this->sorting_entitys_[obj].weight;
        if (this->sorting_groups_.find(this->sorting_entitys_[obj].group_id) != this->sorting_groups_.end()) {
          root["sorting_group"] = this->sorting_groups_[this->sorting_entitys_[obj].group_id].name;
        }
      }
    } });
        }
        void WebServer::handle_alarm_control_panel_request(mg_connection *c, JsonObject doc)
        {
            // struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            for (alarm_control_panel::AlarmControlPanel *obj : App.get_alarm_control_panels())
            {
                if (obj->get_object_id() != doc["oid"])
                    continue;
                if (doc["method"] == "GET")
                {
                    auto detail = DETAIL_STATE;
                    if (doc.containsKey("detail"))
                    {
                        if (doc["detail"] == "all")
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
            struct mg_connection *c;
            std::string type;
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

            std::string newdata;

            if (credentials_.crypt && strlen(data) > 0)
                newdata = encrypt(data);
            else
                newdata = std::string(data);

            for (c = mgr.conns; c != NULL; c = c->next)
            {

                if (credentials_.crypt && !c->data[1])
                    continue; // not authenticated with encrypted response

                if (c->data[0] == 'E')
                {
                    // ESP_LOGD(TAG,"type=%s,len=%d,data=%s",type.c_str(),strlen(data),data);
                    if (id && reconnect)
                        mg_printf(c, PSTR("id: %d\r\nretry: %d\r\nevent: %s\r\ndata: %s\r\n\r\n"), id, reconnect, type.c_str(), newdata.c_str());
                    else
                        mg_printf(c, PSTR("event: %s\r\ndata: %s\r\n\r\n"), type.c_str(), newdata.c_str());

                    if (c->send.len > 15000)
                        c->is_closing = 1; // dead connection. kill it.
                    continue;
                }

                if (c->data[0] != 'W')
                    continue;

                if (mt == PING)
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, PSTR("{\"%s\":\"%s\",\"%s\":\"%d\"}"), "type", type.c_str(), "data", id);
                else if ((mt == LOG || mt == OTA) && !credentials_.crypt)
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, PSTR("{\"%s\":\"%s\",\"%s\":\"%s\"}"), "type", type.c_str(), "data", newdata.c_str());
                else
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, PSTR("{\"%s\":\"%s\",\"%s\":%s}"), "type", type.c_str(), "data", newdata.c_str());

                if (c->send.len > 15000)
                    c->is_closing = 1; // dead connection. kill it.
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
                      PSTR("HTTP/1.1 401 Unauthorized\r\n"
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

            mg_str realm = mg_str(auth_domain);
            std::string u = std::string(username.buf, username.len);
            std::string r = std::string(response.buf, response.len);
            // if (strcmp(creds->username.c_str(),u.c_str())) {
            // MG_INFO(("password=%s",creds->password.c_str()));
            mg_mkmd5resp(
                hm->method.buf, hm->method.len, hm->uri.buf,
                hm->uri.len,
                &username, creds->password.c_str(), &realm, &nonce, &nc, &cnonce,
                &qop, expected_response);
            // MG_INFO(("response =%s, expected=%s, cusername=%s,u=%s",r.c_str(),expected_response,creds->username.c_str(),u.c_str()));
            return mg_casecmp(r.c_str(), expected_response) == 0;
            // }

            /* None of the entries in the passwords file matched - return failure */
            return 0;
        }

        const std::string WebServer::encrypt(const char *message)
        {
            int i = strlen(message);
            // ESP_LOGD(TAG,"len=%d",i);
            if (!i)
                return "";
            int buf = round(i / 16) * 16;
            int length = (buf <= i) ? buf + 16 : buf;
            uint8_t encrypted[length];
            uint8_t iv[16];
            random_bytes(iv, 16);

            std::string eiv = base64_encode(iv, 16);

            AES aes(credentials_.token, iv, AES::AES_MODE_256, AES::CIPHER_ENCRYPT);
            aes.process((uint8_t *)message, &encrypted[0], i);

            std::string em = base64_encode(&encrypted[0], length);

            SHA256HMAC hmac(credentials_.hmackey, 32);
            hmac.doUpdate(eiv.c_str(), eiv.length());
            hmac.doUpdate(em.c_str(), em.length());
            uint8_t authCode[SHA256HMAC_SIZE];
            hmac.doFinal(authCode);

            // std::string ehm=base64_encode(authCode,SHA256HMAC_SIZE);

            std::string enc = "{\"iv\":\"" + eiv + "\",\"data\":\"";
            enc.append(em);
            enc.append("\",\"hash\":\"" + base64_encode(authCode, SHA256HMAC_SIZE) + "\"}");
            // ESP_LOGD(TAG,"message size=%d,length=%d,ensize=%d,output=%s",i,length,encrypted_size,enc.c_str());
            // ESP_LOGD(TAG,"hmac=%s",ehm.c_str());
            return enc;
        }

        const std::string WebServer::decrypt(JsonObject doc, uint8_t *err)
        {
            const char *iv = doc["iv"];
            const char *data = doc["data"];
            std::string hash = doc["hash"];
            unsigned long cid = 0;

            std::string token = "";
            const char *seqstr = "";
            int seq = 0;
            int *lastseq = NULL;
            // right now we don't force a seq/cid field in the encrypted packet.
            if (doc.containsKey("seq"))
            {
                seqstr = doc["seq"];
                seq = (unsigned long)doc["seq"];
            }
            if (doc.containsKey("cid"))
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
                            return "";
                        }
                    }
                    else
                    {
                        *err = 1;
                        return "";
                    }
                }
                else
                {
                    *err = 1;
                    return "";
                }
            }

            uint8_t *key = credentials_.token;
            uint8_t *hmackey = credentials_.hmackey;
            uint8_t data_decoded[strlen(data)];
            uint8_t iv_decoded[strlen(iv)];

            SHA256HMAC hmac(credentials_.hmackey, 32);
            hmac.doUpdate(iv, strlen(iv));
            if (token != "")
            {
                hmac.doUpdate(token.c_str(), token.length());
            }
            if (seqstr != "")
            {
                hmac.doUpdate(seqstr, strlen(seqstr));
            }

            hmac.doUpdate(data, strlen(data));
            uint8_t authCode[SHA256HMAC_SIZE];
            hmac.doFinal(authCode);

            std::string ehm = base64_encode(authCode, SHA256HMAC_SIZE);
            if (ehm != hash)
            {
                ESP_LOGD(TAG, "ehm [%s] does not match hash [%s]", ehm.c_str(), hash.c_str());
                *err = 1;
                return "";
            }
            if (seq > 0 && lastseq != NULL)
                *lastseq = seq;

            int encrypted_length = base64_decode(std::string(data), data_decoded, strlen(data));
            base64_decode(std::string(iv), iv_decoded, strlen(iv));
            AES aes(key, iv_decoded, AES::AES_MODE_256, AES::CIPHER_DECRYPT);
            aes.process((uint8_t *)data_decoded, data_decoded, encrypted_length);
            std::string out = std::string((char *)data_decoded);
            // ESP_LOGD(TAG,"decryption: %s,%s,len=%d\r\nhash=%s",data,iv,strlen(iv),ehm.c_str());
            // return std::string((char*)data_decoded);
            return out;
        }

        static void handle_uploads(struct mg_connection *c, int ev, void *ev_data)
        {
            struct upload_state *us = (struct upload_state *)c->data;
            WebServer *srv = static_cast<WebServer *>(webServerPtr);
            // Catch /update requests early, without buffering whole body
            // When we receive MG_EV_HTTP_HDRS event, that means we've received all
            // HTTP headers but not necessarily full HTTP body
            if (ev == MG_EV_HTTP_HDRS)
            {
                struct mg_http_message *hm = (struct mg_http_message *)ev_data;
                if (mg_match(hm->uri, mg_str("/update/*"), NULL))
                {

                    if (srv->get_credentials()->password != "")
                    {
                        if (!mg_http_check_digest_auth(hm, "webkeypad", srv->get_credentials()))
                        {
                            mg_send_digest_auth_request(c, "webkeypad");
                            c->is_draining = 1;
                            c->recv.len = 0;
                            return;
                        }
                    }

                    char path[100];
                    mg_snprintf(path, sizeof(path), "%.*s", hm->uri.len - 8,
                                hm->uri.buf + 8);
                    MG_INFO(("Performing OTA update..."));
                    us->fn = path;
                    us->expected = hm->body.len;             // Store number of bytes we expect
                    mg_iobuf_del(&c->recv, 0, hm->head.len); // Delete HTTP headers
                    c->pfn = NULL;                           // Silence HTTP protocol handler, we'll use MG_EV_READ
                }
            }
            // Catch uploaded file data for both MG_EV_READ and MG_EV_HTTP_HDRS
            if (us->expected > 0 && c->recv.len > 0)
            {
                // MG_INFO(("Expected bytes: %d, got: %d, received: %d",us->expected,c->recv.len,us->received));
                if ((us->received + c->recv.len) >= us->expected)
                {
                    // Uploaded everything. Send response back
                    MG_INFO(("OTA uploaded %lu bytes from file %s", us->received + c->recv.len, us->fn.c_str()));
                    mg_http_reply(c, 200, NULL, "%lu ok\n", us->received);
                    srv->handleUpload(us->expected, us->fn, us->received, c->recv.buf, c->recv.len, true);
                    memset(us, 0, sizeof(*us)); // Cleanup upload state
                    c->is_draining = 1;         // Close connection when response gets sent
                }
                else
                {
                    srv->handleUpload(us->expected, us->fn, us->received, c->recv.buf, c->recv.len, false);
                    us->received += c->recv.len;
                }

                c->recv.len = 0; // Delete received data
            }
        }

        void WebServer::ev_handler(struct mg_connection *c, int ev, void *ev_data)
        {
            WebServer *srv = static_cast<WebServer *>(webServerPtr);
            handle_uploads(c, ev, ev_data);

            bool final = false;
            if (ev == MG_EV_WRITE)
            {
                if (c->send.len == 0 && c->send.size > 1024)
                {
                    void *p = calloc(1, 1024);
                    if (p != NULL)
                    {
                        size_t *len = (size_t *)ev_data;
                        // keep outbound queue size under 1k to minimize ram use.
                        // ESP_LOGD(TAG,"Send size=%d, len=%d, write size=%d, type=%02x",c->send.size,c->send.len,*len,c->data[0]);
                        memset(c->send.buf, 0, c->send.size);
                        free(c->send.buf);
                        c->send.buf = (unsigned char *)p;
                        c->send.size = 1024;
                    }
                }
            }
            else if (ev == MG_EV_CLOSE)
            {
                ESP_LOGD(TAG, "Connection %d closed", c->id);
                srv->tokens_.erase(c->id);

#if defined(ESP32)
                ESP_LOGD(TAG, "Current Heap values: freeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(8));
#endif
                // srv->sessionTokens.erase(c);
            }
            else if (ev == MG_EV_ACCEPT)
            {
                /*
                const char *cert=srv->get_certificate();
                const char *key=srv->get_certificate_key();
                MG_INFO(("certificate len=%d,%s,%s",strlen(cert),cert,key));

                if (strlen(cert)==0) return;
                struct mg_tls_opts opts = {
                .cert = mg_str(s_tls_cert),
                .key = mg_str(s_tls_key),
                };

                mg_tls_init(c, &opts);
            */
                ESP_LOGD(TAG, "New connection %d accepted", c->id);
#if defined(ESP32)
                ESP_LOGD(TAG, "Current Heap values: freeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(8));
#endif
            }
            else if (ev == MG_EV_WS_MSG)
            {
                // Got websocket frame. Received data is wm->data. Echo it back!
                struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
                DynamicJsonDocument doc(wm->data.len * 1.5);
                JsonObject obj = doc.as<JsonObject>();
                std::string buf = std::string(wm->data.buf, wm->data.len);
                deserializeJson(doc, buf.c_str());
                uint8_t err = 0;

                if (doc.containsKey("iv") && srv->get_credentials()->crypt)
                {

                    buf = srv->decrypt(obj, &err);
                    if (buf == "" || err)
                        err = 1;
                    if (!err)
                        deserializeJson(doc, buf.c_str());
                }
                if (!err)
                {
                    srv->handleRequest(c, obj);
                }
                else
                    mg_http_reply(c, 403, "", "");
            }
            if (ev == MG_EV_HTTP_MSG && c->data[0] != 'U')
            {
                struct mg_http_message *hm = (struct mg_http_message *)ev_data;

                if (srv->get_credentials()->password != "" && !srv->get_credentials()->crypt)
                {
                    if (!mg_http_check_digest_auth(hm, "webkeypad", srv->get_credentials()))
                    {
                        mg_send_digest_auth_request(c, "webkeypad");
                        c->is_draining = 1;
                        c->recv.len = 0;
                        return;
                    }
                }

                if (mg_match(hm->uri, mg_str("/ws"), NULL) && c->data[0] != 'E')
                {

                    // Upgrade to websocket. From now on, a connection is a full-duplex
                    // Websocket connection, which will receive MG_EV_WS_MSG events.
                    mg_ws_upgrade(c, hm, NULL);
                    c->data[0] = 'W';
                    c->send.c = c;
                    std::string enc;
                    bool crypt = srv->get_credentials()->crypt;
                    enc = srv->get_config_json(c->id);
                    if (crypt)
                        enc = srv->encrypt(enc.c_str());
                    mg_ws_printf(c, WEBSOCKET_OP_TEXT, PSTR("{\"%s\":\"%s\",\"%s\":%ul,\"%s\":%s}"), "type", "app_config", "data", enc.c_str());
                    if (strlen(srv->_json_keypad_config) > 0)
                    {
                        enc = srv->_json_keypad_config;
                        if (crypt)
                            enc = srv->encrypt(enc.c_str());
                        mg_ws_printf(c, WEBSOCKET_OP_TEXT, PSTR("{\"%s\":\"%s\",\"%s\":%s}"), "type", "key_config", "data", enc.c_str());
                    }
                    for (auto &group : srv->sorting_groups_)
                    {
                        enc = json::build_json([group](JsonObject root)
                                               {
                   root["name"] = group.second.name;
                   root["sorting_weight"] = group.second.weight; });
                        if (crypt)
                            enc = srv->encrypt(enc.c_str());
                        mg_ws_printf(c, WEBSOCKET_OP_TEXT, PSTR("{\"%s\":\"%s\",\"%s\":%s}"), "type", "sorting_group", "data", enc.c_str());
                    }

                    srv->entities_iterator_.begin(srv->include_internal_);
                }
                else if (mg_match(hm->uri, mg_str("/events"), NULL) && !c->is_websocket)
                {
                    mg_str *hdr = mg_http_get_header(hm, "Accept");
                    // if (hdr != NULL && mg_strstr(*hdr, mg_str("text/event-stream")) != NULL)  {
                    c->data[0] = 'E';
                    mg_printf(c, PSTR("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\n"));
                    c->send.c = c;
                    std::string enc;
                    bool crypt = srv->get_credentials()->crypt;
                    enc = srv->get_config_json(c->id);
                    if (crypt)
                        enc = srv->encrypt(enc.c_str());
                    mg_printf(c, PSTR("id: %d\r\nretry: %d\r\nevent: %s\r\ndata: %s\r\n\r\n"), millis(), 30000, "ping", enc.c_str());

                    for (auto &group : srv->sorting_groups_)
                    {
                        enc = json::build_json([group](JsonObject root)
                                               {
                     root["name"] = group.second.name;
                     root["sorting_weight"] = group.second.weight; });
                        if (crypt)
                            enc = srv->encrypt(enc.c_str());
                        mg_printf(c, PSTR("event: %s\r\ndata: %s\r\n\r\n"), "sorting_group", enc.c_str());
                    }

                    if (strlen(srv->_json_keypad_config) > 0)
                    {
                        enc = srv->_json_keypad_config;
                        if (crypt)
                            enc = srv->encrypt(enc.c_str());
                        mg_printf(c, PSTR("event: %s\r\ndata: %s\r\n\r\n"), "key_config", enc.c_str());
                    }
                    srv->entities_iterator_.begin(srv->include_internal_);
                    // } else
                    //   mg_http_reply(c, 404,"", "");
                }
                else
                {
                    if (!mg_match(hm->uri, mg_str("/update/*"), NULL))
                    {
                        c->send.c = c;
                        srv->handleWebRequest(c, hm);
                    }
                }
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
                c->is_draining = 1;
                return;
            }

#ifdef USE_WEBKEYPAD_CSS_INCLUDE
            if (mg_match(hm->uri, mg_str("/0.css"), NULL))
            {
                this->handle_css_request(c);
                c->is_draining = 1;
                return;
            }
#endif

#ifdef USE_WEBKEYPAD_JS_INCLUDE
            if (mg_match(hm->uri, mg_str("/0.js"), NULL))
            {
                this->handle_js_request(c);
                c->is_draining = 1; // uses about 22k or more of ram and doesnt free it so close it when done sending
                return;
            }
#endif

#ifdef USE_WEBKEYPAD_PRIVATE_NETWORK_ACCESS
            // if (request->method() == HTTP_OPTIONS && request->hasHeader(HEADER_CORS_REQ_PNA)) {
            //  this->handle_pna_cors_request(c);
            //  return;
            //}

            mg_str *hdr = mg_http_get_header(hm, HEADER_CORS_REQ_PNA);
            if (mg_vcasecmp(&hm->method, "OPTIONS") == 0 && hdr != NULL)
            {
                this->handle_pna_cors_request(c);
                c->is_draining = 1;
                return;
            }

#endif
            DynamicJsonDocument doc(hm->message.len * 1.5);
            JsonObject obj = doc.to<JsonObject>();
            if (mg_match(hm->uri, mg_str("/api"), NULL))
            {

                std::string buf = std::string(hm->body.buf, hm->body.len);
                DeserializationError err = deserializeJson(doc, buf.c_str());
                if (!err && doc.containsKey("iv") && credentials_.password != "")
                {
                    uint8_t e = 0;
                    buf = decrypt(obj, &e);
                    if (buf != "")
                        deserializeJson(doc, buf.c_str());
                    else
                        mg_http_reply(c, 403, "", "");
                }
            }

            if (!doc.containsKey("domain"))
            {
                parseUrl(hm, obj);
            }

            handleRequest(c, obj);
            if (c->send.size > 1500 || c->recv.size > 1500)
                c->is_draining = 1; // if send or recv queue getting too large close the connection to free up ram
        }

        void WebServer::handleRequest(mg_connection *c, JsonObject doc)
        {
            std::string d = doc["domain"];
#ifdef USE_SENSOR

            if (doc["domain"] == "sensor")
            {
                this->handle_sensor_request(c, doc);
                return;
            }
#endif

#ifdef USE_SWITCH
            if (doc["domain"] == "switch")
            {
                this->handle_switch_request(c, doc);
                return;
            }
#endif

#ifdef USE_BUTTON
            if (doc["domain"] == "button")
            {
                this->handle_button_request(c, doc);
                return;
            }
#endif

#ifdef USE_BINARY_SENSOR
            if (doc["domain"] == "binary_sensor")
            {
                this->handle_binary_sensor_request(c, doc);
                return;
            }
#endif
#ifdef USE_FAN
            if (doc["domain"] == "fan")
            {
                this->handle_fan_request(c, doc);
                return;
            }
#endif

#ifdef USE_LIGHT
            if (doc["domain"] == "light")
            {
                this->handle_light_request(c, doc);
                return;
            }
#endif

#ifdef USE_TEXT_SENSOR
            if (doc["domain"] == "text_sensor")
            {
                this->handle_text_sensor_request(c, doc);
                return;
            }
#endif

#ifdef USE_COVER
            if (doc["domain"] == "cover")
            {
                this->handle_cover_request(c, doc);
                return;
            }
#endif

#ifdef USE_NUMBER
            if (doc["domain"] == "number")
            {
                this->handle_number_request(c, doc);
                return;
            }
#endif

#ifdef USE_DATETIME_DATE
            if (doc["domain"] == "date")
            {
                this->handle_date_request(c, doc);
                return;
            }
#endif

#ifdef USE_DATETIME_TIME
            if (doc["domain"] == "time")
            {
                this->handle_time_request(c, doc);
                return;
            }
#endif

#ifdef USE_DATETIME_DATETIME
            if (doc["domain"] == "datetime")
            {
                this->handle_datetime_request(c, doc);
                return;
            }
#endif

#ifdef USE_VALVE
            if (doc["domain"] == "valve")
            {
                this->handle_valve_request(c, doc);
                return;
            }
#endif

#ifdef USE_UPDATE
            if (doc["domain"] == "update")
            {
                this->handle_update_request(c, doc);
                return;
            }
#endif

#ifdef USE_EVENT
            if (doc["domain"] == "event")
            {
                this->handle_event_request(c, doc);
                return;
            }
#endif

#ifdef USE_TEXT
            if (doc["domain"] == "text")
            {
                this->handle_text_request(c, doc);
                return;
            }
#endif

#ifdef USE_SELECT
            if (doc["domain"] == "select")
            {
                this->handle_select_request(c, doc);
                return;
            }
#endif

#ifdef USE_CLIMATE
            if (doc["domain"] == "climate")
            {
                this->handle_climate_request(c, doc);
                return;
            }
#endif

#ifdef USE_LOCK
            if (doc["domain"] == "lock")
            {
                this->handle_lock_request(c, doc);
                return;
            }
#endif

#if defined(USE_DSC_PANEL) || defined(USE_VISTA_PANEL)
            if (doc["domain"] == "auth")
            {
                this->handle_auth_request(c, doc);
                return;
            }
            if (doc["domain"] == "alarm_panel")
            {
                this->handle_alarm_panel_request(c, doc);
                return;
            }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
            if (doc["domain"] == "alarm_control_panel")
            {
                this->handle_alarm_control_panel_request(c, doc);
                return;
            }
#endif

            // mg_http_reply(c,404,"","");
            ws_reply(c, "", false);
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

        void WebServer::report_ota_error()
        {
#ifdef USE_ARDUINO
            StreamString ss;
            Update.printError(ss);
            char buf[100];
            ESP_LOGW(TAG, "OTA Update failed! Error: %s", ss.c_str());
            snprintf(buf, 100, "OTA Update failed! Error: %s", ss.c_str());
            std::string ebuf = escape_json(buf);
            this->push(OTA, ebuf.c_str());
#endif
        }
#if defined(ESP32)
        bool WebServer::handleUpload(size_t bodylen, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            char buf[100];
#ifdef USE_ARDUINO
            bool success;
            if (index == 0)
            {
                snprintf(buf, 100, "OTA Update Start: %s", filename.c_str());
                this->push(OTA, buf);
                ESP_LOGI(TAG, "OTA Update Start: %s", filename.c_str());

                this->ota_read_length_ = 0;

                if (Update.isRunning())
                {
                    Update.abort();
                    return false;
                }
#if defined(USE_DSC_PANEL) || defined(USE_VISTA_PANEL)
                if (alarm_panel::alarmPanelPtr != NULL)
                {
                    alarm_panel::alarmPanelPtr->stop();
                }
#endif
                success = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);

                if (!success)
                {
                    report_ota_error();
                    return false;
                }
            }
            else if (Update.hasError())
            {
                this->push(OTA, "OTA Update has error");
                // don't spam logs with errors if something failed at start
                return false;
            }

            success = Update.write(data, len) == len;

            if (!success)
            {
                report_ota_error();
                return false;
            }
            this->ota_read_length_ += len;

            const uint32_t now = millis();
            if (now - this->last_ota_progress_ > 1000)
            {
                if (bodylen != 0)
                {
                    float percentage = (this->ota_read_length_ * 100.0f) / bodylen;
                    ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
                    snprintf(buf, 100, "OTA in progress: %0.1f%%", percentage);
                    this->push(OTA, buf);
                }
                else
                {
                    ESP_LOGD(TAG, "OTA in progress: %u bytes read", this->ota_read_length_);
                    snprintf(buf, 100, "OTA in progress: %u bytes read", this->ota_read_length_);
                    this->push(OTA, buf);
                }

                this->last_ota_progress_ = now;
            }

            if (final)
            {
                if (Update.end(true))
                {
                    ESP_LOGI(TAG, "OTA update successful!");
                    this->push(OTA, "OTA Update successful. Press F5 to reload this page.");
                    this->set_timeout(2000, []()
                                      { App.safe_reboot(); });
                    return true;
                }
                else
                {
                    report_ota_error();
                    return false;
                }
            }
#endif
            return true;
        }
#endif
    } // namespace web_server
} // namespace esphome
