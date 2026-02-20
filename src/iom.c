#include "RTL.h"
#include "os_port.h"
#include "board.h"
#include "crc.h"
#include "meter.h"
#include "modbus.h"
#include "temp_sensor.h"
#include "stdio.h"
#include "string.h"
#include "math.h"


// #define	INT_IO_BLINK	0x01

// //2018-6-14
// typedef enum
// {
// 	IOM_NUMM = 0,
// 	IOM_DIO_1, 
// 	IOM_DIO_2,	
// 	IOM_RTD_1,	
// 	IOM_RTD_2,	
// 	IOM_AIO_1,	
// 	IOM_AIO_2,		
// 	IOM_DCM_1,	
// 	IOM_DCM_2,	
// 	IOM_ETH = 10
// } IO_MODULE_TYPE;

// IOM_MEM_DEF ios;

// extern EXT_IO_DATA	*piom;
// //extern IO_CFG *pcfg;
// extern void getTimeStamp(uint16_t *dst);
// //extern void addEvent(int evtChan, uint16_t *ts, int mod, int point, int cmd);
// extern uint32_t getTickCount();

// int sndRcvMmbFrame(int pid, uint8_t *txb, int nw, uint8_t *rxb, int *nr);

// //void installIOM(uint16_t *ioType);

// int modType[3];
// uint8_t iotb[260], iorb[260];

// int iomdebug;

// // 2018-6-14
// char *iomtype[] = {"NULL", "DIO#0", "DIO#1", "RTD#0", "RTD#1", "AIO#0", "AIO#1", "DCM#1", "DCM#2"};

// extern int ntDebugLevel;


// 2018-9-3 수정: DI8로 고정
int getDiStatus(int mod, int point, char *pbuff)
{
	// if (piom->io.diData[point] == 0) {
	// 	strcpy(pbuff, "OFF");
	// }
	// else {
	// 	strcpy(pbuff, "ON");
	// }
	// return piom->io.diData[point];
}

// SubType0: DI8DO6
// SunType1: DI8DO2
int getDoStatus(int mod, int point, char *pbuff)
{
	// int rval;
	
	// if (piom->modSubType[mod+1] != 0 && point >= 2) {
	// 	strcpy(pbuff, "");
	// 	return 0;	
	// }
	// else {
	// 	if (piom->io.doData[point] == 0) {
	// 		strcpy(pbuff, "OFF");	// Open -> OFF
	// 	}
	// 	else {
	// 		strcpy(pbuff, "ON");	// Close -> ON
	// 	}
	// 	return piom->io.doData[point];
	// }	
}


// Range: 0 ~ 999,999.999 
int getPiCount(int mod, int point, char *pbuff)
{
//	int v1, v2;

//	if (piom->modSubType[mod+1] == 0 && point >= 5) {
//		strcpy(pbuff, "");
//		return 0;
//	} 
//	else {
//		v1 = piom->io[mod].piData[point]/1000;
//		v2 = piom->io[mod].piData[point]%1000;	
//		sprintf(pbuff, "%d.%d", v1, v2);	
//		return (piom->io[mod].piData[point]);
//	}
	// float v = piom->io.diData[point];
	// if ( v < 10) {
	// 	sprintf(pbuff, "%.3f", v);
	// }
	// else if (v < 100) {
	// 	sprintf(pbuff, "%.2f", v);
	// }
	// else if (v < 1000) {
	// 	sprintf(pbuff, "%.1f", v);
	// }
	// else {
	// 	sprintf(pbuff, "%.0f", v);
	// }
	
	// return piom->io.diData[point];
}

//float getAiValue(int mod, int point, char *pbuff)
//{
	// sprintf(pbuff, "%.1f", piom->io.aiData[point]);
	// return (piom->io.aiData[point]);
//}

//int iomInstalled(void)
//{
	// if (piom->modType[1] || piom->modType[2])
	// 	return 1;
	// else
	// 	return 0;
//}

// int makePollFrame(uint8_t *pb, uint8_t id, uint16_t s, uint16_t c)
// {
// 	uint16_t ix=0, crc;
	
// 	pb[ix++] = id;
// 	pb[ix++] = 3;
// 	pb[ix++] = s>>8;
// 	pb[ix++] = s;
// 	pb[ix++] = c>>8;
// 	pb[ix++] = c;
// 	crc = gencrc_modbus(pb, ix);
// 	pb[ix++] = crc;
// 	pb[ix++] = crc>>8;
	
// 	return ix; 
// }

// int makeControlFrame(uint8_t *pb, uint8_t id, uint16_t s, uint16_t c)
// {
// 	uint16_t ix=0, crc, i;
	
// 	pb[ix++] = id;
// 	pb[ix++] = 6;
// 	pb[ix++] = s>>8;
// 	pb[ix++] = s;
// 	pb[ix++] = c>>8;
// 	pb[ix++] = c;
// 	crc = gencrc_modbus(pb, ix);
// 	pb[ix++] = crc;
// 	pb[ix++] = crc>>8;

// 	return ix;
// }

// int makeSettingFrame(uint8_t *pb, uint8_t id, uint16_t s, uint16_t c, uint16_t *pcfg)
// {
// 	uint16_t ix=0, crc, i;
	
// 	pb[ix++] = id;
// 	pb[ix++] = 16;
// 	pb[ix++] = s>>8;
// 	pb[ix++] = s;
// 	pb[ix++] = c>>8;
// 	pb[ix++] = c;
// 	pb[ix++] = c*2;
// 	memcpy(&pb[ix], pcfg, c*2);
// 	ix += (c*2);
// 	crc = gencrc_modbus(pb, ix);
// 	pb[ix++] = crc;
// 	pb[ix++] = crc>>8;

// 	return ix;
// }

// int makeSOEDumpFrame(uint8_t *pb, uint8_t id, uint16_t nsoe)
// {
	// uint16_t s=SOE_START, c=nsoe*8, crc, ix=0;
	
	// // SOE영역의 시작 부터 count 까지 읽는다 
	// pb[ix++] = id;
	// pb[ix++] = 3;
	// pb[ix++] = s>>8;
	// pb[ix++] = s;
	// pb[ix++] = c>>8;
	// pb[ix++] = c;
	// crc = gencrc_modbus(pb, ix); 
	// pb[ix++] = crc;
	// pb[ix++] = crc>>8;

	// return ix;
//}


//int sndRcvMmbFrame(int pid, uint8_t *txb, int nw, uint8_t *rxb, int *nr)
//{
//	int	timer=10;
//	int	st=0, inx=0, length=0;
//	uint8_t inchar;
//	uint16_t	ccrc, rcrc;
//	uint32_t  et1, et2, gcnt=0;
//	
//	UARTSend(pid, txb, nw);
//	dprtbuffer(iomdebug, "IOT:", txb, nw);
//	
//	et1 = getTickCount();
//	while (1) {	
//		if (UARTReceive(pid, &inchar, 1) == 0) {
//			if (--timer == 0) {
//				et2 = getTickCount();
//				printf("sndRcvMmbFrame, timeout, id=%d, st=%d, inx=%d, elapT=%d, gcnt=%d\n", txb[0], st, inx, et2-et1, gcnt);
//				//dprtbuffer(1, "IOR:", rxb, inx);		
//				//os_dly_wait(500);
//				return -1;
//			}
//			else {
//				os_dly_wait(10);
//				continue;
//			}
//		}
//		
//		timer = 10;

//		if (st == 0) {			
//			if (inchar == txb[0]) {
//				inx = 0;
//				rxb[inx++] = inchar;
//				st = 1;
//			}
//			else {
//				gcnt++;
//			}
//		}
//		else if (st == 1) {		
//			if (inchar == txb[1]) {
//				rxb[inx++] = inchar;
//				st = 2;
//			}
//			else {
//				st = 0;
//			}
//		}
//		else if (st == 2) {
//			if ((txb[1] == 5) || (txb[1] == 6) || (txb[1] == 15) || (txb[1] == 16))
//				length = 8;
//			else 
//				length = inchar+5;
//			rxb[inx++] = inchar;
//			st = 3;
//		}
//		else if (st == 3) {
//			rxb[inx++] = inchar;
//			if (inx == length) {
//				if (gencrc_modbus(rxb, length) == 0) {					
//					*nr = inx;
//					dprtbuffer(iomdebug, "IOR:", rxb, inx);					
//					return 0;
//				}
//			}
//		}
//	}
//}

// int decodeEthBattVolt(uint8_t *psrc, int os) {
// 	int ix=os*2;
// 	return (psrc[ix] | psrc[ix+1]<<8);
// }

// int decodeMmbIO(int fc, int s, int c, uint8_t *psrc, uint16_t *pdst)
// {
// 	int i;
	
// 	switch (fc) {
// 	case 3:
// 	case 4:
// 		memcpy(pdst, psrc, c*2);
// 		break;
// 	}
// }

//int ackIOEvent(int addr, int p){
	// int 	i, v, ch;
	
	// if(addr == MBAD_IO_ACK1)
	// 	ch = 0;
	// else if(addr == MBAD_IO_ACK2)
	// 	ch = 1;
	// else
	// 	return 0;

	// for(i=0; i<16; i++){
	// 	v = (p >> i) & 0x01;

	// 	if(v) {
	// 		pcntl->diData[0][ch*16+i] = 0;
	// 		pcntl->diData[1][ch*16+i] = 0;
	// 	}
	// }
	// storeIntDINVR(&intDINvr);
	// return 1;
//}

//void addIOEvent(int id, int p, int s){
	// if(s)
	// 	pcntl->diData[0][p] = INT_IO_BLINK;
	// else
	// 	pcntl->diData[1][p] = INT_IO_BLINK;

	// updateiPSMDIEvent(50+p, 0, s);
	// storeIntDINVR(&intDINvr);
//}

// DIO의 soe를 읽어서 이를 event list에 기록한다 
//int decodeSOE(int ioid, int count, uint8_t *p)
//{
	// int i, c, dsize=6;
	// SOE_DATA soed;

	// for (i=0; i<count; i++) {
	// 	memcpy(&soed, p, sizeof(soed));
	// 	p += sizeof(soed);
	// 	addIOEvent(ioid, soed.index, soed.status);
	// 	//addEvent(E_SOE, soed.ts, ioid, soed.index, soed.status); 
	// }
//}

#if 1

// 알람상태 : Close
// 알람복구 : Open
//void alarmOutput(int point, int state)
//{
// 	int cnt;

// 	if (pdb->iom.doType[point] != DOTYPE_ALM)
// 		return;
	
// 	cnt = pcntl->ctrlFlag;
// 	pcntl->cmId = 0;
// 	pcntl->cpoint[cnt] = point;
// 	pcntl->ccmd[cnt] = (state) ? 0xff00 : 0x0000;
// 	pcntl->ctrlFlag++;
	
// //	if(state) {
// //		if(++pInfo->Alarm_sts >N_ALARM_LIST)
// //			pInfo->Alarm_sts = N_ALARM_LIST;
// //	}
// //	else
// //		pInfo->Alarm_sts = 0;

// 	printf("[%d]alarmOutput(%d-%d-0x%x)\n", cnt, pcntl->cmId, pcntl->cpoint[cnt], pcntl->ccmd[cnt]); 
//}

// point : 1 ~ 3
void assertAlarmOutput(int point, int state) {
//	int cnt;
	
//	if (state > 0 && point > 0) {
//		alarmOutput(point-1, state);
//	}
	
	if(state)
		++pInfo->Alarm_sts;
}


void negateAlarm() {
	// int i;	
	
	// for(i=0; i<3; i++) {
	// 	if(pdb->iom.doType[i] == DOTYPE_ALM) {
	// 		alarmOutput(i, 0);
	// 		osDelayTask(100);
	// 	}
	// }
	pInfo->Alarm_sts = 0;
}

//void eventOutput(int point, int state)
//{
// 	int cnt;

// 	if (pdb->iom.doType[point] != DOTYPE_EVENT)
// 		return;
		
// 	cnt = pcntl->ctrlFlag;
// 	pcntl->cmId = 0;
// 	pcntl->cpoint[cnt] = point;
// 	pcntl->ccmd[cnt] = (state) ? 0xff00 : 0x0000;
// 	pcntl->ctrlFlag++;
	
// //	if(state) {
// //		if(++pInfo->Event_sts >N_EVENT_LIST)
// //			pInfo->Event_sts = N_EVENT_LIST;
// //	}
// //	else
// //		pInfo->Event_sts = 0;
	
// 	printf("[%d]eventOutput(%d-%d-0x%x)\n", cnt,pcntl->cmId, pcntl->cpoint[cnt], pcntl->ccmd[cnt]); 
//}

// point : 1 ~ 3
void assertEventOutput(int point, int state) {
	int cnt;

//	if (state > 0 && point > 0)

//	eventOutput(point-1, state);
	
	if(state)
		++pInfo->Event_sts;
}

void negateEvent() {
	// int i;	
	
	// for(i=0; i<3; i++) {
	// 	if(pdb->iom.doType[i] == DOTYPE_EVENT) {
	// 		eventOutput(i,0);
	// 		osDelayTask(100);
	// 	}
	// }
	pInfo->Event_sts = 0;
}

// void sendIOCommand(int point, int state)
// {
// 	int cnt;

// 	cnt = pcntl->ctrlFlag;
// 	pcntl->cmId = 0;
// 	pcntl->cpoint[cnt] = point;
// 	pcntl->ccmd[cnt] = state;
// 	pcntl->ctrlFlag++;
// 	printf("[%d]sendIOCommand(%d-%d-0x%x)\n", cnt,pcntl->cmId, pcntl->cpoint[cnt], pcntl->ccmd[cnt]); 
// }

#else
// 알람상태 : Close
// 알람복구 : Open
void alarmOutput(int state)
{
	if (pcntl->almMod > 0 && pcntl->almPoint > 0) {				
		pcntl->cmId = pcntl->almMod-1; 
		// 2017-11-23, cpoint : [24..25] -> [0..1]로 변경
		pcntl->cpoint = pcntl->almPoint-1;			
//		pcntl->ccmd = (state) ? 0x80 : 0x40;
		pcntl->ccmd = (state) ? 0xff00 : 0x0000;
		pcntl->ctrlFlag = 1;
		printf("[alarmOutput : %x]\n", pcntl->ccmd);			
		//addEvent(EVENT_DO, NULL, pcntl->cmId, pcntl->cpoint, pcntl->ccmd);
	}
}

// 2017-11-22
// 제어방식은 펄스 방식만 사용한다, DO channel 의 모드가 pulse일때만 동작한다
void eventOutput(int state) 
{
	if (pcntl->evtMod > 0 && pcntl->evtPoint > 0) {		
		pcntl->cmId = pcntl->evtMod-1; 			// cmId : [0..1]
		// 2017-11-23, cpoint : [24..25] -> [0..1]로 변경
		pcntl->cpoint = pcntl->evtPoint-1;			
		
		if (piocfg[pcntl->cmId].doMode[pcntl->cpoint] == 1) {
			pcntl->ccmd = 0x80;
			pcntl->ctrlFlag = 1;
			printf("[eventOutput : %x]\n", pcntl->ccmd);
		}
		else {
			printf("[eventOutput, DO channel is not pulse type (%d,%d)]\n", pcntl->cmId, pcntl->cpoint); 
		}
		//addEvent(EVENT_DO, NULL, pcntl->cmId, pcntl->cpoint, pcntl->ccmd);
	}	
}
#endif

// void pack_uint16_data(uint16_t* output_data, const uint16_t* input_data) {
// 		int		i;
	
//     output_data[0] = 0; // 첫 번째 UINT16 데이터를 0으로 초기화
//     output_data[1] = 0; // 두 번째 UINT16 데이터를 0으로 초기화

//     for (i = 0; i < 16; i++) {
//         // 첫 번째 UINT16: 입력 배열의 0~15번 데이터를 압축
//         if (input_data[i] == 1) {
//             output_data[0] |= (1 << i); // i번째 비트를 1로 설정
//         }

//         // 두 번째 UINT16: 입력 배열의 16~31번 데이터를 압축
//         if (input_data[i + 16] == 1) {
//             output_data[1] |= (1 << i); // i번째 비트를 1로 설정
//         }
//     }
// }

// int iomHandler(int id, IOM_MEM_DEF *p)
// {
// 	int i, piData, inc, chgF=0;
// 	float	fv;
	
// 	if (pcntl->piInitF[id] == 0) {
// 		for (i=0; i<32; i++) {
// 			pcntl->piLastData[id][i] = p->io.piData[i];
// 		}	
// 		pcntl->piInitF[id] = 1;
// 	}
// 	else {
// 		for (i=0; i<32; i++) {
// 			if (p->io.piData[i] > pcntl->piLastData[id][i]) 
// 				piData = p->io.piData[i] - pcntl->piLastData[id][i];
// 			else if (p->io.piData[i] < pcntl->piLastData[id][i]) 
// 				piData = 65536 - pcntl->piLastData[id][i] + p->io.piData[i];
// 			else
// 				piData = 0;
				
// 			pcntl->piLastData[id][i] = p->io.piData[i];
			

// 			// 변화량이 50보다 크면 버린다
// 			if (piData > 0 && piData < 50) {				
// 				pcntl->piReg[id][i] += (float)piData/piocfg[id].piConst[i];
// 				inc = (int)(pcntl->piReg[id][i]/0.001);
// 				pcntl->piAccm[id][i] += inc;
// 				pcntl->piReg[id][i]  = fmodf(pcntl->piReg[id][i], 0.001);
					
// 				if (pcntl->piAccm[id][i] > 999999999) pcntl->piAccm[id][i] = 1000000000 - pcntl->piAccm[id][i];
// 				// pulse constant 처리????
// 				piom->io.diData[i] = pcntl->piAccm[id][i];
// 				printf("pulse, id=%d, chan=%d, accm=%d\n", id, i, pcntl->piAccm[id][i]);
// 				chgF = 1;
// 			}
// 		}
// 	}
	
// 	piom->io.hbeat = p->io.hbeat;
// 	piom->io.type = p->io.type;
// 	piom->io.dbFlag = p->io.dbFlag;
// 	piom->io.subType = p->io.subType;
// #if 1
// 	pack_uint16_data(piom->io.diData, p->io.diStatus);
// 	pack_uint16_data(piom->io.di_on, pcntl->diData[0]);
// 	pack_uint16_data(piom->io.di_off, pcntl->diData[1]);

// 	for (i=0; i<8; i++) {
// 		pmeter->dom[i] = piom->io.doSts[i] = p->io.doStatus[i];
// 	}	

// #else
// 	for (i=0; i<INT_DI_MAX; i++) {
// 		pmeter->dim[i] = piom->io.diData[i] = p->io.diStatus[i] | pcntl->diData[id][i];
// 	}	
// 	for (i=0; i<8; i++) {
// 		pmeter->dom[i] = piom->io.doData[i] = p->io.doStatus[i];
// 	}	
// #endif
// 	for (i=0; i<8; i++) {
// 		pmeter->aim[i] = piom->io.aiData[i] = p->io.aiData[i];
// 	}	
// 	for (i=0; i<3; i++) {
// 		pmeter->dc_aim[i] = piom->io.dcData[i] = p->io.dcData[i];
// 	}	

// 	return chgF;
// }


// int getIOMType(int slot)
// {
// 	return piom->modType[1+slot];
// }

// void copyPulseData()
// {
// 	int i;
		
// 	for (i=0; i<32; i++) {
// 		piom->io.diData[i] = pcntl->piAccm[0][i];
// 		printf("[%d]", pcntl->piAccm[0][i]);
// 	}
// 	printf("\n");
// }


//void simpleMapIO(int ix, int modType, int subType)
//{
//	int j;
//	float newVal;
//	// DI8DO6
//	if (modType == IOM_DIO_1) {
//		for (j=0; j<8; j++) pmeter_s->_diChan[j] = piom->io[ix].diStatus[j];
//		for (j=0; j<6; j++) pmeter_s->_doChan[j] = piom->io[ix].doStatus[j];
//	}
//	// DI8DO6
//	else if (modType == IOM_DIO_2) {
//		for (j=0; j<8; j++) pmeter_s->_diChan[j+8] = piom->io[ix].diStatus[j];
//		for (j=0; j<6; j++) pmeter_s->_doChan[j+6] = piom->io[ix].doStatus[j];
//	}
//	// DCMETER (DI, DO 채널은 DIO 영역의 DI, DO 영역을 그댈로 사용한다 
//	else if (modType == IOM_DCM_1) {		
//// Simple Map에서 DCM의 DI, DO는 사용하지 않는다 
////		for (j=0; j<4; j++) pmeter_s->_diChan[j] = piom->io[ix].diStatus[j];
////		for (j=0; j<2; j++) pmeter_s->_doChan[j] = piom->io[ix].doStatus[j];		
//		for (j=0; j<3; j++) {
//			pmeter_s->_dcMeter[j] = piom->io[ix].aiData[j];
//		}
//	}
//}


//void copyToMeter(void) {
//	memcpy(pmeter->dim, piom->io.diData, sizeof(pmeter->dim));
//	memcpy(pmeter->dim, pcntl->diData, sizeof(pmeter->dim));
	int		i;

	// for(i=0; i<INT_DI_MAX; i++)
	// 	pmeter->dim[i] = piom->io.doData[i] | pcntl->diData[0][i];

	// memcpy(pmeter->dom, piom->io.doData, sizeof(pmeter->dom));
	// memcpy(pmeter->aim, piom->io.aiData, sizeof(pmeter->aim));
	// memcpy(pmeter->dc_aim, piom->io.dcData, sizeof(pmeter->dc_aim));
//}

// 2018-6-14, DCM 추가
// void IOMScan(int pid, int uid) {
// 	int i=0, nw, nr;
// //	IO_CFG *pcfg;
// 	IOM_DATA *pio;
	
// 	pio = &piom->io;
// //	pcfg = &piocfg;
// 	// db download 상태가 0이면 db download 내린다
// #if 1
// 	nw = makePollFrame(iotb, pid+1, IOM_START, IOM_SIZE);
// 	if (sndRcvMmbFrame(uid, iotb, nw, iorb, &nr) != -1) {
// 		decodeMmbIO(iotb[1], IOM_START, IOM_SIZE, &iorb[3], (uint16_t *)&ios); 
				
// 		// pi process
// 		//chgF += iomHandler(i, &ios);					
// 		iomHandler(0, &ios);					
// 		//memcpy(&piom->io[i], &ios.io, sizeof(ios.io));
// 		if (ios.io.dbFlag == 0) {
// 			piom->dbStatus[1+i] = 0;
// 		}
// 	}
// 	// change dump 방식으로 SOE를 요청한다 
// 	if (ios.count) {
// 		// FC = 7 (SOE DUMP, s: 0, c: soe count) 
// 		printf("SOE Dump : c=%d\n", ios.count);
// 		nw = makeSOEDumpFrame(iotb, pid+1, ios.count);
// 		if (sndRcvMmbFrame(uid, iotb, nw, iorb, &nr) != -1) {
// 			decodeSOE(i, ios.count, &iorb[3]);
// 		}			
// 	}
// #else
// 	if (pcntl->ctrlFlag) {
// 		if (pcntl->cmId == 0) {
// 			nw = makeControlFrame(iotb, pid+1, pcntl->cpoint+68, pcntl->ccmd);
// //			nw = makeControlFrame(iotb, pid+1, pcntl->cpoint, pcntl->ccmd);
// 			pcntl->ctrlFlag = 0;
// 			if (sndRcvMmbFrame(uid, iotb, nw, iorb, &nr) != -1) {
// 				printf(">>> send Control(mod=%d, point=%d, cmd=%d) <<<\n", pcntl->cmId, pcntl->cpoint, pcntl->ccmd);
// 			}
// 		}
// 		else {
// 			printf(">>> io_mmb, invalid contrl cmd(%d, %d, %d) <<<\n", pcntl->cmId, pcntl->cpoint, pcntl->ccmd);
// 			pcntl->ctrlFlag = 0;
// 		}
// 	}
// 		else if(pcntl->tod.tm_sec==5){
// 		ios.cfg.enable = 1;
// 		//ios.cfg.ts = sysTick64;	
// 		memcpy(ios.cfg.ts, &sysTick64, sizeof(sysTick64));
// 		nw = makeSettingFrame(iotb, pid+1, SETTING_Time, 4, (uint16_t *)&ios.cfg.ts);
// 		if (sndRcvMmbFrame(uid, iotb, nw, iorb, &nr) != -1) {
// 			piom->dbStatus[1+i]  = 1;
// //			printf(">>> send IO timesync(%d) <<<\n", pid);
// 		}
// 		else {
// //			printf(">>> send IO timesync(%d) failed <<<\n", pid);
// 		}

// 	}
// 	else {
// 		nw = makePollFrame(iotb, pid+1, IOM_START, IOM_SIZE);
// 		if (sndRcvMmbFrame(uid, iotb, nw, iorb, &nr) != -1) {
// 			decodeMmbIO(iotb[1], IOM_START, IOM_SIZE, &iorb[3], (uint16_t *)&ios); 
				
// 			// pi process
// 			//chgF += iomHandler(i, &ios);					
// 			iomHandler(0, &ios);					
// 			//memcpy(&piom->io[i], &ios.io, sizeof(ios.io));
// 			if (ios.io.dbFlag == 0) {
// 				piom->dbStatus[1+i] = 0;
// 			}
// 		}
// 		// change dump 방식으로 SOE를 요청한다 
// 		if (ios.count) {
// 			// FC = 7 (SOE DUMP, s: 0, c: soe count) 
// 			printf("SOE Dump : c=%d\n", ios.count);
// 			nw = makeSOEDumpFrame(iotb, pid+1, ios.count);
// 			if (sndRcvMmbFrame(uid, iotb, nw, iorb, &nr) != -1) {
// 				decodeSOE(i, ios.count, &iorb[3]);
// 			}			
// 		}
// 	}

// #endif				
// 	copyToMeter();
// }


// int sndRcvMmbFrame(int pid, uint8_t *txb, int nw, uint8_t *rxb, int *nr)
// {
// 	int	timer=10;
// 	int	st=0, inx=0, length=0;
// 	uint8_t inchar;
// 	uint16_t	ccrc, rcrc;
// 	uint32_t  et1, et2, gcnt=0;
	
// 	UARTSend(pid, txb, nw);
// 	dprtbuffer(3, "IOT:", txb, nw);
	
// 	et1 = sysTick32;
// 	while (1) {	
// 		if (UARTReceive(pid, &inchar, 1) == 0) {
// 			if (--timer == 0) {
// 				et2 = sysTick32;
// //				printf("sndRcvMmbFrame, timeout, id=%d, st=%d, inx=%d, elapT=%d, gcnt=%d\n", txb[0], st, inx, et2-et1, gcnt);
// 				//dprtbuffer(1, "IOR:", rxb, inx);		
// 				//os_dly_wait(500);
// 				return -1;
// 			}
// 			else {
// 				osDelayTask(10);
// 				continue;
// 			}
// 		}
		
// 		timer = 10;

// 		if (st == 0) {			
// 			if (inchar == txb[0]) {
// 				inx = 0;
// 				rxb[inx++] = inchar;
// 				st = 1;
// 			}
// 			else {
// 				gcnt++;
// 			}
// 		}
// 		else if (st == 1) {		
// 			if (inchar == txb[1]) {
// 				rxb[inx++] = inchar;
// 				st = 2;
// 			}
// 			else {
// 				st = 0;
// 			}
// 		}
// 		else if (st == 2) {
// 			if ((txb[1] == 5) || (txb[1] == 6) || (txb[1] == 15) || (txb[1] == 16))
// 				length = 8;
// 			else 
// 				length = inchar+5;
// 			rxb[inx++] = inchar;
// 			st = 3;
// 		}
// 		else if (st == 3) {
// 			rxb[inx++] = inchar;
// 			if (inx == length) {
// 				if (gencrc_modbus(rxb, length) == 0) {					
// 					*nr = inx;
// 					dprtbuffer(3, "IOR:", rxb, inx);					
// 					return 0;
// 				}
// 			}
// 		}
// 	}
// }


#ifdef __RTX
__task void IOM_Task()
#else
void IOM_Task(void *arg)
#endif
{
    int16_t temp_c, temp_c10;
	int ch;

	// 초기화 (시작 시 한 번, 예: app_main 또는 Board 초기화에서)
	TempSensor_Init();
	
	_enableTaskMonitor(Tid_Iom, 50);
// 	pcntl->ioDbFlag = 0x1234;	// fisrst scan 시 db 전송
//	pInfo->Io_sts = STS_ERROR;

	printf("[IOM Scan Task]\n");

	while (1) {
		pcntl->wdtTbl[Tid_Iom].count++;
		/* ADC0_1 ~ ADC0_4 총 4채널 온도 읽기 */
		for (ch = 0; ch < TEMP_SENSOR_NUM_CHANNELS; ch++) {
			if (TempSensor_ReadTempC(ch, &temp_c))
				printf("ADC0_%d 온도: %d °C\n", ch + 1, (int)temp_c);
			else
				printf("ADC0_%d 온도 읽기 실패\n", ch + 1);
			if (TempSensor_ReadTempCx10(ch, &temp_c10))
				printf("ADC0_%d 온도: %d.%d °C\n", ch + 1, temp_c10 / 10, temp_c10 % 10);
			else
				printf("ADC0_%d 온도 읽기 실패\n", ch + 1);
		}
		osDelayTask(1000);

// 		if(pInfo->Io_sts == STS_ERROR) {
// 			if (sysTick1s == ts1s) {
// 				osDelayTask(100);
// 				continue;
// 			}			
// 			ts1s = sysTick1s;
// 			IOMScan(0, uId);	
// 			osDelayTask(100);
// 			continue;
// 		}

// 		if (pcntl->ctrlFlag) {
// 			if (pcntl->cmId == 0) {
// 				pcntl->ctrlFlag--;

// 				cnt = pcntl->ctrlFlag;
// 				pnt = pcntl->cpoint[cnt];

// 				if(pnt < IOMAX_DO)
// 					nw = makeControlFrame(iotb, uId+1, pcntl->cpoint[cnt]+IDX_MB_DO_STS, pcntl->ccmd[cnt]);
// 				else if(pnt >= IOM_COMMAND)
// 					nw = makeControlFrame(iotb, uId+1, pcntl->cpoint[cnt], pcntl->ccmd[cnt]);

// 	//			nw = makeControlFrame(iotb, pid+1, pcntl->cpoint, pcntl->ccmd);
// 				if (sndRcvMmbFrame(uId, iotb, nw, iorb, &nr) != -1) {
// 					printf(">>>[%d] send Control(mod=%d, point=%d, cmd=%x) <<<\n", cnt, pcntl->cmId, pcntl->cpoint[cnt], pcntl->ccmd[cnt]);
// 				}
// 			}
// 			else {
// 				printf(">>> io_mmb, invalid contrl cmd(%d, %d, %d) <<<\n", pcntl->cmId, pcntl->cpoint[cnt], pcntl->ccmd[cnt]);
// 				pcntl->ctrlFlag = 0;
// 			}
// 		}
// 		else if(pcntl->tod.tm_sec==5){
// 			ios.cfg.enable = 1;
// 			//ios.cfg.ts = sysTick64;	
// 			memcpy(ios.cfg.ts, &sysTick64, sizeof(sysTick64));
// 			nw = makeSettingFrame(iotb, uId+1, SETTING_Time, 4, (uint16_t *)&ios.cfg.ts);
// 			if (sndRcvMmbFrame(uId, iotb, nw, iorb, &nr) != -1) {
// 				piom->dbStatus[1+i]  = 1;
// //				printf(">>> send IO timesync(%d) <<<\n", uId);
// 			}
// 			else {
// 				printf(">>> send IO timesync(%d) failed <<<\n", uId);
// 			}
// 			osDelayTask(1000);
	
// 		}
// 		else if(pcntl->ioDbFlag == 0x1234){
// 			pcntl->ioDbFlag = 0x4321;

// 			nw = makeSettingFrame(iotb, uId+1, SETTING_DB_1, 120, (uint16_t *)&pdbk->iom.diType);
// 			if (sndRcvMmbFrame(uId, iotb, nw, iorb, &nr) != -1) {
// 				piom->dbStatus[1+i]  = 1;
// 				printf(">>> send IO DB#1(%d) <<<\n", uId);
// 			}
// 			else {
// 				printf(">>> send IO DB#1(%d) failed <<<\n", uId);
// 			}
// 		}
// 		else if(pcntl->ioDbFlag == 0x4321){
// 			pcntl->ioDbFlag = 0;

// 			nw = makeSettingFrame(iotb, uId+1, SETTING_DB_2, 40, (uint16_t *)&pdbk->iom.aiScale);
// 			if (sndRcvMmbFrame(uId, iotb, nw, iorb, &nr) != -1) {
// 				piom->dbStatus[1+i]  = 1;
// //				printf(">>> send IO DB#2(%d) <<<\n", uId);
// 			}
// 			else {
// 				printf(">>> send IO DB#2(%d) failed <<<\n", uId);
// 			}
// 		}

// 		else {
// 			if (sysTick1s == ts1s) {
// 				osDelayTask(100);
// 				continue;
// 			}			
// 			ts1s = sysTick1s;
// 			IOMScan(0, uId);	
// 			osDelayTask(100);
// 		}
	}
}

