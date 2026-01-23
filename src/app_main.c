#include "os_port.h"
#include "debug.h"
#include "meter.h"
#include "alarm.h"
#include "proxy.h"
#include "i2c_18xx_43xx.h"

OsTaskId tid_fft, tid_wave[2], tid_meter[2], tid_fs;
OsTaskId tid_rmslog, tid_post, tid_energy, tid_trend;
OsTaskId tid_iom;
OsTaskId tid_rtu, tid_mmb;
OsTaskId tid_shell;
OsTaskId t_net;
OsTaskId tid_ftpc;
OsTaskId tid_CAN;
OsTaskId tid_w5500;
OsTaskId tid_sntp;
OsTaskId tid_led;
OsTaskId tid_ipScan;	// IpList Scan Listener(cskang)
OsTaskId tid_proxy;		// 2025-3-25, proxy server
void resetStart(void);
void Sntp_Set(int v);

extern void GUI_Task(void *);
extern void KEY_Task(void *);
extern void FFT_Task(void *);
extern void Meter0_Task(void *);
extern void Meter1_Task(void *);
extern void Wave_Task(void *);
extern void FS_task(void *);
extern void Test_task(void *);
extern void RMSLog_Task(void *);
extern void PostScan_Task(void *);
extern void Energy_Task(void *);
extern void SMB_rtu_Task(void *);
extern void SMB_rtu_Task2(void *);
//extern void Gateway_Task(void *);
//extern void Gateway_MCS_Task(void *);
extern void Trend_Task(void *);
//extern void IOM_Task(void *);
extern void TempScan_Task(void *);
extern void FTPC_Task(void *);
extern void Shell_Task(void *);
extern void W5500_TcpServer(void *);
extern void ledTask(void *);
#ifdef	FLOW_RATE
   extern void FlowScan_Task(void *);
#endif
#ifdef	CAN
   extern void CAN_Task(void *);
#endif
//extern void Shell_Task(void *arg); 
extern void sntpTask(void *arg);
extern void IpScanListener(void *arg);
extern void ProxyTask(void *arg);

extern void Buzzer_Toggle();
extern void loadEnergy();
extern void loadDemand();
extern void Board_DMA_Init();
extern void smb_tcp(void);
extern void telnet_proc(void);

extern void init_smb(void);
extern void init_smb_session(void);
extern void init_mcast_session(void);
extern void init_udp_session(void);
extern void init_telnet_session(int);
extern int  loadEnergyLogFs();
extern void W5500_TcpServer();
extern void timeStampChanged();

extern void selWire(int);
extern int  getRstpState(int port);
extern void shell (void);

static int tick;
uint32_t sysTick32, sysTick1s, sysTick10s, sysTick10m, sysTick15m, sysTickDemand, WM_tick32;
uint64_t sysTick64;
uint32_t et1, et2;

uint16_t *getNtpIp(void) {
	return pdb->comm.sntp;
}

uint16_t getNtpInterval(void) {
	if(pdb->comm.sntpInterval)
		return pdb->comm.sntpInterval;
	else
		return 1;	
}

int16_t getTimeZone() {
	return pdb->etc.timezone;
}

uint32_t fs_get_date (void) {
  uint32_t m,y,date, time;
	//struct tm ltm;
//  /* Adapt the function; Add a system call to read RTC.  */
//  d = 1;              /* Day:   1 - 31      */
//  m = 11;             /* Month: 1 - 12      */
//  y = 2006;           /* Year:  1980 - 2107 */
	
	//localtime_r(&sysTick1s, &ltm);
	y = pcntl->tod.tm_year;	// 4자리로 이미 표현된다
	m = pcntl->tod.tm_mon;
	//printf("[%d-%d-%d, %d:%d:%d]\n", ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);	

  date = (y << 16) | (m << 8) | pcntl->tod.tm_mday;
  return (date);
}


uint32_t fs_get_time (void) {
  uint32_t time;
	//struct tm ltm;
		
//  /* Modify here; Add a system call to read RTC. */
//  h = 12; /* Hours:   0 - 23 */
//  m = 0;  /* Minutes: 0 - 59 */
//  s = 0;  /* Seconds: 0 - 59 */

	//localtime_r(&sysTick1s, &ltm);
  time = (pcntl->tod.tm_hour << 16) | (pcntl->tod.tm_min << 8) | pcntl->tod.tm_sec;
  return (time);
}

void build_mac(int interface, uint8_t *mac)
{
	if (interface == 0) {
		mac[0] = MAC_MSB0;
		mac[1] = MAC_MSB1;
	}
	else {
		mac[0] = 0x00;
		mac[1] = 0x08;
	}
	//mac[2] = finf->modType;
	mac[2] = pcal->mac[0];
	mac[3] = pcal->mac[1];
	mac[4] = pcal->mac[2];
	mac[5] = pcal->mac[3];
}

void _enableTaskMonitor(int id, int limit) {
	pcntl->wdtTbl[id].enable = 1;
	pcntl->wdtTbl[id].limit = limit;
}

void execReboot() {
	if (pcntl->wdtEn) {
		pcntl->rebootFlag = 1;
	}
	else {
		Board_WDT_Enable();
	}
}

void wdtEnable() {
	pcntl->wdtEn = 1;
	Board_WDT_Enable();
}

// 매 100ms 마다 호출된다 
void taskMonitor() {
	int i, wdtError=0, ix=0;
	
	if (pcntl->wdtEn == 0) {
		if (pcntl->runFlag == 0) {
			Board_WDT_Enable();	// reboot
		}		
	}		
	else {			
		for (i=0; i<32; i++) {
			if (pcntl->wdtTbl[i].enable) {
				if (pcntl->wdtTbl[i].count == 0) {
					if (++pcntl->wdtTbl[i].errCnt > pcntl->wdtTbl[i].limit) {
						wdtError++;
						ix = i;
						printf("@@ wdtError, ix=%d, errCnt=%d @@@\n", ix, pcntl->wdtTbl[ix].errCnt);
					}
				}
				else {
					pcntl->wdtTbl[i].count = pcntl->wdtTbl[i].errCnt = 0;
				}
			}
		}

		if (wdtError == 0 && pcntl->runFlag) {
			Board_WDT_Clear();
		}
		else {
			printf("@@@@ wdtError, ix=%d, errCnt=%d @@@\n", ix, pcntl->wdtTbl[ix].errCnt);
		}
		if(wdtError)
	   		pInfo->DEV_sts = STS_ERROR;
		else
	   		pInfo->DEV_sts = STS_OK;
	}
}

void Sntp_Set(int v)
{
	pInfo->SNTP_sts = v;
}


void tickSet(uint32_t sec,  uint32_t msec, uint32_t mode) {
	int diff = abs(sysTick1s - sec);
		
	if (sec != sysTick1s) {
		sysTick1s = sec;
		//sysTick3s  = sysTick1s/3;
		sysTick10s = sysTick1s/10;	// 10 sec, freq sampling 
#ifdef _QUAL_TEST
		sysTick10m = sysTick1s/60;	// 1 minute, volt. averaging
#else		
		sysTick10m = sysTick1s/600;	// 10 minute, volt. averaging
#endif	
		sysTick15m = sysTick1s/900;	// 15 minute, demand
	
		sysTick64  = (uint64_t)sec*1000 + msec;
		sysTick32  = sysTick64;	
		uLocalTime(&sysTick1s, &pcntl->tod);
	
		if (mode && diff > 1) {
			timeStampChanged();
		}
	}
}

uint64_t getSysTick64() {
	return sysTick64;
}

void spiInit() {
	// SSP initialization(for Meter)
	Board_SSP_Init(LPC_SSP0, 8, 0, 16000000);	// 0:8bit, 0:Manual/1:Auto
	// SSP0, SSP1 DMA 초기화
	Board_DMA_Init();
}

void _eepDone(OsTaskId tid, int flag)
{
   if (tid != 0) {
#ifdef __FREERTOS      
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      // Notify the task
      xTaskNotifyFromISR(tid, flag, eSetBits, &xHigherPriorityTaskWoken);   
#else
      isr_evt_set(flag, tid);
#endif      
   }
}

int  _eepWait(int flag, int tout) 
{
   
#ifdef __FREERTOS   
   uint32_t ulNotificationValue;
   xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, pdMS_TO_TICKS(tout));
#else
   OS_RESULT result;
	result = os_evt_wait_and(flag, tout);
   if (result == OS_R_TMO) {
		printf("I2C Timeout ...\n");
		return -1;
	}
	else {
		return 0;
	}
#endif   
}

// uint32_t convert_uint16_to_uint32(uint16_t data1[4]) { 
//    uint32_t data2 = 0; 
//    data2 |= ((uint32_t)data1[3]) << 24; 
//    data2 |= ((uint32_t)data1[2]) << 16; 
//    data2 |= ((uint32_t)data1[1]) << 8; 
//    data2 |= ((uint32_t)data1[0]); 
//    return data2;
// }

// void replace_mac_suffix(char* mac_addr, uint8_t new_data[4]) { 
//    sprintf(mac_addr + 6, "%02X-%02X-%02X-%02X", new_data[0], new_data[1], new_data[2], new_data[3]);
// }

// //#define APP_MAC_ADDR "00-AB-CD-54-20-00"
// #define APP_MAC_ADDR "00-AB-CD-54-20-00"

//void reInitNet(void)
//{
//   int         i;
//   char        mac_addr[] = APP_MAC_ADDR;
//   NetInterface *interface;
//   MacAddr macAddr;
//   Ipv4Addr ipv4Addr,ipv4Subnet, ipv4Gateway, ipv4Dns0;

//   //Configure the first Ethernet interface
//   interface = &netInterface[0];

////   for(i=0; i<4; i++)
////      macAddr.b[2+i] = pcal->mac[i];
//   pcal->mac[0] = 25;
//   pcal->mac[1] = 1;
//   pcal->mac[2] = 2;
//   pcal->mac[3] = 3;

//   replace_mac_suffix(mac_addr,&pcal->mac[0]);
//   macStringToAddr(mac_addr, &macAddr);

//	printf("[[[pcal->MAC %02x:%02x:%02x:%02x]]]\n", pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3]);
//	printf("[[[mac_addr3 %s]]]\n", mac_addr);

//	ipv4Addr = convert_uint16_to_uint32(pdb->comm.ip0);
//	ipv4Gateway = convert_uint16_to_uint32(pdb->comm.gw0);
//	ipv4Subnet = convert_uint16_to_uint32(pdb->comm.sm0);
//	ipv4Dns0 = convert_uint16_to_uint32(pdb->comm.dns0);

//	printf("ip = %x\n", ipv4Addr);

////   netSetMacAddr(interface, &macAddr);
////   netSetDriver(interface, &lpc43xxEthDriver);

//   if(pdb->comm.dhcpEn==0) {
////      ipv4SetHostAddr(interface, ipv4Addr);
////      ipv4SetSubnetMask(interface, ipv4Addr);
////      ipv4SetDefaultGateway(interface, ipv4Addr);
////      ipv4SetDnsServer(interface, 0, ipv4Addr);
////      ipv4SetDnsServer(interface, 1, ipv4Addr);
//   }
//}



#define  _APP_INIT

#ifdef   _APP_INIT

extern void loadEnergy();
extern void loadDemand();
extern void ExtRTC_Init();


extern void W5500_TcpServer();
extern void init_smb();
//void init_card(); 
void FS_Init();

OsTaskId taskIdWiznet;

void db_init(void)
{
	int   wmode;
	// datetime timer
   Timer1_Init(1000);	
   
   // SSP initialization(for Meter)
	Board_SSP_Init(LPC_SSP0, 8, 0, 16000000);	// 0:8bit, 0:Manual/1:Auto
	Board_SSP_Init(LPC_SSP1, 8, 0, 16000000);	// 0:8bit, 0:Manual/1:Auto

	// SSP0, SSP1 DMA 초기화
	Board_DMA_Init();
   
   // Touch & FRAM Controller
   ChipEepInit();
	I2C_Init(I2C0);
   
#ifdef EXT_RTC
	ExtRTC_Init();
#endif   
     
   // Flash File System
   FS_Init();
//   init_card ();	
   
   initAlarmTable();			
	
	// read Setting
	loadHwSettings(pcal);	
	setMeterInfo();
		
	loadSettings(pdb);

	loadEnergy();
	loadDemand();
	loadMaxMin();
	loadAlarmStatus();
	loadAlarmLog();
	loadEventLog();
	
	loadEnergyLogFs();

   
   init_smb();   
	wmode = (pdb->pt.wiring == WM_3LL3CT || pdb->pt.wiring == WM_3LL2CT) ? 1 : 0;
	selWire(wmode);
}

uint16_t getModbusPort() {
	return pdb->comm.tcpPort;
}

int dhcpModeGet() {
   return pdb->comm.dhcpEn;
}


void ipAddrGet(char *buf) {
   sprintf(buf, "%d.%d.%d.%d", pdb->comm.ip0[0], pdb->comm.ip0[1], pdb->comm.ip0[2], pdb->comm.ip0[3]);
   //sprintf(buf, "%d.%d.%d.%d", 192, 168, 11, 110);
}

void subnetMaskGet(char *buf) {
   sprintf(buf, "%d.%d.%d.%d", pdb->comm.sm0[0], pdb->comm.sm0[1], pdb->comm.sm0[2], pdb->comm.sm0[3]);
}

void gatewayIpGet(char *buf) {
   sprintf(buf, "%d.%d.%d.%d", pdb->comm.gw0[0], pdb->comm.gw0[1], pdb->comm.gw0[2], pdb->comm.gw0[3]);
}

void priDnsIpGet(char *buf) {
   sprintf(buf, "%d.%d.%d.%d", pdb->comm.dns0[0], pdb->comm.dns0[1], pdb->comm.dns0[2], pdb->comm.dns0[3]);
}

void secDnsIpGet(char *buf) {
   sprintf(buf, "%d.%d.%d.%d", pdb->comm.dns0[0], pdb->comm.dns0[1], pdb->comm.dns0[2], pdb->comm.dns0[3]);
}

void macAddrGetStr(char *buf) {
	uint8_t		mac[6];
#if 0
	mac[0] = MAC_MSB0;
	mac[1] = MAC_MSB1;
	mac[2] = pcal->mac[0];
	mac[3] = pcal->mac[1];
	mac[4] = pcal->mac[2];
	mac[5] = pcal->mac[3];

	sprintf(buf, "%02x-%02x-%02x-%02x-%02x-%02x", 
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	printf("MAC(CAL) = %02x-%02x-%02x-%02x-%02x-%02x\n", 
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
#else
	uint32_t uid = Chip_IAP_ReadUID();
	mac[0] = MAC_MSB0;	// 로컬 MAC 주소(LAA) : 두번째 비트가 1
	mac[1] = MAC_MSB1;
	pcal->mac[0] = mac[2] = uid >> 24;
	pcal->mac[1] = mac[3] = uid >> 16; 
	pcal->mac[2] = mac[4] = uid >> 8; 
	pcal->mac[3] = mac[5] = uid >> 0; 
   sprintf(buf, "%02x-%02x-%02x-%02x-%02x-%02x", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	printf("MAC(CPU) = %02x-%02x-%02x-%02x-%02x-%02x\n", 
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
}

void resetStart(void){
	pcntl->runFlag = 0;
}

// 2025-2-26, cskang
void macAddrGet(uint8_t *mac) {
	uint32_t uid = Chip_IAP_ReadUID();
	mac[0] = MAC_MSB0;
	mac[1] = MAC_MSB1;
	mac[2] = uid >> 24;
	mac[3] = uid >> 16; 
	mac[4] = uid >> 8; 
	mac[5] = uid >> 0; 	
}

void app_init(void *params) {
   int   wmode;
   OsTaskParameters taskParams;    
   
	// wmode = (pdb->pt.wiring == WM_3LL3CT || pdb->pt.wiring == WM_3LL2CT) ? 1 : 0;
	// selWire(wmode);
	// runFlag가 0이면 Task가 정지된다 
	pcntl->runFlag = 1;

	// ProxyServer
	initProxyAgent();
//   reInitNet();

   //Set task parameters
   taskParams = OS_TASK_DEFAULT_PARAMS;
   taskParams.stackSize = 256;      // 256 * 4byte = 1024

   // SNTP Task
   if (pdb->comm.useSntp) 
   {
      	tid_sntp = osCreateTask("SNTP", sntpTask, pdb->comm.sntp, &taskParams);
      	if(tid_sntp == OS_INVALID_TASK_ID)
      	{
         	//Debug message
         	TRACE_ERROR("Failed to create task(SNTP)!\r\n");
      	};
   }
   else
   		pInfo->SNTP_sts = 0;

   	// Shell
   	taskParams.priority = OS_TASK_PRIORITY_LLOW;
   	tid_fft   = osCreateTask("shell", Shell_Task, NULL, &taskParams);
   	if(tid_fft == OS_INVALID_TASK_ID)
   	{
      	TRACE_ERROR("Failed to create task(Shell)\r\n");
   	}
	
	// test mode 1, normal 0
	if(pdb->etc.testMode) {
		taskParams.priority = OS_TASK_PRIORITY_LOW;
		taskParams.stackSize = 256;
		tid_meter[0] = osCreateTask("TMODE", Test_task, NULL, &taskParams);
		if(tid_meter[0] == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(Test_task)!\r\n");
		}
		// FS
		taskParams.priority = OS_TASK_PRIORITY_LOW;
		taskParams.stackSize = 256;
		tid_fs = osCreateTask("FS", FS_task, NULL, &taskParams);
		if(tid_fs == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(FS)!\r\n");
		}
		// PostScan
		taskParams.priority = OS_TASK_PRIORITY_HIGH;
		tid_post    = osCreateTask("post", PostScan_Task, NULL, &taskParams);
		if(tid_post == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(PostScan)\r\n");
		}

	}
   	else {
		// FFT
		taskParams.priority = OS_TASK_PRIORITY_LLOW;
		tid_fft   = osCreateTask("fft", FFT_Task, NULL, &taskParams);
		if(tid_fft == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(FFT)\r\n");
		}
		
		// Wave
		taskParams.priority = OS_TASK_PRIORITY_HHIGH;
		tid_wave[0] = osCreateTask("wave", Wave_Task, NULL, &taskParams);
		if(tid_wave[0] == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(Wave)\r\n");
		}
		
		// RMSlog
		taskParams.priority = OS_TASK_PRIORITY_HIGH;
		tid_rmslog    = osCreateTask("rmslog", RMSLog_Task, NULL, &taskParams);
		if(tid_rmslog == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(Wave)\r\n");
		}
			
		// PostScan
		taskParams.priority = OS_TASK_PRIORITY_HIGH;
		tid_post    = osCreateTask("post", PostScan_Task, NULL, &taskParams);
		if(tid_post == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(PostScan)\r\n");
		}
		
		// Energy
		taskParams.priority = OS_TASK_PRIORITY_HIGH;
		tid_energy    = osCreateTask("energy", Energy_Task, NULL, &taskParams);
		if(tid_energy == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(energy)\r\n");
		}
				
		// Meter
		taskParams.priority = OS_TASK_PRIORITY_REALTIME;
		taskParams.stackSize = 256;
		tid_meter[0] = osCreateTask("meter", Meter0_Task, NULL, &taskParams);
		if(tid_meter[0] == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(Meter)!\r\n");
		}
		if(getHwCh()== 0) {
			taskParams.priority = OS_TASK_PRIORITY_REALTIME;
			taskParams.stackSize = 256;
			tid_meter[1] = osCreateTask("meter1", Meter1_Task, NULL, &taskParams);
			if(tid_meter[1] == OS_INVALID_TASK_ID)
			{
				TRACE_ERROR("Failed to create task(Meter)!\r\n");
			}
		}

		// FS
		taskParams.priority = OS_TASK_PRIORITY_LOW;
		taskParams.stackSize = 256;
		tid_fs = osCreateTask("FS", FS_task, NULL, &taskParams);
		if(tid_fs == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(FS)!\r\n");
		}
		
		// Trend
		taskParams.priority = OS_TASK_PRIORITY_LOW;
		tid_trend = osCreateTask("trend", Trend_Task, NULL, &taskParams);
		if(tid_trend == OS_INVALID_TASK_ID)
		{
			TRACE_ERROR("Failed to create task(Trend)!\r\n");
		}
   	}

   // IOM
//    taskParams.priority = OS_TASK_PRIORITY_LOW;
//    tid_iom = osCreateTask("iom", IOM_Task, NULL, &taskParams);
//    if(tid_iom == OS_INVALID_TASK_ID)
//    {
//       //Debug message
//       TRACE_ERROR("Failed to create task(iom)!\r\n");
//    }
//    pdb->comm.comMode = 1;

//	pdb->comm.gwEable = 0x03;	// gems3600 #1,2 enable	
//  pdb->comm.RS485MasterMode = 2;
 
//	pdb->comm.gwEable = 0x07;	// gems3600 #1,2, DI #1 enable	
// 	pdb->comm.RS485MasterMode = 2;
   	// SMB rtu
   	// if(pdb->comm.comMode == 0){

	// 	taskParams.priority = OS_TASK_PRIORITY_LOW;
	// 	tid_rtu = osCreateTask("rtu", SMB_rtu_Task, NULL, &taskParams);
	// 	if(tid_rtu == OS_INVALID_TASK_ID)
	// 	{
	// 		//Debug message
	// 		TRACE_ERROR("Failed to create task(rtu)!\r\n");
	// 	}
   	// }
	// else {
	// 	// tid_mmb = osCreateTask("mmb", Gateway_Task, NULL, &taskParams);
	// 	// if(tid_mmb == OS_INVALID_TASK_ID)
	// 	// {
	// 	// 	//Debug message
	// 	// 	TRACE_ERROR("Failed to create task(rtu)!\r\n");
	// 	// }
	// }

	// 25-11-11 jskim test
	// pdb->comm.RS485MasterMode = RS485_MASTER_IMCS;
	// pdb->comm.MCS_count = 2;
	// pdb->comm.baud = 0; // 9600

  	// MMB rtu
//	taskParams.priority = OS_TASK_PRIORITY_LOW;
	// taskParams.priority = OS_TASK_PRIORITY_NORMAL;
	// if(pdb->comm.RS485MasterMode == RS485_MASTER_IPSM){
	// 	tid_mmb = osCreateTask("mmb", Gateway_Task, NULL, &taskParams);
	// 	if(tid_mmb == OS_INVALID_TASK_ID)
	// 	{
	// 		//Debug message
	// 		TRACE_ERROR("Failed to create task(rtu)!\r\n");
	// 	}
	// }
	// else {
	// 	pInfo->iPSM_sts[0] = 0;
	// 	pInfo->iPSM_sts[1] = 0;
	// 	pInfo->iPSMDI_sts[0] = 0;
	// 	pInfo->iPSMDI_sts[1] = 0;
	// 	if(pdb->comm.RS485MasterMode == RS485_MASTER_IMCS){
	// 		tid_mmb = osCreateTask("mmb", Gateway_MCS_Task, NULL, &taskParams);
	// 		if(tid_mmb == OS_INVALID_TASK_ID)
	// 		{
	// 			//Debug message
	// 			TRACE_ERROR("Failed to create task(rtu)!\r\n");
	// 		}
	// 	}
	// }

   // Wiznet
   taskParams.priority = OS_TASK_PRIORITY_NORMAL;
   tid_w5500 = osCreateTask("w5500", W5500_TcpServer, NULL, &taskParams);
   if(tid_w5500 == OS_INVALID_TASK_ID)
   {
      //Debug message
      TRACE_ERROR("Failed to create task!\r\n");
   }
   pcntl->wdtEn = 0;
#ifdef	_WDT_EN
   wdtEnable();
#endif	// _WDT_EN	

   //Create a task to blink the LED
#if 1	// 2025-3-11, taskMonitor의 우선순위 높여 wdt clear 한다
	taskParams.priority = OS_TASK_PRIORITY_HHIGH;
#else	
   taskParams.priority = OS_TASK_PRIORITY_NORMAL;	
#endif	
   tid_led = osCreateTask("LED", ledTask, NULL, &taskParams);
   //Failed to create the task?
   if(tid_led == OS_INVALID_TASK_ID)
   {
      //Debug message
      TRACE_ERROR("Failed to create task(LED)!\r\n");
   }
	//
	//Create a task(IpScan)
   taskParams.priority = OS_TASK_PRIORITY_NORMAL;
   tid_ipScan = osCreateTask("IpScan", IpScanListener, NULL, &taskParams);
   //Failed to create the task?
   if(tid_ipScan == OS_INVALID_TASK_ID)
   {
      //Debug message
      TRACE_ERROR("Failed to create task(IpScanListener)!\r\n");
   }
	
	//Create a task(Proxy)
   taskParams.priority = OS_TASK_PRIORITY_NORMAL;
   tid_proxy = osCreateTask("Proxy", ProxyTask, NULL, &taskParams);
   //Failed to create the task?
   if(tid_proxy == OS_INVALID_TASK_ID)
   {
      //Debug message
      TRACE_ERROR("Failed to create task(IpScanListener)!\r\n");
   }
}


void Shell_Task(void *arg) 
{
	int	lastSec=0;

	printf("[task shell started ...\n");

	_enableTaskMonitor(Tid_Shell, 50);
	
	while (1) {
		pcntl->wdtTbl[Tid_Shell].count++;
		shell();
		// 5sec 
		// if(lastSec != pcntl->tod.tm_sec) {
		// 	lastSec = pcntl->tod.tm_sec;
		// 	if(lastSec %5 == 0)
		// 		checkiPSM();
		// }

		osDelayTask(50);
	}
}

/**
 * @brief LED task
 * @param[in] param Unused parameter
 **/

 void ledTask(void *param)
 {
	int i=0, j=0, val;
	uint32_t ts1s = sysTick1s;

	_enableTaskMonitor(Tid_Led, 50);  
	//Endless loop     
	while(1)
	{
	   pcntl->wdtTbl[Tid_Led].count++;
//	   pInfo->HeartBit++;
	   taskMonitor();
	   if (++i >= 10) {
	   		pInfo->HeartBit++;
		  	Board_LED_Toggle(0);
		  	i = 0;
	   }

//		STP_PORT_STATE_DISABLED   = 0(D, x)
//		STP_PORT_STATE_BROKEN     = 1(D, x)
//		STP_PORT_STATE_BLOCKING   = 2(D, x)
//		STP_PORT_STATE_LISTENING  = 3(D, x)
//		STP_PORT_STATE_LEARNING   = 4(L, x)
//		STP_PORT_STATE_FORWARDING = 5(F, o)

	   if(ts1s != sysTick1s) {
			ts1s = sysTick1s;
			for(j=1; j<3; j++) {
				val = getRstpState(j);
				if(val == 1)
					pInfo->RSTP_sts[j-1] = STS_ERROR;
				else
					pInfo->RSTP_sts[j-1] = STS_OK;

//				val = getRstpState(j);
//				printf("RSTP Status[%d] = %d\n", j-1, pInfo->RSTP_sts[j-1]);
//				printf("RSTP Status[%d] = %d\n", j, val);
			}

			if(pInfo->RSTP_sts[0] ==STS_ERROR || pInfo->RSTP_sts[1] ==STS_ERROR)
				pInfo->NET_sts = STS_ERROR;
			else
				pInfo->NET_sts = STS_OK;

			// if(ts1s % 3 == 0)
			// 	checkIO();	
	   }
	   osDelayTask(100);    
	}
}
 
OsTaskId getW5500TaskId() {
	return tid_w5500;
}

OsTaskId getProxyTaskId() {
	return tid_proxy;
}

void tickHandler() {	
	WM_tick32++;
	sysTick32++;
	sysTick64++;
	
#ifdef BUZZER	
	Buzzer_Toggle();
#endif	
	if (++tick >= 1000) {
//		Board_LED_Toggle(0);
		sysTick1s++;
		sysTick10s = sysTick1s/10;	// 10 sec, freq sampling 
#ifdef _QUAL_TEST
		sysTick10m = sysTick1s/60;	// 1 minute, volt. averaging
#else		
		sysTick10m = sysTick1s/600;	// 10 minute, volt. averaging
#endif		
		sysTick15m = sysTick1s/900;	// 15 minute, demand
		if (pcntl->dInterval > 0) {
			sysTickDemand = sysTick1s/(pcntl->dInterval);
		}
		tick = 0;
		
		uLocalTime(&sysTick1s, &pcntl->tod);
		//pcntl->tod.tm_year += 1900;
		//pcntl->tod.tm_mon  += 1;
	}	

	RTS_Control();
}


#endif

