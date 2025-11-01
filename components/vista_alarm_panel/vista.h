#pragma once

#if not defined(USE_ESP_IDF)
#include "Arduino.h"
#else
#define ESP32
#endif

#include <queue>
#include "ECPSoftwareSerial.h"


// #define DEBUG

#define MONITORTX

#define OUTBUFSIZE 30
#define CMDBUFSIZE 50
#ifdef ESP32
#define CMDQUEUESIZE 5
#else
#define CMDQUEUESIZE 2
#endif
#define FAULTQUEUESIZE 5
#define LRRADDR 3

// Used to read bits on F7 message
#define BIT_MASK_BYTE1_BEEP 0x07
#define BIT_MASK_BYTE1_NIGHT 0x10
//F7 00 00 51 10 21 00 - 50 28 - 02 00 00 4C //fault lowbat
//F7 00 00 07 10 16 00 - 12 28 - 02 00 00 20 //check co
//F7 00 00 07 10 03 00 - 00 28 - 02 00 00 46 //fault open
//F7 00 00 40 00 08 00 - 5C 28 - 02 00 00 33 //system ready low bat
//F7 00 00 20 00 08 00 - 4C 28 - 02 00 00 53 /system not ready low bat
//F7 00 00 03 10 08 00 - CC 28 - 02 00 00 31 // system arming stay
//F7 00 00 03 10 08 00 - CC 28 - 02 00 00 31 // system armed stay
//F7 00 00 40 00 BF 00 - 12 28 - 02 00 00 43 //system check 103 ready
//F7 00 00 20 00 BF 04 - 02 28 - 02 00 00 43 //system check 103 not ready
//F7 00 00 20 00 04 01 - 50 38 - 02 00 00 42 // zone bypass
//F7 00 00 03 10 17 00 - 80 2B - 02 00 00 41 // alarm zone 17 , in alarm
//F7 00 00 02 00 08 00 - 8C 28 - 02 00 00 44 //entry when armed
//F7 00 00 03 10 17 00 - 00 2A - 02 00 00 41 // alarm zone 17 , cleared, disarmed
//F7 00 00 03 10 EA 00 - 00 2A - 02 00 00 45 // exit alarm
//F7 00 00 03 10 EA 00 - 00 2A - 02 00 00 45 //alarm cancelled
//F7 00 00 07 10 12 00 - 80 08 - 02 00 00 41  //armed stay countdown
#define BIT_MASK_BYTE2_ARMED_HOME 0x80
#define BIT_MASK_BYTE2_LOW_BAT 0x40
#define BIT_MASK_BYTE2_ZONE_FIRE 0x20
#define BIT_MASK_BYTE2_READY 0x10
#define BIT_MASK_BYTE2_UNKNOWN 0x08
#define BIT_MASK_BYTE2_SYSTEM_FLAG 0x04
#define BIT_MASK_BYTE2_CHECK_FLAG 0x02
#define BIT_MASK_BYTE2_FIRE 0x01

#define BIT_MASK_BYTE3_INSTANT 0x80
#define BIT_MASK_BYTE3_PROGRAM 0x40
#define BIT_MASK_BYTE3_CHIME_MODE 0x20
#define BIT_MASK_BYTE3_BYPASS 0x10
#define BIT_MASK_BYTE3_AC_POWER 0x08
#define BIT_MASK_BYTE3_ARMED_AWAY 0x04
#define BIT_MASK_BYTE3_ZONE_ALARM 0x02
#define BIT_MASK_BYTE3_IN_ALARM 0x01

#define F7_MESSAGE_LENGTH 45
#define N98_MESSAGE_LENGTH 6

#define MAX_MODULES 9

// enum ecpState { sPulse, sNormal, sAckf7,sSendkpaddr,sPolling };
#define sPulse 1
#define sNormal 2
#define sAckf7 3
#define sSendkpaddr 4
#define sPolling 5
#define sCmdHigh 6

struct statusFlagType
{
    char beeps : 3;
    uint8_t armedStay : 1;
    uint8_t armedAway : 1;
    uint8_t night : 1;
    uint8_t instant : 1;
    uint8_t chime : 1;
    uint8_t acPower : 1;
    uint8_t acLoss : 1;
    uint8_t ready : 1;
    uint8_t entryDelay : 1;
    uint8_t programMode : 1;
    uint8_t zoneBypass : 1;
    uint8_t zoneAlarm : 1;
    uint8_t alarm : 1;
    uint8_t check : 1;
    uint8_t systemFlag : 1;
    uint8_t lowBattery : 1;
    uint8_t systemTrouble : 1;
    uint8_t fire : 1;
    uint8_t fireZone : 1;
    uint8_t backlight : 1;
    uint8_t armed : 1;
    uint8_t away : 1;
    uint8_t bypass : 1;
    uint8_t inAlarm : 1;
    uint8_t noAlarm : 1;
    uint8_t exitDelay : 1;
    uint8_t cancel : 1;
    uint8_t fault : 1;
    uint8_t panicAlarm : 1;
    char keypad[4];
    int zone;
    char prompt1[18];
    char prompt2[18];
    char promptPos;
    uint8_t attempts = 10;
    struct
    {
        int code;
        uint8_t qual;
        int data;
        uint8_t partition;
    } lrr;
};

struct expanderType
{
    char expansionAddr;
    char expFault;
    char expFaultBits;
    char relayState;
    uint8_t idx;
};
const expanderType expanderType_INIT = {.expansionAddr = 0xFF,.expFault = 0, .expFaultBits = 0, .relayState = 0, .idx = 0};

struct keyType
{
    char key;
    uint8_t kpaddr;
    bool direct;
    uint8_t count;
    uint8_t seq;
};
const keyType keyType_INIT = {.key = 0, .kpaddr = 0, .direct = false, .count = 0, .seq = 0};

struct cmdQueueItem
{
    char cbuf[CMDBUFSIZE];
    char extbuf[CMDBUFSIZE];
    bool newCmd;
    bool newExtCmd;
    size_t size;
    size_t rawsize;
    struct statusFlagType statusFlags;
};
struct rfSerialQueueItem
{
  uint8_t fault;
  uint32_t serial; 
  uint8_t idx; 
};
//const cmdQueueItem cmdQueueItem_INIT = {.newCmd = false, .newExtCmd = false,.size=0,.rawsize=0};

class Vista
{

public:
    Vista();
    ~Vista();
    void begin(int receivePin, int transmitPin, char keypadAddr, int monitorTxPin, bool invertRx = true, bool invertTx = true, bool invertMon = true, uint8_t inputRx = INPUT, uint8_t inputMon = INPUT);
    void stop();
    bool handle();
    void printStatus();
    void printTrouble();
    void decodeBeeps();
    void decodeKeypads();
    void printPacket(char *, int);
    void write(const char *);
    void write(const char);
    void write(const char *, uint8_t addr);
    void write(const char, uint8_t addr);
    void writeDirect(const char *keys, uint8_t addr, size_t len);
    void writeDirect(const char key, uint8_t addr, uint8_t seq = 0);
    statusFlagType statusFlags;
    SoftwareSerial *vistaSerial, *vistaSerialMonitor;
    void setKpAddr(char keypadAddr)
    {
        if (keypadAddr > 0)
            kpAddr = keypadAddr;
    }
    void addModule(uint8_t addr);
    bool dataReceived;
    void gpioISRHandler();
    void rxHandleISR();

    void txHandleISR();
    bool areEqual(char *, char *, uint8_t);
    bool keybusConnected, connected;
    int toDec(int);
    void resetStatus();
    void initSerialHandlers(int, int, int);
    char *cbuf, *extbuf, *extcmd;

    bool lrrSupervisor;
    void setExpFault(int, bool);
    void setRFFault(uint8_t fault,uint32_t serial);
    bool newExtCmd, newCmd;
    bool filterOwnTx;
    expanderType zoneExpanders[MAX_MODULES];
    uint8_t moduleIdx;
    char b; // used in isr
    bool charAvail();
    bool cmdAvail();
    cmdQueueItem * getNextCmd();
    bool sendPending();
    void set_rf_emulation(bool emulate);
    void set_rf_addr(uint8_t addr);
    bool get_rf_emulation();
    // std::queue<struct cmdQueueItem> cmdQueue;

private:
    char lcbuf[14];
    uint8_t _lcbuflen;
    uint8_t _retriesf9;
    char expectCmd;
    keyType *outbuf;
    char *tmpOutBuf;
    cmdQueueItem *cmdQueue;
    volatile uint8_t outbufIdx, inbufIdx;
    uint8_t outcmdIdx, incmdIdx;
    int rxPin, txPin;
    volatile char kpAddr;
    char monitorPin;
    volatile char rxState;
    volatile unsigned long lowTime, highTime;
    volatile bool pendingAck;
    uint8_t *faultQueue;
    rfSerialQueueItem *rfSerialQueue;
    void setNextFault(uint8_t);
    void setNextRfSerial(uint8_t fault,uint32_t serial);
    expanderType peekNextFault();
    expanderType getNextFault();
    rfSerialQueueItem peekNextRfSerial();
    rfSerialQueueItem getNextRfSerial();
    expanderType currentFault;
    uint8_t idx,outFaultIdx, inFaultIdx,rfSerialIdx,outRfSerialIdx,inRfSerialIdx;
    int gidx;
    volatile int extidx;
    uint8_t write_Seq;
    void onAUI(char *, int *);
    void onDisplay(char *, int *);
    void writeChars();
    volatile uint8_t markPulse;
    void readChars(int, char *, int *);
    bool validChksum(char *, int, int);
    void readChar(char *, int *);
    void onLrr(char *, int *);
    void onExp(char *);
    void onRF(char *);
    keyType getChar();
    uint8_t peekNextKpAddr();
    uint8_t writeSeq;
    char expZone;
    char haveExpMessage;
    char expFault, expBitAddr;
    char expFaultBits;
    size_t decodePacket();
    uint8_t getExtBytes();
    void sendBuffer(char *lcbuf,uint8_t lcbuflen);
    volatile bool is2400;
    void pushCmdQueueItem(size_t cmdsize=0,size_t rawsize=0);
    bool invertRead;
    bool disableRetries;

    char IRAM_ATTR addrToBitmask1(char addr)
    {
        if (addr > 7)
            return 0xFF;
        else
            return 0xFF ^ (0x01 << (addr));
    }
    char IRAM_ATTR addrToBitmask2(char addr)
    {
        if (addr < 8)
            return 0;
        else if (addr > 16)
            return 0xFF;
        else
            return 0xFF ^ (0x01 << (addr - 8));
    }
    char IRAM_ATTR addrToBitmask3(char addr)
    {
        if (addr < 16)
            return 0;
        else
            return 0xFF ^ (0x01 << (addr - 16));
    }

    void hw_wdt_disable();
    void hw_wdt_enable();

    char expectByte;
    volatile uint8_t retries;
    volatile uint8_t retryAddr;
    volatile bool sending;
    bool _emulate_rf_receiver=false;
    uint8_t _rf_addr=0;


#if defined (USE_ESP_IDF)

unsigned long IRAM_ATTR micros() {
  return (unsigned long)(esp_timer_get_time());
}

unsigned long millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
 }

#if not defined(NOP)
#define NOP __asm__ __volatile__ ("nop\n\t")
#endif
void IRAM_ATTR delayMicroseconds(uint32_t us) {
  uint64_t m = (uint64_t)esp_timer_get_time();
  if (us) {
    uint64_t e = (m + us);
    if (m > e) {  //overflow
      while ((uint64_t)esp_timer_get_time() > e) {
        NOP;
      }
    }
    while ((uint64_t)esp_timer_get_time() < e) {
      NOP;
    }
  }
}
 #endif
};
