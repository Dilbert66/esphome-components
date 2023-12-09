#pragma once
 //EN
 
    //lookups for determining zone status as strings.  Must include complete word before zone#. No spaces. obsolete now. No longer needed. Using flags to identify type
    
    extern const char * FAULT;    
    extern const char * BYPAS;
    extern const char * ALARM;
    extern const char * FIRE;
    extern const char * CHECK;
    extern const char * TRBL;    
    
    
    //Looks for the <space>*<space> found in the "Hit * to view messages".
    extern const char * HITSTAR;      
    
 //messages to display to home assistant

    extern const char * STATUS_ARMED;
    extern const char * STATUS_STAY;
    extern const char * STATUS_NIGHT ;
    extern const char * STATUS_OFF ;
    extern const char * STATUS_ONLINE;
    extern const char * STATUS_OFFLINE;
    extern const char * STATUS_TRIGGERED;
    extern const char * STATUS_READY;
      
    //the default ha alarm panel card likes to see "unavailable" instead of not_ready when the system can't be armed
    extern const char * STATUS_NOT_READY;
    extern const char * MSG_ZONE_BYPASS;
    extern const char * MSG_ARMED_BYPASS;
    extern const char * MSG_NO_ENTRY_DELAY;
    extern const char * MSG_NONE;
    

    