#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/automation.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/mg_lib/mongoose.h"
#include <vector>
#include <algorithm>
#include <queue>
#ifdef USE_ESP32
#include <deque>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace esphome
{
  namespace web_notify
  {

    enum msgtype
    {
      mtSendMessage,
      mtEditMessageText,
      mtAnswerCallbackQuery,
      mtEditMessageReplyMarkup,
      mtDeleteMessage,
      mtGetMe,
      mtSwitch
    };

    struct RemoteData
    {
      std::string text;
      std::string chat_id;
      std::string sender;
      std::string date;
      std::string cmd;
      std::string args;
      bool is_callback;
      std::string message_id;
      std::string callback_id;
      bool is_first_cmd;
      std::string to;
    };

    struct SendData
    {
      std::string text = "";
      std::string chat_id = "";
      std::string title = "";
      msgtype type = mtSendMessage;
      std::string tag = "";
      std::string message_id = "";
      std::string reply_markup = "";
      std::string callback_id = "";
      std::string parse_mode = "";
      bool disable_notification = false;
      bool disable_web_page_preview = false;
      bool resize_keyboard = false;
      bool one_time_keyboard = false;
      bool show_alert = false;
      bool selective = false;
      bool force = false;
      int cache_time;
      std::string url;
      void *f{};
    };


    struct c_res_s
    {
      int i = 0;
      struct mg_connection *c;
    };

    class WebNotify : public Controller, public Component
    {
    public:
      WebNotify();

      void setup() override;
      void loop() override;

      void dump_config() override;
      void publish(SendData &out);

      void publish(const std::string &chat_id, const std::string &message, bool force)
      {
        SendData out;
        out.chat_id = chat_id;
        out.parse_mode = "html";
        out.text = message;
        out.force = force;
        out.type = mtSendMessage;
        publish(out);
      }
      
      void publish(const std::string &message, bool force = false)
      {
        SendData out;
        out.chat_id=this->telegramUserId_;
        out.parse_mode = "html";
        out.text = message;
        out.force = force;
        out.type = mtSendMessage;
        publish(out);
      }

      void publish(const std::string &chat_id, const std::string &message, const std::string &reply_markup = "", const std::string &parse_mode = "html", bool disable_notification = false, bool disable_web_page_preview = false, bool resize_keyboard = false, bool one_time_keyboard = false)
      {
        SendData out;
        out.text = message;
        out.chat_id = chat_id;
        out.reply_markup = reply_markup;
        out.parse_mode = parse_mode;
        out.disable_notification = disable_notification;
        out.disable_web_page_preview = disable_web_page_preview;
        out.resize_keyboard = resize_keyboard;
        out.one_time_keyboard = one_time_keyboard;
        out.type = mtSendMessage;
        publish(out);
      };

      void turnon_switch(void *f) {
        SendData out;
        out.type=mtSwitch;
        out.f=f;
        out.selective=true;
        publish(out);
      }

      void turnoff_switch(void *f) {
        SendData out;
        out.type=mtSwitch;
        out.f=f;
        out.selective=false;
        publish(out);
      }
      



      void answerCallbackQuery(const std::string &message, const std::string &callback_id, bool show_alert = false, std::string url = "", int cache_time = 0, bool force = false)
      {
        SendData out;
        out.text = message;
        out.message_id = callback_id;
        out.show_alert = show_alert;
        out.url = url;
        out.cache_time = cache_time;
        out.type = mtAnswerCallbackQuery;
        out.chat_id=this->telegramUserId_;
        out.force = force;
        publish(out);
      }

      void editMessageText(const std::string &chat_id, const std::string &message, const std::string &message_id, const std::string &inline_keyboard = "", const std::string &parse_mode = "html", bool disable_web_page_preview = false)
      {
        SendData out;
        out.chat_id = chat_id;
        out.text = message;
        out.message_id = message_id;
        out.reply_markup = "{'inline_keyboard':" + inline_keyboard + "}";
        out.parse_mode = parse_mode;
        out.disable_web_page_preview = disable_web_page_preview;
        out.type = mtEditMessageText;
        publish(out);
      }

      void editMessageReplyMarkup(const std::string &chat_id, const std::string &message_id, const std::string &inline_keyboard = "", bool disable_web_page_preview = false)
      {
        SendData out;
        out.chat_id = chat_id;
        out.message_id = message_id;
        out.reply_markup = "{'inline_keyboard':" + inline_keyboard + "}";
        out.disable_web_page_preview = disable_web_page_preview;
        out.type = mtEditMessageReplyMarkup;
        publish(out);
      }

      void set_bot_id(std::string &&bot_id) { botId_ = bot_id; }
      void set_bot_name(std::string bot_name) { botName_ = bot_name; }

      void set_chat_id(std::string &&chat_id)
      {
        telegramUserId_ = chat_id;
        if (telegramUserId_ != "" && std::find(allowed_chat_ids_.begin(), allowed_chat_ids_.end(), telegramUserId_) == allowed_chat_ids_.end())
          allowed_chat_ids_.push_back(telegramUserId_);
      }
      std::string get_chat_id() { return telegramUserId_; }
      std::string get_bot_id() { return botId_; }
      void add_chatid(std::string &&chat_id) { allowed_chat_ids_.push_back(std::move(chat_id)); }
      void set_api_host(std::string &&api_host) { apiHost_ = "https://" + std::move(api_host); }
      void set_bot_enable(bool enable) { enableBot_ = enable; }
      void set_send_enable(bool enable) { enableSend_ = enable; }
      void set_skip_first(bool skip) {skipFirst_= skip;}
      bool get_bot_status() { return enableBot_; }
      bool get_send_status() { return enableSend_; }
      bool get_connect_error() { return connectError_;}
      uint8_t get_queue_size() { return messages_.size();}

      std::string get_bot_name() { return botName_; }
      
      using on_message_callback_t = void(RemoteData &x);

      CallbackManager<on_message_callback_t> on_message_;

      void set_on_message(std::function<on_message_callback_t> &&callback)
      {
        this->on_message_.add(std::move(callback));
      }

      std::string telegramUserId_{};

      void set_bot_id_f(std::function<optional<std::string>()> &&f);
      void set_chat_id_f(std::function<optional<std::string>()> &&f);

    private:

      struct mg_mgr mgr_;
      static void notify_fn(struct mg_connection *c, int ev, void *ev_data);

      bool botRequest_{};
      bool skipFirst_{};

      struct outMessage
      {
        std::string msg;
        void *f;
        bool state;
        msgtype type;
      };

      // #ifdef ESP32
      //       TaskHandle_t xHandle;
      //       static void telegramTask(void *args);
      // #endif

      struct c_res_s c_res_;

      std::string apiHost_ = "https://api.telegram.org/";
      //const uint32_t timeout_ms = 15000; // Connect timeout in milliseconds
      int lastMsgReceived_ = 0;
      std::string botId_ = "";
      std::string botName_ = "";

      bool enableBot_ = false;
      bool enableSend_ = true;
      bool sending_ = false;
      bool connected_ = false;
      bool connectError_=false;
      unsigned long retryDelay_ = 0;
      int delayTime_ = 15000; // ms
      std::queue<outMessage> messages_;
      std::vector<std::string> allowed_chat_ids_;
      bool isAllowed(std::string chat_id);
      void parseArgs(RemoteData &x);
      bool processMessage(const char *payload);

      optional<std::function<optional<std::string>()>> chat_id_f_{};
      optional<std::function<optional<std::string>()>> bot_id_f_{};
    };

    extern WebNotify *global_notify;

    template <typename... Ts>
    class TelegramPublishAction : public Action<Ts...>
    {
    public:
      TelegramPublishAction(WebNotify *parent) : parent_(parent) {}
      TEMPLATABLE_VALUE(std::string, to)
      TEMPLATABLE_VALUE(std::string, message)
      TEMPLATABLE_VALUE(std::string, reply_markup)
      TEMPLATABLE_VALUE(std::string, keyboard)
      TEMPLATABLE_VALUE(std::string, inline_keyboard)
      TEMPLATABLE_VALUE(std::string, parse_mode)
      TEMPLATABLE_VALUE(std::string, title)
      TEMPLATABLE_VALUE(bool, disable_notification)
      TEMPLATABLE_VALUE(bool, disable_web_page_preview)
      TEMPLATABLE_VALUE(bool, resize_keyboard)
      TEMPLATABLE_VALUE(bool, one_time_keyboard)
      TEMPLATABLE_VALUE(bool, force)

      void play(Ts... x) override
      {
        SendData y;

        if (this->keyboard_.value(x...) != "")
          if (this->keyboard_.value(x...) == "none" || this->keyboard_.value(x...) == "false")
            y.reply_markup = "{remove_keyboard: true}";
          else
            y.reply_markup = "{'keyboard':" + std::move(this->keyboard_.value(x...)) + "}";
        else if (this->inline_keyboard_.value(x...) != "")
          y.reply_markup = "{'inline_keyboard':" + std::move(this->inline_keyboard_.value(x...)) + "}";
        if (this->reply_markup_.value(x...) != "")
          y.reply_markup = this->reply_markup_.value(x...);
        if (this->parse_mode_.value(x...) != "")
          y.parse_mode = this->parse_mode_.value(x...);
        if (this->to_.value(x...) != "")
          y.chat_id = this->to_.value(x...);
        else
          y.chat_id = this->parent_->telegramUserId_;

        if (this->disable_notification_.value(x...))
          y.disable_notification = this->disable_notification_.value(x...);
        if (this->disable_web_page_preview_.value(x...))
          y.disable_web_page_preview = this->disable_web_page_preview_.value(x...);
        if (this->resize_keyboard_.value(x...))
          y.resize_keyboard = this->resize_keyboard_.value(x...);
        if (this->one_time_keyboard_.value(x...))
          y.one_time_keyboard = this->one_time_keyboard_.value(x...);

        y.text = this->message_.value(x...);
        if (this->title_.value(x...) != "")
          y.text = this->title_.value(x...) + "\n" + y.text;
        if (this->force_.value(x...))
          y.force = this->force_.value(x...);
        y.type = mtSendMessage;
        this->parent_->publish(y);
      }

    protected:
      WebNotify *parent_;
    };

    template <typename... Ts>
    class TelegramAnswerCallBackAction : public Action<Ts...>
    {
    public:
      TelegramAnswerCallBackAction(WebNotify *parent) : parent_(parent) {}
      TEMPLATABLE_VALUE(std::string, message)
      TEMPLATABLE_VALUE(std::string, callback_id)
      TEMPLATABLE_VALUE(bool, show_alert)
      TEMPLATABLE_VALUE(std::string, url)
      TEMPLATABLE_VALUE(int, cache_time)

      void play(Ts... x) override
      {
        SendData y;
        y.callback_id = this->callback_id_.value(x...);
        if (this->show_alert_.value(x...))
          y.show_alert = this->show_alert_.value(x...);
        y.text = this->message_.value(x...);
        y.url = this->url_.value(x...);
        y.cache_time = this->cache_time_.value(x...);
        y.type = mtAnswerCallbackQuery;
        this->parent_->publish(y);
      }

    protected:
      WebNotify *parent_;
    };

    template <typename... Ts>
    class TelegramDeleteMessageAction : public Action<Ts...>
    {
    public:
      TelegramDeleteMessageAction(WebNotify *parent) : parent_(parent) {}
      TEMPLATABLE_VALUE(std::string, chat_id)
      TEMPLATABLE_VALUE(std::string, message_id)

      void play(Ts... x) override
      {
        SendData y;
        y.chat_id = this->chat_id_.value(x...);
        y.message_id = this->message_id_.value(x...);

        y.type = mtDeleteMessage;
        this->parent_->publish(y);
      }

    protected:
      WebNotify *parent_;
    };

    template <typename... Ts>
    class TelegramEditMessageAction : public Action<Ts...>
    {
    public:
      TelegramEditMessageAction(WebNotify *parent) : parent_(parent) {}
      TEMPLATABLE_VALUE(std::string, chat_id)
      TEMPLATABLE_VALUE(std::string, message)
      TEMPLATABLE_VALUE(std::string, inline_keyboard)
      TEMPLATABLE_VALUE(std::string, parse_mode)
      TEMPLATABLE_VALUE(std::string, title)
      TEMPLATABLE_VALUE(std::string, message_id)
      TEMPLATABLE_VALUE(bool, disable_web_page_preview)
      TEMPLATABLE_VALUE(std::string, reply_markup)

      void play(Ts... x) override
      {
        SendData y;
        y.chat_id = this->chat_id_.value(x...);
        if (this->inline_keyboard_.value(x...) != "")
          y.reply_markup = "{'inline_keyboard':" + std::move(this->inline_keyboard_.value(x...)) + "}";
        if (this->reply_markup_.value(x...) != "")
          y.reply_markup = this->reply_markup_.value(x...);
        if (this->parse_mode_.value(x...) != "")
          y.parse_mode = this->parse_mode_.value(x...);
        if (this->disable_web_page_preview_.value(x...))
          y.disable_web_page_preview = this->disable_web_page_preview_.value(x...);
        y.text = this->message_.value(x...);
        y.message_id = this->message_id_.value(x...);
        if (this->title_.value(x...) != "")
          y.text = this->title_.value(x...) + "\n" + y.text;
        y.type = mtEditMessageText;
        this->parent_->publish(y);
      }

    protected:
      WebNotify *parent_;
    };

    template <typename... Ts>
    class TelegramEditReplyMarkupAction : public Action<Ts...>
    {
    public:
      TelegramEditReplyMarkupAction(WebNotify *parent) : parent_(parent) {}
      TEMPLATABLE_VALUE(std::string, chat_id)
      TEMPLATABLE_VALUE(std::string, message_id)
      TEMPLATABLE_VALUE(std::string, reply_markup)
      TEMPLATABLE_VALUE(std::string, inline_keyboard)
      TEMPLATABLE_VALUE(bool, disable_web_page_preview)

      void play(Ts... x) override
      {

        SendData y;
        y.chat_id = this->chat_id_.value(x...);
        if (this->inline_keyboard_.value(x...) != "")
          y.reply_markup = "{'inline_keyboard':" + std::move(this->inline_keyboard_.value(x...)) + "}";
        if (this->reply_markup_.value(x...) != "")
          y.reply_markup = this->reply_markup_.value(x...);
        if (this->disable_web_page_preview_.value(x...))
          y.disable_web_page_preview = this->disable_web_page_preview_.value(x...);
        y.message_id = this->message_id_.value(x...);
        y.text = this->message_.value(x...);
        y.type = mtEditMessageReplyMarkup;
        this->parent_->publish(y);
      }

    protected:
      WebNotify *parent_;
    };

    class TelegramMessageTrigger : public Trigger<RemoteData>
    {
    public:
      bool stringsEqual(std::string str1, std::string str2)
      {
        if (str1.length() != str2.length())
          return false;

        for (int i = 0; i < str1.length(); ++i)
        {
          if (tolower(str1[i]) != tolower(str2[i]))
            return false;
        }

        return true;
      }

      explicit TelegramMessageTrigger(const std::string &cmd, const std::string &type)
      {
        global_notify->set_on_message([cmd, type, this](RemoteData &x)
                                      {
                                        std::string s = x.cmd;
                                        // ESP_LOGD("test","callback is %d, type=%s,cmd=%s",x.is_callback,type.c_str(),cmd.c_str());

                                        if (x.to !="" && !stringsEqual(x.to,global_notify->get_bot_name()) )
                                          return;

                                        if (type == "callback")
                                        {
                                          if (!x.is_callback)
                                            return;
                                          s = x.text;
                                        }
                                        if (type == "cmd")
                                        {
                                          if (x.cmd == "" || x.is_callback)
                                            return;
                                        }

                                        if (type == "text")
                                        {
                                          if (x.cmd != "" || x.is_callback)
                                            return;
                                          s = x.text;
                                        }
                                        

                                        if (cmd.find("," + s + ",") != std::string::npos)
                                          this->trigger(x);
                                        if (cmd.find(",*,") != std::string::npos)
                                          this->trigger(x); });
      };
    };

  } // namespace web_notify
} // namespace esphome
