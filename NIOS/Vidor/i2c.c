/**
 * cfr https://alteraforum.com/forum/showthread.php?t=16220
 */
#include <stdio.h>
#include <io.h>
#include <system.h>

#include "i2c.h"
#include "mb.h"

#define DEBUG_PRINT_ENABLED 0
#if DEBUG_PRINT_ENABLED
#define DGB_PRINTF printf
#else
#define DGB_PRINTF(format, args...) ((void)0)
#endif

// Overall status and control
#define IOWR_I2C_PRERLO(port,data)  IOWR(port, 0, data)
#define IOWR_I2C_PRERHI(port,data)  IOWR(port, 1, data)
#define IOWR_I2C_CTR(port,data)     IOWR(port, 2, data)
#define IOWR_I2C_TXR(port,data)     IOWR(port, 3, data)
#define IOWR_I2C_CR(port,data)      IOWR(port, 4, data)
#define IORD_I2C_PRERLO(port)       IORD(port, 0)
#define IORD_I2C_PRERHI(port)       IORD(port, 1)
#define IORD_I2C_CTR(port)          IORD(port, 2)
#define IORD_I2C_RXR(port)          IORD(port, 3)
#define IORD_I2C_SR(port)           IORD(port, 4)


/* ----- Control register */
#define OC_I2C_EN (1<<7) /* Core enable bit core is: 1: enabled, 0: disabled */
#define OC_I2C_IEN (1<<6) /* Interrupt enable bit Interrupt is: 1: enabled, 0: disabled */
/* Other bits in CR are reserved */

/* ----- Command register bits */
#define I2C_CR_STA   0x80
#define I2C_CR_STO   0x40
#define I2C_CR_RD    0x20
#define I2C_CR_WR    0x10
#define I2C_CR_ACK   0x08
#define I2C_CR_IACK  0x01

/* ----- Status register bits */
#define OC_I2C_RXACK (1<<7) /* ACK received from slave  1: ACK, 0: NACK */
#define OC_I2C_BUSY  (1<<6) /* Busy bit */
#define OC_I2C_TIP   (1<<1) /* Transfer in progress */
#define OC_I2C_IF    (1<<0) /* Interrupt flag */

#define I2C_SR_TIP   0x02
#define I2C_SR_RXACK 0x80

#define I2C_FLAG_RD    1
#define I2C_FLAG_WR    0


#define I2C_DEV_NUM	1

alt_u32 i2c_baseaddr[I2C_DEV_NUM] = {
	CSI_I2C_BASE,
};


void _i2c_wait_tip(alt_u32 base);

void i2c_enable(alt_u32 index);
void i2c_clock_set(alt_u32 index, alt_u32 baud);
void i2c_disable(alt_u32 index);
alt_u32 i2c_write(alt_u32 index, alt_u8 address, alt_u8* data, alt_u32 len);
alt_u32 i2c_read(alt_u32 index, alt_u8 address, alt_u8* data, alt_u32 len);

/**
 *
 */
int i2cCmd(void)
{
	alt_u32 volatile *rpc = (alt_u32*)DPRAM_BASE;

	switch(MB_CMD(rpc[0])){
	case 1:
		/* enableI2C(int index) */
		i2c_enable(MB_SUB(rpc[0]));
		break;
	case 2:
		/* setI2CClock(int index, int baud) */
		i2c_clock_set(MB_SUB(rpc[0]), rpc[1]);
		break;
	case 3:
		/* disableI2C(int index) */
		i2c_disable(MB_SUB(rpc[0]));
		break;
	case 5:
		rpc[1] = i2c_read(MB_SUB(rpc[0]), rpc[1], (alt_u8*)&rpc[3], rpc[2]);
		break;
	case 8:
		rpc[1] = i2c_write(MB_SUB(rpc[0]), rpc[1], (alt_u8*)&rpc[3], rpc[2]);
		break;
	}

	return 0;
}

/**
 *
 */
void i2c_enable(alt_u32 index)
{
	if(index >= I2C_DEV_NUM){
		return;
	}
	alt_u32 baseaddr = i2c_baseaddr[index];

	/* Setup prescaler for 100KHz with sysclk of 154MHz */
	int prescale = 154000000/(5*100000);

	IOWR_I2C_CTR(baseaddr, 0x00); /* Disable core */

	IOWR_I2C_PRERLO(baseaddr,  prescale & 0xff);
	IOWR_I2C_PRERHI(baseaddr, (prescale & 0xff00)>>8);

	IOWR_I2C_CTR(baseaddr, OC_I2C_EN); /* Enable core */
}

/**
 *
 */
void i2c_clock_set(alt_u32 index, alt_u32 baud)
{
	if(index >= I2C_DEV_NUM){
		return;
	}
	alt_u32 baseaddr = i2c_baseaddr[index];
	int prescale = 154000000/(5*baud);	/* Setup prescaler for baud with sysclk of 154MHz */

	/* disable core */
	IOWR_I2C_CTR(baseaddr, 0x00);

	IOWR_I2C_PRERLO(baseaddr,  prescale & 0xFF);
	IOWR_I2C_PRERHI(baseaddr, (prescale & 0xFF00)>>8);

	/* enable core */
	IOWR_I2C_CTR(baseaddr, OC_I2C_EN);
}

/**
 *
 */
void i2c_disable(alt_u32 index)
{
	if(index >= I2C_DEV_NUM){
		return;
	}
	alt_u32 baseaddr = i2c_baseaddr[index];

	/* disable core */
	IOWR_I2C_CTR(baseaddr, I2C_CR_STO);
}


/**
 *
 */
alt_u32 i2c_read(alt_u32 index, alt_u8 address, alt_u8* data, alt_u32 len)
{
	if(index >= I2C_DEV_NUM){
		return 0xFF;
	}
	alt_u32 baseaddr = i2c_baseaddr[index];
	int i;

	_i2c_wait_tip(baseaddr);

	// write address
	IOWR_I2C_TXR(baseaddr, (address<<1)|1);
	IOWR_I2C_CR(baseaddr, I2C_CR_STA | I2C_CR_WR);
	_i2c_wait_tip(baseaddr);
	if(IORD_I2C_SR(baseaddr) & I2C_SR_RXACK){
		return -2;
	}

	// read data
	for(i=0; i<len; i++){
		if(i==(len-1)){
			IOWR_I2C_CR(baseaddr, I2C_CR_RD  | I2C_CR_ACK | I2C_CR_STO);
		}else{
			IOWR_I2C_CR(baseaddr, I2C_CR_RD);
		}
		_i2c_wait_tip(baseaddr);
		*(data + i) = IORD_I2C_RXR(baseaddr);
	}

	return 0;
}

/**
 *
 */
alt_u32 i2c_write(alt_u32 index, alt_u8 address, alt_u8* data, alt_u32 len)
{
	if(index >= I2C_DEV_NUM){
		return -1;
	}
	alt_u32 baseaddr = i2c_baseaddr[index];
	alt_u32 i;

	_i2c_wait_tip(baseaddr);

	// write address
	IOWR_I2C_TXR(baseaddr, address<<1);
	IOWR_I2C_CR(baseaddr, I2C_CR_STA | I2C_CR_WR);
	_i2c_wait_tip(baseaddr);
	if(IORD_I2C_SR(baseaddr) & I2C_SR_RXACK){
		return -2;
	}

	for(i=0; i<len-1; i++){
		IOWR_I2C_TXR(baseaddr, data[i]);
		IOWR_I2C_CR(baseaddr, I2C_CR_WR);
		_i2c_wait_tip(baseaddr);
		if(IORD_I2C_SR(baseaddr) & I2C_SR_RXACK){
			return -2;
		}
	}
	IOWR_I2C_TXR(baseaddr, data[len-1]);
	IOWR_I2C_CR(baseaddr, I2C_CR_WR | I2C_CR_STO);
	_i2c_wait_tip(baseaddr);
	if(IORD_I2C_SR(baseaddr) & I2C_SR_RXACK){
		return -2;
	}
	return 0;
}

/**
 *
 */
void _i2c_wait_tip(alt_u32 base)
{
	while(IORD_I2C_SR(base) & I2C_SR_TIP);
}