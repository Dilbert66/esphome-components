# Various custom components for the ESPHOME system

## Vista alarm system interface
Please see the main project documenation here for the Vista series panels:
https://github.com/Dilbert66/esphome-vistaECP

## DSC alarm system interface
Please see the main project documenation here for the DSC Powerseries panels:
https://github.com/Dilbert66/esphome-dsckeybus

# ESP32 Standalone Telegram Bot

This component provides a standalone telegram bot application running on an ESP32 platform that gives the ability to fully control an Esphome application from your telegram account.  You can send any commands you want as well as have it send back any statuses via a secure https connection using the telegram api, all remotely.  It does not require any other application or home control software.  Due to the need to use TLS for communications (using MbedTLS ) to the Telegram API site, this component will use about 46K of heap memory which should not be a problem on the ESP32.  For performance and stability reasons, I've used the mongoose.ws web library as the base for all http handling.  This library provides an event driven, non-blocking API for creating various TCP apps.  I initially chose it to replace the ESPasyncwebserver library that was used on the web_server component since I was encountering too many heap crashes during heavy traffic.  I have been very pleased with it's minimal memory footprint and reliability. 

This component is part of a complete system that I use to create a standalone alarm control applications using ESP32 hardware.  Why have a telegram bot on the ESP ? Not everyone wants to have a separate home control system or only needs one ESP device to manage something.  For instance, for someone who only wants to control their legacy alarm system remotely , they can do it all usinng one chip.   This bot uses a technique called long polling to wait for messages on the telegram api.  It will create a connection to the api server and wait for up to 2 minutes for new messages.  This is done using a non-blocking event driven loop which will not impact any other running components on the system. When a status update needs to go out from the ESP, the software will terminate the long poll loop, send the message and re-implement the poll.


I will document each option later but for now, I'm providing a fairly complete example below that I use on with vista20 alarm system.  If you have not created a telegram bot before, I've listed a quick list of the required steps below. You can get further details if you need by doing a search on the web or from telegram's site.  Basically what you need is a bot_id (token) to use for connecting to the telegram api and to receive and send cmds. The chat_id is needed in order to have a default destination  to send notifications back to. This can also be a group chat_id.   The example below basically creates a menu of cmds that the user can use to control their alarm system or retrieve system statuses back. This will of course be different on every application.

You will first need a fully working yaml config.  The options below will need to be added to that config.  The implementation uses the esphome "custom components" feature which allows the yaml config to download any external source code as needed.  You can also copy the code directly to a folder on your system if you prefer.   The example below as shown, will download the "telegram_bot" component from my github page when you do your local install/compile.  I've also provided two switch configurations that you can use to turn the bot and notify functions off or on from Home assistant or from the web_server application.

## Bot creation
### Get your bot id:
    a. Start a  conversation with @BotFather or go to url: https://telegram.me/botfather
    b. Create a new bot using BotFather: /newbot
    c. Copy the bot token to the "bot_id:" field
    d. Start a conversation with the newly created bot to open the chat.
    
### Get your chat id: 
    a. Start a conversation with @myidbot, or go to url: https://telegram.me/myidbot to get your chat id.
    b. Get your user chat ID with cmd: /getid
    c. Copy the user shown chat ID to the "chat_id:" field or the allowed_chat_ids: field.
    
Optional. If you also want to use allow a group to control your alarm system,
create a new group.  Add your own chat_id, the bot id and any other authorized users to the group.

### Get the group chat id: 
```
   a. Add @getidsbot to the group.
   b. Get the ID from the id: field under "This Chat".
   c. Copy the group chat ID to the chat_id: field or to the allowed_chat_ids: field.
```  
Make sure to add all other allowed user or group chat ids to the allowed_chat_ids: field. 

## YAML config 
Add the following sections to your chosen yaml configuration. You can use the github release version  or 
copy the source directly from this repository to directory "my_components" under your esphome directory
The example below shows a sample bot config showing how to create a menu of cmd options.

Supported telegram actions:
- SendMessage
- EditMessage
- EditReplyMarkup
- AnswerCallbackQuery
- DeleteMessage


The on_message actions will send a RemoteData structure x to lambda's that holds the following values:
```
  x.text -> Sent text from telegram user or group.
  x.chat_id -> chat_id of sending user or group.
  x.sender -> name of sender
  x.date -> time and date of message
  x.cmd -> if text contains a command in the format /xxxx yyyy.  The cmd /xxxx will be stored in x.cmd
  x.args -> the args yyyy from the above cmd will be stored in x.args
  x.is_callback -> boolean - if this message comes from an inline_keyboard action, this will be true.
  x.message_id -> current message id.  Used in replies, edits, etc.
  x.callback_id -> callback id of callback messaged. Used to answer back to inline_keyboard actions.
  x.is_first_cmd -> boolean - Will be true after reboot and false after the first command is received
  x.force -> boolean - Force publish even if notify is off
```

Example bot config:
```
external_components:
  - source: github://Dilbert66/esphome-components@dev #uncomment to use github repository version
  #- source:  my_components #uncomment to use alocal directory  called "my_components"
    components: [telegram_bot] #components to load
    refresh: 10min  #frequency to check for new versions

telegram_bot:
  id: webnotify
  bot_id: "123467890:SAMPLETELEGRAMBOTKEYSAMPLETELEGRAM"  #can also use a lambda value.
  chat_id: "123455222" #default chat id to send to. Can also be a group id or a lambda value.
  bot_enable: true #if you dont need to have the receive bot set to false. Only publish action will be available.
  allowed_chat_ids:  #chat id's that are allowed to send cmds. Can also be group id's. Use "*" to allow all.
    - "12345522"
    - "-2255221"
 on_message:  #provides returned values in RemoteData structure x.
    - callback: "*" #accept all cmds. Can also select any specific text value.
      then:
        - telegram.answer_callback:
           message: "You sent inline cmd: " + x.text;
           callback_query_id: !lambda return x.callback_id;
           show_alert: true
             
    - text: "*"  #accept all text messages
      then:
        - telegram.publish:
           message: !lambda return "You sent text: " + x.text;
           to: !lambda return x.chat_id;                  

    - command: "/disarm,/d"  #cmds/text/callback values and be comma separated to also accept aliases for the cmd.
      then: 
        - lambda: |-
            $panelId->alarm_keypress_partition("${accesscode}1",1); #uses global value "accesscode"

    - command: "/help,/h"
      then: 
        - telegram.publish:
            message: !lambda return "System state:"+id(ss_1).state; 
            to: !lambda return x.chat_id;  #reply to sending chat_id.
        - telegram.publish:
            message: "Help Menu\r\n/disarm - disarm system\r\n/help - this menu\r\n/keys <args> - send keys\r\n/status - system status\r\n/reboot - reboot esp\r\n/notify <on|off> - enable send\r\n/armstay - arm stay\r\n/bypass - bypass all\r\n"
            to: !lambda return x.chat_id;

    - command: "/status,/s"
      then: 
            webnotify->publish(x.chat_id,"System state is "+id(ss_1).state,1); # note 3rd arg "1"  is "force publish"
            std::string m=webnotify->get_send_status()?"on":"off";
            webnotify->publish(x.chat_id,"Notify is " + m,1); 

    - command: "/keys,/k"
      then: 
        - lambda: |-         
            $panelId->alarm_keypress_partition(x.args,1);

    - command: "/armstay,/a"
      then: 
        - lambda: |-   
            $panelId->alarm_keypress_partition("${accesscode}2",1);

    - command: "/notify,/n"
      then: 
        - lambda: |-
           if (x.args=="on") {
            webnotify->set_send_enable(true);
            webnotify->publish(x.chat_id,"Notify is on",1);
           } else {
            webnotify->set_send_enable(false);
            webnotify->publish(x.chat_id,"Notify is off",1);
           }

    - command: "/reboot,/r"
      then: 
       if:
        condition:
            lambda: return !x.is_first_cmd; #ensure it's not the first cmd after reboot
        then:
            switch.turn_on:  restart_switch
            
    - command: "/bypass,/b"
      then: 
        - lambda: |-    
            $panelId->alarm_keypress_partition("${accesscode}6#",1);

    #below shows how to send custom and inline_keyboards back to client.
    - command: "/keyboardtest,/kt"
      then:
       - telegram.publish:
            message: !lambda return "This is a custom keyboard test reply message to "+ x.sender;
            keyboard: "[[{'text':'/help'},{'text':'/armstay'},{'text':'/disarm'}],[{'text':'/bypass'},{'text':'/status'},{'text':'/notify on'}]]"
            to: !lambda return x.chat_id;

    - command: "/inlinetest,/it"
      then:
       - telegram.publish:
            message: !lambda return "This is an inline keyboard test reply message to "+ x.sender;
            inline_keyboard: "[[{'text':'Help','callback_data':'/help'},{'text':'Arm Stay','callback_data':'/armstay'},{'text':'Disarm','callback_data':'/d'}],[{'text':'Bypass All','callback_data':'/b'},{'text':'Status','callback_data':'/s'},{'text':'Notify On','callback_data':'/notify on'}]]"
            to: !lambda return x.chat_id; 
            
    - command: "/removekb,/rkb"
      then:
        - telegram.publish:
              message: "keyboard removed"
              keyboard: "none" #send none instead of markup to delete a keyboard            
       
    - command: "/deletemessage,/dm"
      then:
        - telegram.delete_message:
            message_id: !lambda return x.message_id;
            chat_id: !lambda return x.chat_id;        

```

Sample telegram publish actions.  message=string or !lambda.
```
  - platform: template
    id: rdy_1
    name: "Ready"
    on_state: 
      then:
        - telegram.publish:
            message: "this is a test message to chat id 123466778" #message.  Can also be a lambda function.
            to: 123466778 #chat id to send to if not default. Optional. 
        - telegram.publish:

  - platform: template
    id: rdy_1
    name: "Ready"
    on_state: 
      then:
        - lambda: |-
             webnotify->publish("123466778","The ready light is: " + id(rdy_1).state); #send to a specific id
             webnotify->publish("The ready light is: " + id(rdy_1).state); #send to default chat id
       
```   
            
Switches to control bot and notify functions:              
```             
             
 switch:              
    name: enable Bot
    restore_mode: RESTORE_DEFAULT_ON    
    lambda: |-
      return webnotify->get_bot_status();
    turn_on_action:
      - lambda: |-
          webnotify->set_bot_enable(true);
    turn_off_action:
      - lambda: |-
          webnotify->set_bot_enable(false); 

  - platform: template
    name: enable Notify
    restore_mode: RESTORE_DEFAULT_ON    
    lambda: |-
      return webnotify->get_send_status();
    turn_on_action:
      - lambda: |-
          webnotify->set_send_enable(true);
    turn_off_action:
      - lambda: |-
          webnotify->set_send_enable(false);  
    
 ```   
Example below of using a text field to populate the chat_id field. You can then set it from the web or HA. The value will be stored in flash;
Use this entry for the telegram chat_id:
  chat_id: !lambda 'return id(chatidtext).state;'
```
text:
  - platform: template
    id: chatidtext
    name: "set_chat_id"
    mode: text
    initial_value: "xxxxxxxxxx"
    restore_value: true
    optimistic: true
    set_action: 
       - lambda: |-
          webnotify->set_chat_id(std::string(x));
          ESP_LOGD("test","set chat id to %s",std::string(x).c_str());
```
```
![telegram](https://github.com/Dilbert66/esphome-components/assets/7193213/e890cceb-b76f-42df-83b9-b78a5f160bb7)

# ESP32 web keypad component. 

This component provides a virtual keypad for each partition of your alarm system via a web page as well providing all sensor statuses. You can configure all buttons and keys using a config file. I’ve provided an example yaml file in my components directory. Just configure the yaml as usual for your system. There is an added section called web_keypad: for this new feature. 

You don’t need to configure anything by default. Just add the section as is also insuring that “web_keypad” is also listed in the external_components section above. All code and config files will be retrieved automatically from my repository and compiled in. You can modify the files to your liking after if needed.

What this accomplishes, is that you can now run this esphome component as a standalone system to provide a virtual keypad and monitor your panel without home assistant. You can disable the api and mqtt section if you want. You can also just enable mqtt and use it with a non home assistant infrastructure and still have a virtual keypad. As usual, this program is still in development so bugs most likely still lurk around. 

As an added bonus, the component also provides end to end encryption for all browser traffic using  AES256 encryption with SHA256HMAC message authentication.  The user name and passwords  are used to create a unique key for the encryption.  The same key is also used for authentication.   Alternatively, you can use simple digest authentication (no data encryption) if that's all you need.  Why use AES, well using standard TLS based https encryption is way too resource hungry on embedded devices. It's overkill for this application as well.

Please note that this app is using the mongoose.ws web application library instead of the ESPasyncwebserver.   I was having way too many heap crashes and instabilities with it.  Also , it's memory footprint was way too big.  The moogoose library is small and fast as well as being non-blocking.
```

external_components:
  - source: github://Dilbert66/esphome-components@dev #uncomment to use github repository version
  #- source:  my_components #uncomment to use alocal directory  called "my_components"
    components: [web_keypad] #components to load
    refresh: 10min  #frequency to check for new versions


web_keypad:
  port: 80  
  partitions: 1
  log: false
  auth:
    username: test
    password: test
    encryption: true
  config_url: https://dilbert66.github.io/config_files/config_vista.yml
  js_url: https://dilbert66.github.io/js_files/www.js
  #js_local: ./www.js    
  #config_local: ./config_vista.yml  

```
This project is licensed under the `Lesser General Public License` version `2.1`, or (at your option) any later version as per it's use of other libraries and code. Please see `COPYING.LESSER` for more information.

![vistaalarm](https://github.com/Dilbert66/esphome-components/assets/7193213/047c1fdb-1d90-4c14-8585-87309310d2bc)

If you like this project and wish to supplement my coffee intake, please click the button below to donate! Thank you!

[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/Dilbert66)
