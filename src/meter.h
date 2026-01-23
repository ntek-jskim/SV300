#ifndef _METER_H

#define	_METER_H

//#include <LPC43xx.h>  
#include "os_port.h"
#include "board.h"
#include "time.h"

// Qual Test 
#undef	_QUAL_TEST

#define	SYS_DIR	"\\system"
#define	LOG_PQ_DIR "\\log_pq"
#define	LOG_TREND_DIR	"\\log_trend"
#define	TRG_PQ_DIR "\\trg_pq"
#define	TRG_TRANSIENT_DIR "\\trg_tvc"
#define	FW_DIR "\\firmware"
#define	ALARM_DIR	"\\alarm"
#define	EVENT_DIR "\\event"

#define	ALOG_VER "0"
#define	ELOG_VER "0"
#define	QUAL_VER "1"
#define	CONCAT(x, y) x""y
#define	CONCAT3(x, y, z) x""y""z
#define	CONCAT4(x, y, z, a) x""y""z""a

#define	TEMP_FILE	"\\temp.$$$"

#define	QW_TEMP_FILE	CONCAT4(LOG_PQ_DIR, "\\qw", QUAL_VER, "_temp.d")
#define	QW_LAST_FILE	CONCAT4(LOG_PQ_DIR, "\\qw", QUAL_VER, "_last.d")
#define	QL_TEMP_FILE	CONCAT4(LOG_PQ_DIR, "\\ql", QUAL_VER, "_temp.d")

#define	QW_FILE CONCAT4(LOG_PQ_DIR, "\\qw", QUAL_VER, "_")
#define	QL_FILE CONCAT4(LOG_PQ_DIR, "\\ql", QUAL_VER, "_")

#define	SETTING_FILE	CONCAT(SYS_DIR, "\\settings.d")
#define DEMAND_FILE CONCAT(SYS_DIR, "\\demand.d")
#define DEMAND_FILE1 CONCAT(SYS_DIR, "\\demand.d1")
#define DEMAND_FILE2 CONCAT(SYS_DIR, "\\demand.d2")
#define	ENERGY_FILE	CONCAT(SYS_DIR, "\\energy.d")

#define	ENERGY_LOG_FILE0	CONCAT(SYS_DIR, "\\egy_log0.d")
#define	ENERGY_LOG_FILE1	CONCAT(SYS_DIR, "\\egy_log1.d")

#define	MAXMIN_FILE	CONCAT(SYS_DIR, "\\maxmin.d")
#define	ALARM_ST_FILE CONCAT(SYS_DIR, "\\astat.d")
#define ALARM_LIST_FILE CONCAT4(ALARM_DIR, "\\alog", ALOG_VER, "_")
#define EVENT_LIST_FILE CONCAT4(EVENT_DIR, "\\elog", ELOG_VER, "_")
#define TREND_FILE CONCAT(LOG_TREND_DIR, "\\trd")

#define	EVENT_FIFO_FILE	CONCAT4(EVENT_DIR, "\\elog", ALOG_VER, "_fifo.d")
#define	ALARM_FIFO_FILE	CONCAT4(ALARM_DIR, "\\alog", ALOG_VER, "_fifo.d")

#define	LOG_FIFO_SIZE	1024

#define	F_INIT	"init.ini"

#define	IOM_DO_BASE0	6148	// ~ 3451
#define	IOM_DO_BASE1	6208	// ~ 3493
// gems7000
#define	MBAD_G7_METER	0
#define	MBAD_G7_MINMAX	300
#define	MBAD_G7_HARMONICS 800
#define	MBAD_G7_DEMAND 1300
#define	MBAD_G7_ALARM 1500
#define	MBAD_G7_WAVE 1900
#define	MBAD_G7_EVENT 3000
#define	MBAD_G7_IOM 3400
#define	MBAD_G7_SOE 3550

#define	MBAD_G7_SETTING 6300
#define	MBAD_SET_P1	6300		// Comm, PT, CT, ETC
#define	MBAD_SET_P2	6410		// Sag, Swell, Transient, WaveRcrd, Disturbance
#define	MBAD_SET_P3	6470		// Trend Rcrd, PQ Report, 
#define	MBAD_SET_P4 6560		// Alarm Setting
#define	MBAD_SET_P5	6760		// IO Setting#1
#define	MBAD_SET_P6 6850		// IO Setting#2
#define	MBAD_SET_TS 6940		// Time 
#define	MBAD_G7_CMD 6950		
#define	MBAD_G7_END 7000

#define	ADD_ADE9000 10000


#define ZCT_NONE		0
#define ZCT_2000_1		1
#define ZCT_2500_1		2
#define ZCT_300_1		3

#define	MAC_MSB0	0x1e
#define	MAC_MSB1	0x35


typedef struct {
	uint16_t mk;
	uint16_t crc;
	uint16_t reserved[2];
	int64_t	kWh[8];		// kWh=imp/exp/tot/net, this month imp/exp/tot/net
	int64_t	kvarh[8];	// kvarh=imp/exp/tot/net, this month imp/exp/tot/net
	int64_t	kVAh[2];	// tot, this month
} FRAM_ENERGY1;

typedef struct {
	uint16_t mk;
	uint16_t crc;
	uint16_t amr[2];
	int64_t kWh[4];		
	int64_t kvarh[4];	
	int64_t kVAh;			
} FRAM_ENERGY2;


typedef struct {
	uint16_t year;
	uint16_t month;
	uint16_t mday;
	uint16_t hour;
	uint16_t minute;
	uint16_t sec;
} TIME_STAMP;


// WaveFormBuffer : 2048, 32bit memory(16 Page), 각 상별로 256 데이터 저장한다 
// 1. 8k Sample : 32m Interrupt 발생된다.
// 2. 32k Sample : 8ms 마다 interrupt 발생된다.
// 데이터 수집을 위해 page 7, 15가 찰때 마다 intr이 발생되므로 주기는 16ms/4ms 이다 

//  8k sps : 1 PG_BUF = 16 sample -> 8000/16 = 1초에 500 page 필요하다 
#define	PG8K_CNT_1s		(500)		

// 32k sps : 1 PG_BUF = 16 sample -> 32000/16 = 1초에 2000 page 필요하다
// XXX: 1초 데이터 저장, 1000ms/8ms=125*16page=2000 page 필요하다 
#define	PG32K_CNT_1s	(2000)	

// 200ms 마다 sample 수집해서 보낸다, Dobule buffer로 사용한다 
#define	PG8K_CNT	(PG8K_CNT_1s/5)			//  500/5 -> 100
#define	PG32K_CNT	(PG32K_CNT_1s/5)		// 2000/5 -> 400

//#define	PG_BUF_CNT	(PG32K_CNT*2)
#define	PG_BUF_CNT	(PG8K_CNT*2)

#define	L_HFWVWIN	6400	// 32k sps, 200ms 저장
#define	L_LFWVWIN	1600	//  8k sps, 200ms 저장

typedef struct {
	int32_t w[3][L_HFWVWIN];
} WAVE_PHASE_HF;

typedef struct {
	int32_t w[3][L_LFWVWIN];
} WAVE_PHASE_LF;


//typedef struct {
//	uint64_t ts;
//	float vrms[3], irms[3];
//	int32_t hI[3][L_HFWVWIN];
//	int32_t hU[3][L_HFWVWIN];	
//	int32_t lI[3][L_LFWVWIN];	// 12cyc sample
//	int32_t lU[3][L_LFWVWIN];	
//} WAVE_WINDOW;
typedef struct {
	uint64_t ts;
	float vrms[3], irms[3];
	WAVE_PHASE_HF hI;
	WAVE_PHASE_HF hU;	
	WAVE_PHASE_LF lI;	// 12cyc sample
	WAVE_PHASE_LF lU;	
} WAVE_WINDOW;

typedef struct {
	int ix;
	WAVE_WINDOW	ww[3];	
} WAVE_WINDOW_BLK;

//typedef struct {
//	int32_t hD[3][L_HFWVWIN];
//} WAVE_32K_DATA;

typedef struct {
	uint16_t	fr, re;
	uint64_t ts;
	//int32_t I[3][1280];
	//int32_t U[3][1280];
//	int32_t I[3][1024];
//	int32_t U[3][1024];		
	int32_t I[3][L_HFWVWIN];	// 12cyc sample
	int32_t U[3][L_HFWVWIN];	
} WAVE_32K_BUF;


typedef struct {
	uint16_t	fr, re;
	uint64_t ts;
	float	vrms[3], irms[3];
	//int32_t I[3][1280];
	//int32_t U[3][1280];
//	int32_t I[3][1024];
//	int32_t U[3][1024];	
	int32_t I[3][L_LFWVWIN];	// 12cyc sample
	int32_t U[3][L_LFWVWIN];	
	int32_t Upp[3][L_LFWVWIN];
} WAVE_8K_BUF;

// wave capture buffer
#define	N_DAQ_BUF	(32000)
typedef struct {
	uint16_t	dx, complete, ftpreq;
	uint64_t	ts;
	
	char	filename[32];
	uint8_t	*ftpbuf;
	int		_filesize, _nleft, _fileptr;
	int		idle;
	//
	int 	normalize;
	// header (onion type)
	float vfull, ifull;
	float vscale, iscale;
	//
	uint32_t chunkId, chunkSize;
	uint32_t format;
	uint32_t subChunk1Id, subChunk1Size;
	uint16_t audioFormat, nChannels;
	uint32_t sampleRate, byteRate;
	uint16_t blockAlign, bitsPerSample;
	uint32_t subChunk2Id, subChunk2Size;
	union {
		int 	   ldata[N_DAQ_BUF][6];	// variable rate buffer;
		short		 sdata[N_DAQ_BUF*2][6];	// variable rate buffer;
	} wd;
} DAQ_BUF;

// Transient Capture용
//#define	N_HFCAP	1600
#define	N_HFCAP	(1600*2)

//typedef struct {	
//	uint64_t ts;	// 발생시각
//	float	scale;
//	uint16_t pos, mask, srate, _r;
//	int32_t hU[3][N_HFCAP];		
//	int32_t hI[3][N_HFCAP];
//} WAVE_HF_CAP;

// 전압 또는 전류만 저장한다 
typedef struct {	
	uint64_t ts;	// 발생시각
	float	scale;
	uint16_t pos, mask, srate, _r;
	int32_t hD[3][N_HFCAP];		
} WAVE_HF_CAP;

//// Sag/Swell Capture용
#define	N_LFCAP	(1600*2)

typedef struct {
	uint64_t ts;		// 발생시각
	float		scale;	
	uint16_t pos;
	uint16_t mask;	// 발생이벤트 위치, mask

	uint16_t srate;
	uint16_t _r;
	
	int32_t lW[3][N_LFCAP];	//int32_t lI[3][N_LFCAP];	
} WAVE_LF_CAP;

#define	N_FASTRMS_BUF	60

typedef struct {
	uint64_t ts;
	int32_t U[3][120];
	int32_t I[3][120];
} FAST_RMS;

typedef struct {
	int fr, re, ix, n;
	FAST_RMS	buf[N_FASTRMS_BUF];
} FAST_RMS_BUF;


typedef struct {
	uint64_t ts;
	uint16_t pos, mask, _r0[2];
	float	rms[3][1200];
	//float	I[3][1200];
} RMS_CAP;


typedef struct {
	uint64_t ts;	// 발생시각
	
	uint16_t pos[2];
	uint16_t mask;	// 발생이벤트 위치, mask
	uint16_t type;	// 0: start, 1: end
	
	uint16_t sampleRate;	// 8000 or 32000
	uint16_t _r;
	
	float		val[3];	// sag/swell Value
	float		engMax;	// U full
	int32_t rawMax;	// adc Full
	
	int32_t lU[3][N_LFCAP];
	int32_t lI[3][N_LFCAP];	
	float	U[3][1200];
	float	I[3][1200];
} PQ_EVT_CAP;



#define	M_FFT	4096
#define	N_FFT	1600

typedef struct {
	float cos_rdx[M_FFT/2], sin_rdx[M_FFT/2];	// radix2:N/2, bluestein:N
	float cos_czt[N_FFT], sin_czt[N_FFT];			// radix2:N/2, bluestein:N
	float xreal[N_FFT], ximag[N_FFT];					// input
	float areal[M_FFT], aimag[M_FFT];	
	float breal[M_FFT], bimag[M_FFT];	
	float creal[M_FFT], cimag[M_FFT];	
	float amp[N_FFT/2];												// output
} FFT_CZT;


//typedef union {
//	int16_t b16[2];
//	int32_t b32;
//} UBUF;

//// single page (128 Sample, 128/8=16/Phase
//typedef struct {
//	int32_t	ub[128];	// 8 channel * 16 data
//} WAVE8k_PAGE;

// 32K Wave, SINC Filter Range: 67,107,786/V
// 8K  Wave, Decimation Filter Range : 74,518,688

// 한페이지에 128 sample, phase당 16 sample
// 1초 sample 저장위해 512 page 필요하다 

//typedef struct {
//	int fr, re;
//	int32_t wb[PG8K_CNT][128];
//} WAVE8k_PGBUF;

typedef struct  {
	int32_t buf[128];
} WVPG;

//typedef struct {
//	int fr, re;
//	//WVPG wb[PG32K_CNT];
//	uint32_t ts[PG32K_CNT];
//	int32_t wb[PG32K_CNT][128];
//} WAVE32k_PGBUF;

// 2019-9-18: time stamp 추가
// 2023-5-10: ready 추가
typedef struct {
	int fr, re, count;
	uint16_t lastolc, olc;
	uint64_t ts;
	int hs, halfFull;	// high speed, buff full
	
	WVPG	wb[PG_BUF_CNT];
	//int32_t wb[PG32K_CNT][128];
} WAVE_PGBUF;

typedef struct {
	int16_t fr, re;
	int16_t	wave[1024][6];
} ADE_WAVE;

//typedef struct {
//	int32_t	wv[N_WAVE*8][7];
//} WV32_BUF;

enum BAUDRATE {
	B9600,
	B19200,
	B38400,
	B57600,
	B115200
};

enum CT_TYPE {
	CT_5A,
	CT_100mA,
	CT_333mV,
	CT_RCT,
};

enum FREQ_VAL {
	FREQ_60,
	FREQ_50
};

// 2016-12-19, Wiring mode 추가
enum WIRING_MODE {
	WM_3LN3CT, //WM_3P4W,
	WM_3LL3CT, //WM_3P3W(3CT),
	WM_1LN1CT, //WM_1P2W,
	WM_1LL2CT, //WM_1P3W
	WM_3LL2CT, //WM_3P3W(2CT)
	SIMULATION,
};

typedef struct {
	uint32_t utc;	  // Year-Month-MDay, hour:minute:second	
	float Freq;
	float	Temp;
	//
	float U[4];	
	float	Upp[4];		// U[3] = average
	float Ubal[2];	// Uu/Uo
	float	Uzs[2], Ups[2], Uns[2];	// zero seq., pos. seq., negative seq.
	float Uangle[3];
	float UUndev[3];
	float UOvdev[3];	
	float	Ur1[2];
	//
	float	I[4], Itot, In, Isum, Ig; // In: Neutal Current(measured), Isum: Neutral Current(Calculated)
	float 	Izs[2], Ips[2], Ins[2], Imax[3];
	float Iangle[3];
	float	Ibal[2], Ir2;
	//
	float	P[4], fP[4];	
	float	Q[4], fQ[4];
	float	S[4], fS[4];
	float	PF[4], dPF[4];
	float 	D[4];	// deformed power D
	float 	Pangle[4];	// power angle.
	//
	float 	THD_U[3], THD_Upp[3];
	float	CF_U[3], CF_Upp[3];
	float 	THD_I[3], TDD_I[3], KF_I[3], CF_I[3];
	float fU[4], fI[4], _r2[3];	// float _r2[27];
	// io 영역 중 일부를 복사
	float temp[8];		// temp or aim
	uint16_t dim[16];
	uint16_t dom[12];
	uint16_t aom[4];
	uint16_t battVolt, _r3;
} METERING;

typedef struct {
	uint32_t mdTime;
	float	value;	
} MAX_DEMAND;

typedef struct {
	// Max Demand
	MAX_DEMAND MD_P[2], MD_Q[2], MD_S, MD_I[3];
	// Dynamic Demand
	uint32_t ddTime;
	float DD_P[2], DD_Q[2], DD_S, DD_I[3];
	// present Demand
	float	CD_P[2], CD_Q[2], CD_S;
	// Predict Demand
	float	PD_P, DD_P_15, _r[2];	
	// Demand Log(24시간=4*24)	
	uint32_t dmdLogTs;	// demand log의 시작 시간, 0=미지정
	float	DP_P_Log[96];
	float r1[18];
	uint16_t magic, crc;
} DEMAND;
//


#define	MAX_EREG64	(99999999999LL)
#define	MAX_EREG32	(999999999)
#define	EREG32_UNIT		(100)

// REG64: 99,999,999.999 wh (1w 단위)
typedef struct {
//	uint64_t kwh[2];	// import, export
//	uint64_t kvarh[2];	// import, export
//	uint64_t kvah;
	uint64_t eh[3][2];
} ENERGY_REG64;

// REG32: 99,999,999.9 kwh(0.1kwh 단위)
typedef struct {
//	uint32_t kwh[2];	// import, export
//	uint32_t kvarh[2];	// import, export
//	uint32_t kvah;
	uint32_t eh[3][2];
} ENERGY_REG32;

typedef struct {
//	float kwh[2];	// import, export
//	float kvarh[2];	// import, export
//	float kvah;
	float eh[3][2];
} ENERGY_FREG;
													
// modbus  영역 -> 32bit에서 64bit로 변경
typedef struct {																																														
	ENERGY_REG32	Ereg32[3];	// Total, Last Month, This Month
	ENERGY_REG64  Ereg64;
	uint32_t      _r[20];
} ENERGY;

typedef struct {																				
	uint32_t magic;
	uint32_t ts;	
	ENERGY_REG64 Ereg64[2][3];	// Total, Last Month, This Month		
	uint16_t _r[3], crc;
} ENERGY_NVRAM;


typedef struct {
	// deviation
	float Uud[3], Uod[3];
	// Flicker
	float Pi[3], Pim[3], Pst[3], Plt[3];		
	float	SV[3];	
	float r[29];	
} VQDATA;

#ifdef VQ_VAR_V0

// EN50160
typedef struct {
	uint32_t sTime, eTime;
	uint16_t status, compliance;
	uint16_t Fvar1, Fvar2, Ubalance;
	uint16_t Uvar1[3], Uvar2[3];
	uint16_t Pst[3], Plt[3];
	uint16_t Dip[3], Swell[3];
	uint16_t INTRs[3], INTRl[3];	// Short Interruption, Long Interruption
	uint16_t Urc[3];	//rapid change
	uint16_t THD[3], Harmonice[3], SV[3];
	uint16_t r1[5];
} EN50160;

#else

// EN50160
typedef struct {
	uint32_t sTime, eTime;
	uint32_t compliance;
	// variation
	uint16_t Fvar1, Fvar2;
	uint16_t Volt1[3], Volt2[3];
	uint16_t Voltbal;
	uint16_t VoltThd[3], VoltHd[3];
	uint16_t Pst[3], Plt[3], Svolt[3];
	// event count
	uint16_t sag[4], swell[4];
	uint16_t shortIntr[4], longIntr[4];	// Short Interruption, Long Interruption
	uint16_t rvc[4];
} EN50160;

#endif

typedef struct {
	uint16_t U[3][64];
	uint16_t Upp[3][64];
	uint16_t I[3][64];	
	uint16_t r1[24];
} HARMONICS;

typedef struct {
	float U[3][64];
	float Upp[3][64];
	float I[3][64];	
	float r1[24];
} HARMONICS_RAW;

typedef struct {
	uint16_t U[3][80];
	uint16_t Upp[3][80];
	uint16_t I[3][80];
	uint16_t r[80];
} INTERHARMONICS;

// modbus
typedef struct {
	int16_t U[3][160];	
	int16_t Upp[3][160];	
	int16_t I[3][160];	
	float	vscale;
	float iscale;
	int16_t r[56];
} WAVEFORM_L16;

typedef struct {
	int16_t ph[640];	
	//float ph[640];
} WAVEPHASE;

typedef struct {
	WAVEPHASE U[3];	
	WAVEPHASE Upp[3];	
	WAVEPHASE I[3];	
	int Umax[3], UppMax[3], Imax[3];
	float vscl[3], iscl[3];
} WAVEFORM_GUI;


typedef struct {
	float max;
	uint32_t max_ts;
	float min;
	uint32_t min_ts;
} MAXMIN_DATA;

typedef struct {
	uint16_t chan, cond;
	float level;	
	uint16_t status, count;	
} ALARM_DATA;

typedef struct {
	uint16_t fr, re;
	uint32_t resetTs;	
	ALARM_DATA st[32];
	uint32_t updateTs;	
	uint16_t almCount, _r;	
} ALARM_STATUS;

#define	N_ALARM_LIST	32
#define	N_ALARM_FIFO	1024

typedef struct {
	uint32_t ts;
	uint16_t status;	// 0 : Normal, 1: Alarm
	uint8_t chan, cond;
	float	level;
	float	value;			// min or max;
} ALARM_LOG;

typedef struct {
	int	magic;
	int 	fr;
	int 	count;
	uint32_t ts;
} ALARM_HEAD;

typedef union {
	ALARM_HEAD	head;
	ALARM_LOG	data;
} ALARM_U;

typedef struct {
	uint16_t 	fr, re, count, seq;
	ALARM_LOG	alog[N_ALARM_LIST];
	uint16_t 	_r1[40];
} ALARM_LIST;

#if 1	/ 2025-3-13
typedef struct {
	uint16_t 	fr, re, count, next;
	ALARM_LOG	alog[N_ALARM_FIFO];
} ALARM_FIFO;
#endif

#define	E_SAG	1		
#define	E_SWELL	2
#define	E_sINTR	3
#define	E_lINTR	4
#define	E_OC	5
#define	E_RVC	6
#define	E_TrV	7
#define	E_TrC	8
// IOM
#define	E_SOE	9

#define	N_EVENT_LIST	32
#define	N_ITIC_LIST		32
// EVENT Log : 14 wrod
#define	N_EVENT_FIFO	1024

typedef struct {
	uint32_t startTs;
	uint16_t msec, duration, type, mask;
	float	level[3];
} EVENT_LOG;

typedef struct {
	uint16_t 	fr, re, count, seq;
	EVENT_LOG 	elog[N_EVENT_LIST];
	uint16_t 	_r1[2];
} EVENT_LIST;

typedef struct {
	uint32_t startTs;
	uint16_t msec, duration, type, mask;
	float	level[3];
	float	norm;	// 정격전압, 정격전류
} ITIC_LOG;

typedef struct {
	uint16_t 	fr, re, count, seq;
	ITIC_LOG 	elog[N_ITIC_LIST];
	uint16_t 	_r1[148];
} ITIC_EVT_LIST;

typedef ALARM_HEAD EVENT_HEAD;

typedef union {
	EVENT_HEAD	head;
	ITIC_LOG		data;
} EVENT_U;


typedef struct {
	uint16_t fr, re, count, next;
	ITIC_LOG elog[N_EVENT_FIFO];
//	uint16_t ipsm_count, r[3];
} EVENT_FIFO;


#define	N_EVENT_Q	1024
// 10분간 발생 이벤트 임시 저장
typedef struct {
	uint16_t count;
	EVENT_LOG eq[N_EVENT_Q];
} EVENT_Q;

typedef struct {
	uint16_t tvc, tcc;
	uint16_t oc, sag, swell, intr;
	uint16_t _r[4];
} PQ_EVENT_COUNT;

typedef struct {
	uint16_t fr, re;
	uint32_t rstTime;
	MAXMIN_DATA	Freq;
	MAXMIN_DATA	Temp;
	MAXMIN_DATA	U[4];
	MAXMIN_DATA Upp[4];
	MAXMIN_DATA I[4], Itot, In, Isum;
	MAXMIN_DATA P[4], Q[4], S[4];
	MAXMIN_DATA PF[4];
	MAXMIN_DATA THD_U[3], THD_Upp[3], THD_I[3];
	uint32_t ts;	// update
	uint16_t r0, crc;
	uint16_t r1[24];
} MAXMIN;

typedef struct {
	uint16_t  comMode;
	uint16_t	baud;
	uint16_t  parity;	//0:none, 1:odd, 2:even
	uint16_t	devId;	//1~250
	//
	uint16_t ip0[4], sm0[4], gw0[4], dns0[4], sntp[4];
	char     host[32];
	uint16_t tcpPort;		// std modbus port, ext modbus port
	uint16_t dhcpEn;
	// gems3500
//	struct {
//		uint16_t enable, busId;
//	} gw[2];
	
	// Gateway Enable Bit
	// 0: 3500 #1
	// 1: 3500 #2
	// 2: 3500 CB #1
	// 3: 3500 CB #2
	// 4: 3500 Trip #1
	// 5: 3500 Trip #2
	// 6: 3500 ZCT #1
	// 7: 3500 ZCT #2
	uint16_t gwEable;	//		
	uint16_t onionTcpPort;	
	uint16_t r1[2];
	//
	uint16_t daq_ip[4];	// update server ip
	//
	uint16_t RS485MasterMode;	// 0: None, 1: Serial Temp Sensor, 2: TGW10  3: Azbil gateway, 4 : Azbil MCF
	uint16_t daq_srate;	// sampling rate (0: 32k, 1:16k, 2:8k: 3:4k, 4:2k
	uint16_t daq_length;	// 0: 2k, 1:4k, 2:8k, 3:16k, 4: 32k, 5:64k
	uint16_t daq_interval;	//  minute
	uint16_t daq_format;	// 0: wave, 1: onion
	uint16_t daq_id; // 0 ~ 65535
	uint8_t  daq_bitpersample;	// 0 : 16it, 1: 32bit

	uint16_t pullup_485; // 0 ~ 65535
//	uint16_t r0[2];
	uint16_t useSntp;
	uint16_t sntpInterval;
} COMM_CFG;


typedef struct {
	uint16_t	wiring;	// 0:3p4w(3LN,3CT), 1:3p3W(2CT):2LL,2CT, 2:3P3W(3CT):2LL,3CT, 3:1P2W, 4: 1P3W 5 : Simulation
	uint16_t	freq;	// 0:60, 1:50
	uint32_t	vnorm;
	uint32_t	PT1;
	uint16_t	PT2;
	uint16_t	r1[3];	
} PT_DEF;

typedef struct {
	uint16_t	inorm;
	uint16_t	CT1;
	// CT Type
	uint16_t	CT2;	
	uint16_t	nTurns;
	uint16_t	I_start;	
	uint16_t	ct_dir[3];	
	uint16_t	rogowskiParam;	// unused
	int16_t		phaseOfs[3];
	uint16_t  	zctType, zctScale;
	uint16_t	r2[6];	
} CT_DEF;

#define		DO_OPEN_PHASE		0


typedef struct {
	uint16_t	VA_type;
	uint16_t	PF_sign;
	uint16_t	interval;
	uint16_t  	Iload;	
	uint32_t	P_target;	
	uint16_t  	backlightTime;		// 0: always, 1 ~ 15분
	uint16_t  	brightness;		// 0 ~ 9
	uint16_t  	autorotation;	// 0: hold, 1: auto rotation	
	int16_t 	timezone;			// 분 단위로 환산한 시간, seoul(9h, 540m)
	uint16_t  	maxminItv;
	uint16_t  	testMode;
	uint16_t  	testMode_Period;
	uint16_t  	r3[7];
//	uint16_t  	r3[9];
} ETC_DEF;

typedef struct {
	uint16_t	level;
	uint16_t	nCyc;
	uint16_t  action;
	uint16_t  holdOffCyc;
	uint16_t  do_action;
} PQEVENT;

typedef struct {
	uint16_t holdOff;
	uint16_t level;
	uint16_t fastChange;
	uint16_t action;
} TRANSIENT_DEF;

typedef struct {
	uint16_t resolution;
	uint16_t params;
	uint16_t pre, post;
} RECORDER_DEF;

typedef struct {
	uint16_t active, interval, version;
	uint16_t chan[16];
} TREND_DEF;

typedef struct {
	uint16_t active, startDay;
	uint16_t r0[8];
} PQREPORT_DEF;

#define	MAX_ALARM_CH	124

typedef struct {
	uint16_t delay, r0[3];
	struct {
		uint16_t chan, cond, dband, do_action;
		float level;
	} set[32];
	uint16_t r1[4];
} ALARM_DEF;



typedef struct {
	uint16_t modType, _r0;	// 1,2:DIO, 3,4:RTD, 5,6:AIO, Com: 16
	uint32_t ts;
	
	uint16_t diType[8];	// 0:di, 1: pi
	uint16_t debounce[8];	// 1 ~ 255msec
	uint16_t piConst[8];	// 1 ~ 255msec
	// DO Channel을 4->8으로 증설(2개는 비운다)
	uint16_t doType[8];	// 0:output, 1:alarm	 	
	uint16_t doMode[8];	// 0:latch, 1: pulse
	uint16_t doTimer[8];	// pulse width, 0.1 ~ 10s		
	float	aiScale[4][2];// Eng Scale(Gain/Offset)
	struct {
		uint16_t shuntType;	// Shunt Type : 0=50mV, 1:60mV, 2:100mV)
		uint16_t shuntNorm;	// Shunt Nominal Current(default:50A)	
	} dcm[2];
	uint16_t aoDataIndex[2];
	float	aoScale[2][2]; // Eng Scale
	uint16_t _r[8];
} IO_CFG;

#define	PQE_OC	0
#define	PQE_SAG	1
#define	PQE_SWELL	2
#define	PQE_INTR	3

typedef struct {
	COMM_CFG	comm;
	PT_DEF	pt;	
	CT_DEF	ct;
	ETC_DEF etc;
	
	PQEVENT pqevt[4];		// 0: OC, 1: sag, 2:swell, 3: Interruption
	
	TRANSIENT_DEF	transient[2];
	uint16_t _r1[12];
	
	RECORDER_DEF rcrd[2];
	uint16_t _r2[12];
	
	TREND_DEF	trend[4];
	uint16_t _r3[4];
	
	PQREPORT_DEF pqRpt;	
	
	ALARM_DEF	alarm;
	
	IO_CFG	iom[2];

//	FLOW_SETTING	f_setting[MAX_FLOW];
	//uint16_t _r4[9], crc;
} SETTINGS;

//typedef struct {
//	uint16_t header[4];
//	struct {
//		uint16_t almChan;
//		uint16_t state;
//		uint16_t count;
//	} alm[32];
//} ALARM_DEF;


//// 1초 읽어서 10분 데이터 생성(Freq) 
//typedef struct {
//	uint32_t ts10m;
//	uint32_t count, FreqErr[2];
//} QualDataFreq;

//// 1초 데이터 읽어서 10분 데이터 만든다 
//typedef struct {
//	uint32_t ts, count10m;
//	int   Uerr[3][2], UbalErr, UthdErr[3];
//	float U[3], I[3], Uthd[3], Ithd[3], Ubal;
//	float W[3], PF;
//	
//	float	Us[3], Is[3], UthdS[3], IthdS[3], UbalS;	
//	float	Ws[3], PFs;
//} QualData;

#ifdef VQ_VAR_V0

typedef struct {
	uint16_t	err, _res;
	float	var;
} QualVariation;

typedef struct {
	// Event
	uint16_t	evtCount[12];	// 10분 동안 발생한 sag, swell Count, interruption
	// Freq
	QualVariation	Freq1;
	QualVariation	Freq2;
	
	QualVariation	Volt1[3];
	QualVariation	Volt2[3];
	
	QualVariation	Voltbal;
	QualVariation	VoltThd[3];
	QualVariation	VoltHd[3];
	QualVariation	Pst[3];
	QualVariation	Plt[3];
	QualVariation	Svolt[3];	
	// U
	//uint16_t 	Uerr[2][3], UbalErr, UthdErr[3], UhdErr[3], PstErr[3], PltErr[3], SvoltErr[3];	
	//float 	  UVar[2][3], UbalVar, UthdVar[3], UhdVar[3], PstVar[3], PltVar[3], SvoltVar[3];	
} QualVarData;

#else

typedef struct {
	uint32_t	err;
	float	var;
} QualVariation;

typedef struct {
	// Event
	uint32_t sag[4];
	uint32_t swell[4];
	uint32_t shortIntr[4];
	uint32_t longIntr[4];
	uint32_t rvc[4];
	
	// Freq
	QualVariation	Freq1;
	QualVariation	Freq2;
	
	QualVariation	Volt1[3];
	QualVariation	Volt2[3];	
	
	QualVariation	Voltbal;
	
	QualVariation	VoltThd[3];
	QualVariation	VoltHd[3];
	QualVariation	Pst[3];
	QualVariation	Plt[3];
	QualVariation	Svolt[3];	
} QualVarData;

#endif

typedef struct {
	float	freq, U[3], Ubal[2]; 	// Uthd[3], Uhd[3][24];	// 2 ~ 25
	float Pst[3], Plt[3], Svolt[3];
	//
	float	I[3], In, Ibal[2];
	float temp[5];
	// 
	float	PF, dPF;
	//
	float	P[2], Q[2], S;
	//int initEgy, accTime;
	//ENERGY_REG64 ereg;
} QualSumData;


// 10분단위 평균
typedef struct {
	uint32_t startTs, endTs;	// 생성시간	
	uint32_t count10s;	// 주파수데이터 수 
#ifdef _QUAL_TEST
	uint16_t ts10[6];		// 10s 단위 sample data, -1 no data
	float Freq[6];	
#else	
	uint16_t ts10[60];	// 10s 단위 sample data, -1 no data
	float Freq[60];	
#endif	
	float U[3], Uthd[3], Ubal[2], Uhd[3][24];	// 10분 평균 데이터
	float Pst[3], Plt[3], Svolt[3];
	// 10분간 발생한 이벤트 추가(19.5.13)
	uint16_t	sag[4], swell[4], shortIntr[4], longIntr[4], rvc[4];
} QualAvgData;


typedef struct {
	float I[3], In, Ibal[2];	// 10분 평균 데이터
	float temp[5];
	float PF, dPF;
	float P[2], Q[2], S;
	//
	ENERGY_REG64 ereg;
} QualAvgExpData;


typedef struct {
	float	I[3], In, Ithd[3], Itdd[3], Ibal[2], kf[3];
} LogData;

// 10분 data
typedef struct {
	uint32_t ts10m, ts10s;	// 비교용 타이머 
	uint32_t count10m, count10s;		// 10분 평균 위한 데이터 카운트 
	QualAvgData avg;	// 10분 평균
	QualAvgExpData avgExp;
	QualVarData var;	// 10분 단위 varariation  갱신
	// 평균값
} QualData10m;

#ifdef VQ_VAR_V0

typedef struct {
	uint32_t startTs, endTs;	
	uint16_t year, woY, complete, compliance;
	//uint32_t bIx;		// bCount: 10분 데이터 카운트
	uint32_t count10s, count10m;	// var계산 위한 10s, 10m 카운트  
	QualVarData var;
} QualWeek;

#else

// size: 296 -> 300
typedef struct {
	uint32_t startTs, endTs;	
	uint32_t complete, compliance;
	uint32_t count10s, count10m;	// var계산 위한 10s, 10m 카운트  
	QualVarData var;
	uint32_t evtCount;	
} QualWeek;

#endif

// 주파수1: +1 ~ -1%, 99.5%/W
// 주파수2: +4 ~ -6%, 100%/W
// 전압1: +10% ~ -10%, 95%/W
// 전압2: +10% ~ -15%, 100%/W
// 불평형률: 2%, 95%
// THD: 8%, 95%/W
// Harmonics: 5th:6%, 7th:5%, 11th:3.5%, 13th:3%, 95%

#define	N_QUAL_WEEK	1008

typedef struct {
	char	qlfn[64], qwfn[64], qwfnLast[64];
	QualSumData		qsum;
	QualData10m		q10m;
	QualWeek			qw;
	int	tsChanged;
} QualLogData;

typedef struct {
		int16_t full, ix, st;
		float buf[120], sum, mean;
} RVC_DEF;


// alarm
typedef struct {
	uint8_t timer;
	uint32_t startTs, endTs;
	uint16_t chan, cond;
	float level;
} LOC_ALM;

typedef struct {	
	uint64_t Ts;
	uint16_t eType, mask;
} PQ_EVT_Q;

typedef struct {
	uint16_t fr, re;
	PQ_EVT_Q	Q[8];
} PQ_EVT_CAP_Q;

typedef struct {
	uint64_t startTs, endTs;
	uint16_t mask, type;
	uint32_t Val[3];
} PQ_EVENT_INFO;


typedef struct {
	PQ_EVT_CAP_Q wQ, rQ;
	PQ_EVENT_INFO	inf[4];	// OC, SAG, SWELL, NTR
} PQ_EVENT;


typedef struct {
	uint8_t  enable, errCnt;
	uint16_t count, limit;
} WDT_DATA;


typedef struct {
	int count;
	uint32_t ts10m;
	
	float Ucfsum[3], Uppcfsum[3],  Icfsum[3];
	float Uthdsum[3], Uppthdsum[3], Ithdsum[3], Itddsum[3], Ikfsum[3];
	uint16_t Uhdsum[3][24], Upphdsum[3][24], Ihdsum[3][25];  
	
	float	Ucf[3], Uppcf[3], Icf[3];
	float Uthd[3], Uppthd[3], Ithd[3], Itdd[3], Ikf[3];
	uint16_t	Uhd[3][24], Upphd[3][24], Ihd[3][24];
} HarmonicsData;

typedef struct {
	// 64bit
	//uint64_t swellTimer;			
	//uint64_t elapT[48];			
	struct tm tod, temptm;
	uint16_t woY;	// week of ther year
	uint64_t blTimer;	// backlight timer	
	//uint64_t r64[6];
	// 3P3W에서 phase ordering이 다른다 
	float	U[3], U_1012[3], I[3], In, Is, Itot; // I1, I2, I3, In, Isum, I-total
	float	fU[3], fI[3];
	// Power
	float	P[4], Q[4], S[4];
	float	fP[4], fQ[4], fS[4];	// fundamental Power
	
	//
	uint32_t	irqstat0, irqstat1;
	uint32_t	irqmask0, irqmask1;
	//
	uint16_t online;	// 
	uint16_t online2;	// alarmProc에서 사용
	uint16_t online3;	// maxMin 함수에서 사용
	//
	uint16_t vFS, zxCnt, zxCntLast;
	uint64_t zxMonCnt;
	// sag, swell
	uint16_t sagSt, sagMask, sagEn, sagCyc, sagRetMask;
	uint16_t swellSt, swellMask, swellEn, swellCyc, swellRetMask;
	uint16_t ocSt, ocMask, ocEn, ocCyc, ocRetMask;	// 2020-3-12	
	uint32_t sagLevel, swellLevel, sagRetLevel, swellRetLevel, ocLevel, ocRetLevel;	
	
	// intr 
	uint16_t intrSt, intrMask, intrEn;
	uint32_t intrLevel;
	uint64_t zxTs;
	
	PQ_EVENT pqe;
	uint16_t sagHoldOff, swlHoldOff, ocHoldOff;
	//
	// control data
	// 
	//int 	meterStart;
	//int		viSumF, viSumIx, viSumCnt;
	uint16_t sumCnt, sumMax, nFastRMS;	// RMS 샘플수
	uint16_t pwrSumF, pwrSumIx, pwrSumCnt, reserved0;
	float	Usum[3];
	float	Isum, Insum, Issum;
	float	Tempbuf;		
	//int		tempSumF, tempSumIx, tempSumCnt;		
	// 누적(4), 현월(4)
	//double kWh[8], kvarh[8], kVAh[2], FkWh[2], Fkvarh[2];			
	float	vscale, iscale, wscale, alphaConst, wh_scale;	
	float	wv_vscale, wv_iscale, wv_vreverse, wv_ireverse;	
	uint32_t pthreshold;
	uint16_t pga_vgain, pga_igain, pga_ingain;
	// add pulse config
	uint16_t	CFxDEN;
	//
	float Uangle[3], Iangle[3];	// xAB, xBC, xAC
	float UIangle[3];	// Va-Ia, Vb-Ib, Vc-Ic
#ifdef _ANGLE_AVERAGE	
	float UangleSum[3][2], UIangleSum[3][2], IangleSum[3][2];
#endif
	//
	float freq[3];
	int periodSum[3];	
	//
	// demand
	//
	uint16_t dInterval, dInterval15m;		// 설정치 
	uint16_t mInterval, mInterval15m;
	uint32_t dmdTs, dmdStartTs, dmdEndTs;
	float dmdI[3];
	float dmdlastkwh;
	float dmdP[2], dmdQ[2], dmdS[2];	// 구간시작 시점 전력량(start, stop)
	float	dtP, dtQ, dtS;		// 변화율
	//
	// demand 15m, 화면표시용 
	//
	uint32_t dmdTs15m, dmdEndTs15m, dmdStartTs15m, dmdTs1H;
	float	dmdP15m[2];
	//
	//float	kWh[2], kvarh[2], kVAh[2], FkWh[2], Fkvarh[2];			
	
	// min, max
	//MAXMIN	maxmin;	
	// calibration
	uint16_t mmChangeF, calEn, calDcOs;
	float	vref, iref, inref;
	
	// alarm
	uint16_t tmrEn[48];
	uint16_t almStat[48];	
	//TIME_STAMP almTs[48];
	
	// fft, wave sampling
//	uint16_t wfr, wre;
//	float	vrms[3], irms[4];	// real-time RMS
//	float	thd[7];	
	// 
	uint32_t et1, et2, etmax;
	
	// reqComSetting
	//uint16_t	commCmd[5];
	uint16_t rstEgy, rstDemand, rstMaxMin, rstAlmList, rstEvtList, rstIticList;
	uint16_t factReset, saveSetting, rebootFlag, runFlag, wdtEn;	// 공장초기화 후 재부팅, 저장 후 재부팅, 강제 재부팅
	uint16_t daqEnable;
	//
	// eepTask
	//
	uint16_t w_energy, w_demand;
	uint16_t w_minmax, w_alarm, w_event, w_settings;
	uint16_t w_almsetting, w_ethsetting;
	uint16_t w_iomsetting, w_pulseinput;
	// 16-11-23 : reboot req	
	uint16_t w_reboot;
	
//	void   *ptod;
//	void   *psettings;
//	void	 *palmsettings;
//	void   *pethsettings;
//	void   *piomsettings;
//	void   *ppulse;
	

	// io module
	uint16_t ctrlFlag, cmId, cpoint, ccmd;
	uint16_t almMod, almPoint;
	// pi point
	uint16_t piInitF[2];
	uint16_t piLastData[2][8];
	float piReg[2][8];
	uint32_t piAccm[2][8];		
	//uint8_t upConfig[4], retry[4], soeFlag[3], soeFr[3];		
	//uint16_t fr, re, count, seq;
	//SOE_DATA soe[15];
	//
	uint16_t r0;
	float	inoload;
	uint16_t vrmsCmpCnt[3];
	
	// 
	uint16_t rmsCalcF, pwrCalcF, thdCalcF;
	uint16_t rmsInit, energyInit, phMode, phDelay; 
	uint16_t vrmsError;
	// 2017-11-22 : sag-swell event 용
	uint16_t evtMod, evtPoint;
	uint64_t startSagTick, lastSagTick;
	uint64_t lastSwellTick[3], startSwellTick;
	uint16_t swellCnt[3];
	//
	uint16_t revPhase;
	ENERGY_FREG	Ereg;	
	float vload[3], iload[3];	
	// ADE900 Calibration
	int32_t wh[3], varh[3], vah[3];
	uint8_t	wCalF[8];	// 0: Global, 1:U, 2: I, 3: Phase, 4: Power, 5: In

	// RVC
	RVC_DEF rvc[3];
	int rvcCount;
	float rvcThres[2], rvcHyst[2];
	// Transient
	int TrVTrg, TrCTrg;
	//
	float	freqLo[2], freqHi[2];
	float	uLo[2], uHi[2];
	
	// alarm
	uint8_t almTimer[32];
	ALARM_LOG	alog;
	//char	pqpath[64], path2[64] ;
	//char	qualWeekFN[64], qualLogFN[64];
	WAVEFORM_GUI	wvGUI;	// LCD에서 waveform 표시위해 그린다
	
	// 최대 32 task
	WDT_DATA	wdtTbl[32];
	//
	uint16_t	trendChanged[4];
	//
	// Freq Calculation
	//
	int		freqSel;
	float freq1sum;
	int 	freqCount1s;
	//
	HarmonicsData hmd;
	// 시간대별 에너지 로그 생성
	uint32_t egyTs1H, egyTs1D;
	uint32_t egyStartTs1H, egyStartTs1D;
	uint32_t egyBuf[4];
	// 
	uint32_t maxminTs;
	//
	uint16_t battVolt, battMode;
	//
	float	Ig, igscale;
	float	I_start;
	float	In1;
	float	In_offset[2];					// y axis : 입력 전류시 In offset
	float	In_offset_I[2];					// x axis : 평균 전류 크기
	float	In_Slope;						// 기울기 
} CNTL_DATA; 


typedef struct {
	int32_t		vgain[2][3];		// phase-to-neutral gain 튜닝, x1
	int32_t		vppgain[2][3];	// phase-to-phase gain 튜닝, x1
	int32_t 	igain[2][3];		
	int32_t		vgainx2[2][3];	// phase-to-neutral gain 튜닝, x2
	int32_t		Ingain[2];

	int32_t		wgain[2][3];
	int32_t		vppgainx2[2][3];// phase-to-phase gain 튜닝, x2
	int32_t		phcal[2][3];
	
	int32_t		fwgain[2][3];
	int32_t		fwattos[2][3];	
	int32_t		fvargain[2][3];
	int32_t		fvaros[1][3];
	int32_t		fvaros2[1];
	float		v_thd_offset;
	float		i_thd_offset;
	float		In_offset[2];			// offset 
	float		In_Slope[2];			// 기울기
	int32_t		vdcos[2][3], idcos[2][3];	// waveform에서 DC offset 제거위해 사용한다 
	
	uint32_t 	hwModel;	// 0: 5A, 1:100mA, 2:10mA, 3:Rogowski
	uint32_t 	hwVer;	// hw version
	uint32_t 	fwVer;	// fw version
	uint8_t 	mac[4];
	uint32_t 	sn[2];
	uint16_t 	magic, crc;
} METER_CAL;


// 4400
typedef struct {
	int16_t header[2];
	int16_t sample[798];
} SAMPLE_BUF;


// 주요 metering 요소에 대해 10분 평균 제공한다 
typedef struct {
	uint32_t ts;
	float	U[3];
	float Uthd[3], Uppthd[3];
	float Ubal[2];
	float I[3], In;
	float Ithd[3], Itdd[3], kf[3];
	float Ibal[2];
	float kw[2], kvar[2], kVA;
	float PF, dPF;
	float temp[5];
	uint16_t Uhd[3][24];
	uint16_t Upphd[3][24];
	uint16_t Ihd[3][24];
	uint16_t _r[6];
} LOG_DATA;

// 금일, 전일 에너지 로그
typedef struct {
	uint32_t ts;		
	struct {
		uint32_t kwh, kvarh[2], kVAh;
	} egy[24];
	uint16_t _r[4], magic, crc;
} ENERGY_LOG;

typedef struct {
	uint32_t ts;
	float	DP_P_Log[96];
	uint16_t _r[6];
} DEMAND_LOG;	


typedef struct {
	uint16_t status[2];
	uint16_t _r[98];
} GATEWAY_STATUS;


// 2018-9-3 이후 버전
typedef struct {	
	uint16_t type, dbFlag, subType, r2;
	uint16_t diStatus[8];	
	uint32_t piData[8];		// 32bit 데이터
	uint16_t doStatus[8];	// 6개만 사용, 2개는 예비
	uint16_t r0[8];				// do Count  증설
	float    aiData[4];
	uint16_t aoData[2];
	uint16_t r1[6];				// do Count  증설
} IOM_DATA;	

// g7000 mem
typedef struct {
	uint16_t modType[4];		// 장착된 module Type(1:Eth, 2:DIO, 3: DIO2)
	uint16_t modStauts[4];	// 장착된 module 통신상태
	uint16_t dbStatus[4];		// db download 상태
	uint16_t modSubType[4];	// 2018-4-17 추가
	uint16_t r0[4];
	IOM_DATA io[2];
} EXT_IO_DATA;


// IO_MODULE Memory Map
typedef struct {
	uint16_t diStatus[8];
	uint16_t piData[8];		// 
	uint16_t doStatus[8];
	uint16_t r0[8];	
	float    aiData[4];
	uint16_t aoData[8];
	uint16_t type, dbFlag, subType, r1;
} IOS_DATA;	

typedef struct {
	uint64_t ts;
	uint16_t status;
	uint16_t index;
	uint16_t r0[2];
} SOE_DATA;

typedef struct {
	IOS_DATA io;	
	uint16_t count, r0[3];
	SOE_DATA sd[16];
	IO_CFG	 cfg;
} IOM_MEM_DEF;

#define	STS_ERROR	1
#define	STS_OK		0


typedef struct {
	uint16_t sn[6];
	uint16_t mac[4];
	uint16_t hwModel;				// 0: 5A, 1:100mA, 2:10mA, 3:Rogowski
	uint16_t hwVer;
	uint16_t fwVer;
	uint16_t fwBuildYear;
	uint16_t fwBuildMon;
	uint16_t fwBuildDay;
	uint16_t mBusRxCnt;
	uint16_t Alarm_sts;
	uint16_t Event_sts;
//	uint16_t r1[41];
	uint16_t RSTP_sts[2];
	uint16_t mac_msb[2];
	uint16_t ipAddr[4];		// dhcp or static ip
	uint16_t SNTP_sts;
	uint16_t DEV_sts;
	uint16_t HeartBit;
	uint16_t iPSM_sts[2];
	uint16_t iPSMDI_sts[2];
	uint16_t Io_sts;
	uint16_t NET_sts;
	uint16_t TOT_sts;
	uint16_t r1[23];
} METER_INFO;


#define FS_MSG_CNT	4

typedef struct {
	char fname[64];
	char mode[4];		// "w", "wb"
	int	 argv;
	void *pbuf;			// 디스크에 쓸 buf 시작번지
	int	size;
} FS_MSG;

typedef struct {
	int fr, re;
	OsTaskId tid;
	FS_MSG	mQ[FS_MSG_CNT];
} FS_Q;


typedef struct {
	uint16_t cmd[50];
} COMMANDS;



#define	N_BRANCH	54


typedef struct {
	// 8
	uint16_t		hbeat;
	int16_t			temp;
	uint16_t		freq;
	uint16_t		ver;
	uint16_t		co2;
	uint16_t		r1[3];
	// 8
	uint16_t		int_sts;
	uint16_t		feeder_sts[55];
} AI_STATUS;

typedef struct {
	uint32_t		Vn_average;		
	uint32_t		Vl_average;
	uint16_t		Vn_unbal;
	uint16_t		Vl_unbal;
} TOTAL_VOLT_DATA;

typedef struct {
	uint32_t		Vn;
	uint32_t		Vl;
	uint16_t		Vn_unbal;
	uint16_t		Vl_unbal;
} PHASE_VOLT_DATA;

typedef struct {	
	TOTAL_VOLT_DATA		tot;
	PHASE_VOLT_DATA		phase[3];
	uint16_t 	r0[4];
	uint16_t  sag[6], swell[6];
	uint16_t 	r1[16];
} AI_VOLT;

typedef struct {
	uint16_t		type;
	uint16_t		Ileak;
	uint32_t		In;
	int32_t			Pwatt;
	int32_t			Qvar;
	uint32_t		Sva;
	int16_t			pf;
	uint16_t		r1;
	uint16_t		Iub;
	uint16_t		thd;
	uint16_t		reserved[2];			
} COMM_FEEDER_DATA;


typedef struct {
	uint32_t		Vn;
	uint32_t		In;
	int32_t			Pwatt;
	int32_t			Qvar;
	uint32_t		Sva;
	uint16_t		Vub;
	uint16_t		Iub;
	uint16_t		ph;
	int16_t			pf;
	uint16_t		thd;
	uint16_t		r1;
} PHASE_FEEDER_DATA;


typedef struct {
	uint32_t		In;
	int32_t			Pwatt;
	uint16_t		ph;
	int16_t			pf;
	uint16_t		thd;
	uint16_t		r1;
} PHASE_FEEDER_DATA_SM;

typedef struct {
	COMM_FEEDER_DATA	comm;
	PHASE_FEEDER_DATA	phase[3];
} AI_FEEDER;

typedef struct {
	COMM_FEEDER_DATA	comm;
	PHASE_FEEDER_DATA_SM	phase[3];
} AI_FEEDER_SM;

typedef struct {
	uint32_t		Vn;
	uint32_t		Vl;
	uint32_t		In;
	uint32_t		Pwatt;
	uint32_t		Qvar;
	uint32_t		Sva;
	uint16_t		Vub;
	uint16_t		Iub;
	uint16_t		ph;
	int16_t			pf;
	uint16_t		thd;
	uint16_t		resered;		
} PHASE_INPUT_DATA;

typedef struct {
	uint32_t		In;
	uint32_t		Pwatt;
	uint32_t		Qvar;
	uint32_t		Sva;
	int16_t			pf;
	uint16_t		Vub;
	uint16_t		Iub;
	uint16_t		thd;
	uint16_t		Ileak;
	uint16_t		reserved[13];		
} COMM_INPUT_DATA;


typedef	struct {
	uint16_t		feederCount;
	uint16_t		freq;
	uint16_t		vType;
	uint16_t		r1[4];			
	uint16_t		pf_sign;
	uint16_t		va_type;
	uint16_t		r4[5];			
	uint16_t 		protocol;
	uint16_t		mbusId;
	uint16_t		ip[2];
	uint16_t		gw[2];
	uint16_t		sm[2];
	uint16_t		port;
	uint16_t		mac[2];
	uint16_t		comm485_enable;
	uint16_t		comm485_baud;
	uint16_t		r2[5];

	uint32_t		ai_alarm_level;
	uint32_t		deadband;
	uint32_t		pt_1st;
	uint32_t		pt_2nd;
	uint32_t		sag_level;
	uint32_t		swell_level;
	uint16_t		sag_period;
	uint16_t		swell_period;
	uint16_t		r3[6];
	
	uint16_t		demand_time;
	uint16_t		demand_level1;
	uint16_t		demand_level2;
	uint16_t		demand_level3;
} SET_CONFIG;

typedef struct {
	uint16_t		type;
	uint16_t		ct_1st;
	uint16_t		ct_2nd;
	uint16_t		ct_turn;
	uint16_t		ch[3];
	uint16_t		st_current;
	uint32_t		ocr_level;
	uint32_t		demand_target;
	uint16_t		demand_alarm_use;
	uint16_t		demand_do_id;
	uint16_t		demand_do_pnt;
	uint16_t		r1[5];
} SET_FEEDER;

typedef struct {
	uint32_t		sum_kwh;
	uint32_t		nowM_kwh;
	uint32_t		lastM_kwh;

	uint32_t		sum_kvarh;
	uint32_t		nowM_kvarh;
	uint32_t		lastM_kvarh;

	uint32_t		sum_kvah;
	uint32_t		nowM_kvah;
	uint32_t		lastM_kvah;
}WH_FEEDER;

typedef struct {
	uint16_t	reserved;
	uint16_t	year;
 	uint16_t	month;
	uint16_t	day;
	uint16_t	wday;
	uint16_t	hour;
	uint16_t	minute;
	uint16_t	second;	
} NT_DATETIME;


typedef struct {
	// input register
	AI_STATUS    ai_sts;	// 0
	AI_VOLT			 ai_volt;	// 64
	AI_FEEDER_SM ai_feeder[N_BRANCH]; // 120 ~ 2279
	AI_FEEDER_SM ai_incoming; // 2280 ~ 2319
	uint16_t 		r0[80];	// 2320 ~ 2399
} MGEM_SMALL_DATA;

typedef struct {
	struct {
		uint32_t dmdI;
		uint32_t dmdMaxI;
		uint32_t dmdW;
		uint32_t dmdMaxW;
	} phase[3];
	struct {
		uint32_t dmdTotI;
		uint32_t dmdTotMaxI;
		uint32_t dmdTotW;
		uint32_t dmdTotMaxW;
		uint32_t dmdTotPW;
	} total;	
} DEMAND_DATA;

typedef struct {
	// input register
	uint16_t 		r3[20];		// 2400 ~ 2419
	AI_FEEDER	  ai_feeder[N_BRANCH]; // 2420 ~ 5875
	AI_FEEDER	  ai_incoming; // 5876 ~ 5939
	uint16_t 		r0[60];	// 5940 ~ 5999
	DEMAND_DATA dmd[N_BRANCH];// 6000 ~ 7835
	uint16_t 		r1[164];	// 7836 ~ 7999
	WH_FEEDER		wh[N_BRANCH];	// 8000 ~ 8971
	WH_FEEDER		wh_incoming;	// 8972 ~ 8989
	uint16_t    r2[10];				// 8990 ~ 8999
} MGEM_FULL_DATA;

typedef struct {
	// holding register
	NT_DATETIME	date;		// 0
	uint16_t		r0[112];
	uint16_t		demand_reset;	// 120
	uint16_t		db_write_enable;	// 121
	SET_CONFIG	set_cfg;	// 122
	uint16_t 		r1[62]; // 178 ~ 239
	SET_FEEDER	set_feeder[N_BRANCH];	// 240 ~
	uint16_t    r2[80];
} MGEM_SETTING;

// sizeof(MGEM_DATA) = 10,400
typedef struct {
	MGEM_SMALL_DATA sm;	// small meter (0 ~ 2300)
	MGEM_FULL_DATA  fm;	// full meter+WH (2400 ~ )
	MGEM_SETTING    db;
} MGEM_DATA;

// Feeder Data
typedef struct {
	uint16_t type;
	uint16_t status;
	float	U, I, Iz;
	float	P, Q, S, PF;
	float	THD, Uub, Iub;
	uint32_t kwh[3];
	uint32_t _r[2];
} FEEDER_DATA;


typedef struct {
	uint16_t	nfeeder;
	FEEDER_DATA	feeder[54];
} MGEM3500_DATA;

typedef struct {
	uint32_t 	eh;			// kWH, kVarH, kVAh(import, export)
	float		P;	
	float 		U[3];	
	float		I[3], Ig; 	// In: Neutal Current(measured), Isum: Neutral Current(Calculated)
	float 		THD_U[3];
	float 		THD_I[3], TDD_I[3];
	uint16_t 	_r[10];
} CH_DATA;

typedef struct {
	CH_DATA	    ch[2];
	float		temp;
	uint16_t 	_r[106];
} SIMPLE_DATA;


//typedef struct {
//	// input register
//	AI_STATUS    ai_sts;	// 0
//	AI_VOLT			 ai_volt;	// 64
//	AI_FEEDER_SM ai_feeder[N_BRANCH]; // 120 ~
//	uint16_t 		r0[20];	// 2280 ~ 2299
//} MGEM_SMALL_DATA;

// 2016-8-24


#define	RING_SIZE					256

typedef struct {
	int	fr;
	int	re;
	struct {
		uint8_t reqId;
		uint8_t devId;
	} buffer[RING_SIZE];
} RING_BUF2;

typedef struct {
	METERING	meter;
	ENERGY	egy;
	DEMAND	dm;	
	VQDATA  vq;
	EN50160	rpt[2];
	HARMONICS hd;
	INTERHARMONICS interhd;
	WAVEFORM_L16 wv;
	MAXMIN	maxmin;	
	ALARM_STATUS alarm;
	ALARM_LIST alist;	// 4200
	EVENT_LIST elist;	// 4500
	PQ_EVENT_COUNT pqEvtCnt;	// 4890
	LOG_DATA	 log;			// 4900
	ENERGY_LOG	elog[2];// 5200
	DEMAND_LOG	dlog;		// 5600 - last da demand log
	//SAMPLE_BUF sample;	// 5800	- reserved area
	MAXMIN	lastmaxmin;	// 5800
	GATEWAY_STATUS gwst;// 6000
	EXT_IO_DATA iom;		// 6100
	METER_INFO info;		// 6240
	SETTINGS	setting;	// 6300
//	uint16_t 	_r[10];

	COMMANDS	cmds;			// 6950
//	FLOW_DATA	flowMeter;
//
#if 1	// 2025-3-12, ITIC -> reseved
	ITIC_EVT_LIST	itic;	// 7400
	ITIC_EVT_LIST	itic2;// 8000
//	uint16_t			resv[1400];
#endif
	EVENT_FIFO 	eventFifo;
	ALARM_FIFO	alarmFifo;	

	CNTL_DATA cntl;
	METER_CAL	cal;
	QualLogData	qdLog;
	//ENERGY_NVRAM egyNvr;
	SETTINGS	db;	// loadSettings을 통해 읽는다 
	float		almCnt;
} METER_DEF;


typedef struct {
	uint32_t ts;
	uint16_t type, valid;	// type : 0
	float pen[16];
} TREND_RECORD;

// 파일이 처음 만들어 지거나 중간에 Trend 정보가 변경되면 기록한다 
typedef struct {
	uint32_t ts;
	uint16_t type, version;	//
	uint16_t channel[16];
	uint16_t _r[16];
} TREND_INFO;


//typedef struct {
//	uint16_t mag[64];
//} HD_DATA;

//typedef struct {
//	HD_DATA	vhd[3];
//	HD_DATA	ihd[3];
//	// even, odd, zero, pos, neg THDs
//	uint16_t vTHDs[3][5];
//	uint16_t iTHDs[3][5];
//	uint16_t cF[3];	// crest factor
//	uint16_t kF[3];	// K factor
//	uint16_t reserved[80];
//} HARMONICS;




//typedef struct {
//	METERING	meter;
//	MINMAX		history;
//	HARMONICS	hd;
////	DEMAND		demand;
////	ALARM_LOG	alog;
////	WV16_BUF	wv;
////	ALARM_LOG	evlog;
////	EXT_IO_DATA	 iodata;
////	EXT_IO_SOE soe;	
//	SETTINGS	setting;	
////	ALARM_SET	aset;	
////	CMD_AREA	cmd;	
////	EXT_MOD_CFG	 iocfg;	// 4050	
////	uint16_t  r0[90];		// 90
//	CNTL_DATA	cntl;			// 4400
////	//uint16_t  r1[80];	
////	UTIL_AREA	util;			// 5800
//} METER_DEF;


#define	N_CMD_Q	8

typedef struct {
	int fr, re;
	uint32_t tid;
	struct {
		uint16_t id, s, c;
	} item[N_CMD_Q];
} CMD_Q;

typedef struct {
	char keystr[8];	// 0: "*netinf*
	uint8_t	ip[4];	// 8:
	uint8_t mac[6];	// 12:
	char host[32];	// 18:
	uint8_t sn[6];	// 50:
	uint8_t hwModel, hwVer;
	uint8_t fwVer, fwDate[3], _r[2];
} RESP_NETINF;

#define	Tid_Sntp		0
#define Tid_Shell		1 
#define	Tid_FFT			2 
#define	Tid_Wave		3 
#define	Tid_Rmslog		4
#define	Tid_PostScan	5 
#define Tid_Energy		6
#define	Tid_Meter		7
#define	Tid_Meter2		8
#define Tid_Fs 			9
#define Tid_Trend		10
#define Tid_Iom			11
#define	Tid_SMB			12
#define	Tid_NET			13
#define	Tid_Led			14
#define	Tid_GW			15

#define PQ
#define _DM_


//#define _EGY_LOG_ 
//#define EGY_LOG

extern const float _pi;
extern void getFloatStr(float v, char *pbuf);
extern void getDoubleStr(double v, char *pbuf);

extern SIMPLE_DATA	*psmap;

extern METER_DEF	meter[];
extern ENERGY_NVRAM egyNvr;

extern MAXMIN 	*pmm;
extern METER_CAL	*pcal;
extern SETTINGS		db[];
extern SETTINGS		setting[];
extern METERING 	*pmeter;
extern SETTINGS		*pdb, *pdbk, *pdb2, *pdbk2;
extern METER_INFO *pInfo;
//extern CMD_AREA 	*pcmd;
//extern UTIL_AREA	*putil;
extern HARMONICS  *pHD;
//extern WV16_DATA	*pwv;
//extern WV16_BUF	  *pwbuf;
extern DEMAND			*pdm;
extern DEMAND_LOG	*pdmlog;

extern EVENT_FIFO	*pEvtFifo;
extern ALARM_FIFO *pAlmFifo;

extern VQDATA     *pVQ;
extern WAVEFORM_L16	*pWFL16;
//extern ENERGY_NVRAM egyNvr;
extern ALARM_STATUS *palm;
extern ALARM_LIST	*palist;
extern EVENT_LIST	*pelist;
extern ITIC_EVT_LIST *piticlist;
extern ITIC_EVT_LIST *piticlist2;
extern PQ_EVENT_COUNT *ppqEvtCnt;
//extern ALARM_LOG  *palog;
//extern ALARM_LOG  *pevt;
//extern ALARM_SET  *paset;
//extern FRAM_ENERGY1 *pfEnergy1;
//extern FRAM_ENERGY2 *pfEnergy2;
//extern FRAM_DEMAND	*pfDemand;
extern EXT_IO_DATA *piom;
//extern EXT_IO_SOE  *psoe;
extern GATEWAY_STATUS	*pgwst;
extern COMM_CFG *pComm;
extern IO_CFG	*piocfg;
//extern EXT_MOD_CFG *piocfg, *piobk;
//extern METER_DEF	meter;
extern ENERGY *pEgy;
extern CNTL_DATA	*pcntl;
extern EN50160 *pRPT;
extern LOG_DATA	*pld;


extern WAVE_8K_BUF wbFFT8k[2];
extern WAVE_PGBUF	wQ[2];
//extern WAVE8k_PGBUF	 w8kQ;
extern WAVE_32K_BUF wbCap32k[];
//extern WAVE32k_PGBUF w32kQ;
extern QualLogData	*pqLog;
//extern VarDataSet *pVds;
extern SAMPLE_BUF *pSp;
extern FAST_RMS_BUF rmsWin;

extern DAQ_BUF	daq;

//extern ITIC_EVENT_BUF *pitic;

extern void copySummary(void);

extern int loadSettings(SETTINGS	*pdb);
extern int saveSettings(SETTINGS	*pdb);
extern void initW8kFB();
extern void initWv1024Buf();
extern int loadHwSettings(METER_CAL *pcal);
extern void storeHwSettings(METER_CAL *pcal);
extern int pushEvent(int id, PQ_EVENT_INFO *pInf);
extern void clearEventList();
extern void updateEventCount(int mask, uint32_t *ec);

//extern void tickSet(uint32_t time, int mode);
extern void initCmdQ(uint32_t tid);
extern void _enableTaskMonitor(int id, int limit);
extern void setMeterInfo(void);
extern int loadMaxMin(void);
extern void copyEreg64(ENERGY_REG64 *, ENERGY_REG64 *);
extern void clearIticFile();
extern uint32_t sysTick32, sysTick1s, sysTick10s, sysTick10m, sysTick15m, sysTickDemand, WM_tick32;
extern uint64_t sysTick64;

extern void fetchEvent(int);
extern void fetchAlarm(int);
extern void fetchItic(int);
extern void fetchItic2(int);
// 2025-3-25
extern void setMeterIpAddr(uint32_t);
extern void getMeterIpAddr(uint8_t *);

#endif
