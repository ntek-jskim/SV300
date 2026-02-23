/**
 * @file debug.c
 * @brief Debugging facilities
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2024 Oryx Embedded SARL. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 2.4.4
 **/

//Dependencies
#include "lpc43xx.h"
#include "debug.h"


/**
 * @brief Debug UART initialization
 * @param[in] baudrate UART baudrate
 **/
int ntDebugLevel;

 void debug(int nLevel)
{
	ntDebugLevel = nLevel;
	printf("nDebug Flag is set to %d\n", nLevel);
}
void setNtDebug(int val)
{
	ntDebugLevel = val;
}

int getDbgLevel(void)
{
	return ntDebugLevel;
}

void debugInit(uint32_t baudrate)
{
   uint32_t div;

   //Compute the UART baudrate prescaler
   div = ((SystemCoreClock / 16) + (baudrate / 2)) / baudrate;

   //Enable GPIO peripheral clock
   LPC_CCU1->CLK_M4_GPIO_CFG |= CCU1_CLK_M4_GPIO_CFG_RUN_Msk;
   while(!(LPC_CCU1->CLK_M4_GPIO_STAT & CCU1_CLK_M4_GPIO_STAT_RUN_Msk));

#if 1	// UART3
   //Configure P2.3 (U3_TXD) and P2.4 (U3_RXD)
   LPC_SCU->SFSP2_3 = (2 & SCU_SFSP2_3_MODE_Msk);
   LPC_SCU->SFSP2_4 = SCU_SFSP2_4_EZI_Msk | (2 & SCU_SFSP2_4_MODE_Msk);

   //USART3 source clock selection
   LPC_CGU->BASE_UART3_CLK = CGU_BASE_UART3_CLK_AUTOBLOCK_Msk |
      ((9 << CGU_BASE_UART3_CLK_CLK_SEL_Pos) & CGU_BASE_UART3_CLK_CLK_SEL_Msk);

   //Enable USART3 peripheral clock
   LPC_CCU1->CLK_M4_USART3_CFG |= CCU1_CLK_M4_USART3_CFG_RUN_Msk;
   while(!(LPC_CCU1->CLK_M4_USART3_STAT & CCU1_CLK_M4_USART3_STAT_RUN_Msk));

   //Reset USART3 peripheral
   LPC_RGU->RESET_EXT_STAT47 |= RGU_RESET_EXT_STAT47_PERIPHERAL_RESET_Msk;
   LPC_RGU->RESET_EXT_STAT47 &= ~RGU_RESET_EXT_STAT47_PERIPHERAL_RESET_Msk;

   //Configure UART3 (8 bits, no parity, 1 stop bit)
   LPC_USART3->LCR = USART3_LCR_DLAB_Msk | (3 & USART3_LCR_WLS_Msk);
   //Set baudrate
   LPC_USART3->DLM = MSB(div);
   LPC_USART3->DLL = LSB(div);
   //Clear DLAB
   LPC_USART3->LCR &= ~USART3_LCR_DLAB_Msk;

   //Enable and reset FIFOs
   LPC_USART3->FCR = USART3_FCR_TXFIFORES_Msk |
      USART3_FCR_RXFIFORES_Msk | USART3_FCR_FIFOEN_Msk;
      
   // Enable Transmitter (Tx Enable 누락)
   LPC_USART3->TER = USART3_TER_TXEN_Msk;
#else
   //Configure P6.4 (U0_TXD) and P2.1 (U0_RXD)
   LPC_SCU->SFSP6_4 = (2 & SCU_SFSP6_4_MODE_Msk);
   LPC_SCU->SFSP2_1 = SCU_SFSP2_1_EZI_Msk | (1 & SCU_SFSP2_1_MODE_Msk);

   //USART0 source clock selection
   LPC_CGU->BASE_UART0_CLK = CGU_BASE_UART0_CLK_AUTOBLOCK_Msk |
      ((9 << CGU_BASE_UART0_CLK_CLK_SEL_Pos) & CGU_BASE_UART0_CLK_CLK_SEL_Msk);

   //Enable USART0 peripheral clock
   LPC_CCU1->CLK_M4_USART0_CFG |= CCU1_CLK_M4_USART0_CFG_RUN_Msk;
   while(!(LPC_CCU1->CLK_M4_USART0_STAT & CCU1_CLK_M4_USART0_STAT_RUN_Msk));

   //Reset USART0 peripheral
   LPC_RGU->RESET_EXT_STAT44 |= RGU_RESET_EXT_STAT44_PERIPHERAL_RESET_Msk;
   LPC_RGU->RESET_EXT_STAT44 &= ~RGU_RESET_EXT_STAT44_PERIPHERAL_RESET_Msk;

   //Configure UART0 (8 bits, no parity, 1 stop bit)
   LPC_USART0->LCR = USART0_LCR_DLAB_Msk | (3 & USART0_LCR_WLS_Msk);
   //Set baudrate
   LPC_USART0->DLM = MSB(div);
   LPC_USART0->DLL = LSB(div);
   //Clear DLAB
   LPC_USART0->LCR &= ~USART0_LCR_DLAB_Msk;

   //Enable and reset FIFOs
   LPC_USART0->FCR = USART0_FCR_TXFIFORES_Msk |
      USART0_FCR_RXFIFORES_Msk | USART0_FCR_FIFOEN_Msk;
      
   // Enable Transmitter (Tx Enable 누락)
   LPC_USART0->TER = USART0_TER_TXEN_Msk;
#endif	  
}


/**
 * @brief Display the contents of an array
 * @param[in] stream Pointer to a FILE object that identifies an output stream
 * @param[in] prepend String to prepend to the left of each line
 * @param[in] data Pointer to the data array
 * @param[in] length Number of bytes to display
 **/

void debugDisplayArray(FILE *stream,
   const char_t *prepend, const void *data, size_t length)
{
   uint_t i;

   for(i = 0; i < length; i++)
   {
      //Beginning of a new line?
      if((i % 16) == 0)
         fprintf(stream, "%s", prepend);
      //Display current data byte
      fprintf(stream, "%02" PRIX8 " ", *((uint8_t *) data + i));
      //End of current line?
      if((i % 16) == 15 || i == (length - 1))
         fprintf(stream, "\r\n");
   }
}


/**
 * @brief Write character to stream
 * @param[in] c The character to be written
 * @param[in] stream Pointer to a FILE object that identifies an output stream
 * @return On success, the character written is returned. If a writing
 *   error occurs, EOF is returned
 **/

void debug_putchar(int_t c) 
{
#if 1   
   //Wait for the transmitter to be ready
   while(!(LPC_USART3->LSR & USART3_LSR_THRE_Msk));
   //Send character
   LPC_USART3->THR = c;   
#else
   //Wait for the transmitter to be ready
   while(!(LPC_USART0->LSR & USART0_LSR_THRE_Msk));
   //Send character
   LPC_USART0->THR = c;   
#endif   
}

void stdout_putchar(int c) {
	debug_putchar(c);
}

int debug_getchar() 
{	
   if (LPC_USART3->LSR & USART3_LSR_RDR_Msk) {
      return LPC_USART3->RBR & USART3_RBR_RBR_Msk;
   }
   else {
      return -1;   
   }
}

// RTX 사용시 층돌발생한다
#ifdef USE_FREERTOS
int_t fputc(int_t c, FILE *stream)
{
   //Standard output or error output?
   if(stream == stdout || stream == stderr)
   {
#if 1
      if (c == '\n') {
         debug_putchar('\r');
      }
      debug_putchar(c);
#else      
#if 1	// USART3
      //Wait for the transmitter to be ready
      while(!(LPC_USART3->LSR & USART3_LSR_THRE_Msk));
      //Send character
      LPC_USART3->THR = c;
      //Wait for the transfer to complete
      while(!(LPC_USART3->LSR & USART3_LSR_TEMT_Msk));
#else	   
      //Wait for the transmitter to be ready
      while(!(LPC_USART0->LSR & USART0_LSR_THRE_Msk));
      //Send character
      LPC_USART0->THR = c;
      //Wait for the transfer to complete
      while(!(LPC_USART0->LSR & USART0_LSR_TEMT_Msk));
#endif
#endif
      //On success, the character written is returned
      return c;
   }
   //Unknown output?
   else
   {
      //If a writing error occurs, EOF is returned
      return EOF;
   }
}
#endif
//-------------------------------------------------------------------------------------
//const uint32_t WDT_PIN[][3] = {	
//	{0x1, 3, ((0 & SCU_SFSP1_3_MODE_Msk) | SCU_SFSP1_3_EPD_Msk)},
//	{0x1, 4, ((0 & SCU_SFSP1_4_MODE_Msk) | SCU_SFSP1_4_EPD_Msk)},
//};
//// WDT & Wiring Mode
//const uint32_t WDT_GPIO[][2] = {
//	{0, 10},	// WDT_EN
//	{0, 11},	// WDT_CLR
//};
//const uint32_t ETC_PIN[][3] = {	
//	{0x1, 5, ((0 & SCU_SFSP1_5_MODE_Msk) | SCU_SFSP1_5_EPD_Msk)},	// wiring mode
//	{0x9, 3, ((0 & SCU_SFSP9_3_MODE_Msk) | SCU_SFSP9_3_EPD_Msk)},	// bug: FUNC4 -> FUNC0
//	{0x9, 0, ((0 & SCU_SFSP9_0_MODE_Msk) | SCU_SFSP9_0_EPD_Msk)}	// GPIO_RES2(reset PHY)
//};
//const uint32_t ETC_GPIO[][2] = {
//	{1, 8},	// W_MODE
//	{4, 15},// Beeper	
//	{4, 12}	// reset PHY
//};
//const PINMUX_GRP_T LED_PIN[] = {
//	{0xf, 8, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
//	{0xf, 9, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
//	{0xf, 10, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
//	{0xf, 11, (SCU_MODE_FUNC4 | SCU_MODE_PULLDOWN)},
//};

const uint32_t _LED_GPIO[][2] = {
	{7, 22},
	{7, 23},
	{7, 24},
	{6, 29},
	{6, 30}
};

const uint32_t _ETC_GPIO[][2] = {
   {7, 2},  // wiring
   {7, 25}, // key in 1
   {4, 15}, // key in 2
   {4, 12}  // PHY reset
};

const uint32_t _WDT_GPIO[][2] = {
   {7, 0}, // enable
   {7, 1} // clear
};

const uint32_t _EINT_GPIO[][2] = {
   {5, 8},  // enable
   {5, 9},  // clear
   {6, 1},  // enable
   {6, 2}  // clear
};

const uint32_t _DI_GPIO[][2] = {
   {6, 25},  // DI_1
   {6, 26}, // DI_2
   {6, 27}, // DI_3
   {6, 28}  // DI_4
};

void Board_LED_Toggle(uint8_t ix) {
   int port = _LED_GPIO[ix][0];
   int pin  = _LED_GPIO[ix][1];
   LPC_GPIO_PORT->NOT[port] = (1 << pin);
}

void Board_LED_On(uint8_t ix) {
   int port = _LED_GPIO[ix][0];
   int pin  = _LED_GPIO[ix][1];
   LPC_GPIO_PORT->CLR[port] = (1 << pin);
}

void Board_LED_Off(uint8_t ix) {
   int port = _LED_GPIO[ix][0];
   int pin  = _LED_GPIO[ix][1];
   LPC_GPIO_PORT->SET[port] = (1 << pin);
}

/* Sets the state of a board LED to on or off */
void Board_LED_Set(uint8_t n, uint8_t On)
{
	if (On) 
		Board_LED_On (n);
	else
		Board_LED_Off(n);		
}
void selWire(int m) {
   if (m == 0) // 3p4w
      LPC_GPIO_PORT->CLR[_ETC_GPIO[0][0]] = (1 << _ETC_GPIO[0][1]);
   else        // 3p3w
      LPC_GPIO_PORT->SET[_ETC_GPIO[0][0]] = (1 << _ETC_GPIO[0][1]);
}

void Board_PHY_Reset(int m) {
   if (m == 0) // disable
      LPC_GPIO_PORT->SET[_ETC_GPIO[3][0]] = (1 << _ETC_GPIO[3][1]);
   else        // enable
      LPC_GPIO_PORT->CLR[_ETC_GPIO[3][0]] = (1 << _ETC_GPIO[3][1]);
}

void Board_WDT_Enable() {
   // assert 
	LPC_GPIO_PORT->SET[_WDT_GPIO[0][0]] = (1 << _WDT_GPIO[0][1]);
}

void Board_WDT_Disable() {
   // negate
	LPC_GPIO_PORT->CLR[_WDT_GPIO[0][0]] = (1 << _WDT_GPIO[0][1]);
}

void Board_WDT_Clear() {
	LPC_GPIO_PORT->NOT[_WDT_GPIO[1][0]] = (1 << _WDT_GPIO[1][1]);
}

void Board_GPIO_Init() 
{
   int i;
   
	// WDT
   LPC_SCU->SFSPE_0 = ((0 & SCU_SFSPE_0_MODE_Msk) | SCU_SFSPE_0_EPD_Msk);  // PE.0
   LPC_SCU->SFSPE_1 = ((0 & SCU_SFSPE_1_MODE_Msk) | SCU_SFSPE_1_EPD_Msk);  // PE.1
   LPC_GPIO_PORT->DIR[_WDT_GPIO[0][0]] |= (1<<_WDT_GPIO[0][1]);   // 0.10
   LPC_GPIO_PORT->DIR[_WDT_GPIO[1][0]] |= (1<<_WDT_GPIO[1][1]);   // 0.11
   Board_WDT_Disable();
   
	// SelWire
   LPC_SCU->SFSPE_2 = ((0 & SCU_SFSPE_2_MODE_Msk));
   LPC_GPIO_PORT->DIR[_ETC_GPIO[0][0]] |= (1<<_ETC_GPIO[0][1]);   
   LPC_GPIO_PORT->CLR[_ETC_GPIO[0][0]]  = (1<<_ETC_GPIO[0][1]);	// 0:3p4W, 1:3p3w
	
   // DM8603 Reset
   LPC_SCU->SFSP9_0 = ((0 & SCU_SFSP9_0_MODE_Msk));               // P9.0
   LPC_GPIO_PORT->DIR[_ETC_GPIO[3][0]] |= (1<<_ETC_GPIO[3][1]);   // 4.12
   LPC_GPIO_PORT->SET[_ETC_GPIO[3][0]]  = (1<<_ETC_GPIO[3][1]);   // 4.12
   Board_PHY_Reset(0);  
   

   // LED
   //Configure red LED (6.8)/7.21
   LPC_SCU->SFSPF_8  = (4 & SCU_SFSPF_8_MODE_Msk);
   //Configure red LED (6.9)/7.22
   LPC_SCU->SFSPF_9  = (4 & SCU_SFSPF_9_MODE_Msk);
   //Configure red LED (6.10)/7.23
   LPC_SCU->SFSPF_10 = (4 & SCU_SFSPF_10_MODE_Msk);

   //RSTP Status LED
   LPC_SCU->SFSPD_15 = (4 & SCU_SFSPD_15_MODE_Msk);
   LPC_SCU->SFSPD_16 = (4 & SCU_SFSPD_16_MODE_Msk);
   // set output
   for (i=0; i<5; i++) {
      LPC_GPIO_PORT->DIR[_LED_GPIO[i][0]] |= (1<<_LED_GPIO[i][1]);
      LPC_GPIO_PORT->CLR[_LED_GPIO[i][0]]  = (1<<_LED_GPIO[i][1]);
   }
 
   // EINT
	LPC_SCU->SFSP3_1 = ((4 & SCU_SFSP3_1_MODE_Msk) | SCU_SFSP3_1_EZI_Msk);  // P3.1
   LPC_SCU->SFSP3_2 = ((4 & SCU_SFSP3_2_MODE_Msk) | SCU_SFSP3_2_EZI_Msk);  // P3.2
   LPC_SCU->SFSPC_2 = ((4 & SCU_SFSPC_2_MODE_Msk) | SCU_SFSPC_2_EZI_Msk);  // PC.2
   LPC_SCU->SFSPC_3 = ((4 & SCU_SFSPC_3_MODE_Msk) | SCU_SFSPC_3_EZI_Msk);  // PC.3
   for (i=0; i<4; i++) {
      LPC_GPIO_PORT->DIR[_EINT_GPIO[i][0]] &= ~(1<<_EINT_GPIO[i][1]);
   }

	// DI
   LPC_SCU->SFSPD_11 = ((0 & SCU_SFSPD_11_MODE_Msk) | SCU_SFSPD_11_EZI_Msk);
   LPC_GPIO_PORT->DIR[_DI_GPIO[0][0]] &= ~(1<<_DI_GPIO[0][1]); 
   LPC_SCU->SFSPD_12 =((0 & SCU_SFSPD_12_MODE_Msk) | SCU_SFSPD_12_EZI_Msk);
   LPC_GPIO_PORT->DIR[_DI_GPIO[1][0]] &= ~(1<<_DI_GPIO[1][1]); 
   LPC_SCU->SFSPD_13 = ((0 & SCU_SFSPD_13_MODE_Msk) | SCU_SFSPD_13_EZI_Msk);
   LPC_GPIO_PORT->DIR[_DI_GPIO[2][0]] &= ~(1<<_DI_GPIO[2][1]); 
   LPC_SCU->SFSPD_14 = ((0 & SCU_SFSPD_13_MODE_Msk) | SCU_SFSPD_14_EZI_Msk);
   LPC_GPIO_PORT->DIR[_DI_GPIO[3][0]] &= ~(1<<_DI_GPIO[3][1]); 

	// Key in (KEY1=P7.25, KEY2=P4.15), DI와 동일: SCU 모드0 + GPIO 입력
   LPC_SCU->SFSPF_11 = ((0 & SCU_SFSPF_11_MODE_Msk) | SCU_SFSPF_11_EZI_Msk);
   LPC_GPIO_PORT->DIR[_ETC_GPIO[1][0]] &= ~(1<<_ETC_GPIO[1][1]); 
   LPC_SCU->SFSP9_3  = ((0 & SCU_SFSP9_3_MODE_Msk) | SCU_SFSP9_3_EZI_Msk);
   LPC_GPIO_PORT->DIR[_ETC_GPIO[2][0]] &= ~(1<<_ETC_GPIO[2][1]); 

}

uint32_t DI_Read(int n)
{
	if (n < 0 || n > 3)
		return 0;
	{
		uint32_t port = _DI_GPIO[n][0];
		uint32_t pin  = _DI_GPIO[n][1];
		return (LPC_GPIO_PORT->PIN[port] >> pin) & 1UL;
	}
}

/** KEY: _ETC_GPIO[1]=KEY1, _ETC_GPIO[2]=KEY2. n=0 -> KEY1, n=1 -> KEY2. returns 0 or 1 */
uint32_t KEY_Read(int n)
{
	if (n < 0 || n > 1)
		return 0;
	{
		uint32_t port = _ETC_GPIO[1 + n][0];
		uint32_t pin  = _ETC_GPIO[1 + n][1];
		return (LPC_GPIO_PORT->PIN[port] >> pin) & 1UL;
	}
}

