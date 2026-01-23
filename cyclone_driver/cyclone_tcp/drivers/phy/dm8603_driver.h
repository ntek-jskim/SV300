/**
 * @file DM8603_driver.h
 * @brief LAN8720 Ethernet PHY driver
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

#ifndef _DM8603_DRIVER_H
#define _DM8603_DRIVER_H

//Dependencies
#include "core/nic.h"

//PHY address
#ifndef DM8603_PHY_ADDR
#define DM8603_PHY_ADDR 2	// PHY_ADDR: 2 or 3
#elif (DM8603_PHY_ADDR < 0 || DM8603_PHY_ADDR > 31)
   #error DM8603_PHY_ADDR parameter is not valid
#endif

//LAN8720 PHY registers
#define DM8603_BMCR                      0x00
#define DM8603_BMSR                      0x01
#define DM8603_PHYID1                    0x02
#define DM8603_PHYID2                    0x03
#define DM8603_ANAR                      0x04
#define DM8603_ANLPAR                    0x05
#define DM8603_ANER                      0x06
#define DM8603_MCSR                      0x11
#define DM8603_SMR                       0x12
#define DM8603_SECR                      0x1A
#define DM8603_SCSIR                     0x1B
#define DM8603_ISR                       0x1D
#define DM8603_IMR                       0x1E
#define DM8603_PSCSR                     0x1F

//Basic Control register
#define DM8603_BMCR_RESET                0x8000
#define DM8603_BMCR_LOOPBACK             0x4000
#define DM8603_BMCR_SPEED_SEL            0x2000
#define DM8603_BMCR_AN_EN                0x1000
#define DM8603_BMCR_POWER_DOWN           0x0800
#define DM8603_BMCR_ISOLATE              0x0400
#define DM8603_BMCR_RESTART_AN           0x0200
#define DM8603_BMCR_DUPLEX_MODE          0x0100

//Basic Status register
#define DM8603_BMSR_100BT4               0x8000
#define DM8603_BMSR_100BTX_FD            0x4000
#define DM8603_BMSR_100BTX_HD            0x2000
#define DM8603_BMSR_10BT_FD              0x1000
#define DM8603_BMSR_10BT_HD              0x0800
#define DM8603_BMSR_100BT2_FD            0x0400
#define DM8603_BMSR_100BT2_HD            0x0200
#define DM8603_BMSR_EXTENDED_STATUS      0x0100
#define DM8603_BMSR_AN_COMPLETE          0x0020
#define DM8603_BMSR_REMOTE_FAULT         0x0010
#define DM8603_BMSR_AN_CAPABLE           0x0008
#define DM8603_BMSR_LINK_STATUS          0x0004
#define DM8603_BMSR_JABBER_DETECT        0x0002
#define DM8603_BMSR_EXTENDED_CAPABLE     0x0001

//PHY Identifier 1 register
#define DM8603_PHYID1_PHY_ID_MSB         0xFFFF
#define DM8603_PHYID1_PHY_ID_MSB_DEFAULT 0x0007

//PHY Identifier 2 register
#define DM8603_PHYID2_PHY_ID_LSB         0xFC00
#define DM8603_PHYID2_PHY_ID_LSB_DEFAULT 0xC000
#define DM8603_PHYID2_MODEL_NUM          0x03F0
#define DM8603_PHYID2_MODEL_NUM_DEFAULT  0x00F0
#define DM8603_PHYID2_REVISION_NUM       0x000F

//Auto-Negotiation Advertisement register
#define DM8603_ANAR_REMOTE_FAULT         0x2000
#define DM8603_ANAR_PAUSE                0x0C00
#define DM8603_ANAR_100BTX_FD            0x0100
#define DM8603_ANAR_100BTX_HD            0x0080
#define DM8603_ANAR_10BT_FD              0x0040
#define DM8603_ANAR_10BT_HD              0x0020
#define DM8603_ANAR_SELECTOR             0x001F
#define DM8603_ANAR_SELECTOR_DEFAULT     0x0001

//Auto-Negotiation Link Partner Ability register
#define DM8603_ANLPAR_NEXT_PAGE          0x8000
#define DM8603_ANLPAR_ACK                0x4000
#define DM8603_ANLPAR_REMOTE_FAULT       0x2000
#define DM8603_ANLPAR_PAUSE              0x0400
#define DM8603_ANLPAR_100BT4             0x0200
#define DM8603_ANLPAR_100BTX_FD          0x0100
#define DM8603_ANLPAR_100BTX_HD          0x0080
#define DM8603_ANLPAR_10BT_FD            0x0040
#define DM8603_ANLPAR_10BT_HD            0x0020
#define DM8603_ANLPAR_SELECTOR           0x001F
#define DM8603_ANLPAR_SELECTOR_DEFAULT   0x0001

//Auto-Negotiation Expansion register
#define DM8603_ANER_PAR_DETECT_FAULT     0x0010
#define DM8603_ANER_LP_NEXT_PAGE_ABLE    0x0008
#define DM8603_ANER_NEXT_PAGE_ABLE       0x0004
#define DM8603_ANER_PAGE_RECEIVED        0x0002
#define DM8603_ANER_LP_AN_ABLE           0x0001

//Mode Control/Status register
#define DM8603_MCSR_EDPWRDOWN            0x2000
#define DM8603_MCSR_FARLOOPBACK          0x0200
#define DM8603_MCSR_ALTINT               0x0040
#define DM8603_MCSR_ENERGYON             0x0002

//Special Modes register
#define DM8603_SMR_MODE                  0x00E0
#define DM8603_SMR_PHYAD                 0x001F

//Symbol Error Counter register
#define DM8603_SECR_SYM_ERR_CNT          0xFFFF

//Special Control/Status Indication register
#define DM8603_SCSIR_AMDIXCTRL           0x8000
#define DM8603_SCSIR_CH_SELECT           0x2000
#define DM8603_SCSIR_SQEOFF              0x0800
#define DM8603_SCSIR_XPOL                0x0010

//Interrupt Source register
#define DM8603_ISR_ENERGYON              0x0080
#define DM8603_ISR_AN_COMPLETE           0x0040
#define DM8603_ISR_REMOTE_FAULT          0x0020
#define DM8603_ISR_LINK_DOWN             0x0010
#define DM8603_ISR_AN_LP_ACK             0x0008
#define DM8603_ISR_PAR_DETECT_FAULT      0x0004
#define DM8603_ISR_AN_PAGE_RECEIVED      0x0002

//Interrupt Mask register
#define DM8603_IMR_ENERGYON              0x0080
#define DM8603_IMR_AN_COMPLETE           0x0040
#define DM8603_IMR_REMOTE_FAULT          0x0020
#define DM8603_IMR_LINK_DOWN             0x0010
#define DM8603_IMR_AN_LP_ACK             0x0008
#define DM8603_IMR_PAR_DETECT_FAULT      0x0004
#define DM8603_IMR_AN_PAGE_RECEIVED      0x0002

//PHY Special Control/Status register
#define DM8603_PSCSR_AUTODONE            0x1000
#define DM8603_PSCSR_HCDSPEED            0x001C
#define DM8603_PSCSR_HCDSPEED_10BT_HD    0x0004
#define DM8603_PSCSR_HCDSPEED_100BTX_HD  0x0008
#define DM8603_PSCSR_HCDSPEED_10BT_FD    0x0014
#define DM8603_PSCSR_HCDSPEED_100BTX_FD  0x0018

//C++ guard
#ifdef __cplusplus
extern "C" {
#endif

//LAN8720 Ethernet PHY driver
extern const PhyDriver dm8603PhyDriver;

//LAN8720 related functions
error_t dm8603Init(NetInterface *interface);
void dm8603InitHook(NetInterface *interface);

void dm8603Tick(NetInterface *interface);

void dm8603EnableIrq(NetInterface *interface);
void dm8603DisableIrq(NetInterface *interface);

void dm8603EventHandler(NetInterface *interface);

void dm8603WritePhyReg(NetInterface *interface, uint8_t address,
   uint16_t data);

uint16_t dm8603ReadPhyReg(NetInterface *interface, uint8_t address);

void dm8603DumpPhyReg(NetInterface *interface);

//C++ guard
#ifdef __cplusplus
}
#endif

#endif
