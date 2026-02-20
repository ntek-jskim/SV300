/*
 * @brief EA OEM 4357 board file
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "os_port.h"
#include "board.h"
#include "string.h"
#include "stopwatch.h"

//#include "retarget.h"
#include "pca9532.h"
#include "ea_lcd_board.h"
#include "tsc2046_touch.h"
#include "mem_tests.h"
#include "stdio.h"
#include "stdarg.h"
#include "time.h"
/** @ingroup BOARD_EA_OEM_4357
 * @{
 */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

#define JOYSTICK_UP_GPIO_PORT_NUM         4
#define JOYSTICK_UP_GPIO_BIT_NUM         10
#define JOYSTICK_DOWN_GPIO_PORT_NUM       4
#define JOYSTICK_DOWN_GPIO_BIT_NUM       13
#define JOYSTICK_LEFT_GPIO_PORT_NUM       4
#define JOYSTICK_LEFT_GPIO_BIT_NUM        9
#define JOYSTICK_RIGHT_GPIO_PORT_NUM      4
#define JOYSTICK_RIGHT_GPIO_BIT_NUM      12
#define JOYSTICK_PRESS_GPIO_PORT_NUM      4
#define JOYSTICK_PRESS_GPIO_BIT_NUM       8


static uint16_t memreg_shadow = 0;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* System configuration variables used by chip driver */
const uint32_t ExtRateIn = 0;
const uint32_t OscRateIn = 12000000;

LCD_CONFIG_T EA4357_LCD;

extern void tickSet(uint32_t sec, uint32_t msec, uint32_t mode);

/*****************************************************************************
 * Private functions
 ****************************************************************************/

///* Initializes default settings for UDA1380 */
//static Status Board_Audio_CodecInit(int micIn)
//{
//	/* Reset UDA1380 */
//	pca9532_setLeds(1<<6, 0);
//	Board_DelayUs(1);
//	pca9532_setLeds(0, 1<<6);
//	Board_DelayUs(1);

//	while (!UDA1380_Init(UDA1380_MIC_IN_LR & - (micIn != 0))) {}

//	return SUCCESS;
//}

#ifdef CORE_M4
static Status readUniqueID(uint8_t* buf)
{
//	int16_t len = 6;
//	int i = 0;
//	uint8_t off[1]; // 8 bit addressing

//	if (buf == NULL) {
//		return ERROR;
//	}

//	off[0] = 0xfa;

//	if (Chip_I2C_MasterSend(I2C0, 0x50, off, 1) == 1) {
//		for (i = 0; i < 0x2000; i++)
//			;
//		if (Chip_I2C_MasterRead(I2C0, 0x50, buf, len) == len) {
//			if ((buf[0] != 0x00) || (buf[1] != 0x04) || (buf[2] != 0xA3)) {
//				printf("EEPROM EUI-48: Invalid manufacturer id\r\n");
//				return ERROR;
//			}
//			return SUCCESS;
//		}
//	}

	return ERROR;
}
#endif

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Basic implementation of delay function. For the M0 core a rough time
   estimate with a for-loop is used instead of the StopWatch functionality
   on the M4. */
void Board_DelayMs(uint32_t ms)
{
#ifdef CORE_M4
	static int initialized = 0;
	if (!initialized) {
		initialized = 1;
		StopWatch_Init();
	}
	StopWatch_DelayMs(ms);
#else
	uint32_t i;
	while(ms >= 70000) {
		Board_DelayMs(10000);
		ms -= 10000;
	}
	i = 30000*ms;
	while (i--);
#endif
}

/* Basic implementation of delay function. For the M0 core a rough time
   estimate with a for-loop is used instead of the StopWatch functionality
   on the M4. */
void Board_DelayUs(uint32_t us)
{
#ifdef CORE_M4
	static int initialized = 0;
	if (!initialized) {
		initialized = 1;
		StopWatch_Init();
	}
	StopWatch_DelayUs(us);
#else
	uint32_t i = 41*us;
	while (i--);
#endif
}

const GPIO_ID RTS_GPIO[] = {
	{3, 1},
	{6, 15},
	{4, 10},
};

/* Initialize UART pins */
void Board_UART_Init(LPC_USART_T *pUART)
{
	// IO
	if (pUART == LPC_USART0) {
		Chip_SCU_PinMuxSet(0x9, 5, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC7));		
		Chip_SCU_PinMuxSet(0x9, 6, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC7));	/* PF.11 : UART0_RXD */
		// Chip_SCU_PinMuxSet(0x6, 2, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC0));			//GPIO3.1
		// Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, RTS_GPIO[0].port, RTS_GPIO[0].num, 1);
		// Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, RTS_GPIO[0].port, RTS_GPIO[0].num);
	}
	// SMB
	else if (pUART == LPC_UART1) {
		Chip_SCU_PinMuxSet(0xC, 13, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC2));	
		Chip_SCU_PinMuxSet(0xC, 14, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));	/* PC.14 : UART1_RXD */		
		Chip_SCU_PinMuxSet(0xd, 1, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC4));			//GPIO6.15
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, RTS_GPIO[1].port, RTS_GPIO[1].num, 1);
		Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, RTS_GPIO[1].port, RTS_GPIO[1].num);
	}
	// MMB
	else if (pUART == LPC_USART2) {
		Chip_SCU_PinMuxSet(0xA, 1, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC3));		
		Chip_SCU_PinMuxSet(0xA, 2, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3));	/* PA.2 : UART2_RXD */	
		Chip_SCU_PinMuxSet(0xA, 3, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC0));			//GPIO4.10
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, RTS_GPIO[2].port, RTS_GPIO[2].num, 1);
		Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, RTS_GPIO[2].port, RTS_GPIO[2].num);
	}
	//DEBUG		
	else if (pUART == LPC_USART3) {
		Chip_SCU_PinMuxSet(0x2, 3, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC2));			/* P2.3 : UART3_TXD */
		Chip_SCU_PinMuxSet(0x2, 4, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));	/* P2.4 : UART3_RXD */		
	}
}

/* Initialize debug output via UART for board */
void Board_Debug_Init(void)
{
#if defined(DEBUG_UART)
	Board_UART_Init(DEBUG_UART);
 
	Chip_UART_Init(DEBUG_UART);
	Chip_UART_SetBaud(DEBUG_UART, 115200);
	Chip_UART_ConfigData(DEBUG_UART, UART_LCR_WLEN8 | UART_LCR_SBS_1BIT | UART_LCR_PARITY_DIS);
	//Chip_UART_SetupFIFOS(DEBUG_UART, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV0));

	/* Enable UART Transmit */
	Chip_UART_TXEnable(DEBUG_UART);
#endif
}

/* Sends a character on the UART */
void Board_UARTPutChar(char ch)
{
#if defined(DEBUG_UART)
	/* Wait for space in FIFO */
	while ((Chip_UART_ReadLineStatus(DEBUG_UART) & UART_LSR_THRE) == 0) {}
	Chip_UART_SendByte(DEBUG_UART, (uint8_t) ch);
#endif
}

/* Gets a character from the UART, returns EOF if no character is ready */
int Board_UARTGetChar(void)
{
#if defined(DEBUG_UART)
	if (Chip_UART_ReadLineStatus(DEBUG_UART) & UART_LSR_RDR) {
		return (int) Chip_UART_ReadByte(DEBUG_UART);
	}
#endif
	return EOF;
}

/* Outputs a string on the debug UART */
void Board_UARTPutSTR(char *str)
{
#if defined(DEBUG_UART)
	while (*str != '\0') {
		Board_UARTPutChar(*str++);
	}
#endif
}

/* Transmit and receive ring buffers */
STATIC RINGBUFF_T txring[3], rxring[3];

/* Transmit and receive ring buffer sizes */
#define UART_SRB_SIZE 256	/* Send */
#define UART_RRB_SIZE 256	/* Receive */

/* Transmit and receive buffers */
static uint8_t rxbuff[3][UART_RRB_SIZE], txbuff[3][UART_SRB_SIZE];
static LPC_USART_T *uartBase[] = {LPC_USART0, LPC_UART1, LPC_USART2};
static int uartIrq[] = {USART0_IRQn, UART1_IRQn, USART2_IRQn};

int UART_Init(int uport, int baud)
{
	uint8_t key;
	int bytes;
	LPC_USART_T *pUart = uartBase[uport];
	
	Board_UART_Init(pUart);

	/* Setup UART for 115.2K8N1 */
	Chip_UART_Init(pUart);
	Chip_UART_SetBaud(pUart, baud);
	Chip_UART_ConfigData(pUart, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT));
	Chip_UART_SetupFIFOS(pUart, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV2));
	Chip_UART_TXEnable(pUart);

	/* Before using the ring buffers, initialize them using the ring
	   buffer init function */
	RingBuffer_Init(&rxring[uport], rxbuff[uport], 1, UART_RRB_SIZE);
	RingBuffer_Init(&txring[uport], txbuff[uport], 1, UART_SRB_SIZE);

	/* Reset and enable FIFOs, FIFO trigger level 3 (14 chars) */
	Chip_UART_SetupFIFOS(pUart, (UART_FCR_FIFO_EN | UART_FCR_RX_RS |
							UART_FCR_TX_RS | UART_FCR_TRG_LEV3));

	/* Enable receive data and line status interrupt */
	Chip_UART_IntEnable(pUart, (UART_IER_RBRINT | UART_IER_RLSINT));

	/* preemption = 1, sub-priority = 1 */
#if 1	
   // FreeRTOS 사용시 priority는 6보다 크거나 같아야 한다
	NVIC_SetPriority(uartIrq[uport], 7);
#endif

	NVIC_EnableIRQ(uartIrq[uport]);
	
	return 0;
}

// startup_lpc43xx.c 변경, UART0_IRQHandler -> USART0_IRQHandler
void USART0_IRQHandler(void)
{
	/* Want to handle any errors? Do it here. */

	/* Use default ring buffer handler. Override this with your own
	   code if you need more capability. */
	Chip_UART_IRQRBHandler(LPC_USART0, &rxring[0], &txring[0]);
}

void UART1_IRQHandler(void)
{
	/* Want to handle any errors? Do it here. */

	/* Use default ring buffer handler. Override this with your own
	   code if you need more capability. */
	Chip_UART_IRQRBHandler(LPC_UART1, &rxring[1], &txring[1]);
}

// startup_lpc43xx.c 변경, USART2_IRQHandler -> USART2_IRQHandler
void USART2_IRQHandler(void)
{
	/* Want to handle any errors? Do it here. */

	/* Use default ring buffer handler. Override this with your own
	   code if you need more capability. */
	Chip_UART_IRQRBHandler(LPC_USART2, &rxring[2], &txring[2]);
}

int UartSend(int uport, void *pbuf, int count) {
	return Chip_UART_SendRB(uartBase[uport], &txring[uport], pbuf, count);
}

int UartRecv(int uport, void *pbuf, int count) {
	return Chip_UART_ReadRB(uartBase[uport], &rxring[uport], pbuf, count);
}


//
//
//

const PINMUX_GRP_T LED_PIN[] = {
	{0xf, 8, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
	{0xf, 9, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
	{0xf, 10, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
//	{0xf, 11, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
};

const GPIO_ID LED_GPIO[] = {
	{7, 22},		// run
	{7, 23},		// sts
	{7, 24},		// comm
//	{7, 25},
};
// KEY PIN
const PINMUX_GRP_T KEY_PIN[] = {	
	{0xd, 11, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN)},
	{0xd, 12, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN)},
	{0xd, 13, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN)},
	{0xd, 14, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN)},
	{0xd, 15, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN)},		
	{0xd, 16, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN)},
};
// KEY GPIO
const GPIO_ID KEY_GPIO[] = {
	{6, 25},
	{6, 26},
	{6, 27},
	{6, 28},	
	{6, 29},	
	{6, 30},	
};

const PINMUX_GRP_T WDT_PIN[] = {	
	{1, 3, (SCU_MODE_FUNC0 | SCU_MODE_PULLDOWN)},
	{1, 4, (SCU_MODE_FUNC0 | SCU_MODE_PULLDOWN)},
};
// WDT & Wiring Mode
const GPIO_ID WDT_GPIO[] = {
	{0, 10},	// WDT_EN
	{0, 11},	// WDT_CLR
};
const PINMUX_GRP_T ETC_PIN[] = {	
	{0x1, 5, (SCU_MODE_FUNC0 | SCU_MODE_PULLDOWN)},	// wiring mode
	{0x9, 3, (SCU_MODE_FUNC0 | SCU_MODE_PULLDOWN)},	// bug: FUNC4 -> FUNC0
	{0x9, 0, (SCU_MODE_FUNC0 | SCU_MODE_PULLUP)}	// GPIO_RES2(reset PHY)
};
const GPIO_ID ETC_GPIO[] = {
	{1, 8},	// W_MODE
	{4, 15},// Beeper	
	{4, 12}	// reset PHY
};

// const PINMUX_GRP_T WDT_PIN[] = {	
// 	{0xe, 0, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
// 	{0xe, 1, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
// };
// // WDT & Wiring Mode
// const GPIO_ID WDT_GPIO[] = {
// 	{7, 0},	// WDT_EN
// 	{7, 1},	// WDT_CLR
// };
// const PINMUX_GRP_T ETC_PIN[] = {	
// 	{0xe, 2, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},	// wiring mode
// 	{0x9, 3, (SCU_MODE_FUNC0 | SCU_MODE_PULLDOWN)},	// bug: FUNC4 -> FUNC0
// 	{0x9, 0, (SCU_MODE_FUNC0 | SCU_MODE_PULLUP)}		// GPIO_RES2(reset PHY)
// };
// const GPIO_ID ETC_GPIO[] = {
// 	{7, 2},	// W_MODE
// 	{4, 15},	// Beeper	
// 	{4, 12}	// reset PHY
// };


//
const PINMUX_GRP_T EINT_PIN[] = {
	{0x3, 1, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP)},
	{0x3, 2, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP)},
	{0xC, 2, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP)},
	{0xC, 3, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP)},
	{0xC, 9, (SCU_MODE_FUNC4 | SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP)},
};
const GPIO_ID EINT_GPIO[] = {
	{5, 8},	// Meter0 IRQ#0
	{5, 9},	// Meter0 IRQ#1, unused
	{6, 1},	// Meter1 IRQ#0, unused
	{6, 2},	// Meter1 IRQ#1, unused
	{6, 8}, // TSC
};
//	/* SSP0 */


#ifdef _HW_VER_00
const PINMUX_GRP_T SSP0_PIN_MAN[] = {
	{0x3, 0,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},//SSP0_SCK
	{0xF, 1,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	//SSP0_SSEL -> (GPIO7.16) 	
	{0xF, 2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MISO
	{0xF, 3,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MOSI
};
const PINMUX_GRP_T SSP0_PIN_AUTO[] = {
	{0x3, 0,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},//SSP0_SCK
	{0xF, 1,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_SSEL(AUTO)
	{0xF, 2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MISO
	{0xF, 3,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MOSI
};
#else
const PINMUX_GRP_T SSP0_PIN_MAN[] = {
	{0xF, 0,  (SCU_PINIO_FAST | SCU_MODE_FUNC0)},//SSP0_SCK
	{0xF, 1,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	//SSP0_SSEL -> (GPIO7.16) 	
	{0xF, 2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MISO
	{0xF, 3,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MOSI
};
const PINMUX_GRP_T SSP0_PIN_AUTO[] = {
	{0xF, 0,  (SCU_PINIO_FAST | SCU_MODE_FUNC0)},//SSP0_SCK
	{0xF, 1,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_SSEL(AUTO)
	{0xF, 2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MISO
	{0xF, 3,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP0_MOSI
};
#endif

const PINMUX_GRP_T SSP1_PIN_MAN[] = {
	{0xF, 4,  (SCU_PINIO_FAST | SCU_MODE_FUNC0)},//SSP1_CLK
	{0xF, 5,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	//SSP0_SSEL -> (GPIO7.19) 
	{0xF, 6,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP1_MISO
	{0xF, 7,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP1_MOSI
};

const PINMUX_GRP_T SSP1_PIN_AUTO[] = {
	{0xF, 4,  (SCU_PINIO_FAST | SCU_MODE_FUNC0)},//SSP1_CLK
	{0xF, 5,  (SCU_MODE_FUNC2)},
	{0xF, 6,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP1_MISO
	{0xF, 7,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},//SSP1_MOSI
};

const GPIO_ID SSEL_GPIO[] = {
	{7, 16},
	{7, 19},
};


/* Initializes board LED(s) */
static void Board_LED_Init()
{
	int i;
	
	Chip_SCU_SetPinMuxing(LED_PIN, sizeof(LED_PIN) / sizeof(PINMUX_GRP_T));
	for (i=0; i<3; i++) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, LED_GPIO[i].port, LED_GPIO[i].num, 1);
	}
}

#if 0
/* Sets the state of a board LED to on or off */
void Board_LED_Set(uint8_t n, bool On)
{
	if (On) 
		Chip_GPIO_SetPinOutLow (LPC_GPIO_PORT, LED_GPIO[n].port, LED_GPIO[n].num);
	else
		Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, LED_GPIO[n].port, LED_GPIO[n].num);		
}

void Board_LED_On(uint8_t n) {
	Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, LED_GPIO[n].port, LED_GPIO[n].num);
}

void Board_LED_Off(uint8_t n) {
	Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, LED_GPIO[n].port, LED_GPIO[n].num);
}


///* Returns the current state of a board LED */
//bool Board_LED_Test(uint8_t LEDNumber)
//{
////	if (LEDNumber < 8) {
////		uint32_t val = pca9532_getLedState(0);
////		return (bool) ((val >> (LEDNumber+8)) & 0x1);
////	} else if (LEDNumber < 24) {
////		LEDNumber -= 8;
////		return (bool) ((memreg_shadow >> LEDNumber) & 1);
////	}
//	return false;
//}

void Board_LED_Toggle(uint8_t n)
{
//	static int led;
//	
//	led ^= (1<<n);
//	Board_LED_Set(n, led & (1<<n));
	Chip_GPIO_SetPinToggle(LPC_GPIO_PORT, LED_GPIO[n].port, LED_GPIO[n].num);
}
#endif

/* Returns the MAC address assigned to this board */
void Board_ENET_GetMacADDR(uint8_t *mcaddr)
{
	const uint8_t boardmac[] = {0x00, 0x60, 0x37, 0x12, 0x34, 0x56};

#ifdef CORE_M4
	if (readUniqueID(mcaddr) != SUCCESS) {
		// Failed to read unique ID so go with the default
		memcpy(mcaddr, boardmac, 6);
	}
#else
	// The readUniqueID() function hangs on the M0 so use hardcoded MAC instead
	memcpy(mcaddr, boardmac, 6);
#endif
}

#if 0 // debug.c로 이동 
void Board_GPIO_Init() 
{
	int i;
		
	// WDT
	Chip_SCU_SetPinMuxing(WDT_PIN, sizeof(WDT_PIN) / sizeof(PINMUX_GRP_T));
	for (i=0; i<2; i++) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, WDT_GPIO[i].port, WDT_GPIO[i].num, 1);
	}
	
	// EINT0, EINT1, EINT2, EINT3
	Chip_SCU_SetPinMuxing(EINT_PIN, sizeof(EINT_PIN) / sizeof(PINMUX_GRP_T));
	for (i=0; i<4; i++) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, EINT_GPIO[i].port, EINT_GPIO[i].num, 0);
	}	

	// ETC, add PHY reset
	Chip_SCU_SetPinMuxing(ETC_PIN, sizeof(ETC_PIN) / sizeof(PINMUX_GRP_T));
	for (i=0; i<3; i++) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, ETC_GPIO[i].port, ETC_GPIO[i].num, 1);
	}		

	Chip_SCU_SetPinMuxing(LED_PIN, sizeof(LED_PIN) / sizeof(PINMUX_GRP_T));
	for (i=0; i<4; i++) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, LED_GPIO[i].port, LED_GPIO[i].num, 1);
	}
	// negate PHY reset
	Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, ETC_GPIO[2].port, ETC_GPIO[2].num);
   //Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, ETC_GPIO[2].port, ETC_GPIO[2].num);
}

void Board_WDT_Enable() {
	Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, WDT_GPIO[0].port, WDT_GPIO[0].num);
}

void Board_WDT_Disable() {
	Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, WDT_GPIO[0].port, WDT_GPIO[0].num);
}

void Board_WDT_Clear() {
	Chip_GPIO_SetPinToggle(LPC_GPIO_PORT, WDT_GPIO[1].port, WDT_GPIO[1].num);	
//	static int w=0;
//	if (++w & 0x1) 
//		Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, WDT_GPIO[1].port, WDT_GPIO[1].num);
//	else
//		Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, WDT_GPIO[1].port, WDT_GPIO[1].num);	
}

void Board_Buzzer_Toggle(void) {
	Chip_GPIO_SetPinToggle(LPC_GPIO_PORT, ETC_GPIO[1].port, ETC_GPIO[1].num);
//	static int c=0;
//	if (++c & 0x1) 
//		Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, ETC_GPIO[1].port, ETC_GPIO[1].num);
//	else
//		Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, ETC_GPIO[1].port, ETC_GPIO[1].num);
}

void Board_Buzzer_Off(void) {
	Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, ETC_GPIO[1].port, ETC_GPIO[1].num);
}

void selWire(int mode) {
	if (mode) 
		Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, ETC_GPIO[0].port, ETC_GPIO[0].num);
	else
		Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, ETC_GPIO[0].port, ETC_GPIO[0].num);
}
#ifdef	LANCHECK
void resetPhy(int mode) {
	if (mode) 
		Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, ETC_GPIO[2].port, ETC_GPIO[2].num);
	else
		Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, ETC_GPIO[2].port, ETC_GPIO[2].num);
}
#endif


uint32_t KEY_Read(int n) {
	return Chip_GPIO_GetPinState(LPC_GPIO_PORT, KEY_GPIO[n].port, KEY_GPIO[n].num);
}


/* Set up and initialize all required blocks and functions related to the
   board hardware */
void Board_Init(void)
{
	/* Sets up DEBUG UART */
	Board_Debug_Init();

	/* Initializes GPIO */
	//Chip_GPIO_Init(LPC_GPIO_PORT);
	Board_GPIO_Init();	// WDT, EINT, W_MODE, PWM
	
	// Button
	Board_Buttons_Init();

	/* Initialize LEDs */
//	Board_LED_Init();
	
#if defined(USE_RMII)
	Chip_ENET_RMIIEnable(LPC_ETHERNET);
#else
	Chip_ENET_MIIEnable(LPC_ETHERNET);
#endif

#ifdef	LANCHECK
	resetPhy(1);
#endif

}

#endif   // 0

// LCD Back Light
#define SCT_PWM_LCD_BL	14        /* COUT14 */
#define	SCT_PWM_RATE	1000


void Board_PWM_Init(int freq) {
	/* Initialize the SCT as PWM and set frequency */
	Chip_SCTPWM_Init(LPC_SCT);
	Chip_SCTPWM_SetRate(LPC_SCT, freq);
	
	// LCD Back Light
	Chip_SCU_PinMuxSet(0x7, 0, (SCU_MODE_INACT | SCU_MODE_FUNC1));
	
	/* Use SCT0_OUT1 pin */
	Chip_SCTPWM_SetOutPin(LPC_SCT, 1, SCT_PWM_LCD_BL);

	/* Start with 0% duty cycle */
	Chip_SCTPWM_SetDutyCycle(LPC_SCT, 1, 0);
	Chip_SCTPWM_Start(LPC_SCT);
}


//void PWM_SetDutyCycle(int val) {
//	/* Increment or Decrement Dutycycle by 0.5% every 10ms */
//	Chip_SCTPWM_SetDutyCycle(LPC_SCT, 1, Chip_SCTPWM_PercentageToTicks(LPC_SCT, val));
//}
void Board_LcdBackLightCtrl(int prop) {
	Chip_SCTPWM_SetDutyCycle(LPC_SCT, 1, Chip_SCTPWM_PercentageToTicks(LPC_SCT, prop));
}

//
//
//

#define AUTOPROG_ON     1
#define PAGE_ADDR       0x01

/* Read data from EEPROM */
/* size must be multiple of 4 bytes */
void EEPROM_Read(uint32_t pageOffset, uint32_t pageAddr, uint32_t* ptr, uint32_t size)
{
	uint32_t i = 0;
	uint32_t *pEepromMem = (uint32_t*)EEPROM_ADDRESS(pageAddr,pageOffset);
	for(i = 0; i < size/4; i++) {
		ptr[i] = pEepromMem[i];
	}
}

/* Erase a page in EEPROM */
void EEPROM_Erase(uint32_t pageAddr)
{
	uint32_t i = 0;
	uint32_t *pEepromMem = (uint32_t*)EEPROM_ADDRESS(pageAddr,0);
	for(i = 0; i < EEPROM_PAGE_SIZE/4; i++) {
		pEepromMem[i] = 0;
#if AUTOPROG_ON
		Chip_EEPROM_WaitForIntStatus(LPC_EEPROM, EEPROM_INT_ENDOFPROG);
#endif
	}
#if (AUTOPROG_ON == 0)
	Chip_EEPROM_EraseProgramPage(LPC_EEPROM);
#endif
}

/* Write data to a page in EEPROM */
/* size must be multiple of 4 bytes */
void EEPROM_Write(uint32_t pageOffset, uint32_t pageAddr, uint32_t* ptr, uint32_t size)
{
	uint32_t i = 0;
	uint32_t *pEepromMem = (uint32_t*)EEPROM_ADDRESS(pageAddr,pageOffset);

	if(size > EEPROM_PAGE_SIZE - pageOffset)
		size = EEPROM_PAGE_SIZE - pageOffset;

	for(i = 0; i < size/4; i++) {
		pEepromMem[i] = ptr[i];
#if AUTOPROG_ON
		Chip_EEPROM_WaitForIntStatus(LPC_EEPROM, EEPROM_INT_ENDOFPROG);
#endif
	}

#if (AUTOPROG_ON == 0)
	Chip_EEPROM_EraseProgramPage(LPC_EEPROM);
#endif
}



void ChipEepInit() {
	Chip_EEPROM_Init(LPC_EEPROM);
#if AUTOPROG_ON	
	Chip_EEPROM_SetAutoProg(LPC_EEPROM,EEPROM_AUTOPROG_AFT_1WORDWRITTEN);
#else
	Chip_EEPROM_SetAutoProg(LPC_EEPROM,EEPROM_AUTOPROG_OFF);
#endif	
}


int ChipEepRead(int pgno, void *buf, int nr) {
	int nleft=nr, n;
	uint8_t *ebuf=buf;
	
	while (nleft > 0) {
		n = (nleft > EEPROM_PAGE_SIZE) ? EEPROM_PAGE_SIZE : nleft;
		EEPROM_Read(0, pgno, (uint32_t*)ebuf, n);
		
		ebuf += n;		
		nleft -= n;
		pgno++;
	}
	
	return nr-nleft;
}

int ChipEepWrite(int pgno, void *buf, int nw) {
	int nleft=nw, n;
	uint8_t *ebuf=buf;
	
	while (nleft > 0) {
		n = (nleft > EEPROM_PAGE_SIZE) ? EEPROM_PAGE_SIZE : nleft;
		EEPROM_Erase(pgno);
		EEPROM_Write(0, pgno, (uint32_t*)ebuf, n);
		
		ebuf += n;		
		nleft -= n;
		pgno++;
	}
	
	return nw-nleft;	
}


#ifdef CHIP_EEP_TEST

uint8_t eepbuf[256];

void EEPROM_Test() {
	int i;
	
	/* Erase page */
	EEPROM_Erase(PAGE_ADDR);

	/* Write header + size + data to page */
	printf("\r\nEEPROM write...\r\n");
	for (i=0;i<PAGE_ADDR; i++) eepbuf[i] = i;
	EEPROM_Write(0, PAGE_ADDR, (uint32_t*)eepbuf, sizeof(eepbuf));
	printf("Reading back string...\r\n");

	for (i=0;i<PAGE_ADDR; i++) eepbuf[i] = 0;
	/* Read all data from EEPROM page */
	EEPROM_Read(0, PAGE_ADDR, (uint32_t*)eepbuf, EEPROM_PAGE_SIZE);
	
	for (i=0;i<PAGE_ADDR; i++) {
		if (eepbuf[i] != i) {
			printf("verify failed ....\n");
			return;
		}
	}
	
	printf("EEPROM Test Complete ...\n");
}

#endif

///* Sets up board specific ADC interface */
//void Board_ADC_Init(void)
//{
//}
#define I2C_SLAVE_EEPROM_SIZE       260
#define I2C_SLAVE_EEPROM_ADDR       0x54 //(101 0100)

/* Data area for slave operations */
static uint8_t seep_data[I2C_SLAVE_EEPROM_SIZE + 1];
static uint8_t _eepbuf[I2C_SLAVE_EEPROM_SIZE];
static uint8_t iox_data[2];	/* PORT0 input port, PORT1 Output port */
I2C_XFER_T seep_xfer;	// FRAM 
I2C_XFER_T tsc_xfer;		// Touch Screen Controller
static int mode_poll;


static OsMutex	i2cMutex;
extern void _eepDone(OsTaskId tid, int flag);
extern int  _eepWait(int);


//osMutexId_t i2cMutex;

//void _signalSend(int flag, osThreadId_t tid) {
//	osThreadFlagsSet(tid, flag);
//}

//int _signalWait(int flag) {
//	uint32_t result;
//	result = osThreadFlagsWait(flag, osFlagsWaitAll, 50);FramRead
//	
//	if (result == osFlagsErrorTimeout) {
//		printf("I2C Timeout ...\n");
//		return -1;
//	}
//	else {
//		return 0;
//	}
//}


void i2c_set_mode(I2C_ID_T id, int polling)
{
	if (!polling) {
		mode_poll &= ~(1 << id);
		Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandler);
#if 1		
		// FreeRTOS 사용시 priority는 6보다 크거나 같아야 한다
		NVIC_SetPriority(id == I2C0 ? I2C0_IRQn : I2C1_IRQn, 7);
#endif		
		NVIC_EnableIRQ(id == I2C0 ? I2C0_IRQn : I2C1_IRQn);
	}
	else {
		mode_poll |= 1 << id;
		NVIC_DisableIRQ(id == I2C0 ? I2C0_IRQn : I2C1_IRQn);
		Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandlerPolling);
	}
}

void I2C0_IRQHandler(void)
{
	Chip_I2C_MasterStateHandler(I2C0);
}


//void Board_I2C0_Init(void)
//{
//	Chip_SCU_I2C0PinConfig(I2C0_STANDARD_FAST_MODE);

//	/* Initialize I2C */
//	Chip_I2C_Init(I2C0);
//	Chip_I2C_SetClockRate(I2C0, 400000);
//	/* Set default mode to interrupt */
//	i2c_set_mode(I2C0, 1);
//}

#ifdef	EXT_RTC	// 2023-2, I2C RTC

int I2C_RtcWrite(void *rb) 
{
	int tmp, i, rval;
	I2C_XFER_T *xfer = &tsc_xfer;
	uint8_t buf[8];
		
	buf[0] = 0;
	memcpy(buf+1, rb, 7);
		
	xfer->slaveAddr = 0xd0>>1;		// 1101 000x
	xfer->txSz = 8;
	xfer->txBuff = buf;
	xfer->rxSz = 0;
	xfer->rxBuff = 0;
	xfer->tid = os_tsk_self();
	
	osAcquireMutex(&i2cMutex);	
   
	Chip_I2C_SetClockRate(I2C0, 100000);	// 1000kHz
	rval = Chip_I2C_MasterTransfer(I2C0, xfer);
	if (rval != I2C_STATUS_DONE) {
		printf("I2C_RtcWrite, Error Code=%d\n", rval);
	}
	else {
		printf("I2C_RtcWrite ...\n");
	}
   
	osReleaseMutex(&i2cMutex);
	
	return rval;	
	
}

int I2C_RtcRead(void *rb) 
{
	int tmp, i, rval;
	I2C_XFER_T *xfer = &tsc_xfer;
	uint8_t *pb = (uint8_t *)rb;
	uint8_t addr = 0;
		
	xfer->slaveAddr = 0xd0>>1;		// 1101 000x
	xfer->txSz = 1;
	xfer->txBuff = &addr;
	xfer->rxSz = 8;
	xfer->rxBuff = pb;
	xfer->tid = os_tsk_self();
	
	osAcquireMutex(&i2cMutex);	
   
	Chip_I2C_SetClockRate(I2C0, 100000);	// 1000kHz
	rval = Chip_I2C_MasterTransfer(I2C0, xfer);
	if (rval != I2C_STATUS_DONE) {
		printf("I2C_RtcRead, Error Code=%d\n", rval);
	}
	else {
		printf("I2C_RtcRead, %02x %02x %02x\n", pb[0]&0x7f, pb[1]&0x7f,pb[2]&0x1f);
	}
   
	osReleaseMutex(&i2cMutex);
	
	return rval;	
	
}
#endif


int I2C_TouchRead(void *rb)
{
#ifdef __FREERTOS		
	int tmp, i, rval;
	I2C_XFER_T *xfer = &tsc_xfer;
	uint8_t *pb = (uint8_t *)rb;
		
	xfer->slaveAddr = 0x9a>>1;		// 1001 101x
	xfer->txSz = 0;
	xfer->txBuff = 0;
	xfer->rxSz = 5;
	xfer->rxBuff = rb;

	xfer->tid = xTaskGetCurrentTaskHandle();	
	
   osAcquireMutex(&i2cMutex);	
   
	Chip_I2C_SetClockRate(I2C0, 400000);	// 1000kHz
	rval = Chip_I2C_MasterTransfer(I2C0, xfer);
	if (rval != I2C_STATUS_DONE) {
		printf("I2C_TouchRead, Error Code=%d\n", rval);
	}
	
   osReleaseMutex(&i2cMutex);
   
	return rval;
#else
#endif
//	return xfer->status;
//	/* Setup I2C transfer record */
//	i2cmXferRec.slaveAddr = 0x9a >> 1;
//	i2cmXferRec.options = 0;
//	i2cmXferRec.status = 0;
//	i2cmXferRec.txSz = 0;
//	i2cmXferRec.rxSz = 5;
//	i2cmXferRec.txBuff = 0;
//	i2cmXferRec.rxBuff = rb;
//	return Chip_I2CM_XferBlocking(LPC_I2C0, &i2cmXferRec);		
}


int I2C_EepRead(void *rb, int offset, int nr)
{
	int tmp, i, rval;
	I2C_XFER_T *xfer = &seep_xfer;
	
	xfer->slaveAddr = 0xa8 >> 1;	
	_eepbuf[0] = offset >> 8;
	_eepbuf[1] = offset;
	xfer->txSz = 2;
	xfer->txBuff = _eepbuf;	
	xfer->rxSz = nr;
	xfer->rxBuff = rb;
	xfer->tid = os_tsk_self();
		
	osAcquireMutex(&i2cMutex);
   
	Chip_I2C_SetClockRate(I2C0, 1000000);	// 1000kHz
	rval = Chip_I2C_MasterTransfer(I2C0, xfer);
	if (rval != I2C_STATUS_DONE) {
		printf("I2C_EepRead, Error Code=%d\n", rval);
	}	
   
	osReleaseMutex(&i2cMutex);
	
	return rval;
}


int I2C_EepWrite(void *tb, int offset, int nw)
{
	int tmp, i, rval;
	I2C_XFER_T *xfer = &seep_xfer;

	
	xfer->slaveAddr = 0xa8 >> 1;
	_eepbuf[0] = offset >> 8;
	_eepbuf[1] = offset;
	memcpy(&_eepbuf[2], tb, nw);
	
	xfer->txSz = nw+2;
	xfer->txBuff = _eepbuf;	
	xfer->rxSz = 0;
	xfer->rxBuff = 0;
	xfer->tid = os_tsk_self();
		
	osAcquireMutex(&i2cMutex);
   
	Chip_I2C_SetClockRate(I2C0, 1000000);	// 1000kHz
	rval = Chip_I2C_MasterTransfer(I2C0, xfer);	
	if (rval != I2C_STATUS_DONE) {
		printf("I2C_EepWrite, Error Code=%d\n", rval);
	}		
   
	osReleaseMutex(&i2cMutex);
	
	return rval;
}

int FramWrite(void *tb, int offset, int nw) {
	int nleft = nw, n, status;
	uint8_t *p = tb;
	
	while (nleft > 0) {
		n = (nleft > 256) ? 256 : nleft;
		status = I2C_EepWrite(p, offset, n);
		if (status != I2C_STATUS_DONE) 
			break;
		
		offset += n;
		p += n;
		nleft -= n;
	}	
	
	return (nleft == 0) ? 1 : 0;
}

int FramRead(void *tb, int offset, int nr) {
	int nleft = nr, n, status;
	uint8_t *p = tb;
	
	while (nleft > 0) {
		n = (nleft > 256) ? 256 : nleft;
		status = I2C_EepRead(p, offset, n);
		if (status != I2C_STATUS_DONE) 
			break;
		
		offset += n;
		p += n;
		nleft -= n;
	}	
	
	return (nleft == 0) ? 1 : 0;
}

/* Sets up board specific I2C interface */
void Board_I2C_Init(I2C_ID_T id)
{
	if (id == I2C1) {
//		/* Configure pin function for I2C1 on PE.13 (I2C1_SDA) and PE.15 (I2C1_SCL) */
//		Chip_SCU_PinMuxSet(0xE, 13, (SCU_MODE_ZIF_DIS | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC2));
//		Chip_SCU_PinMuxSet(0xE, 15, (SCU_MODE_ZIF_DIS | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC2));
		printf("*** Can't support I2C1 Peripheral ...\n");
	}
	else {
		Chip_SCU_I2C0PinConfig(I2C0_STANDARD_FAST_MODE);

		/* Init I2C */
		Chip_I2C_Init(I2C0);
		Chip_I2C_SetClockRate(I2C0, 400000);	// 400kHz
		i2c_set_mode(I2C0, 1);	// polling mode
	}
}

void I2C_Init(I2C_ID_T id) {
	Board_I2C_Init(id);
	
	if (id == I2C0) {
		osCreateMutex(&i2cMutex);
	}
}

//
//
//
extern LCD_CONFIG_T EA4357_LCD;
//extern volatile uint64_t sysTick64;

static void lcdDelay(uint32_t delay)
{
//	delay += sysTick64;
//	while (sysTick64 < delay) {}
}

void Board_SetLCDBacklight(uint8_t Intensity)
{
//	// Workaround for stupid interface. See description in board_api.h
//	if (Intensity == 1) {
//		Intensity = 100;
//	}

//	LPC_GPIO_PORT->DIR[3] |= (1 << 8);
//	if (Intensity) {
//		//ea_lcdb_ctrl_backlightContrast(Intensity);		
//		LPC_GPIO_PORT->SET[3]  = (1 << 8);
//	}
//	else {
//		LPC_GPIO_PORT->CLR[3]  = (1 << 8);
//	}
}

void Board_LCD_Init() 
{		
	int32_t i = 0;
	
	/* Reset LCD and wait for reset to complete */
	Chip_RGU_TriggerReset(RGU_LCD_RST);
	while (Chip_RGU_InReset(RGU_LCD_RST)) {
	}	
	
//	// 800x480, 5 inch LCD
//	EA4357_LCD.HBP  = 45;
//	EA4357_LCD.HFP  = 17;
//	EA4357_LCD.HSW  = 2;
//	EA4357_LCD.PPL  = 800;
//	EA4357_LCD.VBP  = 22;
//	EA4357_LCD.VFP  = 22;
//	EA4357_LCD.VSW  = 2;
//	EA4357_LCD.LPP  = 480;
//	EA4357_LCD.IOE  = 0;
//	EA4357_LCD.IPC  = 0;
//	EA4357_LCD.IHS  = 1;
//	EA4357_LCD.IVS  = 1;
//	EA4357_LCD.ACB  = 1;
//	EA4357_LCD.BPP  = 6;	// 5:6:5 -> 6
//	EA4357_LCD.Dual = 0;
//	EA4357_LCD.LCD = LCD_TFT;
//	EA4357_LCD.color_format = LCD_COLOR_FORMAT_BGR;
//	EA4357_LCD.Clock  = 30000000;	// LCD_DCLK : 120/4 => 30MHz (32.4 ~ 43MHz)
	
	
	/* Setup for current board */
	Chip_LCD_Init(LPC_LCD, (LCD_CONFIG_T *) &EA4357_LCD);
	Chip_LCD_SetUPFrameBuffer(LPC_LCD, (void *)FRAMEBUFFER_ADDR);
	Chip_LCD_PowerOn(LPC_LCD);
		
	//lcdDelay(100);
	/* Turn on backlight */
	// PWM 제어로 인해 ade9000이 영향받는다 -> 주파수를 1k -> 10k로 높여서 해결
	// but The recommended PWM frequency range is from 100 Hz to 2 kHz.
	Board_PWM_Init(2000);	// 10KHz -> 2KHz, lot 별로 동작 안하는 경우 있다
	Board_LcdBackLightCtrl(10);	// 10% 부터 시작
	
	printf("Board_LCD_Init OK ...\n");
}



///* Sets up board specific I2S interface and UDA1380 */
//void Board_Audio_Init(LPC_I2S_T *pI2S, int micIn)
//{
//	I2S_AUDIO_FORMAT_T I2S_Config;

//	I2S_Config.SampleRate = 48000;
//	I2S_Config.ChannelNumber = 2;	/* 1 is mono, 2 is stereo */
//	I2S_Config.WordWidth =  16;		/* 8, 16 or 32 bits */
//	Chip_I2S_Init(pI2S);
//	Chip_I2S_TxConfig(pI2S, &I2S_Config);

//	/* Init UDA1380 CODEC */
//	Board_Audio_CodecInit(micIn);
//}

///* Initialize the LCD interface */
//void Board_LCD_Init(void)
//{
//	int32_t dev_lcd = 0;
//	lcdb_result_t result;

//	/* Reset LCD and wait for reset to complete */
//	Chip_RGU_TriggerReset(RGU_LCD_RST);
//	while (Chip_RGU_InReset(RGU_LCD_RST)) {
//	}

//	if ((result = ea_lcdb_open(NULL, NULL, &dev_lcd)) == LCDB_RESULT_OK) {

//		if ((result = ea_lcdb_getLcdParams(&EA4357_LCD)) != LCDB_RESULT_OK) {
//			printf("ea_lcdb_getLcdParams FAILED (%d)\r\n", result);
//		}
//	}
//}

//void Board_InitTouchController(void)
//{
//	CALIBDATA_T c;

//    touch_init();

//    // Try to calibrate using stored parameters
//    if (Board_GetStoredCalibrationData(&c)) {
//        Board_CalibrateTouch(&c);
//    }
//}

///* Poll for Touch position */
//bool Board_GetTouchPos(int16_t *pX, int16_t *pY)
//{
//	int32_t x, y, z;
//	touch_xyz(&x, &y, &z);
//	*pX = (int16_t)x;
//	*pY = (int16_t)y;
//	return (z != 0);
//}

//void Board_CalibrateTouch(CALIBDATA_T* setup)
//{
//	// Allow the board to be recalibrated (i.e. let all read values
//	// be uncalibrated)
//	touch_reinit();

//	if (setup != NULL) {
//		tTouchPoint r1,r2,r3,s1,s2,s3;
//		r1.x = setup->refX1;
//		r1.y = setup->refY1;
//		r2.x = setup->refX2;
//		r2.y = setup->refY2;
//		r3.x = setup->refX3;
//		r3.y = setup->refY3;
//		s1.x = setup->scrX1;
//		s1.y = setup->scrY1;
//		s2.x = setup->scrX2;
//		s2.y = setup->scrY2;
//		s3.x = setup->scrX3;
//		s3.y = setup->scrY3;
//		touch_calibrate(r1, r2, r3, s1, s2, s3);
//	}
//}

//bool Board_GetStoredCalibrationData(CALIBDATA_T* setup)
//{
//	return (ea_lcdb_getTouchCalibData(setup) == LCDB_RESULT_OK);
//}

//bool Board_StoreCalibrationData(CALIBDATA_T* setup)
//{
//	return (ea_lcdb_storeTouchCalibData(setup) == LCDB_RESULT_OK);
//}

///* Turn on LCD backlight */
//void Board_SetLCDBacklight(uint8_t Intensity)
//{
//	// Workaround for stupid interface. See description in board_api.h
//	if (Intensity == 1) {
//		Intensity = 100;
//	}

//	ea_lcdb_ctrl_backlightContrast(Intensity);
//}

/* Initializes SDMMC interface */
void Board_SDMMC_Init(void)
{
	Chip_SCU_PinMuxSet(0xC, 4, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.4 connected to SDIO_D0 */
	Chip_SCU_PinMuxSet(0xC, 5, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.5 connected to SDIO_D1 */
	Chip_SCU_PinMuxSet(0xC, 6, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.6 connected to SDIO_D2 */
	Chip_SCU_PinMuxSet(0xC, 7, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.7 connected to SDIO_D3 */

	Chip_SCU_PinMuxSet(0xC, 8, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC7));	/* PC.4 connected to SDIO_CD */
	Chip_SCU_PinMuxSet(0xC, 10, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.10 connected to SDIO_CMD */
	Chip_SCU_PinMuxSet(0xC, 0, (SCU_MODE_INACT | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC7));/* PC.0 connected to SDIO_CLK */
}


void SSP_SSEL_Mode(int id, int _auto) {
	int n = sizeof(SSP0_PIN_AUTO) / sizeof(PINMUX_GRP_T);
	// SSEL Index : 1
	if (id == 0) {
		if (_auto) {
			Chip_SCU_PinMuxSet(SSP0_PIN_AUTO[1].pingrp, SSP0_PIN_AUTO[1].pinnum, SSP0_PIN_AUTO[1].modefunc);
		}
		else {
			Chip_SCU_PinMuxSet(SSP0_PIN_MAN[1].pingrp, SSP0_PIN_MAN[1].pinnum, SSP0_PIN_MAN[1].modefunc);
		}
	}
	else {
		if (_auto) {
			Chip_SCU_PinMuxSet(SSP1_PIN_AUTO[1].pingrp, SSP1_PIN_AUTO[1].pinnum, SSP1_PIN_AUTO[1].modefunc);
		}
		else {
			Chip_SCU_PinMuxSet(SSP1_PIN_MAN[1].pingrp, SSP1_PIN_MAN[1].pinnum, SSP1_PIN_MAN[1].modefunc);
		}		
	}

	if (_auto == 0) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, SSEL_GPIO[id].port, SSEL_GPIO[id].num, 1);
		Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, SSEL_GPIO[id].port, SSEL_GPIO[id].num);				
	}		
}

/* Initializes SSP interface */
void Board_SSP_Init(LPC_SSP_T *pSSP, int w, int _auto, int speed)
{
	if (pSSP == LPC_SSP0) {
		if (_auto == 0) {
			Chip_SCU_SetPinMuxing(SSP0_PIN_MAN, sizeof(SSP0_PIN_MAN) / sizeof(PINMUX_GRP_T));
			Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, SSEL_GPIO[0].port, SSEL_GPIO[0].num, 1);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, SSEL_GPIO[0].port, SSEL_GPIO[0].num);		
		}
		else {
			Chip_SCU_SetPinMuxing(SSP0_PIN_AUTO, sizeof(SSP0_PIN_AUTO) / sizeof(PINMUX_GRP_T));
		}				
	}
	else if (pSSP == LPC_SSP1) {
		if (_auto == 0) {
			Chip_SCU_SetPinMuxing(SSP1_PIN_MAN, sizeof(SSP1_PIN_MAN) / sizeof(PINMUX_GRP_T));
			Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, SSEL_GPIO[1].port, SSEL_GPIO[1].num, 1);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, SSEL_GPIO[1].port, SSEL_GPIO[1].num);				
		}
		else {
			Chip_SCU_SetPinMuxing(SSP1_PIN_AUTO, sizeof(SSP1_PIN_AUTO) / sizeof(PINMUX_GRP_T));
		}
	}
	else {
		return;
	}
		
	Chip_SSP_Init(pSSP);
	// POLLING ; 16, DMA : 8
	if (w == 8) {
		Chip_SSP_SetFormat(pSSP, SSP_BITS_8, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA1_CPOL1);
	}
	else {
		Chip_SSP_SetFormat(pSSP, SSP_BITS_16, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA1_CPOL1);
	}
	// 20M로 설정시 DMA Xfer 에서 정지되는 문제 발생한다 
	Chip_SSP_SetBitRate(pSSP, speed);	// 10M	
	Chip_SSP_SetMaster(pSSP, 1);	// Master Mode
	Chip_SSP_Enable(pSSP);		
}

void selectMeter(int id) {
	Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, SSEL_GPIO[id].port, SSEL_GPIO[id].num);
}

void deSelectMeter(int id) {
	Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, SSEL_GPIO[id].port, SSEL_GPIO[id].num);
}

/* Initializes board specific buttons */
void Board_Buttons_Init(void)
{
	int i;
	
	// Button
	Chip_SCU_SetPinMuxing(KEY_PIN, sizeof(KEY_PIN) / sizeof(PINMUX_GRP_T));
	for (i=0; i<2; i++) {
		Chip_GPIO_SetPinDIR(LPC_GPIO_PORT, KEY_GPIO[i].port, KEY_GPIO[i].num, 0);
	}	
}

///* Sets up default states for joystick */
//void Board_Joystick_Init(void)
//{
//	Chip_SCU_PinMuxSet(0xA,  1, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0));  /* PA_0 as GPIO4[8] */
//	Chip_SCU_PinMuxSet(0xA,  2, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0));  /* PA_1 as GPIO4[9] */
//	Chip_SCU_PinMuxSet(0xA,  3, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0));  /* PA_2 as GPIO4[10] */
//	Chip_SCU_PinMuxSet(0x9,  0, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0));  /* P9_0 as GPIO4[12] */
//	Chip_SCU_PinMuxSet(0x9,  1, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0));  /* P9_1 as GPIO4[13] */

//	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, JOYSTICK_UP_GPIO_PORT_NUM, JOYSTICK_UP_GPIO_BIT_NUM);		/* input */
//	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, JOYSTICK_DOWN_GPIO_PORT_NUM, JOYSTICK_DOWN_GPIO_BIT_NUM);	/* input */
//	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, JOYSTICK_LEFT_GPIO_PORT_NUM, JOYSTICK_LEFT_GPIO_BIT_NUM);	/* input */
//	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, JOYSTICK_RIGHT_GPIO_PORT_NUM, JOYSTICK_RIGHT_GPIO_BIT_NUM);	/* input */
//	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, JOYSTICK_PRESS_GPIO_PORT_NUM, JOYSTICK_PRESS_GPIO_BIT_NUM);	/* input */
//}

///* Gets joystick status */
//uint8_t Joystick_GetStatus(void)
//{

//	uint8_t ret = NO_BUTTON_PRESSED;

//	if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, JOYSTICK_UP_GPIO_PORT_NUM, JOYSTICK_UP_GPIO_BIT_NUM) == 0) {
//		ret |= JOY_UP;
//	}
//	else if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, JOYSTICK_DOWN_GPIO_PORT_NUM, JOYSTICK_DOWN_GPIO_BIT_NUM) == 0) {
//		ret |= JOY_DOWN;
//	}
//	else if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, JOYSTICK_LEFT_GPIO_PORT_NUM, JOYSTICK_LEFT_GPIO_BIT_NUM) == 0) {
//		ret |= JOY_LEFT;
//	}
//	else if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, JOYSTICK_RIGHT_GPIO_PORT_NUM, JOYSTICK_RIGHT_GPIO_BIT_NUM) == 0) {
//		ret |= JOY_RIGHT;
//	}
//	else if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, JOYSTICK_PRESS_GPIO_PORT_NUM, JOYSTICK_PRESS_GPIO_BIT_NUM) == 0) {
//		ret |= JOY_PRESS;
//	}

//	return ret;
//}

///* Gets buttons status */
//uint32_t Buttons_GetStatus(void)
//{
//	uint8_t ret = NO_BUTTON_PRESSED;

//	uint32_t val = pca9532_getLedState(0);
//	if (val & KEY1) {
//		ret |= BUTTONS_BUTTON2;
//	}
//	if (val & KEY2) {
//		ret |= BUTTONS_BUTTON3;
//	}
//	if (val & KEY3) {
//		ret |= BUTTONS_BUTTON4;
//	}
//	if (val & KEY4) {
//		ret |= BUTTONS_BUTTON5;
//	}
//	return ret;
//}

///* Initialize DAC interface for the board */
//void Board_DAC_Init(LPC_DAC_T *pDAC)
//{
//	Chip_SCU_DAC_Analog_Config();
//}

#define DRAM_BASE_ADDRESS (uint32_t *) 0x28000000
#define DRAM_SIZE (32 * 1024 * 1024)


//void MemoryTestSimple(void) {
//	MEM_TEST_SETUP_T memSetup;
//	
//	/* Walking 0 test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_walking0(&memSetup)) {
//		printf("Walking 0 memory test passed\n");
//	}
//	else {
//		printf("Walking 0 memory test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}	
//}

//void MemoryTest(void) {
//	MEM_TEST_SETUP_T memSetup;
//	
//	/* Walking 0 test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_walking0(&memSetup)) {
//		printf("Walking 0 memory test passed\n");
//	}
//	else {
//		printf("Walking 0 memory test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}
//	
//	/* Walking 1 test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_walking1(&memSetup)) {
//		printf("Walking 1 memory test passed\n");
//	}
//	else {
//		printf("Walking 1 memory test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}

//	/* Address test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_address(&memSetup)) {
//		printf("Address test passed\n");
//	}
//	else {
//		printf("Address test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}

//	/* Inverse address test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_invaddress(&memSetup)) {
//		printf("Inverse address test passed\n");
//	}
//	else {
//		printf("Inverse address test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}

//	/* Pattern test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_pattern(&memSetup)) {
//		printf("Pattern (0x55/0xAA) test passed\n");
//	}
//	else {
//		printf("Pattern (0x55/0xAA) test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}

//	/* Seeded pattern test */
//	memSetup.start_addr = DRAM_BASE_ADDRESS;
//	memSetup.bytes = DRAM_SIZE;
//	if (mem_test_pattern_seed(&memSetup, 0x12345678, 0x50005)) {
//		printf("Seeded pattern test passed\n");
//	}
//	else {
//		printf("Seeded pattern test failed at address %p\n", memSetup.fail_addr);
//		printf(" Expected %08x, actual %08x\n", memSetup.ex_val, memSetup.is_val);
//	}
//}

//void  dprintf (const  char *format, ...)
//{
//    static  char  buffer[512 + 1];
//	
//	char *p = buffer;
//	va_list     vArgs;
//	char	*tmp;
//    va_start(vArgs, format);
//    vsprintf((char *)buffer, (char const *)format, vArgs);
//    va_end(vArgs);

//	while (*p != 0) {
//#ifdef	__RTX
//		sendchar(*p++);
//#else		
//		sendchar(*p++);
//		//stdout_putchar(*p++);
//#endif	
//	}
//}

extern void meterIrqSvc(void);

// Falling Edge triggered interrupt
void ExtINTR_Init(int index, int intNo, int level) {
	Chip_SCU_GPIOIntPinSel(index, EINT_GPIO[intNo].port, EINT_GPIO[intNo].num);
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(index));
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(index));
	if (level == 0) {
		Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(index));		
	}
	else {
		Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH(index));
	}
}


// 1) Set priority grouping (3 bits for pre-emption priority, no bits for subpriority)
// LPC43XX_ETH_IRQ_PRIORITY_GROUPING(4) : 모든 비트를 서브 우선순위에 사용
// NVIC_SetPriorityGrouping(LPC43XX_ETH_IRQ_PRIORITY_GROUPING);
//
// 2) Configure Ethernet interrupt priority
// LPC43XX_ETH_IRQ_GROUP_PRIORITY(6) -> 192
// NVIC_SetPriority(ETHERNET_IRQn, NVIC_EncodePriority(LPC43XX_ETH_IRQ_PRIORITY_GROUPING,
//    LPC43XX_ETH_IRQ_GROUP_PRIORITY, LPC43XX_ETH_IRQ_SUB_PRIORITY));
// 
void ExtINTR_Enable(int intNo) {   
	switch (intNo) {
		case 0:
			NVIC_ClearPendingIRQ(PIN_INT0_IRQn);
#if 1			
			NVIC_SetPriority(PIN_INT0_IRQn, 6);
#endif			
			NVIC_EnableIRQ(PIN_INT0_IRQn);
			break;
		case 1:			
			NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
#if 1			
			NVIC_SetPriority(PIN_INT1_IRQn, 6);
#endif			
			NVIC_EnableIRQ(PIN_INT1_IRQn);
			break;
		case 2:
			NVIC_ClearPendingIRQ(PIN_INT2_IRQn);
#if 1			
			NVIC_SetPriority(PIN_INT2_IRQn, 6);
#endif			
			NVIC_EnableIRQ(PIN_INT2_IRQn);
			break;		
		case 3:	
			NVIC_ClearPendingIRQ(PIN_INT3_IRQn);
#if 1			
			NVIC_SetPriority(PIN_INT3_IRQn, 6);
#endif
			NVIC_EnableIRQ(PIN_INT3_IRQn);\
			break;
	}
}

//void GPIO1_IRQHandler() {
//	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(1));
//	dprintf("IRQ1\n");
//}

//
//
//

static int dlyEnable[3];

// 2020-3-20, U2_TXD와 U2_RXD가 short circuit 발생
// 이를 해결하기 위해 Tx끝난 후 TXD를  GPIO로 변경한다, 향후 HW 수정 후 복원한다 
void UART2_TX_Pin_Mode(int en) {
	if (en) 
		Chip_SCU_PinMuxSet(0xA, 1, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC3));		
	else
		Chip_SCU_PinMuxSet(0xA, 1, (SCU_MODE_INACT | SCU_MODE_FUNC0));	
}


void RTS_Off(int i) {
	Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, RTS_GPIO[i].port, RTS_GPIO[i].num);
	
	// TXD pin mode를 GPIO로 변경한다 
#ifdef GEMS7300	
	if (i == 2) {
		UART2_TX_Pin_Mode(0);	
	}
#endif
}


void RTS_On(int i) {
	Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, RTS_GPIO[i].port, RTS_GPIO[i].num);
}


void RTS_OffDelay(LPC_USART_T *pUART) {
	int i;
	
	for (i=0; i<3; i++) {
		if (pUART == uartBase[i]) {
			dlyEnable[i] = 1;
		}
	}
}

void RTS_Control() {
	int i;
		
	for (i=0; i<3; i++) {
		if (dlyEnable[i]) {
			if (Chip_UART_ReadLineStatus(uartBase[i]) & UART_LSR_TEMT) {
				RTS_Off(i);
				dlyEnable[i] = 0;
			}
		}
	}
}

int UARTSend(int port, uint8_t *pb, int c) {
	// U2_TXD를 UART Mode로 복원한다 
#ifdef GEMS7300
	if (port == 2) {
		UART2_TX_Pin_Mode(1);
	}
#endif	
	RTS_On(port);
	return Chip_UART_SendRB(uartBase[port], &txring[port], pb, c);
}


int UARTReceive(int port, uint8_t *pb, int c) {
	return Chip_UART_ReadRB(uartBase[port], &rxring[port], pb, c);
}


extern void tickHandler();
	
void TIMER1_IRQHandler(void)
{
	static bool On = false;
	int i;
		
	if (Chip_TIMER_MatchPending(LPC_TIMER1, 1)) {
		Chip_TIMER_ClearMatch(LPC_TIMER1, 1);
		tickHandler();
	}
}


void Timer1_Init(int freq) {
	uint32_t timerFreq;
	
	/* Enable timer 1 clock and reset it */
	Chip_TIMER_Init(LPC_TIMER1);
	Chip_RGU_TriggerReset(RGU_TIMER1_RST);
	while (Chip_RGU_InReset(RGU_TIMER1_RST)) {}

	/* Get timer 1 peripheral clock rate */
	timerFreq = Chip_Clock_GetRate(CLK_MX_TIMER1);

	/* Timer setup for match and interrupt at TICKRATE_HZ */
	Chip_TIMER_Reset(LPC_TIMER1);
	Chip_TIMER_MatchEnableInt(LPC_TIMER1, 1);
	Chip_TIMER_SetMatch(LPC_TIMER1, 1, (timerFreq / freq));
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER1, 1);
	Chip_TIMER_Enable(LPC_TIMER1);
#if 1	
	// FreeRTOS 사용시 priority는 6보다 크거나 같아야 한다
	NVIC_SetPriority(PIN_INT3_IRQn, 7);
#endif   
   /* Enable timer interrupt */      
	NVIC_EnableIRQ(TIMER1_IRQn);
	NVIC_ClearPendingIRQ(TIMER1_IRQn);
}


void RIT_IRQHandler(void)
{
	/* Clearn interrupt */
	Chip_RIT_ClearInt(LPC_RITIMER);
	//tickHandler();
}

void RIT_Init(int freq) {
	/* Initialize RITimer */
	Chip_RIT_Init(LPC_RITIMER);
	/* Configure RIT for a 1s interrupt tick rate */
	Chip_RIT_SetTimerInterval(LPC_RITIMER, freq);
   
   // FreeRTOS 사용시 priority는 6보다 크거나 같아야 한다
   NVIC_SetPriority(PIN_INT3_IRQn, 7);
   
	NVIC_EnableIRQ(RITIMER_IRQn);	
}
//
//
//

RTC_TIME_T _rtc_set, _rtc_get;
//
//
// 0: second
// 1: minute
// 2: hour
// 3: dayOfMonth
// 4: dayOfWeek
// 5: dayOfYear
// 6: Month
// 7: Year
// 9: update


void RTC_To_UTC(RTC_TIME_T *prtc, uint32_t *utc) {
	struct tm lt;
	
	memset(&lt, 0, sizeof(lt));
	lt.tm_year = prtc->time[RTC_TIMETYPE_YEAR] - 1900;	// 
	lt.tm_mon  = prtc->time[RTC_TIMETYPE_MONTH] - 1;	// 0 ~ 11
	lt.tm_mday = prtc->time[RTC_TIMETYPE_DAYOFMONTH];
	lt.tm_hour = prtc->time[RTC_TIMETYPE_HOUR];
	lt.tm_min  = prtc->time[RTC_TIMETYPE_MINUTE];
	lt.tm_sec  = prtc->time[RTC_TIMETYPE_SECOND];
	*utc = mktime(&lt);
}

void UTC_To_RTC(uint32_t utc, RTC_TIME_T *prtc) {
//	struct tm lt;
//	
//	uLocalTime(&utc, &lt);
//	
//	prtc->time[RTC_TIMETYPE_YEAR] = lt.tm_year;
//	prtc->time[RTC_TIMETYPE_MONTH] = lt.tm_mon;
//	prtc->time[RTC_TIMETYPE_DAYOFMONTH] = lt.tm_mday;
//	
//	prtc->time[RTC_TIMETYPE_DAYOFWEEK] = lt.tm_wday;
//	prtc->time[RTC_TIMETYPE_DAYOFYEAR] = lt.tm_yday;
//	
//	prtc->time[RTC_TIMETYPE_HOUR] = lt.tm_hour;
//	prtc->time[RTC_TIMETYPE_MINUTE] = lt.tm_min;
//	prtc->time[RTC_TIMETYPE_SECOND] = lt.tm_sec;
}

#ifdef	EXT_RTC
int bcd2dec(int bcd) {
	int dec = (bcd >> 4)*10;
	dec += (bcd & 0xf);
	return dec;
}

int dec2bcd(int decimal) {
	int bcd = (decimal / 10) << 4;
	bcd |= (decimal % 10);
	return bcd;
}

void ExtRTC_to_tm(uint8_t regs[], struct tm *ptm) {
	int hour;
	
	// bit 6: 12(1) or 24(0)
	// bit 5: AM(1) or PM(0)
	if (regs[2] & 0x40) 
	{	
		ptm->tm_hour = (regs[2] & 0x20) ? bcd2dec(regs[2]&0x1f) : bcd2dec(regs[2]&0x1f) + 12;
	}
	else 
	{		
		ptm->tm_hour = bcd2dec(regs[2]&0x3f);
	}
	ptm->tm_min  = bcd2dec(regs[1] & 0x7f);
	ptm->tm_sec  = bcd2dec(regs[0] & 0x7f);
	
	ptm->tm_year = bcd2dec(regs[6]) + 100;
	ptm->tm_mon  = bcd2dec(regs[5] & 0x1f)-1;
	ptm->tm_mday = bcd2dec(regs[4] & 0x3f);
	
	printf("ExtRTC_to_tm=%04d-%02d-%02d %02d:%02d:%02d\n", 
		ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
}

void tm_to_ExtRTC(struct tm *ptm, uint8_t regs[]) {
	regs[6] = dec2bcd(ptm->tm_year%100);
#ifdef	EXT_RTC
	regs[5] = dec2bcd(ptm->tm_mon);
#else
	regs[5] = dec2bcd(ptm->tm_mon+1);
#endif
	regs[4] = dec2bcd(ptm->tm_mday);
	regs[2] = dec2bcd(ptm->tm_hour);
	regs[1] = dec2bcd(ptm->tm_min);
	regs[0] = dec2bcd(ptm->tm_sec);
	
	printf("[%02x %02x %02x %02x %02x %02x]\n", regs[0], regs[1], regs[2], regs[4], regs[5], regs[6]);
}

void ExtRTC_To_UTC(uint8_t *regs, uint32_t *utc) {
	struct tm lt;
	
	memset(&lt, 0, sizeof(lt));
	ExtRTC_to_tm(regs, &lt);
	*utc = mktime(&lt);	
}

void UTC_To_ExtRTC(uint32_t utc, uint8_t *regs) {
	struct tm lt;
	
	uLocalTime(&utc, &lt);	// month:1~12
	printf("UTC_To_ExtRTC=%04d-%02d-%02d %02d:%02d:%02d\n", 
		lt.tm_year, lt.tm_mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
	tm_to_ExtRTC(&lt, regs);
}
#endif

// LPC_RTC->CCR을 변경하면 cpu가 잠시 멈추는 문제 발생한다 
// RTC를 정지시키지 않고 시간을 쓴다
// Remark: Only write to the RTC registers while the 32 kHz oscillator is running. Repeated
// writes to the RTC registers without the 32 kHz clock can stall the CPU.
void rtc_write(RTC_TIME_T *prtc) {

    // Pause clock, and clear counter register (clears us count)
    //LPC_RTC->CCR |= 2;
    
    // Set the RTC
    LPC_RTC->TIME[RTC_TIMETYPE_SECOND]     = prtc->time[RTC_TIMETYPE_SECOND];
    LPC_RTC->TIME[RTC_TIMETYPE_MINUTE]     = prtc->time[RTC_TIMETYPE_MINUTE];
    LPC_RTC->TIME[RTC_TIMETYPE_HOUR]       = prtc->time[RTC_TIMETYPE_HOUR];
    LPC_RTC->TIME[RTC_TIMETYPE_DAYOFMONTH] = prtc->time[RTC_TIMETYPE_DAYOFMONTH];
    LPC_RTC->TIME[RTC_TIMETYPE_DAYOFWEEK]  = prtc->time[RTC_TIMETYPE_DAYOFWEEK];
    LPC_RTC->TIME[RTC_TIMETYPE_DAYOFYEAR]  = prtc->time[RTC_TIMETYPE_DAYOFYEAR];
    LPC_RTC->TIME[RTC_TIMETYPE_MONTH]      = prtc->time[RTC_TIMETYPE_MONTH];
    LPC_RTC->TIME[RTC_TIMETYPE_YEAR]       = prtc->time[RTC_TIMETYPE_YEAR];
    
    // Restart clock
    //LPC_RTC->CCR &= ~((uint32_t)2);
}

void RTC_SetTimeUTC(uint32_t utc) {
#ifdef EXT_RTC
	uint8_t regs[8];
	UTC_To_ExtRTC(utc, regs);
	I2C_RtcWrite(regs);
#else	
	UTC_To_RTC(utc, &_rtc_set);
	rtc_write(&_rtc_set);
	/* Set current time for RTC 2:00:00PM, 2012-10-05 */
	
	//Chip_RTC_Enable(LPC_RTC, DISABLE);		// 많은 시간을 소모한다 
	//Chip_RTC_SetFullTime(LPC_RTC, &_rtc_set);	
	//Chip_RTC_Enable(LPC_RTC, ENABLE);
#endif
}


uint32_t RTC_GetTimeUTC(void) {
	uint32_t utc;
	
	RTC_To_UTC(&_rtc_get, &utc);	
	return utc;
}


void RTC_IRQHandler(void)
{
	uint32_t sec;

	/* Toggle heart beat LED for each second field change interrupt */
	if (Chip_RTC_GetIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE)) {
		Chip_RTC_GetFullTime(LPC_RTC, &_rtc_get);
		/* Clear pending interrupt */
		Chip_RTC_ClearIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE);
	}
}

void Board_RTC_ColdInit(void) {
	// Repeated writes to the RTC registers without the 32 kHz clock can stall the CPU.
	// To confirm that the 32 kHz clock is running, read the Alarm timer counter register (DOWNCOUNTER, see Table 865), 
	// which counts down from a preset value using the 1024 Hz clock signal derived from the 32 kHz oscillator.	
	Chip_ATIMER_Init(LPC_ATIMER, 1024);
	// 2021-4-30
	// 초기화에 실패시 return
	if (Chip_RTC_Init(LPC_RTC) == 0) {
		Chip_RTC_GetFullTime(LPC_RTC, &_rtc_get);
		// 읽은날짜가 올바르지 않으면 초기화 한다 
		if (_rtc_get.time[RTC_TIMETYPE_YEAR] == 0) {
			printf("[[[Invalid DateTime, set Default DateTime..]]]\n");
			_rtc_set.time[RTC_TIMETYPE_SECOND]  = 0;
			_rtc_set.time[RTC_TIMETYPE_MINUTE]  = 0;
			_rtc_set.time[RTC_TIMETYPE_HOUR]    = 0;
			_rtc_set.time[RTC_TIMETYPE_DAYOFMONTH]  = 1;
			_rtc_set.time[RTC_TIMETYPE_DAYOFWEEK]   = 1;
			_rtc_set.time[RTC_TIMETYPE_DAYOFYEAR]   = 1;
			_rtc_set.time[RTC_TIMETYPE_MONTH]   = 1;
			_rtc_set.time[RTC_TIMETYPE_YEAR]    = 2018;		
			Chip_RTC_SetFullTime(LPC_RTC, &_rtc_set);		
		}
		
		/* Set the RTC to generate an interrupt on each second */
		Chip_RTC_CntIncrIntConfig(LPC_RTC, RTC_AMR_CIIR_IMSEC, ENABLE);
		/* Clear interrupt pending */
		Chip_RTC_ClearIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE | RTC_INT_ALARM);

		/* Enable RTC interrupt in NVIC */
		NVIC_EnableIRQ((IRQn_Type) RTC_IRQn);	
		
		Chip_RTC_Enable(LPC_RTC, ENABLE);	
	}
}

// 
void Board_RTC_WarmInit(void) {	
	Chip_RTC_GetFullTime(LPC_RTC, &_rtc_get);
		
//	/* Set the RTC to generate an interrupt on each second */
//	Chip_RTC_CntIncrIntConfig(LPC_RTC, RTC_AMR_CIIR_IMSEC, ENABLE);
//	/* Clear interrupt pending */
//	Chip_RTC_ClearIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE | RTC_INT_ALARM);	
//	/* Enable RTC interrupt in NVIC */
   
#if 1   
	// FreeRTOS 사용시 priority는 6보다 크거나 같아야 한다
	NVIC_SetPriority(PIN_INT3_IRQn, 7);
#endif   
	NVIC_EnableIRQ((IRQn_Type) RTC_IRQn);
	/* Enable RTC (starts increase the tick counter and second counter register) */	
}

#ifdef	EXT_RTC
void ExtRTC_Init()
{
	uint8_t regs[8];
	uint32_t utcTime;
	
	I2C_RtcRead(regs);
	if (regs[0] & 0x80) {
		// 2023-1-1 00:00:00
		regs[0] = 0;	// address
		regs[1] = 0;	// sec
		regs[2] = 0;	// minute
		regs[3] = 0;	// hour
		regs[4] = 0;	// wday(known)
		regs[5] = 1;	// mday(1~31)
		regs[6] = 1;	// month(1~12)
		regs[7] = 23;	// year(0~99)
		I2C_RtcWrite(regs);
	}
	else {		
		ExtRTC_To_UTC(regs, &utcTime);
		tickSet(utcTime, 0, 0);
	}
}
#endif

void Board_RTC_Init(int mode) {
//	LPC_RTC_T *pRTC = LPC_RTC;
//	uint32_t utcTime;

//	if (mode == 0) 
//		Board_RTC_ColdInit();	// 2018-1-1, 0:0:0으로 설정
//	else
//		Board_RTC_WarmInit();	
//		
//	RTC_To_UTC(&_rtc_get, &utcTime);
//	
//	tickSet(utcTime, 0);
//	
//	printf("[REALTIME CLOCK : %d-%d-%d, %d:%d:%d] -> %d\n", 
//		_rtc_get.time[RTC_TIMETYPE_YEAR], _rtc_get.time[RTC_TIMETYPE_MONTH], _rtc_get.time[RTC_TIMETYPE_DAYOFMONTH], 
//		_rtc_get.time[RTC_TIMETYPE_HOUR], _rtc_get.time[RTC_TIMETYPE_MINUTE], _rtc_get.time[RTC_TIMETYPE_SECOND],
//		utcTime);
}

void signalSend() {}
void signalWait() {}

static int _TouchX, _TouchY, _PenIsDown;

int getTouchX(void) {
	return _TouchX;
}

int getTouchY(void) {
	return _TouchY;
}


// TSC 읽는 과정에서 I2C_STATUS_BUSY 자주 발생한다
int CheckUpdateTouch(int *pressed) {
	uint8_t buf[8];
	int rval = -1;
	
	memset(buf, 0, sizeof(buf));
	if (I2C_TouchRead(buf) == I2C_STATUS_DONE) {
		if ((buf[0] & 0x80) && (!(buf[1] & 0x80)) && (!(buf[2] & 0xe0)) && (!(buf[3] & 0x80)) && (!(buf[4] & 0xe0))) {
			if (buf[0] & 1) {
				_TouchX = (buf[1] | (buf[2]<<7));
				//_TouchY = 4096-(buf[3] | (buf[4]<<7));
				_TouchY = (buf[3] | (buf[4]<<7));
				*pressed = 1;
				//printf("X: %d, Y: %d\n", _TouchX, _TouchY);				
			}
			else {
				*pressed = 0;
			}
			rval = 0;
		}
	}
	
	return rval;
}


//
//
//
/**
 * @}
 */
