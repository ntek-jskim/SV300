#include "RTL.h"
#include "os_port.h"
#include "meter.h"
#include "math.h"
#include "crc.h"
#include "string.h"
#include "debug.h"

// #define MBADDR_TIME				0
// #define MBADDR_SET_COMM			122
// #define MBADDR_SET_FEEDER		240
// #define	MBADDR_SET_END			1320
// #define MBSIZE_TIME				8		// word size
// #define MBSIZE_SET_COMM			56		// word size
// #define MBSIZE_SET_FEEDER		20		// word size

#define MBADDR_AI_SYSTEM		0
#define MBADDR_AI_VOLT			12		// volt
#define MBADDR_AI_FEEDER		50
#define MBADDR_AI_FEEDER_END	6529
#define MBADDR_WH_FEEDER		6532					
#define MBADDR_WH_FEEDER_END	8692
#define MBADDR_U_E_TIME			8700
#define MBADDR_U_E_TIME_END		8711
#define MBADDR_EVT_LIST			8712
#define MBADDR_EVT_LIST_END		8972

#define MBADDR_MINMAX			9000
#define MBADDR_MINMAX_FD		9104

#define MBADDR_MINMAX_END		19476

#define MBADDR_IPSM_STS			19500
#define MBADDR_IPSM_STS_END		19573

#define MBSIZE_AI_SYSTEM		10	// word size
#define MBSIZE_AI_VOLT			32 	// 28 -> 24(reserved를 제외한다)
#define MBSIZE_AI_FEEDER		90		// word size
#define MBSIZE_WH_FEEDER		30		// word size
#define MBSIZE_U_E_TIME			16		// word size		sag/swell time + event fr, re, count, seq

#define MBSIZE_EVT_LIST			8		// word size

#define MBSIZE_MINMAX			144		// word size

#define MBSIZE_IPSM_STS			76		// word size

// iPSM-DI
#define MBADDR_EXTDI_START		0
#define MBSIZE_EXTDI			64

#define MBADDR_EXTPI_START		64
#define MBSIZE_EXTPI			120


#define	MCS_SYS_TYPE			10

#define	MCS_DATA_U1				0
#define	MCS_DATA_U2				200
#define	MCS_DATA_U3				334

#define	MCS_DATA_I1				0
#define	MCS_DATA_I2				224				// In
#define	MCS_DATA_I3				342				// tot kWh~

//MGEM_DATA gems3500[2] __attribute__ ((section ("EXT_RAM"), zero_init));
// modbus copy 영역 
//MGEM_DATA	gems3500[2] __attribute__ ((section ("EXT_RAM"), zero_init));
// 화면 표시 
//MGEM3600_DATA	gemsEng[2] __attribute__ ((section ("EXT_RAM"), zero_init));
//

uint8_t _mbuf[256];

typedef struct {
	uint8_t initFlag, commStatus, commFailCount, devEnabled, fid;	
} GEMS_DEVICE;

//#define	N_SCAN_Q	128
#define	N_SCAN_Q	1024

typedef struct {
	uint8_t id, fc;
	uint16_t s, c;
} SCAN_BLK;

typedef struct {
	int fr, re;
	SCAN_BLK	sblk[N_SCAN_Q];
} SCAN_Q;

GEMS_DEVICE	devCB[8];
SCAN_Q	scanQ;
//static RING_BUF2 scQ_g3500;


//static uint16_t g3500Status[16];
static uint8_t mmbtb[300], mmbrb[300];
static int  Mcs_dbScan[MAX_MCS]={0};

uint8_t mdebug;


extern int sndRcvMmbFrame2(int pid, uint8_t *txb, int nw, uint8_t *rxb, int nr);
extern void copyFeeder(int gid, int fid);
extern void setNumFeeder(int gid, int n);

// const uint16_t ScanBlock_G3500[][4] = 
// {
// 	{1, 3, MBADDR_SET_COMM, 1},										// get feeder count
// 	{1, 4, MBADDR_AI_FEEDER, MBSIZE_AI_FEEDER},		// get feeder data
// 	{1, 4, MBADDR_WH_FEEDER, MBSIZE_WH_FEEDER},		// get wh
// 	{1, 4, MBADDR_WH_FEEDER, MBSIZE_WH_FEEDER},		// get wh
// };


// id=0, Ext Comm. Module
// id=1, gems3500#1
// id=2, gems3500#2
//int chkBD3500Com(int id)
//{
//	return (pgwst->status[id]);
//}

uint32_t getUInt32BE(uint8_t *buf, int *os) {
	uint8_t *p = buf + *os;
	uint32_t v = (*p++ << 24) | (*p++ << 16) | (*p++ << 8) | *p++;
	*os += 4;
	return v;
}

uint16_t getUInt16BE(uint8_t *buf, int *os) {
	uint8_t *p = buf + *os;
	uint16_t v = (p[0] << 8) | p[1];
	*os += 2;
	return v;
}

int32_t floatToI32B(float v, int multi)
{
	int32_t iv;
	
	v = v * multi;
	iv = v;
	
	return (iv>>16) | (iv << 16);
}

uint32_t u32ToU32B(uint32_t iv)
{
	return (iv>>16) | (iv << 16);
}

uint16_t floatToI16(float v, int multi)
{
	int16_t iv;
	
	v = v * multi;
	iv = v;
	
	return iv;
}

uint32_t swapUWord(uint32_t v)
{
	return (v<<16)|(v>>16);
}

int32_t swapSWord(int32_t v)
{
	return ((v << 16) & 0xFFFF0000) | ((v >> 16) & 0x0000FFFF);
}

static int tdebug;

void tdebug_on(void)
{
 	//tdebug = 1;
 	mdebug=1;
}

void tdebug_off(void)
{
 	tdebug = 0;
 	mdebug = 0;
 }

// U2의 경우 Tx신호가 Rx로 들어온다. Tx후 Rx로 수신된 Tx신호를 제거한다 
int sndRcvMmbFrame2(int pid, uint8_t *txb, int nw, uint8_t *rxb, int nr)
{
	int	timer=50;
	int	st=0, inx=0, i;
	uint8_t inchar;
	uint16_t	ccrc, rcrc;

	for (i=0; i<256; i++) {
		if (UARTReceive(pid, &inchar, 1) == 0) break;
//		printf("[%02x]\n", inchar);		
	}

	
	UARTSend(pid, txb, nw);
	dprtbuffer(pid, "TxData>>", txb, nw);
	
//	st = (pcal->hwVer == 0) ? -1 : 0;
	
	while (1) {	
		if (UARTReceive(pid, &inchar, 1) == 0) {
			if (--timer == 0) {
				if (tdebug) printf("sndRcvMmbFrame2 Timeout, st=%d, inx=%d\n", st, inx);
				return 0;
			}
			else {
            osDelayTask(10);
				continue;
			}
		}
		
		timer = 50;		
		
		//printf("[%02x]\n", inchar);		
		// skip txdata 
		if (st == -1) {
			if (++inx == 8) {
				st = inx = 0;
			}
		}
		else if (st == 0) {			
			if (inchar == txb[0]) {
				rxb[inx++] = inchar;
				st = 1;
			}
		}
		else if (st == 1) {		
			if (inchar == txb[1]) {
				rxb[inx++] = inchar;
				st = 2;
			}
			else {
				st = inx = 0;
			}
		}
		else {
			rxb[inx++] = inchar;
			if (inx == nr) {
				if (gencrc_modbus(rxb, nr) == 0) {					
					dprtbuffer(pid, "RxData>>", rxb, inx);					
					return inx;
				}
				else {
					printf("sndRcvMmbFrame2, CRC Error, inx=%d\n", inx);
					return 0;
				}
			}
		}
	}
}


int chkPollBuffer(RING_BUF2 *buf)
{
	return (buf->fr != buf->re);
}

void scan_sb(SCAN_BLK *psb, uint8_t id, uint8_t fc, uint16_t s, uint16_t c) {	
	psb->id = id;
	psb->fc = fc;
	psb->s  = s;
	psb->c  = c;

//	printf("scan_sb[%d] : fc = %d, s = %d, c = %d\n", id, fc, s, c);
}

int getPollReq(SCAN_BLK *psb) 
{
	if (scanQ.fr == scanQ.re) 
		return -1;

	memcpy(psb, &scanQ.sblk[scanQ.re], sizeof(*psb));
	scanQ.re = (scanQ.re + 1) % N_SCAN_Q;

	return 0;
}

int putPollReq(SCAN_BLK *psb)
{
	memcpy(&scanQ.sblk[scanQ.fr], psb, sizeof(*psb));
	scanQ.fr = (scanQ.fr + 1)% N_SCAN_Q;
	return 1;
}


int genG3600Frame(SCAN_BLK *psb, uint8_t *buf, int *pnw, int *pnr)
{
	int 			inx = 0;
	uint16_t	calcCrc;

	buf[inx++] = psb->id;		// address	
	buf[inx++] = psb->fc;		// function code
	buf[inx++] = psb->s>>8;
	buf[inx++] = psb->s;		// start address
	buf[inx++] = psb->c>>8;
	buf[inx++] = psb->c;		// count
	calcCrc = gencrc_modbus(buf, inx);
	buf[inx++] = calcCrc;
	buf[inx++] = calcCrc >> 8;
	
	if (ntDebugLevel==6) printf("genScanReqFrame, id=%d, fc=%d, s=%d, c=%d\n", psb->id, psb->fc, psb->s, psb->c);
//	printf("genScanReqFrame, id=%d, fc=%d, s=%d, c=%d\n", psb->id, psb->fc, psb->s, psb->c);

	*pnw = inx;
	switch (psb->fc) {
		case 1:
		case 2:
			*pnr = 5 + psb->c/8;
			if (psb->c%8) *pnr += 1;
			break;
		
		case 3:
		case 4:
			*pnr = 5 + psb->c*2;
			break;
		case 5:
		case 6:
		case 15:
		case 16:
			*pnr = 6;
			break;
	}
	
	return inx;
}


//  big endian stream을 메모리에 쓴다 
void updateMem(int s, int c, uint8_t *psrc, uint16_t *pdst)
{
	int 		i;
	uint8_t *ptr = (uint8_t *)(pdst+s);

	for (i=0; i<c; i++, psrc+=2) {
		*ptr++ = *(psrc+1);
		*ptr++ = *psrc;
	}
}

void updateMemMCS(int s, int c, uint8_t *psrc, uint16_t *pdst)
{
	int 		i;
	uint8_t *ptr = (uint8_t *)(pdst+s);

	for (i=0; i<c/2; i++) {
		*ptr++ = *(psrc+3);
		*ptr++ = *(psrc+2);
		*ptr++ = *(psrc+1);
		*ptr++ = *psrc;;
		// *ptr++ = *(psrc+2);
		// *ptr++ = *(psrc+3);
		// *ptr++ = *psrc;
		// *ptr++ = *(psrc+1);
		psrc += 4;
	}
}

// int getEventGems3600(int id) {
// 	int	fr, re, cnt, count, i;

// 	 if (id == 1 || id == 2) {
// 	 	fr = meter.iPSM[id-1].elist.fr;
// 		re = meter.iPSM[id-1].elist.re;
// 		count = meter.iPSM[id-1].elist.count;

// 		cnt = pcntl->evt_cnt[id-1];
// 	 }

// }
void updateMCS(SCAN_BLK *psb, uint8_t *gb)
{
	uint8_t 	fc;
	switch (psb->fc) {
		case 3 :		
			updateMemMCS(psb->s-MCS_SYS_TYPE, psb->c, gb, (uint16_t *)&meter.iMCS_int.set.MCSU_mbusId);
			Mcs_dbScan[psb->id-2] = 1;
			break;
		case 4 :		
			updateMemMCS(psb->s, psb->c, gb, (uint16_t *)&meter.iMCS_int.fd[psb->id-3]);
			break;
	}	
}
// modbus stream을 gems3500 영역으로 복사
void updateGems3600(SCAN_BLK *psb, uint8_t *gb)
{
	uint8_t 	fc;
	uint16_t	s, c, feeder, i, ix;
	uint16_t *dst;
	
	switch (psb->fc) {

		case 4 :		
			if (psb->id > 0 && psb->id < 3) {
				updateMem(psb->s, psb->c, gb, (uint16_t *)&meter.iPSM[psb->id-1]);
				if(psb->s == 8952)
					pcntl->evt_scan[psb->id-1] = 1;

				if(psb->s == MBADDR_AI_SYSTEM) {
					if(ntDebugLevel==20)
						printf("updateGems3600[%d] : tick = %lx\n", psb->id, sysTick64);
				}
			}
			else if(psb->id > 2 && psb->id < 5) {
				updateMem(psb->s, psb->c, gb, (uint16_t *)&meter.iPSMDI[psb->id-3]);
			} 
			break;
	}
}

void gen_MCS_ScanReq(int cnt) {
	int i, j, type;
	SCAN_BLK sb;
//	static int  whPeriod[MAX_MCS];

	// 장치 하나만 스캔
	for (i=0; i<cnt+1; i++) {

		// MCS-U
		if(i==0){

			if(Mcs_dbScan[i] ==0){
				//  addr,fc,      start, cnt 
				scan_sb(&sb, i+2, 3, 20, 10);		// modbus addr, baud rate
				putPollReq(&sb);
				scan_sb(&sb, i+2, 3, 61698, 16);	// model info
				putPollReq(&sb);
				scan_sb(&sb, i+2, 3, 64512, 6);	// model info
				putPollReq(&sb);
				scan_sb(&sb, i+2, 3, 64768, 6*cnt);	// model info
				putPollReq(&sb);
			}
			else {
				//  addr,fc,      start, cnt 
				scan_sb(&sb, i+2, 4, MCS_DATA_U1, 72);
				putPollReq(&sb);
				scan_sb(&sb, i+2, 4, MCS_DATA_U2, 50);
				putPollReq(&sb);
				scan_sb(&sb, i+2, 4, MCS_DATA_U3, 8);
				putPollReq(&sb);
			}
		}
		else {
			if(Mcs_dbScan[0] != 0) {
				//  addr,fc,      start, cnt 
				scan_sb(&sb, i+2, 4, MCS_DATA_I1, 112);
				putPollReq(&sb);
				scan_sb(&sb, i+2, 4, MCS_DATA_I2, 46);				// In ~ Max I3 demand
				putPollReq(&sb);
				scan_sb(&sb, i+2, 4, MCS_DATA_I3, 58);				// total kwh ~ total kvarh(cal)
				putPollReq(&sb);
			}
		}
		// type = meter.iMCS_ext.sysType[i];

		// if(type !=0) {
		// 	// U : 0
		// 	scan_sb(&sb, i+3, 4, MCS_DATA_1, 84);
		// 	putPollReq(&sb);
		// 	// Upp : 200
		// 	scan_sb(&sb, i+3, 4, MCS_DATA_2, 120);
		// 	putPollReq(&sb);
		// 	// Upp THD : 334
		// 	scan_sb(&sb, i+3, 4, MCS_DATA_3, 66);
		// 	putPollReq(&sb);
		// }
		// else {
		// 	// System type
		// 	scan_sb(&sb, i+3, 3, MCS_SYS_TYPE, 2);
		// 	putPollReq(&sb);
		// }
	}
}

void genScanReq() {
	int i, j;
	uint8_t whPeriod[2]={10, 10};
	uint8_t mmPeriod[2]={50, 50};
	uint16_t	fr, re, count, cnt;
	SCAN_BLK sb;
	
	// 장치 하나만 스캔
	for (i=0; i<4; i++) {
		if(devCB[i].devEnabled ==0)
			continue;
			
		// Feeder Data
		if (i < 2) {
			// get system
			scan_sb(&sb, i+1, 4, MBADDR_AI_SYSTEM, MBSIZE_AI_SYSTEM);
			putPollReq(&sb);

			if(pInfo->iPSM_sts[i]==STS_ERROR)
				continue;

			scan_sb(&sb, i+1, 4, MBADDR_AI_VOLT, MBSIZE_AI_VOLT);
			putPollReq(&sb);

			// di data 까지 gems9000에서 가공
			// scan_sb(&sb, i+1, 4, MBADDR_IPSM_STS, MBSIZE_IPSM_STS);
			// putPollReq(&sb);

			scan_sb(&sb, i+1, 4, MBADDR_U_E_TIME, 12);
			putPollReq(&sb);

			for(j=0; j<meter.iPSM[i].sys.b_cnt; j++){
				scan_sb(&sb, i+1, 4, MBADDR_AI_FEEDER+(j*MBSIZE_AI_FEEDER), MBSIZE_AI_FEEDER);
				putPollReq(&sb);
			}

			if (++whPeriod[i] >= 10) {
			 	whPeriod[i] = 0;
			 	for(j=0; j<meter.iPSM[i].sys.b_cnt; j++){
			 		scan_sb(&sb, i+1, 4, MBADDR_WH_FEEDER+(j*MBSIZE_WH_FEEDER), MBSIZE_WH_FEEDER);
			 		putPollReq(&sb);
			 	}
				if(pcntl->ipsm_scan[i]==0)
					pcntl->ipsm_scan[i] = 1;
			}			

			if (++mmPeriod[i] >= 50) {
			 	mmPeriod[i] = 0; 
				// temp, freq, U
				scan_sb(&sb, i+1, 4, MBADDR_MINMAX, 104);
				putPollReq(&sb);

			 	for(j=0; j<meter.iPSM[i].sys.b_cnt; j++){
			 		scan_sb(&sb, i+1, 4, MBADDR_MINMAX_FD+(j*MBSIZE_MINMAX), 120);
			 		putPollReq(&sb);

					scan_sb(&sb, i+1, 4, MBADDR_MINMAX_FD+(j*MBSIZE_MINMAX)+120, 24);
			 		putPollReq(&sb);
			 	}
#ifdef	EVENTDUMP				
				// event list
				scan_sb(&sb, i+1, 4, MBADDR_EVT_LIST, 120);
		 		putPollReq(&sb);
				scan_sb(&sb, i+1, 4, MBADDR_EVT_LIST+120, 120);
		 		putPollReq(&sb);
				scan_sb(&sb, i+1, 4, MBADDR_EVT_LIST+240, 18);
		 		putPollReq(&sb);
#endif			
			}
		}
		// di
		else {
			scan_sb(&sb, i+1, 4, MBADDR_EXTDI_START, MBSIZE_EXTDI);
			putPollReq(&sb);
//			scan_sb(&sb, i+1, 4, MBADDR_EXTPI_START, MBSIZE_EXTPI);
//			putPollReq(&sb);
		}
	}
}
void Gateway_MCS_Task(void *arg)
{
	int pid=2, b_cnt;	// 
	int	i, j, ix, nr, nw;
	int gdId;	// device ix
	SCAN_BLK sb, *psb=&sb;

	// UART Init, RS485 port
	UART_Init(pid, getBaudrate(pdb->comm.baud));

	printf("[Tid_GW(RS485) Gateway_MCS_Task, pid=%d, speed=%d]\n", 
		pid, getBaudrate(pdb->comm.baud));
			
	memset(&scanQ, 0, sizeof(scanQ));
	mdebug = 0;
	_enableTaskMonitor(Tid_GW, 50);	
	
	b_cnt = pdb->comm.MCS_count;

	while (1) {		
		pcntl->wdtTbl[Tid_GW].count++;
		gen_MCS_ScanReq(b_cnt);

		while (getPollReq(&sb) == 0) {
			pcntl->wdtTbl[Tid_GW].count++;
			gdId = psb->id - 3;
			// index로 장치주소 얻는다
			genG3600Frame(psb, mmbtb, &nw, &nr);					
			
			dprtbuffer(psb->id+10, "[MMB_TX]", mmbtb, nw); 
			if (sndRcvMmbFrame2(pid, mmbtb, nw, mmbrb, nr) == nr) {
				dprtbuffer(psb->id+10, "[MMB_RX]", mmbrb, nr); 						
				updateMCS(psb, &mmbrb[3]);
				if (devCB[gdId].commStatus) {
//					printf("return from com. error(%d)\n", gdId);
				}
				devCB[gdId].commStatus = 0;	// 통신정상
				devCB[gdId].commFailCount = 0;				
			}
			else {			
				//printf("txcnt=%d, rxcnt=%d, err=%.3f\n", txcnt, rxcnt, (txcnt-rxcnt)*100./txcnt);
				if (devCB[gdId].commFailCount < 3) {
					devCB[gdId].commFailCount++;
				}
				else {
					if (devCB[gdId].commStatus == 0) {
						devCB[gdId].commStatus = 1;	// 통신이상
						if (gdId < 2) {
							devCB[gdId].initFlag = 0;
						}
						printf("Gateway_Task : reply timeout (%d)...\n", gdId);						
					}					
				}
			}
        	osDelayTask(5);
		}
        osDelayTask(50);
	}
}

void Gateway_Task(void *arg)
{
	int pid=2;	// 
 	//uint64_t		ulNextConfigScan;
	int	i, j, ix, nr, nw;
	int gsId;	// scan block ix
	int gdId;	// device ix
	//uint16_t c, sb[4];
	uint8_t *gb, devId, whPeriod[2];		
	int	txcnt=0, rxcnt=0;
	SCAN_BLK sb, *psb=&sb;
	
	// UART Init, RS485 port
	UART_Init(pid, getBaudrate(pdb->comm.baud));

	printf("[Tid_GW(RS485) Gateway_Task, pid=%d, speed=%d]\n", 
		pid, getBaudrate(pdb->comm.baud));
			
	memset(&scanQ, 0, sizeof(scanQ));
	
	for (i=0; i<8; i++) {
		devCB[i].devEnabled = (pdb->comm.gwEable & (1<<i)) ? 1 : 0;
		printf("g36(%d): %d\n", i, devCB[i].devEnabled);
	}

	// printf("sizeof(MGEM3600_DATA) = 0x%x\n", sizeof(MGEM3600_DATA));
	// printf("sizeof(SYSTEM) = 0x%x, addr = 0x%x\n", sizeof(SYSTEM), &meter.iPSM[0].sys);
	// printf("sizeof(VOLTAGE) = 0x%x, addr = 0x%x\n", sizeof(VOLTAGE), &meter.iPSM[0].volt);
	// printf("sizeof(F_DATA) = 0x%x, addr = 0x%x\n", sizeof(F_DATA), &meter.iPSM[0].fd);
	// printf("sizeof(GW_REG32) = 0x%x, addr = 0x%x\n", sizeof(GW_REG32), &meter.iPSM[0].Egy.Ereg32);
	// printf("sizeof(F_ENERGY) = 0x%x, addr = 0x%x\n", sizeof(F_ENERGY), &meter.iPSM[0].Egy);
	// printf("sizeof(E_DATE) = 0x%x, addr = 0x%x\n", sizeof(E_DATE), &meter.iPSM[0].eventTimeTag);
	// printf("sizeof(iPSM_EVENT_LIST) = 0x%x, addr = 0x%x\n", sizeof(iPSM_EVENT_LIST), &meter.iPSM[0].elist);
	// printf("sizeof(iPSM_MAXMIN) = 0x%x, addr = 0x%x\n", sizeof(iPSM_MAXMIN), &meter.iPSM[0].maxmin);
	// printf("sizeof(iPSM_STS) = 0x%x, addr = 0x%x\n", sizeof(iPSM_STS), &meter.iPSM[0].sts);
	

	// gems3500Installed() 에서 사용
	//piom->modType[0] = IOM_COM;

	mdebug = 0;
	_enableTaskMonitor(Tid_GW, 50);	
	
	while (1) {		
		// demand scan
		// gsId: scan block index, 
		// gdId: devive index
		pcntl->wdtTbl[Tid_GW].count++;
		
		genScanReq();
		
		while (getPollReq(&sb) == 0) {
			pcntl->wdtTbl[Tid_GW].count++;
			gdId = psb->id - 1;
			// index로 장치주소 얻는다
			genG3600Frame(psb, mmbtb, &nw, &nr);					
			
			dprtbuffer(psb->id+10, "[MMB_TX]", mmbtb, nw); 
			if (sndRcvMmbFrame2(pid, mmbtb, nw, mmbrb, nr) == nr) {
				dprtbuffer(psb->id+10, "[MMB_RX]", mmbrb, nr); 						
				updateGems3600(psb, &mmbrb[3]);
				if (devCB[gdId].commStatus) {
//					printf("return from com. error(%d)\n", gdId);
				}
				devCB[gdId].commStatus = 0;	// 통신정상
				devCB[gdId].commFailCount = 0;				
			}
			else {			
				//printf("txcnt=%d, rxcnt=%d, err=%.3f\n", txcnt, rxcnt, (txcnt-rxcnt)*100./txcnt);
				if (devCB[gdId].commFailCount < 3) {
					devCB[gdId].commFailCount++;
				}
				else {
					if (devCB[gdId].commStatus == 0) {
						devCB[gdId].commStatus = 1;	// 통신이상
						if (gdId < 2) {
							devCB[gdId].initFlag = 0;
						}
						printf("Gateway_Task : reply timeout (%d)...\n", gdId);						
					}					
				}
			}
        	osDelayTask(5);
		}
        osDelayTask(50);
		
	}
}



uint8_t ttb[32], trb[32];

int pollTemp(int pid, int id, uint16_t *res) {
	int i, n=0, fc=4, s=1000, c=2, nr;
	uint16_t crc;
	
	ttb[n++] = id;
	ttb[n++] = fc;
	ttb[n++] = s>>8;
	ttb[n++] = s;
	ttb[n++] = c>>8;
	ttb[n++] = c;
	
	crc = gencrc_modbus(ttb, n);
	ttb[n++] = crc;
	ttb[n++] = crc>>8;		
	
	nr = c*2 + 5;
	if (sndRcvMmbFrame2(pid, ttb, n, trb, nr) == nr) {
		uint8_t *pb = &trb[3];
		int j=0;	
		
		//dprtbuffer(1, "[Temp Rx]", trb, nr); 		
		for (i=0, j=0; i<c; i++, j+=2) {
			res[i] = pb[j] << 8 | pb[j+1];			
		}
		return 0;
	}
	else {
		return -1;
	}
}

// temp sensor scan task 

void TempScan_Task(void *arg)
 {
	int pid=2, id;
	uint8_t busId[]={1,2,3,4};
	uint16_t tinf[2];
	uint32_t ts1s = sysTick1s;
	
	// UART Init, RS485 port
	UART_Init(pid, 9600);	
	
	printf("[Temp. Sensor Scan Task]\n");
	piom->modType[1] = 3;	// RTD_1
	
	while (1) {		
		if (sysTick1s == ts1s) {
         osDelayTask(100);
			continue;
		}			
		
		ts1s = sysTick1s;
		
		for (id=0; id<4; id++) {
			if (pollTemp(pid, busId[id], tinf) == 0) {
				float fv = tinf[0];			
				switch (tinf[1]) {
				case 0:
					piom->io.aiData[id] = (float)fv;
					break;
				case 1:
					piom->io.aiData[id] = (float)fv/10.;
					break;
				case 2:
					piom->io.aiData[id] = (float)fv/100.;
					break;
				case 3:
					piom->io.aiData[id] = (float)fv/1000.;
					break;
				}								
				//printf("temp(%d) -> %f\n", id, piom->io[0].aiData[id]);
			}
			osDelayTask(100);
		}
		// 온도를 meter 영역으로 복사한다 
		memcpy(pmeter->aim, piom->io.aiData, sizeof(pmeter->aim)/2);
	}	
}

#ifdef	FLOW_RATE
int pollFlow(int pid, int id, uint16_t *res) {
	int i, n=0, fc=3, s=28673, c=8, nr;
	uint16_t crc;
	
	ttb[n++] = id;
	ttb[n++] = fc;
	ttb[n++] = s>>8;
	ttb[n++] = s;
	ttb[n++] = c>>8;
	ttb[n++] = c;
	
	crc = gencrc_modbus(ttb, n);
	ttb[n++] = crc;
	ttb[n++] = crc>>8;		
	
	nr = c*2 + 5;
	if (sndRcvMmbFrame2(pid, ttb, n, trb, nr) == nr) {
		uint8_t *pb = &trb[3];
		int j=0;	
		
//		dprtbuffer(1, "[Temp Rx]", trb, nr); 		
		for (i=0, j=0; i<c; i++, j+=2) {
			res[i] = pb[j] << 8 | pb[j+1];			
		}
		return 0;
	}
	else {
		return -1;
	}
}

__task void FlowScan_Task(void) {
	int pid=2, id, i;
	uint8_t busId[]={1};
	uint16_t tinf[8], scale;
	uint32_t ts1s = sysTick1s;
	
	// UART Init, RS485 port
	UART_Init(pid, 9600);	
	
	printf("[Flow. Sensor Scan Task]\n");
	piom->modType[1] = 3;	// RTD_1
	
	for(i=0; i<8; i++) {
		pmeter->flowScale[i] = 1;	
	}

	while (1) {		
		if (sysTick1s == ts1s) {
			os_dly_wait(100);
			continue;
		}			
		
		ts1s = sysTick1s;
		
		for (id=0; id<1; id++) {
			if (pollFlow(uId, busId[id], tinf) == 0) {
					for(i=0; i<8; i++) {
						scale = pmeter->flowScale[i];
						pmeter->flowVal[i] = tinf[i]*scale;
//						printf("Flow(%d) -> %d(%d)\n", i, tinf[i], scale);
					}
			}
		}
		os_dly_wait(100);
	}
		
}	
#endif
