# Various custom components for the ESPHOME system

## Vista alarm system interface
Please see the main project documenation here for the Vista series panels:
https://github.com/Dilbert66/esphome-vistaECP

## DSC alarm system interface
Please see the main project documenation here for the DSC Powerseries panels:
https://github.com/Dilbert66/esphome-dsckeybus

# ESP32 Standalone Telegram Bot

This component provides a standalone telegram bot application for the ESP32 platform that gives the ability
to fully control an Esphome application from your telegram account.  You can send your Esphome application any commands you want as well as have it send any statuses back to you via a secure https connection to the telegram api, all remotely.  It does not require any outside application or home control software, completely residing on the chip. Due to the need to use TLS for communications to the Telegram API site, it will use about 46K of heap memory which should not be a problem with the ESP32.  For performance and stability reasons, I've used the mongoose.ws web library as the base for all http handling.  This library provides an event driven, non-blocking API for creating various TCP apps.  I initially chose it to replace the ESPasyncwebserver library that was used on the web_server component since I was encountering too many heap crashes during heavy traffic.  I have been very pleased with it's minimal memory footprint and reliability. 

This component is part of a complete system I use to create standalone alarm control applications using ESP32 hardware.  Why have a telegram bot on the ESP ? Not everyone wants to have a separate home control system or only needs one ESP device to manage something.  For instance, for someone who only wants to control their legacy alarm system remotely , they do it all on one chip with no direct web access needed.   This bot uses a technique called long polling to wait for messages on the telegram api.  It will create a connection to their server and wait for up to 2 minutes for new messages.  This is done in a non-blocking event driven loop, so it will not impact any other running components on the system. When a status update needs to go out from the ESP, the software will terminate, the long poll loop, send the message and re-implement the long poll.


I will document each option later but for now, I'm providing a fairly complete example below that I use on my vista20 alarm system.  If you have not created a telegram bot before, I've listed the steps below. You can get further details if you need but doing a search on the web or on telegram's site.  Basically what you need is a bot_id (token) to use for connecting to the telegram api and receive and send cmds.  You also need a chat_id or group chat_id, to send notifications back to.   The example below basically creates a menu of cmds that the user can use to send cmds to the alarm system or receive system statuses back. Your application will be different.

You will first need a fully working yaml config.  The options below will need to be added to that config.  The implementation uses the esphome "custom components" feature which allows the yaml config to download the external source code as needed.  You can also copy the source directly to a folder on your system if you prefer.  The example below as shown, will download the "telegram_bot" and "mg_lib" components from my github page and compile them on your system when you do your local install/compile.  I've also provided two switch configurations that you can use to switch the bot and notify functions off or on from Home assistant or from the web_server application.

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
   a. Add @getidsbot to the group
   b. Get the ID from the id: field under "This Chat"
   c. copy the group chat ID to the chat_id: field or to the allowed_chat_ids: field.
   
Make sure to add all other allowed user or group chat ids to the allowed_chat_ids: field. 

## YAML config 
Add the following sections to your chose yaml configuration.
Custom components. You can use the github release version  or 
Copy the source directly to directory "my_components" under your esphome directory
The example below shows a sample bot config showing how to create a menu of cmd options.
```
external_components:
  - source: github://Dilbert66/esphome-components@dev #uncomment to use github repository version
  #- source:  my_components #uncomment to use alocal directory  called "my_components"
    components: [telegram_bot,mg_lib] #components to load
    refresh: 10min  #frequency to check for new versions

mg_lib:   

telegram_bot:
  id: webnotify
  bot_id: "123467890:SAMPLETELEGRAMBOTKEYSAMPLETELEGRAM"
  chat_id: "123455222" #default chat id to send to. Can also be a group id.
  bot_enable: true #if you dont need to have the receive bot set to false. Only publish action will be available.
  allowed_chat_ids:  #chat id's that are allowed to send cmds. Can also be group id's. Use "*" to allow all.
    - "12345522"
    - "-2255221"
  on_message:  #provides variables cmd, args , chat_id and text. <cmd> <args> , text is both. 
    - command: "*" #wildard captures all cmds
      then:
       - lambda: |-
           #sample code to look for cmds and act on them. Anything you want. Creates a cmd menu 
              #create some strings to hold the menu and system status. To be used by the cmds.
              std::string m="Help Menu\r\n";
              m.append("/disarm - disarm system\r\n");
              m.append("/help - this menu\r\n");
              m.append("/keys <args> - send cmds\r\n");
              m.append("/status - system status\r\n");
              m.append("/reboot - reboot esp\r\n");
              m.append("/notify <on|off> - enable send\r\n");
              m.append("/armstay - arm stay\r\n");
              m.append("/bypass - bypass all\r\n");
              
              std::string ss="System state: "+ id(ss_1).state;
              ss.append("\r\nZone state: "+id(zs).state);    
              
          if (cmd=="/disarm" || cmd=="/d") {
            $panelId->alarm_keypress_partition("${accesscode}1",1);
          } else if (cmd=="/help" || cmd=="/h") {
              webnotify->publish(chat_id,m);  
              webnotify->publish(chat_id,ss); 
          } else if (cmd=="/status" || cmd=="/s") {
               webnotify->publish(chat_id,ss);   
          } else if (cmd=="/keys" || cmd=="/k") {
              $panelId->alarm_keypress_partition(args,1);
          } else if (cmd=="/armstay" || cmd=="/a") {
              $panelId->alarm_keypress_partition("#${accesscode}2",1);
          } else if (cmd=="/notify")  {
            if (args=="on" ){
              webnotify->set_send_enable(true); 
              webnotify->publish("Notify is now on");
            } else
              webnotify->set_send_enable(false);
          } else if (cmd=="/reboot") {
              restart_switch->turn_on();
          } else if (cmd=="/bypass" || cmd=="/b") {
            $panelId->alarm_keypress_partition("${accesscode}6#",1);
          }
          
    #alternate way of handling specified cmds
    - command: "/test"
      then:
          id(ss2)->publish_state(args);    
```

Sample telegram publish action.  message=string or !lambda.
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
            message: "a message to the default chat id"
 ```
 
 Alternative example using a lambda function:
 ```
  - platform: template
    id: rdy_1
    name: "Ready"
    on_state: 
      then:
        - lambda: |-
             webnotify->publish("123466778","The ready light is: " + id(rdy_1).state); send to a specific id
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
          return webnotify->set_bot_enable(1);
    turn_off_action:
      - lambda: |-
          webnotify->set_bot_enable(0); 

  - platform: template
    name: enable Notify
    restore_mode: RESTORE_DEFAULT_ON    
    lambda: |-
      return webnotify->get_send_status();
    turn_on_action:
      - lambda: |-
          return webnotify->set_send_enable(1);
    turn_off_action:
      - lambda: |-
          webnotify->set_send_enable(0);      
 ```   
![telegram](https://github.com/Dilbert66/esphome-components/assets/7193213/e890cceb-b76f-42df-83b9-b78a5f160bb7)

# ESP32 web keypad component. 

This component provides a virtual keypad for each partition of your alarm system via a web page as well providing all sensor statuses. You can configure all buttons and keys using a config file. I’ve provided an example yaml file in my components directory. Just configure the yaml as usual for your system. There is an added section called web_keypad: for this new feature. 

You don’t need to configure anything by default. Just add the section as is also insuring that “web_keypad” is also listed in the external_components section above. All code and config files will be retrieved automatically from my repository and compiled in. You can modify the files to your liking after if needed.

What this accomplishes, is that you can now run this esphome component as a standalone system to provide a virtual keypad and monitor your panel without home assistant. You can disable the api and mqtt section if you want. You can also just enable mqtt and use it with a non home assistant infrastructure and still have a virtual keypad. As usual, this program is still in development so bugs most likely still lurk around. 

As an added bonus, the component also provides end to end encryption for all browser sent data such as cmds/keypad entries and on the receiving side will encrypt all display text fields. This should provide enough security so that access codes are not sent in the clear.  Logs are not encrypted so if using encryption, turn off the feature in the yaml.  The encryption used is AES256 with HMAC256.  The user name and passwords are used to create a unique key to use for the encryption.  It also uses the same key for authentication.   Alternatively, you can use simple digest authentication (no data encryption) if that's all you need.  Why use AES, well using standard TLS based https encryption is too resource hungry on embedded devices. It's overkill for this application as well.

Please note that this app is using the mongoose.ws web application library instead of the ESPasyncwebserver.   I was having way too many heap crashes and instabilities with it.  Also , it's memory footprint was way too big.  The moogoose library is small and fast as well as being non-blocking.
```

external_components:
  - source: github://Dilbert66/esphome-components@dev #uncomment to use github repository version
  #- source:  my_components #uncomment to use alocal directory  called "my_components"
    components: [web_keypad,mg_lib] #components to load
    refresh: 10min  #frequency to check for new versions

mg_lib: 

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

![vistaalarm](https://github.com/Dilbert66/esphome-components/assets/7193213/047c1fdb-1d90-4c14-8585-87309310d2bc)

If you like this project and wish to supplement my coffee intake, please click the button below to donate! Thank you!
[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/Dilbert66)
