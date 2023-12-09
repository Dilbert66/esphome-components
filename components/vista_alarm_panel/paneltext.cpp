 //EN
 
    //lookups for determining zone status as strings.  Must include complete word before zone#. No spaces. obsolete now. No longer needed. Using flags to identify type
    
    const char * FAULT = "FAULT";    
    const char * BYPAS = "BYPAS";
    const char * ALARM = "ALARM";
    const char * FIRE = "FIRE";
    const char * CHECK = "CHECK";
    const char * TRBL = "TRBL";    
    
    
    //Looks for the <space>*<space> found in the "Hit * to view messages".
    const char * HITSTAR = " * ";      
    
 //messages to display to home assistant

    const char * STATUS_ARMED = "armed_away";
    const char * STATUS_STAY = "armed_home";
    const char * STATUS_NIGHT = "armed_night";
    const char * STATUS_OFF = "disarmed";
    const char * STATUS_ONLINE = "online";
    const char * STATUS_OFFLINE = "offline";
    const char * STATUS_TRIGGERED = "triggered";
    const char * STATUS_READY = "ready";
      
    //the default ha alarm panel card likes to see "unavailable" instead of not_ready when the system can't be armed
    //const char * STATUS_NOT_READY = "not_ready";
    const char * STATUS_NOT_READY = "unavailable";    
    const char * MSG_ZONE_BYPASS = "zone_bypass_entered";
    const char * MSG_ARMED_BYPASS = "armed_custom_bypass";
    const char * MSG_NO_ENTRY_DELAY = "no_entry_delay";
    const char * MSG_NONE = "no_messages";
    

    