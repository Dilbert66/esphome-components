#pragma once

#if defined(USE_ESP32) && defined(USE_MG)
#include <string>
#include <queue>
#include <mqtt_client.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"
#include "mqtt_backend.h"
#include "mongoose.h"


namespace esphome {
namespace mqtt {

class MQTTBackendESP32 final : public MQTTBackend {
 public:
  MQTTBackendESP32(); 
  static const size_t MQTT_BUFFER_SIZE = 4096;
  struct mg_connection * s_conn;              // Client connection
  struct mg_mgr mgr; 
  
  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_ = keep_alive; }
  void set_client_id(const char *client_id) final { this->client_id_ = client_id; }
  void set_clean_session(bool clean_session) final { this->clean_session_ = clean_session; }

  void set_credentials(const char *username, const char *password) final {
    if (username)
      this->username_ = username;
    if (password)
      this->password_ = password;
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final {
    if (topic)
      this->lwt_topic_ = topic;
    this->lwt_qos_ = qos;
    if (payload)
      this->lwt_message_ = payload;
    this->lwt_retain_ = retain;
  }
  void set_server(network::IPAddress ip, uint16_t port) final {
    this->host_ = ip.str();
    this->port_ = port;
  
  }
  void set_server(const char *host, uint16_t port) final {
    this->host_ = host;
    this->port_ = port;    
   
  }
  void set_on_connect(std::function<on_connect_callback_t> &&callback) final {
    this->on_connect_.add(std::move(callback));
  }
  void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) final {
    this->on_disconnect_.add(std::move(callback));
  }
  void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) final {
    this->on_subscribe_.add(std::move(callback));
  }
  void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) final {
    this->on_unsubscribe_.add(std::move(callback));
  }
  void set_on_message(std::function<on_message_callback_t> &&callback) final {
    this->on_message_.add(std::move(callback));
  }
  void set_on_publish(std::function<on_publish_user_callback_t> &&callback) final {
    this->on_publish_.add(std::move(callback));
  }
  bool connected() const final { return this->is_connected_; }

  void connect() final {
      MG_INFO(("mqtt in connected before initialized check"));
    if (!this->is_initalized_) {
        MG_INFO(("mqtt in connect, initializing"));        
        this->initialize_();
    }

      if (this->is_initalized_) 
      {
   struct mg_mqtt_opts opts;
   memset(&opts, 0, sizeof(opts));  
   opts.clean = this->clean_session_;
   opts.user = mg_str(this->username_.c_str());
   opts.pass = mg_str(this->password_.c_str());
   opts.version = 4;
   opts.message = mg_str("bye");
 // std::string s_url="mqtts://192.168.2.175:8883";  
MG_INFO((" mqtt in connect url=%s",s_url.c_str())); 
  if (!this->is_connected_)  {
      this->s_conn = mg_mqtt_connect(&mgr,this->s_url.c_str() , &opts, this->mqtt_fn,this);
        MG_INFO(("connected to mqtt and initialized"));      
  }
      
    }
  }
  
  
  void disconnect() final {
      
          MG_INFO(("mqtt disconnect")); 
    if ( this->s_conn != NULL && this->is_connected_) {
       struct mg_mqtt_opts opts = {
                               .version = 4
       };

       mg_mqtt_disconnect(this->s_conn, &opts);
    } 
    this->s_conn=NULL;
    this->is_connected_=false;
  }

  bool subscribe(const char *topic, uint8_t qos) final {
   if (this->s_conn == NULL ) return false;
    struct mg_str subt = mg_str(topic);
    struct mg_mqtt_opts sub_opts;
    memset(&sub_opts, 0, sizeof(sub_opts));
    sub_opts.topic = subt;
    sub_opts.qos = qos;
    mg_mqtt_sub(this->s_conn, &sub_opts); 
    return true;    
  }
  
  bool unsubscribe(const char *topic) final {
   if (this->s_conn == NULL ) return false;
    struct mg_str subt = mg_str(topic);
    struct mg_mqtt_opts sub_opts;
    memset(&sub_opts, 0, sizeof(sub_opts));
    sub_opts.topic = subt;
    mg_mqtt_unsub(this->s_conn, &sub_opts); 
    return true;  
  }

  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final {
    struct mg_mqtt_opts pub_opts;
    memset(&pub_opts, 0, sizeof(pub_opts));
    pub_opts.topic = mg_str(topic);
    pub_opts.message = mg_str_n(payload,length);
    pub_opts.version=4;
    pub_opts.qos = qos, pub_opts.retain = false;
    mg_mqtt_pub(this->s_conn, &pub_opts);
    //MG_INFO(("%lu PUBLISHED %.*s -> %.*s", this->s_conn->id, length, payload,             strlen(topic), topic));
    return true;
  }
  using MQTTBackend::publish;

  void loop() final;
  void set_ca_certificate(const std::string &cert) { ca_certificate_ = cert; }
  void set_skip_cert_cn_check(bool skip_check) { skip_cert_cn_check_ = skip_check; }
  void instance_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data); 
 protected:
 
  bool initialize_();
  bool is_connected_{false};
  bool is_initalized_{false};

  static void mqtt_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
  static void webPollTask(void * args);
  
  std::string s_url;
  std::string host_;
  uint16_t port_;
  std::string username_;
  std::string password_;
  std::string lwt_topic_;
  std::string lwt_message_;
  uint8_t lwt_qos_;
  bool lwt_retain_;
  std::string client_id_;
  uint16_t keep_alive_;
  bool clean_session_;
  std::string ca_certificate_;
  bool skip_cert_cn_check_{false};

  // callbacks
  CallbackManager<on_connect_callback_t> on_connect_;
  CallbackManager<on_disconnect_callback_t> on_disconnect_;
  CallbackManager<on_subscribe_callback_t> on_subscribe_;
  CallbackManager<on_unsubscribe_callback_t> on_unsubscribe_;
  CallbackManager<on_message_callback_t> on_message_;
  CallbackManager<on_publish_user_callback_t> on_publish_;
 // std::queue<Event> mqtt_events_;
};
extern MQTTBackendESP32 * mg_backend;
}  // namespace mqtt
}  // namespace esphome

#endif
