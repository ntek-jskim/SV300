/**
 * @file spi_driver.c
 * @brief SPI driver
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2023 Oryx Embedded SARL. All rights reserved.
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
 * @version 2.3.4
 **/


// Module 3: SMI management interface does not function if SPISN pin is not pulled high
// DESCRIPTION
//   If SPISN (pin 39) is left floating when the MIIM / SMI management mode is selected, the SMI interface will not work.
//   This is due to the internal pull-down resistor on the SPISN pin. The MIIM management mode is unaffected.
// END USER IMPLICATIONS
//   Registers cannot be accessed via SMI management interface if SPISN floats to a low level.
// Work around
//   The problem is solved by pulling SPISN high with an external resistor to VDDIO. Use a resistor value between
//   1kΩ and 10kΩ.
   

//Dependencies
#include "core/net.h"
#include "smi_driver.h"
#include "lpcxpresso_4337.h"

//SPI bitrate

/**
 * @brief SPI driver
 **/

const SmiDriver smiDriver = 
{
	smiInit,
	smiWritePhyReg,
	smiReadPhyReg
};


void delay(__IO uint32_t nCount) {
	while (nCount-- > 0) {
		;
	}
}


void mdio_init()
{
		// lpc43xxEthInitGpio()에서 초기화
}

void output_MDIO(uint32_t val, int n)
{
	for(val <<= (32-n); n; val<<=1, n--)
	{
		if(val & 0x80000000)
			LPC_GPIO_PORT->SET[MDIO_PORT] = (1<<MDIO_PIN);
		else
			LPC_GPIO_PORT->CLR[MDIO_PORT] = (1<<MDIO_PIN);

		delay(DLY);
		LPC_GPIO_PORT->SET[MDC_PORT] = (1<<MDC_PIN);
		delay(DLY);
		LPC_GPIO_PORT->CLR[MDC_PORT] = (1<<MDC_PIN);
	}
}

uint32_t input_MDIO()
{
	uint32_t i, val=0, temp; 
   
	for(i=0; i<16; i++)
	{
		val <<= 1;
		LPC_GPIO_PORT->SET[MDC_PORT] = (1<<MDC_PIN);
		delay(DLY);
		LPC_GPIO_PORT->CLR[MDC_PORT] = (1<<MDC_PIN);
		delay(DLY);
      temp = LPC_GPIO_PORT->PIN[MDIO_PORT];
		val |= (temp & (1<<MDIO_PIN)) != 0 ? 1 : 0;
	}
	return (val);
}

// 
void mdio_dir(uint32_t mode) {
	if (mode == 0) 	// INPUT
		LPC_GPIO_PORT->DIR[MDIO_PORT] &= ~(1<<MDIO_PIN);
	else					// OUTPUT
		LPC_GPIO_PORT->DIR[MDIO_PORT] |=  (1<<MDIO_PIN);
}

// 수신모드
void turnaround_MDIO()
{
	mdio_dir(0);	// Floating

	delay(DLY*10);
   
	LPC_GPIO_PORT->SET[MDC_PORT] = (1<<MDC_PIN);
	delay(DLY);
	LPC_GPIO_PORT->CLR[MDC_PORT] = (1<<MDC_PIN);
	delay(DLY);
}


void idle_MDIO()
{
	mdio_dir(1);	// output mode

	LPC_GPIO_PORT->SET[MDC_PORT] = (1<<MDC_PIN);
	delay(DLY);
	LPC_GPIO_PORT->CLR[MDC_PORT] = (1<<MDC_PIN);
	delay(DLY);
}

uint32_t mdio_read(uint32_t phyAddr, uint32_t regAddr)
{
	uint32_t val = 0;

	/* 32 Consecutive ones on MDO to establish sync */
	output_MDIO(0xFFFFFFFF, 32);

   /* start code 01, read command (10) */
   output_MDIO(0x01, 2);
   // opcode(00)
   output_MDIO(0x00, 2);
	// PHY address 
	output_MDIO(phyAddr, 5);	// ADDR[4..0]
	// PHY register address
	output_MDIO(regAddr, 5);	// REG_ADDR[4..0]

	/* turnaround MDO is tristated */
	turnaround_MDIO();

	/* Read the data value */
	val = input_MDIO();
	
	/* turnaround MDO is tristated */
	idle_MDIO();

	return val;
}

void mdio_write(uint32_t phyAddr, uint32_t regAddr, uint32_t val)
{

	/* 32 Consecutive ones on MDO to establish sync */
	//printf("mdio write- sync \r\n");
	output_MDIO(0xFFFFFFFF, 32);

	/* start code 01, write command (01) */
	//printf("mdio write- start \r\n");
	output_MDIO(0x01, 2);
   // opcode
   // opcode(00)
   output_MDIO(0x00, 2);

	/* write PHY address */
	//printf("mdio write- PHY address \r\n");
	output_MDIO(phyAddr, 5);
	//printf("mdio read - PHY REG address \r\n");
	output_MDIO(regAddr, 5);

	/* turnaround MDO */
	//printf("mdio write- turnaround (1,0)\r\n");
	output_MDIO(0x02, 2);

	/* Write the data value */
	//printf("mdio writeread - read the data value \r\n");
	output_MDIO(val, 16);

	/* turnaround MDO is tristated */
	//printf("mdio write- idle \r\n");
	idle_MDIO();

}


/**
 * @brief SPI initialization
 * @return Error code
 **/

error_t smiInit(void)
{
   uint32_t temp;
   return NO_ERROR;
}

void smiWritePhyReg(uint8_t opcode, uint8_t phyAddr, uint8_t regAddr, uint16_t data) 
{
   mdio_write(phyAddr, regAddr, data);
}

uint16_t smiReadPhyReg(uint8_t opcode, uint8_t phyAddr, uint8_t regAddr) 
{
   //int val = mdio_read(phyAddr, regAddr);
   int val = mdio_read(phyAddr, regAddr);
   return val;
}


