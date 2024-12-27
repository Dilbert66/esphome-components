#include "telegram_bot.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "ArduinoJson.h"
#include <cstdlib>

// #define USETASK
// #define ASYNC_CORE 1

// #if defined(ESP32) && defined(USETASK)
// #include <esp_chip_info.h>
// #include <esp_task_wdt.h>
// #endif

#ifdef USE_ARDUINO
#include <StreamString.h>
#else
#define F(x) x
#define printf_P(fmt, ...) printf(fmt, ##__VA_ARGS__) P

class String : public std::string
{
public:
  String() : std::string() {}
  String(const char *&p, size_t &s) : std::string(p, s) {}

  int indexOf(char ch, unsigned int fromIndex) const
  {
    if (fromIndex >= this->length())
      return -1;
    const char *temp = strchr(this->c_str() + fromIndex, ch);
    if (temp == NULL)
      return -1;
    return temp - this->c_str();
  }

  String substring(unsigned int left, unsigned int right) const
  {
    if (left > right)
    {
      unsigned int temp = right;
      right = left;
      left = temp;
    }
    unsigned int len = this->length();
    String out;
    if (left >= len)
      return out;
    if (right > len)
      right = len;
    out.copy(((char *)this->c_str()) + left, right - left);
    return out;
  }

  long toInt(void) const
  {
    if (this->c_str())
      return atol(this->c_str());
    return 0;
  }
};
#endif

namespace esphome
{
  namespace web_notify
  {
    uint64_t t1;
    static const char *const TAG = "telegram";

    WebNotify::WebNotify()
    {
      global_notify = this;
    }

    void WebNotify::publish(SendData &out)
    {

      if (!enableSend_ && !out.force)
      {
        ESP_LOGE(TAG, "Message sending is not enabled");
        return;
      }

      std::string outmsg = json::build_json([out, this](JsonObject root)
                                            {
    if (out.reply_markup.length() > 0 ) {
      json::parse_json(out.reply_markup,  [&root](JsonObject x) -> bool {
      root["reply_markup"]=x;
      return true;
      });
    }
    if (out.callback_id.length()>0) {
      root["callback_query_id"]=out.callback_id.c_str();
      root["cache_time"]=1;
    }
    if (out.message_id.length() > 0)
      root["message_id"]=out.message_id.c_str();
    if (out.chat_id.length() > 0)
      root["chat_id"] = out.chat_id.c_str();
    if (out.parse_mode.length()>0)
      root["parse_mode"] = out.parse_mode.c_str();
    if(out.disable_notification)
      root["disable_notification"]=out.disable_notification;
    if(out.disable_web_page_preview)
      root["disable_web_page_preview"]=out.disable_web_page_preview;
    if(out.resize_keyboard)
      root["resize_keyboard"]=out.resize_keyboard;
    if(out.one_time_keyboard)
      root["one_time_keyboard"]=out.one_time_keyboard;
    if (out.text.length()>0)
      root["text"] = out.text.c_str();
    if(out.url.length() > 0)
      root["url"]=out.url;
    if (out.cache_time)
      root["cache_time"]=out.cache_time;
    if(out.selective)
      root["selective"]=out.selective; });


      outMessage omsg;
      omsg.msg = outmsg;
      omsg.type = out.type;
      messages.push(omsg);
    }

    bool WebNotify::isAllowed(std::string chat_id)
    {
      if (std::find(allowed_chat_ids.begin(), allowed_chat_ids.end(), "*") != allowed_chat_ids.end())
        return true;

      if (std::find(allowed_chat_ids.begin(), allowed_chat_ids.end(), chat_id) != allowed_chat_ids.end())
        return true;

      return false;
    }

    void WebNotify::parseArgs(RemoteData &x)
    {
      if (x.text[0] != '/')
        return;
      size_t n=x.text.find("@");
      size_t n1=x.text.find(" ");
      if (n != std::string::npos) {
        x.cmd=x.text.substr(0,n);
        if (n1 != std::string::npos) 
          x.to=x.text.substr(n,n1-x.cmd.length());
        else
          x.to=x.text.substr(n);
      } else {
        x.cmd=x.text.substr(0,n1);
        x.to="";
      }
      if (n1 != std::string::npos)
        x.args = x.text.erase(0, n1 + 1);
      else
        x.args="";

      ESP_LOGD(TAG," parse args: cmd=%s,to=%s,args=%s",x.cmd.c_str(),x.to.c_str(),x.args.c_str());

    }
/*{"ok":true,"result":{"id":2136726661,"is_bot":true,"first_name":"@VistaBot","username":"VistaKeypadbot",
"can_join_groups":false,"can_read_all_group_messages":false,"supports_inline_queries":false,"can_connect_to_business":false,"has_main_web_app":false}}
*/
    bool WebNotify::processMessage(const char *payload)
    {

      return json::parse_json(payload, [payload](JsonObject root) -> bool
                              {
                                if (root["result"][0]["callback_query"]["id"])
                                {

                                  int update_id = root["result"][0]["update_id"];

                                  update_id = update_id + 1;
                                  // we ignore the first message on initial start to avoid a reboot loop
                                  // if (global_notify->lastMsgReceived == 0)  global_notify->lastMsgReceived = update_id;

                                  if (global_notify->lastMsgReceived != update_id)
                                  {

                                    RemoteData x;
                                    if (global_notify->lastMsgReceived == 0)
                                      x.is_first_cmd = true;
                                    else
                                      x.is_first_cmd = false;
                                    global_notify->lastMsgReceived = update_id;
                                    x.sender = root["result"][0]["callback_query"]["message"]["from"]["username"].as<std::string>();
                                    x.text = root["result"][0]["callback_query"]["data"].as<std::string>();
                                    x.chat_id = root["result"][0]["callback_query"]["message"]["chat"]["id"].as<std::string>();
                                    x.date = root["result"][0]["callback_query"]["message"]["date"].as<std::string>();
                                    x.message_id = root["result"][0]["callback_query"]["message"]["message_id"].as<std::string>();
                                    x.callback_id = root["result"][0]["callback_query"]["id"].as<std::string>();
                                    x.is_callback = true;
                                    x.cmd = "";
                                    x.args = "";
                                    if (!global_notify->isAllowed(x.chat_id))
                                    {
                                      MG_INFO(("Chat id %s not allowed to send to bot", x.chat_id.c_str()));
                                      return true;
                                    }
                                    global_notify->parseArgs(x);
                                    global_notify->on_message_.call(x);
                                  }
                                  /*
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

                                          // m.sender = sender;
                                          // m.text = text;
                                          // m.chat_id = chat_id;
                                          //m.date = date;
                                          // m.is_callback=false;
                                          global_notify->lastMsgReceived = update_id;
                                        }
                                  */
                                }
                                else if (root["result"][0]["message"]["text"])
                                {
                                  int update_id = root["result"][0]["update_id"];
                                  update_id = update_id + 1;
                                  // we ignore the first message on initial start to avoid a reboot loop
                                  //  if (global_notify->lastMsgReceived == 0)  global_notify->lastMsgReceived = update_id;

                                  if (global_notify->lastMsgReceived != update_id)
                                  {

                                    RemoteData x;
                                    if (global_notify->lastMsgReceived == 0)
                                      x.is_first_cmd = true;
                                    else
                                      x.is_first_cmd = false;
                                    global_notify->lastMsgReceived = update_id;
                                    x.sender = root["result"][0]["message"]["from"]["username"].as<std::string>();

                                    x.text = root["result"][0]["message"]["text"].as<std::string>();
                                    x.chat_id = root["result"][0]["message"]["chat"]["id"].as<std::string>();
                                    x.date = root["result"][0]["message"]["date"].as<std::string>();
                                    x.message_id = root["result"][0]["message"]["message_id"].as<std::string>();
                                    x.is_callback = false;
                                    x.cmd = "";
                                    x.args = "";
                                    if (!global_notify->isAllowed(x.chat_id))
                                    {
                                      MG_INFO(("Chat id %s not allowed to send to bot", x.chat_id.c_str()));
                                      return true;
                                    }
                                    global_notify->parseArgs(x);
                                    global_notify->on_message_.call(x);
                                  }
                                }
                                else if (root["result"]["message_id"])
                                {

                                  bool ok = root["ok"];
                                  std::string id = root["result"]["message_id"];
                                  if (ok)
                                  {
                                    // pop last sent message from queue as it was successful
                                    global_notify->messages.pop();
                                  }
                                  else 
                                  {
                                    global_notify->retryDelay = millis();
                                     ESP_LOGD(TAG, "Error response from server on last send: %s", payload);
                                  }
                                } 
                                else if (root["result"]["is_bot"]) {
                                  if (root["result"]["first_name"]) {
                                    std::string botname=root["result"]["first_name"];
                                    //std::string botusername=root["result"]["username"];
                                    global_notify->set_bot_name(botname);
                                    std::string botusername=root["result"]["username"];
                                    ESP_LOGD(TAG,"Set bot name to %s",botname.c_str());
                                  }
                                  global_notify->messages.pop();
                                } 
                                else if (global_notify->sending)
                                {
                                  // message response. Pop the message anyhow so we don't loop.
                                  global_notify->messages.pop();
                                }
                                else if (root["result"][0]["update_id"])
                                {
                                  int update_id = root["result"][0]["update_id"];
                                  global_notify->lastMsgReceived = update_id + 1;
                                }
                                else
                                {
                                  bool ok = root["ok"];
                                  if (!ok)
                                  {
                                    global_notify->retryDelay = millis();
                                    ESP_LOGD(TAG, "Error response from server: %s", payload);
                                  }
                                }
                                return true; });
    }

    void WebNotify::notify_fn(struct mg_connection *c, int ev, void *ev_data)
    {

      if (global_notify == NULL)
      {
        ESP_LOGE(TAG, "Global telegram pointer is null");
        return;
      }
      int *i = &((struct c_res_s *)c->fn_data)->i;
      if (ev == MG_EV_OPEN)
      {
        c->send.c=c;
        c->recv.c=c;
        global_notify->connected = true;
        // Connection created. Store connect expiration time in c->data
        *(uint64_t *)&c->data[2] = mg_millis() + global_notify->timeout_ms;
      }
      else if (ev == MG_EV_POLL)
      {

        if ((c->is_connecting || c->is_resolving) && mg_millis() > *(uint64_t *)&c->data[2])
        {
          mg_error(c, "Connect timeout");
        }
        if (c->data[0] == 'T')
        {
          if (!global_notify->sending && !c->is_draining && !c->is_closing && (!global_notify->enableBot_ || global_notify->messages.size()))
          {
            c->is_draining = 1; // close long poll so we can send
          }
        }

      }
      else if (ev == MG_EV_CLOSE)
      {
        global_notify->sending = false;
        global_notify->connected = false;
        // MG_INFO(("Got ev close,enablebot %d, ma %d, sending=%d",global_notify->enableBot_, global_notify->messages.size(),global_notify->sending));
      }
      else if (ev == MG_EV_CONNECT)
      {
        c->send.c=c;
        c->recv.c=c;
        if (c->data[0] == 'T')
        {
          c->is_closing = 1; // close long poll if opened so we can send
          // ESP_LOGD("test","Pending message closing poll connection from connect");
          return;
        }
        c->data[0] = 'T';
        global_notify->connected = true;
        // MG_INFO(("got ev connect"));
        // Connected to server. Extract host name from URL
        struct mg_str host = mg_url_host(global_notify->apiHost_.c_str());

        if (mg_url_is_ssl(global_notify->apiHost_.c_str()))
        {
          ESP_LOGD(TAG, "TLS init - Before: freeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(8));
          struct mg_tls_opts opts = {.name = host};
          mg_tls_init(c, &opts);
          if (c->tls == NULL)
            return;
        }

        ESP_LOGD(TAG, "TLS init - After: freeheap: %5d,minheap: %5d,maxfree:%5d\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(8));

        if (global_notify->messages.size())
        {
          global_notify->sending = true; // we are connecting so flag connection as open

          outMessage outmsg = global_notify->messages.front();

          mg_printf(c, "POST /bot%s", global_notify->botId_.c_str());
          if (outmsg.type == mtEditMessageText)
            mg_printf(c, "/editMessageText HTTP/1.1\r\n");
          else if (outmsg.type == mtAnswerCallbackQuery)
            mg_printf(c, "/answerCallbackQuery HTTP/1.1\r\n");
          else if (outmsg.type == mtEditMessageReplyMarkup)
            mg_printf(c, "/editMessageReplyMarkup HTTP/1.1\r\n");
          else if (outmsg.type == mtDeleteMessage)
            mg_printf(c, "/deleteMessage HTTP/1.1\r\n");
          else if (outmsg.type==mtGetMe)
            mg_printf(c,"/getMe HTTP/1.1\r\n");
          else
            mg_printf(c, "/sendMessage HTTP/1.1\r\n");
          mg_printf(c, "Host: %.*s\r\n", host.len, host.buf);
          mg_printf(c, "Content-Type: application/json\r\n");
          mg_printf(c, "Content-Length: %d\r\n\r\n%s\r\n", outmsg.msg.length(), outmsg.msg.c_str());
           //printf("\r\nsent telegram msg %s\r\n",outmsg.msg.c_str());
        }
        else if (global_notify->enableBot_)
        {
          global_notify->sending = false;
          mg_printf(c, "GET /bot%s", global_notify->botId_.c_str());
          mg_printf(c, "/getUpdates?limit=1&timeout=120&offset=%d HTTP/1.1\r\n", global_notify->lastMsgReceived);
          mg_printf(c, "Host: %.*s\r\n", host.len, host.buf);
          mg_printf(c, "Accept: application/json\r\n");
          mg_printf(c, "Cache-Control: no-cache\r\n\r\n");
          // ESP_LOGD("test","sent post data lastmessage=%d",global_notify->lastMsgReceived);
        }
      }
      /*
      else if (ev == MG_EV_POLL && *i != 0) {
         ESP_LOGD(TAG,"data 1a= %d",*i);
         *i=0;
         struct mg_str host = mg_url_host(global_notify->apiHost_.c_str());
         if (global_notify->messages.size()) {
          global_notify->sending=true;       //we are connecting so flag connection as open

          outMessage outmsg=global_notify->messages.front();

          mg_printf(c,"POST /bot%s",global_notify->botId_.c_str());
          if (outmsg.type==mtEditMessageText)
              mg_printf(c,"/editMessageText HTTP/1.1\r\n");
          else if(outmsg.type==mtAnswerCallbackQuery)
              mg_printf(c,"/answerCallbackQuery HTTP/1.1\r\n");
          else if (outmsg.type==mtEditMessageReplyMarkup)
              mg_printf(c,"/editMessageReplyMarkup HTTP/1.1\r\n");
          else if (outmsg.type==mtDeleteMessage)
              mg_printf(c,"/deleteMessage HTTP/1.1\r\n");
          else
              mg_printf(c,"/sendMessage HTTP/1.1\r\n");
          mg_printf(c,"Host: %.*s\r\n",host.len,host.buf);
          mg_printf(c,"Content-Type: application/json\r\n");
          mg_printf(c,"Content-Length: %d\r\n\r\n%s\r\n",outmsg.msg.length(),outmsg.msg.c_str());
          // printf("\r\nsent telegram msg %s\r\n",outmsg.msg.c_str());
        } else if ( global_notify->enableBot_) {
          global_notify->sending=false;
          mg_printf(c,"GET /bot%s",global_notify->botId_.c_str());
          mg_printf(c,"/getUpdates?limit=1&timeout=120&offset=%d HTTP/1.1\r\n",global_notify->lastMsgReceived);
          mg_printf(c,"Host: %.*s\r\n",host.len,host.buf);
          mg_printf(c,"Accept: application/json\r\n");
          mg_printf(c,"Cache-Control: no-cache\r\n\r\n");
          // ESP_LOGD("test","sent post data lastmessage=%d",global_notify->lastMsgReceived);
        }
      } */
      else if (ev == MG_EV_HTTP_MSG)
      {

        // Response is received. Print it
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        // printf("\r\nresponse message from telegram: %.*s\r\n", (int) hm->message.len, hm->message.buf);

        String payload = String(hm->body.buf, hm->body.len);
        // printf("\r\nresponse body from telegram: %s\r\n", payload.c_str());
        if (!global_notify->processMessage(payload.c_str()))
        {
          printf_P("%s", F("\nMessage parsing failed, skipped.\n"));
          int update_id_first_digit = 0;
          int update_id_last_digit = 0;
          for (int a = 0; a < 3; a++)
          {
            update_id_first_digit = payload.indexOf(':', update_id_first_digit + 1);
          }
          for (int a = 0; a < 2; a++)
          {
            update_id_last_digit = payload.indexOf(',', update_id_last_digit + 1);
          }
          global_notify->lastMsgReceived = payload.substring(update_id_first_digit + 1, update_id_last_digit).toInt() + 1;
        }
        global_notify->sending = false;
        c->is_closing = 1; // Tell mongoose to close this connection
      }
      else if (ev == MG_EV_ERROR)
      {
        global_notify->retryDelay = millis();
        ESP_LOGD(TAG, "MG_EV_ERROR server");
      }
    }

    //     #if defined(ESP32) && defined(USETASK)

    //     void WebNotify::telegramTask(void *args)
    //     {

    //       static unsigned long checkTime = millis();
    //       for (;;)
    //       {
    //               mg_mgr_poll(&(global_notify->mgr), 1);

    //               vTaskDelay(4 / portTICK_PERIOD_MS);
    // #if not defined(ARDUINO_MQTT)
    //         if (millis() - checkTime > 30000)
    //         {
    //           UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    //           ESP_LOGD(TAG, "High water stack level: %5d", (uint16_t)uxHighWaterMark);
    //           checkTime = millis();
    //         }
    // #endif
    //       }
    //       vTaskDelete(NULL);
    //     }
    // #endif

    void WebNotify::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up web client...");

      if (this->chat_id_f_.has_value())
      {
        auto val = (*this->chat_id_f_)();
        if (val.has_value())
        {
          set_chat_id(std::string(*val));
        }
      }
      if (this->bot_id_f_.has_value())
      {
        auto val = (*this->bot_id_f_)();
        if (val.has_value())
        {
          set_bot_id(std::string(*val));
        }
      }
      ESP_LOGD(TAG, "chat id=[%s],bot id=[%s]", telegramUserId_.c_str(), botId_.c_str());

      mg_mgr_init(&mgr);
      // std::string msg="{\"chat_id\":"+std::string(telegramUserId_.c_str())+",\"text\":\"Esphome Telegram client started.\"}";
      // outMessage out;
      // out.msg=msg;
      // out.type=mtSendMessage;
      // messages.push(out);

      // #if defined(ESP32) && defined(USETASK)
      //       esp_chip_info_t info;
      //       esp_chip_info(&info);
      //       ESP_LOGD(TAG, "Cores: %d,arduino core=%d", info.cores, CONFIG_ARDUINO_RUNNING_CORE);
      //       uint8_t core = info.cores > 1 ? ASYNC_CORE : 0;
      //       xTaskCreatePinnedToCore(
      //           this->telegramTask, // Function to implement the task
      //           "telegramTask",     // Name of the task
      //           3200,               // Stack size in words
      //           (void *)this,       // Task input parameter
      //           12,                 // Priority of the task
      //           &xHandle            // Task handle.
      //           ,
      //           core // Core where the task should run
      //       );
      // #endif

    }

    void WebNotify::loop()
    {
      static bool firstRun = true;

      if (network::is_connected())
      {
        if (!connected && ((enableBot_ && botId_.length() > 0) || (messages.size() && enableSend_)) && ((millis() - retryDelay) > delayTime || firstRun))
        {
         // ESP_LOGD(TAG, "Connecting to telegram api %d, %d,%d", millis(), delayTime, retryDelay);

          mg_http_connect(&mgr, apiHost_.c_str(), notify_fn, &c_res); // Create client connection
          if (botName_=="") {
            outMessage out;
            out.type=mtGetMe;
            messages.push(out);
          }
          firstRun = false;
        }
      }

      static unsigned long checkTime = millis();
      if (millis() - checkTime > 20000)
      {
        checkTime = millis();
        UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGD(TAG, "Free memory: %5d", (uint16_t)uxHighWaterMark);
      }

      mg_mgr_poll(&mgr, 0);
    }

    void WebNotify::dump_config()
    {
      // ESP_LOGCONFIG(TAG, "Web Server:");
      // ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), port_);
    }

    void WebNotify::set_bot_id_f(std::function<optional<std::string>()> &&f) { this->bot_id_f_ = f; }
    void WebNotify::set_chat_id_f(std::function<optional<std::string>()> &&f) { this->chat_id_f_ = f; }

    WebNotify *global_notify = nullptr;

  } // namespace web_notify
} // namespace esphome
