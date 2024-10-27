#pragma once

#include "Arduino.h"
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

// Used to read bits on F7 message
#define BIT_MASK_BYTE1_BEEP 0x07
#define BIT_MASK_BYTE1_NIGHT 0x10

#define BIT_MASK_BYTE2_ARMED_HOME 0x80
#define BIT_MASK_BYTE2_LOW_BAT 0x40
#define BIT_MASK_BYTE2_ALARM_ZONE 0x20
#define BIT_MASK_BYTE2_READY 0x10
#define BIT_MASK_BYTE2_AC_LOSS 0x08
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
#define F8_MESSAGE_LENGTH 7
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
        int zone;
        uint8_t user;
        uint8_t partition;
    } lrr;
};

struct expanderType
{
    char expansionAddr;
    char expFault;
    char expFaultBits;
    char relayState;
};
const expanderType expanderType_INIT = {.expansionAddr = 0, .expFault = 0, .expFaultBits = 0, .relayState = 0};

struct keyType
{
    char key;
    uint8_t kpaddr;
    bool direct;
    uint8_t count;
    uint8_t seq;
};
const keyType keyType_INIT = {.key = 0, .kpaddr = 0, .direct = false, .count = 0,. seq = 0};

struct cmdQueueItem
{
    char cbuf[CMDBUFSIZE];
    char extcmd[OUTBUFSIZE];
    bool newCmd;
    bool newExtCmd;
    struct statusFlagType statusFlags;
};
const cmdQueueItem cmdQueueItem_INIT = {.newCmd = false, .newExtCmd = false};

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
    bool dataReceived;
    void IRAM_ATTR rxHandleISR(), txHandleISR();
    bool areEqual(char *, char *, uint8_t);
    bool connected;
    int toDec(int);
    void resetStatus();
    void initSerialHandlers(int, int, int);
    char *cbuf, *extbuf, *extcmd;

    bool lrrSupervisor;
    char expansionAddr;
    void setExpFault(int, bool);
    bool newExtCmd, newCmd;
    bool filterOwnTx;
    expanderType zoneExpanders[MAX_MODULES];
    char b; // used in isr
    bool charAvail();
    bool cmdAvail();
    cmdQueueItem getNextCmd();
    bool sendPending();
    // std::queue<struct cmdQueueItem> cmdQueue;

private:
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
    uint8_t *faultQueue;
    void setNextFault(uint8_t);
    expanderType getNextFault();
    expanderType peekNextFault();
    expanderType currentFault;
    uint8_t idx, outFaultIdx, inFaultIdx;
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
    keyType getChar();
    uint8_t peekNextKpAddr();
    uint8_t writeSeq, expSeq;
    char expZone;
    char haveExpMessage;
    char expFault, expBitAddr;
    char expFaultBits;
    bool decodePacket();
    uint8_t getExtBytes();
    volatile bool is2400;
    void pushCmdQueueItem(uint8_t cmdbufsize = CMDBUFSIZE, uint8_t outbufsize = OUTBUFSIZE);
    bool invertRead;

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
};
