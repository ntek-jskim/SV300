#include "os_port.h"
#include "board.h"
#include "meter.h"
#include "stdio.h"
#include "ade9000.h"
#include "ssplib.h"
#include "math.h"
#include "fft.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "crc.h"
#include "alarm.h"

#define	FW_VER	0001
#define	FW_BUILD_YEAR 26
#define	FW_BUILD_MON  2
#define	FW_BUILD_DAY  19

#define	SQRT_2	 1.414213562 

//METER_DEF	meter __attribute__ ((section ("EXT_RAM"), zero_init));
METER_DEF	meter[2] __attribute__ ((section ("EXT_RAM"), zero_init));
METER_CAL	meterCal __attribute__ ((section ("EXT_RAM"), zero_init));
SETTINGS	db[2] __attribute__ ((section ("EXT_RAM"), zero_init));
ALARM_LIST	alist __attribute__ ((section ("EXT_RAM"), zero_init));
EVENT_LIST	elist __attribute__ ((section ("EXT_RAM"), zero_init));
ADE9000_REG ade9000[2] __attribute__ ((section ("EXT_RAM"), zero_init));
EN50160 pqrpt __attribute__ ((section ("EXT_RAM"), zero_init));;

SIMPLE_DATA	smap __attribute__ ((section ("EXT_RAM"), zero_init));
SIMPLE_DATA	*psmap=&smap;

//METER_CAL	*pcal=&meter[0].cal;
METER_CAL	*pcal=&meterCal;

SETTINGS	*pdb=&db[0];
SETTINGS	*pdbk=&meter[0].setting;
SETTINGS	*pdb2=&db[1];
SETTINGS	*pdbk2=&meter[1].setting;

METERING 	*pmeter=&meter[0].meter;
CNTL_DATA	*pcntl=&meter[0].cntl;
HARMONICS *pHD=&meter[0].hd;
METER_INFO *pInfo=&meter[0].info;
ENERGY *pEgy=&meter[0].egy;
EN50160 *pRPT=meter[0].rpt;
VQDATA *pVQ=&meter[0].vq;
DEMAND *pdm=&meter[0].dm;
DEMAND_LOG *pdmlog=&meter[0].dlog;	// demand last day log
MAXMIN *pmm=&meter[0].maxmin; 
EXT_IO_DATA *piom=&meter[0].iom;
//QualDataSet *pQds = &meter.qds;
//VarDataSet *pVds = &meter.vds;
QualLogData	*pqLog=&meter[0].qdLog;
//WAVEFORM_L16	*pWFL16 = &meter.wv;
//SAMPLE_BUF *pSp = &meter.sample;
GATEWAY_STATUS *pgwst = &meter[0].gwst;
ALARM_STATUS *palm = &meter[0].alarm;

//ALARM_LIST	*palist=&meter[0].alist;
//EVENT_LIST	*pelist=&meter[0].elist;
ALARM_LIST	*palist=&alist;
EVENT_LIST	*pelist=&elist;

//ITIC_EVT_LIST *piticlist=&meter[0].itic;
//ITIC_EVT_LIST *piticlist2=&meter[0].itic2;		// configurator
//PQ_EVENT_COUNT *ppqEvtCnt=&meter[0].pqEvtCnt;
PQ_EVENT_COUNT pQevt, *ppqEvtCnt=&pQevt;

//ADE9000_REG _ade9000, *pchip=&_ade9000;
TIME_STAMP freezeTod;
//COMM_CFG *pComm=&meter.db.comm;
IO_CFG	*piocfg=&meter[0].db.iom[0];
#if 1	// 2023-9
EVENT_FIFO *pEvtFifo=&meter[0].eventFifo;
ALARM_FIFO *pAlmFifo=&meter[0].alarmFifo;
#endif
LOG_DATA	*pld=&meter[0].log;
ENERGY_LOG *pegylog=meter[0].elog;

//WAVE8k_PGBUF	w8kQ  __attribute__ ((section ("EXT_RAM"), zero_init));
WAVE_PGBUF	wQ[2] __attribute__ ((section ("EXT_RAM"), zero_init));
WAVE_8K_BUF wbFFT8k[2] __attribute__ ((section ("EXT_RAM"), zero_init)); 
WAVE_WINDOW_BLK wvblk[2] __attribute__ ((section ("EXT_RAM"), zero_init)); 
WAVE_HF_CAP wbCap  __attribute__ ((section ("EXT_RAM"), zero_init)); 
WAVE_LF_CAP wbCapLF __attribute__ ((section ("EXT_RAM"), zero_init)); 
//WAVE_LF_CAP wbLF  __attribute__ ((section ("EXT_RAM"), zero_init)); 
//int32_t TrstBuf[3][1280];	// Transient Buffer
RMS_CAP	rmsCap __attribute__ ((section ("EXT_RAM"), zero_init)); 
FAST_RMS_BUF rmsWin __attribute__ ((section ("EXT_RAM"), zero_init)); 
FFT_CZT fftMem  __attribute__ ((section ("EXT_RAM"), zero_init)); 

EVENT_Q	eventQ __attribute__ ((section ("EXT_RAM"), zero_init)); 

#ifdef	DAQ
DAQ_BUF	daq	__attribute__ ((section ("EXT_RAM"), zero_init)); 
#endif

FS_Q	fsQ;
FFT_CZT *pFFT = &fftMem;

OsMutex	mutWF;
extern OsTaskId  tid_fft;
ENERGY_NVRAM 	egyNvr;

//extern volatile uint64_t sysTick64;
//extern uint32_t sysTickDemand, sysTick15m, sysTick1s, sysTick32;
extern int getPQBinIndex(void);
extern void cmdProc(void);
extern void assertEventOutput(int, int);

const uint16_t def_ip[] = {192,168,8,172};
const uint16_t def_sm[] = {255,255,255,0};
const uint16_t def_gw[] = {192,168,8,1};
const uint16_t def_dn[] = {168,126,63,1};


int		saveLockEnergy=0;

int getTzOffset(int tz) {
	//if (tz > 31) tz = 0;
	//return tzTable[tz]*60;
	return tz*60;
}

void putFsQ(FS_MSG *p) {
	memcpy(&fsQ.mQ[fsQ.fr], p, sizeof(*p));
	if (++fsQ.fr >= FS_MSG_CNT) {
		fsQ.fr = 0;
	}
#ifdef __FREERTOS	
   if (fsQ.tid != 0) 
      xTaskNotify(fsQ.tid, 0x1, eSetBits);
#else
	os_evt_set(0x1, fsQ.tid);
#endif	
}

uint16_t getFwVersion()
{
	return pInfo->fwVer;
}

uint8_t getHwVersion()
{
	return pInfo->hwVer;
}

uint8_t getHwModel()
{
	return pInfo->hwModel;
}
uint8_t getHwCh()
{
	int	mask = 1 << 15;

	int result = pInfo->hwModel & mask;

	return result >> 15;
}

void setMeterInfo() {
	int i;
	
	pInfo->sn[0] = (pcal->sn[0] >> 8) & 0xff;
	pInfo->sn[1] = (pcal->sn[0]) & 0xff;
	pInfo->sn[2] = (pcal->sn[1] >> 24) & 0xff;
	pInfo->sn[3] = (pcal->sn[1] >> 16) & 0xff;
	pInfo->sn[4] = (pcal->sn[1] >> 8) & 0xff;
	pInfo->sn[5] = (pcal->sn[1]) & 0xff;
	
	pInfo->hwVer = pcal->hwVer;
	pInfo->hwModel = pcal->hwModel;
	pInfo->fwVer = FW_VER;
	pInfo->fwBuildYear = FW_BUILD_YEAR;
	pInfo->fwBuildMon  = FW_BUILD_MON;
	pInfo->fwBuildDay  = FW_BUILD_DAY;
	
#if 1	// Chiip ID로 mac 주소 변경
	{
		uint32_t uid = Chip_IAP_ReadUID();
		pInfo->mac[0] = (uid >> 24) & 0xff;
		pInfo->mac[1] = (uid >> 16) & 0xff; 
		pInfo->mac[2] = (uid >> 8)  & 0xff; 
		pInfo->mac[3] = uid & 0xff;
	
		pInfo->mac_msb[0] = MAC_MSB0;
		pInfo->mac_msb[1] = MAC_MSB1;
	}
#else
	for (i=0; i<4; i++) {		
		pInfo->mac[i] = pcal->mac[i];
	}
#endif
	printf("  **********************************************\n\n");
	printf("  1.  Display SV300 Version \n");
    printf("  2.  Copyright NTEK System Co.Ltd\n");
    printf("  3.  Version        :  V%02X.%02X\n", FW_VER>>8&0xff, FW_VER&0xff);
    printf("  4.  Revision Date  :  %d/%02d/%02d\n", FW_BUILD_YEAR, FW_BUILD_MON, FW_BUILD_DAY);  
    printf("  **********************************************\n\n");	
#ifdef	GETTHD
    printf("  thd offset :  v[%.1f]/I[%.1f]\n", pcal->v_thd_offset, pcal->i_thd_offset);  
#endif 

}

uint32_t getThresHold(int type)
{
//	if(type==CT_RCT){
//		return (db[id].freq==0)?THR_VAL_RCT_60Hz:THR_VAL_RCT_50Hz;
//	}
//	else{
//		return _thrList[type];
//	}
	return 0;
}

void build_cal_db1(void)
{
	int id=0;
	// Volt.
	meter[id].setting.pt.wiring = 0;	// 3P4W, 3P3W(2), 3P3W(3)
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 100;
	meter[id].setting.ct.CT1 = 100;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1000;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	// db[id].pt.wiring = 0;	// 3P4W, 3P3W(2), 3P3W(3)
	// db[id].pt.freq = 60;	// 60Hz
	// db[id].pt.vnorm = 220;	
	// db[id].pt.PT1 = 220;
	// db[id].pt.PT2 = 220;
	// db[id].ct.inorm = 100;
	// db[id].ct.CT1 = 100;
	// db[id].ct.CT2 = 1;		// 5A CT
	// db[id].ct.zctScale = 1000;	// 2nd scale
	// db[id].ct.zctType = 1;
	
	id = 1;
	meter[id].setting.pt.wiring = 0;	// 3P4W, 3P3W(2), 3P3W(3)
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 100;
	meter[id].setting.ct.CT1 = 100;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1000;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	reqSaveSettings(0x1234);
}

void build_cal_db2(void)
{
	int id=0;
	// Volt.
	meter[id].setting.pt.wiring = 1;	// 3P4W, 3P3W(2), 3P3W(3)
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 100;
	meter[id].setting.ct.CT1 = 100;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1000;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	
	id = 1;
	meter[id].setting.pt.wiring = 1;	// 3P4W, 3P3W(2), 3P3W(3)
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 100;
	meter[id].setting.ct.CT1 = 100;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1000;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	
	reqSaveSettings(0x1234);
}
void build_cal_db3(void)
{
	int id=0;
	// Volt.
	meter[id].setting.pt.wiring = 0;	// 3P4W, 3P3W(2), 3P3W(3)
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 250;
	meter[id].setting.ct.CT1 = 250;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1250;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	// db[id].pt.wiring = 0;	// 3P4W, 3P3W(2), 3P3W(3)
	// db[id].pt.freq = 60;	// 60Hz
	// db[id].pt.vnorm = 220;	
	// db[id].pt.PT1 = 220;
	// db[id].pt.PT2 = 220;
	// db[id].ct.inorm = 100;
	// db[id].ct.CT1 = 100;
	// db[id].ct.CT2 = 1;		// 5A CT
	// db[id].ct.zctScale = 1000;	// 2nd scale
	// db[id].ct.zctType = 1;
	
	id = 1;
	meter[id].setting.pt.wiring = 0;	// 3P4W, 3P3W(2), 3P3W(3)
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 250;
	meter[id].setting.ct.CT1 = 250;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1250;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	reqSaveSettings(0x1234);
}

void build_set_db(void)
{
	int id=0;
	// Volt.
	meter[id].setting.pt.wiring = 1;	// 3P3W
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 100;
	meter[id].setting.ct.CT1 = 100;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1000;	// 2nd scale
	meter[id].setting.ct.zctType = 1;
	
	id = 1;
	meter[id].setting.pt.wiring = 2;	// 1P2W
	meter[id].setting.pt.freq = 60;	// 60Hz
	meter[id].setting.pt.vnorm = 220;	
	meter[id].setting.pt.PT1 = 220;
	meter[id].setting.pt.PT2 = 220;
	meter[id].setting.ct.inorm = 100;
	meter[id].setting.ct.CT1 = 100;
	meter[id].setting.ct.CT2 = 1;		// 5A CT
	meter[id].setting.ct.zctScale = 1000;	// 2nd scale
	meter[id].setting.ct.zctType = 1;

	meter[id].setting.comm.comMode = 0;		// master
	meter[id].setting.comm.devId = 1;		// master
	meter[id].setting.comm.baud = 0;
	meter[id].setting.comm.parity = 1;

	// memset(&meter[0].setting.f_setting[0], 0, sizeof(FLOW_SETTING)*4);
	// memcpy(&pflow->f_setting[0], &pdbk->f_setting[0], sizeof(FLOW_SETTING)*4);
	reqSaveSettings(0x1234);
}

int initSettings(int id)
{
	printf("{{initSettings}}\n");
	memset(&db[id], 0, sizeof(*pdb));
		
	//db[id].password = 0;
	// com.
	db[id].comm.comMode = 1;
	db[id].comm.baud = 4; // 115200
	db[id].comm.devId = 1;
	db[id].comm.parity = 0;
	db[id].comm.tcpPort = 502;
	db[id].comm.dhcpEn = 0;		// disble
	
	db[id].comm.ip0[0] = 192;
	db[id].comm.ip0[1] = 168;
	db[id].comm.ip0[2] = 9;
	db[id].comm.ip0[3] = 124;
	
	db[id].comm.sm0[0] = 255;
	db[id].comm.sm0[1] = 255;
	db[id].comm.sm0[2] = 0;
	db[id].comm.sm0[3] = 0;
	
	db[id].comm.gw0[0] = 192;
	db[id].comm.gw0[1] = 168;
	db[id].comm.gw0[2] = 9;
	db[id].comm.gw0[3] = 1;	
	
	db[id].comm.dns0[0] = 168;
	db[id].comm.dns0[1] = 126;
	db[id].comm.dns0[2] = 63;
	db[id].comm.dns0[3] = 1;	
	
	db[id].comm.sntp[0] = 0;
	db[id].comm.sntp[1] = 0;
	db[id].comm.sntp[2] = 0;
	db[id].comm.sntp[3] = 0;		
		
	strcpy((char *)db[id].comm.host, "SV300");
	db[id].comm.tcpPort = 502;
	
	db[id].comm.gwEable = 0x0f;
	db[id].comm.RS485MasterMode = 2;		// slave
	db[id].comm.dhcpEn = 0;				// disble
	//
	db[id].etc.backlightTime = 0;
	db[id].etc.brightness = 4;	// 100%
	db[id].etc.timezone = 540;	//60 * 9
	
	// Volt.
	db[id].pt.wiring = 0;	// 3P4W
	db[id].pt.freq = 60;	// 60Hz
	db[id].pt.vnorm = 220;	
	db[id].pt.PT1 = 220;
	db[id].pt.PT2 = 220;
	
	// Current
	if(id == 0) {
		db[id].ct.inorm = 250;
		db[id].ct.CT1 = 250;
		db[id].ct.CT2 = 1;	// 5A CT
		db[id].ct.zctScale = 1000;	// 2nd scale
		db[id].ct.zctType = 1;
	}
	else {
		db[id].ct.inorm = 100;
		db[id].ct.CT1 = 100;
		db[id].ct.CT2 = 1;	// 5A CT
		db[id].ct.zctScale = 1000;	// 2nd scale
		db[id].ct.zctType = 1;
	}

			
	//
	db[id].etc.interval = 2;		// demand Interval, 15분(0:1,1:5,2:15,3:30,4:60)
	db[id].etc.timezone = 540;	// 9:00
	db[id].etc.Iload = 50;		// for TDD calculation	(unused)
	db[id].etc.backlightTime = 1;	// 2min
	db[id].etc.brightness = 2;	// 60%
	db[id].etc.autorotation = 2;	// 6sec
	db[id].etc.maxminItv = 2;		// none
	
	// Transient Event
	db[id].transient[0].level = 200;		// 0:disabled, 100 ~ 200%
	db[id].transient[0].fastChange = 4;	// 0:disabled, 1 ~ 10%
	db[id].transient[0].holdOff = 500;	
	db[id].transient[0].action = 0;			// default : No Action
	
	db[id].transient[1].level = 150;		// 0:disabled, 100 ~ 200%
	db[id].transient[1].fastChange = 4;	// 0:disabled, 1 ~ 10%
	db[id].transient[1].holdOff = 500;
	db[id].transient[1].action = 0;	
	
	// PQ Event
	db[id].pqevt[PQE_OC].level=200;
	db[id].pqevt[PQE_OC].nCyc=1;
	db[id].pqevt[PQE_OC].action = 0;
	db[id].pqevt[PQE_OC].holdOffCyc = 500;	// 500ms
	// sag
	db[id].pqevt[PQE_SAG].level=80;
	db[id].pqevt[PQE_SAG].nCyc=1;
	db[id].pqevt[PQE_SAG].action = 0;
	db[id].pqevt[PQE_SAG].holdOffCyc = 500;	// 500ms
	// swell
	db[id].pqevt[PQE_SWELL].level=120;
	db[id].pqevt[PQE_SWELL].nCyc=1;
	db[id].pqevt[PQE_SWELL].action = 0;
	db[id].pqevt[PQE_SWELL].holdOffCyc = 500;	// 500ms
	// Interruption
	db[id].pqevt[PQE_INTR].level=5;
	db[id].pqevt[PQE_INTR].nCyc=1;
	db[id].pqevt[PQE_INTR].action = 0;
	db[id].pqevt[PQE_INTR].holdOffCyc = 500;	// 500ms, not used
		
	// PQ report
	db[id].pqRpt.active = 1;	
	
	// Trend	
	db[id].trend[0].chan[0] = 1;
	db[id].trend[0].chan[1] = 2;
	

	return 0;
}



int buildSettings(int id)
{
	int ret=0, i;
	float ptratio;
	CNTL_DATA	*_pcntl = &meter[id].cntl;
	
	if (db[id].comm.devId > 250) {
		printf("*** Invalid devId = %d\n", db[id].comm.devId);
		ret = -1;
	}
	
	if (db[id].comm.baud >= 5) {
		printf("*** Invalid baudrate = %d\n", db[id].comm.baud);
		ret = -1;
	}
	
	//if (db[id].wiring > WM_3LL3CT) {
	if (db[id].pt.wiring > WM_3LL2CT) {
		printf("*** Invalid wiring mode = %d\n", db[id].pt.wiring);
		ret = -1;
	}
	
	if (strlen(db[id].comm.host) == 0) {
		strcpy(db[id].comm.host, "SV300");
	}
		
	db[id].ct.CT2 = getHwModel();
	printf("[HW : Model = %d, Version = %d]\n", db[id].ct.CT2, getHwVersion());

	_pcntl->I_start = (float)(db[id].ct.inorm) * db[id].ct.I_start/1000.;
	printf("[start Current = %f, %d, %d]\n", _pcntl->I_start, db[id].ct.inorm, db[id].ct.I_start);

	// 정격전압이 150V 보다 적으면 phase volt. gain을 2로 한다 
	// 2017-11-23, vnorm을 PT2로 변경한다, VNORM은 sag/swell 기준값으로 사용한다 
	//if (db[id].vnorm < 150) {

	meter[id].cntl.pga_vgain = 1;	// 2x
	meter[id].cntl.wh_scale = 2;	// 전압이 1/2이므로 wh를 2배한다 
	meter[id].cntl.pthreshold = getThresHold(db[id].ct.CT2);
	// 2021-7-7, pt ration 구하는 과정에서 정수비가 아닌경우 오류 발생함.// 	
	ptratio = (float)db[id].pt.PT1/db[id].pt.PT2;	
	meter[id].cntl.vscale    = (V_FULL/RMS_MAX) *ptratio/2;
	meter[id].cntl.wv_vscale = (V_FULL/WAVE_MAX)*ptratio/2; //(V_FULL/WAVE_MAX)/2;	
	meter[id].cntl.wscale    = (V_FULL)*(ptratio)/2;
	
	//meter[id].cntl.wv_vscale = (V_FULL/WAVE_MAX)/2;
	meter[id].cntl.wv_vscale = (V_FULL/WAVE_MAX)*10/2;
	meter[id].wv.vscale = meter[id].cntl.wv_vreverse = ptratio/10.;
		
	switch (db[id].ct.CT2) {	
	case CT_5A:		
		// 부담저항 : 66.6
		if (getHwVersion() == 0) {
			meter[id].cntl.iscale    = I_FULL_5A/RMS_MAX  * db[id].ct.CT1/5.; 		
			meter[id].cntl.wscale   *= I_FULL_5A*(db[id].ct.CT1/5.);
			meter[id].cntl.pga_igain = 0;	// 1x
			meter[id].cntl.inoload   = RMS_MAX*I_NOLOAD_5A/I_FULL_5A;	// 2020-4-3, CT ration 적용전에 처리해야 한다 
			//meter[id].cntl.inoload   = db[id].ct.CT1/5.*I_NOLOAD_5A;	// -> 전류에 해당하는 raw 값으로 변환한다 
		
			// CT1이 적으면 계단파로 표시된다 
			//meter[id].cntl.wv_iscale = I_FULL_5A/WAVE_MAX * db[id].ct.CT1/5.; 	
			meter[id].cntl.wv_iscale = I_FULL_5A/WAVE_MAX * 2000;	// ~10617
			meter[id].wv.iscale   = meter[id].cntl.wv_ireverse = (db[id].ct.CT1/5)/2000.;
			// add pulse config
			meter[id].cntl.CFxDEN    = CFxDEN_5A;
		}
		else {
			// 부담저항 : 20
			meter[id].cntl.iscale    = I_FULL_5AN/RMS_MAX  * db[id].ct.CT1/5.; 		
			meter[id].cntl.wscale   *= I_FULL_5AN*(db[id].ct.CT1/5.);
			meter[id].cntl.pga_igain = 0;	// 1x
			meter[id].cntl.inoload   = RMS_MAX*I_NOLOAD_5AN/I_FULL_5AN;	// 2020-4-3, CT ration 적용전에 처리해야 한다 
			//meter[id].cntl.inoload   = db[id].ct.CT1/5.*I_NOLOAD_5A;	// -> 전류에 해당하는 raw 값으로 변환한다 
		
			// CT1이 적으면 계단파로 표시된다 
			//meter[id].cntl.wv_iscale = I_FULL_5A/WAVE_MAX * db[id].ct.CT1/5.; 	
			meter[id].cntl.wv_iscale = I_FULL_5AN/WAVE_MAX * 2000;	// ~10617
			meter[id].wv.iscale   = meter[id].cntl.wv_ireverse = (db[id].ct.CT1/5)/2000.;
			// add pulse config
//			meter[id].cntl.CFxDEN    = CFxDEN_5AN;			
			meter[id].cntl.CFxDEN    = CFxDEN_5AN - getHwVersion();			
		}
		break;
		
	case CT_100mA:
	case CT_333mV:		
		meter[id].cntl.iscale    = I_FULL_100mA/RMS_MAX  * (db[id].ct.CT1/50.)/2; 				
		meter[id].cntl.wscale   *= I_FULL_100mA*(db[id].ct.CT1/50.)/2;
		meter[id].cntl.pga_igain  = 0;	// 1x
		meter[id].cntl.inoload   = RMS_MAX*I_NOLOAD_100mA/I_FULL_100mA;	// 2020-4-3, CT ration 적용전에 처리해야 한다 
		//meter[id].cntl.inoload   = db[id].ct.CT1/50.*I_NOLOAD_100mA;
	
		// CT1이 적으면 계단파로 표시된다 
		//meter[id].cntl.wv_iscale = I_FULL_100mA/WAVE_MAX * (db[id].ct.CT1/50.)/2; 		
		meter[id].cntl.wv_iscale = I_FULL_100mA/WAVE_MAX * 200; // ~11000	
		meter[id].wv.iscale   = meter[id].cntl.wv_ireverse = (db[id].ct.CT1/50.)/200.;
		// add pulse config
		meter[id].cntl.CFxDEN    = CFxDEN_100mA;
		break;
		
	// wave 표시와 관련된 수정 필요하다 ...
	case CT_RCT:		
		if (db[id].pt.freq == 60) {
			meter[id].cntl.iscale    = I_FULL_RCT_60Hz / RMS_MAX;			
			meter[id].cntl.wscale   *= I_FULL_RCT_60Hz;		
			meter[id].cntl.wv_iscale = I_FULL_RCT_60Hz / WAVE_MAX;
		}
		else {
			meter[id].cntl.iscale    = I_FULL_RCT_50Hz / RMS_MAX;			
			meter[id].cntl.wscale   *= I_FULL_RCT_50Hz;			
			meter[id].cntl.wv_iscale = I_FULL_RCT_50Hz / WAVE_MAX;
		}
		
		// 500 ~ 1000(GAIN=2)
		if (db[id].ct.CT1 > 500 && db[id].ct.CT1 <= 1000) {
			meter[id].cntl.iscale /= 2; 
			meter[id].cntl.wscale /= 2;
			meter[id].cntl.pga_igain = 2;		// 4x						
			meter[id].cntl.wv_iscale /= 2;
		}	
		// 1000 ~ 3000(GAIN=1)
		else if (db[id].ct.CT1 > 1000 && db[id].ct.CT1 <= 3000) {
			meter[id].cntl.pga_igain = 1;		// 2x
			meter[id].cntl.wv_iscale *= 10;
		}
		// 500A(GAIN=4)
		else {	
			meter[id].cntl.iscale /= 4;			
			meter[id].cntl.wscale /= 4;
			meter[id].cntl.wv_iscale /= 4;			
			meter[id].cntl.pga_igain = 3;		// 8x
		}
		break;	
	}	
	
	if (db[id].ct.CT2 == CT_5A) {
		meter[id].cntl.pga_ingain = 0;	// 1x
		meter[id].cntl.igscale = meter[id].cntl.iscale;		// In의 scale은 Ia,Ib,Ic와 같다
	}
	else {	
		// 200mA:1.5mA CT 만 지원한다 
		meter[id].cntl.pga_ingain = 2;	// 4x
		meter[id].cntl.igscale = IZ_FULL/RMS_MAX; 			
//		switch (db[id].ct.zctType) {
//		case 0:	// none
//			meter[id].cntl.pga_ingain = 0;	// 2x
//			meter[id].cntl.igscale = 0;	// 1x
//			break;
//		
//		case 1:	// 100mV@200mA, rb=66.6 ohm, unused			
//			meter[id].cntl.pga_ingain = 1;	// 2x
//			meter[id].cntl.igscale = 12.856/RMS_MAX * (db[id].ct.CT1/50.)/2; 		
//			break;
//		
//		case 2:	// 1.5mA@200mA, rb=66.6, unsed		
//			meter[id].cntl.pga_ingain = 0;	// 1x
//			meter[id].cntl.igscale = 1.416/RMS_MAX * (db[id].ct.CT1/50.)/2; 		
//			break;		
//		
//		case 3:	// 0.1mA@200mA, rb=66.6, unused
//			meter[id].cntl.pga_ingain = 2;	// 8x
//			meter[id].cntl.igscale = 1.416/RMS_MAX * (db[id].ct.CT1/50.); 		
//			break;
//		}
	}
	
	// zct 2nd Scale
	if (db[id].ct.zctScale == 0) {
		db[id].ct.zctScale = 1000;
	}
			
	printf("iscale: %f\n", meter[id].cntl.iscale);
	printf("igscale: %f\n", meter[id].cntl.igscale);
	printf("vscale: %f\n", meter[id].cntl.vscale);
	printf("wscale: %f\n", meter[id].cntl.wscale);
	
	
	switch (db[id].etc.interval) {
	case 0:
		meter[id].cntl.dInterval = 1*60;
		break;
	case 1:
		meter[id].cntl.dInterval = 5*60;
		break;	
	case 2:
		meter[id].cntl.dInterval = 15*60;
		break;	
	case 3:
		meter[id].cntl.dInterval = 30*60;
		break;	
	case 4:
		meter[id].cntl.dInterval = 60*60;
		break;	
	default:
		db[id].etc.interval = 0;
		meter[id].cntl.dInterval = 1*60;		
		break;
	}
	
	meter[id].cntl.sumMax = 5;	// 10/12 rms를 읽고 1초 평균 계산한다 
	meter[id].cntl.nFastRMS = db[id].pt.freq*2;
	
	meter[id].cntl.freqLo[0] = db[id].pt.freq - db[id].pt.freq*0.01;
	meter[id].cntl.freqHi[0] = db[id].pt.freq + db[id].pt.freq*0.01;
	meter[id].cntl.freqLo[1] = db[id].pt.freq - db[id].pt.freq*0.06;
	meter[id].cntl.freqHi[1] = db[id].pt.freq + db[id].pt.freq*0.04;
	
	meter[id].cntl.uLo[0] = db[id].pt.vnorm - db[id].pt.vnorm*0.1;
	meter[id].cntl.uHi[0] = db[id].pt.vnorm + db[id].pt.vnorm*0.1;
	meter[id].cntl.uLo[1] = db[id].pt.vnorm - db[id].pt.vnorm*0.15;
	meter[id].cntl.uHi[1] = db[id].pt.vnorm + db[id].pt.vnorm*0.1;	
	
			
// initSag 호출하는 부분으로 이동	
//	
//	if (db[id].wiring == WM_3P4W || meter[id].cntl.sagSwellMask == WM_3P3W) 
//		meter[id].cntl.sagSwellMask = 7;
//	else
//		meter[id].cntl.sagSwellMask = 0;

	//printf("sag   : %d, %d\n", db[id].Sag_lvl, db[id].Sag_cyc);
	//printf("swell : %d, %d\n", db[id].Swell_lvl, db[id].Swell_cyc);
	printf("devId=%d, speed=%d, parity=%d\n", db[id].comm.devId, db[id].comm.baud, db[id].comm.parity);

#ifdef METER_TEST_DATA
	// alarm db
	db[id].etc.timezone = 540;
#endif	
	

#ifdef METER_TEST_DATA
	// trend
	db[id].trend[0].active = 1;
	db[id].trend[0].interval = 0;
	
	i=0;
	db[id].trend[0].chan[i++] = 2;	// Freq
	db[id].trend[0].chan[i++] = 3;	// U
	db[id].trend[0].chan[i++] = 4;	// 
	db[id].trend[0].chan[i++] = 5;	// 
	
	db[id].trend[0].chan[i++] = 13;	// I
	db[id].trend[0].chan[i++] = 14;	// 
	db[id].trend[0].chan[i++] = 15;	// 
	db[id].trend[0].chan[i++] = 22;	// P
	
	db[id].trend[0].chan[i++] = 26;	// Q
	db[id].trend[0].chan[i++] = 34;	// S	
#endif	
	//
	//
	// sag range
	if (db[id].pqevt[PQE_SAG].level > 90 || db[id].pqevt[PQE_SAG].level < 10) {		
		printf("@@@ invalid sag level = %d\n", db[id].pqevt[PQE_SAG].level);
		db[id].pqevt[PQE_SAG].level=90;
	}	
	// intr range
	if (db[id].pqevt[PQE_INTR].level < 5) {		
		printf("@@@ invalid intr level = %d\n", db[id].pqevt[PQE_INTR].level);
		db[id].pqevt[PQE_INTR].level=5;
	}	
	// swell range		
	if (db[id].pqevt[PQE_SWELL].level < 110 || db[id].pqevt[PQE_SWELL].level > 180) {
		printf("@@@ invalid swell level = %d\n", db[id].pqevt[PQE_SWELL].level);
		db[id].pqevt[PQE_SWELL].level = 110;
	}	
	// 2025-3-20, 범위를 0 ~ 200으로 처리
	// oc range 
	if (db[id].pqevt[PQE_OC].level > 200) {
		printf("@@@ invalid swell level = %d\n", db[id].pqevt[PQE_OC].level);
		db[id].pqevt[PQE_OC].level = 200;
	}	
	
#ifdef METER_TEST_DATA	
	//	
	// transient voltage, current
	//
	db[id].transient[0].level = 200;		// 0:disabled, 100 ~ 200%
	db[id].transient[0].fastChange = 4;	// 0:disabled, 1 ~ 10%
	db[id].transient[0].holdOff = 500;	// 500ms
	db[id].transient[0].action = 1;			// capture
	
	db[id].transient[1].level = 150;		// 0:disabled, 100 ~ 200%
	db[id].transient[1].fastChange = 4;	// 0:disabled, 1 ~ 10%
	db[id].transient[1].holdOff = 500;	// 500ms
	db[id].transient[1].action = 1;			// capture
#endif	
	
	for (i=0; i<2; i++) {
		if (db[id].transient[i].level < 120 || db[id].transient[i].level > 300) {		// 0:disabled, 120 ~ 200%
			db[id].transient[i].level = 200;
		}	
	}	
	
	i=0;
	if (db[id].transient[i].fastChange > 10) {	// 0:disabled, 1 ~ 10%	
		db[id].transient[i].fastChange = 4;
	}
	
	i=1;
	if (db[id].transient[i].fastChange > 10) {	// 0:disabled, 1 ~ 10%	
		db[id].transient[i].fastChange = 4;
	}
	//
	//
	//
#ifdef METER_TEST_DATA	
	// sag
	db[id].pqevt[PQE_SAG].level=80;
	db[id].pqevt[PQE_SAG].nCyc=1;
	db[id].pqevt[PQE_SAG].action = 1;
	db[id].pqevt[PQE_SAG].holdOffCyc = 500;	// 500ms
	db[id].pqevt[PQE_SAG].do_action = 3;
	// OC
	db[id].pqevt[PQE_OC].level=200;
	db[id].pqevt[PQE_OC].nCyc=1;
	db[id].pqevt[PQE_OC].action = 1;
	db[id].pqevt[PQE_OC].holdOffCyc = 500;	// 500ms
	// // sag
	db[id].pqevt[PQE_SAG].level=80;
	db[id].pqevt[PQE_SAG].nCyc=1;
	db[id].pqevt[PQE_SAG].action = 1;
	db[id].pqevt[PQE_SAG].holdOffCyc = 500;	// 500ms
	// // swell
	db[id].pqevt[PQE_SWELL].level=120;
	db[id].pqevt[PQE_SWELL].nCyc=1;
	db[id].pqevt[PQE_SWELL].action = 1;
	db[id].pqevt[PQE_SWELL].holdOffCyc = 500;	// 500ms
	// // Interruption
	db[id].pqevt[PQE_INTR].level=5;
	db[id].pqevt[PQE_INTR].nCyc=1;
	db[id].pqevt[PQE_INTR].action = 1;
	db[id].pqevt[PQE_INTR].holdOffCyc = 500;	// 500ms, not used
#endif	

	return ret;
}


void initExtSettings(int id)
{
	int i;	
	
//	memcpy(pComm->ip0, def_ip, sizeof(def_ip));
//	memcpy(pComm->sm0, def_sm, sizeof(def_sm));
//	memcpy(pComm->gw0, def_gw, sizeof(def_gw));
//	memcpy(pComm->dns0, def_dn, sizeof(def_dn));

//	pComm->tcpPort = 502;
//	
//#ifdef _GATEWAY_	
//	pComm->g35k_sb[0].enable = 0;
//	pComm->g35k_sb[0].devId = 1;
//	pComm->g35k_sb[1].enable = 0;
//	pComm->g35k_sb[1].devId = 2;
//#endif
	for (i=0; i<4; i++) {
		db[id].trend[i].active = 0;
	}
	
	for (i=0; i<4; i++) {
		db[id].transient[0].action = 0;
	}
	
	for (i=0; i<4; i++) {
		db[id].pqevt[i].action = 0;
	}
	
	
#ifdef _IOMODULE_
	// 2017-10-18, IOM 초기 설정값 변경 (DI Mode, DO Time =100)
	//piom[0].modType = 0;			
	// 2018-4-17, DIM8로 수정
	for (i=0; i<8; i++) {
		piom[0].diType[i] = 0;
		piom[0].debounce[i] = 20;
	}
	for (i=0; i<8; i++) {
		piom[0].piConst[i] = 1;
	}
	for (i=0; i<2; i++) {
		piom[0].doMode[i] = 0;
		piom[0].doType[i] = 0;
		piom[0].doTimer[i] = 100;
	}
	memcpy(piom[0].ts, meter[0].meter.tod, sizeof(TIME_STAMP));
#endif	
}

int buildExtSettings(int id)
{
//	COMM_CFG *pcom = &piocfg->com;
//	IO_CFG *piom = piocfg->io;
	int i, j, ret=0;
	ALARM_DEF *paset = &db[id].alarm;
	COMM_CFG *pComm=&db[0].comm;

	printf("ipaddress = %d.%d.%d.%d\n", pComm->ip0[0], pComm->ip0[1], pComm->ip0[2], pComm->ip0[3]);
	printf("netmask  = %d.%d.%d.%d\n", pComm->sm0[0], pComm->sm0[1], pComm->sm0[2], pComm->sm0[3]);
	printf("Gateway  = %d.%d.%d.%d\n", pComm->gw0[0], pComm->gw0[1], pComm->gw0[2], pComm->gw0[3]);
	printf("DNS      = %d.%d.%d.%d\n", pComm->dns0[0], pComm->dns0[1], pComm->dns0[2], pComm->dns0[3]);
	printf("MAC      = %02x:%02x:%02x:%02x\n", pcal->mac[0], pcal->mac[1], pcal->mac[2], pcal->mac[3]);

// 2017-12-6
	if (pComm->baud >= 5) {
		printf("Invalid baud rate : %d\n", pComm->baud);
		ret = -1;
	}
	
	
#if 0		// 2025-3-20, doType 사용자가 직접 지정
	// 초기화
	for(i=0; i<IOMAX_DO; i++)
		piocfg->doType[i] = DOTYPE_OUT;

	// alarm do check
	for(i=0; i<32; i++) {
		if(paset->set[i].do_action != 0) {
			piocfg->doType[paset->set[i].do_action-1] = DOTYPE_ALM;			// alarm set
			printf("[alarm output channel(%d, %d)]\n", i, paset->set[i].do_action);
		}
	}
	// event do check
	for(i=0; i<4; i++) {
		if(db[id].pqevt[i].do_action != 0) {
			piocfg->doType[db[id].pqevt[i].do_action-1] = DOTYPE_EVENT;			// event set
			printf("[event output channel(%d, %d)]\n", i, db[id].pqevt[i].do_action);
		}
	}
#endif
	
#ifdef _IOMOD_	
	// alarm output mod, point 초기화 
	meter[id].cntl.almMod = meter[id].cntl.almPoint = 0;
	// 2017-11-22
	meter[id].cntl.evtMod = meter[id].cntl.evtPoint = 0;
	
	// 2018-4-17, DIM8로 수정
	for (i=0; i<2; i++) {
		for (j=0; j<8; j++) {
			if (piom[i].diType[j] != 0 && piom[i].diType[j] != 1) {
				printf("!!! (%d,%d) Bad DI type = %d\n", i, j, piom[i].diType[j]);
				ret = -1;
			}
			
			if (piom->debounce[i] < 4 || piom[i].debounce[j] > 999) {
				printf("!!! (%d,%d) Out of range DI debounce = %d\n", i, j, piom[i].debounce[j]);
				ret = -1;
			}
		}

				
		for (j=0; j<2; j++) {
			// relay output
			if (piom[i].doType[j] == DOTYPE_OUT) {
			}
			// alarm output, 여러 DO 채널이 알람타입이라 해도 첫번째 정의된 DO 채널을 alarm output용으로 사용
			else if (piom[i].doType[j] == DOTYPE_ALM) {
				// module 타입이 DIO 일때만 alarm Output 
				if (meter[id].cntl.almMod == 0) {
					meter[id].cntl.almMod = i+1;
					meter[id].cntl.almPoint = j+1;
					printf("[alarm output channel(%d, %d)]\n", meter[id].cntl.almMod, meter[id].cntl.almPoint);
				}
			}
			// 2017-11-22, sag-swell event
			else if (piom[i].doType[j] == DOTYPE_EVENT) {
				if (meter[id].cntl.evtMod == 0) {
					meter[id].cntl.evtMod = i+1;
					meter[id].cntl.evtPoint = j+1;
					printf("[event output channel(%d, %d)]\n", meter[id].cntl.evtMod, meter[id].cntl.evtPoint);
				}
			}
			else {
				printf("!!! (%d,%d) Bad DO type = %d\n", i, j, piom[i].doType[j]);
				ret = -1;
			}
			
			if (piom[i].doMode[j] != 0 && piom[i].doMode[j] != 1) {
				printf("!!! (%d,%d) Bad DO mode = %d\n", i, j, piom[i].doMode[j]);
				ret = -1;
			}			
			if (piom->doTimer[j] < 100 || piom[i].doTimer[j] > 9999) {
				printf("!!! (%d,%d) Bad DO mode = %d\n", i, j, piom[i].doTimer[j]);
				ret = -1;
			}					
		}
	}
#endif	
	return ret;
}


int pushWaveForm(void)
{

}

void dump(uint8_t *buf, int n) {
	int i;
	
	for (i=0; i<n; i++) {
		printf("%02x ", buf[i]);
		if ((i+1)%16 == 0) printf("\n");		
	}
	printf("\n");
}

int loadHwSettings(METER_CAL *pcal) {
	uint16_t crc;
	
	printf("sizeof(METER_CAL) = %d\n", sizeof(METER_CAL));
	ChipEepRead(0, pcal, sizeof(METER_CAL));
	crc = gencrc_modbus((uint8_t *)pcal, sizeof(METER_CAL));
	
	if (crc == 0) {
		dump((void *)pcal, sizeof(METER_CAL));
		return 0;
	}
	else {
		printf("{{Can't load HW Settings, crc=%04x/%04x, magic=%04x ...}}\n", crc, pcal->crc, pcal->magic);		
		dump((void *)pcal, sizeof(METER_CAL));
		memset(pcal, 0, sizeof(METER_CAL));
		storeHwSettings(pcal);
		return -1;
	}
}

void storeHwSettings(METER_CAL *pcal) {
	pcal->magic = 0x1234;
	pcal->crc = gencrc_modbus((uint8_t *)pcal, sizeof(METER_CAL)-2);
	ChipEepWrite(0, pcal, sizeof(METER_CAL));
	
	printf("storeHwSettings, crc=%04x\n", pcal->crc);
	dump((void *)pcal, sizeof(METER_CAL));
}

int loadSettings(SETTINGS	*pdb)
{
	int ret=0;
	FILE *fp;
	fp = fopen(SETTING_FILE, "rb");
	if (fp == NULL) {
		printf("{{Can't load Settings}}\n");
		initSettings(0);
		initSettings(1);
		build_set_db();		
		initExtSettings(0);
		
		fp = fopen(SETTING_FILE, "wb");
		if (fp != NULL) {
			fwrite(pdb, 1, sizeof(*pdb)*2, fp);
			fclose(fp);
			printf("[[Create Default Settings(%s)]]\n", SETTING_FILE);
		}		
		ret = -1;
	}
	else {
		fread(pdb, 1, sizeof(*pdb)*2, fp);
		fclose(fp);				
		printf("loadSettings ok\n");
	}

	memcpy(&db[1].comm, &db[0].comm, sizeof(COMM_CFG));
	memcpy(&meter[0].setting, &db[0], sizeof(SETTINGS));
	memcpy(&meter[1].setting, &db[1], sizeof(SETTINGS));

	

	buildSettings(0);
	buildSettings(1);

#ifdef	METER_TEST_DATA
	buildAlarmSettings();	
#endif	
	buildExtSettings(0);
	buildExtSettings(1);
		
#ifdef	DAQ
	db[id].comm.daq_ip[0] = 192;
	db[id].comm.daq_ip[0] = 168;
	db[id].comm.daq_ip[0] = 8;
	db[id].comm.daq_ip[0] = 71;
	
	db[id].comm.daq_format = 0;	// wave
	db[id].comm.daq_id = 1;
	db[id].comm.daq_interval = 1;	// 1 minute
	db[id].comm.daq_length = 4;	// 32k (2k<<4)
	db[id].comm.daq_srate = 2;	// 8k (2k<<2)
	db[id].comm.daq_bitpersample = 0;	// 16bit
#endif
	
	// setting을 backup영역으로 복사한다 
//	memcpy(pdbk, pdb, sizeof(SETTINGS));		
	
	return ret;
}


int saveSettings(SETTINGS	*pdb) {
	FILE *fp = fopen(SETTING_FILE, "wb");
	if (fp != NULL) {
		fwrite(pdb, 1, sizeof(*pdb)*2, fp);
		fclose(fp);
		printf("[[Save Settings(%s)]]\n", SETTING_FILE);
		return 0;
	}			
	else {
		return -1;
	}
}


void getTimeStamp(uint16_t *dst)
{
	//memcpy(dst, meter[0].meter.tod, sizeof(TIME_STAMP));
	//dst[5] = dst[5]*1000+getTickMsec();
	memcpy(dst, &freezeTod, sizeof(TIME_STAMP));
}


void getFloatStr(float v, char *pbuf) {
	float av;

	if (isnan(v)) {
		strcpy(pbuf, "----");
		return;
	}
	
	av = fabs(v);	
		
	if (av < 10) {
		sprintf(pbuf, "%.3f", v);
	}
	else if (av < 100) {
		sprintf(pbuf, "%.2f", v);
	}
	else if (av < 1000) {
		sprintf(pbuf, "%.1f", v);
	}
	else {
		sprintf(pbuf, "%.0f", v);
	}
}

//void resetMaxMin() {
//	memset(pmm, 0, sizeof(MAXMIN));
//	pmm->rstTime = sysTick1s;
//}

int updateMaxMin(MAXMIN_DATA *pmm, float comp) {
	int change=0;
	
	// min 초기값이 0으로 고정되는 문제 처리, 또는 NAN 경우 처리
	if (pmm->max == 0 && pmm->min == 0 || isnan(pmm->max) || isnan(pmm->min)) {
		pmm->max = pmm->min = comp;
	}
	else {
		if (pmm->max < comp) {
			pmm->max = comp;
			pmm->max_ts = sysTick1s;
			change++;
		}

		if (pmm->min > comp) {
			pmm->min = comp;
			pmm->min_ts = sysTick1s;
			change++;
		}	
	}
	return change;
}

int maxMinRmsFreq(int id)
{
	int i, ix=0, changeF=0;
	//MAXMIN_DATA *comp;
	
	if (meter[id].cntl.online3++ < 5) return 0;
	
	pmm->fr += updateMaxMin(&pmm->Freq, meter[id].meter.Freq);
	
	for (i=0; i<4; i++) {
		pmm->fr += updateMaxMin(&pmm->U[i], meter[id].meter.U[i]);
	}
	//changeF += updateMaxMin(&meter[id].cntl.maxmin.Ubalance, meter[0].meter.Ubalance);
	for (i=0; i<4; i++) {
		pmm->fr += updateMaxMin(&pmm->Upp[i], meter[id].meter.Upp[i]);
	}	
	//changeF += updateMaxMin(&meter[id].cntl.maxmin.UppBalance, meter[0].meter.UppBalance);
	for (i=0; i<4; i++) {
		pmm->fr += updateMaxMin(&pmm->I[i], meter[id].meter.I[i]);
	}	
	pmm->fr += updateMaxMin(&pmm->Itot, meter[id].meter.Itot);
	pmm->fr += updateMaxMin(&pmm->In, meter[id].meter.In);
	pmm->fr += updateMaxMin(&pmm->Isum, meter[id].meter.Isum);
	pmm->fr += updateMaxMin(&pmm->Temp, meter[id].meter.Temp);
	
	return changeF;
}

int maxMinPower(int id) {
	int i, ix=0, changeF=0;
	//MAXMIN_DATA *comp;
	
	if (meter[id].cntl.online3 < 5) return 0;
	
	for (i=0; i<4; i++) {
		pmm->fr += updateMaxMin(&pmm->P[i], meter[id].meter.P[i]);
		pmm->fr += updateMaxMin(&pmm->Q[i], meter[id].meter.Q[i]);
		pmm->fr += updateMaxMin(&pmm->S[i], meter[id].meter.S[i]);
		pmm->fr += updateMaxMin(&pmm->PF[i], meter[id].meter.PF[i]);
	}	
	return changeF;	
}

int maxMinTHD(int id) {
	int i, ix=0, changeF=0;
	
	if (meter[id].cntl.online3 < 5) return 0;

#ifdef	VFRMS
	for (i=0; i<3; i++) {	
		pmm->fr += updateMaxMin(&pmm->THD_U[i], meter[id].meter.THD_U[i]);
		pmm->fr += updateMaxMin(&pmm->THD_Upp[i], meter[id].meter.THD_Upp[i]);		
		pmm->fr += updateMaxMin(&pmm->THD_I[i], meter[id].meter.THD_I[i]);		
	}
#else
	for (i=0; i<4; i++) {
		pmm->fr += updateMaxMin(&pmm->THD_U[i], meter[id].meter.THD_U[i]);
		pmm->fr += updateMaxMin(&pmm->THD_Upp[i], meter[id].meter._uppthd[i]);		
		pmm->fr += updateMaxMin(&pmm->THD_I[i], meter[id].meter.THD_I[i]);		
	}
#endif
}


const float hmLimit[] = {
	2,	// 2
	5,	// 3
	1,	// 4
	6,	// 5
	0.5,// 6
	5,	// 7
	0.5,// 8
	1.5,// 9
	0.5,// 10
	3.5,// 11
	0.5,// 12
	3,	// 13
	0.5,// 14
	0.5,// 15
	0.5,// 16
	2,	// 17
	0.5,// 18
	1.5,// 19
	0.5,// 20
	0.5,// 21
	0.5,// 22
	1.5,// 23
	0.5,// 24
};

int checkHmLimit(float *pmv) {
	int i;
	
	for (i=2; i<25; i++) {
		if (pmv[i] > hmLimit[i]) {
			return 1;
		}
	}
	
	return 0;
}

typedef struct {
	//
	// Transient 
	//	
	int	action, st, lastVal[3], rocLevel;
	int32_t peakLevel, absval[3], max[3];
	int holdT, mask, capF, mode;	//, hfIx, hfCnt, lfIx, lfCnt, initF, ;
	float rscale;
	uint64_t ts1, ts2;
} TS_CNTL;

TS_CNTL	tvCntl, tcCntl;
PQ_EVENT_INFO eTrV, eTrC;

float scaleVWave(int raw) {
	return raw*tvCntl.rscale;
}

float scaleCWave(int raw) {
	return raw*tcCntl.rscale;
}

// 과도한 Transient Trigger 막기 위해 roc limit는 3~4%(90 ~ 120%)가 적당하며, holdOff Time은 200ms로 한다
void initTransientTrigger(int id) {
	TS_CNTL *ptvc = &tvCntl;
	TS_CNTL *ptcc = &tcCntl;	
	int normraw;	// overflow 조심해야 ....
	float rev_ptratio = (float)db[id].pt.PT2/db[id].pt.PT1;
	float ptratio = (float)db[id].pt.PT1/db[id].pt.PT2;
		
	// rms 단위로 peaklevel 계산하고, peak levelpeak 전압은 계측된값에 sqrt(2)를 곱한다 
	normraw = (WAVE_MAX/V_FULL)*(1<<meter[id].cntl.pga_vgain)*(db[id].pt.vnorm*rev_ptratio);	
	ptvc->peakLevel = (db[id].transient[0].level < 120) ? 0 : normraw*(db[id].transient[0].level/100.);	
	ptvc->rocLevel  = (db[id].transient[0].fastChange == 0) ? WAVE_MAX : normraw*(db[id].transient[0].fastChange/100.*31.25);
	ptvc->rscale    = V_FULL/WAVE_MAX*SQRT_2*ptratio;
	ptvc->holdT     = db[id].transient[0].holdOff;	// msec
	ptvc->mode      = 0;
	ptvc->action    = db[id].transient[0].action;
	
	// Current
	switch (db[id].ct.CT2) {
	case CT_5A:
#if 1
		normraw = WAVE_MAX/I_FULL_5AN * (1<<meter[id].cntl.pga_igain)*db[id].ct.inorm * (5/db[id].ct.CT1);
		ptcc->rscale    = I_FULL_5AN  / WAVE_MAX*SQRT_2*db[id].ct.CT1/5;		// peak(DC)로 환산			
#else
		if (getHwVersion() == 0) {
			normraw = WAVE_MAX/I_FULL_5A * (1<<meter[id].cntl.pga_igain)*db[id].ct.inorm * (5/db[id].ct.CT1);
			ptcc->rscale    = I_FULL_5A  / WAVE_MAX*SQRT_2*db[id].ct.CT1/5;		// peak(DC)로 환산
		}
		else {
			normraw = WAVE_MAX/I_FULL_5AN * (1<<meter[id].cntl.pga_igain)*db[id].ct.inorm * (5/db[id].ct.CT1);
			ptcc->rscale    = I_FULL_5AN  / WAVE_MAX*SQRT_2*db[id].ct.CT1/5;		// peak(DC)로 환산			
		}
#endif
		break;
	
	case CT_100mA:
	case CT_333mV:
		normraw = WAVE_MAX/I_FULL_100mA * (1<<meter[id].cntl.pga_igain)*db[id].ct.inorm * (100/db[id].ct.CT1);
		ptcc->rscale    = I_FULL_100mA / WAVE_MAX*SQRT_2*db[id].ct.CT1/100;	// peak(DC)로 환산
		break;
	}
	
	ptcc->peakLevel = (db[id].transient[1].level < 120) ? 0 : normraw*(db[id].transient[1].level/100.);
	if (ptcc->peakLevel > WAVE_MAX) ptcc->peakLevel = WAVE_MAX;
	ptcc->rocLevel  = (db[id].transient[1].fastChange == 0) ? WAVE_MAX : normraw*(db[id].transient[1].fastChange/100.*31.25);
	ptcc->holdT     = db[id].transient[1].holdOff;	// msec
	ptcc->mode      = 1;
	ptcc->action    = db[id].transient[1].action;
	
	printf("[U peak level=%d, roc level=%d, holdT=%d, action=%d]\n", ptvc->peakLevel, ptvc->rocLevel, ptvc->holdT, ptvc->action);
	printf("[I peak level=%d, roc level=%d, holdT=%d, action=%d]\n", ptcc->peakLevel, ptcc->rocLevel, ptcc->holdT, ptcc->action);
}

//#define	L_HF_PRE	400
//#define	L_HF_POST	1200
#define	L_HF_PRE	800
#define	L_HF_POST	2400

#define	L_LF_PRE	200
#define	L_LF_POST	1400

void getTrgFileName(char *title, uint64_t ts, char *buf) {
	struct tm ltm;
	uint32_t t = ts/1000, ms=ts%1000;

	uLocalTime(&t, &ltm);
	sprintf(buf, "%s%04d%02d%02dT%02d%02d%02d_%03d.d", 
		title, ltm.tm_year, ltm.tm_mon, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec, ms);	
}


// event 발생 위치가 pprev와 pcurr 사이 
int wavePreCaptureLF(int id, WAVE_WINDOW *pcur, WAVE_WINDOW *pprev, WAVE_WINDOW *pnext, WAVE_LF_CAP *pCap, PQ_EVT_Q *pqE) {
	int dx, sx, cnt, j, n, ms;
	int32_t *psrc;
	FILE *fp;
	FS_MSG fsmsg;
	char path[64];
		
	printf("PreCapLF: %lld, %lld, %lld : %lld\n", pprev->ts, pcur->ts, pnext->ts, pqE->Ts);
		
	ms = (pqE->Ts < pcur->ts) ? pqE->Ts - pprev->ts : pqE->Ts - pcur->ts;		
	pCap->ts = pqE->Ts;
	pCap->pos = ms/0.125;	//1600;	
	pCap->mask = pqE->mask;	
	pCap->scale = (pqE->eType == E_OC) ? meter[id].cntl.iscale : meter[id].cntl.vscale;
	
	if (pCap->pos >= N_LFCAP) {
		printf("&&& invalid pCap->pos(%d), abort capture ...\n", pCap->pos);
		return -1;
	}
			
	sx = pCap->pos-800;
	if (sx < 0) sx += 1600;
	dx = 0;
	cnt = 1600-sx;			
	// 이벤트 발생시점을 기준으로 800 이전 sample 부터 복사
	for (n=cnt*sizeof(int), j=0; j<3; j++) {
		psrc = (pqE->eType == E_OC) ? &pprev->lI.w[j][sx] : &pprev->lU.w[j][sx];
		memcpy(&pCap->lW[j][dx], psrc, n);
	}						
	//
	sx = 0;
	dx += cnt;		
	cnt = 3200-dx;
	if (cnt > 1600) cnt = 1600;	
	for (n=cnt*sizeof(int), j=0; j<3; j++) {
		psrc = (pqE->eType == E_OC) ? &pprev->lI.w[j][sx] : &pprev->lU.w[j][sx];
		memcpy(&pCap->lW[j][dx], psrc, n);
	}						
	// 
	sx = 0;
	dx += cnt;
	cnt = 3200-dx;	
	for (n=cnt*sizeof(int), j=0; j<3; j++) {
		psrc = (pqE->eType == E_OC) ? &pprev->lI.w[j][sx] : &pprev->lU.w[j][sx];
		memcpy(&pCap->lW[j][dx], psrc, n);
	}					

	// FS_Task 에 쓰기 요청한다 
	if (pqE->eType == E_SAG) {
		strcpy(path, "\\Trg_PQ\\");
		strcat(path, "WSAG_");		
	}
	else if (pqE->eType == E_SWELL) {
		strcpy(path, "\\Trg_PQ\\");
		strcat(path, "WSWL_");
	}
	else if (pqE->eType == E_OC) {
		strcpy(path, "\\Trg_PQ\\");
		strcat(path, "WOC_");
	}
	else
		return 0;
	
	getTrgFileName(path, pqE->Ts, fsmsg.fname);
	strcpy(fsmsg.mode, "wb");
	fsmsg.pbuf = pCap;
	fsmsg.size = sizeof(*pCap);
	putFsQ(&fsmsg);
	printf("wavePreCaptureLF1 PQ Event: %s\n", fsmsg.fname);
	
	return 0;
}

// npre: # of pre sample(400/200)
// n : window length(6400/1600)
// pprev(6400) | pcur(6400) | pnext(6400)

void waveCaptureHF(int id, int i, WAVE_WINDOW *pcur, WAVE_WINDOW *pprev, WAVE_WINDOW *pnext, WAVE_HF_CAP *pCap, int mode) {
	int j, cnt, dx, sx, ix, n; 			
	int npre, npost, nwin;
	WAVE_PHASE_HF *psrc;
	
	// 이벤트 발생 시점을 기준으로 pre, post sample data 복사한다 
	//  pre trigger sample data가 prev, current buffer에 분산되있다
	npre  = L_HF_PRE;
	npost = L_HF_POST;
	nwin  = L_HFWVWIN;
	
	pCap->pos = i;
	pCap->srate = 32000;
	pCap->mask = 0;
	pCap->scale = meter[id].cntl.vscale;
		
	ix = i-npre; 			
	if (ix < 0) {
		// i = 100, ix=100-800=-700
		// 이전 wave buffer 에서 복사
		dx = 0;		
		sx = nwin + ix;		// 6400 - 700 = 5700
		cnt = -ix;				// 700
	
		psrc = (WAVE_PHASE_HF *)((mode == 0) ? &pprev->hU : &pprev->hI);		
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->hD[j][dx], &psrc->w[j][sx], n);	//memcpy(&pCap->hI[j][dx], &pprev->hI[j][sx], n);
		}
		dx += cnt;
		
		// 현재 wave buffer에서 복사				
		sx = 0;		
		cnt = npre - cnt;	// 800 - 700 = 100
		psrc = (WAVE_PHASE_HF *)((mode == 0) ? &pcur->hU : &pcur->hI);		
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->hD[j][dx], &psrc->w[j][sx], n);	//memcpy(&pCap->hI[j][dx], &pcur->hI[j][sx], n);
		}		
		dx += cnt;
	}
	//  pretrigger sample data가 현재 buffer에 있다
	else {
		dx = 0;
		sx = ix;
		cnt = npre;
		psrc = (WAVE_PHASE_HF *)((mode == 0) ? &pcur->hU : &pcur->hI);	
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->hD[j][dx], &psrc->w[j][sx], n);	//memcpy(&pCap->hI[j][dx], &pcur->hI[j][sx], n);
		}
		dx += cnt;
	}	
	
	// post trigger sample data가 current, next buffer에 분산되 있다
	// i = 6000, ix=6000+2400=8400
	ix = i + npost;	
	if (ix > nwin) {
		sx = i;	
		cnt = nwin - i;	// 6400-6000=400
		psrc = (WAVE_PHASE_HF *)((mode == 0) ? &pcur->hU : &pcur->hI);			
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->hD[j][dx], &psrc->w[j][sx], n);	//memcpy(&pCap->hI[j][dx], &pcur->hI[j][sx], n);
		}
		dx += cnt;
		
		sx = 0;
		cnt = npost-cnt;	// 2400-400=2000		
		psrc = (WAVE_PHASE_HF *)((mode == 0) ? &pnext->hU : &pnext->hI);	
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->hD[j][dx], &psrc->w[j][sx], n);	//memcpy(&pCap->hI[j][dx], &pnext->hI[j][sx], n);
		}		
		dx += cnt;
	}
	else {
		sx = i;
		cnt = npost;
		psrc = (WAVE_PHASE_HF *)((mode == 0) ? &pcur->hU : &pcur->hI);	
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->hD[j][dx], &psrc->w[j][sx], n);	//memcpy(&pCap->hI[j][dx], &pcur->hI[j][sx], n);
		}
	}
	
#ifdef _Transient_LF_Cap	
	//
	// Low Freq. Capture
	//
	npre  = L_LF_PRE;
	npost = L_LF_POST;
	nwin  = L_LFWVWIN;	
	
	i = i/4;	// down sampling 고려
	ix = i-npre; 			
	if (ix < 0) {
		// 이전 wave buffer 에서 복사
		dx = 0;		
		sx = nwin + ix;		
		cnt = -ix;
		
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->lU[j][dx], &pprev->lU[j][sx], n);
		}
		dx += cnt;
		
		// 현재 wave buffer에서 복사				
		sx = 0;		
		cnt = npre - cnt;
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->lU[j][dx], &pcur->lU[j][sx], n);
		}		
		dx += cnt;
	}
	//  pretrigger sample data가 현재 buffer에 있다
	else {
		dx = 0;
		sx = ix;
		cnt = npre;
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->lU[j][dx], &pcur->lU[j][sx], n);
		}
		dx += cnt;
	}	
	
	// post trigger sample data가 current, next buffer에 분산되 있다
	ix = i + npost;
	if (ix > nwin) {
		sx = i;
		cnt = nwin - i;
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->lU[j][dx], &pcur->lU[j][sx], n);
		}
		dx += cnt;
		
		sx = 0;
		cnt = npost-cnt;		
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->lU[j][dx], &pnext->lU[j][sx], n);
		}		
		dx += cnt;
	}
	else {
		sx = i;
		cnt = npost;
		for (n=cnt*sizeof(int), j=0; j<3; j++) {
			memcpy(&pCap->lU[j][dx], &pcur->lU[j][sx], n);
		}
	}
#endif	
}

int TransientEvent(int id, WAVE_WINDOW *pww, int bx, TS_CNTL *ptrg, int mode) {
	int i, j, ph, ix, cnt, *pbuf;
	int trType=0, res=0, detect, etype=0;
	WAVE_WINDOW	*pcur, *pprev, *pnext;
	WAVE_PHASE_HF *wph;
	int 	fchgCount[3];	// fastChangeEvent Count
	FS_MSG	fsmsg;
	char path[64];
		
	pnext = &pww[bx];
	pcur  = &pww[(bx+2)%3];		
	pprev = &pww[(bx+1)%3];
		
	// disable 조건 검사(peak level, online)
	if (meter[id].cntl.online == 0 || ptrg->action == 0 || ptrg->peakLevel == 0) {
		return res;
	}
	
	wph = (mode == 0) ? &pcur->hU : &pcur->hI;
	
	// roc 계산 위한 초기값 복사 
	for (ph=0; ph<3; ph++) {
		ptrg->lastVal[ph] = wph->w[ph][0];
	}

	i = 1;	// 1부터 비교
	// 10/12 cycle 동안 수집된 데이터에 대해 transient 검사한다 	
	// 2025-3-20, WaveCapture가 연속적이지 않은 경우 이벤트 발생할 수 있으므로 2회연속 발생시 처리한다
	memset(fchgCount, 0, sizeof(fchgCount));
	
	while (i<L_HFWVWIN) {		
		for (detect=0, ph=0; ph<3; ph++) {			
			ptrg->absval[ph] = abs(wph->w[ph][i]);
			
			etype = 0;
			if (ptrg->absval[ph] > ptrg->peakLevel) {
				// 25-08-13 jskim : rms 값이 peak값보다 클경우(transient가 아닌 고전압 또는 설정오류)
				if((mode == 0 && meter[0].meter.U[ph] < ptrg->peakLevel) ||(mode == 1 && meter[id].meter.I[ph] < ptrg->peakLevel))
					etype = 1;
			}
			else if (abs(wph->w[ph][i]-ptrg->lastVal[ph]) > ptrg->rocLevel) {				
				etype = 2;
			}
			// Test Mode for Volt
			else if (mode == 0 && meter[id].cntl.TrVTrg) {
				etype = 3;
			}
			// Test Mode for Current
			else if (mode != 0 && meter[id].cntl.TrCTrg) {				
				etype = 4;
			}	
			
			switch (etype){
			case 2:
				// 2025-3-20, WaveCapture가 연속적이지 않은 경우 이벤트 발생할 수 있으므로 3회연속 발생시 처리한다
				if (++fchgCount[ph] >= 3) {
					//printf("FastChange TransientEvent(%d, %d)\n", ph, etype);
					detect |= (1<<ph);				
				}				
				break;
			case 1:
			case 3:
			case 4:
				fchgCount[ph] = 0;
				printf("TransientEvent(%d, %d)\n", ph, etype);
				detect |= (1<<ph);				
				break;
			}
			
			ptrg->lastVal[ph] = wph->w[ph][i];	// last <- new value
		}		
			
		// 레벨 검사 
		// 3상중에 하나라도 발생하면 ON, HoldTimer 가동
		if (ptrg->st == 0) {							
			// level 검출
			// 32k sample : -20us ~ 60us
			// 8k  sample : 발생전 1 cycle, 발생 후 15 cycle 저장
			if (detect) {			
				printf(">>> TrE(%d): ix=%d, phase=%x, Flag=%d\n", mode, i, detect, meter[id].cntl.TrVTrg || meter[id].cntl.TrCTrg);								
				if (mode == 0) meter[id].cntl.TrVTrg = 0;
				else 	meter[id].cntl.TrCTrg = 0;
				
				ptrg->st = 1;
				ptrg->ts1 = ptrg->ts2 = sysTick64;
				ptrg->mask = detect;
				for (ph=0; ph<3; ph++) {
					ptrg->max[ph] = ptrg->absval[ph];
				}
								
				// wave capture
				if (ptrg->action == 2) {
					wbCap.ts = sysTick64;
					waveCaptureHF(id, i, pcur, pprev, pnext, &wbCap, mode);				
					res = ptrg->capF = 1;				
				}			
			}
		}
		else if (ptrg->st == 1) {	
			// holdOff Timer 갱신
			if (detect) {
				ptrg->mask |= detect;
				for (ph=0; ph<3; ph++) {
					if (ptrg->max[ph] < ptrg->absval[ph]) ptrg->max[ph] = ptrg->absval[ph];
				}
				ptrg->ts2 = sysTick64;	
			}
			// holdTimer 기간동안 데이타 skip 한다 
			else if ((sysTick64-ptrg->ts2) > ptrg->holdT) {
				ptrg->st = 0;				
				
				// event list에 추가할지 ???
				if (mode == 0) {
					ppqEvtCnt->tvc++;
					
					eTrV.type = E_TrV;
					eTrV.mask = ptrg->mask;
					eTrV.startTs = ptrg->ts1;
					eTrV.endTs   = ptrg->ts2;
					for (ph=0; ph<3; ph++) eTrV.Val[ph] = ptrg->max[ph];
					pushEvent(id, &eTrV);	
					printf(">>> EoTrV(%d): %d\n", mode, i);
				}
				else {
					ppqEvtCnt->tcc++;
					
					eTrC.type = E_TrC;
					eTrC.mask = ptrg->mask;
					eTrC.startTs = ptrg->ts1;
					eTrC.endTs   = ptrg->ts2;
					for (ph=0; ph<3; ph++) eTrC.Val[ph] = ptrg->max[ph];
					pushEvent(id, &eTrC);	
					printf(">>> EoTrC(%d): %d\n", mode, i);
				}				
			}
		}				
		i++;
	}
	
	if (ptrg->capF) {		
		if (mode == 0) 
			strcpy(path, "\\Trg_TVC\\WTV_");		
		else
			strcpy(path, "\\Trg_TVC\\WTC_");		
		getTrgFileName(path, ptrg->ts1, fsmsg.fname);		
		strcpy(fsmsg.mode, "wb");
		fsmsg.pbuf = &wbCap;
		fsmsg.size = sizeof(wbCap);
		putFsQ(&fsmsg);
		
		ptrg->capF = 0;
		printf("Capture Transient Event: %s\n", fsmsg.fname);			
	}
	
	return res;
}
//// transient 발생하면 전후 5-6 cycle 분량의 wave capture 한다
//// 이벤트 리스트에 추가 (???)
//// 5~6 cycle 쓸때 걸리는 시간 : 100ms 이하이나 많이 걸릴때도 있다 (500ms)
//int TransientVoltage(WAVE_WINDOW *pww, int bx, TS_CNTL *ptrg) {
//	int i, j, ph, ix, cnt, *pbuf;
//	int trType=0, res=0, detect;
//	WAVE_WINDOW	*pcur, *pprev, *pnext;
//	FS_MSG	fsmsg;
//	char path[64];
//		
//	pnext = &pww[bx];
//	pcur  = &pww[(bx+2)%3];		
//	pprev = &pww[(bx+1)%3];
//		
//	// disable 조건 검사(peak level, online)
//	if (meter[id].cntl.online == 0 || ptrg->action == 0 || ptrg->peakLevel == 0) {
//		return res;
//	}
//	
//	// roc 계산 위한 초기값 복사 
//	for (ph=0; ph<3; ph++) {
//		ptrg->lastVal[ph] = pcur->hU[ph][0];
//	}

//	i = 0;
//	// 10/12 cycle 동안 수집된 데이터에 대해 transient 검사한다 	
//	while (i<L_HFWVWIN) {		
//		for (detect=0, ph=0; ph<3; ph++) {
//			if (abs(pcur->hU[ph][i]) > ptrg->peakLevel || abs(pcur->hU[ph][i]-ptrg->lastVal[ph]) > ptrg->rocLevel) {				
//				detect = i;
//			}		
//			ptrg->lastVal[ph] = pcur->hU[ph][i];	// last <- new value
//		}		
//			
//		// 레벨 검사 
//		// 3상중에 하나라도 발생하면 ON, HoldTimer 가동
//		if (ptrg->st == 0) {							
//			// level 검출
//			// 32k sample : -20us ~ 60us
//			// 8k  sample : 발생전 1 cycle, 발생 후 15 cycle 저장
//			if (detect) {			
//				printf(">>> TrV: ix=%d\n", i);				
//				
//				ptrg->st = 1;
//				// 발생시간
//				ptrg->timer = ptrg->holdT;
//				ptrg->ts1 = ptrg->ts2 = sysTick64;
//				
//				// wave capture
//				if (ptrg->action == 2) {
//					wbCap.ts = sysTick64;
//					waveCaptureHF(i, pcur, pprev, pnext, &wbCap, 0);				
//					res = ptrg->capF = 1;				
//				}
//				// event list에 추가할지 ???
//				ppqEvtCnt->tvc++;
//			}
//		}
//		else if (ptrg->st == 1) {	
//			// holdOff Timer 갱신
//			if (detect) {
//				ptrg->ts2 = sysTick64;	
//			}
//			// holdTimer 기간동안 데이타 skip 한다 
//			else if ((sysTick64-ptrg->ts2) > ptrg->holdT) {
//				ptrg->st = 0;
//				printf(">>> EoTrV: %d\n", i);
//			}
//		}				
//		i++;
//	}
//	
//	if (ptrg->capF) {					
//		strcpy(path, "\\Trg_TVC\\WTV_");		
//		getTrgFileName(path, ptrg->ts1, fsmsg.fname);
//		
//		strcpy(fsmsg.mode, "wb");
//		fsmsg.pbuf = &wbCap;
//		fsmsg.size = sizeof(wbCap);
//		putFsQ(&fsmsg);
//		
//		ptrg->capF = 0;
//		printf("Capture Transient Voltage: %s\n", fsmsg.fname);			
//	}
//	
//	return res;
//}

//int TransientCurrent(WAVE_WINDOW *pww, int bx, TS_CNTL *ptrg) {
//	int i, j, ph, ix, cnt, *pbuf;
//	int trType=0, res=0, detect;
//	WAVE_WINDOW	*pcur, *pprev, *pnext;
//	FS_MSG	fsmsg;
//	char path[64];
//		
//	pnext = &pww[bx];
//	pcur  = &pww[(bx+2)%3];		
//	pprev = &pww[(bx+1)%3];
//		
//	// disable 조건 검사(peak level, online)
//	if (meter[id].cntl.online == 0 || ptrg->action == 0 || ptrg->peakLevel == 0) {
//		return res;
//	}
//	
//	for (ph=0; ph<3; ph++) {
//		ptrg->lastVal[ph] = pcur->hI[ph][0];
//	}

//	i = 0;
//	// 10/12 cycle 동안 수집된 데이터에 대해 transient 검사한다 	
//	while (i<L_HFWVWIN) {		
//		for (detect=0, ph=0; ph<3; ph++) {
//			if (abs(pcur->hI[ph][i]) > ptrg->peakLevel || abs(pcur->hI[ph][i]-ptrg->lastVal[ph]) > ptrg->rocLevel) {
//				detect = 1;
//			}		
//			ptrg->lastVal[ph] = pcur->hI[ph][i];	// last <- new value
//		}		
//			
//		// 레벨 검사 
//		// 3상중에 하나라도 발생하면 ON, HoldTimer 가동
//		if (ptrg->st == 0) {							
//			// level 검출
//			// 32k sample : -20us ~ 60us
//			// 8k  sample : 발생전 1 cycle, 발생 후 15 cycle 저장
//			if (detect) {			
//				ptrg->st = 1;
//				ptrg->timer = ptrg->holdT;
//				ptrg->ts1 = ptrg->ts2 = sysTick64;
//				
//				if (ptrg->action == 2) {
//					wbCap.ts = sysTick64;
//					waveCaptureHF(i, pcur, pprev, pnext, &wbCap, 1);				
//					ptrg->capF = 1;
//				}
//				//printf(">>> Capture: %d, %lld\n", i, ptrg->ts1);
//				ppqEvtCnt->tcc++;
//			}
//		}
//		else if (ptrg->st == 1) {						
//			// holdOff Timer 갱신
//			if (detect) {
//				ptrg->ts2 = sysTick64;
//			}
//			// holdTimer 기간동안 데이타 skip 한다 
//			else if ((sysTick64-ptrg->ts2) > ptrg->holdT) {
//				ptrg->st = 0;
//				printf(">>> EoTrC: %d\n", i);
//			}
//		}				
//		i++;
//	}
//	
//	if (ptrg->capF) {					
//		strcpy(path, "\\Trg_TVC\\WTC_");		
//		getTrgFileName(path, ptrg->ts1, fsmsg.fname);
//		
//		strcpy(fsmsg.mode, "wb");
//		fsmsg.pbuf = &wbCap;
//		fsmsg.size = sizeof(wbCap);
//		putFsQ(&fsmsg);
//		
//		ptrg->capF = 0;
//		printf("Capture Transient Current Event: %s\n", fsmsg.fname);			
//	}
//}




void rmDcOffset(WAVE_8K_BUF *pwb) {
	int i, j, n=1600, dcOs;
	float sum;
	
	for (i=0; i<3; i++) {
		for (sum=0, j=0; j<n; j++) {
			sum += pwb->U[i][j];
		}
		dcOs = sum / n;
		for (j=0; j<n; j++) {
			pwb->U[i][j] -= dcOs;
		}		
	}
	
	for (i=0; i<3; i++) {
		for (sum=0, j=0; j<n; j++) {
			sum += pwb->I[i][j];
		}
		dcOs = sum / n;
		for (j=0; j<n; j++) {
			pwb->I[i][j] -= dcOs;
		}		
	}	
}

// 8k sample을 16 bit로 변환한다 
// ADC Range: 67,107,786 -> 27bit
void copyModbusWaveData(int id) {
	WAVE_WINDOW *pww = &wvblk[id].ww[wvblk[id].ix];
	WAVE_PHASE_LF *wv, *wi;
	int i, j, k, prev, ix=0, dx;
	float vscl[3], iscl[3];
	
	// scale이 작으면 계단파로 보이는 문제 있다
	for (j=0; j<3; j++) {
		vscl[j] = (1 + (pcal->vgain[id][j]/134217728.)) * meter[id].cntl.wv_vscale * sqrt(2);
		iscl[j] = (1 + (pcal->igain[id][j]/134217728.)) * meter[id].cntl.wv_iscale * sqrt(2);
	}
	
	// cycle 시작지점 검색
	//prev = pww->lU.w[0][0];
	wv = &pww->lU;	
	wi = &pww->lI;	
	prev = wv->w[0][0];
	
	for (i=1; i<1600; i++) {
		if (prev < 0 && wv->w[0][i] >= 0) {
			ix = i;
			break;
		}
		else {
			prev = wv->w[0][i];
		}
	}
	// Modbus Wave영역으로 데이터 복사
	// 16bit로 Down Scale 위해 scale factor 곱한다	
	if (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT) {		
		for (i=0, dx=ix; i<160; i++, dx+=2) meter[id].wv.U[0][i] =  wv->w[0][dx]*vscl[0];
		for (i=0, dx=ix; i<160; i++, dx+=2) meter[id].wv.U[1][i] = -wv->w[2][dx]*vscl[2];
		for (i=0, dx=ix; i<160; i++, dx+=2) meter[id].wv.U[2][i] =  wv->w[1][dx]*vscl[1];
	}
	else {
		for (i=0, dx=ix; i<160; i++, dx+=2) {		
			meter[id].wv.U[0][i] = wv->w[0][dx]*vscl[0];
			meter[id].wv.U[1][i] = wv->w[1][dx]*vscl[1];
			meter[id].wv.U[2][i] = wv->w[2][dx]*vscl[2];
		}	
	}
	
	for (i=0, dx=ix; i<160; i++, dx+=2) {		
		meter[id].wv.I[0][i] = wi->w[0][dx]*iscl[0];
		meter[id].wv.I[1][i] = wi->w[1][dx]*iscl[1];
		meter[id].wv.I[2][i] = wi->w[2][dx]*iscl[2];
	}		
	
	// 3P4W 이면 선간 전압파형 계산한다 
	if (db[id].pt.wiring == WM_3LN3CT) {
		// cal. Upp
		for (j=0; j<3; j++) {
			k = (j+1)%3;
			for (i=0; i<160; i++) meter[id].wv.Upp[j][i] = meter[id].wv.U[k][i] - meter[id].wv.U[j][i];
		}
	}
	else {
		for (j=0; j<3; j++) {
			for (i=0; i<160; i++) meter[id].wv.Upp[j][i] = meter[id].wv.U[j][i];
		}
	}
}

int __iabs(int val) {
	return (val < 0) ? -val : val;
}

int copyGUIWaveData(int id,int sel) {
	WAVE_WINDOW *pww = &wvblk[id].ww[wvblk[id].ix];
	WAVE_PHASE_HF *wv, *wi;
	WAVEFORM_GUI *pwvgui = &meter[id].cntl.wvGUI;
	int i, j, k, prev, ix=0, absVal;	
	float vscl[3], iscl[3];
	
	// PICKUP_VOLT 이하 값은 표시하지 않는다 
	if (sel == 0 && meter[id].meter.U[0] == 0 && meter[id].meter.U[1] == 0 && meter[id].meter.U[2] == 0) {
		return -1;
	}
	else if (sel == 1 && meter[id].meter.Upp[0] == 0 && meter[id].meter.Upp[1] == 0 && meter[id].meter.Upp[2] == 0) {
		return -1;
	}
	else if (sel == 2 && meter[id].meter.I[0] == 0 && meter[id].meter.I[1] == 0 && meter[id].meter.I[2] == 0) {
		return -1;
	}
	
	for (j=0; j<3; j++) {
		vscl[j] = (1 + (pcal->vgain[id][j]/134217728.)) * meter[id].cntl.wv_vscale * sqrt(2);
		iscl[j] = (1 + (pcal->igain[id][j]/134217728.)) * meter[id].cntl.wv_iscale * sqrt(2);
	}
	
	// cycle 시작지점 검색, increment를 1로 하면 glitch에 영향 받는다 
	wv = &pww->hU;
	wi = &pww->hI;
	prev = wv->w[0][0];
	//prev = pww->hU[0][0];
	for (i=1; i<640; i+=5) {
		if (prev < 0 && wv->w[0][i] > 0) {
			//printf("copyGUIWaveData, p=%d, n=%d, n-1=%d, i=%d\n", prev, pww->hU[0][i], pww->hU[0][i-10], i);
			ix = i;
			break;
		}
		else {
			prev = wv->w[0][i];
		}
	}	
	
	// 
	// vscale = (V_FULL/RMS_MAX)*(db[id].pt.PT1/db[id].pt.PT2)/4;
	// GUI Wave영역으로 데이터 복사
	if (sel == 0 || sel == 1) {
		if (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT) {
			pwvgui->Umax[0] = pwvgui->Umax[1] = pwvgui->Umax[2] = 0;
		
			j = 0;
			k = ix;			
			for (i=0; i<640; i++, k++) {		
				pwvgui->U[j].ph[i] = wv->w[j][k]*vscl[j];
				absVal = __iabs(pwvgui->U[j].ph[i]);
				if (pwvgui->Umax[j] < absVal) pwvgui->Umax[j] = absVal;			
			}			
			
			j = 1;
			k = ix;
			for (i=0; i<640; i++, k++) {		
				pwvgui->U[j].ph[i] = -(wv->w[2][k]*vscl[2]);
				absVal = __iabs(pwvgui->U[j].ph[i]);
				if (pwvgui->Umax[j] < absVal) pwvgui->Umax[j] = absVal;			
			}							
			
			j = 2;
			k = ix;
			for (i=0; i<640; i++, k++) {		
				pwvgui->U[j].ph[i] = wv->w[1][k]*vscl[1];
				absVal = __iabs(pwvgui->U[j].ph[i]);
				if (pwvgui->Umax[j] < absVal) pwvgui->Umax[j] = absVal;			
			}																		
		}
		else {
			for (j=0; j<3; j++) {		
				k = ix;
				pwvgui->Umax[j] = 0;
				for (i=0; i<640; i++, k++) {		
					pwvgui->U[j].ph[i] = wv->w[j][k]*vscl[j];
					absVal = __iabs(pwvgui->U[j].ph[i]);
					if (pwvgui->Umax[j] < absVal) pwvgui->Umax[j] = absVal;			
				}					
			}	
		}
		
		if (sel == 1) {
			if (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT) {
				// cal. Upp	
				for (j=0; j<3; j++) {				
					pwvgui->UppMax[j] = 0;
					for (i=0; i<640; i++) {		
						pwvgui->Upp[j].ph[i] = pwvgui->U[j].ph[i];
						if (pwvgui->UppMax[j] < __iabs(pwvgui->Upp[j].ph[i])) pwvgui->UppMax[j] = pwvgui->Upp[j].ph[i];
					}					
				}		
			}
			else {
				// cal. Upp	
				for (j=0; j<3; j++) {				
					k = (j+1)%3;
					pwvgui->UppMax[j] = 0;
					for (i=0; i<640; i++) {		
						pwvgui->Upp[j].ph[i] = pwvgui->U[k].ph[i] - pwvgui->U[j].ph[i];
						if (pwvgui->UppMax[j] < __iabs(pwvgui->Upp[j].ph[i])) pwvgui->UppMax[j] = pwvgui->Upp[j].ph[i];
					}					
				}					
			}
		}
	}
	
	
	if (sel == 2) {
		for (j=0; j<3; j++) {		
			k = ix;
			pwvgui->Imax[j] = 0;
			for (i=0; i<640; i++, k++) {		
				pwvgui->I[j].ph[i] = wi->w[j][k]*iscl[j];
				absVal = __iabs(pwvgui->I[j].ph[i]);			
				if (pwvgui->Imax[j] < absVal) pwvgui->Imax[j] = absVal;
			}					
		}	
	}
	
	if (sel == 0) {
		printf("MAX: U(%d,%d,%d)\n", pwvgui->Umax[0], pwvgui->Umax[1], pwvgui->Umax[2]);
	}	
	else if (sel == 1) {
		printf("MAX: U(%d,%d,%d), Upp(%d,%d,%d)\n", pwvgui->Umax[0], pwvgui->Umax[1], pwvgui->Umax[2], 
			pwvgui->UppMax[0], pwvgui->UppMax[1], pwvgui->UppMax[2]);
	}
	else if (sel == 2) {
		printf("MAX: I(%d,%d,%d)\n", pwvgui->Imax[0], pwvgui->Imax[1], pwvgui->Imax[2]);		
	}
	return 0;
}

// 32ksps 에서 사용한다 
// Dc Offset과  Copy 작업을 수행한다
int copyToWaveWindow(int id, WAVE_PGBUF *pwQ, WAVE_WINDOW *pww)
{
	int dx, i, j, sx, wr, res=0;	
	CNTL_DATA	*_pcntl = &meter[id].cntl;
	
	wr=pwQ->re;
	for (dx=0, i=0; i<(PG_BUF_CNT/2); i++) {
		int32_t *pwv = pwQ->wb[wr++].buf;
		for (sx=0, j=0; j<16; j++) {					
			pww->hI.w[0][dx] = pwv[sx++];
			pww->hU.w[0][dx] = pwv[sx++];
			pww->hI.w[1][dx] = pwv[sx++];
			pww->hU.w[1][dx] = pwv[sx++];
			pww->hI.w[2][dx] = pwv[sx++];			
			pww->hU.w[2][dx] = pwv[sx++];
			dx++;
		}
	}			
	pww->ts = pwQ->ts;
	pwQ->re = (wr >= PG_BUF_CNT) ? 0 : wr;
	pwQ->halfFull = 0;
	
	for (i=0; i<3; i++) {
		pww->vrms[i] = _pcntl->U[i];
		pww->irms[i] = _pcntl->I[i];
	}
		
	return 0;
}

int copyToWaveWindowWithDcOffSet(int id, WAVE_PGBUF *pwQ, int hs, WAVE_WINDOW *pww) {
	int64_t sum[6]; 	
	int dx, i, j, sx, wr;
	int32_t *pwv;
	int res = 0, n;
	
	CNTL_DATA *pcntl = &meter[id].cntl;
	wr = pwQ->re;
	
	if (pcntl->calDcOs) {					
		memset(sum, 0, sizeof(sum));
		for (dx=0, i=0; i<400; i++) {
			pwv = pwQ->wb[wr++].buf;
			for (sx=0, j=0; j<16; j++) {					
				sum[0] += pwv[sx++];
				sum[1] += pwv[sx++];
				sum[2] += pwv[sx++];
				sum[3] += pwv[sx++];
				sum[4] += pwv[sx++];			
				sum[5] += pwv[sx++];
				dx++;
			}
		}				

		n = (hs) ? 6400 : 1600;		
		pcal->idcos[id][0] = -sum[0]/n;
		pcal->vdcos[id][0] = -sum[1]/n;
		pcal->idcos[id][1] = -sum[2]/n;
		pcal->vdcos[id][1] = -sum[3]/n;
		pcal->idcos[id][2] = -sum[4]/n;
		pcal->vdcos[id][2] = -sum[5]/n;	
		
		pcntl->calDcOs = 0;
		return 0;
	}
	else {
		for (dx=0, i=0; i<400; i++) {
			pwv = wQ[id].wb[wr++].buf;
			for (sx=0, j=0; j<16; j++) {					
				pww->hI.w[0][dx] = pwv[sx++] + pcal->idcos[id][0];
				pww->hU.w[0][dx] = pwv[sx++] + pcal->vdcos[id][0];
				pww->hI.w[1][dx] = pwv[sx++] + pcal->idcos[id][1];
				pww->hU.w[1][dx] = pwv[sx++] + pcal->vdcos[id][1];
				pww->hI.w[2][dx] = pwv[sx++] + pcal->idcos[id][2];			
				pww->hU.w[2][dx] = pwv[sx++] + pcal->vdcos[id][2];
				dx++;
			}
		}			
		res = 1;
	}
	
	pww->ts = pwQ->ts;
	//wQ[id].re = (wr >= PG_BUF_CNT) ? 0 : wr;
	
	for (i=0; i<3; i++) {
		pww->vrms[i] = pcntl->U[i];
		pww->irms[i] = pcntl->I[i];
	}
			
	return res;
}

// hs: high speed (32k)
void downSampling(WAVE_WINDOW *pww, int hs)
{
	int i, sx, inc;
	
	inc = (hs) ? 4 : 1;
	
	// 32k sample을 8k sample로 down sampling 한다 		
	// Down Sampling(32k to 8k)	// 6400 sample -> 1600 sample			
	for (sx=0, i=0; i<L_LFWVWIN; i++, sx+=inc) {
		pww->lI.w[0][i] = pww->hI.w[0][sx];
		pww->lU.w[0][i] = pww->hU.w[0][sx];
		pww->lI.w[1][i] = pww->hI.w[1][sx];
		pww->lU.w[1][i] = pww->hU.w[1][sx];
		pww->lI.w[2][i] = pww->hI.w[2][sx];
		pww->lU.w[2][i] = pww->hU.w[2][sx];
	}		
}



int copyFftData(int id, WAVE_WINDOW *pww,	uint32_t *ptick_10s)
{
	// FFT task로 1600 sample을 공급한다
	// FFT buffer가 비워있거나 10s가 경과시 fft sample  data를 공급한다  
	if (wbFFT8k[id].fr == wbFFT8k[id].re && sysTick10s != *ptick_10s) {			
		*ptick_10s = sysTick10s;
		//printf("ts: %lld\n", pww->ts);
		// Copy wbCap8k to wbFFT8k
		memcpy(wbFFT8k[id].vrms, pww->vrms, sizeof(pww->vrms));
		memcpy(wbFFT8k[id].irms, pww->irms, sizeof(pww->irms));
		
		memcpy(wbFFT8k[id].I, &pww->lI, sizeof(wbFFT8k[id].I));
		memcpy(wbFFT8k[id].U, &pww->lU, sizeof(wbFFT8k[id].U));
		
		// 32k sample adc range : 67,107,786 
		// 8k  sample adc range : 74,518,668			
#ifdef _TEST_FUNC							
		if (initF == 0) {				
			// FFT 연산을 위해 10/12 cycle 데이터 필요하다
			for (i=0; i<1600; i++) {
				pSp->sample[i] = pww->hU[0][i]>>12;
			}					
		}					
#endif							
		
		wbFFT8k[id].fr++;					
		os_evt_set(0x1, tid_fft);
		return 1;
	}		
	else {
		return 0;
	}
}

// Meter_Scan Task로 부터 매 200ms 마다 호출된다
void Wave_Task(void *arg)
{
	int id, i, j, dx, sx, initF=0, bx, bF=0, wr, re, mask,cnt;
	int32_t *pwv;
	WAVE_WINDOW *pww;
	uint64_t Ts;
	uint32_t tick10s[2];
//	int64_t sum[6]; 
	int64_t sum[7]; 
  uint32_t notificationValue;
	
	tick10s[0] = tick10s[1] = sysTick10s;
	_enableTaskMonitor(Tid_Wave, 50);
	
	while (1) {
#ifdef __FREERTOS		
		xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);
#else
      os_evt_wait_and(0x1, 0xffff);
#endif		
		meter[0].cntl.wdtTbl[Tid_Wave].count++;
		if(getHwCh()== 0)
		    cnt = 2;
		else
			cnt = 1;
		for (id=0; id<cnt; id++) {
			if (wQ[id].halfFull) {
				//printf("--> ready wave buffer full, id=%d, ix=%d, re=%d\n", id, wvblk[id].ix, wQ[id].re);
				// 10/12 cycle동안 수집한 wave data(32k:400 page, 8k: 100 page )를 
				// ring buffer에서 추출하여 wb32k buffer로 이동한다 
				// - 32k: 6400 sample = 400*16
				// -  8k: 1600 sample = 100*16
				bx = wvblk[id].ix;
				pww = &wvblk[id].ww[bx];				

				// 32k -> 6400sample(400 pg), 8k -> 1600 sample(100 pg)
				wr = copyToWaveWindow(id, &wQ[id], pww);	
				
				if (++bx >= 3) {
					bx = 0;
					bF = 1;
				}
				wvblk[id].ix = bx;									
				
				// down sampleing
				downSampling(pww, wQ[id].hs);
				
				// copy fft data
				copyFftData(id, pww, &tick10s[id]);				
			}
		}

#ifdef __FREERTOS
			if (tid_fft != 0)
            xTaskNotify(tid_fft, 0x1, eSetBits);
#else
//			os_evt_set(0x1, tid_fft);
#endif
	}
}

float	makeRdData(float v, int i, int j) {
	float	val, val1;

	val1 = rand()%10;
	val = v+i+(val1/100) * j;

	return val;
}

void makeRandomData(void) {
	int	id, i,j,k, l;

	for(id=0; id<2; id++) {
		meter[id].meter.utc = sysTick1s;
		meter[id].meter.Freq = makeRdData(60, 0,7);
		meter[id].meter.Temp = makeRdData(25, 0,7);
		for(i=0; i<3; i++) {
			meter[id].meter.U[i] = makeRdData(220, i,5);
			meter[id].meter.Upp[i] = makeRdData(380, i,5);
			meter[id].meter.I[i] = makeRdData(10, i,8);
			meter[id].meter.P[i] = makeRdData(2200*0.866+i*10, i,7);
			meter[id].meter.Q[i] = makeRdData(300*0.2588+i*10, i,7);
			meter[id].meter.S[i] = makeRdData(2200, i,6);
			meter[id].meter.PF[i] = makeRdData(85, i,6);
			meter[id].meter.dPF[i] = makeRdData(75, i,7);
			meter[id].meter.D[i] = makeRdData(95, i,7);
			meter[id].meter.Pangle[i] = makeRdData(67, i,7);
			meter[id].meter.UUndev[i] = makeRdData(25, i,7);
			meter[id].meter.UOvdev[i] = makeRdData(35, i,7);
			meter[id].meter.THD_U[i] = makeRdData(55, i,7);
			meter[id].meter.THD_Upp[i] = makeRdData(65, i,7);
			meter[id].meter.CF_U[i] = makeRdData(75, i,7);
			meter[id].meter.CF_Upp[i] = makeRdData(85, i,7);
			meter[id].meter.THD_I[i] = makeRdData(95, i,7);
			meter[id].meter.TDD_I[i] = makeRdData(105, i,7);
			meter[id].meter.KF_I[i] = makeRdData(115, i,7);
			meter[id].meter.CF_I[i] = makeRdData(125, i,7);
		}
		meter[id].meter.U[3] = (meter[id].meter.U[0] + meter[id].meter.U[1] + meter[id].meter.U[2])/3.;
		meter[id].meter.Upp[3] = (meter[id].meter.Upp[0] + meter[id].meter.Upp[1] + meter[id].meter.Upp[2])/3.;
		meter[id].meter.I[3] = (meter[id].meter.I[0] + meter[id].meter.I[1] + meter[id].meter.I[2])/3.;
		// meter[id].meter.P[3] = meter[id].meter.P[0] + meter[id].meter.P[1] + meter[id].meter.P[2];
		// meter[id].meter.Q[3] = meter[id].meter.Q[0] + meter[id].meter.Q[1] + meter[id].meter.Q[2];
		// meter[id].meter.S[3] = meter[id].meter.S[0] + meter[id].meter.S[1] + meter[id].meter.S[2];
		meter[id].meter.PF[3] = (meter[id].meter.PF[0] + meter[id].meter.PF[1] + meter[id].meter.PF[2])/3;
		meter[id].meter.dPF[3] = (meter[id].meter.dPF[0] + meter[id].meter.dPF[1] + meter[id].meter.dPF[2])/3;
	
		meter[id].meter.Itot = meter[0].meter.I[0] + meter[0].meter.I[1] + meter[0].meter.I[2];
		meter[id].meter.In = makeRdData(123, 0,7);
		meter[id].meter.Isum = makeRdData(321, 0,7);
		meter[id].meter.Ig = makeRdData(98, 0,7);
	
		meter[id].meter.Uangle[0] = makeRdData(12, 0,7);
		meter[id].meter.Uangle[1] = makeRdData(252, 0,7);
		meter[id].meter.Uangle[2] = makeRdData(122, 0,7);
	
		meter[id].meter.Iangle[0] = makeRdData(12+15, i,7);
		meter[id].meter.Iangle[1] = makeRdData(252+16, i,7);
		meter[id].meter.Iangle[2] = makeRdData(122+17, i,7);
	
		for(i=0; i<2; i++) {
			meter[id].meter.Ubal[i] = makeRdData(16, i,2);
			meter[id].meter.Uzs[i] = makeRdData(23, i,3);
			meter[id].meter.Ups[i] = makeRdData(29, i,4);
			meter[id].meter.Uns[i] = makeRdData(36, i,5);
			meter[id].meter.Izs[i] = makeRdData(43, i,6);
			meter[id].meter.Ips[i] = makeRdData(49, i,7);
			meter[id].meter.Ins[i] = makeRdData(56, i,8);
			meter[id].meter.Ibal[i] = makeRdData(63, i,9);
		}
	
	}

	for(i=0; i<3; i++) {
		for(j=0; j<64; j++) {
			if(j < 15) {
				if(j%2==0) {
					pHD->U[i][j] = makeRdData(2+i, i, 3);
					pHD->Upp[i][j] = makeRdData(1+i, i, 3);
					pHD->I[i][j] = makeRdData(5+i, i, 3);
				}
				else {
					pHD->U[i][j] = makeRdData(11+i, i, 5);
					pHD->Upp[i][j] = makeRdData(21+i, i, 5);
					pHD->I[i][j] = makeRdData(15+i, i, 5);
				}
			}
			else {
				if(j%2==0) {
					pHD->U[i][j] = makeRdData(1+i, i, 5);
					pHD->Upp[i][j] = makeRdData(2.5+i, i,5);
					pHD->I[i][j] = makeRdData(1.6+i, i, 5);
				}
				else {
					pHD->U[i][j] = makeRdData(5+i, i, 4);
					pHD->Upp[i][j] = makeRdData(7.5+i, i, 4);
					pHD->I[i][j] = makeRdData(10+i, i, 4);
				}
			}
		}
	}

	// total kwh
	meter[id].egy.Ereg32[0].eh[0][0] = 123456;
	meter[id].egy.Ereg32[0].eh[0][1] = 654;
	// this kwh
	meter[id].egy.Ereg32[1].eh[0][0] = 7890;
	meter[id].egy.Ereg32[1].eh[0][1] = 543;
	// last kwh
	meter[id].egy.Ereg32[2].eh[0][0] = 123;
	meter[id].egy.Ereg32[2].eh[0][1] = 456;

	// total kvarh
	meter[id].egy.Ereg32[0].eh[1][0] = 11234;
	meter[id].egy.Ereg32[0].eh[1][1] = 654321;
	// this kvarh
	meter[id].egy.Ereg32[1].eh[1][0] = 1356;
	meter[id].egy.Ereg32[1].eh[1][1] = 785;
	// last kvarh
	meter[id].egy.Ereg32[2].eh[1][0] = 548;
	meter[id].egy.Ereg32[2].eh[1][1] = 145;

	// total kvah
	meter[id].egy.Ereg32[0].eh[2][0] = meter[id].egy.Ereg32[0].eh[0][0]+ meter[id].egy.Ereg32[0].eh[0][1] + meter[id].egy.Ereg32[0].eh[1][0]+ meter[id].egy.Ereg32[0].eh[1][1];
	// this kvah
	meter[id].egy.Ereg32[1].eh[2][0] = meter[id].egy.Ereg32[1].eh[0][0]+ meter[id].egy.Ereg32[1].eh[0][1] + meter[id].egy.Ereg32[1].eh[1][0]+ meter[id].egy.Ereg32[1].eh[1][1];
	// last kvah
	meter[id].egy.Ereg32[2].eh[2][0] = meter[id].egy.Ereg32[2].eh[0][0]+ meter[id].egy.Ereg32[2].eh[0][1] + meter[id].egy.Ereg32[2].eh[1][0]+ meter[id].egy.Ereg32[2].eh[1][1];

	//	srand(time(NULL));
}


void Test_task(void *arg)
{
	int	 dTime;

	dTime = db[0].etc.testMode_Period * 1000;

	if(dTime == 0)
		dTime = 5000;
	while(1){
		makeRandomData();
		os_dly_wait(dTime);
	}
}

// case 1: Update 하는경우
// size:115232, 107ms, 234ms,  233ms, 496ms, 509ms, 473ms, 
// case 2: 새로 만드는 경우 
// size:115232, 279ms, 145ms, 78ms, 71ms, 169ms, 74ms
void FS_task(void *arg) 
{
	FILE *fp=NULL;
	FS_MSG *pmsg;
	uint8_t *ptr;
	int length, cnt=0, i, ix, id=0;
	uint64_t t1, t2;
	uint32_t t3;
	uint32_t notificationValue;
	EVENT_U elog;
	
#ifdef __FREERTOS	
	fsQ.tid = xTaskGetCurrentTaskHandle();
#else
	fsQ.tid = os_tsk_self();
#endif
	
	_enableTaskMonitor(Tid_Fs, 50);
	
	t3 = sysTick32;
	while (meter[id].cntl.runFlag) {
#ifdef __FREERTOS		
      xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, pdMS_TO_TICKS(100));
#else
      os_evt_wait_and(0x1, 100);
#endif		
		//printf("[Fs:%d]\n", sysTick32-t3);
		t3 = sysTick32;
		
		meter[0].cntl.wdtTbl[Tid_Fs].count++;
		
		if (fsQ.fr != fsQ.re) {
			t1 = sysTick64;
			pmsg = &fsQ.mQ[fsQ.re];			
			
			// delete
			if (strcmp(pmsg->mode, "rm") == 0) {
#ifdef USE_CMSIS_RTOS2				
            fdelete(pmsg->fname, NULL);				
#else
				fdelete(pmsg->fname);
#endif				
				t2 = sysTick64;			
				printf("FS, delete(%s, %s), elap=%lld\n", pmsg->fname, pmsg->mode, t2-t1);
			}
#if 1	// 2025-3-13, event Fifo			
			else if (strcmp(pmsg->mode, "ff") == 0) {				
				fp = fopen(pmsg->fname, "r+b");
				if (fp == NULL) {
					// create header
					memset(&elog, 0, sizeof(elog));
					elog.head.magic = 0x1234abcd;
					elog.head.fr = 1;
					elog.head.count = 1;
					elog.head.ts = sysTick1s;
					fp = fopen(pmsg->fname, "wb");
					fwrite(&elog, sizeof(elog), 1, fp);
					fwrite(pmsg->pbuf, pmsg->size, 1, fp);
					fclose(fp);
				}
				else {
					// update header
					fread(&elog, sizeof(elog), 1, fp);
					if (elog.head.count < pmsg->argv) {
						elog.head.count++;
					}
					if (++elog.head.fr > pmsg->argv) {
						elog.head.fr = 1;
					}	
				
					// update header
					fseek(fp, 0, SEEK_SET);
					fwrite(&elog, sizeof(elog), 1, fp);						
					// append or update data
					fseek(fp, sizeof(elog)*elog.head.fr, SEEK_SET);
					fwrite(pmsg->pbuf, pmsg->size, 1, fp);
					fclose(fp);
				}
			}
#endif			
			else {
				fp = fopen(pmsg->fname, pmsg->mode);
				if (fp != NULL) {
					fwrite(pmsg->pbuf, pmsg->size, 1, fp);
					fclose(fp);
				}
				t2 = sysTick64;			
				printf("FS, write(%s, %s, %d), elap=%lld\n", pmsg->fname, pmsg->mode, pmsg->size, t2-t1);
			}
			
			if (++fsQ.re >= FS_MSG_CNT) {
				fsQ.re = 0;
			}			
		}
		else if (meter[id].cntl.saveSetting == 0x1234) {
			// 저장 후 정지
			memcpy(&db[0], pdbk, sizeof(*pdb));
			memcpy(&db[1], pdbk2, sizeof(*pdb));
			saveSettings(db);

			meter[id].cntl.saveSetting = 0;
			meter[id].cntl.runFlag = 0;
			osDelayTask(2000);
			printf("... save settings, ip=%d.%d.%d.%d\n", 
				pdbk->comm.ip0[0], pdbk->comm.ip0[1], pdbk->comm.ip0[2], pdbk->comm.ip0[3]);
		}
		else if (meter[id].cntl.factReset == 0x1234) {
			// db 초기화 후 정지
			initSettings(0);		
			initSettings(1);
			build_set_db();		
			initExtSettings(0);

			saveSettings(db);
			meter[id].cntl.factReset = 0;
			meter[id].cntl.runFlag = 0;
		}
		else if (meter[id].cntl.rebootFlag == 0x1234) {			
			// 재시작
			meter[id].cntl.rebootFlag = meter[id].cntl.runFlag = 0;
		}
	}
	
	// wait forever
	printf("FS_task stopped ...\n");
#ifdef __FREERTOS	
	vTaskSuspend(NULL);
#else
	os_evt_wait_and(0xffff, 0xffff);
#endif	
}


FAST_RMS *_getFastRMSBuf(int ix) 
{
	if (ix < 0) 
		return &rmsWin.buf[N_FASTRMS_BUF-1];
	else if (ix == N_FASTRMS_BUF) 
		return &rmsWin.buf[0];
	else 
		return &rmsWin.buf[ix];
}


void RMSCapture(int id, int ix) {
	FAST_RMS *pCur, *pNext, *pWin;
	uint64_t Ts;
	int i, pos, n, sx, eType;
	FILE *fp;
	FS_MSG fsmsg;
	PQ_EVENT *pqE = &meter[id].cntl.pqe;
	char path[64];
	
	pCur  = _getFastRMSBuf(ix);
	pNext = _getFastRMSBuf(ix+1);
	
	if (pqE->rQ.fr == pqE->rQ.re) 
		return;
	
	Ts = pqE->rQ.Q[pqE->rQ.re].Ts;
	
	if (pCur->ts <= Ts && Ts < pNext->ts) {				
		i = 1000./db[id].pt.freq;	// 시간간격		
		rmsCap.pos  = pos = (Ts-pCur->ts)/i;	// sag 시작위치 검색
		rmsCap.ts   = Ts;
		rmsCap.mask = pqE->rQ.Q[pqE->rQ.re].mask;
		eType       = pqE->rQ.Q[pqE->rQ.re].eType;
		
		pqE->rQ.re = (pqE->rQ.re+1)%8;
		
		printf("===> PQ EVENT(%d): %d, %lld, %lld, %d\n", eType, ix, pCur->ts, Ts, pos);
		
		// 현 위치를 기준으로 1초 전 데이터 부터, 10초간 데이터 복사
		pWin = _getFastRMSBuf(ix-1);				
		
		if (eType == E_OC) {
			for (i=0; i<1200; i++) {
				rmsCap.rms[0][i] = scaleIrms(id, pWin->I[0][pos]);
				rmsCap.rms[1][i] = scaleIrms(id, pWin->I[1][pos]);
				rmsCap.rms[2][i] = scaleIrms(id, pWin->I[2][pos]);
				
				if (++pos >= meter[id].cntl.nFastRMS) {
					pos = 0;
					if (++ix >= N_FASTRMS_BUF) ix = 0;
					pWin = _getFastRMSBuf(ix-1);
				}
			}			
		}
		else {
			for (i=0; i<1200; i++) {
				rmsCap.rms[0][i] = scaleVrms(id, pWin->U[0][pos]);
				rmsCap.rms[1][i] = scaleVrms(id, pWin->U[1][pos]);
				rmsCap.rms[2][i] = scaleVrms(id, pWin->U[2][pos]);
				
				if (++pos >= meter[id].cntl.nFastRMS) {
					pos = 0;
					if (++ix >= N_FASTRMS_BUF) ix = 0;
					pWin = _getFastRMSBuf(ix-1);
				}
			}
		}
#ifdef _DIRECT_FS
//		fp = fopen("RMS_Cap.csv", "w");
//		if (fp != NULL) {
//			for (i=0; i<1200; i++) {
//				fprintf(fp, "%f, %f, %f\n", rmsCap.U[0][i], rmsCap.U[1][i], rmsCap.U[2][i]);
//			}
//			fclose(fp);
//		}		
#else	
		// FS_Task에 쓰기 요청한다 
		if (eType == E_SAG) {
			strcpy(path, "\\Trg_PQ\\");
			strcat(path, "DSAG_");		
		}
		else if (eType == E_SWELL) {
			strcpy(path, "\\Trg_PQ\\");
			strcat(path, "DSWL_");		
		}
		else if (eType == E_OC) {
			strcpy(path, "\\Trg_PQ\\");
			strcat(path, "DOC_");		
		}
		else
			return;
		
		getTrgFileName(path, Ts, fsmsg.fname);
		strcpy(fsmsg.mode, "wb");
		fsmsg.pbuf = &rmsCap;
		fsmsg.size = sizeof(rmsCap);
		putFsQ(&fsmsg);				
#endif			
	}
}


void updatePresentDemand(int id) {
	int i, dInterval;
	float mult;
	
	//dInterval = sysTick1s - meter[id].cntl.dmdStartTs;
	dInterval = meter[id].cntl.mInterval;
	mult = 3600./dInterval;
	
	for (i=0; i<2; i++) {
		pdm->CD_P[i] = meter[id].cntl.dmdP[i]*mult;
		pdm->CD_Q[i] = meter[id].cntl.dmdQ[i]*mult;		
	}
	pdm->CD_S = meter[id].cntl.dmdS[0]*mult;
}

// 예측전력(Q) = 현재누적전력량 + 단위시간당 전력변화량 × 남은수요시간(분)
void updatePredictDemand(int id,float delta) {
	int i;
	int mInterval = meter[id].cntl.mInterval; //sysTick1s - meter[id].cntl.dmdStartTs;
	float mult = 60/(mInterval/60);
		
	if (delta < 0) delta = 0;
	pdm->PD_P = (meter[id].cntl.dmdP[0] + delta*(meter[id].cntl.dInterval-mInterval+1))*mult;
}

int updateMaxDemand(int id) {
	int i, mInterval, mult, wF=1;
	float dt;
	
	meter[id].cntl.dmdEndTs = sysTick1s;
	//dInterval = meter[id].cntl.dmdEndTs - meter[id].cntl.dmdStartTs;	// 중간에 시간이 변경되면 Interval에 오류 발생
	mInterval = meter[id].cntl.mInterval;
		
	// dynamic demand I
	pdm->ddTime = sysTick1s;
	for (i=0; i<3; i++) {
		pdm->DD_I[i] = meter[id].cntl.dmdI[i]/mInterval;
		meter[id].cntl.dmdI[i] = 0;
	}
	
	// demand power = 누적값 * 60/주기(분) * 수집시간/실제수집시간
	// 
	mult = 60/(meter[id].cntl.dInterval/60);	// 60/주기(분)
	dt = (float)meter[id].cntl.dInterval/mInterval;	// 주기(초)/실제수집시간(초)
	// dynamic demand Power
	for (i=0; i<2; i++) {
		pdm->DD_P[i] = meter[id].cntl.dmdP[i]*dt*mult;		
		pdm->DD_Q[i] = meter[id].cntl.dmdQ[i]*dt*mult;		
		meter[id].cntl.dmdP[i] = meter[id].cntl.dmdQ[i] = 0;
	}
	pdm->DD_S = meter[id].cntl.dmdS[0]*dt*mult;	
	meter[id].cntl.dmdS[0] = 0;
	
	printf("DD: %f, %f, %f, Interval=%d\n", pdm->DD_P[0], pdm->DD_Q[0], pdm->DD_S, mInterval);
	
	// max demand 갱신
	for (i=0; i<3; i++) {
		if (pdm->MD_I[i].value < pdm->DD_I[i]) {			
			pdm->MD_I[i].value = pdm->DD_I[i];
			pdm->MD_I[i].mdTime = sysTick1s;
			wF++;
		}
	}
	//
	for (i=0; i<2; i++) {
		if (pdm->MD_P[i].value < pdm->DD_P[i]) {
			pdm->MD_P[i].value = pdm->DD_P[i];
			pdm->MD_P[i].mdTime = sysTick1s;
			wF++;
		}
		
		if (pdm->MD_P[i].value < pdm->DD_P[i]) {
			pdm->MD_Q[i].value = pdm->DD_Q[i];
			pdm->MD_Q[i].mdTime = sysTick1s;
			wF++;
		}
	}
	if (pdm->MD_S.value < pdm->DD_S) {
		pdm->MD_S.value = pdm->DD_S;
		pdm->MD_S.mdTime = sysTick1s;
		wF++;
	}	
	
	meter[id].cntl.dmdStartTs = meter[id].cntl.dmdEndTs;
	
	return wF;
}


void updateDemandI(int id)
{
	int i;
	for (i=0; i<3; i++) {
		meter[id].cntl.dmdI[i] += meter[id].meter.I[i];
	}
}	


void copyDemandBin() {
	int i, id;
	
	for(id=0; id<2; id++) {
		meter[id].dlog.ts = pdm->dmdLogTs;	
		for (i=0; i<96; i++) {
			meter[id].dlog.DP_P_Log[i] = pdm->DP_P_Log[i];
		}
	}
}
// 00:00 ~ 00:15 분 까지 데이터는 0에 저장
void putDemandBin(int id, float dd, uint32_t utc) {
	int sod = utc % 86400;
	int bin = sod/900;
	int i;
	
	// 날이 변경되면 금일데이터를 전일데이터에 복사
	if (bin == 0) {
		for (i=0; i<96; i++) {
			meter[id].dlog.DP_P_Log[i] = pdm->DP_P_Log[i];
			pdm->DP_P_Log[i] = 0;
		}
		meter[id].dlog.ts = pdm->dmdLogTs;
	}
	
	// 날이 변경되거나, 설정이 안되었으면 날짜 설정
	if (bin == 0 || pdm->dmdLogTs == 0) {
		pdm->dmdLogTs = utc;
	}
	pdm->DP_P_Log[bin] = dd;
}

// 현재 전력량을 임시 저장
void initEnergyLog(int id) {
	meter[id].cntl.egyBuf[0] = meter[id].egy.Ereg32[0].eh[0][0];	// imp kwh
	meter[id].cntl.egyBuf[1] = meter[id].egy.Ereg32[0].eh[1][0];	// kvarh lag
	meter[id].cntl.egyBuf[2] = meter[id].egy.Ereg32[0].eh[1][0];	// kvarh lead
	meter[id].cntl.egyBuf[3] = meter[id].egy.Ereg32[0].eh[2][0];	// kVAh
}


// mode : 1 (MD)
// mode : 2 (DD)
void resetDemand(int id, int mode) {
	int i;
	uint32_t ts = sysTick1s;
	
	// clear MD
	for (i=0; i<3; i++) {
		pdm->MD_I[i].mdTime = sysTick1s;
		pdm->MD_I[i].value = 0;
	}
	
	for (i=0; i<2; i++) {
		pdm->MD_P[i].mdTime = ts;
		pdm->MD_P[i].value = 0;
		pdm->MD_Q[i].mdTime = ts;
		pdm->MD_Q[i].value = 0;
		pdm->MD_S.mdTime = ts;
		pdm->MD_S.value = 0;
	}
	
	for (i=0; i<3; i++) {
		pdm->DD_I[i] = 0;
	}
	for (i=0; i<2; i++) {
		pdm->DD_P[i] = 0;
		pdm->DD_Q[i] = 0;		
	}
	pdm->DD_S = 0;
	
	meter[id].cntl.dmdTs = sysTickDemand;
	meter[id].cntl.dmdStartTs = ts;
	
	// 
	pdm->dmdLogTs = 0;
	for (i=0; i<96; i++) {
		pdm->DP_P_Log[i] = 0;
	}
		
	meter[id].dlog.ts = 0;
	for (i=0; i<96; i++) {
		meter[id].dlog.DP_P_Log[i] = 0;
	}
}


void storeDemand() {	
	FILE *fp;
	char path[64];
	
	sprintf(path, "%s", DEMAND_FILE);
	fp = fopen(path, "wb");	
	if (fp == NULL) 
		return;
	
	fwrite(pdm, sizeof(DEMAND), 1, fp);
	fwrite(pdmlog, sizeof(DEMAND_LOG), 1, fp);
	fclose(fp);
}

int storeDemandNVR() {
	int os=2048, ret=0, id;
	
	for(id=0; id<2; id++) {	
		pdm->magic = 0x1234;
		pdm->crc = gencrc_modbus((uint8_t *)&meter[id].dm, sizeof(DEMAND)-2);	
		
		if (FramWrite(&meter[id].dm, os, sizeof(DEMAND)) == 0) {
			printf("FramWrite failed(%d, %d, %d)\n", id, os, sizeof(DEMAND));
			ret = -1;
		}
	}	
	
	return ret;
}

void loadDemand() {
	FILE *fp;
	char path[64];
	
	sprintf(path, "%s", DEMAND_FILE);
	fp = fopen(path, "rb");
	if (fp == NULL) 
		return;
	
	fread(pdm, sizeof(DEMAND), 1, fp);
	fread(pdmlog, sizeof(DEMAND_LOG), 1, fp);
	fclose(fp);	
}


int loadDemandNVR() {
	uint16_t os = 2048, crc;
	int i, id;
	
	for(id=0; id<2; id++) {
		for (i=0; i<2; i++) {
			FramRead(&meter[id].dm, os, sizeof(DEMAND));
			crc = gencrc_modbus((uint8_t *)&meter[id].dm, sizeof(DEMAND));
			if (id == 1 && crc == 0) {	
				return 0;
			}
			
			os += 640;
		}
	}	
	
	return -1;
}


// void storeIntDINVR(INTDI_STS_NVRAM *pDINvr){
// 	uint16_t os=2816, i;

// 	for(i=0; i<INT_DI_MAX; i++) {
// 		pDINvr->b_on_dim[i] = meter[id].cntl.diData[0][i];
// 		pDINvr->b_off_dim[i] = meter[id].cntl.diData[1][i];
// 	}

// 	pDINvr->magic = 0x1234;
// 	pDINvr->crc = gencrc_modbus((uint8_t *)&pDINvr, sizeof(INTDI_STS_NVRAM)-2);	

// 	if (FramWrite(pDINvr, os, sizeof(INTDI_STS_NVRAM)) == 0) 	
// 		printf("FramWrite failed(%d, %d)\n", os, sizeof(INTDI_STS_NVRAM));
// 	// else
// 	// 	printf("storeIntDINVR : FramWrite (%x, %d)\n", pDINvr->crc, sizeof(INTDI_STS_NVRAM));

// }

void storeEnergyNVR(ENERGY_NVRAM *pEgyNvr) {
	int os=0;
	
	//
	if (FramWrite(pEgyNvr, os, sizeof(ENERGY_NVRAM)) == 0) 	
		printf("FramWrite failed(%d, %d)\n", os, sizeof(ENERGY_NVRAM));
	
	os += 512;
	
	if (FramWrite(pEgyNvr, os, sizeof(ENERGY_NVRAM)) == 0) 	
		printf("FramWrite failed(%d, %d)\n", os, sizeof(ENERGY_NVRAM));
}


int storeEnergyLogNVR(int sel, ENERGY_LOG *pEgyLog) {
	int os = (sel == 0) ? 1024 : (1024+512), ret=0;
	
	pEgyLog[sel].magic = 0x1234;
	pEgyLog[sel].crc = gencrc_modbus((uint8_t *)&pEgyLog[sel], sizeof(ENERGY_NVRAM)-2);	
	
	if (FramWrite(&pEgyLog[sel], os, sizeof(ENERGY_LOG)) == 0) {
		printf("FramWrite failed(%d, %d)\n", os, sizeof(ENERGY_LOG));
		ret = -1;
	}
	
	return ret;
}


// nvram energy를 meter영역으로 복사한다 
void copyEreg32(int id) {
	int ix, j;

	// ix: 누적,현월,전월
	// j : kwh, kvarh, kvah
	for (ix=0; ix<3; ix++) {
		for (j=0; j<3; j++) {
			meter[id].egy.Ereg32[ix].eh[j][0] = egyNvr.Ereg64[id][ix].eh[j][0]/100;		// Imp
			meter[id].egy.Ereg32[ix].eh[j][1] = egyNvr.Ereg64[id][ix].eh[j][1]/100;		// Exp
		}
	}
}

void copyEreg64(ENERGY_REG64 *pdst, ENERGY_REG64 *psrc) {
	int ix=0, j;
	
	// j: kwh, kvarh, kvah
	memcpy(pdst, psrc, sizeof(ENERGY_REG64));
}


// nvram energy만 저장한다 
int updateEh(int id, int mode, float diff) {
	int i, dir, ret=0, dt;
	uint32_t ieh;
	
	if (diff > 0) {
		dir = 0;
	}
	else if (diff < 0) {
		dir = 1;
		diff = -diff;
	}
	else {
		return ret;
	}
	
	// 1w 이상인 값을 추출한다
	meter[id].cntl.Ereg.eh[mode][dir] += diff;
	dt = meter[id].cntl.Ereg.eh[mode][dir];
	if (dt) {
		meter[id].cntl.Ereg.eh[mode][dir] -= dt;
	
		// 누적, 현월 반영		
		for (i=0; i<2; i++) {											
			egyNvr.Ereg64[id][i].eh[mode][dir] += dt;						
			if (egyNvr.Ereg64[id][i].eh[mode][dir] >= MAX_EREG64) {
				egyNvr.Ereg64[id][i].eh[mode][dir] -= MAX_EREG64;
			}					
			
			// 64bit 누적 -> meter 64bit 누적으로 복사 
			if (i==0) {
				meter[id].egy.Ereg64.eh[mode][dir] = egyNvr.Ereg64[id][i].eh[mode][dir];
			}
			
			// 100wh 이상 변할때 마다 저장한다 
			ieh = egyNvr.Ereg64[id][i].eh[mode][dir]/EREG32_UNIT;		
			if (meter[id].egy.Ereg32[i].eh[mode][dir] != ieh) {			
				//printf("eh, i=%d, m=%d, dir=%d, e32=%d, e64=%d\n", i, mode, dir, meter[id].egy.Ereg32[i].eh[mode][dir], ieh);
				ret |= (1<<mode);
			}
		}
	} 
	
	return ret;	
}


// int loadEnergyLogNVR(int sel, ENERGY_LOG *pEgyLog) {
// 	uint16_t os = 0, crc;
// 	int i, j, ret=0;
// 	//uint64_t eh;
	
// 	FramRead(&pEgyLog[sel], os, sizeof(ENERGY_LOG));
// 	crc = gencrc_modbus((uint8_t *)&pEgyLog[sel], sizeof(ENERGY_LOG));
// 	if (crc) {	
// 		printf(">>Error on EnergyLog(%d) <<<\n", sel);
// 		ret = -1;
// 	}
	
// 	return ret;
// }

int loadEnergyLogFs() {
	FILE *fp;
	uint16_t crc;
		
	fp = fopen(ENERGY_LOG_FILE0, "rb");
	if (fp == NULL) {
		printf("{{Can't load Energy Log File(%s)}}\n", ENERGY_LOG_FILE0);
		return -1;
	}	
	fread(&pegylog[0], sizeof(ENERGY_LOG), 1, fp);
	fclose(fp);	
	
	fp = fopen(ENERGY_LOG_FILE1, "rb");
	if (fp == NULL) {
		printf("{{Can't load Energy Log File(%s)}}\n", ENERGY_LOG_FILE1);
		return -1;
	}	
	fread(&pegylog[1], sizeof(ENERGY_LOG), 1, fp);
	fclose(fp);		
	
	return 0;
}

int loadEnergyNVR(ENERGY_NVRAM *pEgyNvr) {
	uint16_t os = 0, crc;
	int id, i, j, ret=0;
	//uint64_t eh;
	
	FramRead(pEgyNvr, os, sizeof(ENERGY_NVRAM));
	crc = gencrc_modbus((uint8_t *)pEgyNvr, sizeof(ENERGY_NVRAM));
	if (crc) {	
		printf(">>Error on 1st Energy Register <<<\n");
		os += 512;	
		FramRead(pEgyNvr, os, sizeof(ENERGY_NVRAM));
		crc = gencrc_modbus((uint8_t *)pEgyNvr, sizeof(ENERGY_NVRAM));
		if (crc) {
			printf(">>Error on 2nd Energy Register <<<\n");
			return -1;
		}
	}
	
	//ENERGY_REG64 Ereg[3]
	if (crc == 0) {
		for (i=0; i<3; i++) {
			for (j=0; j<3; j++) {
				if (egyNvr.Ereg64[id][i].eh[j][0] > MAX_EREG64) {
					printf(">>Error on Ereg[%d].eh[%d][0] = %lld<<\n", i, j, egyNvr.Ereg64[id][i].eh[j][0]);
					ret = -1;
				}
				else
					printf(">>ENERGY_NVRAM Ereg[%d].eh[%d][0] = %lld<<\n", i, j, egyNvr.Ereg64[id][i].eh[j][0]);

				if (egyNvr.Ereg64[id][i].eh[j][1] > MAX_EREG64) {
					printf(">>Error on Ereg[%d].eh[%d][1] = %lld<<\n", i, j, egyNvr.Ereg64[id][i].eh[j][1]);
					ret = -1;
				}
				else
					printf(">>ENERGY_NVRAM Ereg[%d].eh[%d][1] = %lld<<\n", i, j, egyNvr.Ereg64[id][i].eh[j][1]);

			}
		}
	}
	
	return ret;
}


int loadEnergyFs(ENERGY_NVRAM *pEgyNvr) {
	FILE *fp = fopen(ENERGY_FILE, "rb");
	uint16_t crc;
		
	if (fp == NULL) {
		printf("{{Can't load Energy File(%s)}}\n", ENERGY_FILE);
		return -1;
	}
	
	fread(pEgyNvr, sizeof(ENERGY_NVRAM), 1, fp);
	fclose(fp);	
	
	return 0;
}


void storeEnergyFs(ENERGY_NVRAM *pEgyNvr) {
	FILE *fp = fopen(ENERGY_FILE, "wb");				
	
	if (fp != NULL) {		
		fwrite(pEgyNvr, sizeof(ENERGY_NVRAM), 1, fp);
		fclose(fp);
	}
}


void storeEnergyLogFs(int sel, ENERGY_LOG *pEgyLog) {
	FILE *fp;
	
	if (sel == 0) {
		fp = fopen(ENERGY_LOG_FILE0, "wb");	
		if (fp != NULL) {		
			fwrite(&pEgyLog[0], sizeof(ENERGY_LOG), 1, fp);
			fclose(fp);
		}			
	}
	else {
		fp = fopen(ENERGY_LOG_FILE1, "wb");	
		if (fp != NULL) {		
			fwrite(&pEgyLog[1], sizeof(ENERGY_LOG), 1, fp);
			fclose(fp);
		}			
	}
}

// energy

void storeEnergy() {
	int	i;
	egyNvr.ts = sysTick1s;
	egyNvr.crc = gencrc_modbus((uint8_t *)&egyNvr, sizeof(ENERGY_NVRAM)-2);	
	storeEnergyNVR(&egyNvr);
	
	if (egyNvr.magic != 0x1234abcd) {
		printf("&&& storeEnergyNVR. bad magic number(%x) &&&\n", egyNvr.magic);
	}		
	else {
		printf("+++ store Energy, crc=%x(%x)\n", egyNvr.crc, gencrc_modbus((uint8_t *)&egyNvr, sizeof(ENERGY_NVRAM)));		
	}
}


void checkEnergy(int id) {	
	int i, j;
	struct tm lto;//, ltn;

	// 마지막 기록 시간에서 달이 경과 하면 현월을 전월로 이동한다 
	uLocalTime(&egyNvr.ts, &lto);
	//localtime_r(&sysTick1s, &ltn);
	
	if (lto.tm_mon != meter[0].cntl.tod.tm_mon) {
		memcpy(&egyNvr.Ereg64[0][2], &egyNvr.Ereg64[0][1], sizeof(ENERGY_REG64));	// 현월->전월
		memset(&egyNvr.Ereg64[0][1], 0, sizeof(ENERGY_REG64));	// 현월 reset
		memcpy(&egyNvr.Ereg64[1][2], &egyNvr.Ereg64[1][1], sizeof(ENERGY_REG64));	// 현월->전월
		memset(&egyNvr.Ereg64[1][1], 0, sizeof(ENERGY_REG64));	// 현월 reset
		
	}	
	
	id = 0;
	copyEreg32(id);
	copyEreg64(&meter[id].egy.Ereg64, egyNvr.Ereg64[id]);
	printf("[[[[%d]Energy Reg : %lld, %lld, %lld]]]\n", 
		id, egyNvr.Ereg64[id][0].eh[0][0], egyNvr.Ereg64[id][0].eh[1][0], egyNvr.Ereg64[id][0].eh[2][0]);
	id = 1;
	copyEreg32(id);
	copyEreg64(&meter[id].egy.Ereg64, egyNvr.Ereg64[id]);
	printf("[[[[%d]Energy Reg : %lld, %lld, %lld]]]\n", 
		id, egyNvr.Ereg64[id][0].eh[0][0], egyNvr.Ereg64[id][0].eh[1][0], egyNvr.Ereg64[id][0].eh[2][0]);
	storeEnergy();
}

int loadEnergy(int id) {
	int pass=0;
	
	if (loadEnergyNVR(&egyNvr) < 0)  
	{		
		printf(">>Can't load Energy from NVR ...<<\n");
#ifdef	SUPPORT_FS_ENERGY		
		if (loadEnergyFs(&egyNvr) < 0) 
			printf(">>Can't load Energy from FS ...<<\n");
#endif			
		{
			memset(&egyNvr, 0, sizeof(egyNvr));
			egyNvr.magic = 0x1234abcd;
			storeEnergy();
			pass = -1;
		}
	}
	
	checkEnergy(id);
	return pass;
}


int loadMaxMin() {
	MAXMIN *pmm;
	FILE *fp = fopen(MAXMIN_FILE, "rb");
	uint16_t crc;
		
	if (fp == NULL) 
		return -1;

	pmm = &meter[0].maxmin;
	fread(pmm, sizeof(MAXMIN), 1, fp);	// 현재 max/min
	
	pmm = &meter[1].maxmin;
	fread(pmm, sizeof(MAXMIN), 1, fp);	// 현재 max/min
	
	//	fread(&meter.lastmaxmin, sizeof(MAXMIN), 1, fp);	// 과거 max,min
	fclose(fp);	
	return 0;
}

int storeMaxMin() {
	MAXMIN *pmm;
	FILE *fp = fopen(MAXMIN_FILE, "wb");
	uint16_t crc;
		
	if (fp == NULL) 
		return -1;

	pmm = &meter[0].maxmin;
	fwrite(pmm, sizeof(MAXMIN), 1, fp);	// 현재 max/min
			
	pmm = &meter[1].maxmin;
	fwrite(pmm, sizeof(MAXMIN), 1, fp);	// 현재 max/min
	fclose(fp);	
	return 0;	
}

int putEnergyLog(int id, int hix) {			
	int wF=0;
	ENERGY_LOG *peLog = &pegylog[0];
	
	if (peLog->ts == 0) {
		peLog->ts = meter[id].cntl.egyStartTs1D;
	}

	// delta = 현재값 - 한시간 전 값
	peLog->egy[hix].kwh 	   = meter[id].egy.Ereg32[0].eh[0][0] - meter[id].cntl.egyBuf[0];
	peLog->egy[hix].kvarh[0] = meter[id].egy.Ereg32[0].eh[1][0] - meter[id].cntl.egyBuf[1];
	peLog->egy[hix].kvarh[1] = meter[id].egy.Ereg32[0].eh[1][1] - meter[id].cntl.egyBuf[2];
	peLog->egy[hix].kVAh     = meter[id].egy.Ereg32[0].eh[2][0] - meter[id].cntl.egyBuf[3];
	
	meter[id].cntl.egyBuf[0] = meter[id].egy.Ereg32[0].eh[0][0];
	meter[id].cntl.egyBuf[1] = meter[id].egy.Ereg32[0].eh[1][0];
	meter[id].cntl.egyBuf[2] = meter[id].egy.Ereg32[0].eh[1][1];
	meter[id].cntl.egyBuf[3] = meter[id].egy.Ereg32[0].eh[2][0];	
	
	// 1일이 경과하면 금일 데이터를 전일 데이토로 복사한다 
	if (meter[id].cntl.egyTs1D != meter[id].cntl.tod.tm_mday) {
		meter[id].cntl.egyTs1D = meter[id].cntl.tod.tm_mday;
				
		memcpy(&pegylog[1], peLog, sizeof(ENERGY_LOG));
		memset(peLog, 0, sizeof(ENERGY_LOG));
		peLog->ts = meter[id].cntl.egyStartTs1D = sysTick1s; 
		
		wF = 1;
	}		
		
	storeEnergyLogFs(0, pegylog);
	if (wF == 2) {
		storeEnergyLogFs(1, pegylog);
	}
	
	return wF;	// set write flag
}


// 1초 마다 호출
void energy_scan(int id, METER_EH_REGS *ereg, ENERGY_NVRAM *pEgyNvr) {
	//static int initF=0;
	float	dval,freq;
	int32_t	wh, varh, vah, es[3];
	int		i, ewF=0, wF=0;
	uint16_t phnoload;
	struct tm lto;//, ltn;
	
	//최초 읽는 energy register는 쓰레기 값이 들어있으므로 버린다.
	if (meter[id].cntl.energyInit==0){
		meter[id].cntl.energyInit=1;
		return ;
	}

//	//online이 아니면 읽은 전력량을 초기화한다
//	if(meter[id].cntl.online==0){
//		return;
//		//memset(pchip->energy, 0, sizeof(pchip->energy));
//	}

	
	//유효전력, 무효전력
	// dval : 3상 적산전력 합 계산
	
	es[0] = es[1] = es[2] = 0;
	for (i=0; i<3; i++) {			
		wh   = (meter[id].cntl.I[i] == 0) ? 0 : ereg[i].wh;
		varh = (meter[id].cntl.I[i] == 0) ? 0 : ereg[i].varh;
		vah  = (meter[id].cntl.I[i] == 0) ? 0 : ereg[i].vah;
		
		meter[id].cntl.wh[i]   = wh;
		meter[id].cntl.varh[i] = varh;
		meter[id].cntl.vah[i]  = vah;
		
		//wiring모드에 따라 불 필요한 항목 지운다		
		switch(db[id].pt.wiring){
		//1CT:2,3을지운다
		case WM_1LN1CT:
			if (i != 0) wh = varh = vah = 0;
			break;
		
		//2CT:2제거
		case WM_3LL3CT:	// 2017-9-26 (추가)
		case WM_3LL2CT:
		case WM_1LL2CT:
			if (i == 1) wh = varh = vah = 0;
			break;
		}
		
		//total energy
		if(db[id].ct.ct_dir[i]){
			wh = -wh;
			varh = -varh;
		}
		
		es[0] += wh;
		es[1] += varh;
		es[2] += vah;
	}
	
	// 2017-10-11, 각 상별로 개별적으로 누적하면 3P3W, PF=0.5 이하에서 누적 문제 발생한다
	//noload,16-10-12
	meter[id].cntl.dtP = scaleEnergy(id, es[0]);
	meter[id].cntl.dtQ = scaleEnergy(id, es[1]);		
	if (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT)  
		meter[id].cntl.dtS = scaleEnergy(id,es[2])*SQRT_3/2;
	else
		meter[id].cntl.dtS = scaleEnergy(id,es[2]);

	// demand interval 계산
	meter[id].cntl.mInterval++;
	meter[id].cntl.mInterval15m++;
	// demand 적산
	updateDemandI(id);
	
	// 예측 수요전력, 기존누적값, 현재 변화율을 가지고 계산한다 
	updatePredictDemand(id, meter[id].cntl.dtP);	
	
	incEnergy(meter[id].cntl.dtP, meter[id].cntl.dmdP);
	incEnergy(meter[id].cntl.dtQ, meter[id].cntl.dmdQ);
	incEnergy(meter[id].cntl.dtS, meter[id].cntl.dmdS);
	
	// 15분 전용 demand 수집
	incEnergy(meter[id].cntl.dtP, meter[id].cntl.dmdP15m);
		
	// 현재까지 누적된 값을 가지고 수요전력 계산
	updatePresentDemand(id);	
		
	// 지정된 주기 수집
	if (meter[id].cntl.dmdTs != sysTickDemand) {
		wF = updateMaxDemand(id);	
		meter[id].cntl.dmdTs = sysTickDemand;		
		meter[id].cntl.dmdStartTs = sysTick1s;
		meter[id].cntl.mInterval = 0;
	}
	
	// 15분 단위 수집
	if (meter[id].cntl.dmdTs15m != sysTick15m) {
		meter[id].cntl.dmdEndTs15m = sysTick1s;			
		// 시간이 변경될 경우 문제의 소지 있다
		//pdm->DD_P_15 = meter[id].cntl.dmdP15m[0]*4*960/(meter[id].cntl.dmdEndTs15m - meter[id].cntl.dmdStartTs15m);	
		pdm->DD_P_15 = meter[id].cntl.dmdP15m[0]*4*960/meter[id].cntl.mInterval15m;
		meter[id].cntl.dmdP15m[0] = 0;		
		
		putDemandBin(id, pdm->DD_P_15, meter[id].cntl.dmdStartTs15m);		
		
		meter[id].cntl.dmdTs15m = sysTick15m;		
		meter[id].cntl.dmdStartTs15m = sysTick1s;
		meter[id].cntl.mInterval15m = 0;
		wF++;
	}
	
	// 1시간에 마다 전력량 데이터 기록
	if (meter[id].cntl.egyTs1H != meter[id].cntl.tod.tm_hour) {		
		//storeEnergyFs(pEgyNvr);		
		// 1시간 에너지 로그 갱신한다 
		putEnergyLog(id, meter[id].cntl.egyTs1H);
		meter[id].cntl.egyTs1H = meter[id].cntl.tod.tm_hour;
	}
	
	
	if (meter[id].cntl.rstDemand == 0x1234) {
		meter[id].cntl.rstDemand = 0;
		resetDemand(id, 0);
		storeDemand();
	}
	else if (wF) {
		storeDemand();
	}
		
	// Total, 현월 에너지 누적
	ewF += updateEh(id, 0, meter[id].cntl.dtP);
	ewF += updateEh(id, 1, meter[id].cntl.dtQ);
	ewF += updateEh(id, 2, meter[id].cntl.dtS);
	
	// 월 변경 검사
	uLocalTime(&pEgyNvr->ts, &lto);
	if (lto.tm_mon != meter[0].cntl.tod.tm_mon) {
		printf("+++ copy this month energy to last month ...\n");
		memcpy(&egyNvr.Ereg64[0][2], &egyNvr.Ereg64[0][1], sizeof(ENERGY_REG64));	// 현월 -> 전월
		memset(&egyNvr.Ereg64[0][1], 0, sizeof(ENERGY_REG64));			// 현월 reset
		memcpy(&egyNvr.Ereg64[1][2], &egyNvr.Ereg64[1][1], sizeof(ENERGY_REG64));	// 현월 -> 전월
		memset(&egyNvr.Ereg64[1][1], 0, sizeof(ENERGY_REG64));			// 현월 reset
		ewF += (1<<3);
	}	
	// 전력량 지운다 	
	if (meter[id].cntl.rstEgy == 0x1234) {
		printf("+++ clear energy ...\n");
		meter[id].cntl.rstEgy = 0;
		memset(&egyNvr.Ereg64[id][0], 0, sizeof(ENERGY_REG64));	
		memset(&egyNvr.Ereg64[id][1], 0, sizeof(ENERGY_REG64));	
		memset(&egyNvr.Ereg64[id][2], 0, sizeof(ENERGY_REG64));	
//		memset(&egyNvr.Ereg64[1][0], 0, sizeof(ENERGY_REG64));	
//		memset(&egyNvr.Ereg64[1][1], 0, sizeof(ENERGY_REG64));	
//		memset(&egyNvr.Ereg64[1][2], 0, sizeof(ENERGY_REG64));	
		// 64bit 영역 반영
		copyEreg64(&meter[id].egy.Ereg64, egyNvr.Ereg64[id]);
//		copyEreg64(&meter[1].egy.Ereg64, egyNvr.Ereg64[1]);
		ewF += (1<<3);
	}
	
	
	if(ewF != 0) {	// 달이 변경되거나 0.1kw 이상 변경되면 저장한다 		
		//printf("+++ storeEnergy, %x...\n", ewF);
		if(saveLockEnergy ==0) {
			copyEreg32(0);
			copyEreg32(1);
			storeEnergy();	
		}
	}

// energy write test	
//	copyEreg32(pEgyNvr);		
//	storeEnergy(pEgyNvr);
}

void updateEventCount(int mask, uint32_t *ec) {
	int i, mp=0;
	
	for (i=0; i<3; i++) {
		if (mask & (1<<i)) {
			ec[i]++;
			mp++;
		}
	}	
	
	if (mp > 1) {
		ec[3]++;
	}
}

// MeterScan Task에서 Volt. event를 전송하기 위해 사용한다 
// 이벤트 보유 기간: 월 -> 년으로 변경
static ITIC_LOG  _iticlog;
//
int pushEvent(int id, PQ_EVENT_INFO *pInf) {
	ITIC_LOG *pilog=&_iticlog;
	FS_MSG fsmsg;
	int i, os, year, woY;
	
	pilog->type = pInf->type;
	pilog->startTs = pInf->startTs/1000;
	pilog->msec = pInf->startTs%1000;
	
	// long interrupt는 최대 10000s로 한다 
#if 1
	i = (pInf->endTs - pInf->startTs);
	if (i < 10000) {
		pilog->duration = i;
	}
	else {
		// sec로 변환
		pilog->duration = i/1000 | 0x8000;
	}
#else	
	if (pInf->type == E_lINTR) {
		i = (pInf->endTs - pInf->startTs)/1000;
		pelog->duration = (i > 10000) ? 10000 : i;
	}
	else {
		pelog->duration =  pInf->endTs - pInf->startTs;	
	}
#endif	
	pilog->mask = pInf->mask;
	
// 2020-4-3, TrV, TrC 추가	
//	전압 전류 스케일 따로 적용
//	for (i=0; i<3; i++) {
//		pelog->level[i] = (pInf->type == E_OC) ? scaleIrms(pInf->Val[i]) : scaleVrms(pInf->Val[i]);
//	}

	//  이벤트 타일별 전압 전류 스케일 따로 적용
	switch (pInf->type) {
		case E_SAG:
		case E_lINTR:
		case E_sINTR:
		case E_SWELL:
			for (i=0; i<3; i++) {
				pilog->level[i] = scaleVrms(id, pInf->Val[i]);
			}	
			pilog->norm = db[id].pt.PT1;
			break;
			
		case E_OC:
			for (i=0; i<3; i++) {
				pilog->level[i] = scaleIrms(id, pInf->Val[i]);	// 값
			}	
			pilog->norm = db[id].ct.CT1;
			break;
		
		case E_TrV:
			for (i=0; i<3; i++) {
				pilog->level[i] = scaleVWave(pInf->Val[i]);	// 값
			}	
			pilog->norm = db[id].pt.PT1;
			break;
		case E_TrC:
			for (i=0; i<3; i++) {
				pilog->level[i] = scaleCWave(pInf->Val[i]);	// 값
			}	
			pilog->norm = db[id].ct.CT1;
			break;
	}		


	//
	// 주간리포트에 이벤트 추가
	//
	// pilog에서 norm을 제외하고 복사한다
	// 2025-3-18, eventQ에 append 하기전에 범위 확인한다
	if (eventQ.count < N_EVENT_Q) {
		memcpy(&eventQ.eq[eventQ.count], pilog, sizeof(EVENT_LOG));
		eventQ.count++;
	}
	else {
		printf("@@@ eventQ Full ...\n");
	}
	

	// 월단위 이벤트 로그에 추가
	sprintf(fsmsg.fname, "%s%04d%02d.d", EVENT_LIST_FILE, meter[id].cntl.tod.tm_year, meter[id].cntl.tod.tm_mon);
	//sprintf(fsmsg.fname, "%s%04d.d", EVENT_LIST_FILE, meter[id].cntl.tod.tm_year);
	strcpy(fsmsg.mode, "ab");	
	fsmsg.pbuf = (uint8_t *)pilog;	// 전역변수 만 쓸 수 있다
	fsmsg.size = sizeof(*pilog);
	putFsQ(&fsmsg);	
	
	// event FIFO File에 추가
	sprintf(fsmsg.fname, "%s", EVENT_FIFO_FILE);
	strcpy(fsmsg.mode, "ff");
	fsmsg.argv = N_EVENT_FIFO;
	fsmsg.pbuf = (uint8_t *)pilog;	// 전역변수 만 쓸 수 있다
	fsmsg.size = sizeof(*pilog);
	putFsQ(&fsmsg);	
	
	// Event FiFo에 추가한다
	memcpy(&pEvtFifo->elog[pEvtFifo->fr], pilog, sizeof(*pilog));	
	if (pEvtFifo->count < N_EVENT_FIFO) {
		pEvtFifo->count++;
		if (++pEvtFifo->fr >= N_EVENT_FIFO) pEvtFifo->fr = 0;
	}
	else {
		// Full 발생하면, fr, re 모두 이동한다
		if (++pEvtFifo->re >= N_EVENT_FIFO) pEvtFifo->re = 0;
		if (++pEvtFifo->fr >= N_EVENT_FIFO) pEvtFifo->fr = 0;
	}
	
	// 현재 이벤트 페이지 갱신한다
	fetchEvent(0);
	fetchItic(0);
	fetchItic2(0);
	
	printf("pushEvent(%d,%d.%d), itic count=%d\n", pilog->type, pilog->startTs, pilog->msec, pEvtFifo->count);
#if 1
	{
		switch (pInf->type) {
			case E_OC:
				assertEventOutput(db[id].pqevt[0].do_action, 1);
				break;
			case E_SAG:
				assertEventOutput(db[id].pqevt[1].do_action, 1);
				break;
			case E_SWELL:
				assertEventOutput(db[id].pqevt[2].do_action, 1);
				break;						
			case E_sINTR:
			case E_lINTR:
				assertEventOutput(db[id].pqevt[3].do_action, 1);
				break;			
			case E_TrV:
				assertEventOutput(db[id].pqevt[4].do_action, 1);
				break;			
			case E_TrC:
				assertEventOutput(db[id].pqevt[5].do_action, 1);
				break;							
		}		
	}
#endif

}

// 이벤트 FiFo를 지운다
void clearEventList(int id) {
	FS_MSG fsmsg;
	
	memset(ppqEvtCnt, 0, sizeof(*ppqEvtCnt));
	memset(pelist, 0, sizeof(*pelist));
	memset(pEvtFifo, 0, sizeof(*pEvtFifo));
	//sprintf(fsmsg.fname, "%s%04d%02d.d", EVENT_LIST_FILE, meter[id].cntl.tod.tm_year, meter[id].cntl.tod.tm_mon);
	//sprintf(fsmsg.fname, "%s%04d.d", EVENT_LIST_FILE, meter[id].cntl.tod.tm_year);
	
	
	strcpy(fsmsg.fname, EVENT_FIFO_FILE);
	strcpy(fsmsg.mode, "rm");
	fsmsg.pbuf = (uint8_t *)NULL;
	fsmsg.size = 0;
	putFsQ(&fsmsg);
}

//
//
//

void Energy_Task(void *arg) 
{
   uint32_t notificationValue;
   int	id=0;
   ADE9000_REG *pchip = &ade9000[id];
 
	// demand time stamp 초기화
	meter[id].cntl.dmdTs = sysTickDemand;
	meter[id].cntl.dmdTs15m = sysTick15m;
	//meter[id].cntl.dmdTs1H = meter[id].cntl.tod.tm_hour;
	meter[id].cntl.dmdStartTs = sysTick1s;
	meter[id].cntl.dmdStartTs15m = sysTick1s;	
	
	// energy log
	meter[id].cntl.egyTs1H = meter[id].cntl.tod.tm_hour;
	meter[id].cntl.egyTs1D = meter[id].cntl.tod.tm_mday;
	meter[id].cntl.egyStartTs1D = sysTick1s;
	
//	initEnergyLog();
	_enableTaskMonitor(Tid_Energy, 50);
		
	while (meter[id].cntl.runFlag) {
#ifdef __FREERTOS		
		xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);
#else
		os_evt_wait_and(0x1, 0xffff);
#endif		
		for (id=0; id<2; id++) {
			meter[0].cntl.wdtTbl[Tid_Energy].count++;
		
			//printf("tick : %d\n", sysTick32);
			if (ade9000[id].efr != ade9000[id].ere) {
				energy_scan(id, ade9000[id].energy[ade9000[id].ere], &egyNvr);
				ade9000[id].ere ^= 1;
			}
		}
	}
	
	printf("Energy_Task stopped ...\n");
#ifdef __FREERTOS	
	vTaskSuspend(NULL);
#else
	os_evt_wait_and(0xffff, 0xffff);
#endif	
}


void RMSLog_Task(void *arg)
{
	FAST_RMS *pFast;
	int i, j, id=0, bF=0;
   uint32_t ulNotificationValue;
	
	_enableTaskMonitor(Tid_Rmslog, 50);
	
	while (1) {
#ifdef __FREERTOS		
		xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY);
#else
		os_evt_wait_and(0x1, 0xffff);
#endif		
		meter[0].cntl.wdtTbl[Tid_Rmslog].count++;
		
		if (rmsWin.fr != rmsWin.re) {
			if (bF) {
				RMSCapture(id, rmsWin.re);
				if (++rmsWin.re >= 60) rmsWin.re = 0;
			}
			else {
				// 10 데이터가 채워지면 rms Capture 시작한다 
				if (rmsWin.fr > 10) {
					bF = 1;
					printf("[Buffer Ready]\n");
				}
			}
		}
      osDelayTask(100);
	}
}


// 전압이 sag 시작 조건(모든 전압이 sag limit 보다 커야한다)
int checkSagVolt(int id) {
	int i, ret=1;
	float	level = db[id].pt.vnorm*db[id].pqevt[PQE_SAG].level/100.;
	
	for (i=0; i<3; i++) {
		if (meter[id].cntl.U[i] < level) {	// level 보다 작으면 false
			ret = 0;
			break;
		}
	}		
	return ret;
}

int checkSagCond(int id) {
	return (db[id].pqevt[PQE_SWELL].nCyc > 0 && checkSagVolt(id));
}

int checkSwellVolt(int id) {
	int i, ret=1;
	float	level = db[id].pt.vnorm*db[id].pqevt[PQE_SWELL].level/100.;
	
	for (i=0; i<3; i++) {
		if (meter[id].cntl.U[i] > level) {	// level 보다 크면 false
			ret = 0;
			break;
		}
	}			
	return ret;
}

int checkSwellCond(int id) {
	return (db[id].pqevt[PQE_SWELL].nCyc > 0 && checkSwellVolt(id));
}


// alarm status, alarm list를 지운다 
void resetAlarm(int c) {
	int i;
	
	// clear alarm status
	// 2025-3-20, alarm count만 지운다 
	for (i=0; i<32; i++) {
		palm->st[i].count = 0;
	}
	palm->fr = palm->re = palm->almCount = 0;
	palm->resetTs = palm->updateTs = sysTick1s;
	
	// clear alarm list
	memset(palist, 0, sizeof(ALARM_LIST));	
}


void initMaxMinItv(int id) {
	// maxmin reset interval
	switch (db[id].etc.maxminItv) {
		case 0:	// daily
			meter[id].cntl.maxminTs = meter[id].cntl.tod.tm_mday;
			break;
		case 1:	// weekly
			meter[id].cntl.maxminTs = meter[id].cntl.tod.tm_wday;
			break;
		case 2:	// monthly
			meter[id].cntl.maxminTs = meter[id].cntl.tod.tm_mon;
			break;
	}	
}


// max, min refresh 주기를 검사하고 flag를 설정한다 
int checkMaxMinItv(int id) {
	switch (db[id].etc.maxminItv) {
		case 0:	// daily
			if (meter[id].cntl.maxminTs != meter[id].cntl.tod.tm_mday) {
				meter[id].cntl.maxminTs = meter[id].cntl.tod.tm_mday;
				meter[id].cntl.rstMaxMin = 0x1234;
			}
			break;
		case 1:	// weekly
			if (meter[id].cntl.maxminTs != meter[id].cntl.tod.tm_wday) {
				meter[id].cntl.maxminTs = meter[id].cntl.tod.tm_wday;
				meter[id].cntl.rstMaxMin = 0x1234;
			}
			break;
		case 2:	// monthly
			if (meter[id].cntl.maxminTs != meter[id].cntl.tod.tm_mon) {
				meter[id].cntl.maxminTs = meter[id].cntl.tod.tm_wday;
				meter[id].cntl.rstMaxMin = 0x1234;
			}
			break;
	}	
}

void PostScan_Task(void *arg) 
{
	int id=0;
   	uint32_t notificationValue;
   
#ifdef __FREERTOS	
	initCmdQ(xTaskGetCurrentTaskHandle());
#else
	initCmdQ(os_tsk_self());
#endif
	
	_enableTaskMonitor(Tid_PostScan, 50);
	
	while(meter[id].cntl.runFlag) {
#ifdef __FREERTOS		
		xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, pdMS_TO_TICKS(100));
#else
		os_evt_wait_or(0xf, 100);
#endif		
		for (id=0; id<2; id++) {
			meter[0].cntl.wdtTbl[Tid_PostScan].count++;
			
			// 1초 단위로 호출 (5번의 10/12 cycle 마다 호출된다)
			if (meter[id].cntl.rmsCalcF) {
				meter[id].cntl.rmsCalcF = 0;
				calcRmsAngle(id);

	#ifdef _CHIP_SAG_SWELL			
				if (!meter[id].cntl.sagEn) {
					if (checkSagCond()) {
						printf("Enable Sag ...\n");
						meter[id].cntl.sagEn = 1;
					}
				}
				if (!meter[id].cntl.swellEn) {
					if (checkSwellCond()) {
						printf("Enable Swell ...\n");
						meter[id].cntl.swellEn = 1;
					}
				}			
	#endif			
				meter[id].meter.utc = sysTick1s;
			}
			
			// 1s
			if (meter[id].cntl.pwrCalcF) {
				meter[id].cntl.pwrCalcF = 0;
				calcPower(id);
				
				if (meter[id].cntl.rstAlmList == 0x1234) {
					meter[id].cntl.rstAlmList = 0;
					resetAlarm(0);
					storeAlarmStatus();		
					// Alarm Fifo 지운다
					deleteAlarmLog();				
//					Board_LED_Off(1);				// alarm off
				}
				else if (alarmProc() > 0) {
					palm->updateTs = sysTick32;
					storeAlarmStatus();
//					Board_LED_On(1);				// alarm on
				}
				
				Board_LED_Set(1, palm->almCount);
			}
			
			// 1s
			if (meter[id].cntl.thdCalcF) {
				meter[id].cntl.thdCalcF = 0;
				calcTHD(id);
			}
			
			// demand 지운다 
			if (meter[id].cntl.rstMaxMin == 0x1234) {
				meter[id].cntl.rstMaxMin = 0;
				
				// 현재 영역을 last 영역으로 복사
				memset(pmm, 0, sizeof(MAXMIN));
				pmm->rstTime = sysTick1s;
				storeMaxMin();
			}
			
			if (pmm->fr != pmm->re) {
				pmm->ts = sysTick1s;
				// save MaxMin Data
				pmm->re = pmm->fr;
				storeMaxMin();
			}
		}
#ifdef __FREERTOS		
		if (notificationValue & 0x8) 
#else
		if (os_evt_get() & 0x8)
#endif		
		{
			cmdProc();
		}
	}
	
	// reboot가 set되면 
	printf("PostScan_Task stopped ...\n");
#ifdef __FREERTOS	
	vTaskSuspend(NULL);
#else
	os_evt_wait_and(0xffff, 0xffff);
#endif	
}


#if 	0
void loadITICevent() {		
	char name[64];
	FILE *fp;
	int nr;
	
#if 1
	// EVENT_FIFO_FILE을 사용한다 
	memset(pEvtFifo, 0, sizeof(*pEvtFifo));
#else	
	sprintf(name, "%s%04d.d", EVENT_LIST_FILE, meter[id].cntl.tod.tm_year);
	fp = fopen(name, "rb");
	if (fp == NULL) {
		printf("@ can not open %s\n", name);
		pitic->count = 0;
		return;
	}
	
	nr = fread(pitic->elog, 1, sizeof(EVENT_FIFO), fp);
	pEvtFifo->count = nr / sizeof(EVENT_LOG);
#endif	
	printf("--> loadITICevent, size=%d, count=%d\n", nr, pEvtFifo->count);
}

void clearEventFifoFile()
{
	char name[64];
	FILE *fp;
	int nr;
	
#if 1 
	memset(pEvtFifo, 0, sizeof(*pEvtFifo));
	printf("--> clearEventFifo ...\n");
#else	
	sprintf(name, "%s%04d.d", EVENT_LIST_FIFO, meter[id].cntl.tod.tm_year);
	fdelete(name);
#endif
	printf("--> clearIticFile, size=%d, count=%d\n", nr, pEvtFifo->count);
}
#endif	// 0

void Meter0_Task(void *param) 
{
	uint32_t stat, ldata;
	uint16_t wdata;
	int i, tc, rc, temp, id=0;
	METER_DEF *_pmeter = &meter[0];
	
	printf("[task Meter#0 started ...\n");
	printf("sizeof(METER_DEF) = %d\n", sizeof(METER_DEF));
	printf("meter:%d, %d\n", ((uint32_t)&_pmeter->meter - (uint32_t)_pmeter)/2, sizeof(_pmeter->meter)/2);
	printf("egy:  %d, %d\n", ((uint32_t)&_pmeter->egy - (uint32_t)_pmeter)/2, sizeof(_pmeter->egy)/2);
	printf("dm:   %d, %d\n", ((uint32_t)&_pmeter->dm - (uint32_t)_pmeter)/2, sizeof(_pmeter->dm)/2);
	printf("vq:   %d, %d\n", ((uint32_t)&_pmeter->vq - (uint32_t)_pmeter)/2, sizeof(_pmeter->vq)/2);
	printf("rpt:  %d, %d\n", ((uint32_t)&_pmeter->rpt - (uint32_t)_pmeter)/2, sizeof(_pmeter->rpt)/2);
	printf("hd:   %d, %d\n", ((uint32_t)&_pmeter->hd - (uint32_t)_pmeter)/2, sizeof(_pmeter->hd)/2);
	printf("ithd: %d, %d\n", ((uint32_t)&_pmeter->interhd - (uint32_t)_pmeter)/2, sizeof(_pmeter->interhd)/2);
	printf("wv:   %d, %d\n", ((uint32_t)&_pmeter->wv - (uint32_t)_pmeter)/2, sizeof(_pmeter->wv)/2);
	printf("mm:   %d, %d\n", ((uint32_t)&_pmeter->maxmin - (uint32_t)_pmeter)/2, sizeof(_pmeter->maxmin)/2);
	printf("alarm:%d, %d\n", ((uint32_t)&_pmeter->alarm - (uint32_t)_pmeter)/2, sizeof(_pmeter->alarm)/2);
	printf("alist:%d, %d\n", ((uint32_t)&_pmeter->alist - (uint32_t)_pmeter)/2, sizeof(_pmeter->alist)/2);
	printf("elist:%d, %d\n", ((uint32_t)&_pmeter->elist - (uint32_t)_pmeter)/2, sizeof(_pmeter->elist)/2);
	printf("pqec: %d, %d\n", ((uint32_t)&_pmeter->pqEvtCnt - (uint32_t)_pmeter)/2, sizeof(_pmeter->pqEvtCnt)/2);
	printf("log:  %d, %d\n", ((uint32_t)&_pmeter->log - (uint32_t)_pmeter)/2, sizeof(_pmeter->log)/2);
	printf("elog: %d, %d\n", ((uint32_t)&_pmeter->elog - (uint32_t)_pmeter)/2, sizeof(_pmeter->elog)/2);
	printf("dlog: %d, %d\n", ((uint32_t)&_pmeter->dlog - (uint32_t)_pmeter)/2, sizeof(_pmeter->dlog)/2);	//
	printf("lmm:  %d, %d\n", ((uint32_t)&_pmeter->lastmaxmin - (uint32_t)_pmeter)/2, sizeof(_pmeter->lastmaxmin)/2);	//
	printf("gwst :%d, %d\n", ((uint32_t)&_pmeter->gwst - (uint32_t)_pmeter)/2, sizeof(_pmeter->egy)/2);
	
	printf("iom:  %d, %d\n", ((uint32_t)&_pmeter->iom - (uint32_t)_pmeter)/2, sizeof(_pmeter->iom)/2);
	printf("info  %d, %d\n", ((uint32_t)&_pmeter->info - (uint32_t)_pmeter)/2, sizeof(_pmeter->info)/2);
	printf("set:  %d, %d\n", ((uint32_t)&_pmeter->setting - (uint32_t)_pmeter)/2, sizeof(_pmeter->setting)/2);
	printf("cmds :%d, %d\n", ((uint32_t)&_pmeter->cmds - (uint32_t)_pmeter)/2, sizeof(_pmeter->cmds)/2);

	printf("comm:     %d, %d\n", ((uint32_t)&_pmeter->setting.comm - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.comm)/2);
	printf("PT:       %d, %d\n", ((uint32_t)&_pmeter->setting.pt - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.pt)/2);
	printf("CT:       %d, %d\n", ((uint32_t)&_pmeter->setting.ct - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.ct)/2);
	printf("ETC:      %d, %d\n", ((uint32_t)&_pmeter->setting.etc - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.etc)/2);
	printf("PQE:      %d, %d\n", ((uint32_t)&_pmeter->setting.pqevt - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.pqevt)/2);
	printf("TVC:      %d, %d\n", ((uint32_t)&_pmeter->setting.transient - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.transient)/2);
	printf("rcrd:     %d, %d\n", ((uint32_t)&_pmeter->setting.rcrd - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.rcrd)/2);
	printf("trend:    %d, %d\n", ((uint32_t)&_pmeter->setting.trend - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.trend)/2);
	printf("pqRpt:    %d, %d\n", ((uint32_t)&_pmeter->setting.pqRpt - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.pqRpt)/2);
	printf("alarm:    %d, %d\n", ((uint32_t)&_pmeter->setting.alarm - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.alarm)/2);
	printf("iom:      %d, %d\n", ((uint32_t)&_pmeter->setting.iom - (uint32_t)&_pmeter->setting)/2, sizeof(_pmeter->setting.iom)/2);

	printf("egyNvr:   %d\n", sizeof(ENERGY_NVRAM));
	printf("egyLog:   %d\n", sizeof(ENERGY_LOG));
	printf("demand:   %d\n", sizeof(DEMAND));
	
	
	
	memset(&wQ[id], 0, sizeof(wQ[id]));
	
	wbFFT8k[id].fr = wbFFT8k[id].re = 0;

	initADE9000(id);
	
	initTransientTrigger(id);

	ExtINTR_Init(0, 0, 0);	// PINT0, EINT0, active low
	ExtINTR_Enable(0);	// PINT0
	//os_itv_set(5);
	
	// 처음 시작 하거나 재시작할때 대책 ????
	initPQHeader(id);
	
	_enableTaskMonitor(Tid_Meter, 50);
	
	while (1) {
		meter[0].cntl.wdtTbl[Tid_Meter].count++;
		
		meter_scan(id);
//		os_dly_wait(1000);
	}
}

void Meter1_Task(void *param) {
	int	id=1;
//	
	printf("[task Meter#1 started ...\n");
//	
	memset(&wQ[id], 0, sizeof(wQ[id]));
		
	wbFFT8k[id].fr = wbFFT8k[id].re = 0;

	initADE9000(id);
//	
//	memset(&wQ, 0, sizeof(wQ));
//	wQ.count = PG32K_CNT;
//	
//	//wb32k.fr = wb32k.re = 0;
//	

//	//init_fftTable();
//	
	ExtINTR_Init(id, 2, 0);	// PINT1, EINT2
	ExtINTR_Enable(id);	// PINT1
//	//os_itv_set(5);
	_enableTaskMonitor(Tid_Meter2, 50);
//	
	while (1) {
		meter[0].cntl.wdtTbl[Tid_Meter2].count++;
		meter_scan_2(id);
//		//os_dly_wait(1000);
	}
}


int checkPassword(uint8_t *pwd) {
	return 1;
}

// 2025-3-25
void getMeterIpAddr(uint8_t *buf) {
	buf[0] = pInfo->ipAddr[0];
	buf[1] = pInfo->ipAddr[1];
	buf[2] = pInfo->ipAddr[2];
	buf[3] = pInfo->ipAddr[3];
}

// 2025-3-25
void setMeterIpAddr(uint32_t ipAddr) {
	pInfo->ipAddr[0] = ipAddr & 0xff;
	pInfo->ipAddr[1] = (ipAddr >> 8)  & 0xff;
	pInfo->ipAddr[2] = (ipAddr >> 16) & 0xff;
	pInfo->ipAddr[3] = (ipAddr >> 24) & 0xff;
}


// ftp client
#ifdef __FTP_CLIENT
void ftpc_notify (U8 event) {
  /* Result notification function. */

  switch (event) {
    case FTPC_EVT_SUCCESS:
      printf ("Command successful\n");
      break;

    case FTPC_EVT_TIMEOUT:
      printf ("Failed, timeout expired\n");
      break;

    case FTPC_EVT_LOGINFAIL:
      printf ("Failed, username/password invalid\n");
      break;

    case FTPC_EVT_NOACCESS:
      printf ("Failed, operation not allowed\n");
      break;

    case FTPC_EVT_NOTFOUND:
      printf ("Failed, file or path not found\n");
      break;

    case FTPC_EVT_NOPATH:
      printf ("Failed, working directory not found\n");
      break;

    case FTPC_EVT_ERRLOCAL:
      printf ("Failed, local file open error\n");
      break;

    case FTPC_EVT_ERROR:
      printf ("Failed, unspecified protocol error\n");
      break;
  }
}


void FTPC_Task(void) {
	U8 srvip[4] = {192,168,8,71};
	
	while (1) {
		if (daq.ftpreq) {
			daq.ftpreq = 0;				
			printf("--> start ftp client, %s\n", daq.filename);
			ftpc_connect (srvip, 0, FTPC_CMD_PUT, ftpc_notify);
		}
		else {
			osDelayTask(1000);
		}
	}
}


#endif
