/**
 * @file spi_driver.h
 * @brief SPI driver
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

#ifndef _SMI_DRIVER_H
#define _SMI_DRIVER_H

#include "core/net.h"

// P1.17
#define	MDC_PORT 	6
#define	MDC_PIN  	0
// PC.1
#define	MDIO_PORT	0
#define	MDIO_PIN		12

#define  DLY         1  // MDC clock :  2.5MHz

//SMI driver
extern const SmiDriver smiDriver;
error_t smiInit(void);
void smiWritePhyReg(uint8_t opcode, uint8_t phyAddr, uint8_t regAddr, uint16_t data);
uint16_t smiReadPhyReg(uint8_t opcode, uint8_t phyAddr, uint8_t regAddr);

#endif
