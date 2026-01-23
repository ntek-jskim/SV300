#ifndef _ADE9000_H

#define	_ADE9000_H

#include "board.h"
#include "meter.h"

#define MAX_METER       1

#define MAX_CHAN        (MAX_METER*3)


#define	GAIN_MAX			(134217728)	// power(2,27)

//
// IRQ Status & Mask Bit
// status 0

#define	PI							3.14159265358979323846
#define	SQRT_3					1.732051

// scale factor
//#define	V_FULL					(792.6667017)
//#define	V_NORM					(380.)	 				// 380V로 튜닝
#define	IZ_NORM					(0.03)					// 30mA로 튜닝

// TYPE Low Voltage
#define	V_FULL_LV				(1584.63/2)
#define V_NORM_LV				(380.0)

// TYPE High Voltage
#define	V_FULL_HV				V_FULL_LV		//(198.4318405)
#define V_NORM_HV				(100.0)

// _TYPE_6A
#define	A_FULL_6A				(7.085238288)	  // 2000:1 for NJ3CT
#define	A_FULL_6A_INV		(7.099465675)	  // 1000:1 for inverter
#define	I_NORM_6A				(3.)					  // 3A로 튜닝
#define	THR_VAL_6A			(171870521)		  // Vfs, Ifs 변경시 같이 계산할것 for NJ3CT
#define	THR_VAL_6A_INV	(171526091)		  // Vfs, Ifs 변경시 같이 계산할것 for inverter
#define THR_VAL_HV_6A		(686563401)
#define PH_NORM_6A		  (570.0)			    // P=VIcos(theta), 전압/전류 위상을 60도 차이나게 하여 calibration 할 것이므로 P=V_NORM(상전압)*I_NORM(상전류)*cos(60) = 570.0
#define PH_NORM_HV_6A   (150.)
#define	ALPHA_CONST_6A	0.001			    // 10E-3

// _TYPE_30A
#define	A_FULL_30A			(35.35533906)	  // 2000:1 
#define	A_FULL_30A_INV	0
#define	I_NORM_30A			(12.)					  // 12A로 튜닝
#define	THR_VAL_30A			(34442990)		  // Vfs, Ifs 변경시 같이 계산할것
#define	THR_VAL_30A_INV	0
#define THR_VAL_HV_30A  0
#define PH_NORM_30A   	(2280.0)			  // P=VIcos(theta), 전압/전류 위상을 60도 차이나게 하여 calibration 할 것이므로 P=V_NORM(상전압)*I_NORM(상전류)*cos(60) = 2280.0
#define PH_NORM_HV_30A	0
#define	ALPHA_CONST_30A	0.001			      // 10E-3

// _TYPE_60A
#define	A_FULL_60A			(70.85238288)	  // 2000:1 
#define	A_FULL_60A_INV	0	
#define	I_NORM_60A			(30.)					  // 30A로 튜닝
#define	THR_VAL_60A			(171870521)		  // Vfs, Ifs 변경시 같이 계산할것
#define	THR_VAL_60A_INV	0
#define THR_VAL_HV_60A  0
#define PH_NORM_60A   	(5700.0)			  // P=VIcos(theta),
#define PH_NORM_HV_60A  0
#define	ALPHA_CONST_60A	0.01			      // 10E-2, alpha const를 0.001로 하면 THR_VALUE가 minimum값인 33,516,139를 초과하므로 0.01로 설정

// _TYPE_100A
#define	A_FULL_100A_INV		(220.97)
#define	I_NORM_100A				(30.)					  // 30A로 튜닝
#define	THR_VAL_100A_INV	(110217569)
#define PH_NORM_100A   		(5700.0)			  // P=VIcos(theta),
#define	ALPHA_CONST_100A	0.01			      // 10E-2, alpha const를 0.001로 하면 THR_VALUE가 minimum값인 33,516,139를 초과하므로 0.01로 설정
#define	THR_VAL_100A_HV		(440281139)
#define PH_NORM_HV_100A   (1500.)

// _TYPE_200A
#define	A_FULL_200A_INV		(220.9708691)
#define	I_NORM_200A				(100.)					// 100A로 튜닝
#define	THR_VAL_200A_INV	(55108784)
#define PH_NORM_200A   		(19000.0)			  // P=VIcos(theta),
#define	ALPHA_CONST_200A	0.01			      // 10E-2, alpha const를 0.001로 하면 THR_VALUE가 minimum값인 33,516,139를 초과하므로 0.01로 설정

// LV(100mA)
#define	A_FULL_LV					(220.97)
// HV(5A)
#define	A_FULL_HV					(14.14213562)	  // 2000:1


//
// 위상 보정 방법 
//
// reference값으로 cos(60)를 넣었으므로 선간전압과 60도 차이나는 경우의 위상차 만큼을 보정할 것임
// 그렇게 하기 위해 A상을 보정할 때는, 현재 선간전압(Vab)과의 위상차이가 -30도 만큼 차이가 나므로 30도 만큼을 더 지연시키고
// (오미크론에서 -30도로 밸런스 맞춤)
// C상을 보정할 때는, 현재 선간전압(Vcb)와의 위상차가 +30도 이므로 90도를 더 지연시켜 (오미크론에서 -90도로 밸런스 맞춤) 보정함
//
// (요약)
// 0) 에너지가 측정되는 주기가 3초 이므로 값을 넣은 뒤, 3초 기다린 후 보정할 것
// 1) A상 보정 : 오미크론에서 밸런스 -30으로 한 뒤, shell에서 shell> calpha [ENTER]
// 2) C상 보정 : 오미크론에서 밸런스 -90으로 한 뒤, shell에서 shell> calphc [ENTER]

#define	METER_CONST		1000
//#define	CFNUM_VAL			(1000/(METER_CONST*ALPHA_CONST))
#define	PMAX_CONST		(33516139)

//#define	V_SCALE				(V_FULL/4191910.)
//#define	A_SCALE				(A_FULL/4191910.)

#define	AZ_FULL		(1.039862914)
#define	AZ_SCALE	(AZ_FULL/4191910.)		// 2000:1

//#define	VLEVEL_VAL		(V_FULL*491520/V_NORM)
#define VLEVEL_VAL_LV		(V_FULL_LV*491520/V_NORM_LV)
#define VLEVEL_VAL_HV		(V_FULL_HV*491520/V_NORM_HV)

//#define	THR_VAL			151306797

//#define	WTHR_H_VAL		(THR_VAL >> 24)
//#define	WTHR_L_VAL		(THR_VAL & 0xffffff)

//#define	VARTHR_H_VAL  (THR_VAL >> 24)
//#define	VARTHR_L_VAL	(THR_VAL & 0xffffff)

//#define	VATHR_H_VAL		(THR_VAL >> 24)
//#define	VATHR_L_VAL		(THR_VAL & 0xffffff)

//#define	WATT_SCALE		(V_SCALE*A_SCALE*16/PMAX_CONST)

// line cycle accumulation mode
#define	LC_WATT			0x1
#define	LC_VAR			0x2
#define	LC_VA				0x4
#define	LC_ZXSEL_A	0x8
#define	LC_ZXSEL_B	0x10
#define	LC_ZXSEL_C	0x20
#define	LC_RSTREAD	0x40
#define	LC_FREQ			0x80
//#define LCYCMOD_CAL_VAL (LC_FREQ | LC_RSTREAD | LC_VA | LC_VAR | LC_WATT)
#define LCYCMOD_CAL_VAL (LC_FREQ | LC_VA | LC_VAR | LC_WATT)

#define LCYCMOD_NOR_VAL 0x78    // set RSTREAD, clear line cycle accm. mode

//#define	E_INTERVAL			1				// enegry 수집 주기, alpha const = 0.0001
#define	E_INTERVAL			3				// enegry 수집 주기, alpha const = 0.001 
#define LINE_CYCLE_VAL  (120*E_INTERVAL)		// 60 * 2 * 1(# of phase) = 120, HALF Line cycle

#define APCFDEN_600     1260		// 220V/60A
#define VARCFDEN_600    1260	 	// 220V/60A

#define APCFDEN_300     1382		// 220V/30A
#define VARCFDEN_300    1382	 	// 220V/30A

#define APCFDEN_120     1321		// 220V/15A
#define VARCFDEN_120    1321	 	// 220V/15A

#define APCFDEN_5       1536		// 220V/5A
#define VARCFDEN_5      1536	 	// 220V/5A

#define WDIV_VAL        4

#define	VPH_A		0
#define	VPH_B		2
#define	VPH_C		1

#define	MBUS_AI		1202
#define	MBUS_AO		770

#define	AO_CHIP_COUNT	0
#define	AO_CHIP_TYPE	2
#define	AO_ID			26
#define	AO_IP			27

#define	WH_RESET		36


#define	IN_CH_TYPE		40
#define	AO_CH_TYPE		50
#define	AO_CT_RATIO		51
#define	AO_CH_1			52

#define	TYPE_3P3W_1CT	1
#define	TYPE_3P3W_2CT	2
#define	TYPE_3P4W		3
#define	TYPE_1P_AB		4
#define	TYPE_1P_BC		5
#define	TYPE_1P_CA		6

#define	IN_TYPE_3P3W	1
#define	IN_TYPE_3P4W	2

#define	DB_OFFSET		0x800
#define	EB_SIZE			108			// Energy Block Size = 108(18*3*2)

#define	M_PI				3.141592654
#define	PHRES_60	(360.0*60./1024000.)
#define	PHRES_50	(360.0*50./1024000.)

//#define ENGVAL_FOR_ONEmA_ZCT		1320.28		// ZCT 1mA에 해당하는 engineering value. MSU_Scale_ZCT.xls 참조, 127:1 CT
//#define ENGVAL_FOR_ONEmA_ZCT		838				// ZCT 1mA에 해당하는 engineering value. MSU_Scale_ZCT.xls 참조, 2000:1 CT, 부담저항 100오옴
#define ENGVAL_FOR_ONEmA_ZCT		5701				// ZCT 1mA에 해당하는 engineering value. MSU_Scale_ZCT.xls 참조, 2000:1 CT, 부담저항 680오옴
#define MAX_GFAULT_COUNT				4					// Ground Fault에 해당하는 전류가 2cylcle 이내에 4번 이상 발생하면 Ground Fault로 간주
#define MAX_GF_CONST						4					// Ground Fault관련 상수는 4개, 0 - 50mA, 1 - 100mA, 2 - 200mA, 3 - 500mA

#define MIN_ZRMS_VAL						0.0005		// 0.5mA 미만이면 0으로 표시한다.


#define	PWR_MAX	20694066
#define	PERCENT_SCALE		(100./(1<<27))
#define	RMS_MAX	 52702092
#define	WAVE_MAX 67107786

//typedef struct {
//	long	vgain[3];
//	long	voffset[3];
//	long 	igain[3];
//	long	ioffset[3];

//	long	wattgain[3];
//	long	wattos[3];
//	long	vargain[3];
//	long 	varos[3];
//	long	vagain[3];	
//	long	phcal[3];
//	
//	long	fwgain[3];
//	long	fwattos[3];	
//	long	fvargain[3];
//	long	fvaros[3];	
//	uint32_t	chksum;
//} CAL_TBL;


//typedef struct {
//	uint32_t	mbusId;
//	uint32_t 	ct_ratio;		// 1차 전류 
//} SYS_DB;


//typedef struct {
//	// energy
//	long wattHR;
//	long varHR;

//	// realtime value
//	//uint16_t vrms[3];
//	//uint16_t irms[3];
//	float vrms[3];
//	float irms[3];
//	uint16_t freq;
//	uint16_t pf;

//	uint16_t watt[3];
//	uint16_t var[3];
//	uint16_t va[3];

//	uint16_t	reserved[2];
//} MBUS_AIDATA;

//typedef struct {
//	uint8_t	coil[8];
//} MBUS_DIDATA;


//typedef struct {
//	// energy
//	long wattHR;
//	long varHR;

//	// 소수점 이하 전력량을 보상하기 위해 사용한다 
//	float	fWattsum;
//	float fVarsum;

//	uint32_t  irqmask;
//	uint16_t  apcfden;
//	uint16_t	period;

//	float vrms[3];
//	float irms[3];
//	float	zrms;
//	float freq, _freq[3];
//	float pf, _pf[3];

//	float watt;
//	float var;
//	float va;
//	
//	float	angleVI[3];	// V-I, VIa, VIb, VIc
//	float angleVV[3];	// V-V, Vac, Vbc, Vab
//	float angleII[3];	// I-I, Iac, Ibc, Iab
//	float	absAngleV[3];	// 0도를 기준으로 하는 Va, Vb, Vc위상
//	float	absAngleI[3];	// 0도를 기준으로 하는 Ia, Ib, Ic위상
//	SYS_DB 	config;
//	
//	//
//	//
//	//
//	float	vthd[3], ithd[3];
//	float vrms1[3], irms1[3];
//	float _watt[3], _var[3], _VA[3];
//	int64_t wh[3], varh[3], VAh[3];
//	float	watt_reg, var_reg;
//	//
//	float ctratio, ptratio;
//	float	iscale, izscale, vscale, wscale;
//	float	vfull, ifull;
//} METER_DATA;


//typedef struct {
//	uint8_t	sec;
//	uint8_t	minute;
//	uint8_t	hour;
//	uint8_t	mday;
//	uint8_t	month;
//	uint8_t	year;
//} RTC_TYPE;


//// 누적값 : engineering value
//typedef struct {	
//	uint32_t	wattHR[3], varHR[3], vaHR[3];
//} FRAM_DATA;

////
//// 16bit resampled buffer
////

////#define	WQ_CNT	2

////typedef struct {
////	int16_t buf[2048][2];
////} WAVE_BUF;

////typedef struct {
////	WAVE_BUF	wb[WQ_CNT];
////	int fr, re;
////} WAVE_RBUF;

////
//// 32bit fixed rate buffer
//// 

//#define	PG_CNT	(16*32)	// 16*16*32 = 8192 sample
//#define	PG8K_CNT	(16*32)	// 16*16*32 = 8192 sample
//#define	PG32K_CNT	(250)

//typedef union {
//	int16_t b16[2];
//	int32_t b32;
//} UBUF;

//// single page
//typedef struct {
//	UBUF	ub[128];	// 8 channel * 16 data
//} WAVE8k_PAGE;

//typedef struct {
//	int fr, re;
//	WAVE8k_PAGE	wb[PG8K_CNT];	
//} WAVE8k_PGBUF;

//// single page
//typedef struct {
//	UBUF	ub[1024];	// 8 channel * 16 data
//	//uint16_t ub[2050];
//} WAVE32k_PAGE;

//typedef struct {
//	int fr, re;
//	WAVE32k_PAGE	wb[PG32K_CNT];	
//} WAVE32k_PGBUF;

//typedef struct {
//	int	fr, re;
//	int32_t wv[1024][6];
//} WAVE_1024_BUF;

//typedef struct {
//	float mag[64];
//} HD_DATA;

//typedef struct {
//	// even, odd, zero, pos, neg THDs
//	float vTHD[4];
//	float iTHD[4];
//	
//	HD_DATA	vhd[3];
//	HD_DATA	ihd[3];
//	
//	float cF[3];	// crest factor
//	float kF[3];	// K factor
//} HARMONICS;

//typedef struct {
//	float vrms[4], vfrms[4], vrms_1[4], vrms_1012[4];
//	float irms[4], ifrms[4], irms_1[4], irms_1012[4];
//	float nirms, nirms_1, nirms_1012, isum;
//	float	watt[4], fwatt[4];
//	float var[4], fvar[4];
//	float VA[4], fVA[4];
//	float	PF[4];
//	float freq;
//	float vunlal, iunbal;
//	float angleVV[3], angleVI[3], angleII[3];
//	
//	// harmonics
//	HARMONICS	hm;
//} METERING;


typedef struct {
	//
	// ADE9000 Map
	//
	int	ix;
	int	ix2;

	uint32_t	period[3][6];
	
	uint32_t	urms[3][6], irms[3][6], inrms[6], isrms[6];
	uint32_t	urms_1012[3][6];
	uint32_t 	furms[3][6], firms[3][6];
	uint16_t	UIangle[3][6];	// V-I
	uint16_t	Uangle[3][6];	// V-V, Vac, Vbc, Aab
	uint16_t	Iangle[3][6];	// I-I, Iac, Ibc, Iab	
} METER_REGS;

typedef struct {
	int32_t wh;	// wh, varh, vah, fwh, fvarh, fvah
	int32_t varh;	// wh, varh, vah, fwh, fvarh, fvah
	int32_t vah;	// wh, varh, vah, fwh, fvarh, fvah
	int32_t fwh;	// wh, varh, vah, fwh, fvarh, fvah
	int32_t fvarh;	// wh, varh, vah, fwh, fvarh, fvah
	int32_t fvah;	// wh, varh, vah, fwh, fvarh, fvah
} METER_EH_REGS;

typedef struct {
	uint32_t fr, re, status0, status1;	
	// 10/12 registers
	METER_REGS	sr[2];
	// fastrms
	uint32_t 	urmsFast[3], irmsFast[3], freqFast[4];	
	// power
	int watt[3], fwatt[3];
	int var[3], fvar[3];
	int va[3], fva[3];
	int	pf[3], vthd[3], ithd[3];
	
	uint32_t efr, ere;
	METER_EH_REGS energy[2][3];
} ADE9000_REG;

extern ADE9000_REG ade9000[];

#define	AD9X_IGAIN	0x0
#define	AD9X_PHCAL	0x6
#define	AD9X_UGAIN	0xb
#define	AD9X_WGAIN	0xe
#define	AD9X_INGAIN	0x6d

#define	AD9X_GAINSZ	0x20

#define	AD9X_STATUS0	0x402
#define	AD9X_STATUS1	0x403
#define	AD9X_EVENT		0x404

#define	AD9X_MASK0	0x405
#define	AD9X_MASK1	0x406

#define	AD9X_WTHR 0x420
#define	AD9X_VARTHR 0x421
#define	AD9X_VATHR 0x422

#define	AD9X_PART_ID 0x472

#define	AD9X_RUN	0x480
#define	AD9X_CONFIG1	0x481
// ISUM_CFG - 00 : A+B+C, 01 :A+B+C+N, 02 :A+B+C-N  
#define AD9X_CONFIG0 	0x60		// ISUM = A+B+C

#define	AD9X_CFMODE	0x490
#define	AD9X_COMPMODE 0x491
#define	AD9X_ACCMODE 0x492
#define	AD9X_ZXTOUT	0x498
#define	AD9X_ZXTHRSH 0x499

// add pulse config
#define	AD9X_CF1DEN	0x494
#define	AD9X_CF2DEN	0x495
#define	AD9X_CF3DEN	0x496
#define	AD9X_CF4DEN	0x497

#define	AD9X_PWR_TIME 0x4b1
#define	AD9X_EGY_TIME 0x4b2
#define	AD9X_PGA_GAIN 0x4b9
#define	AD9X_WFB_CFG 0x4a0
#define	AD9X_WFB_PG_IRQEN	0x4a1
#define	AD9X_WFB_TRIG_CFG  0x4a2
#define	AD9X_WFB_TRIG_STAT	0x4a3

#define	AD9X_VERSION	0x4fe

#define	AD9X_AVGAIN	0x00b
#define	AD9X_BVGAIN	0x02b
#define	AD9X_CVGAIN	0x04b

#define	AD9X_SAG_A		0x411
#define	AD9X_SWELL_A	0x415


#define	W_60HZ	(2*M_PI*60./8000)
#define	W_50HZ	(2*M_PI*50./8000)

#define	FREQ_CONST	(8000.*65536)
#define	ANGLE_CONST	(360./1024000)
#define	RAD_CONST		(2*M_PI/1024000)


// ADC range가 0.5 에서 1.0으로 두배 증가함.
#define	V_FULL				(848.88*2)	// @VGAIN=2
#define	V_NORM				(220.00)

// JN10, burden=66.6, PF=0.5 조건에서 전류크기별 위상오차 부호가 반전된다, 5A(-), 0.25A(+), 0.5 Class를 벗어난다 
#define	I_FULL_5A			(10.617)		// CT(1000:1), R=66.6, gain=1x, 
#define	I_FULL_5AN		(35.355)		// CT(1000:1), R=20, gain=1x, 
#define	I_FULL_100mA	(220.971)		// CT(100mA), R=3.2, gain=1x
#define	I_FULL_RCT_60Hz	(2946.28)	// 60Hz, 120mV@500A
#define	I_FULL_RCT_50Hz	(3535.53)	// 50Hz, 100mV@500A
#define	CFxDEN_5A				(6306 + 152)		//(0x18a2), 튜닝 후 펄스 오차가 2~ 3% 높게 나온다
#define	CFxDEN_5AN			(1894 + 46)			//(0x766)

#define	V_SCALE					(V_FULL/RMS_MAX)
#define	I_SCALE_5A			(I_FULL_5A/RMS_MAX)
#define	I_SCALE_5AN			(I_FULL_5AN/RMS_MAX)
#define	I_SCALE_100mA		 (I_FULL_100mA/RMS_MAX)
#define	I_SCALE_RCT_60Hz (I_FULL_RCT_60Hz/RMS_MAX)
#define	I_SCALE_RCT_50Hz (I_FULL_RCT_50Hz/RMS_MAX)
#define	CFxDEN_100mA		 (3030 + 60)	//(0xbd6)

#define	IZ_FULL				(3.571)	// 200mA:1.5mA, 4x 기준

// 0.001 -> 0.0005로 변경
#define	I_NOLOAD_5A			(1.0*0.0005)	// (2.5*0.0005)
#define	I_NOLOAD_5AN		(2.5*0.0005)
#define	I_NOLOAD_100mA	(25 *0.0005)	// 50A CT 기준
#define	I_NOLOAD_RCT		(250*0.0005)	// 250A

// 정전(Interruption) 기준 : 11@220, 5.5V@110
#define	PICKUP_VOLT	10	
#define	PICKUP_VOLT_RAW  (RMS_MAX*PICKUP_VOLT/V_FULL*2)

long rms2long(uint32_t src);
float scaleVrms(int id, long src);
float scaleIrms(int id, long src);
float scaleIgrms(int id, long src);
float scaleIzrms(int id, long val);
float scalePhase(int id, uint16_t src);
int scalePower(int id, long val);
//float scaleEnergy(int64_t val);

//extern float A_SCALE, V_SCALE, WATT_SCALE, ALPHA_CONST, CFNUM_VAL;
//extern float I_NORM, PH_NORM, V_NORM, VLEVEL_VAL;
//extern uint32_t THR_VAL, WTHR_H_VAL, WTHR_L_VAL;
//extern uint32_t VARTHR_H_VAL, VARTHR_L_VAL, VATHR_H_VAL, VATHR_L_VAL; 
//extern float STEPVAL_FOR_LOWCUR_XA[MAX_HW_TYPE], STEPVAL_FOR_HIGHCUR_XA[MAX_HW_TYPE], STEPVAL_FOR_VOLT_XA[MAX_HW_TYPE];

//extern void calcConvAngle(int id);
//void updateTF(int id);
//void write_vTF(int id);
//void write_vGain(int id);
//void write_vOffset(int id);
//void write_iTF(int id);
//void write_iGain(int id);
//void write_iOffset(int id);
//void write_wattTF(int id);
//void write_varTF(int id);
//void write_vaTF(int id);
//void write_phTF(int id);
void write_vmon(int id);
void clearEvent(int id);
void initAde7758(int id);

void resetWatt(void);
void resetVar(void);
void resetVA(void);

//extern METER_DATA meterData[MAX_METER];
//extern MBUS_AIDATA  mbusData[MAX_METER];
//extern SYS_DB *mstDB;
extern int zxTimer;


//int read_reg16(uint8_t mid, uint16_t cmd, uint16_t *pdata);
//int read_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata);
//int read_reg32n(uint8_t mid, uint16_t cmd, uint32_t *buf, int n);
//void swapEndian(int *src, int n);
//int write_reg16(uint8_t mid, uint16_t cmd, uint16_t *pdata);
//int write_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata);

//void framLWrite(uint32_t *pbuf, uint32_t offset);
//void framLRead(uint32_t *pbuf, uint32_t offset);
//void framWriteBuf(uint8_t *pbuf, uint32_t offset, int count);
//void framReadBuf(uint8_t *pbuf, uint32_t offset, int count);
//void getDateTime(RTC_TYPE *prtcTime);
//void setDateTime(RTC_TYPE *prtcTime);

void meter_scan(uint8_t id);
void calcRmsFreqAngle(int id);
void calcPower(int id);
void calcTHD(int id);

void Meter1_Scan(uint8_t id);
void Meter1_Init(uint8_t id);
void initAde7878(uint8_t id);
void initDevTypeVar(void);
void readRMS(uint8_t id);
void readPower(uint8_t id);
void readEnergy(uint8_t id);
float scaleEnergy(int id, int32_t raw);
void incEnergy(float diff, float *pAcc);
int updateEh(int id, int mode, float diff);
//int updateWh(float diff);
//int updateVarh(float diff);
//int updateVAh(float diff);
//void meterIntEnable(uint8_t id);



void writeGainU(int id);	 
void clrGainU(int id);
void setGainU(int id, float ref);

void writeGainI(int id);	 
void clrGainI(int id);
void setGainI(int id, float ref);

void writeGainPh(int id);	 
void clrGainPh(int id);
void setGainPh(int id);

void writeGainW(int id);	 
void clrGainW(int id);
void setGainW(int id, float vref, float iref);

void detectGroundFault(int32_t ldata);
void prtwave(void);
void PhCal(int index, float ref);

void readHalfCycleRMS(void);
WVPG *getWave8kBuf(WAVE_PGBUF *, int *);
WVPG *getWave32kBuf(WAVE_PGBUF *);

void initADE9000(uint8_t);
void meterIrqSvc();

void calcSeqComponent(float *mag, float *angle, float *seq);
float calcUnbalance(float *seq, float *bal);
void calcDeviation(int id, float *U, float norm);

extern void setGainU(int id, float vref);
extern void setGainUpp(int id, float ref);
extern void setGainI(int id, float iref);
extern void setGainIn(int id, float iref);
extern void setGainW(int id, float vref, float igain);
extern void setGainPh(int id);
extern void clrGainU(int id);
extern void clrGainUpp(int id);
extern void clrGainI(int id);
extern void clrGainIn(int id);
extern void clrGainW(int id);
extern void clrGainPh(int id);

#endif
