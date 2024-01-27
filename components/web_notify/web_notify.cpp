#include "web_notify.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "ArduinoJson.h"
#include <cstdlib>


#ifdef USE_ARDUINO
#include <StreamString.h>
#endif

namespace esphome {
namespace web_notify {



WebNotify::WebNotify()
      {
 global_notify=this;
}

void WebNotify::publish(std::string to,std::string message) {
  std::string outmsg;
  if (message.length() > 200) {
          ESP_LOGE("webnotify","Message %s to %s too long",message.c_str(),to.c_str());
      
  }
  if (!enableSend_) {
      ESP_LOGE("webnotify","Message sending is not enabled");
      return;
  }
  StaticJsonDocument < 300 > doc;
  doc["chat_id"] = to != "" ?  to : telegramUserId_;
  doc["text"] =message;
  serializeJson(doc, outmsg); 
  messages.push(outmsg);  
    ESP_LOGD("webnotify","sent telegram message %s to %s",message.c_str(),to.c_str());
}

bool WebNotify::isAllowed(std::string chat_id) {
       if ( std::find(allowed_chat_ids.begin(), allowed_chat_ids.end(), chat_id) != allowed_chat_ids.end() ) 
           return true;
       else
           return false;
}

void WebNotify::parseArgs(rx_message_t *m) {
    if (m->text[0]!='/') return;
    std::string s = m->text;
    m->cmd = s.substr(0, s.find(" "));
    m->args= s.erase(0,s.find(" ") + 1); 
    if (m->cmd == m->args) m->args=""; //if no args set to empty
    ESP_LOGD("parse","telegram cmd=%s, args=%s",m->cmd.c_str(),m->args.c_str());    
}

bool WebNotify::processMessage(const char *payload) {
   bool success=false;
  
   json::parse_json(payload,  [&success](JsonObject root) {
     
            rx_message_t m;  
            if (root["result"][0]["callback_query"]["id"]) {

              int update_id = root["result"][0]["update_id"];
        
              update_id = update_id + 1;
              //we ignore the first message on initial start to avoid a reboot loop
              if (global_notify->lastMsgReceived == 0)  global_notify->lastMsgReceived = update_id;
              
              if (global_notify->lastMsgReceived != update_id) {
                std::string sender = root["result"][0]["callback_query"]["message"]["from"]["username"];
                std::string text = root["result"][0]["callback_query"]["data"];
                std::string chat_id = root["result"][0]["callback_query"]["message"]["chat"]["id"];
                std::string date = root["result"][0]["callback_query"]["message"]["date"];
                m.sender = sender;
                m.text = text;
                m.chat_id = chat_id;
                m.date = date;
                global_notify->lastMsgReceived = update_id;
              }
            } else if (root["result"][0]["channel_post"]["id"]) { 
              int update_id = root["result"][0]["update_id"];
              update_id = update_id + 1;
              //we ignore the first message on initial start to avoid a reboot loop
              if (global_notify->lastMsgReceived == 0) global_notify->lastMsgReceived = update_id;
              
              if (global_notify->lastMsgReceived != update_id) {
                std::string sender = root["result"][0]["channel_post"]["message"]["from"]["username"];
                std::string text = root["result"][0]["channel_post"]["message"]["text"];
                std::string chat_id = root["result"][0]["channel_post"]["message"]["chat"]["id"];
                std::string date = root["result"][0]["channel_post"]["message"]["date"];

                m.sender = sender;
                m.text = text;
                m.chat_id = chat_id;
                m.date = date;
                global_notify->lastMsgReceived = update_id;
            
                
              }
            } else if (root["result"][0]["message"]["text"]) {
              int update_id = root["result"][0]["update_id"];
              update_id = update_id + 1;
                ESP_LOGD("test","update=%d",update_id);                
              //we ignore the first message on initial start to avoid a reboot loop
              if (global_notify->lastMsgReceived == 0) global_notify->lastMsgReceived = update_id;
              
              if (global_notify->lastMsgReceived != update_id) {
               std::string sender = root["result"][0]["message"]["from"]["username"];
                std::string text = root["result"][0]["message"]["text"];
                std::string chat_id = root["result"][0]["message"]["chat"]["id"];
                std::string date = root["result"][0]["message"]["date"];

                m.sender = sender;
                m.text = text;
                m.chat_id = chat_id;
                m.date = date;
                global_notify->lastMsgReceived = update_id;
                if (!global_notify->isAllowed(m.chat_id)) {
                    MG_INFO(("Chat id %s not allowed to send to bot",m.chat_id.c_str()));
                    success=true; //message was still parsed ok so we set success
                    return ;
                    
                }      
                //test - echo message back
                std::string msg="{\"chat_id\":"+ std::string(global_notify->telegramUserId_.c_str()) +",\"text\":\"" + text + "\"}"; 
                global_notify->messages.push(msg); 
                
                global_notify->parseArgs(&m);
                global_notify->on_message_.call(&m);  
              }
            } else if (root["result"]["message_id"] ) {
                
                bool ok=root["ok"];
                std::string id=root["result"]["message_id"];
                ESP_LOGD("test","got message response2 %s,%d",id.c_str(),ok);
                if (ok) {
                    //pop last sent message from queue as it was successful
                      global_notify->messages.pop();              
                } else
                          global_notify->retryDelay=millis();
                
            } else if (global_notify->sending) {
                    //message response. Pop the message anyhow so we don't loop.
                      global_notify->messages.pop();
            }
            success=true;
   });
   return success;
}

void  WebNotify::notify_fn(struct mg_connection *c, int ev, void *ev_data) {

  if (global_notify == NULL) {
      ESP_LOGD("test","telegram: global pointer is null");
      return;
  }
  
  if (ev == MG_EV_OPEN) {
      global_notify->connected=true;
    // Connection created. Store connect expiration time in c->data
    *(uint64_t *) &c->data[2] = mg_millis() + global_notify->timeout_ms;
  } else if (ev == MG_EV_POLL) {
    if (mg_millis() > *(uint64_t *) &c->data[2] &&
        (c->is_connecting || c->is_resolving)) {
      mg_error(c, "Connect timeout");
    }
     if (c->data[0]=='T') {
        //if (global_notify->msgAvailable() && !c->is_draining && !global_notify->sending ) {
        if (global_notify->messages.size() && !c->is_draining && !global_notify->sending ) {            
             c->is_draining=1; //close long poll so we can send
             ESP_LOGD("test","Pending message closing poll connection from poll");
        }
       }  
  } else if (ev== MG_EV_CLOSE) {
        global_notify->sending=false;
        global_notify->connected=false;
      MG_INFO(("Got ev close,enablebot %d, ma %d, sending=%d",global_notify->enableBot_, global_notify->messages.size(),global_notify->sending));   
      
  } else if (ev == MG_EV_CONNECT) {
       global_notify->connected=true;
      if (c->data[0]=='T') {
        if (global_notify->messages.size() && !c->is_draining && !global_notify->sending) {
             c->is_draining=1; //close long poll so we can send
             ESP_LOGD("test","Pending message closing poll connection from connect");
        }
        return;
       }
      c->data[0]='T'; 
     
      MG_INFO(("got ev connect"));
    // Connected to server. Extract host name from URL
    struct mg_str host = mg_url_host(global_notify->apiHost_.c_str());
    if (mg_url_is_ssl(global_notify->apiHost_.c_str())) {
    
      struct mg_tls_opts opts = {.name = host};
      mg_tls_init(c, &opts);
    }
      MG_ERROR(("\nfreeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(),esp_get_minimum_free_heap_size(),heap_caps_get_largest_free_block(8)));   
   MG_INFO((" tls init done"));
      if (global_notify->messages.size()) {
        global_notify->sending=true;       //we are connecting so flag connection as open           
        //String msg = global_notify->getNextMsg();
        //String msg = global_notify->peekNextMsg();//only peek it, we will pop from queue if successful later
        std::string msg = global_notify->messages.front();
        mg_printf(c,"POST /bot%s",global_notify->botId_.c_str());
        mg_printf(c,"/sendMessage HTTP/1.1\r\n");
        mg_printf(c,"Host: %.*s\r\n",host.len,host.ptr);
        mg_printf(c,"Content-Type: application/json\r\n");
        mg_printf(c,"Content-Length: %d\r\n\r\n%s\r\n",msg.length(),msg.c_str());
        ESP_LOGD("test","sent telegram msg %s",msg.c_str());
      } else if ( global_notify->enableBot_) {
        global_notify->sending=false;       
        mg_printf(c,"GET /bot%s",global_notify->botId_.c_str());
        mg_printf(c,"/getUpdates?limit=1&timeout=120&offset=%d HTTP/1.1\r\n",global_notify->lastMsgReceived);
        mg_printf(c,"Host: %.*s\r\n",host.len,host.ptr);
        mg_printf(c,"Accept: application/json\r\n");
        mg_printf(c,"Cache-Control: no-cache\r\n\r\n");    
         MG_INFO(("sent post data"));
      }
   
  } else if (ev == MG_EV_HTTP_MSG) {
  
    // Response is received. Print it
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    printf(" response message from telegram: %.*s", (int) hm->message.len, hm->message.ptr);
            
         String payload= String(hm->body.ptr,hm->body.len);    

          if (!global_notify->processMessage(payload.c_str())) {
            printf_P("%s",F("\nMessage parsing failed, skipped.\n"));
            int update_id_first_digit = 0;
            int update_id_last_digit = 0;
            for (int a = 0; a < 3; a++) {
              update_id_first_digit = payload.indexOf(':', update_id_first_digit + 1);
            }
            for (int a = 0; a < 2; a++) {
              update_id_last_digit = payload.indexOf(',', update_id_last_digit + 1);
            }
           global_notify->lastMsgReceived = payload.substring(update_id_first_digit + 1, update_id_last_digit).toInt() + 1;

          }
    global_notify->sending=false;
    c->is_closing = 1;        // Tell mongoose to close this connection
  
  } else if (ev == MG_EV_ERROR) {
       global_notify->retryDelay=millis();
      
  }
}



void WebNotify::setup() {
  ESP_LOGCONFIG("notify", "Setting up web client...");
   if (telegramUserId_ !="")
       allowed_chat_ids.push_back(telegramUserId_);
   mg_mgr_init(&mgr); 
  std::string msg="{\"chat_id\":"+std::string(telegramUserId_.c_str())+",\"text\":\"test message from esphome\"}";
  messages.push(msg);   
}



void WebNotify::loop() {
 
   if (network::is_connected() ) {
      if (((millis() - retryDelay) > delayTime) && !connected && botId_.length() > 0 &&  (enableBot_ || (messages.size() && enableSend_))) {
          ESP_LOGD("test","Telegram connecting to %s",apiHost_.c_str());
          mg_http_connect(&mgr,apiHost_.c_str(), notify_fn,this);  // Create client connection 
      }
      mg_mgr_poll(&mgr, 0);
   }
 
}

void WebNotify::dump_config() {
 // ESP_LOGCONFIG(TAG, "Web Server:");
 // ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), port_);
}



WebNotify * global_notify=nullptr;


}  // namespace web_server
}  // namespace esphome
