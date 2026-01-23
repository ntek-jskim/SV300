/**
 * @file eap_debug.c
 * @brief Data logging functions for debugging purpose (EAP)
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2022-2024 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneEAP Open.
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
#define TRACE_LEVEL EAP_TRACE_LEVEL

//Dependencies
#include "eap/eap.h"
#include "eap/eap_debug.h"
#include "debug.h"

//Check EAP library configuration
#if (EAP_SUPPORT == ENABLED)

#if(EAP_TRACE_LEVEL >= TRACE_LEVEL_DEBUG)

//EAPOL packet types
static const EapParamName eapolPacketTypeList[] =
{
   {EAPOL_TYPE_EAP,                    "EAPOL-EAP"},
   {EAPOL_TYPE_START,                  "EAPOL-Start"},
   {EAPOL_TYPE_LOGOFF,                 "EAPOL-Logoff"},
   {EAPOL_TYPE_KEY,                    "EAPOL-Key"},
   {EAPOL_TYPE_ENCAPSULATED_ASF_ALERT, "EAPOL-Encapsulated-ASF-Alert"}
};

//EAP codes
static const EapParamName eapCodeList[] =
{
   {EAP_CODE_REQUEST,  "Request"},
   {EAP_CODE_RESPONSE, "Response"},
   {EAP_CODE_SUCCESS,  "Success"},
   {EAP_CODE_FAILURE,  "Failure"}
};

//EAP method types
static const EapParamName methodTypeList[] =
{
   {EAP_METHOD_TYPE_IDENTITY,      "Identity"},
   {EAP_METHOD_TYPE_NOTIFICATION,  "Notification"},
   {EAP_METHOD_TYPE_NAK,           "Nak"},
   {EAP_METHOD_TYPE_MD5_CHALLENGE, "MD5-Challenge"},
   {EAP_METHOD_TYPE_OTP,           "One-Time Password"},
   {EAP_METHOD_TYPE_GTC,           "Generic Token Card"},
   {EAP_METHOD_TYPE_TLS,           "EAP-TLS"},
   {EAP_METHOD_TYPE_TTLS,          "EAP-TTLS"},
   {EAP_METHOD_TYPE_PEAP,          "PEAP"},
   {EAP_METHOD_TYPE_MSCHAP_V2,     "EAP-MSCHAP-V2"},
   {EAP_METHOD_TYPE_EXPANDED_NAK,  "Expanded NAK"}
};


/**
 * @brief Dump EAPOL header for debugging purpose
 * @param[in] header Pointer to the EAPOL header
 **/

void eapolDumpHeader(const EapolPdu *header)
{
   const char_t *packetTypeName;

   //Convert the Packet Type field to string representation
   packetTypeName = eapGetParamName(header->packetType, eapolPacketTypeList,
      arraysize(eapolPacketTypeList));

   //Dump EAPOL header contents
   TRACE_DEBUG("  Protocol Version = %" PRIu8 "\r\n", header->protocolVersion);
   TRACE_DEBUG("  Packet Type = %" PRIu8 " (%s)\r\n", header->packetType, packetTypeName);
   TRACE_DEBUG("  Packet Body Length = %" PRIu16 "\r\n", ntohs(header->packetBodyLen));
}


/**
 * @brief Dump EAP header for debugging purpose
 * @param[in] header Pointer to the EAP header
 **/

void eapDumpHeader(const EapPacket *header)
{
   const char_t *codeName;
   const char_t *methodTypeName;

   //Convert the Code field to string representation
   codeName = eapGetParamName(header->code, eapCodeList,
      arraysize(eapCodeList));

   //Dump EAP header contents
   TRACE_DEBUG("  Code = %" PRIu8 " (%s)\r\n", header->code, codeName);
   TRACE_DEBUG("  Identifier = %" PRIu8 "\r\n", header->identifier);
   TRACE_DEBUG("  Length = %" PRIu16 "\r\n", ntohs(header->length));

   //Check Code field
   if(header->code == EAP_CODE_REQUEST ||
      header->code == EAP_CODE_RESPONSE)
   {
      //Convert the Method Type field to string representation
      methodTypeName = eapGetParamName(header->data[0], methodTypeList,
         arraysize(methodTypeList));

      //Dump Method Type field
      TRACE_DEBUG("  Method Type = %" PRIu8 " (%s)\r\n", header->data[0],
         methodTypeName);

      //EAP-TLS method?
      if(header->data[0] == EAP_METHOD_TYPE_TLS)
      {
         //Dump Flags field
         eapDumpTlsFlags(header->data[1]);
      }
   }
}


/**
 * @brief Dump EAP-TLS flags
 * @param[in] flags EAP-TLS specific options
 **/

void eapDumpTlsFlags(uint8_t flags)
{
   uint8_t l;
   uint8_t m;
   uint8_t s;

   //The L flag (length included) is set to indicate the presence of the
   //four-octet TLS Message Length field
   l = (flags & EAP_TLS_FLAGS_L) ? 1 : 0;

   //The M flag (more fragments) is set on all but the last fragment
   m = (flags & EAP_TLS_FLAGS_M) ? 1 : 0;

   //The S flag (EAP-TLS start) is set only within the EAP-TLS start message
   s = (flags & EAP_TLS_FLAGS_S) ? 1 : 0;

   //Check whether any flag is set
   if(l != 0 || m != 0 || s != 0)
   {
      //Dump the value of the Flags field
      TRACE_DEBUG("  Flags = 0x%02" PRIX8 " (", flags);

      //Dump flags
      while(1)
      {
         if(l != 0)
         {
            TRACE_DEBUG("Length");
            l = FALSE;
         }
         else if(m != 0)
         {
            TRACE_DEBUG("More");
            m = FALSE;
         }
         else if(s != 0)
         {
            TRACE_DEBUG("Start");
            s = FALSE;
         }
         else
         {
         }

         if(l != 0 || m != 0 || s != 0)
         {
            TRACE_DEBUG(", ");
         }
         else
         {
            TRACE_DEBUG(")\r\n");
            break;
         }
      }
   }
   else
   {
      //Dump the value of the Flags field
      TRACE_DEBUG("  Flags = 0x%02" PRIX8 "\r\n", flags);
   }
}

#endif


/**
 * @brief Convert a parameter to string representation
 * @param[in] value Parameter value
 * @param[in] paramList List of acceptable parameters
 * @param[in] paramListLen Number of entries in the list
 * @return NULL-terminated string describing the parameter
 **/

const char_t *eapGetParamName(uint_t value, const EapParamName *paramList,
   size_t paramListLen)
{
   uint_t i;

   //Default name for unknown values
   static const char_t defaultName[] = "Unknown";

   //Loop through the list of acceptable parameters
   for(i = 0; i < paramListLen; i++)
   {
      if(paramList[i].value == value)
         return paramList[i].name;
   }

   //Unknown value
   return defaultName;
}

#endif
