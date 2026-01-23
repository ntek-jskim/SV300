/**
 * @file sja1105smbevm.h
 * @brief SJA1105SMBEVM evaluation board
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2024 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneTCP Open.
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

#ifndef _SJA1105SMBEVM_H
#define _SJA1105SMBEVM_H

//Dependencies
#include "mpc5748g.h"

//LED_GP1 (PG2)
#define LED_GP1 98
//LED_GP2 (PG3)
#define LED_GP2 99
//LED_GP3 (PG5)
#define LED_GP3 101
//LED_GP4 (PG6)
#define LED_GP4 102
//LED_GP5 (PG7)
#define LED_GP5 103
//LED_GP6 (PG8)
#define LED_GP6 104
//LED_ALIVE (PG4)
#define LED_ALIVE 100

//PB_SW3 button (PE7)
#define PB_SW3 71
//PB_SW4 button (PE6)
#define PB_SW4 70

//DP83848 reset pin (PI14)
#define DP83848_RST 142

//SJA1105 #1 reset pin (PI5)
#define SJA1105_RST1 133
//SJA1105 #2 reset pin (PI6)
#define SJA1105_RST2 134

//KSZ9031 #1 reset pin (PI7)
#define KSZ9031_RST1 135
//KSZ9031 #2 reset pin (PI8)
#define KSZ9031_RST2 136

//TJA1102 #1 reset pin (PI10)
#define TJA1102_RST1 138
//TJA1102 #2 and #3 reset pin (PI9)
#define TJA1102_RST2 137

//TJA1102 #1 enable pin (PJ4)
#define TJA1102_EN1 146
//TJA1102 #2 enable pin (PJ0)
#define TJA1102_EN2 144
//TJA1102 #3 enable pin (PJ1)
#define TJA1102_EN3 145

#endif
