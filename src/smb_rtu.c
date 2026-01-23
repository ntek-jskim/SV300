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
extern int getCmdQ(int *id, int *s, int *c);
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
	
	// UART Init, RS485 port
	UART_Init(uId, getBaudrate(pdb->comm.baud));

	printf("[smb(RS485) meterTask, devId=%d, speed=%d, parity=%d]\n", 
		pdb->comm.devId, getBaudrate(pdb->comm.baud), pdb->comm.parity);
	
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
	
	// UART Init, RS485 port
	UART_Init(uId, getBaudrate(pdb->comm.baud));

	printf("[Tid_GW(RS485) meterTask, uId=%d, speed=%d, parity=%d]\n", 
		uId, getBaudrate(pdb->comm.baud), pdb->comm.parity);
	
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



void calibration(int code) {
	switch (code) {
		case 0:
			pcntl->calEn = 1;
			break;
		case 1: 
			if (pcntl->calEn){
				setGainU(0, pcntl->vref);
				setGainU(1, pcntl->vref);
			}
			break;
		case 2:
			if (pcntl->calEn){
				setGainI(0, pcntl->iref);
				setGainI(1, pcntl->iref);
			}
			break;
		case 3:
			if (pcntl->calEn){
				setGainW(0, pcntl->vref, pcntl->iref);
				setGainW(1, pcntl->vref, pcntl->iref);
			}
			break;		
		case 4:
			if (pcntl->calEn){
				setGainPh(0);		// Watt와 Var의 비율로 위상오차 보정한다 
				setGainPh(1);
			}
			break;		
		case 5: 
			if (pcntl->calEn) {
				clrGainU(0);
				clrGainU(1);
			}
			break;
		case 6:
			if (pcntl->calEn) {
				clrGainI(0);
				clrGainI(1);
			}
			break;
		case 7:
			if (pcntl->calEn) {
				clrGainW(0);
				clrGainW(1);
			}
			break;		
		case 8:
			if (pcntl->calEn) {
				clrGainPh(0);
				clrGainPh(1);
			}
			break;				
		case 9:
			if (pcntl->calEn) {
				setGainIn(0, pcntl->inref);	// 2020-3-13 
				setGainIn(1, pcntl->inref);
			}
			break;
		case 10:
			if (pcntl->calEn) {
				clrGainIn(0);
				clrGainIn(1);
			}
			break;
		case 11: 
			pcntl->calEn = 0;
			break;
		case 12: 
			{
//			FS_MSG fsmsg;
//			strcpy(fsmsg.fname, "HwSetting.bin");
//			strcpy(fsmsg.mode, "wb");
//			fsmsg.pbuf = pcal;
//			fsmsg.size = sizeof(*pcal);
//			putFsQ(&fsmsg);		
			storeHwSettings(pcal);	
			}
			break;
		case 13:
			if (pcntl->calEn) {
				setGainUpp(0, pcntl->vref*SQRT_3);
				setGainUpp(1, pcntl->vref*SQRT_3);
			}
			break;
		case 14:
			if (pcntl->calEn) {
				clrGainUpp(0);
				clrGainUpp(1);
			}
			break;
		case 15: 
			if (pcntl->calEn) {
				setDcOffset(0);
				setDcOffset(1);
			}
			break;
		case 16:
			if (pcntl->calEn) {
				clrDcOffset(0);
				clrDcOffset(1);
			}
			break;
		case 17:	
			//pdb->pt.wiring = WM_3LN3CT;
			//selWire(0);	// 3P4W
			break;
		case 18:
			//pdb->pt.wiring = WM_3LL3CT;
			//selWire(1);	// 3P3W
			break;		
	}
}

extern void resetDemand(int);

void clearDemand(int cmd) {
	printf("[Clear Demand]\n");
	//resetDemand(0);
	pcntl->rstDemand = cmd;
}

void clearMinMax(int cmd) {
	printf("[Clear MaxMin]\n");
	pcntl->rstMaxMin = cmd;
}

void clearEnergy(int cmd) {
	printf("[Clear Energy]\n");
	meter[0].cntl.rstEgy = cmd;
	meter[1].cntl.rstEgy = cmd;
}

void clearAlarm(int cmd) {
	printf("[Clear Alarm]\n");
	pcntl->rstAlmList = cmd;
}

void clearEvent(int cmd) {
	printf("[Clear Event]\n");
	pcntl->rstEvtList = cmd;
}

void clearIticList(int cmd) {
	printf("[Clear ITIC list]\n");
	pcntl->rstIticList = cmd;
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

void cmdProc()
{
	int id, s, c, addr, i;
	
	if (getCmdQ(&id, &s, &c) < 0) 
		return;

	id = (s < 10000) ? 0 : 1;
	printf("cmdProc, s=%d, c=%x\n", s, c);
			
	// IO module #1
	if (s >= IOM_DO_BASE0 && s < (IOM_DO_BASE0+8)) {
//		sendExtIOMControl(0, s-IOM_DO_BASE0, c);
	}
	// IO module #2
	// else if (s >= IOM_DO_BASE1 && s <= (IOM_DO_BASE1+8)) {		
	// 	sendExtIOMControl(1, s-IOM_DO_BASE1, c);
	// }
	else {		
		addr = s-MBAD_G7_CMD;	// 6950	
		
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
		// calibration
		case 2:		
			calibration(c);
			break;
		// set vref
		case 3:		
			printf("[Set V-ref]\n");
			pcntl->vref = c;
			break;
		// set iref
		case 4:
			printf("[Set I-ref]\n");
			pcntl->iref = c/10.;
			break;
		// clear demand
		case 5:		
			clearDemand(c);
			break;
		// clear minmax
		case 6:		
			clearMinMax(c);			
			break;
		// clear energy
		case 7:		
			clearEnergy(c);
			break;		
		// clear alarm
		case 8:		
			clearAlarm(c);
			break;		
		// init settings
		case 9:		
			reqFactoryReset(c);
			break;	
		// clear event
		case 10:
			clearEvent(c);		
			break;
		// set Inref
		case 11:
			printf("[Set In ref]\n");
			pcntl->inref = c/10.;
			break;
		// reset alarm ack
		case 12:
			printf("[Ack alarm]\n");
			negateAlarm();
//			putCmdQ();
			break;
//		case 29:
//			writeBooLog(cmdQ.cmdbuf[0]);
//			break;			
		// DAQ enable/disable
		case 13:
			pcntl->daqEnable = c;
			break;
		case 14:
			clearIticList(c);
			break;
		// reset event ack
		case 15:
			printf("[Ack event]\n");
			negateEvent();
//			putCmdQ();
			break;
		// clear pi	
		// case 16:
		// 	// iom pi clear send
		// 	sendIOCommand(IOM_COMMAND+1, 0x1234);
		// 	break;
		// }
		}
	}
}



//void putAlmSettings(int addr, int count, uint16_t *pdata) 
//{
//	uint16_t  *pbuf = (uint16_t *)&dbalm;
//	memcpy(&pbuf[addr], pdata, count*2);
//}
