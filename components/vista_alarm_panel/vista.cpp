#include "vista.h"

#if !defined(ARDUINO_MQTT)
#include "esphome/core/defines.h"
#endif

Vista *pointerToVistaClass;
#if defined(USE_ESP_IDF) or defined(ESP32)
void IRAM_ATTR rxISRHandler(void* args)
#else
void IRAM_ATTR rxISRHandler()
#endif
{                                     // define global handler
  pointerToVistaClass->rxHandleISR(); // calls class member handler
  
}



#ifdef MONITORTX
#if defined( USE_ESP_IDF ) or defined(ESP32)
void IRAM_ATTR txISRHandler(void* args)
#else
void  IRAM_ATTR txISRHandler()
#endif
{                                     // define global handler
  pointerToVistaClass->txHandleISR(); // calls class member handler
}
#endif

Vista::Vista()
{

#ifdef MONITORTX
  _extbuf = new char[OUTBUFSIZE];
  _extcmd = new char[OUTBUFSIZE];
#endif

  #ifdef VISTA_FILTER_OWN_TX
  _filterOwnTx=true;
  #else
  _filterOwnTx=false;
  #endif
  _inbufIdx = 0;
  _outbufIdx = 0;
  _incmdIdx = 0;
  _outcmdIdx = 0;
  _moduleIdx = 0;
  _rxState = sNormal;
  pointerToVistaClass = this;
  _cbuf = new char[CMDBUFSIZE];
  _outbuf = new keyType[CMDBUFSIZE];
  _tmpOutBuf = new char[CMDBUFSIZE];
  _cmdQueue = new cmdQueueItem[CMDQUEUESIZE];
  _faultQueue = new uint8_t[FAULTQUEUESIZE];
  _rfSerialQueue = new rfSerialQueueItem[FAULTQUEUESIZE];
  lrrSupervisor = false;

}

Vista::~Vista()
{
  free(vistaSerial);
  #if defined(USE_ESP_IDF) or defined(ESP32)
  gpio_intr_disable((gpio_num_t) _rxPin);
  gpio_isr_handler_remove((gpio_num_t) _rxPin);
#else
  detachInterrupt(_rxPin);
#endif
#ifdef MONITORTX
  if (vistaSerialMonitor)
  {
    free(vistaSerialMonitor);
    #if defined(USE_ESP_IDF) or defined(ESP32)
  gpio_intr_disable((gpio_num_t) _monitorPin);
  gpio_isr_handler_remove((gpio_num_t) _monitorPin);
#else
    detachInterrupt(_monitorPin);
#endif
  }
  delete[] _extbuf;
  delete[] _extcmd;
#endif
  delete[] _cbuf;
  delete[] _outbuf;
  delete[] _tmpOutBuf;
  delete[] _cmdQueue;
  delete[] _faultQueue;
  free(pointerToVistaClass);
}

expanderType Vista::peekNextFault()
{
  expanderType currentFault = expanderType_INIT;
  if (_inFaultIdx != _outFaultIdx) {
    currentFault=_zoneExpanders[_faultQueue[_outFaultIdx]];
    currentFault.idx=_outFaultIdx;
  }
  return currentFault;
}

expanderType Vista::getNextFault()
{
  expanderType currentFault=peekNextFault();
  if (currentFault.expansionAddr!=0xff)
    _outFaultIdx = (_outFaultIdx + 1) % FAULTQUEUESIZE;
  return currentFault;

}


rfSerialQueueItem Vista::peekNextRfSerial()
{
  rfSerialQueueItem q={0,0,0};
  if (_inRfSerialIdx != _outRfSerialIdx) {
    q = _rfSerialQueue[_outRfSerialIdx];
    q.idx=_outRfSerialIdx;
  }
  return  q;
}


rfSerialQueueItem Vista::getNextRfSerial()
{
  rfSerialQueueItem q = peekNextRfSerial();
  if (q.serial)
    _outRfSerialIdx = (_outRfSerialIdx + 1) % FAULTQUEUESIZE;
  return  q;
}

void Vista::setNextFault(uint8_t idx)
{
  _faultQueue[_inFaultIdx] = idx;
  _inFaultIdx = (_inFaultIdx + 1) % FAULTQUEUESIZE;
}

void Vista::setNextRfSerial(uint8_t fault, uint32_t serial)
{
  rfSerialQueueItem q;
  q.fault=fault;
  q.serial=serial;
  _rfSerialQueue[_inRfSerialIdx] = q;
  _inRfSerialIdx = (_inRfSerialIdx + 1) % FAULTQUEUESIZE;
}

void Vista::readChars(int ct, char buf[], int *idx)
{

  int x = 0;
  int idxval = *idx;
  unsigned long timeout = millis();
  while (x < ct && millis() - timeout < 20)
  {
    if (vistaSerial->available())
    {
      timeout = millis();
      buf[idxval++] = vistaSerial->read();
      x++;
    }
#ifdef ESP32 
    else
      vTaskDelay(5);
#else
    else
     delayMicroseconds(4);
#endif
  }
  *idx = idxval;
}

void Vista::onAUI(char cbuf[], int *idx)
{

  // byte 2 is length of message
  // byte 3 is length of headers
  // last byte of headers is counter
  // remaining bytes are body
  return; // no need for to process these for now until more info is gathered
  // F2 messages with 18 bytes or less don't seem to have
  //  any important information
  if (18 >= (uint8_t)cbuf[1])
  {
    return;
  }

  // 19th spot seems to be a decimal value
  // 01 is disarmed
  // 02 is armed
  // 03 is disarmed chime
  // short armed = 2 == (uint8_t) cbuf[19];
  statusFlags.armed = (0x02 & cbuf[19]) && !(cbuf[19] & 0x01);
  // 20th spot is away / stay
  //  this bit is really confusing
  //  it clearly switches to 2 when you set away mode
  //  but it is also 0x02 when an alarm is canceled,
  //  but not cleared - even if you are in stay mode.
  // 11th  byte in status body

  statusFlags.away = ((0x02 & cbuf[20]) > 0);

  statusFlags.zoneBypass = ((0x02 & cbuf[21]) > 0);
  // 21st spot is for bypass
  // 12th byte in status_body
  // short bypass = 0x02 & status_body[12];

  // 22nd spot is for alarm types
  // 1 is no alarm
  // 2 is ignore faults (like exit delay)
  // 4 is a alarm
  // 6 is a fault that does not cause an alarm
  // 8 is for panic alarm.

  statusFlags.noAlarm = ((cbuf[22] & 0x01) > 0);
  statusFlags.exitDelay = ((cbuf[22] & 0x02) > 0);
  statusFlags.fault = ((cbuf[22] & 0x04) > 0);
  statusFlags.panicAlarm = ((cbuf[22] & 0x08) > 0);

  if (statusFlags.armed && statusFlags.fault && !statusFlags.exitDelay)
  {
    statusFlags.alarm = 1;
  }
  else if (!statusFlags.armed && statusFlags.fault && !statusFlags.exitDelay)
  {
    statusFlags.alarm = 0;
  }
}

void Vista::onDisplay(char cbuf[], int *idx)
{
  // Byte 0 = F7
  // Bytes 1,2,3,4 are the device address masks
  // 5th byte represents zone
  // 6th beeps/night mode
  // 7th various system statuses
  // 8th various system statuses
  // 9th byte Programming mode = 0x01
  // 10th byte prompt position in the display message of the expected input

  statusFlags.keypad[0] = cbuf[1]; // 0 to 7

  statusFlags.keypad[1] = cbuf[2]; // 8 to 15

  statusFlags.keypad[2] = cbuf[3]; // 16 - 23

  statusFlags.keypad[3] = cbuf[4]; // 24 - 31.

  statusFlags.zone = (int)toDec(cbuf[5]);

  statusFlags.beeps = cbuf[6] & BIT_MASK_BYTE1_BEEP;

  statusFlags.fire = ((cbuf[7] & BIT_MASK_BYTE2_FIRE) > 0);
  statusFlags.systemFlag = ((cbuf[7] & BIT_MASK_BYTE2_SYSTEM_FLAG) > 0);
  statusFlags.ready = ((cbuf[7] & BIT_MASK_BYTE2_READY) > 0);

  statusFlags.night = ((cbuf[6] & BIT_MASK_BYTE1_NIGHT) > 0);
  statusFlags.armedStay = ((cbuf[7] & BIT_MASK_BYTE2_ARMED_HOME) > 0);

  statusFlags.lowBattery = ((cbuf[7] & BIT_MASK_BYTE2_LOW_BAT) > 0);
  // statusFlags.acLoss = ((cbuf[7] & BIT_MASK_BYTE2_UNKNOWN) > 0);

  statusFlags.check = ((cbuf[7] & BIT_MASK_BYTE2_CHECK_FLAG) > 0);
  statusFlags.fireZone = ((cbuf[7] & BIT_MASK_BYTE2_ZONE_FIRE) > 0);

  statusFlags.inAlarm = ((cbuf[8] & BIT_MASK_BYTE3_IN_ALARM) > 0);
  statusFlags.acPower = ((cbuf[8] & BIT_MASK_BYTE3_AC_POWER) > 0);
  statusFlags.chime = ((cbuf[8] & BIT_MASK_BYTE3_CHIME_MODE) > 0);
  statusFlags.bypass = ((cbuf[8] & BIT_MASK_BYTE3_BYPASS) > 0);
  statusFlags.programMode = (cbuf[8] & BIT_MASK_BYTE3_PROGRAM);

  statusFlags.instant = ((cbuf[8] & BIT_MASK_BYTE3_INSTANT) > 0);
  statusFlags.armedAway = ((cbuf[8] & BIT_MASK_BYTE3_ARMED_AWAY) > 0);

  if (!statusFlags.systemFlag)
  {
    statusFlags.alarm = ((cbuf[8] & BIT_MASK_BYTE3_ZONE_ALARM) > 0);
  }
  /*
  if (!statusFlags.inAlarm && (statusFlags.fireZone || statusFlags.alarm) )
           statusFlags.cancel=true;
   else
      statusFlags.cancel=false;
  */

  statusFlags.promptPos = cbuf[10];

  int y = 0;
  statusFlags.backlight = ((cbuf[12] & 0x80) > 0);
  cbuf[12] = (cbuf[12] & 0x7F);
  memcpy(statusFlags.prompt1, &cbuf[12], 16);
  statusFlags.prompt1[16] = 0;
  memcpy(statusFlags.prompt2, &cbuf[28], 16);
  statusFlags.prompt2[16] = 0;
}

int Vista::toDec(int n)
{
  char b[4];
  char *p;
  itoa(n, b, 16);
  long int li = strtol(b, &p, 10);
  return (int)li;
}

cmdQueueItem * Vista::getNextCmd() // return index instead
{
  cmdQueueItem* c = NULL;
  if (_outcmdIdx != _incmdIdx) {
    c = &_cmdQueue[_outcmdIdx];
    _outcmdIdx = (_outcmdIdx + 1) % CMDQUEUESIZE;
  }
  return c;
}

bool Vista::cmdAvail()
{
  if (_outcmdIdx == _incmdIdx)
    return false;
  else
    return true;
}

void Vista::pushCmdQueueItem(size_t size, size_t rawsize)
{
  struct cmdQueueItem q;
  q.statusFlags = statusFlags;
  q.newCmd = _newCmd;
  q.newExtCmd = _newExtCmd;
  q.size = size;
  q.rawsize = rawsize;

  if (_newExtCmd)
  {
    for (uint8_t i = 0; i < size; i++)
    {
      q.cbuf[i] = _extcmd[i];
    }
    for (uint8_t i = 0; i < rawsize; i++)
    {
      q.extbuf[i] = _extbuf[i];
    }
  }
  else
  {
    for (uint8_t i = 0; i < size; i++)
    {
      q.cbuf[i] = _cbuf[i];
    }
  }
  _cmdQueue[_incmdIdx] = q;
  _incmdIdx = (_incmdIdx + 1) % CMDQUEUESIZE;
}


void Vista::onRF(char cbuf[])
{

  if (!_emulate_rf_receiver || _rf_addr > 30) return;


  uint8_t req_addr;
   switch (cbuf[1]) {
    case 1: req_addr=7;break;
    case 2: req_addr=0;break;
    case 4: req_addr=1;break;
    case 8: req_addr=2;break;
    case 0x10: req_addr=3;break;
    case 0x20: req_addr=4;break;
    case 0x40: req_addr=5;break;
    case 0x80: req_addr=6;break;
    default: return; //unknown
  }
  if (req_addr != _rf_addr) return; //not for this this device

  char type = cbuf[3];
  char seq = cbuf[2];
  char lcbuf[8];
  uint8_t lcbuflen;
  char expSeq = (seq == 0x20 ? 0x24 : 0x21); //get current alternating sequence number. Top nibble is byte count

  // 0xF1 - response to request
  if (type == 0xF1)
  {
    rfSerialQueueItem rfitem=peekNextRfSerial(); 
    if (!rfitem.serial ) return; //no pending request 
    getNextRfSerial(); // clear this request as we are now processing it
    expanderType currentFault = peekNextFault(); 
    if (currentFault.expansionAddr==_rf_addr)
      getNextFault(); //clear this request as it's from an rf request

    uint8_t serial[4];
    *(uint32_t*)&serial = rfitem.serial; //convert from int to byte array
    lcbuflen=6;
    lcbuf[0]=_rf_addr; //address
    lcbuf[1]=0x50 | (expSeq & 0x0f);
    lcbuf[2]=serial[2] | 0x80; //unknown what 0x80 bit is for 
    lcbuf[3]=serial[1];
    lcbuf[4]=serial[0];
    lcbuf[5]=rfitem.fault;

    rfitem=peekNextRfSerial();

    if (rfitem.serial) 
      setNextFault(rfitem.idx); // push to the pending queue as we have another pending fault
  }

  else if (  (type == 0x81 || type==0x82 || type==0x60 || type == 0x40)) { //wireless device supervision query 
    lcbuflen = 3;
    lcbuf[0] = _rf_addr;
    lcbuf[1] = expSeq;
    lcbuf[2] = type==0x82?5:0;     //5881enh id is 5 
  } else
  {
    return; // unknown so we don't acknowledge  
  }

  ckSumSendBuffer(lcbuf,lcbuflen);

}


/**
 * Typical packet
 * positions 8 and 9 hold the report code
 * in Ademco Contact ID format
 * the lower 4 bits of 8 and both bites of 9
 * ["F9","43","0B","58","80","FF","FF","18","14","06","01","00","20","90"]}
 *
 * It seems that, for trouble indicators (0x300 range) the qualifier is flipped
 * where 1 means "new" and 3 means "restored"
 * 0x48 is a startup sequence, the byte after 0x48 will be 00 01 02 03
 //1=new event or opening, 3=new restore or closing,6=previous still present
 */
void Vista::onLrr(char *cbuf, int *idx)
{
  //response to a f9 resend. Send back last message
  if (_retriesf9 > 0 && _retriesf9 < 4 && lrrSupervisor)
  {
    if (peekNextKpAddr() == LRRADDR)
      getChar(); //remove last request
    sendBuffer(_lcbuf,_lcbuflen);
    _expectByte = _lcbuf[0];
    _expectCmd = 0xf9;
    _retriesf9++;
    return;

  }

  _retriesf9 = 0;
  int len = cbuf[2];
  _lcbuflen = 0;

  if (len == 0)
    return;
  char type = cbuf[3];

  // 0x52 means respond with only cycle message
  // 0x48 means same thing
  //, i think 0x52 and and 0x48 are the same
  if (type == (char)0x52 || type == (char)0x48)
  {
    _lcbuf[0] = (char)cbuf[1];
    _lcbuflen++;
  }
  else if (type == (char)0x58)
  {
    // just respond, but 0x58s have lots of info
    int c = (((0x0f & cbuf[8]) << 8) | cbuf[9]);
    c = toDec(c); // convert to decimal representation for correct code display
    statusFlags.lrr.qual = (uint8_t)(0xf0 & cbuf[8]) >> 4;
    statusFlags.lrr.code = c;
    statusFlags.lrr.data = toDec(((uint8_t)cbuf[12] >> 4) | ((uint8_t)cbuf[11] << 4));
    statusFlags.lrr.partition = (uint8_t)cbuf[10];

    _lcbuf[0] = (char)(cbuf[1]);
    _lcbuflen++;
  }
  else if (type == (char)0x53)
  {
    _lcbuf[0] = (char)((cbuf[1] + 0x40) & 0xFF);
    _lcbuf[1] = (char)0x04;
    _lcbuf[2] = (char)0x00;
    _lcbuf[3] = (char)0x00;
    // 0x08 is sent if we're in test mode
    // 0x0a after a test
    // 0x04 if you have network problems?
    // 0x06 if you have network problems?
    _lcbuf[4] = (char)0x00;
    if (lrrSupervisor) {
      _lcbuflen = 5;
      _expectByte = _lcbuf[0];
       _expectCmd = 0xf9;
      _retriesf9++;
    }
  }

  // we don't need a checksum for 1 byte messages (no length bit)
  // if we don't even have a message length byte, then we are just
  //  ACKing a cycle header byte.
  if (_lcbuflen >= 2 && lrrSupervisor)
  {
    uint32_t chksum = 0;
    for (int x = 0; x < _lcbuflen; x++)
    {
      chksum += _lcbuf[x];
    }
    chksum = (chksum -1) ^ 0xFF;
    _lcbuf[_lcbuflen] = (char)chksum;
    _lcbuflen++;
  }

  if (lrrSupervisor)
  {
    sendBuffer(_lcbuf,_lcbuflen);
  }
}

  void Vista::set_rf_emulation(bool emulate){
        _emulate_rf_receiver=emulate;
  }

  void Vista::set_rf_addr(uint8_t addr){
        _rf_addr=addr;
  }

  bool Vista::get_rf_emulation() {
      return _emulate_rf_receiver;
  }

// add new expander modules and init zone fields
void Vista::addModule(uint8_t addr)
{
  if (addr > 30)
    return;
  for (uint8_t x=0;x < _moduleIdx;x++){
    if (addr==_zoneExpanders[x].expansionAddr) return;
  }
  if (_moduleIdx < MAX_MODULES)
  {
    _zoneExpanders[_moduleIdx] = expanderType_INIT;
    _zoneExpanders[_moduleIdx].expansionAddr = addr;
    _moduleIdx++;
  }
}

void Vista::setRFFault(uint8_t fault,uint32_t serial)
{
  if (!_emulate_rf_receiver || _rf_addr > 30) return;
  uint8_t idx;
  for (idx = 0; idx < _moduleIdx; idx++)
  {
    if (_zoneExpanders[idx].expansionAddr == _rf_addr)
      break;
  }
  if (idx == _moduleIdx)
    return;
  setNextRfSerial(fault,serial);
  setNextFault(idx); // push to the pending queue
                                                       
}

void Vista::setExpFault(int zone, bool fault)
{
  // expander address 7 - zones: 9 - 16
  // expander address 8 - zones:  17 - 24
  // expander address 9 - zones: 25 - 32
  // expander address 10 - zones: 33 - 40
  // expander address 11 - zones: 41 - 48
  uint8_t addr = 0;
  if (zone > 8 && zone < 17)
  {
    addr = 7;
  }
  else if (zone > 16 && zone < 25)
  {
    addr = 8;
  }
  else if (zone > 24 && zone < 33)
  {
    addr = 9;
  }
  else if (zone > 32 && zone < 41)
  {
    addr = 10;
  }
  else if (zone > 40 && zone < 49)
  {
    addr = 11;
  }
  else
    return;

  uint8_t idx;
  for (idx = 0; idx < _moduleIdx; idx++)
  {
    if (_zoneExpanders[idx].expansionAddr == addr)
      break;
  }

  if (idx == _moduleIdx)
    return;

  _expFaultBits = _zoneExpanders[idx].expFaultBits;

  int z = zone & 0x07;                                                            // convert zone to range of 1 - 7,0 (last zone is 0)
  _expFault = z << 5 | (fault ? 0x8 : 0);                                          // 0 = terminated(eol resistor), 0x08=open, 0x10 = closed (shorted)  - convert to bitfield for F1 response
  z = (zone - 1) & 0x07;                                                          // now convert to 0 - 7 for F7 poll response
  _expFaultBits = fault ? _expFaultBits | (0x80 >> z) : _expFaultBits ^ (0x80 >> z); // setup bit fields for return response with fault values for each zone
  expanderType lastFault = peekNextFault();
  if (lastFault.expansionAddr != _zoneExpanders[idx].expansionAddr || lastFault.expFault != _expFault || lastFault.expFaultBits != _expFaultBits)
  {
    _zoneExpanders[idx].expFault = _expFault;
    _zoneExpanders[idx].expFaultBits = _expFaultBits;
    setNextFault(idx); // push to the pending queue
  }
}



// 98 2E 02 20 F7 EC
// 98 2E 04 20 F7 EA
// 98 2E 01 04 25 F1 EA - relay address 14 and 15  have an extra byte . byte 2 is flag and shifts other bytes right
// 98 2E 40 35 00 01 xx - relay cmd 00 has the extra cmd data byte
//  byte 3 is the binary position encoded expander addresss 02=107, 04=108,08=109, etc
// byte 4 is a seq byte, changes from 20 to 25 every request sequence
// byte 5 F7 for poll, F1 for a msg request, 80 for a resend request
//  The 98 serial data has some inconsistencies with some bit durations on byte 1. Might be an issue with my panel.  Be interested to know how it looks on another panel.  The checksum does not work as a two's complement so something is off. if instead of 0x2e we use 0x63, it works.  Either way, the bytes we need work fine for our purposes.

void Vista::onExp(char cbuf[])
{

  char type = cbuf[4];
  char seq = cbuf[3];
  char lcbuf[6];

  char expansionAddr = 0;
  uint8_t idx;

  if (cbuf[2] & 1)
  {
    seq = cbuf[4];
    type = cbuf[5];
    for (idx = 0; idx < _moduleIdx; idx++)
    {
      expansionAddr = _zoneExpanders[idx].expansionAddr;
      if (cbuf[2] == (0x01 << (expansionAddr - 13)))
        break; // for relay addresses 14-15
    }
  }
  else
  {
    for (idx = 0; idx < _moduleIdx; idx++)
    {
      expansionAddr = _zoneExpanders[idx].expansionAddr;
      if (cbuf[2] == (0x01 << (expansionAddr - 6)))
        break; // for address range 7 -13
    }
  }

  if (idx == _moduleIdx)
  {
    return; // no match return
  }
  _expFaultBits = _zoneExpanders[idx].expFaultBits;

  uint8_t lcbuflen = 0;
  char expSeq = (seq == 0x20 ? 0x34 : 0x31);

  // we use zone to either | or & bits depending if in fault or reset
  // 0xF1 - response to request, 0xf7 - poll, 0x80 - retry ,0x00 relay control
  if (type == 0xF1)
  {

    expanderType currentFault = peekNextFault(); // check next item. Don't pop it yet
    if (currentFault.expansionAddr && expansionAddr == currentFault.expansionAddr)
    {
      getNextFault(); // pop it from the queue since we are processing it now
    }
    else
      currentFault = _zoneExpanders[idx]; // no pending fault, use current fault data instead
    lcbuflen = 4;
    lcbuf[0] = (char)currentFault.expansionAddr;
    lcbuf[1] = expSeq;
    // lcbuf[2] = (char) currentFault.relayState;
    lcbuf[2] = 0;
    lcbuf[3] = (char)currentFault.expFault; // we send out the current zone state
  }
  else if (type == 0xF7)
  { // periodic  zone state poll (every 30 seconds) expander
    lcbuflen = 4;
    lcbuf[0] = (char)0xF0;
    lcbuf[1] = expSeq;
    // lcbuf[2]= (char) _expFaultBits ^ 0xFF; //closed zones - opposite of expfaultbits. If set in byte3 we clear here. (not used )
    lcbuf[2] = 0;                  // we simulate having a termination resistor so set to zero for all zones
    lcbuf[3] = (char)_expFaultBits; // opens zones - we send out the list of zone states. if 0 in both fields, means terminated
  }
  else if (type == 0x00 || type == 0x0D)
  { // relay module
    lcbuflen = 4;
    lcbuf[0] = (char)expansionAddr;
    lcbuf[1] = expSeq;
    lcbuf[2] = (char)0x00;
    if (cbuf[2] & 1)
    { // address 14/15
      _zoneExpanders[idx].relayState = cbuf[6] & 0x80 ? _zoneExpanders[idx].relayState | (cbuf[6] & 0x7f) : _zoneExpanders[idx].relayState & ((cbuf[6] & 0x7f) ^ 0xFF);
      lcbuf[3] = (char)cbuf[6];
    }
    else
    {
      _zoneExpanders[idx].relayState = cbuf[5] & 0x80 ? _zoneExpanders[idx].relayState | (cbuf[5] & 0x7f) : _zoneExpanders[idx].relayState & ((cbuf[5] & 0x7f) ^ 0xFF);
      lcbuf[3] = (char)cbuf[5];
    }
  }
  else
  {
     return; // we don't acknowledge   //0x80 or 0x81
  }

  ckSumSendBuffer(lcbuf,lcbuflen);
}

    // int checksum = 0;
    // uint8_t x;
    // for (x = 2; x < _tmpOutBuf[1] + 1; x++)
    // {
    //   checksum += (char)_tmpOutBuf[x];
    // }
    // uint32_t chksum = 0x100 - (_tmpOutBuf[0] + _tmpOutBuf[1] + checksum);
    // _tmpOutBuf[x]=(char)chksum;

void Vista::ckSumSendBuffer(char *lcbuf,uint8_t lcbuflen) {
  uint32_t chksum = 0;
  for (uint8_t x = 0; x < lcbuflen; x++)
  {
    chksum += lcbuf[x];
  }
  lcbuf[lcbuflen]=(char)((chksum-1) ^ 0xFF);
  sendBuffer(lcbuf,lcbuflen+1);
 
}

void Vista::sendBuffer(char *lcbuf,uint8_t lcbuflen) {
  delayMicroseconds(500);
  if (_filterOwnTx)
    memset(_extbuf, 0, OUTBUFSIZE);
  for (uint8_t x = 0; x < lcbuflen; x++)
  {
    vistaSerial->write(lcbuf[x],_filterOwnTx);
    if (_filterOwnTx) 
      _extbuf[x]=lcbuf[x];
  }
  if (_filterOwnTx) {
    _extIdx=lcbuflen;
  }
}

void Vista::write(const char key, uint8_t addr)
{
  if (!addr || addr > 23)
    return;

  if ((key >= 0x30 && key <= 0x39) || key == 0x23 || key == 0x2a || key == '|' || (key >= 0x41 && key <= 0x44) || key == 0x46 || key == 0x4d || key == 0x50 || key == 0x47 || key == 0x74)
  {
    keyType kt;
    kt.key = key;
    kt.kpaddr = addr;
    kt.direct = false;
    kt.count = 0;
    kt.seq = 0;
    _outbuf[_inbufIdx] = kt;
    _inbufIdx = (_inbufIdx + 1) % CMDBUFSIZE;
  }
}

void Vista::writeDirect(const char key, uint8_t addr, uint8_t seq)
{
  if (!addr || addr > 23)
    return;

  keyType kt;
  kt.key = key;
  kt.kpaddr = addr;
  kt.direct = true;
  kt.count = 0;
  kt.seq = seq;
  _outbuf[_inbufIdx] = kt;
  _inbufIdx = (_inbufIdx + 1) % CMDBUFSIZE;
}

void Vista::write(const char key)
{
  write(key, _kpAddr);
}

void Vista::write(const char *receivedKeys)
{
  
  int x = 0;
  while (receivedKeys[x] != '\0')
  {
    write(receivedKeys[x], _kpAddr);
    x++;
  }
}

void Vista::writeDirect(const char *receivedKeys, uint8_t addr, size_t len)
{
  if (!addr || addr > 23)
    return;
  int x = 0;

  uint8_t seq = (((++_writeSeq) << 6) & 0xc0) | (addr & 0x3F); // so that we don't mix cmd sequences
  while (x < len)
  {
    writeDirect(receivedKeys[x++], addr, seq);
  }
}

void Vista::write(const char *receivedKeys, uint8_t addr)
{
  if (!addr || addr > 23)
   return;
  int x = 0;
  while (receivedKeys[x] != '\0')
  {
    write(receivedKeys[x], addr);
    x++;
  }
}

keyType Vista::getChar()
{
  keyType c = keyType_INIT;
  if (_outbufIdx == _inbufIdx)
    return c;
  c = _outbuf[_outbufIdx];
  _outbufIdx = (_outbufIdx + 1) % CMDBUFSIZE;
  return c;
}

uint8_t Vista::peekNextKpAddr()
{
  if (_outbufIdx == _inbufIdx)
    return 0;
  keyType c = _outbuf[_outbufIdx];
  return c.kpaddr;
}

bool Vista::sendPending()
{
  if (_outbufIdx == _inbufIdx && _retries == 0)
    return false;
  else
    return true;
}

bool Vista::charAvail()
{
  if (_outbufIdx == _inbufIdx)
    return false;
  else
    return true;
}

/**
 * Send 0-9, # and * characters
 * 1,2,3,4,5,6,7,8,9,0,#,*,
 * 31,32,33,34,35,36,37,38,39,30,23,2A,
 * 0011-0001,0011-0010,0011-0011,0011-0100,0011-0101,0011-0110,0011-0111,0011-1000,0011-1001,0011-0000,0010-0011,0010-1010,
 *
 */

void Vista::writeChars()
{

  if (!charAvail() && _retries == 0 && _expectByte)
    return;

  // if _retries are getting out of control with no successfull callback
  // just clear the queue
  if (_retries > 4)
  {
    _retries = 0;
    _retryAddr = 0;
    _expectByte = 0;
    _outbufIdx = _inbufIdx;
    return;
  }


  int tmpIdx = 0;
  // header is the bit mask YYXX-XXXX
  //   where YY is an incrementing sequence number
  //   and XX-XXXX is the keypad address (decimal 16-31)
  // vistaSerial->write(header);
  // vistaSerial->write(_outbufIdx +1);

  // adjust characters to hex values.
  // ASCII numbers get translated to hex numbers
  // # and * get converted to 0xA and 0xB
  //  send any other chars straight, although this will probably
  //  result in errors
  // 0xc = A (*/1), 0xd=B (*/#) , 0xe=C (3/#)

  if (_retries == 0)
  {

    keyType kt;
    char c;
    int sz = 0;
    tmpIdx = 2;
    uint8_t lastkpaddr = 0;
    uint8_t lastseq = 0;
    _retryAddr=0;
    while (charAvail() && sz < OUTBUFSIZE)
    {
      if (!(lastkpaddr == 0 || lastkpaddr == peekNextKpAddr()))
        break;
      if (!(lastseq == 0 || lastseq == _outbuf[_outbufIdx].seq))
        break;
      kt = getChar();
      if (!kt.kpaddr) break; //should not be needed
      c = kt.key;
      if (c == '|' && !kt.direct ) // break sequence and send the previous immediately
        break;

      lastkpaddr = kt.kpaddr;
      lastseq = kt.seq;
      _retryAddr = kt.kpaddr;
      sz++;
 
      if (!kt.direct)
      {
        // translate digits between 0-9 to hex/decimal
        if (c >= 0x30 && c <= 0x39)
        {
          c -= 0x30;
        }
        else
          // translate * to 0x0b
          if (c == 0x23)
          {
            c = 0x0B;
          }
          else
            // translate # to 0x0a
            if (c == 0x2A)
            {
              c = 0x0A;
            }
            else if (c == 0x46)
            { // zone 95 (F)
              c = 0x0C;
            }
            else if (c == 0x4D)
            { // zone 99 (M)
              c = 0x0D;
            }
            else if (c == 0x50)
            { // zone 96 (P)
              c = 0x0E;
            }
            else if (c == 0x47)
            { // zone 92 (G)
              c = 0x0F;
            }
            else
              // translate A to 0x1C (function key A)
              // translate B to 0x1D (function key B)
              // translate C to 0x1E (function key C)
              // translate D to 0x1F (function key D)
              if (c >= 0x41 && c <= 0x44)
              {
                c = c - 0x25;
              }
      }
      _tmpOutBuf[tmpIdx++] = c;

    }

    if (kt.seq > 0)
      _tmpOutBuf[0] = kt.seq;
    else
      _tmpOutBuf[0] = ((++_writeSeq << 6) & 0xc0) | (lastkpaddr & 0x3F);

    _tmpOutBuf[1] = sz + 1;

    int checksum = 0;
    uint8_t x;
    for (x = 0; x < _tmpOutBuf[1] + 1; x++)
    {
      checksum += (char)_tmpOutBuf[x];
    }
    _tmpOutBuf[x] = (char)((checksum-1) ^ 0xFF);
  }
  sendBuffer(_tmpOutBuf,_tmpOutBuf[1] + 2);
  _expectByte = _tmpOutBuf[0];
  _expectCmd = 0xf6;
  _retries++;

}

// void Vista::gpioISRHandler()
// {
//   /* Calc */
//   uint32_t gpio_num = 0;
//     uint32_t gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);   //read status to get interrupt status for GPIO0-31
//     uint32_t gpio_intr_status_h = READ_PERI_REG(GPIO_STATUS1_REG);//read status1 to get interrupt status for GPIO32-39
//     SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, gpio_intr_status);    //Clear intr for gpio0-gpio31
//     SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, gpio_intr_status_h); //Clear intr for gpio32-39
//     do {
//       if(gpio_num < 32) {
		  
		  			  
//         if(gpio_intr_status & BIT(gpio_num)) { //gpio0-gpio31
// 		      switch (gpio_intr_status & BIT(gpio_num)) {
			   
// 			    case _rxPin: rxHandleISR(); break;
			   
// 			    case _monitorPin: txHandleISR();break;
					   
// 		    } 

		   
// 		  }
//       } else {
//         if(gpio_intr_status_h & BIT(gpio_num - 32)) {
//           ets_printf("2 Intr GPIO%d, val : %d\n",gpio_num,gpio_get_level(gpio_num));
//           //This is an isr handler, you should post an event to process it in RTOS queue.
//         }
//       }
//   } while(++gpio_num < GPIO_PIN_COUNT);
  
//   /* push_status = ; */
//  }
 

void IRAM_ATTR Vista::rxHandleISR()
{
  static byte b;
  static uint8_t ackAddr;
  static uint8_t ackCount=0;
    #if defined(USE_ESP_IDF) or defined(ESP32)
  bool level=gpio_get_level((gpio_num_t) _rxPin);
  #else
  bool level=digitalRead(_rxPin);
  #endif
    if ( level == _invertRead)
  {
    if (_lowTime)
     _lowTime = micros() - _lowTime;

    _highTime = micros();


    if (_lowTime > 9000)
    {
      _markPulse = 2;

      ackAddr = _inFaultIdx == _outFaultIdx ? 0xFF : _zoneExpanders[_faultQueue[_outFaultIdx]].expansionAddr;

      if (ackAddr > 0 && ackAddr < 24)
      {
        vistaSerial->write(addrToBitmask1(ackAddr), false, 4800);
        b = addrToBitmask2(ackAddr);
        if (b)
          vistaSerial->write(b, false, 4800);
        b = addrToBitmask3(ackAddr);
        if (b)
          vistaSerial->write(b, false, 4800);
      }
      else if (_outbufIdx != _inbufIdx || _retries)
      {
        if (_pendingAck) { //we wait at least 2 cycles to wait for an F6 response and if not we cancel the pending ack and allow a new one to go through
          if (ackCount > 1) {
            _pendingAck=false;
            ackCount=0;
          } else
            ackCount++;
        }

        if (!_retries && _outbuf[_outbufIdx].count > 2)
        { // after x failed _retries to send, we remove this entry from the buffer
          ackAddr = _outbuf[_outbufIdx].kpaddr;
          _outbufIdx = (_outbufIdx + 1) % CMDBUFSIZE; // Not valid or no answer. Skip it.
          while (_outbufIdx != _inbufIdx && _outbuf[_outbufIdx].kpaddr == ackAddr)
            _outbufIdx = (_outbufIdx + 1) % CMDBUFSIZE; // skip any other entries with same address
        }

        if (!_pendingAck && (_outbufIdx != _inbufIdx || _retries))
        {
          ackAddr = _retries ? _retryAddr : _outbuf[_outbufIdx].kpaddr; // get pending keypad address
          if (!_retries)
            _outbuf[_outbufIdx].count++;
          
          if (ackAddr > 0 && ackAddr < 24) {
            vistaSerial->write(addrToBitmask1(ackAddr), false, 4800);
            b = addrToBitmask2(ackAddr);
            if (b)
              vistaSerial->write(b, false, 4800);
            b = addrToBitmask3(ackAddr);
            if (b)
              vistaSerial->write(b, false, 4800);
            _pendingAck=true;

          }
          
        }
      }
      _rxState = sPolling; // set flag to skip capturing pulses in the receive buffer during polling phase
    }
    else if (_lowTime > 4600 && _rxState == sPolling)
    { // 2400 baud cmd preamble
      _is2400 = true;
      _markPulse = 3;
      _rxState = sCmdHigh;
    }
    else if (_lowTime > 3000 && _rxState == sPolling)
    { // 4800 baud cmd preamble
      _is2400 = false;
      _markPulse = 4;
      _rxState = sNormal; // ok we have the message preamble. Lets start capturing receive bytes
    }

    _lowTime = 0;
  }
  else
  {

    if (_highTime && micros() - _highTime > 6000 && _rxState == sNormal) {

      _rxState = sPolling;
    }
    if (_rxState == sCmdHigh) // end 2400 baud cmd preamble
      _rxState = sNormal;

    _lowTime = micros();

    _highTime = 0;
  }
  if (_rxState == sNormal || _highTime == 0)
    vistaSerial->rxRead();
#ifdef ESP8266
  else // clear pending interrupts for this pin if any occur during transmission
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << _rxPin);
#endif
}

#ifdef MONITORTX
void IRAM_ATTR Vista::txHandleISR()
{
  //if ((!sending || !_filterOwnTx) && _rxState == sNormal)
  if (_rxState == sNormal)
    vistaSerialMonitor->rxRead();
}
#endif

bool Vista::validChksum(char cbuf[], int start, int len)
{
  uint16_t chksum = 0;
  for (uint8_t x = start; x < len; x++)
  {
    chksum += cbuf[x];
  }
  if (chksum % 256 == 0)
    return true;
  else
    return false;
}

#ifdef MONITORTX
size_t Vista::decodePacket()
{
  _newExtCmd = false;
  // format 0xFA deviceid subcommand channel on/off
  if (_extcmd[0] == 0xFA)
  {

    if (!validChksum(_extbuf, 0, 5))
    {
      _extcmd[0] = _extbuf[0];
      _extcmd[1] = _extbuf[1];
      _extcmd[2] = _extbuf[2];
      _extcmd[3] = _extbuf[3];
      _extcmd[4] = _extbuf[4];
      _extcmd[5] = _extbuf[5];
      _extcmd[6] = _extbuf[6];
      _extcmd[12] = 0x77; // flag to identify chksum error
      _newExtCmd = true;
      return 13; // for debugging return what was sent so we can see why the chcksum failed
    }

    char cmdtype = (_extcmd[2] & 1) ? _extcmd[5] : _extcmd[4];

    if (_extcmd[2] & 1)
    {
      for (uint8_t i = 0; i < 8; i++)
      {
        if ((_extcmd[3] >> i) & 0x01)
        {
          _extcmd[1] = i + 13; // get device id
          break;
        }
      }
      // shift bytes down
      _extcmd[2] = _extcmd[3];
      _extcmd[3] = _extcmd[4];
      _extcmd[4] = _extcmd[5];
    }
    else
    {
      for (uint8_t i = 0; i < 8; i++)
      {
        if ((_extcmd[2] >> i) & 0x01)
        {
          _extcmd[1] = i + 6; // get device id
          break;
        }
      }
    }
    if (cmdtype == 0xF1)
    { // expander channel update
      uint8_t channel;
      if (_extbuf[3])
      { // if we have zone expander data
        channel = (_extbuf[3] >> 5);
        if (!channel)
          channel = 8;
        channel = ((_extcmd[1] - 7) * 8) + 8 + channel; // calculate zone
        _extcmd[4] = ((_extbuf[3] >> 3) & 3) ? 1 : 0;    // fault
      }
      else
      {
        channel = 0; // no zone data so set to zero
        _extcmd[4] = 0;
      }
      _extcmd[2] = cmdtype; // copy subcommand to byte 2
      _extcmd[3] = channel;
      _extcmd[5] = _extbuf[2]; // relay data
      _newExtCmd = true;
      return 6;
    }
    else if (cmdtype == 0xf7)
    {                      // expander poll request
      _extcmd[2] = cmdtype; // copy subcommand to byte 2
      _extcmd[3] = 0;
      _extcmd[4] = _extbuf[3]; // zone faults
      _newExtCmd = true;
      return 5;
    }
    else if (cmdtype == 0x00 || cmdtype == 0x0D)
    {                      // relay channel update
      _extcmd[2] = cmdtype; // copy subcommand to byte 2
      uint8_t channel;
      switch (_extbuf[3] & 0x07f)
      {
      case 1:
        channel = 1;
        break;
      case 2:
        channel = 2;
        break;
      case 4:
        channel = 3;
        break;
      case 8:
        channel = 4;
        break;
      default:
        channel = 0;
      }
      _extcmd[3] = channel;
      _extcmd[4] = _extbuf[3] & 0x80 ? 1 : 0;
      _newExtCmd = true;
      return 5;
    }
    else
    { // unknown subcommand for FA
      _extcmd[0] = _extbuf[0];
      _extcmd[1] = _extbuf[1];
      _extcmd[2] = _extbuf[2];
      _extcmd[3] = _extbuf[3];
      _extcmd[4] = _extbuf[4];
      _extcmd[5] = _extbuf[5];
      _extcmd[6] = _extbuf[6];
      _extcmd[12] = 0x72; // flag to identify unknown subcommand
      _newExtCmd = true;
      return 13; // for debugging return what was sent so we can see why the chcksum failed
    }
  }
  else if (_extcmd[0] == 0xFB)
  {
    //07 54 84 F3 5C 80 52
    // Check how many bytes are in RF message (stored in upper nibble of Byte 2)
    uint8_t n_rf_bytes = _extbuf[1] >> 4;

    if (n_rf_bytes == 5)
    { // For monitoring, we only care about 5 byte messages since that contains data about sensors
      // Verify data
      uint16_t rf_checksum = 0;
      for (uint8_t i = 0; i < n_rf_bytes + 2; i++)
      {
        rf_checksum += _extbuf[i];
      }
      if (rf_checksum % 256 == 0)
      {
        // If checksum is correct, fill _extcmd with data
        // Set second byte of _extcmd to number of data bytes
        _extcmd[1] = 4;
        // The 3rd, 4th, and 5th bytes in _extbuf have the sending device serial number
        // The 3rd byte has the MSB of the number and the 5th has the LSB
        // Fill these into _extcmd
        _extcmd[2] = _extbuf[2] & 0xF;
        _extcmd[3] = _extbuf[3];
        _extcmd[4] = _extbuf[4];
        // 6th byte in _extbuf contains the sensor data
        // bit 1 - ?
        // bit 2 - Battery (0=Normal, 1=LowBat)
        // bit 3 - Heartbeat (Sent every 60-90min) (1 if sending heartbeat)
        // bit 4 - ?
        // bit 5 - Loop 3 (0=Closed, 1=Open)
        // bit 6 - Loop 2 (0=Closed, 1=Open)
        // bit 7 - Loop 4 (0=Closed, 1=Open)
        // bit 8 - Loop 1 (0=Closed, 1=Open)
        _extcmd[5] = _extbuf[5];
        _newExtCmd = true;
        return 6;

        // How to rebuild serial into single integer
        // uint32_t device_serial = (_extbuf[2] & 0xF) << 16;  // Only the lower nibble is part of the device serial
        // device_serial += _extbuf[3] << 8;
        // device_serial += _extbuf[4];
      }
      else
      {
        // also print if chksum fails
        _extcmd[0] = _extbuf[0];
        _extcmd[1] = _extbuf[1];
        _extcmd[2] = _extbuf[2];
        _extcmd[3] = _extbuf[3];
        _extcmd[4] = _extbuf[4];
        _extcmd[5] = _extbuf[5];
        _extcmd[6] = _extbuf[6];
        _extcmd[12] = 0x77; // flag to identify cheksum failed
        _newExtCmd = false;
        return 13;
      }
    }
    else
    {//uint8_t n_rf_bytes = _extbuf[1] >> 4;
       // FB packet but with different length then 5
      // we send out the packet as received for debugging
      _extcmd[0] = _extbuf[0];
      _extcmd[1] = _extbuf[1];
      _extcmd[2] = _extbuf[2];
      _extcmd[3] = _extbuf[3];
      _extcmd[4] = _extbuf[4];
      _extcmd[5] = _extbuf[5];
      _extcmd[6] = _extbuf[6];
      _extcmd[12] = 0x74; // flag to identify unknown command
      _newExtCmd = true;
      return 13;
    }
  }   
  else if (_extcmd[0] == 0xF0) 
  {
    //f0 xx C8 87 00 xx xx xx 00 00 A9
      uint16_t lp_checksum = 0;
      for (uint8_t i = 0; i < 9; i++)
      {
        lp_checksum += _extbuf[i];
        _extcmd[i+2]=_extbuf[i];
      }
      if (lp_checksum % 256 == 0)
      {
        _newExtCmd = true;
        _extcmd[1]=9;
        return 11;

      } else {
        _extcmd[1]=0;
        _extcmd[12]=0x77; //cksum error
        return 13;
      }
  }
  else if (_extcmd[0] != 0 && _extcmd[0] != 0xf6)
  {
    _extcmd[1] = 0; // no device
  }
  _extIdx = _extIdx < OUTBUFSIZE - 2 ? _extIdx : _extIdx - 2;
  for (uint8_t i = 0; i < _extIdx; i++)
  {
    _extcmd[2 + i] = _extbuf[i]; // populate  buffer 0=cmd, 1=device, rest is tx data
    //  printf("_extcmd %02x\r\n",_extcmd[2+i]);
  }
  _newExtCmd = true;
  return _extIdx + 2;
}
#endif
#ifdef MONITORTX
uint8_t Vista::getExtBytes()
{
  uint8_t x;
  uint8_t ret = 0;

  if (!vistaSerialMonitor)
    return 0;
  while (vistaSerialMonitor->available())
  {
    x = vistaSerialMonitor->read();
    if (_extIdx < OUTBUFSIZE)
      _extbuf[_extIdx++] = x;
    _markPulse = 0; // reset pulse flag to wait for next inter msg gap
#ifdef ESP32
    taskYIELD();
#else
    yield();
#endif
  }

  if (_extIdx > 0 && _markPulse)
  {
    // ok, we are on the next pulse (gap) , lets decode the previous msg data
    ret = decodePacket();
    pushCmdQueueItem(ret, _extIdx);
    memset(_extbuf, 0, OUTBUFSIZE); // clear buffer mem
    _extIdx = 0;
  }

  return ret;
}
#endif

void Vista::pushExtBuffer() 
{
  if (_filterOwnTx && _extIdx) {
    _newCmd=false;
    _newExtCmd=true;
    uint8_t ret = decodePacket();
    pushCmdQueueItem(ret, _extIdx);
    _extIdx=0;
  }
}

bool Vista::handle()
{
  _newCmd = false;
  _newExtCmd = false;
  uint8_t x = 0;
  int gidx;

#ifdef MONITORTX
  if (vistaSerialMonitor != NULL)
  {
    if (getExtBytes())
      return true;
  }
#endif

  if (vistaSerial == NULL)
    return false;

  if (_is2400)
    vistaSerial->setBaud(2400);
  else
    vistaSerial->setBaud(4800);

  if (vistaSerial->available())
  {
    
    x = vistaSerial->read();
    if (_outbufIdx == _inbufIdx && !_retries) _pendingAck=false;
    memset(_cbuf, 0, CMDBUFSIZE); // clear buffer mem
    if (_expectByte && x)
    {
      if (x == _expectByte)
      {
        
        _retries = 0;
        _retriesf9 = 0;
        _expectByte = 0;
        _retryAddr = 0;
        _cbuf[0] = 0x78; // for flagging an expect byte found ok
        _cbuf[1] = x;
        pushCmdQueueItem(2);
        return 1; // 1 for logging. 0 for normal
      }
      else if (_expectCmd == 0xf9)
      {
        //request an F9 resend since we did not get the ack from the panel
        if (peekNextKpAddr() != LRRADDR) {
            keyType kt;
            kt.kpaddr = LRRADDR;
            kt.count = 0;
            _outbuf[_inbufIdx] = kt;
            _inbufIdx = (_inbufIdx + 1) % CMDBUFSIZE;
        }

      }
      // we did not get the expect byte response. So assume this byte is another cmd
      _expectByte = 0;
      _pendingAck=false;
      #ifdef VISTA_DISABLE_RETRIES
         _retries = 0;
        // _retriesf9 = 0;
         _retryAddr = 0;
      #endif
    
     
    }
    // expander request command
    if (x == 0xFA)
    {
      vistaSerial->setBaud(4800);
      gidx = 0;
      _cbuf[gidx++] = x;
      readChars(1, _cbuf, &gidx); // 01 ?
      readChars(1, _cbuf, &gidx); // dev id or len code if &1
      readChars(1, _cbuf, &gidx); // seq
      readChars(1, _cbuf, &gidx); // type
      if ((_cbuf[2] & 1))
      {                            // byte 2 = 01 if extended addressing for relay boards 14,15 so packet longer by 1 byte
        readChars(1, _cbuf, &gidx); // extra byte
      }
      else if (_cbuf[4] == 0x00 || _cbuf[4] == 0x0D)
      {                            // 00 cmds use an extra byte
        readChars(1, _cbuf, &gidx); // cmd
      }
      readChars(1, _cbuf, &gidx); // chksum
      #ifdef MONITORTX
      memset(_extcmd, 0, OUTBUFSIZE); // store the previous panel sent data in _extcmd buffer for later use
      memcpy(_extcmd, _cbuf, 7);
      #endif
      if (!validChksum(_cbuf, 0, gidx))
        _cbuf[12] = 0x77;
      else
      {
        onExp(_cbuf);
        _newCmd = true;
        pushCmdQueueItem(gidx);
        pushExtBuffer();
        return 1;
      }
    }

    if (x == 0xF7)
    {
      vistaSerial->setBaud(4800);
      gidx = 0;

      _cbuf[gidx++] = x;
      readChars(F7_MESSAGE_LENGTH - 1, _cbuf, &gidx);
      readChars(3, _cbuf, &gidx); // clear following zeros
      if (!validChksum(_cbuf, 0, gidx))
        _cbuf[12] = 0x77;
      else
      {
        onDisplay(_cbuf, &gidx);
        _newCmd = true; // new valid cmd, process it
      }
      pushCmdQueueItem(gidx);
      return 1; // return 1 to log packet
    }

    // Long Range Radio (LRR)
    if (x == 0xF9)
    {
      vistaSerial->setBaud(4800);
      gidx = 0;
      _cbuf[gidx++] = x;
      // read cycle
      readChars(1, _cbuf, &gidx);
      // read len
      readChars(1, _cbuf, &gidx);
      if (_cbuf[2] == 0)
      {
        onLrr(_cbuf, &gidx);
        _newCmd = true;
      }
      else
      {
        readChars(_cbuf[2], _cbuf, &gidx);
        if (!validChksum(_cbuf, 0, gidx))
          _cbuf[12] = 0x77;
        else
        {
          onLrr(_cbuf, &gidx);
          _newCmd = true;
        }
      }
#ifdef MONITORTX
      memset(_extcmd, 0, OUTBUFSIZE); // store the previous panel sent data in _extcmd buffer for later use
      memcpy(_extcmd, _cbuf, 6);
#endif
      pushCmdQueueItem(gidx);
      pushExtBuffer();
      return 1;
    }
    // key ack
    if (x == 0xF6)
    {
      
      vistaSerial->setBaud(4800);
      _newCmd = true;
      gidx = 0;
      _cbuf[gidx++] = x;
      readChars(1, _cbuf, &gidx);
      uint8_t kpaddr = _retries ? _retryAddr : peekNextKpAddr();
      if (_cbuf[1] == kpaddr && kpaddr > 0)
      {
        _pendingAck=false;
        writeChars();

      }
#ifdef MONITORTX
      memset(_extcmd, 0, OUTBUFSIZE); // store the previous panel sent data in _extcmd buffer for later use
      memcpy(_extcmd, _cbuf, 7);

#endif
      pushCmdQueueItem(gidx);
      pushExtBuffer();
      return 1;
    }

    // AUI handling
    if (x == 0xF2)
    {
      vistaSerial->setBaud(4800);
      gidx = 0;
      _cbuf[gidx++] = x;
      readChars(1, _cbuf, &gidx); // len
      readChars(_cbuf[1], _cbuf, &gidx);
      if (!validChksum(_cbuf, 0, gidx))
        _cbuf[12] = 0x77;
      else
      {
        onAUI(_cbuf, &gidx);
        _newCmd = true;
      }
      pushCmdQueueItem(gidx);
      return 1;
    }
    // // unknown
    if (x == 0xF8)
    {
      vistaSerial->setBaud(4800);
      gidx = 0;
      _cbuf[gidx++] = x;                // cmd byte
      readChars(1, _cbuf, &gidx);       // header byte
      readChars(1, _cbuf, &gidx);       // len byte
      readChars(_cbuf[2], _cbuf, &gidx); // read len bytes
      if (!validChksum(_cbuf, 0, gidx))
        _cbuf[12] = 0x77;
      else
        _newCmd = true;
      pushCmdQueueItem(gidx);
      return 1;
    }
    // polling loop
    if (x == 0xF0)
    {
      vistaSerial->setBaud(4800);
      _newCmd = true;
      gidx = 0;
      _cbuf[gidx++] = x;
      readChars(1, _cbuf, &gidx);
#ifdef MONITORTX
      memset(_extcmd, 0, OUTBUFSIZE); // store the previous panel sent data in _extcmd buffer for later use
      memcpy(_extcmd, _cbuf, 2);
#endif
      pushCmdQueueItem(gidx);
      return 1;
    }

    // RF supervision
    if (x == 0xFB)
    {
      vistaSerial->setBaud(4800);
      gidx = 0;
      _cbuf[gidx++] = x;
      readChars(4, _cbuf, &gidx);
      if (!validChksum(_cbuf, 0, gidx))
        _cbuf[12] = 0x77;
      else
        _newCmd = true;
      onRF(_cbuf); //if rf emulation
#ifdef MONITORTX
      memset(_extcmd, 0, OUTBUFSIZE); // store the previous panel sent data in _extcmd buffer for later use
      memcpy(_extcmd, _cbuf, 6);
#endif
      pushCmdQueueItem(gidx);
      pushExtBuffer();
      return 1;
    }

    // capture any unknown cmd byte if exists
    if (!x)
      return 0; // clear any stray zeros
    gidx = 0;
    _cbuf[gidx++] = x;
    _cbuf[12] = 0x90; // possible ack byte or new unknown cmd
    unsigned long timeout = millis();
    uint8_t i = 0;
    int num = 12;
    while (i < num && millis() - timeout < 5)
    {
      if (vistaSerial->available())
      {
        timeout = millis();
        x = vistaSerial->read();
        _cbuf[gidx++] = x;
        i++;
      }
#ifdef ESP32
      taskYIELD();
#else
      yield();
#endif
    }
    pushCmdQueueItem(gidx);
    return 1;
  }

  return 0;
}

// i've included these for debugging code only to stop processing during a hw lockup. do not use in production
void Vista::hw_wdt_disable()
{
#ifndef ESP32
  *((volatile uint32_t *)0x60000900) &= ~(1); // Hardware WDT OFF
#endif
}

void Vista::hw_wdt_enable()
{
#ifndef ESP32
  *((volatile uint32_t *)0x60000900) |= 1; // Hardware WDT ON
#endif
}

void Vista::stop()
{
  // hw_wdt_enable(); //debugging only
  #if defined(USE_ESP_IDF) or defined(ESP32)
  gpio_intr_disable((gpio_num_t) _rxPin);
  gpio_isr_handler_remove((gpio_num_t) _rxPin);
#else
  detachInterrupt(_rxPin);
#endif
#ifdef MONITORTX
  if (vistaSerialMonitor)
  {
    #if defined(USE_ESP_IDF) or defined(ESP32)
  gpio_intr_disable((gpio_num_t) _monitorPin);
  gpio_isr_handler_remove((gpio_num_t) _monitorPin);
#else
    detachInterrupt(_monitorPin);
#endif
  }
#endif
  keybusConnected = false;
  connected = false;
}

void Vista::begin(int receivePin, int transmitPin, char keypadAddr, int monitorTxPin, bool invertRx, bool invertTx, bool invertMon, uint8_t inputRx, uint8_t inputMon)
{
#ifndef ESP32
// hw_wdt_disable(); //debugging only
// ESP.wdtDisable(); //debugging only
#endif
  _expectByte = 0;
  _retries = 0;
  _is2400 = false;
  _pendingAck=false;
  

  _kpAddr = keypadAddr;
  _txPin = transmitPin;
  _rxPin = receivePin;
  _monitorPin = monitorTxPin;
  _invertRead = invertRx;

// panel data rx interrupt - yellow line
#ifdef ESP32
  vistaSerial = new SoftwareSerial(_rxPin, _txPin, invertRx, invertTx, 2, 60 * 10, inputRx);
#else
  vistaSerial = new SoftwareSerial(_rxPin, _txPin, invertRx, invertTx, 2, 60 * 10, inputRx);
#endif

  if (vistaSerial->isValidGPIOpin(_rxPin))
  {
    vistaSerial->begin(4800, SWSERIAL_8E2);
      #if defined (USE_ESP_IDF)  or defined(ESP32)
       gpio_install_isr_service(0);
       gpio_set_intr_type((gpio_num_t)_rxPin, GPIO_INTR_ANYEDGE);
       gpio_isr_handler_add((gpio_num_t)_rxPin, rxISRHandler, (void*)(gpio_num_t)_rxPin);

    //gpio_set_intr_type((gpio_num_t)_rxPin, GPIO_INTR_ANYEDGE);
   // esp_err_t err = esp_intr_alloc(ETS_GPIO_INTR_SOURCE, 0, rxISRHandler, NULL, NULL);
   //  gpio_isr_register(rxISRHandler, NULL, ESP_INTR_FLAG_LOWMED, NULL);
     //gpio_intr_enable((gpio_num_t) _rxPin);

        #else
    attachInterrupt(digitalPinToInterrupt(_rxPin), rxISRHandler, CHANGE);
    #endif
    vistaSerial->processSingle = true;
  }
  else
  {
    free(vistaSerial);
    vistaSerial = NULL;
    printf("Warning rx pin %d is invalid", _rxPin);
  }
#ifdef MONITORTX
// interrupt for capturing keypad/module data on green transmit line
#ifdef ESP32
  vistaSerialMonitor = new SoftwareSerial(_monitorPin, -1, invertMon, false, 2, OUTBUFSIZE * 10, inputMon);
#else
  vistaSerialMonitor = new SoftwareSerial(_monitorPin, -1, invertMon, false, 2, OUTBUFSIZE * 10, inputMon);
#endif
  if (vistaSerialMonitor->isValidGPIOpin(_monitorPin))
  {
    vistaSerialMonitor->begin(4800, SWSERIAL_8E2);
      #if defined (USE_ESP_IDF) or defined(ESP32)
       //gpio_install_isr_service(0);
       gpio_set_intr_type((gpio_num_t)_monitorPin, GPIO_INTR_ANYEDGE);
       gpio_isr_handler_add((gpio_num_t)_monitorPin, txISRHandler, (void*)(gpio_num_t)_monitorPin);
      //  gpio_intr_enable((gpio_num_t) _monitorPin);

         //  gpio_set_intr_type((gpio_num_t)_monitorPin, GPIO_INTR_ANYEDGE);
   // esp_err_t err = esp_intr_alloc(ETS_GPIO_INTR_SOURCE, 0, txISRHandler, NULL, NULL);
        #else
    attachInterrupt(digitalPinToInterrupt(_monitorPin), txISRHandler, CHANGE);
    #endif
    vistaSerialMonitor->processSingle = true;
  }
  else
  {
    free(vistaSerialMonitor);
    vistaSerialMonitor = NULL;
    printf("Warning monitor rx pin %d is invalid", _monitorPin);
  }
#endif
}
