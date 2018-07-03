/*
 * SPI Master library for Arduino Zero.
 * Copyright (c) 2015 Arduino LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _VIDORSPI_H_INCLUDED
#define _VIDORSPI_H_INCLUDED

#include <Arduino.h>
#include <SPI.h>
#include <VidorBase.h>

class VidorSPISettings {
  public:
  VidorSPISettings(uint32_t clock, BitOrder bitOrder, uint8_t dataMode) {
    if (__builtin_constant_p(clock)) {
      init_AlwaysInline(clock, bitOrder, dataMode);
    } else {
      init_MightInline(clock, bitOrder, dataMode);
    }
  }

  // Default speed set to 4MHz, SPI mode set to MODE 0 and Bit order set to MSB first.
  VidorSPISettings() { init_AlwaysInline(4000000, MSBFIRST, SPI_MODE0); }

  VidorSPISettings(SPISettings& settings) {
	  init_MightInline(settings.getClockFreq(), settings.getBitOrder(), settings.getDataMode());
  }

  VidorSPISettings(SPISettings settings) {
	  init_MightInline(settings.getClockFreq(), settings.getBitOrder(), settings.getDataMode());
  }

  bool operator==(const VidorSPISettings& rhs) const
  {
    if ((this->clockFreq == rhs.clockFreq) &&
        (this->bitOrder == rhs.bitOrder) &&
        (this->dataMode == rhs.dataMode)) {
      return true;
    }
    return false;
  }

  bool operator!=(const VidorSPISettings& rhs) const
  {
    return !(*this == rhs);
  }

  bool operator==(const SPISettings& rhs) const
  {
    if ((this->clockFreq == rhs.getClockFreq()) &&
        (this->bitOrder == rhs.getBitOrder()) &&
        (this->dataMode == rhs.getDataMode())) {
      return true;
    }
    return false;
  }

  bool operator!=(const SPISettings& rhs) const
  {
    return !(*this == rhs);
  }

  private:
  void init_MightInline(uint32_t clock, BitOrder bitOrder, uint8_t dataMode) {
    init_AlwaysInline(clock, bitOrder, dataMode);
  }

  void init_AlwaysInline(uint32_t clock, BitOrder bitOrder, uint8_t dataMode) __attribute__((__always_inline__)) {
    this->clockFreq = clock;
    this->bitOrder = bitOrder;
    this->dataMode = dataMode;
  }

  uint32_t clockFreq;
  uint8_t dataMode;
  BitOrder bitOrder;

  friend class VidorSPIClass;
  friend class BitBangedSPI;
};


class VidorSPIClass {
  public:
  VidorSPIClass(VidorBase * s, int index);

  byte transfer(uint8_t data);
  uint16_t transfer16(uint16_t data);
  void transfer(void *buf, size_t count);

  // Transaction Functions
  void usingInterrupt(int interruptNumber);
  void notUsingInterrupt(int interruptNumber);
  void beginTransaction(SPISettings settings);
  void endTransaction(void);

  // SPI Configuration methods
  void attachInterrupt();
  void detachInterrupt();

  void begin();
  void end();

  void setBitOrder(BitOrder order);
  void setDataMode(uint8_t uc_mode);
  void setClockDivider(uint8_t uc_div);

  private:
  void init();
  void config(SPISettings settings);

  VidorBase *s;
  VidorSPISettings settings;

  int index;
  bool initialized;
  uint8_t interruptMode;
  char interruptSave;
  uint32_t interruptMask;
};


class BitBangedSPI {
public:

  BitBangedSPI() {}

  void begin() {
    pinMode(SCK_BB, OUTPUT);
    pinMode(MOSI_BB, OUTPUT);
    //pinMode(MISO_BB, INPUT);
    digitalWrite(SCK_BB, LOW);
    digitalWrite(MOSI_BB, LOW);
  }

  void beginTransaction(VidorSPISettings settings) {
    pulseWidth = (500000 + settings.clockFreq - 1) / settings.clockFreq;
    if (pulseWidth == 0)
      pulseWidth = 1;
  }

  void endTransaction() {}

  void end() {}

  uint8_t transfer (uint8_t b) {
    for (unsigned int i = 0; i < 8; ++i) {
      digitalWrite(MOSI_BB, (b & 0x80) ? HIGH : LOW);
      digitalWrite(SCK_BB, HIGH);
      //delayMicroseconds(pulseWidth);
      b = (b << 1) | digitalRead(MISO_BB);
      digitalWrite(SCK_BB, LOW); // slow pulse
      //delayMicroseconds(pulseWidth);
    }
    Serial.println(b, HEX);
    return b;
  }

  uint8_t transfer (uint8_t* data, size_t count) {
    for (size_t i=0; i<count; i++) {
      data[i] = transfer(data[i]);
    }
	return count;
  }

  uint16_t transfer16 (uint16_t data) {
    union { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; } t;

    t.val = data;

    t.msb = transfer(t.msb);
    t.lsb = transfer(t.lsb);

    return t.val;
  }

  void setBitOrder(uint8_t) {}
  void setDataMode(uint8_t) {}
  void setClockDivider(uint32_t) {}

private:
  unsigned long pulseWidth; // in microseconds
};

extern BitBangedSPI SPIExBB;
extern VidorSPIClass SPIEx;

#endif
