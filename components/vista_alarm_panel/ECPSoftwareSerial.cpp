/*

SoftwareSerial.cpp - Implementation of the Arduino software serial for ESP8266/ESP32.
Copyright (c) 2015-2016 Peter Lerup. All rights reserved.
Copyright (c) 2018-2019 Dirk O. Kaar. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Modified for 4800 8E2
*/

#include "ECPSoftwareSerial.h"
#if !defined(ARDUINO_MQTT)
#include "esphome/core/defines.h"
#endif


SoftwareSerial::SoftwareSerial(
    int receivePin, int transmitPin, bool invertRx, bool invertTx, int bufSize, int isrBufSize, int inputRx)
{
    m_isrBuffer = 0;
    m_isrOverflow = false;
    m_isrLastCycle = 0;
    m_oneWire = (receivePin == transmitPin);
    m_invert_tx = invertTx;
    m_invert_rx = invertRx;
    m_input_type = inputRx;
    if (isValidGPIOpin(receivePin))
    {
        m_rxPin = receivePin;
        m_bufSize = bufSize;
        m_buffer = (uint8_t *)malloc(m_bufSize);
        m_isrBufSize = isrBufSize ? isrBufSize : 10 * bufSize;
        m_isrBuffer = static_cast<std::atomic<uint32_t> *>(malloc(m_isrBufSize * sizeof(uint32_t)));
    }
    if (isValidGPIOpin(transmitPin)
#ifdef ESP8266
        || (!m_oneWire && (transmitPin == 16)))
    {
#else
    )
    {
#endif
        m_txValid = true;
        m_txPin = transmitPin;
    }
}

SoftwareSerial::~SoftwareSerial()
{
    end();
    if (m_buffer)
    {
        free(m_buffer);
    }
    if (m_isrBuffer)
    {
        free(m_isrBuffer);
    }
}

bool SoftwareSerial::isValidGPIOpin(int pin)
{
#ifdef ESP8266
    return (pin >= 0 && pin <= 5) || (pin >= 12 && pin <= 15);
#endif
#ifdef ESP32
#if CONFIG_IDF_TARGET_ESP32
	// Datasheet https://documentation.espressif.com/esp32_datasheet_en.pdf
	// Pinout    https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/_images/esp32_devkitC_v4_pinlayout.png
    return pin == 1 || (pin >= 3 && pin <= 5) || (pin >= 12 && pin <= 15) ||
		(pin >= 18 && pin <= 19) || (pin >= 21 && pin <= 23) || (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 33);
#elif CONFIG_IDF_TARGET_ESP32S2
	// Datasheet https://documentation.espressif.com/esp32-s2_datasheet_en.pdf
	// Pinout    https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s2/_images/esp32-s2-devkitm-1-v1-pin-layout.png
	return (pin >= 1 && pin <= 21) || (pin >= 33 && pin <= 44);
#elif CONFIG_IDF_TARGET_ESP32S3
	// Datasheet https://documentation.espressif.com/esp32-s3_datasheet_en.pdf
	// Pinout    https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/_images/ESP32-S3_DevKitC-1_pinlayout_v1.1.jpg
	return (pin >= 1 && pin <= 2) || (pin >= 4 && pin <= 21) || pin == 26 || (pin >= 33 && pin <= 44);
#elif CONFIG_IDF_TARGET_ESP32C3
	// Datasheet https://documentation.espressif.com/esp32-c3_datasheet_en.pdf
	// Pinout    https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/_images/esp32-c3-devkitm-1-v1-pinout.png
    return (pin >= 0 && pin <= 1) || (pin >= 3 && pin <= 7) || pin == 10 || (pin >= 18 && pin <= 21);
#endif
#endif
#ifdef USE_RP2040
    return (pin >= 0 && pin <= 21) || (pin >=26 && pin <= 27);
#endif
}


#ifndef ESP32
uint32_t SoftwareSerial::m_savedPS = 0;
#else
portMUX_TYPE SoftwareSerial::m_interruptsMux = portMUX_INITIALIZER_UNLOCKED;
#endif

ALWAYS_INLINE_ATTR inline void IRAM_ATTR SoftwareSerial::disableInterrupts()
{
#ifndef ESP32
    m_savedPS = xt_rsil(15);
#else
    taskENTER_CRITICAL(&m_interruptsMux);
#endif
}

ALWAYS_INLINE_ATTR inline void IRAM_ATTR SoftwareSerial::restoreInterrupts()
{
#ifndef ESP32
    xt_wsr_ps(m_savedPS);
#else
    taskEXIT_CRITICAL(&m_interruptsMux);
#endif
}

void SoftwareSerial::setBaud(int32_t baud)
{

    m_bitCycles = microsToTicks(1000000UL) / baud;

    if (baud == 4800) // we save 4800 bit cycles for call from ISR later
        m_4800_bitCycles = m_bitCycles;
}

void SoftwareSerial::setConfig(int32_t baud, SoftwareSerialConfig config)
{
    setBaud(baud);
    m_dataBits = 5 + (config % 4);
}

void SoftwareSerial::begin(int32_t baud, SoftwareSerialConfig config)
{
    m_dataBits = 5 + (config % 4);
    setBaud(baud);
    if (m_buffer != 0 && m_isrBuffer != 0)
    {
        m_rxValid = true;
        m_inPos = m_outPos = 0;
        m_isrInPos.store(0);
        m_isrOutPos.store(0);
        #if defined (USE_ESP_IDF) or defined(ESP32)
        gpio_reset_pin((gpio_num_t)m_rxPin);
        gpio_set_direction((gpio_num_t)m_rxPin, GPIO_MODE_INPUT);
        gpio_pullup_dis((gpio_num_t)m_rxPin);
        gpio_pulldown_dis((gpio_num_t)m_rxPin);
        if (m_input_type==INPUT_PULLDOWN) {
              gpio_pulldown_en((gpio_num_t)m_rxPin);
        } else if (m_input_type==INPUT_PULLUP) {
            gpio_pullup_en((gpio_num_t)m_rxPin);
        }
        #else
        pinMode(m_rxPin, m_input_type);
        #endif

    }
    
    if (m_txValid && !m_oneWire)
    {

        #if defined(USE_ESP_IDF) or defined(ESP32)
        gpio_reset_pin((gpio_num_t)m_txPin);
        gpio_pulldown_dis((gpio_num_t)m_txPin);
        gpio_pullup_dis((gpio_num_t)m_txPin); 
        gpio_set_direction((gpio_num_t)m_txPin, GPIO_MODE_OUTPUT);
        #else
            pinMode(m_txPin, OUTPUT);
        #endif

        digitalWriteByte(m_txPin, !m_invert_tx);
    }

    if (!m_rxEnabled)
    {
        enableRx(true);
    }
}

void SoftwareSerial::end()
{
    enableRx(false);
}

void SoftwareSerial::enableTx(bool on)
{
    if (m_txValid && m_oneWire)
    {
        if (on)
        {
            enableRx(false);
            #if defined (USE_ESP_IDF) or defined(ESP32)
            gpio_reset_pin((gpio_num_t)m_txPin);
            gpio_pulldown_dis((gpio_num_t)m_txPin);
            gpio_pullup_dis((gpio_num_t)m_txPin); 
            gpio_set_direction((gpio_num_t)m_txPin, GPIO_MODE_OUTPUT);
            #else
            pinMode(m_txPin, OUTPUT);
            #endif
            digitalWriteByte(m_txPin, !m_invert_tx);
        }
        else
        {
            #if defined (USE_ESP_IDF)  or defined(ESP32)
            gpio_reset_pin((gpio_num_t)m_rxPin);
            gpio_set_direction((gpio_num_t)m_rxPin, GPIO_MODE_INPUT);
            gpio_pullup_dis((gpio_num_t)m_rxPin);
            gpio_pulldown_dis((gpio_num_t)m_rxPin);
    
            #else
            pinMode(m_rxPin, INPUT);
            #endif
            enableRx(true);
        }
    }
}

void SoftwareSerial::enableRx(bool on)
{
    if (m_rxValid)
    {
        if (on)
        {
            m_rxCurBit = m_dataBits + 2;
        }
        m_rxEnabled = on;
    }
}

int SoftwareSerial::read()
{
    if (!m_rxValid)
    {
        return -1;
    }
    if (m_inPos == m_outPos)
    {
        return -1;
    }
    uint8_t ch = m_buffer[m_outPos];
    m_outPos = (m_outPos + 1) % m_bufSize;
    return ch;
}

int SoftwareSerial::read(bool processRxbits)
{
    if (!m_rxValid)
    {
        return -1;
    }
    if (m_inPos == m_outPos)
    {
        if (processRxbits)
            rxBits();
        if (m_inPos == m_outPos)
        {
            return -1;
        }
    }
    uint8_t ch = m_buffer[m_outPos];
    m_outPos = (m_outPos + 1) % m_bufSize;
    return ch;
}

int SoftwareSerial::available()
{
    if (!m_rxValid)
    {
        return 0;
    }
    rxBits();
    int avail = m_inPos - m_outPos;
    if (avail < 0)
    {
        avail += m_bufSize;
    }
    if (!avail)
    {
        rxBits();
        
        avail = m_inPos - m_outPos;
        if (avail < 0)
        {
            avail += m_bufSize;
        }
    }
    return avail;
}

#define WAIT                           \
    {                                  \
        while (ticks() - start < wait) \
            ;                          \
        wait += m_bitCycles;           \
    }


size_t IRAM_ATTR SoftwareSerial::write(uint8_t b, bool parity, int32_t baud)
{
    int32_t origCycles = m_bitCycles;
    bool origParity = m_parity;

    if (baud == 4800 && m_4800_bitCycles > 0)
        m_bitCycles = m_4800_bitCycles;
    else
        m_bitCycles = microsToTicks(1000000UL) / baud;
        //  m_bitCycles = microsToTicks(APB_CLK_FREQ/64) / baud;


    m_parity = parity;
    size_t r = write(b,true);
    m_parity = origParity;
    m_bitCycles = origCycles;
    return r;
}

size_t IRAM_ATTR SoftwareSerial::write(uint8_t b,bool interrupt)
{
    uint8_t parity = 0;
    if (!m_txValid)
        return 0;
    if (m_invert_tx)
        b = ~b;

    unsigned long wait = m_bitCycles;

    unsigned long start = ticks();
    if (interrupt) disableInterrupts();
    // Start bit;
    if (m_invert_tx)
        digitalWriteByte(m_txPin, HIGH);
    else
        digitalWriteByte(m_txPin, LOW);
    WAIT;
    for (int i = 0; i < m_dataBits; i++)
    {
        if (b & 1)
        {
            digitalWriteByte(m_txPin, HIGH);
            parity = parity ^ 0x01;
        }
        else
        {
            digitalWriteByte(m_txPin, LOW);
            parity = parity ^ 0x00;
        }
        WAIT;
        b >>= 1;
    }
    // parity bit
    if (m_parity)
    {
        if (parity == 0)
        {
            if (m_invert_tx && m_dataBits != 5)
            {
                digitalWriteByte(m_txPin, HIGH);
            }
            else
            {
                digitalWriteByte(m_txPin, LOW);
            }
        }
        else
        {
            if (m_invert_tx && m_dataBits != 5)
            {
                digitalWriteByte(m_txPin, LOW);
            }
            else
            {
                digitalWriteByte(m_txPin, HIGH);
            }
        }
        WAIT;
    }

    // restore pin to natural state
    if (m_invert_tx)
    {
        digitalWriteByte(m_txPin, LOW);
    }
    else
    {
        digitalWriteByte(m_txPin, HIGH);
    }
    WAIT;                // 1st stop bit
    if (m_dataBits != 5) // 1 stop bit for keypad send
        WAIT;
    if (interrupt) restoreInterrupts();
    return 1;
}

bool SoftwareSerial::overflow()
{
    bool res = m_overflow;
    m_overflow = false;
    return res;
}

int SoftwareSerial::peek()
{
    //  if (!m_rxValid || (rxBits(), m_inPos == m_outPos)) {
    // return -1;
    //  }
    if (!m_rxValid || (m_inPos == m_outPos))
    {
        return -1;
    }
    return m_buffer[m_outPos];
}

uint8_t SoftwareSerial::checkParity(uint8_t b)
{
    uint8_t parity = 0;
    for (int i = 0; i < m_dataBits; i++)
    {
        if (b & 1)
        {
            parity = parity ^ 0x00;
        }
        else
        {
            parity = parity ^ 0x01;
        }
        b >>= 1;
    }
    return parity;
}

void SoftwareSerial::rxBits()
{
    int avail = m_isrInPos.load() - m_isrOutPos.load();
    if (avail < 0)
    {
        avail += m_isrBufSize;
    }
    if (m_isrOverflow.load())
    {
        m_overflow = true;
        m_isrOverflow.store(false);
    }

    // stop bit can go undetected if leading data bits are at same level
    // and there was also no next start bit yet, so one byte may be pending.
    // low-cost check first

    if (avail == 0 && m_rxCurBit < m_dataBits + 2 && m_isrInPos.load() == m_isrOutPos.load() && m_rxCurBit >= 0)
    {
        uint32_t expectedCycle = m_isrLastCycle.load() + (m_dataBits + 3 - m_rxCurBit) * m_bitCycles;

        if (static_cast<int32_t>(ticks() - expectedCycle) > m_bitCycles)
        {

            // Store inverted stop bit edge and cycle in the buffer unless we have an overflow
            // cycle's LSB is repurposed for the level bit
            int next = (m_isrInPos.load() + 1) % m_isrBufSize;
            if (next != m_isrOutPos.load())
            {
                m_isrBuffer[m_isrInPos.load()].store((expectedCycle | 1) ^ !m_invert_rx);
                m_isrInPos.store(next);
                ++avail;
            }
            else
            {
                m_isrOverflow.store(true);
            }
        }
    }

    static uint8_t parity;
    //  static uint8_t stop1;
    //   static uint8_t stop2;

    while (avail--)
    {
        // error introduced by edge value in LSB is negligible
        uint32_t isrCycle = m_isrBuffer[m_isrOutPos.load()].load();
        // extract inverted edge value
        bool level = (isrCycle & 1) == m_invert_rx;
        m_isrOutPos.store((m_isrOutPos.load() + 1) % m_isrBufSize);

        int32_t cycles = static_cast<int32_t>(isrCycle - m_isrLastCycle.load()) - (m_bitCycles / 2);
        if (cycles < 0)
            cycles = +0x7fffffff;
        //   if (cycles < 0)
        //     cycles= +0x80000000;

        m_isrLastCycle.store(isrCycle);

        do
        {
            // data bits
            uint32_t bits = 0;
            uint32_t hiddenBits = 0;
            bool lastBit = false;
            if (m_rxCurBit >= -1 && m_rxCurBit < (m_dataBits))
            {
                parity = 0;
                // stop1=1;
                // stop2=1;
                // Serial.printf("curbit=%d,cycles=%04x,level=%d\n",m_rxCurBit,cycles,level);

                if (cycles >= m_bitCycles)
                {
                    // preceding masked bits
                    bits = cycles / m_bitCycles;
                    if (bits >= m_dataBits - m_rxCurBit)
                    {
                        hiddenBits = (m_dataBits - 1) - m_rxCurBit;
                    }
                    else
                    {
                        hiddenBits = bits;
                    }
                    bits -= hiddenBits;
                    lastBit = m_rxCurByte & 0x80;
                    m_rxCurByte >>= hiddenBits;
                    // masked bits have same level as last unmasked bit
                    if (lastBit)
                    {
                        m_rxCurByte |= 0xff << (8 - hiddenBits);
                    }
                    m_rxCurBit += hiddenBits;
                    cycles -= hiddenBits * m_bitCycles;
                    if (bits)
                    {
                        if (lastBit)
                        {
                            parity = 1;
                        }
                        // if (bits > 3) bits=3;

                        cycles -= m_bitCycles; // remove parity bit cycles
                        m_rxCurBit++;          // advance to stop bits
                                               // Serial.printf("got bits. bits=%d,Lastbit=%d,curbit=%d\n",bits,lastBit,m_rxCurBit);
                                               //  --bits;
                    }
                }
                if (m_rxCurBit == m_dataBits - 1)
                {
                    ++m_rxCurBit;
                    cycles -= m_bitCycles;
                    parity = level;
                    //  Serial.printf("Set parity from level %d,bits=%d,curbit=%d,byte=%02X,cycles=%d,bitcycles=%d,avail=%d\n",level,bits,m_rxCurBit,m_rxCurByte,cycles,m_bitCycles,avail);
                }
                // Serial.printf("curbyte=%02X,cycle=%08X,bitcycles=%08X,bits=%d,curbit=%d,hiddenbits=%d,parity=%d,lastBit=%d\n",m_rxCurByte,cycles,m_bitCycles,bits,m_rxCurBit,hiddenBits,parity,lastBit);

                if (m_rxCurBit < (m_dataBits - 1))
                {

                    ++m_rxCurBit;
                    cycles -= m_bitCycles;
                    m_rxCurByte >>= 1;
                    if (level)
                    {
                        m_rxCurByte |= 0x80;
                    }
                }
                continue;
            }
            // 1st stop bit
            if (m_rxCurBit == m_dataBits)
            {
                // uint8_t bits = cycles / m_bitCycles;
                ++m_rxCurBit;
                cycles -= m_bitCycles;
                continue;
            }
            // 2nd stop bit and save byte
            if (m_rxCurBit == m_dataBits + 1)
            {
                uint8_t bits = cycles / m_bitCycles;
                ++m_rxCurBit;
                // stop2=level;
                // if (bits ) {
                //  stop2=!level;
                //}
                cycles -= m_bitCycles;
                // Store the received value in the buffer unless we have an overflow
                int next = (m_inPos + 1) % m_bufSize;
                char byt = m_rxCurByte >> (8 - m_dataBits);
                if (checkParity(byt) != parity && byt) {
                        printf("\nParity error: byte=%02X\n",byt);
                }

               //  if (!byt || bits > 12) printf("*** byte=%02X, parity=%d,checkParity=%d,bits=%d,level=%d,cycles=%d,m_bitCycles=%d,self=%d\n\n",byt,parity,checkParity(byt),bits,level,cycles,m_bitCycles,this);
                if (checkParity(byt) == parity && bits < 15)
                //if (bits < 15)
                {
                    if (next != m_outPos)
                    {
                        m_buffer[m_inPos] = byt;
                        m_inPos = next;
                    }
                    else
                    {
                        m_overflow = true;
                    }
               }
                // reset to 0 is important for masked bit logic
                m_rxCurByte = 0;

                continue;
            }
            if (m_rxCurBit > m_dataBits + 1)
            {
                // start bit level is low
                if (!level)
                {
                    m_rxCurBit = -1;
                }
                // if flag set, we only process 1 byte at a time
                if (processSingle)
                {
                    avail = 0;
                }
            }
            break;
        } while (cycles >= 0);
    }
}

void IRAM_ATTR SoftwareSerial::rxRead()
{
    #if defined(USE_ESP_IDF) or defined(ESP32)
    bool level= gpio_get_level((gpio_num_t)m_rxPin);
    #else
    bool level = digitalRead(m_rxPin);
    #endif
    rxSave(level);
}

void IRAM_ATTR SoftwareSerial::rxSave(bool level)
{
    unsigned long curCycle = ticks();
    // Store inverted edge value & cycle in the buffer unless we have an overflow
    // cycle's LSB is repurposed for the level bit
    int next = (m_isrInPos.load() + 1) % m_isrBufSize;
    if (next != m_isrOutPos.load())
    {
        m_isrBuffer[m_isrInPos.load()].store((curCycle | 1) ^ level);
        m_isrInPos.store(next);
    }
    else
    {
        m_isrOverflow.store(true);
    }
}

int SoftwareSerial::bitsAvailable()
{
    int avail = m_isrInPos.load() - m_isrOutPos.load();
    if (avail < 0)
    {
        avail += m_isrBufSize;
    }
    return avail;
}
