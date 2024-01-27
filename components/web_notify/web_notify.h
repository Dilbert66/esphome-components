#pragma once
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "mongoose.h"
#include "esphome/components/json/json_util.h"
#include <vector>
#include <algorithm>
#include <queue>
#ifdef USE_ESP32
#include <deque>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace esphome {
namespace web_notify {



  
struct rx_message_t {
  std::string text;
  std::string chat_id;
  std::string sender;
  std::string date;
  std::string cmd;
  std::string args;
};



class WebNotify : public Controller, public Component {
 public:
  WebNotify();



  // (In most use cases you won't need these)
  /// Setup the internal web server and register handlers.
  void setup() override;
  void loop() override;

  void dump_config() override;
  void publish(std::string to,std::string message);

  void set_bot_id(std::string bot_id) {botId_=bot_id;
  ESP_LOGD("test","telegram: bot id set to %s,%d,%d",botId_.c_str(),botId_.length(),sending);
  }
  void set_chat_id(std::string chat_id) { telegramUserId_=chat_id;}
  void add_chatid(std::string chat_id) {allowed_chat_ids.push_back(chat_id);}
  void set_api_host(std::string api_host) { apiHost_="https://" + api_host;
  ESP_LOGD("test","telegram: api host set to %s",apiHost_.c_str());
  }
  void set_bot_enable(bool enable) {enableBot_=enable; }
  void set_send_enable(bool enable) {enableSend_=enable;}
  
  bool get_bot_status() { return enableBot_;} 
  bool get_send_status() { return enableSend_;}  
  using on_message_callback_t = void( rx_message_t *m);
  
  CallbackManager<on_message_callback_t> on_message_;
   
  void set_on_message(std::function<on_message_callback_t> &&callback) {
    this->on_message_.add(std::move(callback));
  }
  

  
private:
  struct mg_mgr mgr;
  static void  notify_fn(struct mg_connection *c, int ev, void *ev_data); 


  std::string apiHost_ = "https://api.telegram.org/";
  const uint64_t timeout_ms = 1500;  // Connect timeout in milliseconds
  int lastMsgReceived = 0;
  std::string botId_="";
  std::string telegramUserId_="";
  uint8_t inMsgIdx, outMsgIdx;
  const uint8_t msgQueueSize = 10;  
  bool enableBot_=false;
  bool enableSend_=true;
  bool sending=false;
  bool connected=false;
  unsigned long retryDelay=0;
  int delayTime=15000; //ms
  std::queue<std::string> messages;
  std::vector<std::string> allowed_chat_ids;
  bool isAllowed(std::string chat_id);
  void parseArgs(rx_message_t *m);  
  


  bool processMessage(const char * payload);

};

extern WebNotify * global_notify;

template<typename... Ts> class TelegramPublishAction : public Action<Ts...> {
 public:
  TelegramPublishAction(WebNotify *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, to)
  TEMPLATABLE_VALUE(std::string, message)

  void play(Ts... x) override {
    this->parent_->publish(this->to_.value(x...), this->message_.value(x...));
  }

 protected:
  WebNotify *parent_;
};

class TelegramMessageTrigger : public Trigger<std::string,std::string,std::string> {
 public:
  explicit TelegramMessageTrigger(const std::string &cmd) {
      
         global_notify->set_on_message([cmd,this](rx_message_t *m) {
        ESP_LOGD("test","got trigger sending back payload %s",m->text.c_str());
       if (cmd.compare("")==0) 
           this->trigger(m->cmd,m->args,m->text);
       else if (cmd.compare(m->cmd)==0)       
           this->trigger(m->cmd,m->args,m->text);
       
    }); 
      
      
  };


};

}  // namespace web_server
}  // namespace esphome
