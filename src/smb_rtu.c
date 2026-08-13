#include "os.h"
#include "board.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include "meter.h"
#include "modbus.h"
#include "crc.h"
#include "ade9000.h"

void smb(int uId, int devId);
void ext_smb(int uId, int devId);

uint8_t rxbuf[300], txbuf[256];

// modbus, hmi에서 사용하는 설정영역
//ALARM_SET dbalm;
//EXT_MOD_CFG dbExt;

//extern uint16_t	cmdQ[];
extern CMD_Q cmdQ;
void cmdProc(void);

extern int checkFwDnld(void);
extern int checkFwApply(void);
extern void writeFwFile(void);
extern void applyFwFile(void);
//extern void usrLocalTime_r(const uint32_t *t, struct tm *ptm);
extern void initSettings();
extern int getCmdQ(int *s, int *c);
extern void setDcOffset(int id);
extern void clrDcOffset(int id);
extern void selWire(int mode);
extern void negateAlarm();
extern void negateEvent();

const int baudlist[]={9600, 19200, 38400, 57600, 115200};

int getBaudrate(int ix)
{
	if (ix >= 0 && ix < 5) {
		return baudlist[ix];
	}
	else {
		return baudlist[0];
	}
}

#ifdef __FREERTOS
void SMB_rtu_Task(void)
#else
void SMB_rtu_Task(void *arg)
#endif
{
	int uId=1;	// USART1
	
	// UART Init, RS485 port (baud 설정 제거됨 → 115200 고정)
	UART_Init(uId, getBaudrate(4));

	printf("[smb(RS485) meterTask, devId=%d, speed=%d]\n",
		pdb->comm.devId, getBaudrate(4));
	
	makeCRC32table();
	
	_enableTaskMonitor(Tid_SMB, 50);
		
	while(1) {
		pcntl->wdtTbl[Tid_SMB].count++;
		
		// 통신설정 변경은 재부팅을 통해서만 가능하다.
//		if (pcntl->commCmd[uId]) {
//			printf("new baud=%d, parity=%d\n", getBaudrate(pdb->baud), pdb->parity);
//			UARTReInit(uId, getBaudrate(pdb->baud), pdb->parity);
//			pcntl->commCmd[uId] = 0;
//		}
		smb(uId, pdb->comm.devId);
		
//		if (checkFwDnld()) {
//			writeFwFile();
//		}
//		
//		if (checkFwApply()) {
//			applyFwFile();
//		}
		//cmdProc();
		os_dly_wait(10);	
		//incTaskCount(TASK_SMB);
	}
	
//	while (1) {
//		for (ix=0, i='A'; i<='z'; i++) {
//			txbuf[ix++] = i;
//		}
//		
//		//LPC_GPIO1->SET = (1<<30);		
//		UARTSend(3, txbuf, ix);
//		//LPC_GPIO1->CLR = (1<<30);
//		
//		os_dly_wait(1000);
//	}
}

void SMB_rtu_Task2(void *arg)
{
	int uId=2;
	
	// UART Init, RS485 port (baud 설정 제거됨 → 115200 고정)
	UART_Init(uId, getBaudrate(4));

	printf("[Tid_GW(RS485) meterTask, uId=%d, speed=%d]\n",
		uId, getBaudrate(4));
	
	makeCRC32table();
	
	_enableTaskMonitor(Tid_GW, 50);
		
	while(1) {
		pcntl->wdtTbl[Tid_GW].count++;
		
		// 통신설정 변경은 재부팅을 통해서만 가능하다.
//		if (pcntl->commCmd[uId]) {
//			printf("new baud=%d, parity=%d\n", getBaudrate(pdb->baud), pdb->parity);
//			UARTReInit(uId, getBaudrate(pdb->baud), pdb->parity);
//			pcntl->commCmd[uId] = 0;
//		}
		smb(uId, pdb->comm.devId);
		
//		if (checkFwDnld()) {
//			writeFwFile();
//		}
//		
//		if (checkFwApply()) {
//			applyFwFile();
//		}
		//cmdProc();
		os_dly_wait(10);	
		//incTaskCount(TASK_SMB);
	}
	
//	while (1) {
//		for (ix=0, i='A'; i<='z'; i++) {
//			txbuf[ix++] = i;
//		}
//		
//		//LPC_GPIO1->SET = (1<<30);		
//		UARTSend(3, txbuf, ix);
//		//LPC_GPIO1->CLR = (1<<30);
//		
//		os_dly_wait(1000);
//	}
}


// 2017-8-16, device ID 1로 응답하는 버그수정
void smb(int uId, int devId)
{
	static int st, ix, len, timer;	
	static uint8_t buf[256];
	
	uint8_t inchar;
	int		i, nr, nw;
	uint16_t crc;
	
	nr = UARTReceive(uId, buf, sizeof(buf));	
	
	//if (nr > 0) printf("nr=%d\n", nr);
		
	for (i=0; i<nr; i++) {
		inchar = buf[i];
		
		if(st == 0) {
			if(inchar == devId) {
				ix = 0;
				rxbuf[ix++] = inchar;
				timer = 5;
				st = 1;
			}
		}
		else if(st == 1) {
			if (inchar >= 1 && inchar <= 6) {     
				rxbuf[ix++] = inchar;
				len = 8;
				st = 3; 				
			}                                      
			else if (inchar == 15 || inchar == 16) {
				rxbuf[ix++] = inchar;
				st = 2; 				   
			}
			else {
				printf("smb485: bad fc(%d)...\n\r", inchar);
				st = 0;
				timer = 0;
			}			
		}
		else if(st == 2) {
			rxbuf[ix++] = inchar;
			if (ix == 7) {                   
				len = inchar+9;	// id+FC+s(2)+c(2)+crc(2)=8
				timer = 20;
				//printf("smb485 : multiple frame, length=%d, timer=%d\n\r", len, timer);
				st = 3;
			}
		}
		else if(st == 3) {
			rxbuf[ix++] = inchar;
			if (ix >= len) {	
//				for (i=0; i<len; i++) {
//					printf("%02x ", rxbuf[i]);
//				}
//				printf("\n");
			
				if (gencrc_modbus(rxbuf, len) != 0) {                
					printf("smb485: bad crc\n"); 
				}		  
				else {		               										
					// 2017-8-16, device ID를 항상 1로 응답하는 문제 해결위해 modbusSlvProcFrame()를 수정
					nw = modbusSlvProcFrame(rxbuf, len, txbuf, 0);
					if (nw > 0) {						
						txbuf[0] = rxbuf[0];
						crc = gencrc_modbus(txbuf, nw);						
						txbuf[nw++] = crc;	// crc
						txbuf[nw++] = crc >> 8;			

						// 2017-8-16, 충돌 막기위해 delay 추가
						os_dly_wait(1);
						UARTSend(uId, txbuf, nw);
					}
				}			    
				timer = st = 0;

				//pos485 = 0;			
			}
		}
	}

	if (st > 0) {
		if (--timer == 0) {			
			printf("smb485 frameing timeout, st=%d, ix=%d ...\n", st, ix);
			st = 0;
		}
	}
}

// smb에서 호출된다 
// 간접제어방식을 사용 (특정번지에 제어하려는 Module/point/Command를 쓴다)
void sendDOControl(uint16_t *pcmd)
{
#if 0	
	// Alarm Output 채널인지 검사한다 
	//if ((pcntl->almMod-1) == pcntl->cmId && (pcntl->almPoint-1) == (pcntl->cpoint-24)) {
	if ((pcntl->almMod-1) == pcmd[0] && (pcntl->almPoint-1) == pcmd[1]) {
		printf("!!! Not relay point, Alarm Channel(%d-%d)\n", pcntl->cmId, pcntl->cpoint); 
	}
	// 2017-11-22(event point 추가)
	else if ((pcntl->evtMod-1) == pcmd[0] && (pcntl->evtPoint-1) == pcmd[1]) {
		printf("!!! Not relay point, Event Channel(%d-%d)\n", pcntl->cmId, pcntl->cpoint); 
	}	
	else {
		pcntl->cmId = pcmd[0];
		pcntl->cpoint = pcmd[1];
		pcntl->ccmd = pcmd[2];
		pcntl->ctrlFlag = 1;
		//addEvent(EVENT_DO, NULL, pcntl->cmId, pcntl->cpoint, pcntl->ccmd);
	}
#endif		
}

void sendExtIOMControl(int id, int s, int cmd)
{
	// int	cnt, i, v;

	// cnt = pcntl->ctrlFlag;
	// if(piocfg->doType[s]==DOTYPE_OUT){
	// 	pcntl->cmId = id;
	// 	pcntl->cpoint[cnt] = s;
	// 	pcntl->ccmd[cnt] = cmd;
	// 	pcntl->ctrlFlag++;
	// 	printf("[%d]sendExtIOMControl(%d-%d-0x%x)\n", cnt, pcntl->cmId, pcntl->cpoint[cnt], pcntl->ccmd[cnt]); 
	// }
	// else
	// 	printf("!!! Not relay point, Channel(%d)=%d\n", s, piocfg->doType[s]); 
}

//void writeBooLog(int arg)
//{
//	FILE *fp;

//	printf("[BOOT_LOG, arg=%d]\n", arg);
//	fp = fopen("bootlog.bin", "wb");
//	fwrite(&arg, sizeof(arg), 1, fp);
//	fclose(fp);
//}

// meterTask에서 주기적으로 호출된다. 수행시간 오래 걸리는 작업 호출시 MBX_OVF 발생한다.

//void reboot(int c) {
//	printf("[[reboot ...]]\n");
//}



void calibration(int id, int code) {
	CNTL_DATA *pc = &meter[id].cntl;

	if (id < 0 || id >= METER_CH_COUNT)
		return;

	switch (code) {
		case 0:
			pc->calEn = 1;
			break;
		case 1: 
			if (pc->calEn){
				setGainU(id, pcntl->vref);
			}
			break;
		case 2:
			if (pc->calEn){
				setGainI(id, pcntl->iref);
			}
			break;
		case 3:
			if (pc->calEn){
				setGainW(id, pcntl->vref, pcntl->iref);
			}
			break;		
		case 4:
			if (pc->calEn){
				setGainPh(id);
			}
			break;		
		case 5: 
			if (pc->calEn) {
				clrGainU(id);
			}
			break;
		case 6:
			if (pc->calEn) {
				clrGainI(id);
			}
			break;
		case 7:
			if (pc->calEn) {
				clrGainW(id);
			}
			break;		
		case 8:
			if (pc->calEn) {
				clrGainPh(id);
			}
			break;				
		case 9:
			if (pc->calEn) {
				setGainIn(id, pcntl->inref);
			}
			break;
		case 10:
			if (pc->calEn) {
				clrGainIn(id);
			}
			break;
		case 11: 
			pc->calEn = 0;
			break;
		case 12: 
			storeHwSettings(pcal);	
			break;
		case 13:
			if (pc->calEn) {
				setGainUpp(id, pcntl->vref*SQRT_3);
			}
			break;
		case 14:
			if (pc->calEn) {
				clrGainUpp(id);
			}
			break;
		case 15: 
			if (pc->calEn) {
				setDcOffset(id);
			}
			break;
		case 16:
			if (pc->calEn) {
				clrDcOffset(id);
			}
			break;
		case 17:	// Temp Cali — 측정온도를 기준온도(Temp-ref@7463, 미설정 시 TEMP_CAL_REF)에 맞추는 오프셋(idempotent)
			if (pc->calEn) {
				float tref = (pcntl->tref > 0) ? pcntl->tref : TEMP_CAL_REF;
				pcal->tempOfs[id] = tref - (meter[id].meter.Temp - pcal->tempOfs[id]);
			}
			break;
		case 18:	// Temp Init — 오프셋 클리어
			if (pc->calEn) {
				pcal->tempOfs[id] = 0;
			}
			break;
	}
}

static void dispatchMeterChCmd(int base, int addr, int cmd, void (*fn)(int, int))
{
	int ch;

	if (addr == base) {
		for (ch = 0; ch < METER_CH_COUNT; ch++)
			fn(ch, cmd);
		return;
	}
	ch = addr - base - 1;	/* base+1~3 → CH0~2 */
	if (ch >= 0 && ch < METER_CH_COUNT)
		fn(ch, cmd);
}

extern void resetDemand(int);

void clearDemand(int id, int cmd) {
	printf("[Clear Demand M%d]\n", id);
	meter[id].cntl.rstDemand = cmd;
}

void clearMinMax(int id, int cmd) {
	printf("[Clear MaxMin M%d]\n", id);
	meter[id].cntl.rstMaxMin = cmd;
}

void clearEnergy(int id, int cmd) {
	printf("[Clear Energy M%d]\n", id);
	meter[id].cntl.rstEgy = cmd;
}

void clearAlarm(int id, int cmd) {
	printf("[Clear Alarm M%d]\n", id);
	meter[id].cntl.rstAlmList = cmd;
}

void clearEventListCmd(int id, int cmd) {
	printf("[Clear Event M%d]\n", id);
	meter[id].cntl.rstEvtList = cmd;
}

void clearIticList(int id, int cmd) {
	printf("[Clear ITIC list M%d]\n", id);
	meter[id].cntl.rstIticList = cmd;
}

/* METER_INFO(Alarm_sts/Event_sts)는 CH0 pInfo 공통 — +0~+3 주소 모두 동일 ACK */
static void ackAlarmCmd(int cmd) {
	(void)cmd;
	printf("[Ack alarm — METER_INFO common]\n");
	negateAlarm();
}

static void ackEventCmd(int cmd) {
	(void)cmd;
	printf("[Ack event — METER_INFO common]\n");
	negateEvent();
}

// smb, GUI로 부터 호출된다 
void reqFactoryReset(int cmd) {
	printf("[run FactoryReset]\n");
	pcntl->factReset = cmd;
}

void reqReboot(int cmd) {
	printf("[go reboot]\n");
	pcntl->rebootFlag = cmd;
}

void reqSaveSettings(int cmd) {
	printf("[save settings]\n");
	pcntl->saveSetting = cmd;
}

/* Modbus 명령 큐 전용 — PostScan과 분리(캘리브레이션·clear 등 장시간 처리) */
void CmdProc_Task(void *arg)
{
#ifdef __FREERTOS
	initCmdQ(xTaskGetCurrentTaskHandle());
#else
	initCmdQ(os_tsk_self());
#endif
	_enableTaskMonitor(Tid_CmdProc, 50);

	while (pcntl->runFlag) {
		/* 명령 유무와 무관하게 매 주기 워치독 kick.
		 * idle(명령 대기) 중인 정상 태스크를 멈춤으로 오판하지 않도록,
		 * 무한 대기 대신 유한 타임아웃으로 깨어나 kick 한다 (SMB 태스크와 동일 패턴). */
		meter[0].cntl.wdtTbl[Tid_CmdProc].count++;
#ifdef __FREERTOS
		uint32_t nv;
		if (xTaskNotifyWait(0, 0x08, &nv, pdMS_TO_TICKS(100)) != pdPASS)
			continue;	/* 타임아웃(명령 없음) → 루프 반복하며 다시 kick */
#else
		if (os_evt_wait_and(0x8, 100) == OS_R_TMO)
			continue;	/* 타임아웃(명령 없음) → 루프 반복하며 다시 kick */
#endif
		do {
			cmdProc();
		} while (cmdQ.fr != cmdQ.re);
	}

#ifdef __FREERTOS
	vTaskSuspend(NULL);
#else
	os_evt_wait_and(0xffff, 0xffff);
#endif
}

void cmdProc()
{
	int s, c, addr;
	
	if (getCmdQ(&s, &c) < 0) 
		return;

	printf("cmdProc, s=%d, c=%x\n", s, c);

	addr = s - MBAD_SET_CMD;

	printf("{{cmdProc(%d, 0x%x)}}\n", s, c);
	switch (addr) {
		// reboot
		case 0:
			// iom reset send
//			sendIOCommand(IOM_COMMAND, 0x1234);
//			os_dly_wait(1000);	
			reqReboot(c);
			break;	
		// save db
		case 1:		
			//reqSaveSettings(c, pdbk);
			reqSaveSettings(c);
			break;		
		// calibration: base+0=all, base+1~3=CH0~2
		case 2:
		case 3:
		case 4:
		case 5:
			dispatchMeterChCmd(2, addr, c, calibration);
			break;
		// set V-ref / I-ref / In-ref (공통 pcntl, CH1~CH3 동일)
		case 12:
			printf("[Set V-ref]\n");
			pcntl->vref = c;
			break;
		case 13:
			printf("[Set I-ref]\n");
			pcntl->iref = c/10.;
			break;
		case 14:
			printf("[Set In-ref]\n");
			pcntl->inref = c/10.;
			break;
		case 15:	// 260812 신설: Temp-ref (7463), 1=1℃
			printf("[Set Temp-ref]\n");
			pcntl->tref = c;
			break;
		case 16:
			printf("[INIT SETTINGS]\n");
			reqFactoryReset(c);
			break;
		case 17:
			printf("[CLEAR PI]\n");
			{ int k; for (k = 0; k < 4; k++) meter[0].iom.piData[k] = 0; }	/* PI 펄스 적산 초기화 */
			break;
		/* 260812 맵: Temp-ref(7463) 삽입으로 이후 +1 시프트. load event/alarm(38~45)은 handleFetchCmd 동기처리 */
		// clear demand: base+0=all, base+1~3=CH0~2  (7466~7469)
		case 18:
		case 19:
		case 20:
		case 21:
			dispatchMeterChCmd(18, addr, c, clearDemand);
			break;
		// clear minmax (7470~7473)
		case 22:
		case 23:
		case 24:
		case 25:
			dispatchMeterChCmd(22, addr, c, clearMinMax);
			break;
		// clear energy (7474~7477)
		case 26:
		case 27:
		case 28:
		case 29:
			dispatchMeterChCmd(26, addr, c, clearEnergy);
			break;
		// clear alarm (7478~7481)
		case 30:
		case 31:
		case 32:
		case 33:
			dispatchMeterChCmd(30, addr, c, clearAlarm);
			break;
		// clear event (7482~7485)
		case 34:
		case 35:
		case 36:
		case 37:
			dispatchMeterChCmd(34, addr, c, clearEventListCmd);
			break;
		// alarm ack (7494~7497, +0~+3 → 공통 METER_INFO)
		case 46:
		case 47:
		case 48:
		case 49:
			ackAlarmCmd(c);
			break;
		// event ack (7498~7501, +0~+3 → 공통 METER_INFO)
		case 50:
		case 51:
		case 52:
		case 53:
			ackEventCmd(c);
			break;
		// clear ITIC,ITIC2 (7504, 단일 레지스터 → CH1, itic·itic2 동시)  [260812: 55→56]
		case 56:
			clearIticList(0, c);
			break;
	}
}



//void putAlmSettings(int addr, int count, uint16_t *pdata) 
//{
//	uint16_t  *pbuf = (uint16_t *)&dbalm;
//	memcpy(&pbuf[addr], pdata, count*2);
//}
