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

int	writeSingleMem(int id, uint16_t start, uint16_t cmd);
int	writeMultiMem(uint16_t start, uint16_t count, uint8_t *prx);
uint16_t 	gencrc_modbus(uint8_t * ptr, int len);
void cmd_reboot(char *par);
void copySummary(void);
//int modbusSlvChkFrame(uint8_t fc, uint16_t start, uint16_t count, uint8_t *perrCode);
//int makeExceptFrame(uint8_t addr, uint8_t fc, uint8_t errCode, uint8_t *pBuf);
//int writeMem(uint8_t *prx, uint8_t *ptx, uint16_t start, uint16_t count);
//int	readMem(uint8_t *ptx, uint16_t start, uint16_t count);

#define	APP_NAME	"gems7000_app.bin"
#define	APP_OLD		"gems7000_old.bin"
#define	APP_NEW		"gems7000_new.bin"
#define	BOOT_LOG	"bootlog.bin"

void init_smb(void)
{
	int id;
	
	id=0;
	// gems7000 : 0 ~ 4400
	// gesm3500 #0: 10000 ~ 20400 
	// gems3500 #1: 20400 ~ 30800
	
	// 2017-7-5: gems3500 #2 접근불가 문제 발생, MGEM_DATA -> gems3500 으로 변경	
	smbSize[id][3] = 65000;
	smbSize[id][4] = 65000;
	smbBase[id][3] = pmeter;
	smbBase[id][4] = pmeter;
	
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

void copySummary(void)
{
	int 	i,j,k,fr=0;

	for(i=0; i<2; i++) {
		psmap->ch[i].eh = meter[i].egy.Ereg32[0].eh[0][0];			// import kwh
		psmap->ch[i].P = meter[i].meter.P[3];

		for(j=0; j<3; j++) {
			psmap->ch[i].U[j] = meter[i].meter.U[j];
			psmap->ch[i].I[j] = meter[i].meter.I[j];
		}
		psmap->ch[i].Ig = meter[i].meter.Ig;				// Ig

		for(j=0; j<3; j++) {
			psmap->ch[i].THD_U[j] = meter[i].meter.THD_U[j];
			psmap->ch[i].THD_I[j] = meter[i].meter.THD_I[j];
			psmap->ch[i].TDD_I[j] = meter[i].meter.TDD_I[j];
		}
	}
}



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
		if(start < 10000) {
			if (longFrame) 
				size = readMem2(&ptx[inx], start, count);
			else {
				if (start == MBAD_G7_SETTING) {
					printf("read setting #1, s=%d, c=%d\n", start, count);
				}
				size = readMem(&ptx[inx], start, count);		
			}
		}
		else {
			if (longFrame) 
				size = readMem4(&ptx[inx], start-10000, count);
			else {
				if (start == (MBAD_G7_SETTING+10000)) {
					printf("read setting #2, s=%d, c=%d\n", start, count);
				}
				size = readMem3(&ptx[inx], start-10000, count);		
			}
		}
		break; 
	
	// 제어 Command로 사용
	case 6:
		{
			int id;

			if (start < 10000) {
				id = 0;
			}
			else {
				id = 1;
				start -= 10000;
			}
			size = writeSingleMem(id, start, count);			
			ptx[inx++] = start >> 8;
			ptx[inx++] = start;
			ptx[inx++] = count >> 8;
			ptx[inx++] = count;
		}
		break;
		
	// Setting 변경용으로 사용
	case 16:
		if (start >= MBAD_G7_SETTING && (start+count) <= MBAD_G7_CMD) {
			size = writeMultiMem(start, count, &prx[7]);				
			ptx[inx++] = start >> 8;
			ptx[inx++] = start;
			ptx[inx++] = count >> 8;
			ptx[inx++] = count;
		}
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
	uint16_t    *psmb = (uint16_t *)pmeter;
	
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
	uint16_t    *psmb = (uint16_t *)pmeter;

//	if (start == SMB_WAVE) {
//		buildWV16();
//	}
	// Wave 데이터를 load 한다 
	if (start == 2300) {
		copyModbusWaveData();
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
	uint16_t    *psmb = (uint16_t *)&meter[1].meter;
	
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
	uint16_t    *psmb;

	psmb = (uint16_t *)&meter[1].meter;

	ptx[inx++] = count << 1;  // bc = count * 2
	for (i=0; i<count; i++)  {
		ptx[inx++] = psmb[start+i] >> 8;
		ptx[inx++] = psmb[start+i];
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
//	if (start >= IOM_DO_BASE0 && start < (IOM_DO_BASE0+8)) {
//		putCommandS(start, count);
//	}
//	else if (start >= IOM_DO_BASE1 && start < (IOM_DO_BASE1+8)) {
//		putCommand(start, count, pcmd);
//	}	
//	// Write Single Hold Register
//	// setting

//	// Command Area	
//	else if (start >= MBAD_G7_CMD && start < MBAD_G7_CMD) {
//		putCommand(start, count, pcmd);
//	}		
//}

void initCmdQ(OsTaskId tid) 
{
	cmdQ.tid = tid;
}

void putCmdQ(int id, int s, int c) {	
	cmdQ.item[cmdQ.fr].id = id;
	cmdQ.item[cmdQ.fr].s = s;
	cmdQ.item[cmdQ.fr].c = c;
	cmdQ.fr = (cmdQ.fr+1) % N_CMD_Q;
#ifdef __FREERTOS	
	xTaskNotify(cmdQ.tid, 0x08, eSetBits);
#else
	os_evt_set(0x8, cmdQ.tid);
#endif	
}

int getCmdQ(int *id, int *s, int *c) {
	if (cmdQ.fr == cmdQ.re) 
		return -1;
	
	*id = cmdQ.item[cmdQ.re].id;
	*s = cmdQ.item[cmdQ.re].s;
	*c = cmdQ.item[cmdQ.re].c;
	cmdQ.re = (cmdQ.re+1) % N_CMD_Q;
	
	return 0;
}

// *prx : start of data (bc 다음 부터 )
int	writeSingleMem(int id, uint16_t start, uint16_t cmd)
{
	uint16_t    i, inx = 0, six=0;
	
	if ((start >= IOM_DO_BASE0 && start <(IOM_DO_BASE0+8)) || 
			(start >= IOM_DO_BASE1 && start <(IOM_DO_BASE1+8)) || 
			(start >= MBAD_G7_CMD && start < MBAD_G7_END))
	{	
		switch (start) {
			case 7370:
				fetchEvent(cmd);
				break;
			case 7371:
				fetchAlarm(cmd);
				break;
			case 7372:
				fetchItic(cmd);
				break;
			case 7373:
				fetchItic2(cmd);
				break;
			default:
				putCmdQ(id, start, cmd);
				break;
		}
	}
	return 0;
}

int	writeMultiMem(uint16_t start, uint16_t count, uint8_t *pcmd)
{
	
	// setting 영역에 바로 쓰면 어느부분이 변경되었는지 알수 없기때문에
	// 영역별로 따로 처리한다.
	if(start > ADD_ADE9000)	{
		if (start >= (MBAD_G7_SETTING+ADD_ADE9000) && start < (MBAD_G7_CMD+ADD_ADE9000)) {
			printf("recv Settings, s=%d, c=%d ...\n", start, count);
			putSettings(start-(MBAD_G7_SETTING+ADD_ADE9000), count, pcmd, (uint16_t *)pdbk2);
		}
	}
	else {
		if (start >= MBAD_SET_TS && count == 2) {
#if 1	// 데이터 형식을 다른 데이터와 동일하게 한다	
			uint32_t utc = pcmd[0]<<24 | pcmd[1]<<16 | pcmd[2]<<8 | pcmd[3];
			printf("recv UTC Time, s=%d, c=%d, UTC=%d ...\n", start, count, utc);
#endif		
			putUTCtime(start-MBAD_SET_TS, count, pcmd);
		}		
		else if (start >= MBAD_G7_SETTING && start < MBAD_G7_CMD) {
			printf("recv Settings, s=%d, c=%d ...\n", start, count);
			putSettings(start-MBAD_G7_SETTING, count, pcmd, (uint16_t *)pdbk);
		}
		return 0;
	}
}

int   readMemCb(uint16_t address, uint16_t *value) 
{
	int id;
	// Wave 데이터를 load 한다 
	if (address == 2300) {
		copyModbusWaveData();
	}

//	pInfo->MbusHeartBit++;
	//printf("MbusHeartBit = %d\n", pInfo->MbusHeartBit);

	if (address >= 0 && address < 10000) {
		uint16_t    *psmb = (uint16_t *)pmeter;
		id = 0;
		*value = psmb[address];
		return 0;
	}
	else if(address >= 10000 && address < 20000) {
		uint16_t    *psmb = (uint16_t *)&meter[1].meter;
		id = 1;
		*value = psmb[address-10000];
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
	else {
		return -1;
	}
}

int writeMemCb(uint16_t address, uint16_t value) {
   	static uint32_t _utc;   
   	uint16_t *uptr = (uint16_t *)&_utc;
   	uint16_t *psmb = (uint16_t *)pdbk;
   
	// meter 2
	if(address > ADD_ADE9000) {
		psmb = (uint16_t *)pdbk2;
		psmb[address-ADD_ADE9000] = value;
	}
	else {
		if (address == MBAD_SET_TS) {
			uptr[0] = value;
		 }
		 else if (address == MBAD_SET_TS+1) {
			uptr[1] = value;
			printf("recv UTC Time=%d ...\n", _utc);
			tickSet(_utc, 0, 1);		
			RTC_SetTimeUTC(_utc);
		 }
		 else if (address >= MBAD_G7_SETTING && address < MBAD_G7_CMD) {
		  //printf("writeMemCb Settings, s=%d, v=%d ...\n", address, value);
			psmb[address-MBAD_G7_SETTING] = value;
	  	}
	  	else if((address >= IOM_DO_BASE0 && address <(IOM_DO_BASE0+8)) || 
		  (address >= MBAD_G7_CMD && address < MBAD_G7_END)) {
		  printf("putCmdQ Settings, s=%d, v=%d ...\n", address, value);
			  
		  switch (address) {
			  case 7370:
				  fetchEvent(value);
				  break;
			  case 7371:
				  fetchAlarm(value);
				  break;
			  case 7372:
				  fetchItic(value);
				  break;
			  case 7373:
				  fetchItic2(value);
				  break;
			  default:
				  putCmdQ(0, address, value);
				  break;
		  	}
	  	}
  
	}
	// else if(address >= MBAD_G7_END1 && address < MBAD_G7_END2) {
	// 	ackIOEvent(address, value);
	// 	printf("INT DI Ack rcv = [%d][0x%x]\n", address, value);	
	// }
	// else if(address >= MBAD_SET_DI && address <= MBAD_SET_DI_END) {
	//  	uint16_t *psmb = (uint16_t *)pdb2;
	// 	psmb[address-MBAD_SET_DI] = value;
	//  	return 0;
	// }
//	else if (address >= IOM_DO_BASE0 && address < (IOM_DO_BASE0+8)) {
//		sendExtIOMControl(0, address-IOM_DO_BASE0, value);
//	}
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