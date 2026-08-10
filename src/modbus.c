#include "os.h" 
#include "meter.h"
#include "stdio.h"
#include "modbus.h"
#include "smb_rtu.h"
#include "string.h"
#include "crc.h"
#include "debug.h"

uint8_t filebuf[0x80000] __attribute__ ((section ("EXT_RAM"), zero_init));

uint16_t	mbus_seq=0;

void *smbBase[5][5];
int smbSize[5][5];
//uint16_t	cmdQ[16];	// flag, addr, cmds
CMD_Q	cmdQ;

// fw download, 2017-2-17
int fwSize, fwCRC, fwDnldFlag, fwApplyFlag;

extern uint8_t bin_mem[], bout_mem[];
extern uint16_t	sdbSize[4];
//extern volatile unsigned long	tick_msec, tick_sec, tick_int;
extern uint8_t	ledflag;
extern void cmdProc();
//extern void sendExtIOMControl(int id, int s, int cmd);
extern void macAddrGet(uint8_t *);

//extern ALARM_SET dbalm;
//extern EXT_MOD_CFG dbExt;

//extern MGEM_DATA  gm35_Full[2];
//extern MGEM_ONION_DATA gm35_Small[2];

int	writeSingleMem(uint16_t start, uint16_t cmd);
int	writeMultiMem(uint16_t start, uint16_t count, uint8_t *prx);
uint16_t 	gencrc_modbus(uint8_t * ptr, int len);
void cmd_reboot(char *par);
//int modbusSlvChkFrame(uint8_t fc, uint16_t start, uint16_t count, uint8_t *perrCode);
//int makeExceptFrame(uint8_t addr, uint8_t fc, uint8_t errCode, uint8_t *pBuf);
//int writeMem(uint8_t *prx, uint8_t *ptx, uint16_t start, uint16_t count);
//int	readMem(uint8_t *ptx, uint16_t start, uint16_t count);

#if METER_CH_COUNT > 2
int	readMem4_m3(uint8_t *ptx, uint16_t start, uint16_t count);
int	readMem3_m3(uint8_t *ptx, uint16_t start, uint16_t count);
#endif

#define	APP_NAME	"gems7000_app.bin"
#define	APP_OLD		"gems7000_old.bin"
#define	APP_NEW		"gems7000_new.bin"
#define	BOOT_LOG	"bootlog.bin"

void init_smb(void)
{
	int id, fc;

	/* clear table first */
	memset(smbBase, 0, sizeof(smbBase));
	memset(smbSize, 0, sizeof(smbSize));

	/*
	 * Channel map
	 *  - id 0 : meter[0] (0~9999)
	 *  - id 1 : meter[1] (10000~19999)
	 *  - id 2 : meter[2] (20000~29999, CH3)
	 *
	 * Current frame checker still uses addr=0 for legacy path,
	 * but keep per-channel table populated for readability and
	 * future addr-based dispatch.
	 */
	for (id = 0; id < METER_CH_COUNT; id++) {
		for (fc = 3; fc <= 4; fc++) {
			smbSize[id][fc] = 65000;
			smbBase[id][fc] = (void *)&meter[id].meter;
		}
	}
	
//	// gems3500 Full Map
//	id=1;
//	smbSize[id][3] = sizeof(MGEM_DATA)/2;
//	smbSize[id][4] = sizeof(MGEM_DATA)/2;
//	smbBase[id][3] = &gm35_Full[0];
//	smbBase[id][4] = &gm35_Full[0];
//	

//	id=2;
//	smbSize[id][3] = sizeof(MGEM_DATA)/2;
//	smbSize[id][4] = sizeof(MGEM_DATA)/2;
//	smbBase[id][3] = &gm35_Full[1];
//	smbBase[id][4] = &gm35_Full[1];
//		
//	// gems3500 Small Map
//	id=3;	
//	smbSize[id][3] = sizeof(MGEM_ONION_DATA)/2;
//	smbSize[id][4] = sizeof(MGEM_ONION_DATA)/2;
//	smbBase[id][3] = &gm35_Small[0];
//	smbBase[id][4] = &gm35_Small[0];

//	id=4;
//	smbSize[id][3] = sizeof(MGEM_ONION_DATA)/2;
//	smbSize[id][4] = sizeof(MGEM_ONION_DATA)/2;	
//	smbBase[id][3] = &gm35_Small[1];
//	smbBase[id][4] = &gm35_Small[1];	
}

//uint16_t modbusSlvProcFrame(uint8_t *prx, uint16_t rxsize, uint8_t *ptx, int proto)
//{
//	uint16_t 		start, count;
//	uint16_t		size=0, inx=0, len;
//	uint8_t			fc, addr, exceptCode;

//	addr	= prx[0];
//	if (addr == 0) {
//		printf("modbusSlvProcFrame, bad address(%d)\n", addr);
//		return 0;
//	}
//	
//	addr -= 1;
//	fc 		= prx[1];
//	start = (uint16_t)(prx[2]<<8) + prx[3];
//  count = (uint16_t)(prx[4]<<8) + prx[5];

//	len = start + count;
//	if (modbusSlvChkFrame(addr, fc, start, count, &exceptCode, proto) == ERROR) {
//		printf("modbusSlvProcFrame, bad frame(%d, %d)\n", addr, exceptCode);
//		return makeExceptFrame(addr, fc, exceptCode, ptx);
//	}

//	ptx[inx++] = addr+1;
//	ptx[inx++] = fc;

//	switch(fc)
//	{
//	case 3:
//	case 4:
//		if (smbBase[addr][fc]) {
//			if (proto) 
//				size = readMem2(&ptx[inx], start, count, smbBase[addr][fc]);
//			else
//				size = readMem(&ptx[inx], start, count, smbBase[addr][fc]);
//		}
//		else
//			printf("mem not assigned ...\n");
//		break;
//	
//	case 6:
//		if (smbBase[addr][fc]) {
//			size = writeMem(&prx[4], &ptx[inx], start, 1, smbBase[addr][3]);
//		}
//		break;
//		
//	case 16:
//		if (smbBase[addr][fc]) {
//			size = writeMem(&prx[7], &ptx[inx], start, count, smbBase[addr][3]);	
//		}
//		break;
//		
//	default:   
//		break;
//	}

//	return (inx+size);
//}

/* SMP_MAP(psmpMap) 갱신 — memorymap '#1,2,3 Simple' 시트.
 *  buildFeederMap()으로 논리 피더 ch[f]를 물리 미터/CT슬롯에 배정해 복사.
 *  단상 피더: ADC_REDIRECT로 전압이 이미 해당 상으로 정합 → 슬롯 인덱스 하나로 U/I/P 읽음.
 *  PostScan_Task(HIGH)에서 500ms 주기 호출. */
static void smpFillCommon(SMP_MAP *dst)
{
	SMP_COMMON *c = &dst->common;
	METER_INFO *pi = &meter[0].info;

	c->utc         = meter[0].meter.utc;
	c->heartBit    = pi->HeartBit;
	c->hwModel     = pi->hwModel;
	c->hwVer       = pi->hwVer;
	c->fwVer       = pi->fwVer;
	c->fwBuildYear = pi->fwBuildYear;
	c->fwBuildMon  = pi->fwBuildMon;
	c->fwBuildDay  = pi->fwBuildDay;
	c->serialNum   = pi->sn[0];
	c->temp        = meter[0].meter.Temp;
}

static void smpFill3ph(SMP_CHANNEL *d, SMP_HARMONICS *h, int m)
{
	METERING *pm = &meter[m].meter;
	ENERGY_REG32 *e = meter[m].egy.Ereg32;
	int j, o;

	d->Freq = pm->Freq;
	for (j = 0; j < 3; j++) {
		d->U[j]     = pm->U[j];
		d->Upp[j]   = pm->Upp[j];
		d->I[j]     = pm->I[j];
		d->P[j]     = pm->P[j];
		d->THD_U[j] = pm->THD_U[j];
		d->THD_I[j] = pm->THD_I[j];
		d->TDD_I[j] = pm->TDD_I[j];
	}
	d->U[3]   = pm->U[3];		/* 상전압 평균 */
	d->Upp[3] = pm->Upp[3];		/* 선간전압 평균 */
	d->I[3]   = pm->I[3];		/* 상전류 평균 (cbStatus 판정 기준) */
	d->In    = pm->In;
	d->Ptot  = pm->P[3];
	d->Qtot  = pm->Q[3];
	d->Stot  = pm->S[3];
	d->PFtot = pm->PF[3];
	d->Uunb  = pm->Ubal[0];
	d->Iunb  = pm->Ibal[0];

	d->kWh_imp       = EGY_TOTAL(e[EGY_PERIOD_TOTAL],      EGY_MODE_KWH,   EGY_SIGN_IMPORT);
	d->kWh_imp_thisM = EGY_TOTAL(e[EGY_PERIOD_THIS_MONTH], EGY_MODE_KWH,   EGY_SIGN_IMPORT);
	d->kWh_imp_lastM = EGY_TOTAL(e[EGY_PERIOD_LAST_MONTH], EGY_MODE_KWH,   EGY_SIGN_IMPORT);
	d->kVARh_imp     = EGY_TOTAL(e[EGY_PERIOD_TOTAL],      EGY_MODE_KVARH, EGY_SIGN_IMPORT);
	d->kVAh          = EGY_TOTAL(e[EGY_PERIOD_TOTAL],      EGY_MODE_KVAH,  EGY_SIGN_IMPORT);

	for (j = 0; j < 3; j++)
		for (o = 0; o < SMP_HARM_ORDER; o++) {
			h->U[j][o] = (int16_t)meter[m].hd.U[j][o];
			h->I[j][o] = (int16_t)meter[m].hd.I[j][o];
		}
}

/* 단상 피더: 측정 원본은 CT 슬롯(slot), 저장 위치는 wiring 상(vphase).
 *  WM_1LN1CT_L1→[0], L2→[1], L3→[2] 자리에 저장(나머지 상 인덱스는 0 유지). */
static void smpFill1ph(SMP_CHANNEL *d, SMP_HARMONICS *h, int m, int slot, int vphase)
{
	METERING *pm = &meter[m].meter;
	ENERGY_REG32 *e = meter[m].egy.Ereg32;
	int o;
	int p = vphase;				/* 목적지 상 인덱스 0/1/2 */

	d->Freq  = pm->Freq;
	d->U[p]  = pm->U[slot];		/* redirect로 U[slot]가 해당 상 전압 */
	d->I[p]  = pm->I[slot];
	d->P[p]  = pm->P[slot];
	d->Ptot  = pm->P[slot];
	d->Qtot  = pm->Q[slot];
	d->Stot  = pm->S[slot];
	d->PFtot = pm->PF[slot];
	d->THD_U[p] = pm->THD_U[slot];
	d->THD_I[p] = pm->THD_I[slot];
	d->TDD_I[p] = pm->TDD_I[slot];
	d->U[3] = pm->U[slot];		/* 단상: 해당 상전압이 곧 평균 */
	d->I[3] = pm->I[slot];		/* 단상: 해당 상전류가 곧 평균 (cbStatus 기준) */
	/* Upp[3](선간전압 평균)·나머지 상 인덱스·In/Uunb/Iunb 는 0 유지 */

	d->kWh_imp       = EGY_PHASE(e[EGY_PERIOD_TOTAL],      EGY_MODE_KWH,   EGY_SIGN_IMPORT, slot);
	d->kWh_imp_thisM = EGY_PHASE(e[EGY_PERIOD_THIS_MONTH], EGY_MODE_KWH,   EGY_SIGN_IMPORT, slot);
	d->kWh_imp_lastM = EGY_PHASE(e[EGY_PERIOD_LAST_MONTH], EGY_MODE_KWH,   EGY_SIGN_IMPORT, slot);
	d->kVARh_imp     = EGY_PHASE(e[EGY_PERIOD_TOTAL],      EGY_MODE_KVARH, EGY_SIGN_IMPORT, slot);
	d->kVAh          = EGY_PHASE(e[EGY_PERIOD_TOTAL],      EGY_MODE_KVAH,  EGY_SIGN_IMPORT, slot);

	for (o = 0; o < SMP_HARM_ORDER; o++) {
		h->U[p][o] = (int16_t)meter[m].hd.U[slot][o];
		h->I[p][o] = (int16_t)meter[m].hd.I[slot][o];
	}
}

void copySimpleMap(void)
{
	FEEDER_MAP map[MAX_CH];
	SMP_MAP *dst;
	int f;

	/* 읽는 쪽이 현재 보고 있지 않은 뒷버퍼를 고른다 */
	dst = (psmpMap == &smpMapBuf[0]) ? &smpMapBuf[1] : &smpMapBuf[0];

	buildFeederMap(map);
	smpFillCommon(dst);

	for (f = 0; f < MAX_CH; f++) {
		SMP_CHANNEL *d = &dst->ch[f];
		SMP_HARMONICS *h = &dst->harm[f];

		memset(d, 0, sizeof(*d));
		memset(h, 0, sizeof(*h));

		if (!map[f].valid)
			continue;
		if (map[f].is3ph)
			smpFill3ph(d, h, map[f].meter);
		else
			smpFill1ph(d, h, map[f].meter, map[f].slot, map[f].vphase);

		/* cbType = 이 피더의 wiring 설정 */
		d->cbType = pdb->pt[f].wiring;
		/* cbStatus = 평균 상전류(I[3])가 시동전류 이상이면 1(ONLINE), 아니면 0(OFFLINE) */
		d->cbStatus = (d->I[3] >= meter[map[f].meter].cntl.I_start) ? 1 : 0;
	}

	/* 완결된 스냅샷을 원자적으로 발행 (volatile 포인터 단일 스토어) */
	psmpMap = dst;
}


// 미사용
// smb에서 호출
// 2017-8-16, modbusSlvChkFrame호출시 addr를 0으로 설정
int modbusSlvProcFrame(uint8_t *prx, uint16_t rxsize, uint8_t *ptx, int longFrame)
{
	uint16_t 		start, count, cmd;
	uint16_t		size=0, inx=0;
	uint8_t			fc, exceptCode;
	int i;

// 2017-8-16, 삭제 
//	addr	= prx[0];
//	if (addr == 0) {
//		printf("modbusSlvProcFrame, bad address(%d)\n", addr);
//		return 0;
//	}
//			
//	addr -= 1;

	fc 		= prx[1];
	start = (uint16_t)(prx[2]<<8) + prx[3];
  	count = (uint16_t)(prx[4]<<8) + prx[5];

	if (modbusSlvChkFrame(0, fc, start, count, &exceptCode, longFrame) == ERROR) {
		printf("modbusSlvProcFrame, bad frame(%d, %d)\n", prx[0], exceptCode);
		dprtbuffer(9, "[ERR]", prx, rxsize);
		return makeExceptFrame(prx[0], fc, exceptCode, ptx);
	}

	// 2017-8-16
	//ptx[inx++] = addr+1;
	ptx[inx++] = prx[0];
	ptx[inx++] = fc;	

	switch(fc)
	{
	case 3:
	case 4:
		if (start < ADD_ADE9000_M2) {
			if (longFrame) 
				size = readMem2(&ptx[inx], start, count);
			else {
				if (start == MBAD_SETTING) {
					printf("read setting #1, s=%d, c=%d\n", start, count);
				}
				size = readMem(&ptx[inx], start, count);		
			}
		}
#if METER_CH_COUNT > 2
		else if (start < ADD_ADE9000_M3) {
			if (readRangeBlocked(start, count))
				return makeExceptFrame(prx[0], fc, 2, ptx);
			if (longFrame) 
				size = readMem4(&ptx[inx], start - ADD_ADE9000_M2, count);
			else
				size = readMem3(&ptx[inx], start - ADD_ADE9000_M2, count);
		}
		else if (start < 30000) {
			if (readRangeBlocked(start, count))
				return makeExceptFrame(prx[0], fc, 2, ptx);
			if (longFrame) 
				size = readMem4_m3(&ptx[inx], start - ADD_ADE9000_M3, count);
			else
				size = readMem3_m3(&ptx[inx], start - ADD_ADE9000_M3, count);
		}
#else
		else if (start < ADD_ADE9000_M3) {
			if (readRangeBlocked(start, count))
				return makeExceptFrame(prx[0], fc, 2, ptx);
			if (longFrame) 
				size = readMem4(&ptx[inx], start - ADD_ADE9000_M2, count);
			else
				size = readMem3(&ptx[inx], start - ADD_ADE9000_M2, count);
		}
#endif
		else if (start >= ADD_SIMPLE_MAP &&
		         (start + count) <= ADD_SIMPLE_MAP + (uint16_t)(sizeof(SMP_MAP)/sizeof(uint16_t))) {
			size = readMemSmp(&ptx[inx], start - ADD_SIMPLE_MAP, count, longFrame);
		}
		else {
			return makeExceptFrame(prx[0], fc, 2, ptx);
		}
		break;
	
	// FC6: CH0 명령 또는 M1/M2 RW 포켓(MBAD_M1_RW_* / MBAD_M2_RW_*)
	case 6:
		if (start < ADD_ADE9000_M2) {
			size = writeSingleMem(start, count);
		}
		else if (start >= MBAD_M1_RW_START && start < MBAD_M1_RW_END) {
			if (writeMeterHoldReg(1, start - ADD_ADE9000_M2, count) < 0)
				return makeExceptFrame(prx[0], fc, 2, ptx);
		}
#if METER_CH_COUNT > 2
		else if (start >= MBAD_M2_RW_START && start < MBAD_M2_RW_END) {
			if (writeMeterHoldReg(2, start - ADD_ADE9000_M3, count) < 0)
				return makeExceptFrame(prx[0], fc, 2, ptx);
		}
#endif
		else {
			return makeExceptFrame(prx[0], fc, 2, ptx);
		}
		ptx[inx++] = start >> 8;
		ptx[inx++] = start;
		ptx[inx++] = count >> 8;
		ptx[inx++] = count;
		break;
		
	// FC16: CH0 P2~P4 설정(6700~7049)·UTC(7446) 또는 M1/M2 RW 포켓
	case 16:
		if (start >= MBAD_SETTING_M2)
			return makeExceptFrame(prx[0], fc, 2, ptx);
		if (start >= MBAD_SETTING && (start + count) <= MBAD_SET_CMD) {
			size = writeMultiMem(start, count, &prx[7]);
		}
		else if (start >= MBAD_M1_RW_START && (start + count) <= MBAD_M1_RW_END) {
			if (writeMeterHoldRegs(1, start - ADD_ADE9000_M2, count, &prx[7]) < 0)
				return makeExceptFrame(prx[0], fc, 2, ptx);
		}
#if METER_CH_COUNT > 2
		else if (start >= MBAD_M2_RW_START && (start + count) <= MBAD_M2_RW_END) {
			if (writeMeterHoldRegs(2, start - ADD_ADE9000_M3, count, &prx[7]) < 0)
				return makeExceptFrame(prx[0], fc, 2, ptx);
		}
#endif
		else {
			return makeExceptFrame(prx[0], fc, 2, ptx);
		}
		ptx[inx++] = start >> 8;
		ptx[inx++] = start;
		ptx[inx++] = count >> 8;
		ptx[inx++] = count;
		break;
		
	default:   
		break;
	}

	return (inx+size);
}

// 2018-1-26 
//  MAX PDU : FC + DATA_SIZE = 253(FC + BC + DATA(250))
//	: data dump size 크기 조정: 123 -> 125
//  : multiple register : 124 -> 123
int modbusSlvChkFrame(uint8_t addr, uint8_t fc, uint16_t start, uint16_t count, uint8_t *perrCode, int longFrame)
{
	uint8_t exceptCode=0;
	
	addr = 0;	// 2017-8-16

	switch (fc) {
	case 1:
	case 2:
		exceptCode = 1;
		break;

	case 3:
	case 4:
		if ((longFrame == 0 && count > 125) || (longFrame != 0 && count > 1024))
			exceptCode = 3;
		else if ((start+count) > smbSize[addr][fc])
			exceptCode = 2;		
		break;
		
	case 5:
		exceptCode = 1;
		break;
		
	case 6:
		if (start >= smbSize[addr][3])
			exceptCode = 3;
		break;

	case 16:
		if(count < 1 || count > 123)
     	exceptCode = 3;
		else if ((start+count) > smbSize[addr][3])
			exceptCode = 2;
    break;

	default:
		exceptCode = 1;
		break;
	}

	*perrCode = exceptCode;
	if (exceptCode > 0)
		return 0;
	else
		return 1;	
}

int makeExceptFrame(uint8_t addr, uint8_t fc, uint8_t errCode, uint8_t *pBuf)
{
	int inx=0;

	pBuf[inx++] = addr;
	pBuf[inx++] = fc + 0x80;
	pBuf[inx++] = errCode;

	return inx;
}

void dprtbuffer(int dbgF, char *title, uint8_t *pbuf, int count)
{
	int i, cc;
	
	if (dbgF != ntDebugLevel) return;

	printf("%s\n\r", title);
	cc = 0;
	for (i=0; i<count; i++) {
		printf("%02x ", pbuf[i]);
		if (++cc == 16) {
			printf("\n\r");
			cc = 0;
		}
	}
	printf("\n\r");
}

static uint16_t *getMeterRegBaseById(int id) {
	if (id < 0 || id >= METER_CH_COUNT) {
		return NULL;
	}
	return (uint16_t *)&meter[id].meter;
}

static int meterRwRangeValid(uint16_t offset, uint16_t count)
{
	/* Energy 영역(260807 맵: R/W) — U32 누적값 구간만 허용([START, U64_START)) */
	if (offset >= MBAD_ENERGY_START &&
	    (uint32_t)offset + count <= MBAD_ENERGY_U64_START)
		return 1;
	if (offset < MBAD_RW_OFF_START)
		return 0;
	if ((uint32_t)offset + count > MBAD_RW_OFF_END)
		return 0;
	return 1;
}

static int writeMeterHoldReg(int id, uint16_t offset, uint16_t value)
{
	uint16_t *psmb = getMeterRegBaseById(id);

	if (psmb == NULL || !meterRwRangeValid(offset, 1))
		return -1;
	psmb[offset] = value;
	return 0;
}

static int writeMeterHoldRegs(int id, uint16_t offset, uint16_t count, uint8_t *pcmd)
{
	uint16_t *psmb = getMeterRegBaseById(id);
	uint8_t *pbuf;
	int i;

	if (psmb == NULL || pcmd == NULL || !meterRwRangeValid(offset, count))
		return -1;

	pbuf = (uint8_t *)(psmb + offset);
	for (i = 0; i < count; i++) {
		*pbuf++ = *(pcmd + 1);
		*pbuf++ = *pcmd;
		pcmd += 2;
	}
	return 0;
}

/* M1/M2 read: ≥17220/27220 또는 그 구간을 침범하면 거부 */
static int readRangeBlocked(uint16_t start, uint16_t count)
{
	uint32_t end = (uint32_t)start + count;

	if (start >= ADD_ADE9000_M2 && start < ADD_ADE9000_M3) {
		if (start >= MBAD_METER_END_M2 || end > MBAD_METER_END_M2)
			return 1;
	}
#if METER_CH_COUNT > 2
	if (start >= ADD_ADE9000_M3 && start < 30000) {
		if (start >= MBAD_METER_END_M3 || end > MBAD_METER_END_M3)
			return 1;
	}
#endif
	return 0;
}

static int readMeterChOffsetBlocked(int id, uint16_t offset)
{
	if (id == 1 || id == 2)
		return offset >= MBAD_RW_OFF_END;
	return 0;
}

static int decodeMeterAddress(uint16_t address, int *id, uint16_t *offset) {
	if (address < ADD_ADE9000_M2) {
		*id = 0;
		*offset = address;
		return 0;
	}
#if METER_CH_COUNT > 2
	if (address < ADD_ADE9000_M3) {
		*id = 1;
		*offset = address - ADD_ADE9000_M2;
		return 0;
	}
	if (address < 30000) {
		*id = 2;
		*offset = address - ADD_ADE9000_M3;
		return 0;
	}
#else
	if (address < 20000) {
		*id = 1;
		*offset = address - ADD_ADE9000_M2;
		return 0;
	}
#endif
	return -1;
}

/* fetch: MBAD_CMD_FETCH_* (+67/+77/+107/+110) 베이스+0/1/2 → id 0/1/2 */
static int handleFetchCmd(uint16_t cmdAddr, uint16_t cmd)
{
	int ch;
	/* load event: ALL(base)=전채널, base+1~3=CH1~3 */
	if (cmdAddr >= MBAD_CMD_FETCH_EVENT && cmdAddr <= MBAD_CMD_FETCH_EVENT + METER_CH_COUNT) {
		if (cmdAddr == MBAD_CMD_FETCH_EVENT)
			for (ch = 0; ch < METER_CH_COUNT; ch++) fetchEvent(ch, cmd);
		else
			fetchEvent(cmdAddr - MBAD_CMD_FETCH_EVENT - 1, cmd);
		return 1;
	}
	/* load alarm: ALL(base)=전채널, base+1~3=CH1~3 */
	if (cmdAddr >= MBAD_CMD_FETCH_ALARM && cmdAddr <= MBAD_CMD_FETCH_ALARM + METER_CH_COUNT) {
		if (cmdAddr == MBAD_CMD_FETCH_ALARM)
			for (ch = 0; ch < METER_CH_COUNT; ch++) fetchAlarm(ch, cmd);
		else
			fetchAlarm(cmdAddr - MBAD_CMD_FETCH_ALARM - 1, cmd);
		return 1;
	}
	/* load ITIC / ITIC2: 단일 레지스터(CH1) */
	if (cmdAddr == MBAD_CMD_FETCH_ITIC)  { fetchItic(0, cmd);  return 1; }
	if (cmdAddr == MBAD_CMD_FETCH_ITIC2) { fetchItic2(0, cmd); return 1; }
	return 0;
}

int readBioMem(uint8_t *ptx, uint16_t start, uint16_t count, uint8_t *psmb)
{
	uint16_t    i, inx = 0;
	int bc;
	
	bc=(count-1)/8+1;
	//uint16_t    *psmb = (uint16_t *)pmeter;	
	ptx[inx++] = bc;
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i];
	}
	
	return inx;
}


int	readMem2(uint8_t *ptx, uint16_t start, uint16_t count)
{
	uint16_t    i, inx = 0, c;
	uint16_t    *psmb = getMeterRegBaseById(0);
	
//  capture	
//	if (start == SMB_WAVE) {
//		buildWV16();
//	}

	c = count << 1;
	// length (2 bytes)
	ptx[inx++] = c << 8; 
	ptx[inx++] = c;
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
	}
	
	return inx;
}

int	readMem(uint8_t *ptx, uint16_t start, uint16_t count)
{
	uint16_t    i, inx = 0;	
	uint16_t    *psmb = getMeterRegBaseById(0);

//	if (start == SMB_WAVE) {
//		buildWV16();
//	}
	// Wave 데이터를 load 한다
	if (start == MBAD_WV_REG) {
		copyModbusWaveData(0);
	}

	ptx[inx++] = count << 1;  // bc = count * 2
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
	}
    	
	return inx;
}

int	readMem4(uint8_t *ptx, uint16_t start, uint16_t count)
{
	uint16_t    i, inx = 0, c;
	uint16_t    *psmb = getMeterRegBaseById(1);
	
//  capture	
//	if (start == SMB_WAVE) {
//		buildWV16();
//	}

	c = count << 1;
	// length (2 bytes)
	ptx[inx++] = c << 8; 
	ptx[inx++] = c;
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
	}
	
	return inx;
}

int	readMem3(uint8_t *ptx, uint16_t start, uint16_t count)
{
	uint16_t    i, inx = 0;	
	uint16_t    *psmb = getMeterRegBaseById(1);

	ptx[inx++] = count << 1;  // bc = count * 2
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
	}
    	
	return inx;
}

#if METER_CH_COUNT > 2
int	readMem4_m3(uint8_t *ptx, uint16_t start, uint16_t count)
{
	uint16_t    i, inx = 0, c;
	uint16_t    *psmb = getMeterRegBaseById(2);

	c = count << 1;
	ptx[inx++] = c << 8;
	ptx[inx++] = c;
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
	}

	return inx;
}

int	readMem3_m3(uint8_t *ptx, uint16_t start, uint16_t count)
{
	uint16_t    i, inx = 0;
	uint16_t    *psmb = getMeterRegBaseById(2);

	ptx[inx++] = count << 1;
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
	}

	return inx;
}
#endif

/* #1,2,3 Simple 맵(50000~) 블록 읽기 — offset은 ADD_SIMPLE_MAP 기준.
 *  longFrame=0: 1바이트 바이트수, 아니면 2바이트 길이(readMem2 계열과 동일 포맷). */
int	readMemSmp(uint8_t *ptx, uint16_t offset, uint16_t count, int longFrame)
{
	uint16_t    i, inx = 0, c;
	uint16_t    *psmb = (uint16_t *)psmpMap;

	c = count << 1;
	if (longFrame) {
		ptx[inx++] = c << 8;
		ptx[inx++] = c;
	}
	else {
		ptx[inx++] = c;		// bc = count * 2
	}
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[offset+i] >> 8;
		ptx[inx++] = psmb[offset+i];
	}

	return inx;
}


//void putCommandS(int addr, int count)
//{
//	int i, ix=0;
//	
//	cmdQ.s = addr;
//	cmdQ.c = count;	
//	for (i=0; i<count; i++, ix+=2) {
//		cmdQ.cmdbuf[i] = (uint16_t)pcmd[ix]<<8 | pcmd[ix+1];
//	}	
//	cmdQ.flag = 1;
//	printf("putCommand, addr=%d, count=%d\n", addr, count);

//	cmdProc(&cmdQ);
//}



// write multie register
void putSettings(int addr, int count, uint8_t *psrc, uint16_t *pdst)
{
	uint8_t *pbuf = (uint8_t *)(pdst+addr);
	int i;
	
	for (i=0; i<count; i++)  {
		*pbuf++ = *(psrc+1);
		*pbuf++ = *(psrc);
		psrc += 2;
	}	
}


// 시간동기시 Wave Capture sampling data에 왜곡이 발생한다.
// RTC_SetTimeUTC이 wave capture에 영향미친다 
void putUTCtime(int addr, int count, uint8_t *psrc) {
	uint32_t utc, diff;
#if 1	// 시간만 데이터 형식이 다르다. 다른 데이터 형식만 동일하게 처리
	int i;
	uint8_t *pbuf = (uint8_t *)&utc;
	for (i=0; i<count; i++)  {
		*pbuf++ = *(psrc+1);
		*pbuf++ = *(psrc);
		psrc += 2;
	}	
	printf("recv UTC Time=%d ...\n", utc);
#else	
	utc = (psrc[0]<<24) | (psrc[1]<<16) | (psrc[2]<<8) | psrc[3];	
#endif
	diff = sysTick1s - utc;
	tickSet(utc, 0, 1);	
	
	RTC_SetTimeUTC(utc);
	//printf ("%d seconds elapsed since 1.1.1970, diff=%d\n", utc, diff);
}

void _swap_w(uint8_t *pdst, uint8_t *psrc)
{
	pdst[1] = psrc[0];	// msb
	pdst[0] = psrc[1];	// lsb
}

void _swap_dw(uint8_t *pdst, uint8_t *psrc)
{
	pdst[1] = psrc[0];	// msb
	pdst[0] = psrc[1];	// 
	pdst[3] = psrc[2];	// 
	pdst[2] = psrc[3];	// lsb	
}

#ifdef _FILE_XFER

int checkFwDnld(void)
{
	return fwDnldFlag;
}

int checkFwApply(void)
{
	return fwApplyFlag;
}


//void writeFwFile(void)
//{
//	uint32_t crc32, bnum, bsize;
//	FILE *fp;
//	
//	printf("writeFwFile, size=%d, crc=%x\n", fwSize, fwCRC);
//	crc32 = gencrc_crc32(filebuf, fwSize, 0);
//	if (crc32 == fwCRC) {
//		printf("[EOT, CRC OK]\n");
//		fp = fopen(APP_NEW, "wb");
//		if (fp != NULL) {
//			bnum = 0;
//			while (fwSize > 0) {
//				bsize = (fwSize > 1024) ? 1024 : fwSize;
//				fwrite(&filebuf[bnum*1024], bsize, 1, fp);
//				fwSize -= bsize;
//				bnum++;
//			}
//			fclose(fp);				
//		}
//		printf("[EOT, CRC OK, File Xfer is success(%s)]\n", APP_NEW);		
//	}
//	else {
//		printf("[EOT, CRC Error occured, f=%x, r=%x]\n", fwCRC, crc32);
//	}
//	
//	fwDnldFlag = 0;
//}


//void applyFwFile(void)
//{
//	fdelete(APP_OLD);
//	frename(APP_NAME, APP_OLD);
//	frename(APP_NEW, APP_NAME);
//	writeBootLog(0);
//	fwApplyFlag = 0;
//	cmd_reboot(0);
//}

//// 2017-2-15
//// pbuf: data 영역
//void putFileXfer(int count, uint8_t *pbuf)
//{
//	static uint32_t tsize, lbnum;
//	uint16_t cmd, bnum, bsize;
//	uint32_t crc32;
//	
//	memcpy((uint8_t *)&cmd, &pbuf[0], sizeof(uint16_t));
//	
//	if (cmd == 1) {
//		memcpy((uint8_t *)&fwSize, &pbuf[2], sizeof(uint32_t));
//		memcpy((uint8_t *)&fwCRC, &pbuf[6], sizeof(uint32_t));
//		tsize = 0;
//		memset(filebuf, 0, sizeof(filebuf));
//		printf("[SOT, fsize=%d, fcrc=0x%x]\n", fwSize, fwCRC);
//	}
//	else if (cmd == 2) {
//		memcpy((uint8_t *)&bnum, &pbuf[2], sizeof(uint16_t));
//		memcpy((uint8_t *)&bsize, &pbuf[4], sizeof(uint16_t));		
//		memcpy(&filebuf[bnum*240], &pbuf[6], bsize);
//		tsize += bsize;
//		printf("[XFER, bnum = %d, bsize = %d, tsize = %d]\n", bnum, bsize, tsize);			
//	}
//	else if (cmd == 3) {
//		if (tsize != fwSize) {
//			printf("[EOT, length is different, f=%d, r=%d]\n", fwSize, tsize);			
//		}
//		else {
//			fwDnldFlag = 1;
//			printf("[EOT, success]\n");
//		}
//	}	
//	else if (cmd == 4) {	
//		fwApplyFlag = 1;
//	}
//}


#endif

//void writeBootLog(int code)
//{
//	FILE *fp;

//	printf("writeBootLog, code=%d\n", code);
//	fp = fopen(BOOT_LOG, "wb");
//	if (fp != NULL) {
//		fwrite(&code, sizeof(int), 1, fp);
//		fclose(fp);
//	}
//}


//// modbus memory map 참조, 
//// FC=6 or 16 이면 modbus handler에서 writeMem을 호출한다 
//// start: starting address
//// count: # of word
//// pcnd : data buffer
//void decodeCmd(int start, int count, uint8_t *pcmd)
//{
//	if (count == 0) return;
//	
//	//printf("decodeCmd, s=%d, c=%d\n", start, count);
//	
//	// IOM Control 영역
//	// Write Single Hold Register
//	// setting

//	// Command Area	
//	else if (start >= MBAD_SET_CMD && start < MBAD_SET_CMD) {
//		putCommand(start, count, pcmd);
//	}		
//}

void initCmdQ(OsTaskId tid) 
{
	cmdQ.tid = tid;
}

void putCmdQ(int s, int c) {	
	cmdQ.item[cmdQ.fr].s = s;
	cmdQ.item[cmdQ.fr].c = c;
	cmdQ.fr = (cmdQ.fr+1) % N_CMD_Q;
#ifdef __FREERTOS	
	xTaskNotify(cmdQ.tid, 0x08, eSetBits);
#else
	os_evt_set(0x8, cmdQ.tid);
#endif	
}

int getCmdQ(int *s, int *c) {
	if (cmdQ.fr == cmdQ.re) 
		return -1;
	
	*s = cmdQ.item[cmdQ.re].s;
	*c = cmdQ.item[cmdQ.re].c;
	cmdQ.re = (cmdQ.re+1) % N_CMD_Q;
	
	return 0;
}

// *prx : start of data (bc 다음 부터 )
int	writeSingleMem(uint16_t start, uint16_t cmd)
{
	if (start >= MBAD_SET_CMD && start < MBAD_SET_END)
	{	
		if (handleFetchCmd(start, cmd))
			return 0;

		putCmdQ(start, cmd);
	}
	return 0;
}

int	writeMultiMem(uint16_t start, uint16_t count, uint8_t *pcmd)
{
	// setting 영역에 바로 쓰면 어느부분이 변경되었는지 알수 없기때문에
	// CH0 설정·시간만 처리한다.
	if (start >= MBAD_SET_TS && count == 2 && start < MBAD_SET_CMD) {
#if 1
		uint32_t utc = pcmd[0]<<24 | pcmd[1]<<16 | pcmd[2]<<8 | pcmd[3];
		printf("recv UTC Time, s=%d, c=%d, UTC=%d ...\n", start, count, utc);
#endif
		putUTCtime(start-MBAD_SET_TS, count, pcmd);
		return 0;
	}
	if (start >= MBAD_SETTING && start < MBAD_SETTING_END) {
		printf("recv Settings(M1), s=%d, c=%d ...\n", start, count);
		putSettings(start-MBAD_SETTING, count, pcmd, pmset);
		return 0;
	}
	if (start >= MBAD_P1_SETTING && start < MBAD_P1_SETTING_END) {
		/* P1 설정(device id/comm/PT/CT/ETC/IOM, 7110~7445) — save settings로 db 반영 */
		printf("recv P1 Settings, s=%d, c=%d ...\n", start, count);
		putSettings(start-MBAD_SETTING, count, pcmd, pmset);
		return 0;
	}
	return 0;
}

int   readMemCb(uint16_t address, uint16_t *value) 
{
	int id;
	uint16_t offset;
	uint16_t *psmb;

//	pInfo->MbusHeartBit++;
	//printf("MbusHeartBit = %d\n", pInfo->MbusHeartBit);

	if (decodeMeterAddress(address, &id, &offset) == 0) {
		// Wave 데이터를 load 한다 (CH별 offset == MBAD_WV_REG)
		if (offset == MBAD_WV_REG)
			copyModbusWaveData(id);
		if (readMeterChOffsetBlocked(id, offset))
			return -1;
		psmb = getMeterRegBaseById(id);
		if (psmb == NULL) {
			return -1;
		}
		*value = psmb[offset];
		return 0;
	}
	// else if(address >= 10000 && address < 30000) {
	//  	uint16_t    *psmb = (uint16_t *)&meter.iPSM[0];
	//  	*value = psmb[address-10000];
	//  	return 0;
	// }
	// else if(address >= 30000 && address < 50000) {
	//  	uint16_t    *psmb = (uint16_t *)&meter.iPSM[1];
	//  	*value = psmb[address-30000];
	//  	return 0;
	// }
	// else if(address >= 50000 && address < 51000) {
	//  	uint16_t    *psmb = (uint16_t *)&meter.iPSMDI[0];
	//  	*value = psmb[address-50000];
	//  	return 0;
	// }
	// else if(address >= 51000 && address < 52000) {
	//  	uint16_t    *psmb = (uint16_t *)&meter.iPSMDI[1];
	//  	*value = psmb[address-51000];
	//  	return 0;
	// }
	else if (address >= ADD_SIMPLE_MAP &&
	         address < ADD_SIMPLE_MAP + (uint16_t)(sizeof(SMP_MAP) / sizeof(uint16_t))) {
		/* #1,2,3 Simple 맵(50000~) — copySimpleMap()가 500ms 주기 갱신 */
		*value = ((uint16_t *)psmpMap)[address - ADD_SIMPLE_MAP];
		return 0;
	}
	else {
		return -1;
	}
}

int writeMemCb(uint16_t address, uint16_t value) {
   	static uint32_t _utc;   
   	uint16_t *uptr = (uint16_t *)&_utc;
   	int id;
   	uint16_t offset;

	if (decodeMeterAddress(address, &id, &offset) != 0)
		return -1;

	/* Energy U32 누적값 구간만 R/W(260807 맵) — CH1(id0)/CH2/CH3 egy에 직접 기록 */
	if (offset >= MBAD_ENERGY_START && offset < MBAD_ENERGY_U64_START)
		return (writeMeterHoldReg(id, offset, value) < 0) ? -1 : 0;

	if (id == 1 && offset >= MBAD_RW_OFF_START && offset < MBAD_RW_OFF_END) {
		if (writeMeterHoldReg(1, offset, value) < 0)
			return -1;
		return 0;
	}
#if METER_CH_COUNT > 2
	else if (id == 2 && offset >= MBAD_RW_OFF_START && offset < MBAD_RW_OFF_END) {
		if (writeMeterHoldReg(2, offset, value) < 0)
			return -1;
		return 0;
	}
#endif
	else if (id == 0 && offset == MBAD_SET_TS) {
		uptr[0] = value;
		return 0;
	}
	else if (id == 0 && offset == MBAD_SET_TS+1) {
		uptr[1] = value;
		printf("recv UTC Time=%d ...\n", _utc);
		tickSet(_utc, 0, 1);		
		RTC_SetTimeUTC(_utc);
		return 0;
	}
	else if (id == 0 && offset >= MBAD_SETTING && offset < MBAD_SETTING_END) {
		pmset[offset-MBAD_SETTING] = value;
		return 0;
	}
	else if (id == 0 && offset >= MBAD_P1_SETTING && offset < MBAD_P1_SETTING_END) {
		/* P1 설정(device id/comm/PT/CT/ETC/IOM, 7110~7445) — save settings 명령으로 db 반영·적용 */
		pmset[offset-MBAD_SETTING] = value;
		return 0;
	}
	else if (id == 0 && offset >= MBAD_SET_CMD && offset < MBAD_SET_END) {
		if (handleFetchCmd(offset, value))
			return 0;

		printf("putCmdQ, s=%d, v=%d ...\n", offset, value);
		putCmdQ(offset, value);
		return 0;
	}
	return -1;
	// else if(address >= MBAD_SET_END1 && address < MBAD_SET_END2) {
	// 	ackIOEvent(address, value);
	// 	printf("INT DI Ack rcv = [%d][0x%x]\n", address, value);	
	// }
	// else if(address >= MBAD_SET_DI && address <= MBAD_SET_DI_END) {
	//  	uint16_t *psmb = (uint16_t *)pdb2;
	// 	psmb[address-MBAD_SET_DI] = value;
	//  	return 0;
	// }
}

// 2025-2-26, cskang
void getMeterInfoV0(uint8_t *sb) {
	int i;
	
	if (pdb->comm.dhcpEn) {
		getMeterIpAddr(sb);
	}
	else {
		sb[0] = pdb->comm.ip0[0];
		sb[1] = pdb->comm.ip0[1];
		sb[2] = pdb->comm.ip0[2];
		sb[3] = pdb->comm.ip0[3];
	}
	
	macAddrGet(&sb[4]);
	strcpy((char *)(sb+10), pdb->comm.host);

	memcpy(sb+42, &pdb->comm.tcpPort,2);
}

// 2025-2-26, cskang
void getMeterInfoV1(RESP_NETINF *sb) {
	int i;
	
	strcpy(sb->keystr, "*netinf*");
	//
	if (pdb->comm.dhcpEn) {
		getMeterIpAddr(sb->ip);
	}
	else {
		sb->ip[0] = pdb->comm.ip0[0];
		sb->ip[1] = pdb->comm.ip0[1];
		sb->ip[2] = pdb->comm.ip0[2];
		sb->ip[3] = pdb->comm.ip0[3];		
	}
	//
	macAddrGet(sb->mac);
	//
	memcpy(sb->host, pdb->comm.host, sizeof(sb->host));	
	//
	for (i=0; i<6; i++) {
		sb->sn[i] = pInfo->sn[i];
	}
	sb->hwModel = pInfo->hwModel;
	sb->hwVer = pInfo->hwVer;
	sb->fwVer = pInfo->fwVer;
	sb->fwDate[0] = pInfo->fwBuildYear;
	sb->fwDate[1] = pInfo->fwBuildMon;
	sb->fwDate[2] = pInfo->fwBuildDay;	
}