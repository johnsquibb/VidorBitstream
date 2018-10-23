/*
  This file is part of the VidorPeripherals/VidorGraphics library.
  Copyright (c) 2018 Arduino SA. All rights reserved.

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "VidorMailbox.h"
#include "VidorIP.h"
#include "defines.h"

#ifndef __VIDOR_IO_H__
#define __VIDOR_IO_H__

extern "C" {
	void pinModeExtended( uint32_t ulPin, uint32_t ulMode );
	void digitalWriteExtended( uint32_t ulPin, uint32_t ulVal );
	int digitalReadExtended( uint32_t ulPin );
	void analogWriteExtended( uint32_t pin, uint32_t value );
}

class VidorIO : public VidorIP {

public:

	VidorIO(int _base) : base(_base) {}

	void pinMode(uint32_t pin, uint32_t mode) {
		uint32_t rpc[256];

		// get giid and chan for required pin (pin here has the complete numbering scheme)
		// TODO: should I ask this to UID 0 == FPGA ?
		if (!initialized) {
			int ret = init(PIO_UID, pin + 32*base);
			if (ret < 0) {
				return;
			}
			initialized = true;
		}

		rpc[0] = RPC_CMD(info.giid, info.chn, 5);
		rpc[1] = pin;

		switch (mode) {
			case OUTPUT:
				rpc[2] = 2;
				break;
			case INPUT:
				rpc[2] = 1;
				break;
			default:
				rpc[2] = mode;
		}
		VidorMailbox.sendCommand(rpc, 3);
	}

	void digitalWrite(uint32_t pin, uint32_t mode) {
		uint32_t rpc[256];
		rpc[0] = RPC_CMD(info.giid, info.chn, 6);
		rpc[1] = pin;
		rpc[2] = mode;
		VidorMailbox.sendCommand(rpc, 3);
	}

	int digitalRead(uint32_t pin) {
		uint32_t rpc[256];
		rpc[0] = RPC_CMD(info.giid, info.chn, 7);
		rpc[1] = pin;
		return VidorMailbox.sendCommand(rpc, 2);
	}

	int period = -1;

	void analogWriteResolution(int bits, int frequency) {

		uint32_t rpc[256];
		period = 2 << bits;
		int prescaler = (2 * F_CPU / frequency) / period;

		rpc[0] = RPC_CMD(info.giid, info.chn, 8);
		rpc[1] = prescaler;
		rpc[2] = period;
		VidorMailbox.sendCommand(rpc, 3);
	}

	void analogWrite(uint32_t pin, uint32_t mode) {

		uint32_t rpc[256];

		if (period == -1) {
			// sane default
			analogWriteResolution(8, 490);
		}
		pinMode(pin, 3);

		rpc[0] = RPC_CMD(info.giid, info.chn, 9);
		rpc[1] = pin;
		rpc[2] = mode;
		rpc[3] = period - mode;
		VidorMailbox.sendCommand(rpc, 4);
	}

	int begin() {
		return 0;
	};

	/* I2C functions moved*/
	/* UART functions moved */
	/* SPI functions moved */
  private:
	int base;
	bool initialized = false;
};

static VidorIO* instance[3] = {NULL, NULL, NULL};
class VidorIOContainer {
	public:

		static void pinMode(int pin, int mode) {
			if (instance[pin/32] == NULL) {
				instance[pin/32] = new VidorIO(pin/32);
			}
			instance[pin/32]->pinMode(pin % 32, mode);
		}

		static void digitalWrite(int pin, int value) {
			instance[pin/32]->digitalWrite(pin % 32, value);
		}

		static int digitalRead(int pin) {
			return instance[pin/32]->digitalRead(pin);

		}
		static void analogWriteResolution(int bits, int frequency) {
			instance[0]->analogWriteResolution(bits, frequency);
		}

		static void analogWrite(int pin, int mode) {
			instance[pin/32]->analogWrite(pin % 32, mode);
		}
};


#endif