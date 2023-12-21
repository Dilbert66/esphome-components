#if defined(USE_ESP32) && defined(USE_MG)
#include <string>
#include "mg_mqtt.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/network/util.h"


namespace esphome {
namespace mqtt {
static const char *const TAG = "mqtt.mg";
MQTTBackendESP32::MQTTBackendESP32() {
    mg_backend=this;
}

/*
struct mg_mqtt_opts {
  struct mg_str user;               // Username, can be empty
  struct mg_str pass;               // Password, can be empty
  struct mg_str client_id;          // Client ID
  struct mg_str topic;              // message/subscription topic
  struct mg_str message;            // message content
  uint8_t qos;                      // message quality of service
  uint8_t version;                  // Can be 4 (3.1.1), or 5. If 0, assume 4
  uint16_t keepalive;               // Keep-alive timer in seconds
  bool retain;                      // Retain flag
  bool clean;                       // Clean session flag
  struct mg_mqtt_prop *props;       // MQTT5 props array
  size_t num_props;                 // number of props
  struct mg_mqtt_prop *will_props;  // Valid only for CONNECT packet (MQTT5)
  size_t num_will_props;            // Number of will props
};
*/


bool MQTTBackendESP32::initialize_() {
    #if defined(MG_USE_SSL)
    char buf[100];
    snprintf(buf,100,"mqtts://%s:%d",this->host_.c_str(),this->port_);
    s_url=buf;
    #else
    char buf[100];
    snprintf(buf,100,"mqtt://%s:%d",this->host_.c_str(),this->port_);
    s_url=buf;    
    #endif   
    MG_INFO(("mqtt url=%s",s_url.c_str()));
    mg_mgr_init(&mgr);
    
    /*
       int ASYNC_CORE=1;
    xTaskCreatePinnedToCore(
    this -> webPollTask, //Function to implement the task
    "webPollTask", //Name of the task
    40000, //Stack size in words
    (void * ) this, //Task input parameter
    10, //Priority of the task
    NULL, //Task handle.
    ASYNC_CORE //Core where the task should run
  );  
   */
    
    
    is_initalized_ = true;
    return true;

}


 
/*

void MQTTBackendESP32::webPollTask(void * args) {

  //WebServer * _this = (WebServer * ) args;
  static unsigned long checkTime = millis();  
  for (;;) { 
     if (network::is_connected())  
            mg_mgr_poll(&mg_backend->mgr, 1000) ;

     
        if (millis() - checkTime > 30000) {
            UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            printf("\nTaskupdates free memory: %5d\n", (uint16_t) uxHighWaterMark);
            checkTime=millis();
        }
        delay(10);
  }
  vTaskDelete(NULL);
}
*/

void MQTTBackendESP32::loop() {
   if (network::is_connected() && this->is_initalized_ ) {
    mg_mgr_poll(&mgr,0);
   }

}

void MQTTBackendESP32::instance_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_OPEN) {
    MG_INFO(("mqtt %lu CREATED", c->id));
    // c->is_hexdumping = 1;
  } else if (ev == MG_EV_ERROR) {
    // On error, log error message
    MG_ERROR(("mqtt %lu ERROR %s", c->id, (char *) ev_data));
    //this->is_connected_=false;
    //this->s_conn = NULL;
  } else if (ev == MG_EV_CONNECT) {
      ESP_LOGV(TAG, "MQTT_EVENT_BEFORE_CONNECT");
    // If target URL is SSL/TLS, command client connection to use TLS
    MG_INFO(("mqtt url is %s",s_url.c_str()));
    
    #if defined(MG_USE_SSL) 
MG_INFO(("mqtt use ssl is on"))   ; 
    struct mg_str host = mg_url_host(s_url.c_str());
    if (mg_url_is_ssl(s_url.c_str())) {
      struct mg_tls_opts opts;
      memset(&opts, 0, sizeof(opts));  
      if(ca_certificate_.length() > 0 && !skip_cert_cn_check_)      
        opts.ca = mg_str(ca_certificate_.c_str());
      opts.name=host;
      mg_tls_init(c, &opts);
    }
    #endif
    MG_INFO((" mqtt connecting"));
  } else if (ev == MG_EV_MQTT_OPEN) {
      this->is_connected_ = true;
      on_connect_.call(true);    
      ESP_LOGV(TAG, "MQTT_EVENT_CONNECTED");
      MG_INFO(("mqtt connected"));

  } else if (ev == MG_EV_MQTT_MSG) {
    //call subs
    // When we get echo response, print it
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    MG_INFO(("mqtt %lu RECEIVED %.*s <- %.*s", c->id, (int) mm->data.len,
             mm->data.ptr, (int) mm->topic.len, mm->topic.ptr));
             
     static std::string topic=std::string(mm->topic.ptr,mm->topic.len);
      ESP_LOGV(TAG, "MQTT_EVENT_DATA %s", topic.c_str());
      this->on_message_.call(mm->topic.len > 0 ? topic.c_str() : nullptr, mm->data.ptr, mm->data.len,0, mm->data.len);          
             
  } else if (ev == MG_EV_CLOSE) {
    //calls subs
     this->is_connected_ = false;
     this->s_conn = NULL;  // Mark that we're closed     
     on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
     MG_INFO(("mqtt %lu CLOSED", c->id));

  } else if (ev == MG_EV_MQTT_CMD) {
      
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    if (mm->cmd == MQTT_CMD_SUBACK) {
      ESP_LOGV(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", mm->id);
      // hardcode QoS to 0. QoS is not used in this context but required to mirror the AsyncMqtt interface
      on_subscribe_.call((int) mm->id, 0);
    } else if (mm->cmd == MQTT_CMD_PUBACK) {
      ESP_LOGV(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", mm->id);
      on_publish_.call((int) mm->id);

    } else if (mm->cmd == MQTT_CMD_UNSUBACK) {
      ESP_LOGV(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", mm->id);
      // hardcode QoS to 0. QoS is not used in this context but required to mirror the AsyncMqtt interface
     // on_unsubscribe_.call(int(mm->id), 0);
    }        
      
  }
  (void) fn_data;
}

void MQTTBackendESP32::mqtt_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    ///MQTTBackendESP32 *instance = static_cast<MQTTBackendESP32 *>(fn_data);   
//MQTTBackendESP32 *instance =mg_backend; 
   // if (instance)
        mg_backend->instance_fn(c, ev,ev_data, fn_data);
    
}



MQTTBackendESP32* mg_backend =nullptr;


/*
struct mg_mqtt_message {
  struct mg_str topic;  // Parsed topic for PUBLISH
  struct mg_str data;   // Parsed message for PUBLISH
  struct mg_str dgram;  // Whole MQTT packet, including headers
  uint16_t id;          // For PUBACK, PUBREC, PUBREL, PUBCOMP, SUBACK, PUBLISH
  uint8_t cmd;          // MQTT command, one of MQTT_CMD_*
  uint8_t qos;          // Quality of service
  uint8_t ack;          // CONNACK return code, 0 = success
  size_t props_start;   // Offset to the start of the properties (MQTT5)
  size_t props_size;    // Length of the properties
};
*/
/*
void mg_mqtt_unsub(struct mg_connection *c, const struct mg_mqtt_opts *opts) {
  uint8_t qos_ = opts->qos & 3;
  size_t plen = c->is_mqtt5 ? get_props_size(opts->props, opts->num_props) : 0;
  size_t len = 2 + opts->topic.len + 2 + 1 + plen;
  mg_mqtt_send_header(c, MQTT_CMD_UNSUBSCRIBE, 2, (uint32_t) len);
  if (++c->mgr->mqtt_id == 0) ++c->mgr->mqtt_id;
  mg_send_u16(c, mg_htons(c->mgr->mqtt_id));
  if (c->is_mqtt5) mg_send_mqtt_properties(c, opts->props, opts->num_props);

  mg_send_u16(c, mg_htons((uint16_t) opts->topic.len));
  mg_send(c, opts->topic.ptr, opts->topic.len);
}
*/
}  // namespace mqtt
}  // namespace esphome
#endif  // USE_ESP32
