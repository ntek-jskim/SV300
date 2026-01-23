/**
 * @file main.c
 * @brief Main routine
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
#include <stdlib.h>
#if 1    // NXP
   #include "lpcxpresso_4337.h"
#else
   #include "sam.h"
   #include "same54_curiosity_ultra.h"
#endif   // NXP

#include "core/net.h"

#if 1    // NXP
   #include "drivers/mac/lpc43xx_eth_driver.h"
   #include "drivers/phy/lan8720_driver.h"
#else
   #include "drivers/mac/same54_eth_driver.h"
#endif   // NXP

#include "drivers/switch/ksz8863_driver.h"
#include "drivers/switch/lan9303_driver.h"
#include "rstp/rstp.h"
#include "dhcp/dhcp_client.h"
#include "ipv6/slaac.h"
#include "snmp/snmp_agent.h"
#include "mibs/snmp_mib_module.h"
#include "mibs/snmp_mib_impl.h"
#include "mibs/bridge_mib_module.h"
#include "mibs/bridge_mib_impl.h"
#include "mibs/rstp_mib_module.h"
#include "mibs/rstp_mib_impl.h"
// modbus server
#if 1
   #include "modbus/modbus_server.h"
   //Application configuration
   #define APP_MODBUS_SERVER_PORT 9502
   #define APP_MODBUS_SERVER_TIMEOUT 600000
   #define APP_MODBUS_SERVER_KEEP_ALIVE_IDLE 10000
   #define APP_MODBUS_SERVER_KEEP_ALIVE_INTERVAL 5000
   #define APP_MODBUS_SERVER_KEEP_ALIVE_PROBES 4
#endif

#if 1
   #include "sntp/sntp_client.h"
   #include "date_time.h"
#endif

#if 1    // NXP
	#include "smi_driver.h" // for SMI protocol
#else 
   #include "spi_driver.h"
#endif   // NXP   
#include "debug.h"
#include "proxy.h"


#if 1	// IP Scan Listener(cskang)
	#include "ipv4/ipv4.h"
	#define MULTICAST_IP "238.0.100.1"  // 예제 멀티캐스트 주소
	#define MULTICAST_PORT 5050             // 예제 멀티캐스트 포트
	#define KEY_V0	"*get_network_info*"
	#define KEY_V1	"@get_network_info@"
	static uint8_t mcast_rb[64];
	static uint8_t mcast_tb[64];	
	extern void getMeterInfoV0(uint8_t *sb);
	extern void getMeterInfoV1(uint8_t *sb);
#endif

//Ethernet interface configuration
#define APP_IF_NAME "eth0"
#define APP_HOST_NAME "rstp-bridge-demo"
#define APP_MAC_ADDR "00-AB-CD-54-20-00"

//#define APP_USE_DHCP_CLIENT ENABLED
#define APP_USE_DHCP_CLIENT DISABLED
#define APP_IPV4_HOST_ADDR "192.168.8.102"
#define APP_IPV4_SUBNET_MASK "255.255.255.0"
#define APP_IPV4_DEFAULT_GATEWAY "192.168.8.1"
#define APP_IPV4_PRIMARY_DNS "168.126.63.1"
#define APP_IPV4_SECONDARY_DNS "8.8.8.8"

#define APP_USE_SLAAC ENABLED
#define APP_IPV6_LINK_LOCAL_ADDR "fe80::5420"
#define APP_IPV6_PREFIX "2001:db8::"
#define APP_IPV6_PREFIX_LENGTH 64
#define APP_IPV6_GLOBAL_ADDR "2001:db8::5420"
#define APP_IPV6_ROUTER "fe80::1"
#define APP_IPV6_PRIMARY_DNS "2001:4860:4860::8888"
#define APP_IPV6_SECONDARY_DNS "2001:4860:4860::8844"

//Application configuration
#define RSTP_BRIDGE_NUM_PORTS 2

//Global variables
RstpBridgeSettings rstpBridgeSettings;
RstpBridgeContext rstpBridgeContext;
RstpBridgePort rstpBridgePorts[RSTP_BRIDGE_NUM_PORTS];
DhcpClientSettings dhcpClientSettings;
DhcpClientContext dhcpClientContext;
SlaacSettings slaacSettings;
SlaacContext slaacContext;
SnmpAgentSettings snmpAgentSettings;
SnmpAgentContext snmpAgentContext;

extern uint16_t *getNtpIp();
extern uint16_t getNtpInterval();
extern int16_t  getTimeZone();
extern uint16_t getModbusPort();
extern void     setMeterIpAddr(uint32_t);
extern OsTaskId getW5500TaskId();
extern void Sntp_Set(int v);

#if 1 // modbus server
   ModbusServerSettings modbusServerSettings;
   ModbusServerContext modbusServerContext;
   //Forward declaration of functions
   error_t modbusServerOpenCallback(ModbusClientConnection *connection,
      IpAddr clientIpAddr, uint16_t clientPort);

   void modbusServerLockCallback(void);
   void modbusServerUnlockCallback(void);

   error_t modbusServerReadCoilCallback(const char_t *role, uint16_t address,
      bool_t *state);

   error_t modbusServerWriteCoilCallback(const char_t *role, uint16_t address,
      bool_t state, bool_t commit);

   error_t modbusServerReadRegCallback(const char_t *role, uint16_t address,
      uint16_t *value);

   error_t modbusServerWriteRegCallback(const char_t *role, uint16_t address,
      uint16_t value, bool_t commit);
   
   extern int readMemCb(int address, uint16_t *value);
   extern int writeMemCb(int address, uint16_t value);
#endif
	 
#ifdef _FTP_SERVER
   //Application configuration
   #define APP_FTP_MAX_CONNECTIONS 4
   
	#include "ftp/ftp_server.h"
	FtpServerSettings ftpServerSettings;
	FtpServerContext ftpServerContext;
   FtpClientConnection ftpConnections[APP_FTP_MAX_CONNECTIONS];
   
   uint_t ftpServerCheckUserCallback(FtpClientConnection *connection,
      const char_t *user);

   uint_t ftpServerCheckPasswordCallback(FtpClientConnection *connection,
      const char_t *user, const char_t *password);

   uint_t ftpServerGetFilePermCallback(FtpClientConnection *connection,
      const char_t *user, const char_t *path);
#endif // _FTP_SERVER
   
#if 1
   void init(void);
   void db_init(void);
   void app_init(void *);
   void Timer1_Init(int);
   extern void tickSet(uint32_t sec, uint32_t msec, uint32_t mode);
   int  dhcpModeGet();
   void ipAddrGet(char *);
   void subnetMaskGet(char *);
   void gatewayIpGet(char *);
   void priDnsIpGet(char *);
   void secDnsIpGet(char *);
   void macAddrGetStr(char *);
#endif   

#if 1
   SntpClientContext sntpClientContext;   
#endif
 
/**
 * @brief I/O initialization
 **/
void ioInit(void)
{
   //Enable GPIO peripheral clock
   LPC_CCU1->CLK_M4_GPIO_CFG |= CCU1_CLK_M4_GPIO_CFG_RUN_Msk;
   while(!(LPC_CCU1->CLK_M4_GPIO_STAT & CCU1_CLK_M4_GPIO_STAT_RUN_Msk));
   Board_GPIO_Init();
}


#define APP_NTP_SERVER_NAME "time.google.com"
#define APP_NTP_IP "216.239.35.4"

error_t sntpGetTime(char *ntp_server)
{
   error_t error;
   uint32_t kissCode;
   time_t unixTime;
   DateTime date;
   IpAddr ipAddr;
   NtpTimestamp timestamp;
#if 1
   uint64_t fraction;
   int msec;
#endif   

   //Initialize SNTP client context
   sntpClientInit(&sntpClientContext);

   //Start of exception handling block
   do
   {
      //Debug message
      TRACE_INFO("\r\n\r\nResolving server name(%s)...\r\n", ntp_server);

      //Resolve NTP server name
      error = getHostByName(NULL, ntp_server, &ipAddr, 0);
      //Any error to report?
      if(error)
      {
         //Debug message
         TRACE_INFO("Failed to resolve server name!\r\n");
         break;
      }

      //Set timeout value for blocking operations
      error = sntpClientSetTimeout(&sntpClientContext, 20000);
      //Any error to report?
      if(error)
         break;

      //Specify the IP address of the NTP server
      error = sntpClientSetServerAddr(&sntpClientContext, &ipAddr, NTP_PORT);
      //Any error to report?
      if(error)
         break;

      //Retrieve current time from NTP server
      error = sntpClientGetTimestamp(&sntpClientContext, &timestamp);

      //Check status code
      if(error == NO_ERROR)
      {
         //Unix time starts on January 1st, 1970
         unixTime = timestamp.seconds - NTP_UNIX_EPOCH;
         //Convert Unix timestamp to date
         convertUnixTimeToDate(unixTime, &date);
         //Debug message
         TRACE_INFO("UTC   date/time: %s.%03d\r\n", formatDate(&date, NULL), msec);   
//         printf("UTC   date/time: %s.%03d\r\n", formatDate(&date, NULL), msec);   
#if 1
         fraction = timestamp.fraction;
         // 밀리초 계산 (하위 32비트를 2^32로 나누고 1000을 곱함)
         msec = (uint32_t)((fraction * 1000ULL) >> 32);
			// utc time to localtime
			unixTime += (getTimeZone() * 60);	// timezon			
			tickSet(unixTime, msec, 0);
#endif
			//Convert Unix timestamp to date
         convertUnixTimeToDate(unixTime, &date);           
         //Debug message
         TRACE_INFO("Local date/time: %s.%03d\r\n", formatDate(&date, NULL), msec);       
      }
      else if(error == ERROR_REQUEST_REJECTED)
      {
         //Retrieve kiss code
         kissCode = sntpClientGetKissCode(&sntpClientContext);
         // printf("UTC   ERROR_REQUEST_REJECTED: kissCode  '%c%c%c%c'\r\n", (kissCode >> 24) & 0xFF,
         //    (kissCode >> 16) & 0xFF, (kissCode >> 8) & 0xFF, kissCode & 0xFF);

         //Debug message
         TRACE_INFO("Kiss code: '%c%c%c%c'\r\n", (kissCode >> 24) & 0xFF,
            (kissCode >> 16) & 0xFF, (kissCode >> 8) & 0xFF, kissCode & 0xFF);
      }
      else
      {
         //Debug message
         TRACE_INFO("Failed to retrieve NTP timestamp!\r\n");
//         printf("[%d]Failed to retrieve NTP timestamp!\r\n", error);   
      }

      //End of exception handling block
   } while(0);

   //Release SNTP client context
   sntpClientDeinit(&sntpClientContext);

   //Return status code
   return error;
}

void sntpTask(void *arg)
{
   char_t buffer[40];
#if (IPV4_SUPPORT == ENABLED)
   Ipv4Addr ipv4Addr;
#endif
   int interval = 0, errcnt=0;
   error_t error;
   uint16_t *ntp_addr = getNtpIp();
	uint16_t ntpInterval = getNtpInterval();

   //Point to the network interface
   NetInterface *interface = &netInterface[0];

   printf("sntpTask interval = %d\n", ntpInterval);
   
   osDelayTask(10000);
   interval = 60;
//   ntpInterval = 1;
   //Endless loop
   while(1)
   {
#if (IPV4_SUPPORT == ENABLED)
      ipv4GetHostAddr(interface, &ipv4Addr);
      printf("IPv4 Addr : %-16s\r\n", ipv4AddrToString(ipv4Addr, buffer));
#endif
      if (++interval >= ntpInterval) {
         interval = 0;
         sprintf(buffer, "%d.%d.%d.%d", ntp_addr[0], ntp_addr[1], ntp_addr[2], ntp_addr[3]);
         error = sntpGetTime(buffer);
         if (error == NO_ERROR) {
            Sntp_Set(0);
            errcnt = 0;            
//            interval = 0;
         }
         else {
            if(++errcnt > 5)
               Sntp_Set(1);
               
            osDelayTask(1000);
            continue;
         }
      }               
      osDelayTask(60000);   
//      osDelayTask(10000);   
   }   
}

int   getRstpState(int port)
{
   error_t error;
   StpPortState     sts;

   error = rstpGetPortState(&rstpBridgeContext, port, &sts);
   //printf("getRstpState[%d] = %d, %d\n", port, sts, error);
   return sts;
}

void dhcpClientStateChangeCallback(DhcpClientContext *context,
   NetInterface *interface, DhcpState state) 
{
	if (state == DHCP_STATE_BOUND) {
		Ipv4Addr ipAddr;
		char ipStr[16];

		ipv4GetHostAddr(interface, &ipAddr);
		ipv4AddrToString(ipAddr, ipStr);
		printf("💡 DHCP IP Assigned: %s\n", ipStr);
		
		//memcpy(&dhcpIpAddr, &ipAddr, sizeof(dhcpIpAddr));
		setMeterIpAddr(ipAddr);
	}
}

void init(void) 
{
   error_t error;
   uint_t i;
   OsTaskId taskId;
   OsTaskParameters taskParams;
   NetInterface *interface;
   MacAddr macAddr;
   Ipv4Addr ipv4Addr;
   Ipv6Addr ipv6Addr;
   char macstr[32];
   
   db_init();


   //SNMPv2-MIB initialization
   error = snmpMibInit();
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize SNMPv2-MIB!\r\n");
   }

   //BRIDGE-MIB initialization
   error = bridgeMibInit();
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize BRIDGE-MIB!\r\n");
   }

   //RSTP-MIB initialization
   error = rstpMibInit();
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize RSTP-MIB!\r\n");
   }

   //TCP/IP stack initialization
   error = netInit();
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize TCP/IP stack!\r\n");
   }

   //Configure the first Ethernet interface
   interface = &netInterface[0];

   //Set interface name
   netSetInterfaceName(interface, APP_IF_NAME);
   //Set host name
   netSetHostname(interface, APP_HOST_NAME);
   //Set host MAC address
   macAddrGetStr(macstr);
   macStringToAddr(macstr, &macAddr);
   netSetMacAddr(interface, &macAddr);
   //Select the relevant MAC driver
   netSetDriver(interface, &lpc43xxEthDriver);
   
#if defined(USE_KSZ8863)
   //Select the relevant switch driver
   netSetSwitchDriver(interface, &ksz8863SwitchDriver);
#if 1
   //Underlying SPI driver
   netSetSmiDriver(interface, &smiDriver);
#else   
   //Underlying SPI driver
   netSetSpiDriver(interface, &spiDriver);
#endif   
#elif defined(USE_LAN9303)
   //Select the relevant switch driver
   netSetSwitchDriver(interface, &lan9303SwitchDriver);
#endif

   //Initialize network interface
   error = netConfigInterface(interface);
   //Any error to report?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to configure interface %s!\r\n", interface->name);
   }

   //Get default settings
   rstpGetDefaultSettings(&rstpBridgeSettings);
   //Underlying network interface
   rstpBridgeSettings.interface = interface;
   //Bridge ports
   rstpBridgeSettings.numPorts = RSTP_BRIDGE_NUM_PORTS;
   rstpBridgeSettings.ports = rstpBridgePorts;

   //RSTP bridge initialization
   error = rstpInit(&rstpBridgeContext, &rstpBridgeSettings);
   //Failed to initialize RSTP bridge?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize RSTP bridge!\r\n");
   }

   //Configure bridge ports
   for(i = 1; i <= rstpBridgeContext.numPorts; i++)
   {
      rstpSetAdminPointToPointMac(&rstpBridgeContext, i, RSTP_ADMIN_P2P_MAC_FORCE_TRUE);
      rstpSetAutoEdgePort(&rstpBridgeContext, i, TRUE);
      rstpSetAdminPortState(&rstpBridgeContext, i, TRUE);
   }

   //Start RSTP bridge
   error = rstpStart(&rstpBridgeContext);
   //Failed to start RSTP bridge?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start RSTP bridge!\r\n");
   }

   //Attach the RSTP bridge context to the BRIDGE-MIB and RSTP-MIB
   bridgeMibSetRstpBridgeContext(&rstpBridgeContext);
   rstpMibSetRstpBridgeContext(&rstpBridgeContext);

   if (dhcpModeGet()) {
      //Get default settings
      dhcpClientGetDefaultSettings(&dhcpClientSettings);
      //Set the network interface to be configured by DHCP
      dhcpClientSettings.interface = interface;
      //Disable rapid commit option
      dhcpClientSettings.rapidCommit = FALSE;

		// 2025-3-24, dhcpStateChange CallBack
		dhcpClientSettings.stateChangeEvent = dhcpClientStateChangeCallback;
		
      //DHCP client initialization
      error = dhcpClientInit(&dhcpClientContext, &dhcpClientSettings);
      //Failed to initialize DHCP client?
      if(error)
      {
         //Debug message
         TRACE_ERROR("Failed to initialize DHCP client!\r\n");
      }

      //Start DHCP client
      error = dhcpClientStart(&dhcpClientContext);
      //Failed to start DHCP client?
      if(error)
      {
         //Debug message
         TRACE_ERROR("Failed to start DHCP client!\r\n");
      }
   }
   else {
      char ipstr[32];
      
      //Set IPv4 host address
      ipAddrGet(ipstr);
      ipv4StringToAddr(ipstr, &ipv4Addr);
      ipv4SetHostAddr(interface, ipv4Addr);
		setMeterIpAddr(ipv4Addr);
		
      //Set subnet mask
      subnetMaskGet(ipstr);
      ipv4StringToAddr(ipstr, &ipv4Addr);
      ipv4SetSubnetMask(interface, ipv4Addr);

      //Set default gateway
      gatewayIpGet(ipstr);
      ipv4StringToAddr(ipstr, &ipv4Addr);
      ipv4SetDefaultGateway(interface, ipv4Addr);

      //Set primary and secondary DNS servers
      priDnsIpGet(ipstr);
      ipv4StringToAddr(ipstr, &ipv4Addr);
      ipv4SetDnsServer(interface, 0, ipv4Addr);
      secDnsIpGet(ipstr);
      ipv4StringToAddr(ipstr, &ipv4Addr);
      ipv4SetDnsServer(interface, 1, ipv4Addr);
   }

#if (IPV6_SUPPORT == ENABLED)
#if (APP_USE_SLAAC == ENABLED)
   //Get default settings
   slaacGetDefaultSettings(&slaacSettings);
   //Set the network interface to be configured
   slaacSettings.interface = interface;

   //SLAAC initialization
   error = slaacInit(&slaacContext, &slaacSettings);
   //Failed to initialize SLAAC?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize SLAAC!\r\n");
   }

   //Start IPv6 address autoconfiguration process
   error = slaacStart(&slaacContext);
   //Failed to start SLAAC process?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start SLAAC!\r\n");
   }
#else
   //Set link-local address
   ipv6StringToAddr(APP_IPV6_LINK_LOCAL_ADDR, &ipv6Addr);
   ipv6SetLinkLocalAddr(interface, &ipv6Addr);

   //Set IPv6 prefix
   ipv6StringToAddr(APP_IPV6_PREFIX, &ipv6Addr);
   ipv6SetPrefix(interface, 0, &ipv6Addr, APP_IPV6_PREFIX_LENGTH);

   //Set global address
   ipv6StringToAddr(APP_IPV6_GLOBAL_ADDR, &ipv6Addr);
   ipv6SetGlobalAddr(interface, 0, &ipv6Addr);

   //Set default router
   ipv6StringToAddr(APP_IPV6_ROUTER, &ipv6Addr);
   ipv6SetDefaultRouter(interface, 0, &ipv6Addr);

   //Set primary and secondary DNS servers
   ipv6StringToAddr(APP_IPV6_PRIMARY_DNS, &ipv6Addr);
   ipv6SetDnsServer(interface, 0, &ipv6Addr);
   ipv6StringToAddr(APP_IPV6_SECONDARY_DNS, &ipv6Addr);
   ipv6SetDnsServer(interface, 1, &ipv6Addr);
#endif
#endif

   //Get default settings
   snmpAgentGetDefaultSettings(&snmpAgentSettings);
   //Minimum version accepted by the SNMP agent
   snmpAgentSettings.versionMin = SNMP_VERSION_1;
   //Maximum version accepted by the SNMP agent
   snmpAgentSettings.versionMax = SNMP_VERSION_2C;
   //SNMP port number
   snmpAgentSettings.port = SNMP_PORT;
   //SNMP trap port number
   snmpAgentSettings.trapPort = SNMP_TRAP_PORT;

   //SNMP agent initialization
   error = snmpAgentInit(&snmpAgentContext, &snmpAgentSettings);
   //Failed to initialize SNMP agent?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize SNMP agent!\r\n");
   }

   //Load SNMPv2-MIB
   snmpAgentLoadMib(&snmpAgentContext, &snmpMibModule);
   //Load BRIDGE-MIB
   snmpAgentLoadMib(&snmpAgentContext, &bridgeMibModule);
   //Load RSTP-MIB
   snmpAgentLoadMib(&snmpAgentContext, &rstpMibModule);

   //Set read-only community string
   snmpAgentCreateCommunity(&snmpAgentContext, "public",
      SNMP_ACCESS_READ_ONLY);

   //Set read-write community string
   snmpAgentCreateCommunity(&snmpAgentContext, "private",
      SNMP_ACCESS_READ_WRITE);

   //Start SNMP agent
   error = snmpAgentStart(&snmpAgentContext);
   //Failed to start SNMP agent?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start SNMP agent!\r\n");
   }

#if   1  // modbus
//Get default settings
   modbusServerGetDefaultSettings(&modbusServerSettings);
   //Bind Modbus/TCP server to the desired interface
   modbusServerSettings.interface = &netInterface[0];
   //Listen to port 502
   modbusServerSettings.port = getModbusPort(); 	// APP_MODBUS_SERVER_PORT;
   //Idle connection timeout
   modbusServerSettings.timeout = APP_MODBUS_SERVER_TIMEOUT;
   //Callback functions
   modbusServerSettings.openCallback = modbusServerOpenCallback;
   modbusServerSettings.lockCallback = modbusServerLockCallback;
   modbusServerSettings.unlockCallback = modbusServerUnlockCallback;
   modbusServerSettings.readCoilCallback = modbusServerReadCoilCallback;
   modbusServerSettings.writeCoilCallback = modbusServerWriteCoilCallback;
   modbusServerSettings.readRegCallback = modbusServerReadRegCallback;
   modbusServerSettings.writeRegCallback = modbusServerWriteRegCallback;

   //Modbus/TCP server initialization
   error = modbusServerInit(&modbusServerContext, &modbusServerSettings);
   //Failed to initialize Modbus/TCP server?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize Modbus/TCP server!\r\n");
   }

   //Start Modbus/TCP server
   error = modbusServerStart(&modbusServerContext);
   //Failed to start Modbus/TCP server?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start Modbus/TCP server!\r\n");
   }
#endif
   
#ifdef _FTP_SERVER	
   //Get default settings
   ftpServerGetDefaultSettings(&ftpServerSettings);
   //Bind FTP server to the desired interface
   ftpServerSettings.interface = &netInterface[0];
   //Listen to port 21
   ftpServerSettings.port = FTP_PORT;
	ftpServerSettings.passivePortMin = 50000;  // Passive Mode 포트 범위 시작
   ftpServerSettings.passivePortMax = 50100;  // Passive Mode 포트 범위 끝
	
   //Security modes
   ftpServerSettings.mode = FTP_SERVER_MODE_PLAINTEXT;
   //Client connections
   ftpServerSettings.maxConnections = APP_FTP_MAX_CONNECTIONS;
   ftpServerSettings.connections = ftpConnections;
   //Root directory
   strcpy(ftpServerSettings.rootDir, "/");
   //TLS initialization callback function
   //ftpServerSettings.tlsInitCallback = ftpServerTlsInitCallback;
   //User verification callback function
   ftpServerSettings.checkUserCallback = ftpServerCheckUserCallback;
   //Password verification callback function
   ftpServerSettings.checkPasswordCallback = ftpServerCheckPasswordCallback;
   //Callback used to retrieve file permissions
   ftpServerSettings.getFilePermCallback = ftpServerGetFilePermCallback;

   //FTP server initialization
   error = ftpServerInit(&ftpServerContext, &ftpServerSettings);
   //Failed to initialize FTP server?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to initialize FTP server!\r\n");
   }

   //Start FTP server
	ftpServerContext.taskParams.priority = OS_TASK_PRIORITY_LOW;
   error = ftpServerStart(&ftpServerContext);
   //Failed to start FTP server?
   if(error)
   {
      //Debug message
      TRACE_ERROR("Failed to start FTP server!\r\n");
   }	 
#endif	// _FTP_SERVER


   //Set task parameters
   taskParams = OS_TASK_DEFAULT_PARAMS;
   taskParams.stackSize = 200;
   taskParams.priority = OS_TASK_PRIORITY_NORMAL;

   app_init(0);
   
#ifdef __FREERTOS   
   vTaskSuspend(NULL);
#else
   os_tsk_delete_self();
#endif   
}

#if 1

void proxyGetTxBuf(uint8_t *buf, int *nw);
void proxyPutRxBuf(uint8_t *buf, int  nr);
// proxy buffer
static uint8_t _buf[N_PROXY_BUF];

// buffer 부족시 nsent 는 nw 보다 적게 반환된다
int n_send(Socket *sock, uint8_t *buf, int nw) {
	size_t nsent, os = 0;
	int error;
	while (nw > 0) {
		error = socketSend(sock, &_buf[os], nw, &nsent, 0);
		if (nw != nsent) printf("--> socketSend, nw=%d, nsent=%d, e=%d\n", nw, nsent, error);
		if (error == 0 || error == ERROR_TIMEOUT) {
			if (nw != nsent) osDelayTask(5);
			nw -= nsent;
			os += nsent;
		}
		else {
			return -1;
		}
	}
	
	return nw == 0 ? 0 : -1;
}

extern void osEventFlagsSet(OsTaskId tid, int timeot);
extern void osEventFlagsWait(int flag, int timeout);

// 2025-2-26, cskang
void ProxyTask(void *arg) 
{
	error_t error;
	Socket *socket;
	Socket *clisock;
	size_t nr, nsent, nw, os;
	int	cmd;
	
	setProxyServerTid();
	
	// TCP 소켓 생성
	socket = socketOpen(SOCKET_TYPE_STREAM, SOCKET_IP_PROTO_TCP);
	if (!socket)
	{
		printf("Error on socketOpen!\n");
		osDeleteTask(OS_SELF_TASK_ID);
	}

	error = socketBind(socket, &IP_ADDR_ANY, 22);
	if (error)
	{
		printf("Error on socketBind!\n");
		socketClose(socket);
		osDeleteTask(OS_SELF_TASK_ID);;
	}

	error = socketListen(socket, 1);
	if (error ) {
		printf("Error on socketBind!\n");
		socketClose(socket);
		osDeleteTask(OS_SELF_TASK_ID);;
	}	
	
	while (1)
	{
		clisock = socketAccept(socket, NULL, NULL);
		if (!clisock)
		{
			printf("⚠️ Failed to accept connection\n");
			continue;
		}
		printf("--> accept new connection\n");
				
		socketSetTimeout(clisock, 0);  // Non-blocking
		pushSvrCmd(PROXY_ACCEPT);
		 
		while (1) {
			if (isSvrQFull() == 0) {
				
				error = socketReceive(clisock, _buf, sizeof(_buf), &nr, 0);			
				if (error == 0 || error == ERROR_TIMEOUT) {
				}
				else {
					printf("--> Client disconnected or error (%d)\n", error);
					break;				
				}
				
				if (nr > 0) {
					//printf("--> socketReceive, nr=%d\n", nr);
					pushSvrData(PROXY_S_TO_C, _buf, nr);
				}
			}

			//
			error = popCliData(&cmd, _buf, &nw);
			if (error == 0) {				
				if (cmd == PROXY_C_TO_S) {
					error = n_send(clisock, _buf, nw);
					if (error == 0) {
					}
					else {
						break;
					}
				}
				else if (cmd == PROXY_CLOSE) {
					break;
				}
			}
			
			//osDelayTask(10);
			osThreadFlagsWait(0x1, 10);
		}
		printf("close client socket ...\n");
		socketClose(clisock);
		pushSvrCmd(PROXY_CLOSE);
		
		//osDelayTask(10);
		osThreadFlagsWait(0x1, 10);
	}
}

#endif

// 2025-2-26, cskang
void IpScanListener(void *arg) 
{
	error_t error;
	Socket *socket;
	IpAddr groupAddr;
	Ipv4Addr localAddr;
	IpAddr remoteAddr;
	size_t length, nw;
	uint16_t remotePort;
	NetInterface *interface;

	// 네트워크 인터페이스 가져오기 (기본 인터페이스 사용)
	interface = netGetDefaultInterface();

	// UDP 소켓 생성
	socket = socketOpen(SOCKET_TYPE_DGRAM, SOCKET_IP_PROTO_UDP);
	if (!socket)
	{
		printf("Error on socketOpen!\n");
		osDeleteTask(OS_SELF_TASK_ID);
	}

	// 소켓을 특정 네트워크 인터페이스에 바인딩
	ipv4StringToAddr("0.0.0.0", &localAddr);
	error = socketBindToInterface(socket, interface);
	if (error)
	{
		printf("Error on socketBindToInterface!\n");
		socketClose(socket);
		osDeleteTask(OS_SELF_TASK_ID);;
	}

	// 로컬 포트 바인딩 (멀티캐스트 포트)
	error = socketBind(socket, &IP_ADDR_ANY, MULTICAST_PORT);
	if (error)
	{
		printf("Error on socketBind!\n");
		socketClose(socket);
		osDeleteTask(OS_SELF_TASK_ID);;
	}

	// 멀티캐스트 그룹 가입
	ipv4StringToAddr(MULTICAST_IP, &groupAddr.ipv4Addr);
	groupAddr.length = sizeof(Ipv4Addr);
	error = socketJoinMulticastGroup(socket, &groupAddr);
	if (error)
	{
		printf("Error on socketJoinMulticastGroup!\n");
		socketClose(socket);
		osDeleteTask(OS_SELF_TASK_ID);;
	}
	
	while (1)
	{
		// 데이터 수신
		length = sizeof(mcast_rb);
		error = socketReceiveFrom(socket, &remoteAddr, &remotePort, mcast_rb, sizeof(mcast_rb), &length, 0);
		if (!error)
		{
			mcast_rb[length] = 0;
			printf("[IpScanListener] %s:%d -> %s\n", ipAddrToString(&remoteAddr, NULL), remotePort, mcast_rb);
			
			nw = 0;
			if (strncmp((char *)mcast_rb, KEY_V0, strlen(KEY_V0)) == 0) {		
				getMeterInfoV0(mcast_tb);
				socketSendTo(socket, &groupAddr, remotePort, mcast_tb, sizeof(mcast_tb), &nw, 0);
			}
			else if (strncmp((char *)mcast_rb, KEY_V1, strlen(KEY_V1)) == 0) {
				getMeterInfoV1(mcast_tb);		
				socketSendTo(socket, &groupAddr, remotePort, mcast_tb, sizeof(mcast_tb), &nw, 0);
			}							
		}
		osDelayTask(100);
	}
}


/**
 * @brief Main entry point
 * @return Unused value
 **/

int_t main(void)
{
   //Update system core clock
   SystemCoreClockUpdate();

   //Initialize kernel
   osInitKernel();  	

/* Sets up DEBUG UART */
	Board_Debug_Init();

   //Configure I/Os
   ioInit();
   

   //Start the execution of tasks
#ifdef __FREERTOS   
   osStartKernel();
#else
   osStartKernel(init);
#endif

   //This function should never return
   return 0;
}


/**
 * @brief TCP connection open callback function
 * @param[in] clientIpAddr IP address of the client
 * @param[in] clientPort Port number used by the client
 * @return Error code
 **/

error_t modbusServerOpenCallback(ModbusClientConnection *connection,
   IpAddr clientIpAddr, uint16_t clientPort)
{
   error_t error;

   //Set TCP keep-alive parameters
   error = socketSetKeepAliveParams(connection->socket,
      APP_MODBUS_SERVER_KEEP_ALIVE_IDLE, APP_MODBUS_SERVER_KEEP_ALIVE_INTERVAL,
      APP_MODBUS_SERVER_KEEP_ALIVE_PROBES);
   //Any error to report?
   if(error)
      return error;

   //Enable TCP keep-alive
   error = socketEnableKeepAlive(connection->socket, TRUE);
   //Any error to report?
   if(error)
      return error;

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Lock Modbus table callback function
 **/

void modbusServerLockCallback(void)
{
}


/**
 * @brief Unlock Modbus table callback function
 **/

void modbusServerUnlockCallback(void)
{
}


/**
 * @brief Get coil state callback function
 * @param[in] role Client role (NULL-terminated string)
 * @param[in] address Coil address
 * @param[out] state Current coil state
 * @return Error code
 **/

error_t modbusServerReadCoilCallback(const char_t *role, uint16_t address,
   bool_t *state)
{
   error_t error;

   //Initialize status code
   error = NO_ERROR;

   //Check register address
   printf("modbusServerReadCoilCallback, address=%d\n", address);
   //Return status code
   return error;
}


/**
 * @brief Set coil state callback function
 * @param[in] role Client role (NULL-terminated string)
 * @param[in] address Address of the coil
 * @param[in] state Desired coil state
 * @param[in] commit This flag indicates the current phase (validation phase
 *   or write phase if the validation was successful)
 * @return Error code
 **/

error_t modbusServerWriteCoilCallback(const char_t *role, uint16_t address,
   bool_t state, bool_t commit)
{
   error_t error;

   //Initialize status code
   error = ERROR_INVALID_ADDRESS;

   //Return status code
   return error;
}


/**
 * @brief Get register value callback function
 * @param[in] role Client role (NULL-terminated string)
 * @param[in] address Register address
 * @param[out] state Current register value
 * @return Error code
 **/

error_t modbusServerReadRegCallback(const char_t *role, uint16_t address,
   uint16_t *value)
{
   error_t error;

   //Initialize status code
   error = NO_ERROR;
   
   //Check register address
   //printf("modbusServerReadRegCallback, address=%d\n", address);
   if (readMemCb(address, value) < 0)
      error = ERROR_INVALID_ADDRESS;

   //Return status code
   return error;
}


/**
 * @brief Set register value callback function
 * @param[in] role Client role (NULL-terminated string)
 * @param[in] address Register address
 * @param[in] state Desired register value
 * @param[in] commit This flag indicates the current phase (validation phase
 *   or write phase if the validation was successful)
 * @return Error code
 **/

error_t modbusServerWriteRegCallback(const char_t *role, uint16_t address,
   uint16_t value, bool_t commit)
{
   error_t error;

   //Initialize status code
   error = NO_ERROR;
   
	// printf("modbusServerWriteRegCallback, address=%d, value=%d, commit=%d\n", 
	//    address, value, commit);

   if (commit) {
      writeMemCb(address, value);
   }
//   //Check register address
//   if(address == 40000)
//   {
//      //Write phase?
//      if(commit)
//      {
//      }
//   }
//   else if(address == 40001)
//   {
//      //Write phase?
//      if(commit)
//      {
//      }
//   }
//   else if(address >= 40002 && address <= 40009)
//   {
//      //Writes to registers 40002 to 40009 are ignored
//   }
//   else
//   {
//      //The register address is not acceptable
//      error = ERROR_INVALID_ADDRESS;
//   }

   //Return status code
   return error;
}


#ifdef _FTP_SERVER

/**
 * @brief User verification callback function
 * @param[in] connection Handle referencing a client connection
 * @param[in] user NULL-terminated string that contains the user name
 * @return Access status (FTP_ACCESS_ALLOWED, FTP_ACCESS_DENIED or FTP_PASSWORD_REQUIRED)
 **/

uint_t ftpServerCheckUserCallback(FtpClientConnection *connection,
   const char_t *user)
{
   //Debug message
   TRACE_INFO("FTP server: User verification\r\n");

   //Manage authentication policy
   if(!strcmp(user, "anonymous"))
   {
      return FTP_ACCESS_ALLOWED;
   }
   else if(!strcmp(user, "ftpuser"))
   {
      return FTP_PASSWORD_REQUIRED;
   }
   else
   {
      return FTP_ACCESS_DENIED;
   }
}


/**
 * @brief Password verification callback function
 * @param[in] connection Handle referencing a client connection
 * @param[in] user NULL-terminated string that contains the user name
 * @param[in] password NULL-terminated string that contains the corresponding password
 * @return Access status (FTP_ACCESS_ALLOWED or FTP_ACCESS_DENIED)
 **/

uint_t ftpServerCheckPasswordCallback(FtpClientConnection *connection,
   const char_t *user, const char_t *password)
{
   //Debug message
   TRACE_INFO("FTP server: Password verification\r\n");

   //Verify password
   if(!strcmp(user, "ftpuser") && !strcmp(password, "7300"))
   {
      return FTP_ACCESS_ALLOWED;
   }
   else
   {
      return FTP_ACCESS_DENIED;
   }
}


/**
 * @brief Callback used to retrieve file permissions
 * @param[in] connection Handle referencing a client connection
 * @param[in] user NULL-terminated string that contains the user name
 * @param[in] path Canonical path of the file
 * @return Permissions for the specified file
 **/

uint_t ftpServerGetFilePermCallback(FtpClientConnection *connection,
   const char_t *user, const char_t *path)
{
   uint_t perm;

   //Debug message
   TRACE_INFO("FTP server: Checking access rights for %s\r\n", path);

   //Manage access rights
   if(!strcmp(user, "anonymous"))
   {
      //Check path name
      if(pathMatch(path, "/temp/*"))
      {
         //Allow read/write access to temp directory
         perm = FTP_FILE_PERM_LIST | FTP_FILE_PERM_READ | FTP_FILE_PERM_WRITE;
      }
      else
      {
         //Allow read access only to other directories
         perm = FTP_FILE_PERM_LIST | FTP_FILE_PERM_READ;
      }
   }
   else if(!strcmp(user, "ftpuser"))
   {
      //Allow read/write access
      perm = FTP_FILE_PERM_LIST | FTP_FILE_PERM_READ | FTP_FILE_PERM_WRITE;
   }
   else
   {
      //Deny access
      perm = 0;
   }

   //Return the relevant permissions
   return perm;
}

#endif	// FTPserver

#ifdef USE_FREERTOS
void vApplicationStackOverflowHook(OsTaskId xTask, char *pcTaskName) {
    // Print the name of the task that caused the stack overflow
    printf("Stack overflow in task: %s\n", pcTaskName);

    // Optionally, halt the system for debugging
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void) {
    // Log an error message
    printf("Malloc failed! FreeRTOS could not allocate memory.\n");

    // Disable interrupts and halt the system for debugging
    taskDISABLE_INTERRUPTS();
    for (;;);
}
#endif


