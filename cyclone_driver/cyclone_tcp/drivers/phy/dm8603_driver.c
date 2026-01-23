/**
 * @file DM8603_driver.c
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

//Switch to the appropriate trace level
#define TRACE_LEVEL NIC_TRACE_LEVEL

//Dependencies
#include "core/net.h"
#include "drivers/phy/DM8603_driver.h"
#include "debug.h"


/**
 * @brief LAN8720 Ethernet PHY driver
 **/

const PhyDriver dm8603PhyDriver =
{
   dm8603Init,
   dm8603Tick,
   dm8603EnableIrq,
   dm8603DisableIrq,
   dm8603EventHandler
};


/**
 * @brief LAN8720 PHY transceiver initialization
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t dm8603Init(NetInterface *interface)
{
   //Debug message
   TRACE_INFO("Initializing DM8603...\r\n");

   //Undefined PHY address?
   if(interface->phyAddr >= 32)
   {
      //Use the default address
      interface->phyAddr = DM8603_PHY_ADDR;
   }

   //Initialize serial management interface
   if(interface->smiDriver != NULL)
   {
      interface->smiDriver->init();
   }

   //Initialize external interrupt line driver
   if(interface->extIntDriver != NULL)
   {
      interface->extIntDriver->init();
   }

   //Reset PHY transceiver (soft reset)
   dm8603WritePhyReg(interface, DM8603_BMCR, DM8603_BMCR_RESET);

   //Wait for the reset to complete
   while(dm8603ReadPhyReg(interface, DM8603_BMCR) & DM8603_BMCR_RESET)
   {
   }

   //Dump PHY registers for debugging purpose
   dm8603DumpPhyReg(interface);

   //Restore default auto-negotiation advertisement parameters
   dm8603WritePhyReg(interface, DM8603_ANAR, DM8603_ANAR_100BTX_FD |
      DM8603_ANAR_100BTX_HD | DM8603_ANAR_10BT_FD | DM8603_ANAR_10BT_HD |
      DM8603_ANAR_SELECTOR_DEFAULT);

   //Enable auto-negotiation
   dm8603WritePhyReg(interface, DM8603_BMCR, DM8603_BMCR_AN_EN);

#if 1
#else   
   //The PHY will generate interrupts when link status changes are detected
   dm8603WritePhyReg(interface, DM8603_IMR, DM8603_IMR_AN_COMPLETE |
      DM8603_IMR_LINK_DOWN);
#endif
   //Perform custom configuration
   dm8603InitHook(interface);

   //Force the TCP/IP stack to poll the link state at startup
   interface->phyEvent = TRUE;
   //Notify the TCP/IP stack of the event
   osSetEvent(&netEvent);

   //Successful initialization
   return NO_ERROR;
}


/**
 * @brief LAN8720 custom configuration
 * @param[in] interface Underlying network interface
 **/

__weak_func void dm8603InitHook(NetInterface *interface)
{
}


/**
 * @brief LAN8720 timer handler
 * @param[in] interface Underlying network interface
 **/

void dm8603Tick(NetInterface *interface)
{
   uint16_t value;
   bool_t linkState;

   //No external interrupt line driver?
   if(interface->extIntDriver == NULL)
   {
      //Read basic status register
      value = dm8603ReadPhyReg(interface, DM8603_BMSR);
      //Retrieve current link state
      linkState = (value & DM8603_BMSR_LINK_STATUS) ? TRUE : FALSE;

      //Link up event?
      if(linkState && !interface->linkState)
      {
         //Set event flag
         interface->phyEvent = TRUE;
         //Notify the TCP/IP stack of the event
         osSetEvent(&netEvent);
      }
      //Link down event?
      else if(!linkState && interface->linkState)
      {
         //Set event flag
         interface->phyEvent = TRUE;
         //Notify the TCP/IP stack of the event
         osSetEvent(&netEvent);
      }
   }
}


/**
 * @brief Enable interrupts
 * @param[in] interface Underlying network interface
 **/

void dm8603EnableIrq(NetInterface *interface)
{
   //Enable PHY transceiver interrupts
   if(interface->extIntDriver != NULL)
   {
      interface->extIntDriver->enableIrq();
   }
}


/**
 * @brief Disable interrupts
 * @param[in] interface Underlying network interface
 **/

void dm8603DisableIrq(NetInterface *interface)
{
   //Disable PHY transceiver interrupts
   if(interface->extIntDriver != NULL)
   {
      interface->extIntDriver->disableIrq();
   }
}


/**
 * @brief LAN8720 event handler
 * @param[in] interface Underlying network interface
 **/

void dm8603EventHandler(NetInterface *interface)
{
   uint16_t value;
   
#if 1 // SPEED, DUPLEX를 켠다
   //Any link failure condition is latched in the BMSR register. Reading
   //the register twice will always return the actual link status
   value = dm8603ReadPhyReg(interface, DM8603_BMSR);
   value = dm8603ReadPhyReg(interface, DM8603_BMSR);

   //100BASE-TX full-duplex
   interface->linkSpeed = NIC_LINK_SPEED_100MBPS;
   interface->duplexMode = NIC_FULL_DUPLEX_MODE;

   //Update link state
   interface->linkState = TRUE;

   //Adjust MAC configuration parameters for proper operation
   interface->nicDriver->updateMacConfig(interface);   
#else
   //Read status register to acknowledge the interrupt
   value = dm8603ReadPhyReg(interface, DM8603_ISR);
   //Link status change?
   if((value & (DM8603_IMR_AN_COMPLETE | DM8603_IMR_LINK_DOWN)) != 0)
   {
      //Any link failure condition is latched in the BMSR register. Reading
      //the register twice will always return the actual link status
      value = dm8603ReadPhyReg(interface, DM8603_BMSR);
      value = dm8603ReadPhyReg(interface, DM8603_BMSR);

      //Link is up?
      if((value & DM8603_BMSR_LINK_STATUS) != 0)
      {
         //Read PHY special control/status register
         value = dm8603ReadPhyReg(interface, DM8603_PSCSR);

         //Check current operation mode
         switch(value & DM8603_PSCSR_HCDSPEED)
         {
         //10BASE-T half-duplex
         case DM8603_PSCSR_HCDSPEED_10BT_HD:
            interface->linkSpeed = NIC_LINK_SPEED_10MBPS;
            interface->duplexMode = NIC_HALF_DUPLEX_MODE;
            break;

         //10BASE-T full-duplex
         case DM8603_PSCSR_HCDSPEED_10BT_FD:
            interface->linkSpeed = NIC_LINK_SPEED_10MBPS;
            interface->duplexMode = NIC_FULL_DUPLEX_MODE;
            break;

         //100BASE-TX half-duplex
         case DM8603_PSCSR_HCDSPEED_100BTX_HD:
            interface->linkSpeed = NIC_LINK_SPEED_100MBPS;
            interface->duplexMode = NIC_HALF_DUPLEX_MODE;
            break;

         //100BASE-TX full-duplex
         case DM8603_PSCSR_HCDSPEED_100BTX_FD:
            interface->linkSpeed = NIC_LINK_SPEED_100MBPS;
            interface->duplexMode = NIC_FULL_DUPLEX_MODE;
            break;

         //Unknown operation mode
         default:
            //Debug message
            TRACE_WARNING("Invalid operation mode!\r\n");
            break;
         }

         //Update link state
         interface->linkState = TRUE;

         //Adjust MAC configuration parameters for proper operation
         interface->nicDriver->updateMacConfig(interface);
      }
      else
      {
         //Update link state
         interface->linkState = FALSE;
      }

      //Process link state change event
      nicNotifyLinkChange(interface);
   }
#endif   
}


/**
 * @brief Write PHY register
 * @param[in] interface Underlying network interface
 * @param[in] address PHY register address
 * @param[in] data Register value
 **/

void dm8603WritePhyReg(NetInterface *interface, uint8_t address,
   uint16_t data)
{
   //Write the specified PHY register
   if(interface->smiDriver != NULL)
   {
      interface->smiDriver->writePhyReg(SMI_OPCODE_WRITE,
         interface->phyAddr, address, data);
   }
   else
   {
      interface->nicDriver->writePhyReg(SMI_OPCODE_WRITE,
         interface->phyAddr, address, data);
   }
}


/**
 * @brief Read PHY register
 * @param[in] interface Underlying network interface
 * @param[in] address PHY register address
 * @return Register value
 **/

uint16_t dm8603ReadPhyReg(NetInterface *interface, uint8_t address)
{
   uint16_t data;

   //Read the specified PHY register
   if(interface->smiDriver != NULL)
   {
      data = interface->smiDriver->readPhyReg(SMI_OPCODE_READ,
         interface->phyAddr, address);
   }
   else
   {
      data = interface->nicDriver->readPhyReg(SMI_OPCODE_READ,
         interface->phyAddr, address);
   }

   //Return the value of the PHY register
   return data;
}


/**
 * @brief Dump PHY registers for debugging purpose
 * @param[in] interface Underlying network interface
 **/

void dm8603DumpPhyReg(NetInterface *interface)
{
   uint8_t i;

#if 1
   for(i = 0; i < 6; i++)
   {      
      //Display current PHY register
      TRACE_INFO("%02" PRIu8 ": 0x%04" PRIX16 "\r\n", i,
         dm8603ReadPhyReg(interface, i));
   }   
#else   
   //Loop through PHY registers
   for(i = 0; i < 32; i++)
   {      
      //Display current PHY register
      TRACE_DEBUG("%02" PRIu8 ": 0x%04" PRIX16 "\r\n", i,
         dm8603ReadPhyReg(interface, i));
   }
#endif         

   //Terminate with a line feed
   TRACE_DEBUG("\r\n");
}
