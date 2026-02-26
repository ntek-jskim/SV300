
//#include <LPC43xx.h> 
#include "os_port.h"
#include "ade9000.h"
#include "meter.h"
#include "stdio.h"
#include "math.h"
#include "ssplib.h"
#include "board.h"
#include "string.h"

#define	CONST_SIN_60	0.866025404
#define	CONST_COS_60	0.5
#define	CONST_OMEGA_50	0.039269908
#define	CONST_OMEGA_60	0.047123890

//const int sagMask   = (1<<23) | (1<<24) | (1<<25);
//const int swellMask = (1<<20) | (1<<21) | (1<<22);

//float lineFreq = 60;
uint32_t irqFlag;

#ifdef _IRQ_MBX
os_mbx_declare(mbox, 16);
#endif

//FRAM_ENERGY1 fenergy1, *pfEnergy1=&fenergy1;
//FRAM_ENERGY2 fenergy2, *pfEnergy2=&fenergy2;


extern uint64_t sysTick64;
extern OsTaskId tid_wave[];
extern OsTaskId tid_meter[];
extern OsTaskId tid_rmslog, tid_post, tid_energy;

extern void  updateQualData();
extern int maxMinRmsFreq(int id);
extern int maxMinPower(int id);
extern int maxMinTHD(int id);

#define	RAD_SCALE	(M_PI/180)

float radians(float angle) {
	return angle*RAD_SCALE; 
}

float degrees(float rad) {
	float f = rad/RAD_SCALE;
	f += (f < 0) ? 360 : 0;
	return f;
}

void write_log(int errCode, int errType) {
//	FILE *fp;
//	if (eLog.fs == 0) 
//		return;
//			
//	memcpy(eLog.log[eLog.fr].ts, pmeter->tod, sizeof(pmeter->tod));
//	eLog.log[eLog.fr].errCode = errCode;
//	eLog.log[eLog.fr].errType = errType;
//	eLog.fr = (eLog.fr + 1) % N_ERR_LOG;
//	if (eLog.fr == eLog.re) { 
//		eLog.re = (eLog.re + 1) % N_ERR_LOG;
//	}
//	
//	fp = fopen(LOG_FILE, "wb");
//	if (fp != NULL) {
//		fwrite(&eLog, sizeof(eLog), 1, fp);
//		fclose(fp);
//	}
//	printf("write_log(%d, %d), fr=%d\n", errCode, errType, eLog.fr);
}


float average(float *pf,int c)
{
	float s=0;
	int i;
	
	for(i=0;i<c;i++){
		s+=pf[i];
	}
	return s/c;
}


float summation(float *pf,int c)
{
	float s=0;
	int i;
	
	for(i=0;i<c;i++){
		s+=pf[i];
	}
	return s;
}


//chip으로부터읽은주파수를가지고위상계산한다
// 기준점으로 zc가 검출된 기간으로 위상각 계산
float scaleAngle(int id, int raw)
{
	//float angle = 360 - raw * ANGLE_CONST *pmeter->Freq;
	float angle = raw * ANGLE_CONST *meter[id].cntl.freq[0];
	return angle;
	//return fmod(angle, 360);
}

//
void getComplexSum(int id, int raw, float *cx) {
	float rad = raw * RAD_CONST * meter[id].cntl.freq[0];
	cx[0] += cos(rad);
	cx[1] += sin(rad);
}

float getAngle(int id, int n, uint16_t *angle) {
	float x=0, y=0, a;
	int i;
	
	for (i=0; i<5; i++) {
		a = scaleAngle(id, angle[i]);
		x += cos(radians(a));
		y += sin(radians(a));
	}
		
	return 360 - degrees(atan2(y/n, x/n));	// double atan2(double y, double x)
}


// zxtout이 발생하면 주파수는 라인주파수가 된다
__inline float scaleFreq(uint32_t period) {
	return FREQ_CONST/(period+1);
}



float scaleVrms(int id, long val)
{
	return val*meter[id].cntl.vscale;
}

//float getIScale(int src)
//{
//	float i = src*meter[id].cntl.iscale;
//	return (meter[id].cntl.pga_igain) ? i/meter[id].cntl.pga_vgain : i;
//}


//소수점첫째자리까지구한다
//16-10-12,I_NOLOAD검사하는부분을평균값계산하는부분으로이동
float scaleIrms(int id, long val)
{
	return val*meter[id].cntl.iscale;
}

float scaleIgrms1(int id, long val) {
	return val*meter[id].cntl.igscale;
}

float scaleIgrms(int id, long val) {
//	return val*meter[id].cntl.igscale;
	float x1, x2, v1, v2;

	x1 = meter[id].meter.I[3];				// 현재 계측되는 전류의 평균
	x2 = meter[id].cntl.In_offset_I[0];		// seq 0 일때 입력 전류

	v1 = pcal->In_Slope[id]*(x1-x2) + pcal->In_offset[id];
	v2 = val*meter[id].cntl.igscale - v1;

	if(v2<0)
		v2 = 0;
	return v2;

}

void writeGainI(int id) {
	int	addr=AD9X_IGAIN, j;

	for(j=0; j<3; j++, addr+=AD9X_GAINSZ){
		write_reg32(id, addr, (uint32_t*)&pcal->igain[id][j]);
	}	
}

// PT2 < 150 이면 vgain x2 사용한다 
void writeGainU(int id) {
	int	addr=AD9X_UGAIN, j, *pvg;

#ifdef	PT_HV	
	if (pdb->pt.PT2 < 150) {
		pvg = (pdb->pt.wiring == WM_3LL3CT || pdb->pt.wiring == WM_3LL2CT) ? pcal->vppgainx2 : pcal->vgainx2;
	}
	else {
		pvg = (pdb->pt.wiring == WM_3LL3CT || pdb->pt.wiring == WM_3LL2CT) ? pcal->vppgain : pcal->vgain;
	}
#else
	pvg = (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT) ? pcal->vppgain[id] : pcal->vgain[id];
#endif	
		
	for (j=0;j<3;j++, addr+=AD9X_GAINSZ) {
		write_reg32(id, addr,(uint32_t*)&pvg[j]);
	}
}

void resetGainInOffset(int id){
	int			i;
	for(i=0; i<2; i++) {
		meter[id].cntl.In_offset[i] = 0;
		meter[id].cntl.In_offset_I[i] = 0;
	}
	meter[id].cntl.In_Slope = 0;
	pcal->In_offset[id] = 0;
	pcal->In_Slope[id] = 0;
}

void setGainInOffset(int id, int seq)
{	
	// 평균전류를 offset
	meter[id].cntl.In_offset[seq] = meter[id].cntl.In1;
	meter[id].cntl.In_offset_I[seq] = meter[id].meter.I[3];

	printf("setGainInOffset : id[%d], seq[%d] --> I[%.1f], In offset[%.4f]\n", id, seq, meter[id].cntl.In_offset_I[seq], meter[id].cntl.In_offset[seq]);
	
//	pcal->In_offset[id] = meter[id].cntl.In_offset[0];
//	pcal->In_Slope[id] = meter[id].cntl.In_Slope;
}
void writeInOffset(int id) {
	meter[id].cntl.In_Slope = (meter[id].cntl.In_offset[1] - meter[id].cntl.In_offset[0])/(meter[id].cntl.In_offset_I[1]-meter[id].cntl.In_offset_I[0]);

	pcal->In_offset[id] = meter[id].cntl.In_offset[0];
	pcal->In_Slope[id] = meter[id].cntl.In_Slope;

	printf("writeOffsetIn : id[%d] --> I slope[%.1f], In offset[%.4f]\n", id, pcal->In_offset[id], pcal->In_Slope[id]);

//	meter[id].cntl.In_offset = pcal->In_offset[id];
}

void writeGainPh(int id) {
	int addr=AD9X_PHCAL, j;
	
	for(j=0;j<3;j++, addr+=AD9X_GAINSZ) {
		write_reg32(id, addr, (uint32_t*)&pcal->phcal[id][j]);
	}		
}

void writeGainIn(int id) {
	int addr=AD9X_INGAIN;
	write_reg32(id, addr, (uint32_t*)&pcal->Ingain[id]);
}

void writeGainW(int id) {
	int addr=AD9X_WGAIN, j;
	
	for(j=0;j<3;j++, addr+=AD9X_GAINSZ) {
		write_reg32(id, addr, (uint32_t*)&pcal->wgain[id][j]);	
	}		
}

// U
// 3P4W, 3P3W 모드, X1, X2 에서 각각 조정한다 
void setGainU(int id, float ref)
{
	float fg;
	int i, *pvg;

#ifdef	PT_HV
	pvg = (pdb->pt.PT2 < 150) ? pcal->vgainx2 : pcal->vgain;
#else
	pvg = pcal->vgain[id];
#endif	
	
	// Vab: vrms[0] -> vgain[0]
	for (i=0; i<3; i++) {
		fg = ref/meter[id].meter.U[i];
		pvg[i] = (fg-1)*GAIN_MAX;
	}

	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 1;
	printf("vg : %d, %d, %d\n", pvg[0], pvg[1], pvg[2]);
}
// V0: Va-Vb -> U[0]
// V1: Vc-Va -> U[2]
// V2: Vc-Vb -> U[1]
void setGainUpp(int id, float ref)
{
	float fg;
	int i, *pvg;
	
#ifdef	PT_HV	
	pvg = (pdb->pt.PT2 < 150) ? pcal->vppgainx2 : pcal->vppgain;
#else
	pvg = pcal->vppgain[id];
#endif	

	fg = ref/meter[id].meter.U[0];
	pvg[0] = (fg-1)*GAIN_MAX;
	
	fg = ref/meter[id].meter.U[1];
	pvg[2] = (fg-1)*GAIN_MAX;
	
	fg = ref/meter[id].meter.U[2];
	pvg[1] = (fg-1)*GAIN_MAX;

	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 1;
	printf("vpp_g : ref(%f), %d, %d, %d\n", ref, pvg[0], pvg[1], pvg[2]);
}


void clrGainU(int id)
{
	int i, *pvg;

#ifdef	PT_HV	
	pvg = (pdb->pt.PT2 < 150) ? pcal->vgainx2 : pcal->vgain;
#else
	pvg = pcal->vgain[id];
#endif	

	for (i=0; i<3; i++) {
		pvg[i] = 0;
	}
	
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 1;
	printf("clear volt gain\n");
}
void clrGainUpp(int id)
{
	int i, *pvg;

#ifdef	PT_HV	
	pvg = (pdb->pt.PT2 < 150) ? pcal->vppgainx2 : pcal->vppgain;
#else
	pvg = pcal->vppgain[id];
#endif	

	for (i=0; i<3; i++) {
		pvg[i] = 0;
	}
	
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 1;
	printf("clear phase to phase volt gain\n");
}

void clrDcOffset(int id) {
	int i=0; 
	
	for (i=0; i<3; i++) {
		pcal->vdcos[id][i] = pcal->idcos[id][i] = 0;
	}
}

void setDcOffset(int id) {
	int i;
	meter[id].cntl.calDcOs = 1;
	for (i=0; i<5 && meter[id].cntl.calDcOs; i++) {
		osDelayTask(100);
	}
	
	if (i<5) {
		printf("DC V OS: %d, %d, %d\n", pcal->vdcos[id][0], pcal->vdcos[id][1], pcal->vdcos[id][2]);
		printf("DC I OS: %d, %d, %d\n", pcal->idcos[id][0], pcal->idcos[id][1], pcal->idcos[id][2]);
	}
	else {
		printf("Can't get DC Offset ...\n");
	}
}

// I
// 20% 이상 차이 나면 보정하지 않는다
void setGainI(int id, float ref)
{
	float 					fg, error;
	int 						i;

	for (i=0; i<3; i++) {
		error = (ref-meter[id].meter.I[i])/ref;
		
		if (fabs(error) > 0.2) 
			continue;
		
		fg = ref/meter[id].meter.I[i];
		pcal->igain[id][i] = (fg-1) * GAIN_MAX;
	}
	
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 2;
	printf("ig : ref=%f, (%f, %f, %f), %d, %d, %d\n", ref, meter[id].meter.I[0], meter[id].meter.I[1], meter[id].meter.I[2], pcal->igain[0], pcal->igain[1], pcal->igain[2]);
}
void clrGainI(int id)
{
	int i;

	for (i=0; i<3; i++) {
		pcal->igain[id][i] = 0;
	}
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 2;
}
// PT ratio = 1;1
// CT ratio = 50/5
void setGainW(int id, float refU, float refI) {
	int i;
	float exp = refU*refI, error;
	
	for (i=0; i<3; i++) {
		error = (exp - meter[id].meter.P[i])/exp;
		pcal->wgain[id][i] = error * GAIN_MAX;
	}
//	float Ifsp, Ufsp, error, exp;

//	Ufsp = refU*(1<<meter[id].cntl.pga_vgain)/V_FULL;
//	Ifsp = refI*(1<<meter[id].cntl.pga_igain)/I_FULL_5A/(pdb->ct.CT1/5.);
//	
//	exp = Ufsp*Ifsp*20694066*8000/8192;
//		
//	for (i=0; i<3; i++) {
//		error = (exp - meter[id].cntl.wh[i])/exp;		
//		printf("w error : %f\n", error);
//		
//		if (fabs(error) > 0.2) 
//			continue;
//				
//		pcal->wgain[i] = error * GAIN_MAX;
//	}	
	printf("wg : ref(%f), %d(%d), %d(%d), %d(%d)\n", exp, 
		pcal->wgain[id][0], meter[id].cntl.wh[0], pcal->wgain[id][1], meter[id].cntl.wh[1], pcal->wgain[id][2], meter[id].cntl.wh[2]);
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 3;
}


void clrGainW(int id) {
	int i;

	for (i=0; i<3; i++) {
		pcal->wgain[id][i] = 0;
	}
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 3;
}
// phase
void setGainPh(int id)
{
	float a, b, c, omega;
	int i;
		
	omega = (pdb->pt.freq == 60) ? CONST_OMEGA_60 : CONST_OMEGA_50;
	
	for (i=0; i<3; i++) {
		a = meter[id].cntl.wh[i]*CONST_SIN_60 - meter[id].cntl.varh[i]*CONST_COS_60;
		b = meter[id].cntl.wh[i]*CONST_COS_60 + meter[id].cntl.varh[i]*CONST_SIN_60;
		c = -atan(a/b);
		
		a = sin(c-omega) + sin(omega);
		b = sin(2*omega-c);
		c = a/b*GAIN_MAX;
		
		pcal->phcal[id][i] = c;
	}
	
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 4;
}
void clrGainPh(int id)
{
	int i;

	for (i=0; i<3; i++) {
		pcal->phcal[id][i] = 0;
	}
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 4;
}
// In
void setGainIn(int id, float ref)
{
	float 					fg, error;

	error = (ref-meter[id].meter.In)/ref;
	if (fabs(error) <= 0.2) {
		fg = ref/meter[id].meter.In;
		pcal->Ingain[id] = (fg-1) * GAIN_MAX;
		meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 5;
	}
	printf("zgain : %d\n", pcal->Ingain[id]);
}
void clrGainIn(int id)
{																																																									
	pcal->Ingain[id] = 0;
	meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 5;
}

//
float calcUIAngle(int id, int sel)
{
	float a,t;
	
	//DividebyZero방지
	if(meter[id].meter.I[1]==0){
		return 0;
	}
	
	if(meter[id].cntl.Iangle[0]>180){	// 정상
		a = 360 - meter[id].cntl.Iangle[0];
		t = (meter[id].cntl.I[0] - meter[id].cntl.I[2]*cos((180-a)*M_PI/180.))/meter[id].cntl.I[1];
		t = 180-acos(t)*180/M_PI;
		return (meter[id].cntl.UIangle[0]+t-meter[id].cntl.Uangle[1]);
	}
	else{	// 역상
		a=meter[id].cntl.Iangle[0];	
		t=(meter[id].cntl.I[0]-meter[id].cntl.I[2]*cos((180-a)*M_PI/180.))/meter[id].cntl.I[1];
		t=180+acos(t)*180/M_PI;		// 전류 위상차 Ia-Ic위상차를 이용하여 Ia-Ib를 구한다
		t=fmod(meter[id].cntl.UIangle[0]+t,360); // Ib의 절대위상각을 구한다.
		a=fmod(meter[id].cntl.Uangle[2]-t,360)-180;	// Vb-Ib의 위상차를 구한다 
		return a;
	}
}


float _toAngle(float v) {
	if (v > 360) 
		return v - 360;
	else if (v < 0) 
		return 360 + v;
	else
		return v;
}

void calcAbsAngle(int id)
{			
	float angle;
	int i;
	
	if(db[id].pt.wiring==WM_3LL3CT||db[id].pt.wiring==WM_3LL2CT){
		//normal: VaVb,(240), VbVc(60), VaVc(300)
		meter[id].meter.Uangle[0] = 0;	// V1-V1
		meter[id].meter.Uangle[1] = _toAngle(meter[id].cntl.Uangle[2] + 180);	// V1-V2
		meter[id].meter.Uangle[2] = meter[id].cntl.Uangle[0];	// V1-V3

		if (db[id].pt.wiring==WM_3LL2CT) {
			meter[id].cntl.Iangle[1] = calcUIAngle(id, 1);
		}
		
		meter[id].meter.Iangle[0] = _toAngle(meter[id].meter.Uangle[0] + meter[id].cntl.UIangle[0]);// V1-I1
		meter[id].meter.Iangle[1] = _toAngle(meter[id].meter.Iangle[0] + meter[id].cntl.Iangle[0]);	// V1-I1;	// V1-I2
		meter[id].meter.Iangle[2] = _toAngle(meter[id].meter.Iangle[0] + meter[id].cntl.Iangle[2]);	// V1-I1;	// V1-I3

		meter[id].meter.Pangle[0] = (meter[id].meter.I[0] == 0) ? 0 : _toAngle(meter[id].meter.Uangle[0] - meter[id].meter.Iangle[0]);
		meter[id].meter.Pangle[1] = (meter[id].meter.I[1] == 0) ? 0 : _toAngle(meter[id].meter.Uangle[1] - meter[id].meter.Iangle[1]);
		meter[id].meter.Pangle[2] = (meter[id].meter.I[2] == 0) ? 0 : _toAngle(meter[id].meter.Uangle[2] - meter[id].meter.Iangle[2]);
	}
	else if(pdb->pt.wiring==WM_3LN3CT){
		meter[id].meter.Uangle[0] = 0;	// V1-V1
		meter[id].meter.Uangle[1] = meter[id].cntl.Uangle[0];//V1-V2
		meter[id].meter.Uangle[2] = meter[id].cntl.Uangle[2];//V1-V3
					
		meter[id].meter.Iangle[0] = _toAngle(meter[id].meter.Uangle[0] + meter[id].cntl.UIangle[0]);	// V1-I1
		meter[id].meter.Iangle[1] = _toAngle(meter[id].meter.Uangle[1] + meter[id].cntl.UIangle[1]); 	// V1-I2
		meter[id].meter.Iangle[2] = _toAngle(meter[id].meter.Uangle[2] + meter[id].cntl.UIangle[2]);	// V1-I3
		
		meter[id].meter.Pangle[0] = (meter[id].meter.I[0] == 0) ? 0 : 360 - meter[id].cntl.UIangle[0];
		meter[id].meter.Pangle[1] = (meter[id].meter.I[1] == 0) ? 0 : 360 - meter[id].cntl.UIangle[1];
		meter[id].meter.Pangle[2] = (meter[id].meter.I[2] == 0) ? 0 : 360 - meter[id].cntl.UIangle[2];
	}
	else if(pdb->pt.wiring==WM_1LL2CT){
		meter[id].meter.Uangle[0] = meter[id].meter.Uangle[1] = 0;	// V1-V1
		meter[id].meter.Uangle[2] = meter[id].cntl.Uangle[2];				// V1-V3
		
		meter[id].meter.Iangle[0] = _toAngle(meter[id].meter.Uangle[0] + meter[id].cntl.UIangle[0]);	// V1-I1
		pmeter->Iangle[1] = 0;
		meter[id].meter.Iangle[2] = _toAngle(meter[id].meter.Uangle[2] + meter[id].cntl.UIangle[2]);	// V1-I3
		
		meter[id].meter.Pangle[0] = (meter[id].meter.I[0] == 0) ? 0 : 360 - meter[id].cntl.UIangle[0];
		meter[id].meter.Pangle[1] = 0;
		meter[id].meter.Pangle[2] = (meter[id].meter.I[2] == 0) ? 0 : 360 - meter[id].cntl.UIangle[2];		
	}
	else if(pdb->pt.wiring==WM_1LN1CT){
		meter[id].meter.Uangle[0] = meter[id].meter.Uangle[1] = meter[id].meter.Uangle[2] = 0;
		
		meter[id].meter.Iangle[0] = _toAngle(meter[id].meter.Uangle[0] + meter[id].cntl.UIangle[0]);	// V1-I1
		meter[id].meter.Iangle[1] = meter[id].meter.Iangle[2] = 0;		//V2-I2,V3-I3	
		
		meter[id].meter.Pangle[0] = (meter[id].meter.I[0] == 0) ? 0 : 360 - meter[id].cntl.UIangle[0];	
		meter[id].meter.Pangle[1] = meter[id].meter.Pangle[2] = 0;
	}
	
//	for (i=0; i<3; i++) {
//		if (pdb->ct.ct_dir[i]){
//			pmeter->Iangle[i] = fmod(pmeter->Iangle[i]+180, 360);
//		}
//	}
}

// cid: channel id, accm : watt/period
int scalePower(int id, long val)
{
  float watt;

	watt = (val * meter[id].cntl.wscale)/PWR_MAX;
	return watt;
}

// Power accumulation register = ROUNDDOWN(USER_POWER_ACCUMULATION(8000)/8192)
float scalePowerAcc(int id, int32_t raw) {
	//float pwr = (raw * meter[id].cntl.wscale)/PWR_MAX*8192/8000;
	float pwr = (raw * meter[id].cntl.wscale)/PWR_MAX;
	return pwr;
}

//cid:channelid,accm:watt/period
float scaleEnergy(int id, int32_t raw)
{
	//float pwr = (raw * meter[id].cntl.wscale)/PWR_MAX*(8192./8000)/3600.;
	float pwr = (raw * meter[id].cntl.wscale)/PWR_MAX/3600.;
	return pwr;
}

float scalePercent(long val) 
{
	float v = val * PERCENT_SCALE;
	return v;
}

//float scaleVrms(long val)
//{
//	float volt = val*meter[id].cntl.vscale*meter[id].cntl.ptratio;
//	return volt;
//}

// 소수점 첫째자리까지 구한다
//float scaleIrms(long val)
//{
//	float amp = val*meter[id].cntl.iscale*meter[id].cntl.ctratio;
//	return amp;
//}


//void incAppEnergy(double*dst,doublediff,doublelimit)
void incAppEnergy(float *dst,float diff)
{
	*dst+=diff;
}

//1)import,export,total,network모두연산으로하는방식
//2)import,export만누적하고total,network은연산으로하는방식


void incEnergy(float diff, float *pAcc)
{
	if (diff > 0) 
		pAcc[0] += diff;
	else 
		pAcc[1] += -diff;
}

//float calcPower(int ienergy,float freq)
//{
//	float pwr;
//	
//	if(pdb->CT2==CT_5A){
//		pwr=ienergy*3600./E_INTERVAL*ALPHA_CONST_5A*freq*meter[id].cntl.wscale*meter[id].cntl.wh_scale;
//	}
//	else if(pdb->CT2==CT_RCT){
//		pwr=ienergy*3600./E_INTERVAL*ALPHA_CONST_RCT*freq*meter[id].cntl.wscale*meter[id].cntl.wh_scale;
//	}
//	else{
//		pwr=ienergy*3600./E_INTERVAL*ALPHA_CONST_100mA*freq*meter[id].cntl.wscale*meter[id].cntl.wh_scale;
//	}
//	//NoLoadthreshold보다작은값은버린다
//	return (fabs(pwr)>=_apNoLoadList[0])?pwr:0;
//}

//float radians(float ang) {
//	return ang*M_PI/180.;
//}

//float degrees(float rad) {
//	return rad*180/M_PI;
//}


void getSeqComp(float *x, float *y, float *comp) {
	float X, Y, a;
	
	X = (x[0]+x[1]+x[2]);
	Y = (y[0]+y[1]+y[2]);
	
	comp[0] = sqrt(X*X+Y*Y)/3;
	
	if (X == 0) {
		a = 0;
	}
	else {
		a = degrees(atan(Y/X));	
		if (X==0 && Y>=0) {
			comp[1] = a;
		}
		else if (X>=0 && Y<0) {
			comp[1] = 360 + a;	
		}
		else if (X<0) {
			comp[1] = 180 + a;
		}
	}
}


// seq: zsm, zsa, psm, psa, nsm, nsa
const float xu=-0.5, yu=0.866025404, xu2=-0.5, yu2=-0.866025404;
const float rad120=2.094395102, rad240=4.188790205;

void calcSeqComponent(float *mag, float *angle, float *seq) {	
	float x[3], y[3], rad[3];
	float Up2[2], Useq2[2], Up3[2], Useq3[2];
	float Q0x[2], Q1x[2], Q2x[2];
	int i;
					
	// 크기가 0이면 계산하지 않는다 
	for (i=0; i<3; i++) {
		rad[i] = radians(angle[i]);
		x[i]   = mag[i] * cos(rad[i]);
		y[i]   = mag[i] * sin(rad[i]);		
	}
	Up2[0]   = mag[1] * cos(rad120+rad[1]);	// 120 + VbAngle
	Up2[1]   = mag[1] * sin(rad120+rad[1]);
	Useq2[0] = mag[1] * cos(rad240+rad[1]);	// 240 + VbAngle
	Useq2[1] = mag[1] * sin(rad240+rad[1]);
	Up3[0]   = mag[2] * cos(rad120+rad[2]);	// 120 + VcAngle
	Up3[1]   = mag[2] * sin(rad120+rad[2]);
	Useq3[0] = mag[2] * cos(rad240+rad[2]);	// 240 + VcAngle
	Useq3[1] = mag[2] * sin(rad240+rad[2]);

	Q0x[0] = (x[0] + x[1] + x[2])/3;
	Q0x[1] = (y[0] + y[1] + y[2])/3;
	Q1x[0] = (x[0] + Up2[0] + Useq3[0])/3;
	Q1x[1] = (y[0] + Up2[1] + Useq3[1])/3;
	Q2x[0] = (x[0] + Useq2[0] + Up3[0])/3;
	Q2x[1] = (y[0] + Useq2[1] + Up3[1])/3;	

	// zero
	seq[0] = sqrt(Q0x[0] * Q0x[0] + Q0x[1] * Q0x[1]);	
	rad[0] = (Q0x[0] == 0) ? 0 : atan(Q0x[1] / Q0x[0]);
	seq[1] = degrees(rad[0]);
	// pos
	seq[2] = sqrt(Q1x[0] * Q1x[0] + Q1x[1] * Q1x[1]);
	rad[1] = (Q1x[0] == 0) ? 0 : atan(Q1x[1] / Q1x[0]);
	seq[3] = degrees(rad[1]);
	// neg.
	seq[4] = sqrt(Q2x[0] * Q2x[0] + Q2x[1] * Q2x[1]);	
	rad[2] = (Q2x[0] == 0) ? 0 : atan(Q2x[1] / Q2x[0]);
	seq[5] = degrees(rad[2]);
}

float calcUnbalance(float *seq, float *bal)
{
	float un, uo, check;
#ifdef	SAMTEST
	if(seq[0]>100){
		bal[0] = fabs(seq[0]-pmeter->U[3])/pmeter->U[3]*100;
		bal[1] = 0;
	}
	else {
		// 0: zero, 2:pos, 4:neg	
		if (seq[2] == 0) {
			// bal[0] = 100;
			// bal[1] = 300;
			// ISKRA : Uu = 100, Iu= 0,    ACCURA : Uu = 0, Iu= 0
			bal[0] = 0;
			bal[1] = 0;
		}
		else {
			un = seq[4]/seq[2];	// Uu or Iu (neg/pos)		
			bal[0] = (un > 1) ? 100 : un*100;

			uo = seq[0]/seq[2];	// Uo or Io	(zero/pos);	
			bal[1] = (uo > 3) ? 300 : uo*100;
		}
	}
#else
	// 0: zero, 2:pos, 4:neg	
	if (seq[2] == 0) {
		// bal[0] = 100;
		// bal[1] = 300;
		// ISKRA : Uu = 100, Iu= 0,    ACCURA : Uu = 0, Iu= 0
	 	bal[0] = 0;
		bal[1] = 0;
	}
	else {
		un = seq[4]/seq[2];	// Uu or Iu (neg/pos)		
		bal[0] = (un > 1) ? 100 : un*100;

		uo = seq[0]/seq[2];	// Uo or Io	(zero/pos);	
		bal[1] = (uo > 3) ? 300 : uo*100;
	}
#endif	
}

void calcDeviation(int id, float *U, float norm) {
	int i;
	float dev;
	
	for (i=0; i<3; i++) {
		if (U[i] < norm) {
			meter[id].meter.UUndev[i] = (norm - U[i])/norm*100;
		}
		else if (U[i] > norm) {
			meter[id].meter.UOvdev[i] = (U[i]-norm)/norm*100;
		}
		else {
			meter[id].meter.UUndev[i] = 0;
			meter[id].meter.UOvdev[i] = 0;
		}
	}
}

//void calcEnergy(int64_t *e64,int32_t *e32)
//{
//	int64_t v;
//	int i;
//	
//	//누적
//	//total=import+export
//	v=e64[0]+e64[1];
//	e64[2] = (v>MAX_EREG64) ? v-99999999999LL:v;
//	//network=import-network
//	e64[3]=e64[0]-e64[1];	
//	
//	//현월
//	//total=import+export
//	v=e64[4]+e64[5];
//	e64[6]=(v>99999999999LL)?v-99999999999LL:v;
//	//network=import-network
//	e64[7]=e64[4]-e64[5];		
//	
//	for(i=0;i<4;i++){
//		e32[i]=e64[i]/100;
//		e32[i+4]=e64[i+4]/100;
//	}
//}

//// 2017-9-27 추가
//void calcEnergy2(int64_t *e64,int32_t *e32)
//{
//	int64_t v;
//	int i;
//			
//	e32[0]=e64[0]/100;
//	e32[1]=e64[1]/100;
//}


//유효,무효, 피상 전력갱신
//int updateEh(int mode, float diff) {
//	int i, dir, ret=0, dt;
//	uint32_t ieh;
//	
//	if (diff > 0) {
//		dir = 0;
//	}
//	else if (diff < 0) {
//		dir = 1;
//		diff = -diff;
//	}
//	else {
//		return ret;
//	}
//	
//	// 1w 이상인 값을 추출한다
//	meter[id].cntl.Ereg.eh[mode][dir] += diff;
//	dt = meter[id].cntl.Ereg.eh[mode][dir];
//	if (dt) {
//		meter[id].cntl.Ereg.eh[mode][dir] -= dt;
//		ret = 1;
//	
//		// 누적, 현월 반영		
//		for (i=0; i<2; i++) {											
//			pEgyNvr->Ereg[i].eh[mode][dir] += dt;	
//			if (pEgyNvr->Ereg[i].eh[mode][dir] >= 99999999999LL) {
//				pEgyNvr->Ereg[i].eh[mode][dir] -= 99999999999LL;
//			}		
//			// 100w 이상 변할때 마다 저장한다 
//			ieh = pEgyNvr->Ereg[i].eh[mode][dir]/100;		
//			if (pEgy->Ereg[i].eh[mode][dir] != ieh) {			
//				pEgy->Ereg[i].eh[mode][dir] = ieh;
//				ret += 100;
//			}
//		}
//	}
//	
//	return ret;	
//}


//int updateWh(float diff) {
//	int i, dir, ret=0, dt;
//	uint32_t ieh;
//	
//	if (diff > 0) {
//		dir = 0;
//	}
//	else if (diff < 0) {
//		dir = 1;
//		diff = -diff;
//	}
//	else {
//		return ret;
//	}
//	
//	// 1w 이상인 값을 추출한다
//	meter[id].cntl.Ereg.kwh[dir] += diff;
//	dt = meter[id].cntl.Ereg.kwh[dir];
//	if (dt) {
//		meter[id].cntl.Ereg.kwh[dir] -= dt;
//	}
//	
//	// 누적, 현월 반영		
//	for (i=0; i<2; i++) {											
//		pEgyNvr->Ereg[i].kwh[dir] += dt;	
//		if (pEgyNvr->Ereg[i].kwh[dir] >= 99999999999LL) {
//			pEgyNvr->Ereg[i].kwh[dir] -= 99999999999LL;
//		}		
//		// 100w 이상 변할때 마다 저장한다 
//		ieh = pEgyNvr->Ereg[i].kwh[dir]/100;		
//		if (pEgy->Ereg[i].kwh[dir] != ieh) {			
//			pEgy->Ereg[i].kwh[dir] = ieh;
//			ret = 1;
//		}
//	}
//	
//	return ret;
//}

//int updateVarh(float diff) {
//	int i, dir, ret=0, dt;
//	uint32_t ieh;
//	
//	if (diff > 0) {
//		dir = 0;
//	}
//	else if (diff < 0) {
//		dir = 1;
//		diff = -diff;
//	}
//	else {
//		return ret;
//	}
//	
//	// 1w 이상인 값을 추출한다
//	meter[id].cntl.Ereg.kvarh[dir] += diff;
//	dt = meter[id].cntl.Ereg.kvarh[dir];
//	if (dt) {
//		meter[id].cntl.Ereg.kvarh[dir] -= dt;
//	}	
//			
//	for (i=0; i<2; i++) {											
//		pEgyNvr->Ereg[i].kvarh[dir] += diff;	// 누적, 현월 반영
//		if (pEgyNvr->Ereg[i].kvarh[dir] >= 99999999999LL) {
//			pEgyNvr->Ereg[i].kvarh[dir] -= 99999999999LL;
//		}
//		
//		ieh = pEgyNvr->Ereg[i].kvarh[dir]/100;
//		if (pEgy->Ereg[i].kvarh[dir] != ieh) {
//			pEgy->Ereg[i].kvarh[dir] = ieh;
//			ret = 1;
//		}
//	}
//	
//	return ret;
//}

//int updateVAh(float diff) {
//	int i, ret=0, dt;
//	uint32_t ieh;
//	
//	if (diff == 0) 
//		return ret;
//	
//	// 1w 이상인 값을 추출한다
//	meter[id].cntl.Ereg.kvah += diff;
//	dt = meter[id].cntl.Ereg.kvah;
//	if (dt) {
//		meter[id].cntl.Ereg.kvah -= dt;
//	}		
//	
//	for (i=0; i<2; i++) {											
//		pEgyNvr->Ereg[i].kvah += diff;	// 누적, 현월 반영
//		if (pEgyNvr->Ereg[i].kvah >= 99999999999LL) {
//			pEgyNvr->Ereg[i].kvah -= 99999999999LL;
//		}
//		
//		ieh = pEgyNvr->Ereg[i].kvah/100;
//		if (pEgy->Ereg[i].kvah != ieh) {
//			pEgy->Ereg[i].kvah = ieh;
//			ret = 1;
//		}			
//	}		
//	return ret;
//}


void calcDPF(int id, int mode) {
	int i;
	float dPF;
	
	for (i=0; i<4; i++) {
		if (meter[id].cntl.fP[i] == 0) {
			dPF = 0;
		}
		else {
			dPF = fabs(meter[id].cntl.fP[i]/meter[id].cntl.fS[i]);
		}
			
		//IEC : (P >= 0) ? positive : negative;
		if (mode == 0) {			
			dPF = (meter[id].cntl.P[i] < 0) ? -dPF : dPF;
		}
		//IEEE: (P*Q >= 0) ? negative : positive
		else {
			dPF = (meter[id].cntl.P[i]*meter[id].cntl.Q[i] >= 0) ? -dPF : dPF; 
		}		
		meter[id].meter.dPF[i] = dPF;
	}	
}

//순간전력으로역률계산시낮은전류흔들리는문제발생한다
//순간전력,평균전력선택할수있게변경
void calcPF(float *P, float *Q, float *S, float *PF, int mode)
{
	int i;
	float pf;

	for (i=0;i<4;i++) {
		pf = (P[i] == 0 || S[i] == 0) ? 0 : fabs(P[i]/S[i]*100);						
		pf = (pf > 100) ? 100 : pf;
		
		//IEC : (P >= 0) ? positive : negative;		
		if (mode == 0) {			
			PF[i] = (P[i] >= 0) ? pf : -pf;
		}
		//IEEE: (P*Q >= 0) ? negative(Inductive Load) : positive(Capacitive Load)
		else {
			PF[i] = (P[i]*Q[i] >= 0) ? -pf : pf;
		}			
	}
}


// Distortion Power
void calcD(float *P, float *fQ, float *S, float *D) {
	int i;
	float sum, sq;
	
	for (i=0; i<4; i++) {
		sq = S[i]*S[i] - P[i]*P[i] - fQ[i]*fQ[i];
		D[i] = (sq >= 0) ? sqrt(sq) : 0;
	}
}


// Voltage balance

float safe_fmod(float x, float y) {
    if (isnan(x) || isnan(y) || y == 0) {
        return 0;  // 기본값 설정
    }
    return fmod(x, y);
}


float calcLineVolt(int id, int ph)
{
	float v1,v2,angle, ret;
	
	//AB
	if(ph==0){
		v1=meter[id].meter.U[0];
		v2=meter[id].meter.U[1];
		angle=safe_fmod(meter[id].cntl.Uangle[1]+180,360);
	}
	//BC
	else if(ph==1){
		v1=meter[id].meter.U[1];
		v2=meter[id].meter.U[2];	
		angle=safe_fmod(meter[id].cntl.Uangle[0]+180,360);
	}
	//AC
	else{
		v1=meter[id].meter.U[2];
		v2=meter[id].meter.U[0];	
		angle=safe_fmod(meter[id].cntl.Uangle[1]+180,360);
	}
	
	ret = sqrt(v1*v1+v2*v2 + 2*v1*v2*cos(radians(angle)));
	
	return (ret < PICKUP_VOLT) ? 0 : ret;
}

void readPhaseTHD(uint8_t id) 
{
	int i, addr, ldata; 
	
	for (addr=0x217, i=0; i<3; i++, addr+=0x20) {	
		read_reg32(id, addr+0, (uint32_t *)&ldata);
		ade9000[id].vthd[i] = scalePercent(ldata) - pcal->v_thd_offset/10;
		read_reg32(id, addr+1, (uint32_t *)&ldata);
		ade9000[id].ithd[i] = scalePercent(ldata) - pcal->i_thd_offset/10;
	}
	
	meter[id].cntl.thdCalcF = 1;
#ifdef __FREERTOS	
	if (tid_post != 0) xTaskNotify(tid_post, 0x4, eSetBits);
#else
	if (tid_post != 0) os_evt_set(0x4, tid_post);
#endif	
}


//void reqTemp(uint8_t id) {
//	uint16_t wtemp;
//	// temperatue. enbale, start, 640ms
//	wtemp = (1<<3) | (1<<2) | (2);	
//	write_reg16(id, 0x4b6, &wtemp);
//}


void readTemp(uint8_t id) {
	uint16_t wtemp;
	uint32_t trim;
	float gain, os;
	
	// read temp trim(gain & offset)
	read_reg32(id, 0x474, &trim);
	gain = -((trim & 0xffff) / 65536.);	
	os   =  ((trim >> 16) & 0xffff) / 32.;
	
	// read temp_result
	read_reg16(id, 0x4b7, &wtemp);
	pmeter->Temp = (wtemp & 0xfff) * gain + os;
	//printf("readTemp : (%f)\n", pmeter->Temp);
	
	// restart
	wtemp = (1<<3) | (1<<2) | (2);	// start, enable, temp_time
	write_reg16(id, 0x4b6, &wtemp);	
}


void calcTHD(int id) {
	int i, sel=0;
	float	nh, nt, ih;
	CNTL_DATA	*_pcntl = &meter[id].cntl;

	for (i=0; i<3; i++) {

		if(meter[id].meter.I[i] >= _pcntl->I_start) {
			nh = meter[id].meter.THD_I[i]/100;					// 단위 고조파 전류
			nt = sqrt(1+ nh*nh);					// 단위토탈전류
			ih = nh*meter[id].meter.I[i]/nt;	// 실효고조파전류 = 단위고조파전류*실효토탈전류/단위토탈전류 = 단위고조파전류*전류비
			meter[id].meter.TDD_I[i] = ih/db[id].ct.inorm*100;	// 실효고조파전류/TDD정격전류
		}
		else {
			meter[id].meter.THD_I[i] = 0;
			meter[id].meter.TDD_I[i] = 0;
		}
	}	
	//maxMinTHD();	
}

//int64_t _makeEnergyReg(uint32_t *reg) 
//{
//	// 음수 처리를 위해 32 data를 64 bit 변수에 치환한다 
//	int64_t energy = *(int *)&reg[1];
//	energy <<= 13;
//	energy |= reg[0];
//	return energy;
//}

void readPhaseEnergy(uint8_t id, METER_EH_REGS *peh) 
{
	int32_t i, addr;
	uint32_t *pegy = (uint32_t *)peh;
		
	// read wh, varh, vah, fwh, fvarh, fvah
	for (addr = 0x2e7, i=0; i<18; i++, addr+=0xa) {
		read_reg32(id, addr, (uint32_t *)&pegy[i]); 
	}
}

void readEnergy(uint8_t id)
{	
	readPhaseEnergy(id, ade9000[id].energy[ade9000[id].efr]);
	ade9000[id].efr ^= 1;
#ifdef __FREERTOS	
	if (tid_energy != 0) xTaskNotify(tid_energy, 0x1, eSetBits);
#else
	if (tid_energy != 0) os_evt_set(0x1, tid_energy);
#endif	
}

float calcPhaseCurrent(int id, int sel)
{
	float i1,i2,i3,angle;
	
	i1=meter[id].meter.I[0];
	i3=meter[id].meter.I[2];
	angle = (meter[id].cntl.Iangle[0]>180) ? 360-meter[id].cntl.Iangle[0] : meter[id].cntl.Iangle[0];
	i2=sqrt(i1*i1+i3*i3+2*i1*i3*cos(angle*M_PI/180.));
	return i2;
}


////피상전력갱신
//int updateEnergy2(float *energy,int64_t *dst64,int32_t *dst32)
//{
//	int i,ret=0;
//	uint32_t diff;
//	int64_t v;
//	
//	//1W이상변화가발생하면EnergyReg(64bit)에반영한다
//	diff=(int32_t)energy[0];
//	if(diff>0){
//		energy[0]-=diff;
//		//누적
//		v=dst64[0]+diff;
//		dst64[0]=(v>99999999999LL)?v-99999999999LL:v;
//		//금월
//		v=dst64[1]+diff;
//		dst64[1]=(v>99999999999LL)?v-99999999999LL:v;			
//		ret=1;
//	}
//	
//	// 2017-9-27 추가 (실시간으로 화면에 누적 안된다)
//	if(ret){
//		calcEnergy2(dst64,dst32);
//	}	
//	return ret;
//}


void configWave8kCapture(int id) {
	uint16_t wtemp;
	// set cfg
	// WF_SRC(8k, xI, xV), WF_MODE(Cont), WF_CAP_SEL(Fixed), BURST_CHAN(All)
	wtemp = (3<<8) | (3<<6) | (1<<5);	
	write_reg16(id, AD9X_WFB_CFG, &wtemp);	
}

void configWave32kCapture(int id) {
	uint16_t wtemp;
	// set cfg
	// WF_SRC(32K, Sinc4), WF_MODE(Cont), WF_CAP_SEL(Fixed), BURST_CHAN(All)
	wtemp = (0<<8) | (3<<6) | (1<<5);	
	write_reg16(id, AD9X_WFB_CFG, &wtemp);	
}


void enableCapture(int id)
{
	uint16_t wtemp;
	
	// start
	read_reg16(id, AD9X_WFB_CFG, &wtemp);
	wtemp |= (1<<4);
	write_reg16(id, AD9X_WFB_CFG, &wtemp);
	
	printf("WFB_CFG: %x\n", wtemp);
}		


void disableCapture(int id)
{
	uint16_t wtemp;
	
	wtemp = (3<<8) | (3<<6) | (1<<5);	// Continuous fill
	write_reg16(id, AD9X_WFB_CFG, &wtemp);	
}





//void readPhaseFreq(ADE9000_REG *pu) {
//	int i, addr;
//	uint16_t wtemp;
//	uint32_t dtemp;
//	float ftemp = FREQ_CONST;
//	
//	addr =0x418;
//	for (i=0; i<3; i++) {
//		read_reg32(id, addr++, &dtemp);
//		pu->freq[i] = ftemp/(dtemp+1);
//	}
//	//pu->freq = pu->_freq[0];
//}


// read xWATT_ACC, xVAR_ACC, xVA_ACC, ...
void readPhasePower(uint8_t id) {
	int i, addr=0x2e5;

	for (i=0; i<3; i++) {
		read_reg32(id, addr, (uint32_t *)&ade9000[id].watt[i]);	
		addr+= 0xa;
		read_reg32(id, addr, (uint32_t *)&ade9000[id].var[i]); 
		addr += 0xa;
		read_reg32(id, addr, (uint32_t *)&ade9000[id].va[i]);
		addr += 0xa;
		read_reg32(id, addr, (uint32_t *)&ade9000[id].fwatt[i]);	
		addr += 0xa;
		read_reg32(id, addr, (uint32_t *)&ade9000[id].fvar[i]); 
		addr += 0xa;
		read_reg32(id, addr, (uint32_t *)&ade9000[id].fva[i]);	
		addr += 0xa;		
	}
	
	meter[id].cntl.pwrCalcF = 1;
#ifdef __FREERTOS	
	if (tid_post != 0) xTaskNotify(tid_post, 0x2, eSetBits);
#else
	if (tid_post != 0) os_evt_set(0x2, tid_post);
#endif	
}

void calcPower(int id) {
	int i;
	
	for (i=0; i<3; i++) {		
		meter[id].cntl.P[i]  = (meter[id].cntl.I[i] == 0) ? 0 : scalePowerAcc(id, ade9000[id].watt[i]);
		meter[id].cntl.Q[i]  = (meter[id].cntl.I[i] == 0) ? 0 : scalePowerAcc(id, ade9000[id].var[i]);
		meter[id].cntl.S[i]  = (meter[id].cntl.I[i] == 0) ? 0 : scalePowerAcc(id, ade9000[id].va[i]);	
		meter[id].cntl.fP[i] = (meter[id].cntl.I[i] == 0) ? 0 : scalePowerAcc(id, ade9000[id].fwatt[i]);
		meter[id].cntl.fQ[i] = (meter[id].cntl.I[i] == 0) ? 0 : scalePowerAcc(id, ade9000[id].fvar[i]);
		meter[id].cntl.fS[i] = (meter[id].cntl.I[i] == 0) ? 0 : scalePowerAcc(id, ade9000[id].fva[i]);
	}
	
	for (i=0; i<3; i++) {
		switch(db[id].pt.wiring){
		//1LN : 2,3 전력을 지운다
		case WM_1LN1CT:
			if(i!=0){
				meter[id].cntl.P[i] = meter[id].cntl.Q[i] = meter[id].cntl.S[i] = 0;
				meter[id].cntl.fP[i] = meter[id].cntl.fQ[i] = meter[id].cntl.fS[i] = 0;
			}
			break;

		//3LL,1LL:2번 전력 지운다
		case WM_3LL3CT:
		case WM_3LL2CT:
		case WM_1LL2CT:
			if(i==1){
				meter[id].cntl.P[i] = meter[id].cntl.Q[i] = meter[id].cntl.S[i] = 0;
				meter[id].cntl.fP[i] = meter[id].cntl.fQ[i] = meter[id].cntl.fS[i] = 0;
			}
			break;
		}
				
		// CT direction 보정
		if(db[id].ct.ct_dir[i]){
				meter[id].cntl.P[i] = -meter[id].cntl.P[i];
				meter[id].cntl.Q[i] = -meter[id].cntl.Q[i];
				meter[id].cntl.fP[i] = -meter[id].cntl.fP[i];
				meter[id].cntl.fQ[i] = -meter[id].cntl.fQ[i];
		}			
	}
	
	
	for (i=0; i<3; i++) {
		if (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT) {
			if (db[id].etc.VA_type == 0) {
				meter[id].cntl.S[i] = meter[id].cntl.S[i]*sqrt(3)/2.;
				meter[id].cntl.fS[i] = meter[id].cntl.fS[i]*sqrt(3)/2.;
			}
			else {
				meter[id].cntl.S[i] = sqrt(meter[id].cntl.P[i]*meter[id].cntl.P[i]+meter[id].cntl.Q[i]*meter[id].cntl.Q[i])*sqrt(3)/2.;
				meter[id].cntl.fS[i] = sqrt(meter[id].cntl.fP[i]*meter[id].cntl.fP[i]+meter[id].cntl.fQ[i]*meter[id].cntl.fQ[i])*sqrt(3)/2.;
			}
		}
		else {
			if (db[id].etc.VA_type == 0) {
			}
			else {
				meter[id].cntl.S[i] = sqrt(meter[id].cntl.P[i]*meter[id].cntl.P[i]+meter[id].cntl.Q[i]*meter[id].cntl.Q[i]);
				meter[id].cntl.fS[i] = sqrt(meter[id].cntl.fP[i]*meter[id].cntl.fP[i]+meter[id].cntl.fQ[i]*meter[id].cntl.fQ[i]);
			}
		}
	}

	meter[id].cntl.P[3] = summation(meter[id].cntl.P, 3);	
	meter[id].cntl.Q[3] = summation(meter[id].cntl.Q, 3);	
	meter[id].cntl.S[3] = summation(meter[id].cntl.S, 3);	
	meter[id].cntl.fP[3] = summation(meter[id].cntl.fP, 3);	
	meter[id].cntl.fQ[3] = summation(meter[id].cntl.fQ, 3);	
	meter[id].cntl.fS[3] = summation(meter[id].cntl.fS, 3);		
	
	// meter영역으로 카피	
	for (i=0; i<4; i++) {
//		if(pdb->wiring==WM_1LN1CT){
//			if (i==1 || i==2) continue;
//		}
//		//1P3W:1,3,Total
//		else if(pdb->wiring==WM_1LL2CT){
//			if (i==1) continue;	
//		}
//		//3LL:Total만유효, 2017-9-27 수정(WM_3LL2CT 누락)
//		else if(pdb->wiring == WM_3LL3CT || pdb->wiring == WM_3LL2CT){
//			if (i!=3) continue;
//		}		
		meter[id].meter.P[i] = meter[id].cntl.P[i];
		meter[id].meter.Q[i] = meter[id].cntl.Q[i];
		meter[id].meter.S[i] = meter[id].cntl.S[i];
		meter[id].meter.fP[i] = meter[id].cntl.fP[i];
		meter[id].meter.fQ[i] = meter[id].cntl.fQ[i];
		meter[id].meter.fS[i] = meter[id].cntl.fS[i];		
	}
	
	calcPF(meter[id].cntl.P, meter[id].cntl.Q, meter[id].cntl.S, meter[id].meter.PF, db[id].etc.PF_sign);	// PF
	calcPF(meter[id].cntl.fP, meter[id].cntl.fQ, meter[id].cntl.fS, meter[id].meter.dPF, db[id].etc.PF_sign);	// displacement PF
	calcD(meter[id].cntl.P, meter[id].cntl.fQ, meter[id].cntl.S, meter[id].meter.D);
	
	maxMinPower(id);
}

//void readPhaseAngle(ADE9000_REG *pchip, float lineFreq) {
//	int i, addr;
//	uint16_t wtemp;
//	uint32_t dtemp;
//	float ftemp = 8000*65536;
//	
//	addr = 0x482;
//	for (i=0; i<3; i++) {
//		read_reg16(id, addr++, &wtemp);	//AB
//		pchip->Uangle[i] = wtemp * ANGLE_CONST * lineFreq;
//	}
//	
//	for (i=0; i<3; i++) {
//		read_reg16(id, addr++, &wtemp);	//A
//		pchip->UangleI[i] = wtemp * ANGLE_CONST * lineFreq;
//	}

//	for (i=0; i<3; i++) {
//		read_reg16(id, addr++, &wtemp);	//AB
//		pchip->IangleI[i] = wtemp * ANGLE_CONST * lineFreq;
//	}
//	
//	// check voltage phase sequence : 
//	// Va-Vb(120), Va-Vc(24)  => Normal
//	// Va-Vb(240), Va-Vc(120) => Reverse Phase
//	if (pchip->Uangle[0] <= pchip->Uangle[2]) 
//		meter[id].cntl.revPhase = 0;
//	else
//		meter[id].cntl.revPhase = 1;
//	
//	calcConvAngle(0);
//	
////	printf("Freq    : %.2f\n", pu->freq);
//// 	printf("VVangle : %.2f, %.2f, %.2f\n", pu->Uangle[0], pu->Uangle[1], pu->Uangle[2]);
////	printf("VIangle : %.2f, %.2f, %.2f\n", pu->UangleI[0], pu->UangleI[1], pu->UangleI[2]);
////	printf("IIangle : %.2f, %.2f, %.2f\n", pu->IangleI[0], pu->IangleI[1], pu->IangleI[2]);		
//// 	printf("VVangle : %.2f, %.2f, %.2f\n", pu->absUangle[0], pu->absUangle[1], pu->absUangle[2]);
////	printf("VIangle : %.2f, %.2f, %.2f\n", pu->absIangle[0], pu->absIangle[1], pu->absIangle[2]);	
//}

//void readPhaseRMS_1012C(uint8_t id) {
//	int i, addr;
//	
//	for (addr=0x21b, i=0; i<3; i++, addr+=0x20) {
//		read_reg32(id, addr+0, &pchip->irms_1012c[i]);
//		read_reg32(id, addr+1, &pchip->vrms_1012c[i]);
//	}
//}


//void readPhaseRMS_1C(uint8_t id)
//{
//	int addr, i;
//	
//	for (addr = 0x219, i=0; i<3; i++, addr+=0x20) {
//		read_reg32(id, addr+0, &pchip->irms_1c[i]);
//		read_reg32(id, addr+1, &pchip->vrms_1c[i]);
//	}
//}

//void readPhasePeriod(uint8_t id) {
//	int i, addr;
//	
//	for (addr=0x418, i=0; i<3; i++, addr++) {
//		read_reg32(id, addr++, &pchip->period[i]);
//	}
//}

//void readPhaseAngle(uint8_t id) {
//	int i, addr;
//	
//	// read xANGLE
//	addr = 0x482;
//	for (i=0; i<3; i++) {
//		read_reg16(id, addr++, &pchip->Uangle[i]);	//Uab, Ubc, Uac
//	}
//	for (i=0; i<3; i++) {
//		read_reg16(id, addr++, &pchip->UIangle[i]);	//UIa, UIb, UIc
//	}
//	for (i=0; i<3; i++) {
//		read_reg16(id, addr++, &pchip->Iangle[i]);	//Iab, Ibc, Iac
//	}	
//}

//// 10/12 cycle RMS값을 읽는다
//void readPhaseRMS(uint8_t id) 
//{

//}


// 주파수 평균 계산위해 주파수 읽기 추가 
void readPeriod(uint8_t id) {
	int addr, i;
	
	if (meter[id].cntl.freqSel == 0) {
		// Period Register
//		for (addr=0x418, i=0; i<4; i++, addr++) {
//			read_reg32(id, addr, &pchip->freqFast[i]);
//		}	
		
		// COM_PERIOD를 사용한다 
		addr = 0x41b;
		read_reg32(id, addr, &ade9000[id].freqFast[0]);
		meter[id].cntl.freq1sum  += scaleFreq(ade9000[id].freqFast[0]);
	
		// 1초 평균주파수 계산 
		if (++meter[id].cntl.freqCount1s >= db[id].pt.freq) {
			//pmeter->Freq = (pmeter->U[3] == 0) ? 0 : meter[id].cntl.freq1sum/meter[id].cntl.freqCount1s;
			meter[id].meter.Freq = (meter[id].cntl.online == 0) ? 0 : meter[id].cntl.freq1sum/meter[id].cntl.freqCount1s;
			meter[id].cntl.freq1sum = 0;
			meter[id].cntl.freqCount1s = 0;
		}
	}
	
	meter[id].cntl.freqSel ^= 0x1;
}


void readPhaseFastRMS(uint8_t id) {
	int addr, i, ix;
	uint32_t rms;
	FAST_RMS *pFast;
	
	for (addr = 0x219, i=0; i<3; i++, addr+=0x20) {
		read_reg32(id, addr+0, &ade9000[id].irmsFast[i]);
		read_reg32(id, addr+1, &ade9000[id].urmsFast[i]);
	}		
		
	pFast = &rmsWin.buf[rmsWin.fr];
	ix = rmsWin.ix;
	for (i=0; i<3; i++) {
		pFast->I[i][ix] = ade9000[id].irmsFast[i];
		pFast->U[i][ix] = ade9000[id].urmsFast[i];	
	}
	
	
	if (ix == 0) {
		pFast->ts = sysTick64;
	}
	
	if (++ix >= meter[id].cntl.nFastRMS) {
		ix =0;
		if (++rmsWin.fr >= N_FASTRMS_BUF) rmsWin.fr = 0;
#ifdef __FREERTOS		
		if (tid_rmslog != 0) xTaskNotify(tid_rmslog, 0x1, eSetBits);
#else
		if (tid_rmslog != 0) os_evt_set(0x1, tid_rmslog);
#endif		
	}
	rmsWin.ix = ix;
}

// 10/12 cycle 단위로 호출된다(200ms) 
void readRmsAngle(uint8_t id)
{
	int i, err=0, addr, ix, ix2;
	float rms, noload, xa;
	METER_REGS *preg = &ade9000[id].sr[ade9000[id].fr];
	//staticint		cnt=0;
	
	// read RMS
	ix=preg->ix;	
	for (addr = 0x20c, i=0; i<3; i++, addr+=0x20) {
		read_reg32(id, addr+0, &preg->irms[i][ix]);
		read_reg32(id, addr+1, &preg->urms[i][ix]);
#ifdef	VFRMS
		read_reg32(id, addr+2, &preg->firms[i][ix]);
		read_reg32(id, addr+3, &preg->furms[i][ix]);
#endif
	}	
	read_reg32(id, 0x266, &preg->inrms[ix]);	
	read_reg32(id, 0x269, &preg->isrms[ix]);	

	// read 10/12 RMS, 전압만 읽는다
	ix=preg->ix;	
	for (addr = 0x21c, i=0; i<3; i++, addr+=0x20) {
		read_reg32(id, addr, &preg->urms_1012[i][ix]);
	}	

//	// period는 fast rms에서 읽는다 
//	// read Period
//	for (addr=0x418, i=0; i<3; i++, addr++) {
//		read_reg32(id, addr++, &preg->period[i][ix]);
//	}	
				
	// read Angle
#ifdef _ANGLE_AVERAGE			
	// Angle 평균을 복소수로 변환 후 평균하고 atan2를 이용하여 degree로 변환한다 
	// angle
	readPhaseAngle(id);
	for (i=0; i<3; i++) {
		float a;
		getComplexSum(pchip->Uangle[i], meter[id].cntl.UangleSum[i]);
		getComplexSum(pchip->UIangle[i], meter[id].cntl.UIangleSum[i]);
		getComplexSum(pchip->Iangle[i], meter[id].cntl.IangleSum[i]);
	}	
#else
	// read xANGLE, Zx Detection 되지 않으면 Angle 갱신되지 않는다 
	addr = 0x482;
	for (i=0; i<3; i++) {
		read_reg16(id, addr++, &preg->Uangle[i][ix]);	//Uab, Ubc, Uac
	}
	for (i=0; i<3; i++) {
		read_reg16(id, addr++, &preg->UIangle[i][ix]);	//UIa, UIb, UIc
	}
	for (i=0; i<3; i++) {
		read_reg16(id, addr++, &preg->Iangle[i][ix]);	//Iab, Ibc, Iac
	}		
#endif	
	
	if (++ix >= 5) {
		ix = 0;
		ade9000[id].fr ^= 1;
		meter[id].cntl.rmsCalcF = 1;
#ifdef __FREERTOS		
		if (tid_post != 0) xTaskNotify(tid_post, 0x1, eSetBits);
#else
		if (tid_post != 0) os_evt_set(0x1, tid_post);
#endif		
	}
	if(++ix2 >= 10)
		ix2 = 0;

	preg->ix = ix;
	preg->ix2 = ix2;

}

int  average_without_min_max(uint32_t numbers[], int length) {
	int				i=0;
	int				sum = 0, val=0;
	int	min = numbers[0];
	int	max = numbers[0];

    for (i = 0; i < length; i++) {
        if (numbers[i] < min) {
            min = numbers[i];
        }
        if (numbers[i] > max) {
            max = numbers[i];
        }
        sum += numbers[i];
    }
    val =  sum - min - max;
	return val;
}

	
// PostScan_Task 1초 마다 실행
void calcRmsAngle(int id) {
	int i, j, sum[3], insum, issum, mean;
#ifdef	VFRMS
	int fsum[3], fmean;
#endif
	float rms, fv;
	METER_REGS *preg = &ade9000[id].sr[ade9000[id].re];
	
	if (meter[id].cntl.rmsInit) {
		
		meter[id].cntl.Isum = 0;
		for (i=0;i<3;i++){
			// average U-RMS
			for (sum[i]=0, j=0; j<5; j++) {
				sum[i] += preg->urms[i][j];
				fsum[i] += preg->furms[i][j];
			}
			// 2020-4-3 : PT ratio 적용전에 pickup volt 보다 낮은값을 버린다
			//rms = scaleVrms(sum[i]/5);
			//meter[id].cntl.U[i] = (rms < PICKUP_VOLT) ? 0 : rms;	-> PT ration 곱하기 전에 처리한다 			
			mean = sum[i]/5;
			meter[id].cntl.U[i] = (mean < PICKUP_VOLT_RAW) ? 0 : scaleVrms(id, mean);
			fmean = fsum[i]/5;
			meter[id].cntl.fU[i] = (fmean < PICKUP_VOLT_RAW) ? 0 : scaleVrms(id, fmean);

			// average U-RMS-1012c
			for (sum[i]=0, j=0; j<5; j++) {
				sum[i] += preg->urms_1012[i][j];
			}			
			// 2020-4-3 : PT ratio 적용전에 pickup volt 보다 낮은값을 버린다
			//rms = scaleVrms(sum[i]/5);
			//meter[id].cntl.U_1012[i] = (rms < PICKUP_VOLT) ? 0 : rms;			
			mean = sum[i]/5;
			meter[id].cntl.U_1012[i] = (mean < PICKUP_VOLT_RAW) ? 0 : scaleVrms(id, mean);
		
			// average I-RMS
			for (sum[i]=0, j=0; j<5; j++) {
				sum[i] += preg->irms[i][j];
				fsum[i] += preg->firms[i][j];
			}		
			
			// 2020-4-3 : CT ratio 적용전에 noload 보자 낮은값 버린다 
			//rms = scaleIrms(sum[i]/5);
			//meter[id].cntl.I[i] = (rms < meter[id].cntl.inoload) ? 0 : rms;
			mean = sum[i]/5;					
			meter[id].cntl.I[i] = (mean < meter[id].cntl.inoload) ? 0 : scaleIrms(id, mean);
			meter[id].cntl.Isum += meter[id].cntl.I[i];
			fmean = fsum[i]/5;
			meter[id].cntl.fI[i] = (fmean < meter[id].cntl.inoload) ? 0 : scaleIrms(id, fmean);	

		}
		
		meter[id].cntl.Isum = rms;
		// Isum = IA + IB + IC
		if(meter[id].cntl.Isum == 0.) {
			meter[id].cntl.In = meter[id].cntl.Is = 0;
		}
		else {
			insum = average_without_min_max(preg->inrms, 10);
			meter[id].cntl.In1 = scaleIgrms1(id, insum/8);	
			meter[id].cntl.In = scaleIgrms(id, insum/8);		// 3P4W에서는 Neutral Current 이지만, 3P3W에서는 누설전류가 된다 
			issum = average_without_min_max(preg->isrms, 10);
			meter[id].cntl.Is = scaleIrms(id, issum/8);			// 일반전류와 같은 scale 사용한다 
		}
							
		// Freq		
		// 10s 단위로 sampling 한다 
//		for (i=0; i<3; i++) {		
//			for (sum[i] = 0, j=0; j<5; j++) {
//				sum[i] += preg->period[i][j];
//			}					
//			meter[id].cntl.freq[i] = scaleFreq(sum[i]/5);
//		}			
//		pmeter->Freq = meter[id].cntl.freq[0];
		// 정전되면 주파수는 60 또는 50으로 읽힌다 
		// 주파수 평균 계산 위해 fast rms로 이동하낟 
//		pmeter->Freq = scaleFreq(preg->period[0][0]);
//		printf("freq = %f\n", pmeter->Freq);

		// angle
		for (i=0; i<3; i++) {
			float r;
			meter[id].cntl.Uangle[i] = getAngle(id, meter[id].cntl.sumMax, preg->Uangle[i]);
			meter[id].cntl.UIangle[i] = getAngle(id, meter[id].cntl.sumMax, preg->UIangle[i]);
			meter[id].cntl.Iangle[i] = getAngle(id, meter[id].cntl.sumMax, preg->Iangle[i]);
		}									
//		for (i=0; i<3; i++) {
//			meter[id].cntl.Uangle[i] = scaleAngle(preg->Uangle[i][0]);
//			meter[id].cntl.UIangle[i] = scaleAngle(preg->UIangle[i][0]);
//			meter[id].cntl.Iangle[i] = scaleAngle(preg->Iangle[i][0]);
//		}
		calcAbsAngle(id);

		//
		meter[id].meter.In = meter[id].cntl.In * (db[id].ct.zctScale / 1000.);	//Neutral Phase Current
		meter[id].meter.Isum = meter[id].cntl.Is;	//Neutral Phase Current	= 
		if(db[id].pt.wiring == WM_1LN1CT){
			meter[id].meter.I[0] = meter[id].cntl.I[0];
			meter[id].meter.I[1] = 0;
			meter[id].meter.I[2] = 0;
			meter[id].meter.I[3] = meter[id].meter.I[0];	//average
			meter[id].meter.Ibal[0] = meter[id].meter.Ibal[1] = 0;
			meter[id].meter.Isum=0;
			meter[id].meter.fI[0] = meter[id].cntl.fI[0];
			meter[id].meter.fI[1] = 0;
			meter[id].meter.fI[2] = 0;
			meter[id].meter.fI[3] = meter[id].meter.fI[0];	//average
		}
		else if(db[id].pt.wiring == WM_1LL2CT){
			meter[id].meter.I[0] = meter[id].cntl.I[0];
			meter[id].meter.I[1] = 0;
			meter[id].meter.I[2] = meter[id].cntl.I[2];		
			meter[id].meter.I[3] = (meter[id].meter.I[0]+meter[id].meter.I[2])/2;	
			meter[id].meter.Ibal[0] = meter[id].meter.Ibal[1] = 0;
			meter[id].meter.Isum=meter[id].cntl.Is;
			meter[id].meter.fI[0] = meter[id].cntl.fI[0];
			meter[id].meter.fI[1] = 0;
			meter[id].meter.fI[2] = meter[id].cntl.fI[2];	
			meter[id].meter.fI[3] = (meter[id].meter.fI[0]+meter[id].meter.fI[2])/2;
		}
		else {
			//3P3W(2CT)는 B상 전류를 벡터계산으로 생성한다
			meter[id].meter.I[0] = meter[id].cntl.I[0];			
			meter[id].meter.I[1] = (db[id].pt.wiring == WM_3LL2CT) ? calcPhaseCurrent(id, 1) : meter[id].cntl.I[1];		
			meter[id].meter.I[2] = meter[id].cntl.I[2];		
			meter[id].meter.I[3] = average(meter[id].meter.I, 3);
			meter[id].meter.Isum = meter[id].cntl.Is;	
#ifdef	VFRMS
			//3P3W(2CT)는 B상 전류를 벡터계산으로 생성한다
			meter[id].meter.fI[0] = meter[id].cntl.fI[0];			
			meter[id].meter.fI[1] = (db[id].pt.wiring == WM_3LL2CT) ? calcPhaseCurrent(id, 1) : meter[id].cntl.fI[1];		
			meter[id].meter.fI[2] = meter[id].cntl.fI[2];		
			meter[id].meter.fI[3] = average(meter[id].meter.fI, 3);
#endif

			//			calcSeqComponent(pmeter->I, pmeter->Iangle, pmeter->Izs);
	//			calcUnbalance(pmeter->Izs, pmeter->Ibal);
		}
		meter[id].meter.Itot = summation(meter[id].meter.I, 3);
		
		
		if (db[id].pt.wiring == WM_1LN1CT) {
			meter[id].meter.Ibal[0] = meter[id].meter.Ibal[1] = 0;
		}
		else if (db[id].pt.wiring == WM_1LL2CT) {
			meter[id].meter.Ibal[0] = meter[id].meter.Ibal[1] = fabs(meter[id].meter.I[3]-meter[id].cntl.I[0]) / meter[id].meter.I[3] * 100; 
		}	
		else {
			calcSeqComponent(meter[id].meter.I, meter[id].meter.Iangle, meter[id].meter.Izs);
			calcUnbalance(meter[id].meter.Izs, meter[id].meter.Ibal);	
		}
		
			
		//pmeter->U=meter[id].cntl.V
		if(db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT){
			//3P3W에서 phase order가 다르다
			meter[id].meter.U[0] = meter[id].cntl.U[0];	// V1 <- V12
			meter[id].meter.U[1] = meter[id].cntl.U[2];	// V2 <- V31
			meter[id].meter.U[2] = meter[id].cntl.U[1];	// V3 <- V32
			meter[id].meter.U[3] = average(meter[id].meter.U, 3);			
			meter[id].meter.fU[0] = meter[id].cntl.fU[0];	// V1 <- V12
			meter[id].meter.fU[1] = meter[id].cntl.fU[2];	// V2 <- V31
			meter[id].meter.fU[2] = meter[id].cntl.fU[1];	// V3 <- V32
			meter[id].meter.fU[3] = average(meter[id].meter.fU, 3);			
		}
		else if(db[id].pt.wiring==WM_1LN1CT){
			meter[id].meter.U[0] = meter[id].meter.U[3] = meter[id].cntl.U[0];
			meter[id].meter.U[1] = meter[id].meter.U[2] = 0;
			meter[id].meter.fU[0] =meter[id].meter.fU[3] = meter[id].cntl.fU[0];
			meter[id].meter.fU[1] = meter[id].meter.fU[2] = 0;
		}
		else if(db[id].pt.wiring==WM_1LL2CT){
			meter[id].meter.U[0] = meter[id].cntl.U[0];
			meter[id].meter.U[1] = 0;
			meter[id].meter.U[2] = meter[id].cntl.U[2];			
			meter[id].meter.U[3] = (meter[id].meter.U[0]+meter[id].meter.U[2])/2;				
			meter[id].meter.fU[0] = meter[id].cntl.fU[0];
			meter[id].meter.fU[1] = 0;
			meter[id].meter.fU[2] = meter[id].cntl.fU[2];			
			meter[id].meter.fU[3] = (meter[id].meter.fU[0]+meter[id].meter.fU[2])/2;				
		}
		else { // 3LN, 3CT
			meter[id].meter.U[0]=meter[id].cntl.U[0];
			meter[id].meter.U[1]=meter[id].cntl.U[1];
			meter[id].meter.U[2]=meter[id].cntl.U[2];		
			meter[id].meter.U[3] = average(meter[id].meter.U, 3);					
			meter[id].meter.fU[0]=meter[id].cntl.fU[0];
			meter[id].meter.fU[1]=meter[id].cntl.fU[1];
			meter[id].meter.fU[2]=meter[id].cntl.fU[2];		
			meter[id].meter.fU[3] = average(meter[id].meter.fU, 3);					
		}		
		
		// 전압 불평형률
		if (db[id].pt.wiring == WM_1LN1CT) {
			meter[id].meter.Ubal[0] = meter[id].meter.Ubal[1] = 0;
		}
		else if (db[id].pt.wiring == WM_1LL2CT) {
			meter[id].meter.Ubal[0] = meter[id].meter.Ubal[1] = fabs(meter[id].meter.U[3]-meter[id].meter.U[0]) / meter[id].meter.U[3] * 100;
		}
		else {
			// 전압, 전류  불평형율 계산 
			calcSeqComponent(meter[id].meter.U, meter[id].meter.Uangle, meter[id].meter.Uzs);
			calcUnbalance(meter[id].meter.Uzs, meter[id].meter.Ubal);	
		}
		
		//calcDeviation(pmeter->U, pdb->pt.vnorm);
		
		//3상4선이면 전압과 위상 이용하여 상간 전압 계산		
		if (db[id].pt.wiring == WM_3LN3CT){		
			//LineVoltage계산
			meter[id].meter.Upp[0] = calcLineVolt(id, 0);
			meter[id].meter.Upp[1] = calcLineVolt(id, 1);
			meter[id].meter.Upp[2] = calcLineVolt(id, 2);		
			meter[id].meter.Upp[3] = average(meter[id].meter.Upp, 3);		
			meter[id].meter.Ig = meter[id].meter.Isum;	
		}
		else if (db[id].pt.wiring == WM_3LL3CT || db[id].pt.wiring == WM_3LL2CT) {
			meter[id].meter.Upp[0] = meter[id].meter.U[0];	// U12
			meter[id].meter.Upp[1] = meter[id].meter.U[1];	// U23
			meter[id].meter.Upp[2] = meter[id].meter.U[2];	// U31		
			meter[id].meter.Upp[3] = average(meter[id].meter.Upp, 3);
#if 1
			fv = meter[id].meter.In;
			if(fv>2)
				fv = 2;
			meter[id].meter.Ig = fv;	
#endif
		}
		else if (db[id].pt.wiring == WM_1LL2CT) {
			meter[id].meter.Upp[0] = meter[id].meter.U[0];	//V1-V2
			meter[id].meter.Upp[1] = meter[id].meter.U[2];	//V2-V3
			meter[id].meter.Upp[2] = meter[id].meter.U[0] + meter[id].meter.U[2];	// V3-V1
			meter[id].meter.Upp[3] = meter[id].meter.Upp[2]/2;
			meter[id].meter.Ig = meter[id].meter.In;
		}
		//3상3선일경우선간전압=상전압
		else if (db[id].pt.wiring == WM_1LN1CT) {
			meter[id].meter.Upp[0] = meter[id].meter.Upp[3] = meter[id].meter.U[0];			
			meter[id].meter.Upp[1] = meter[id].meter.Upp[2] = 0;
			meter[id].meter.Ig = meter[id].meter.In;				
		}

		// Under/Over Deviation
		calcDeviation(id, meter[id].meter.U, db[id].pt.vnorm);		

#if 0	// 2025-3-20, updateDemandI() 중복 호출로 인한 demandI 계산 오류
		updateDemandI();
#endif		
		meter[id].cntl.mmChangeF += maxMinRmsFreq(id);		
		
		updateQualData();
	}
	else {
		meter[id].cntl.rmsInit = 1;		// 첫번째 읽히는 값은 버린다.		
	}
	
	ade9000[id].re ^= 1;
			
//		printf("V:%.2f, I:%.2f, W:%.2f, Var:%.2f, VA:%.2f, PF:%.2f, VTHD:%.2f, ITHD:%.2f, Freq:%.2f\n",
//			pmeter->U[0], pmeter->I[0],
//			pmeter->P[0], pmeter->Q[0], pmeter->S[0], 
//			pmeter->PF[0], pmeter->THD_U[0], pmeter->THD_I[0], pmeter->freq);
}


//void setTimeStamp(uint64_t *ts) {
//	*ts = sysTick64;
//}

WVPG *getWave8kBuf(WAVE_PGBUF *pWQ, int *pix) 
{
	static uint64_t lastTick;
	WVPG *pb = &pWQ->wb[pWQ->fr];
	
	if (++pWQ->fr >= PG8K_CNT) {
		//printf("end of buf, tick=%d\n", TimeTick-lastTick);	
		lastTick = sysTick64;
		pWQ->fr = 0;
	}
	
	*pix = pWQ->fr;
	return pb;
}

//void clearWave8kBuf() {
//	w8kQ.fr = w8kQ.re = 0;
//}
#define	_USE_WAVE8K_BUF

#ifdef _USE_WAVE8K_BUF
// read WFB with Fixed Data Rate
// page7과 page15가 차면 인터럽트가 발생한다
// data dump 시간 : 2ms
void readWFB8k_Data(int id) 
{
	uint16_t wtemp;
	WVPG *pwb;
	int i, page, bix, sp;
	uint64_t t1, t2;
	
	read_reg16(id, AD9X_WFB_TRIG_STAT, &wtemp);	
	page = wtemp >> 12;
	sp = (page == 7) ? 0 : 0x80*8;
	//printf("page=%d, last=%d\n", page, lastpage);	
	// page# 7, 15에서 인터럽트 발생한다
		
	t1 = sysTick64;
		
//	Board_LED_On(2);
	SSP_SSEL_Mode(id, 1);	// SSEL;Auto	
	
	for (i=0; i<8; i++, sp+=0x80) {
		pwb = getWave8kBuf(&wQ[id], &bix);		
		dma_read32n(id, 0x801+sp, (uint32_t *)pwb->buf, 128);		
		//read_reg32n(id, 0x801+(i*0x80), (uint32_t *)pwb, 128);		// polling 방식은 검증 완료		
		// 8k Sample : 1024 sample을 채우면 데이터 처리 태스크 깨운다, 1024/16 -> 64 pag
		if (meter[id].cntl.online) wQ[id].olc++;
		if (wQ[id].count >= 400) {
			// Transient task를 깨운다
			wQ[id].lastolc = wQ[id].olc;
			wQ[id].count = wQ[id].olc = 0;
			wQ[id].ts = sysTick64-200;	// 시작시간 계산 : 완료시간-200ms

			if (tid_wave[id] != 0) {
#ifdef __FREERTOS			
				if (tid_wave[id] != 0) xTaskNotify(tid_wave[id], 0x1, eSetBits);
#else
				if (tid_wave[id] != 0) os_evt_set(0x1, tid_wave[id]);
#endif				
			}			
		}
	}	
	SSP_SSEL_Mode(id, 0);	// SSEL:Manual
//	Board_LED_Off(1);
	
	t2 = sysTick64;
	if (t2-t1 > 10) printf("tick = %d\n", (int)(t2-t1));
}
#endif

WVPG *getWave32kBuf(WAVE_PGBUF *pWQ) 
{
	WVPG *pb = &pWQ->wb[pWQ->fr];
	if (++pWQ->fr >= PG_BUF_CNT) {
		pWQ->fr = 0;
	}	
	pWQ->count++;
	return pb;
}

void readWFB32k_Data(int id) 
{
	uint16_t wtemp;
	WVPG *pwb;
	int i, page, sp;
	uint64_t t1, t2;
	
	read_reg16(id, AD9X_WFB_TRIG_STAT, &wtemp);	
	page = wtemp >> 12;
	sp = (page == 7) ? 0 : 0x80*8;
	//printf("page=%d, last=%d\n", page, lastpage);	
	// page# 7, 15에서 인터럽트 발생한다
		
	t1 = sysTick64;
	
	//Board_LED_On(1);	// 2.4ms
	SSP_SSEL_Mode(id, 1);
	for (i=0; i<8; i++, sp+=0x80) {
		pwb = getWave32kBuf(&wQ[id]);		
		dma_read32n(id, 0x801+sp, (uint32_t *)pwb->buf, 128);		
		//read_reg32n(id, 0x801+(i*0x80), (uint32_t *)pwb, 128);		// polling 방식은 검증 완료		
		// 32k Sample : 640(50Hz) or 533(60Hz) sample을 채우면 데이터 처리 태스크 깨운다 

		//if (bix == 256 && w1280.fr == w1280.re) {
		// case 1:
		// 256 page : 256*16 = 4096 sample/phase @32ksps	
		// 4배 downsample 하면 1024 sample/phase 얻는다 
		// 256/2000 => 0.128초 단위로 transient task를 깨운다
		
		// case 2:
		// 400 page : 400*16 = 6400 sample/phase @32ksps, 매 200msec 마다 interrupt 호출된다 
		//if (bix == 400) {
		if (meter[id].cntl.online) wQ[id].olc++;
		if (wQ[id].count >= 400) {
			// Transient task를 깨운다
			wQ[id].lastolc = wQ[id].olc;
			wQ[id].count = wQ[id].olc = 0;
			wQ[id].ts = sysTick64-200;	// 시작시간 계산 : 완료시간-200ms
			if (tid_wave[0] != 0) {
#ifdef __FREERTOS			
            	if (tid_wave[0] != 0) xTaskNotify(tid_wave[0], 0x1, eSetBits);
#else
				if (tid_wave[0] != 0) os_evt_set(0x1, tid_wave[0]);
#endif				
			}			
		}
	}	
	SSP_SSEL_Mode(id, 0);	

//	// 8 page 모두 읽은 후 transient task에 알린다 		
//	if (tid_app[id] != 0) {
//		os_evt_set(0x1, tid_app[id]);
//	}
	
	t2 = sysTick64;
	//if (t2-t1 > 10) printf("tick = %d\n", (int)(t2-t1));
}


void setPqEventLevel(int id) {
	float scale, ratio;

	scale = RMS_MAX/V_FULL*(1<<meter[id].cntl.pga_vgain)*db[id].pt.vnorm;
	// 전압
	meter[id].cntl.sagLevel      = scale * db[id].pqevt[PQE_SAG].level/100.;
	meter[id].cntl.sagRetLevel   = scale * (db[id].pqevt[PQE_SAG].level+5)/100.;
	meter[id].cntl.swellLevel    = scale * db[id].pqevt[PQE_SWELL].level/100.;
	meter[id].cntl.swellRetLevel = scale * (db[id].pqevt[PQE_SWELL].level-5)/100.;
	meter[id].cntl.intrLevel     = scale * 5/100.;
	printf("---> sag level=%d, swell level=%d\n", meter[id].cntl.sagLevel, meter[id].cntl.swellLevel);
	
	// 전류
	if (db[id].ct.CT2 == 0) {
		ratio = db[id].ct.CT1/5;
		scale = RMS_MAX/I_FULL_5AN*db[id].ct.inorm/ratio;
	}
	else {
		ratio = db[id].ct.CT1/100;
		scale = RMS_MAX/I_FULL_100mA*db[id].ct.inorm/ratio;
	}	
	meter[id].cntl.ocLevel       = scale * db[id].pqevt[PQE_OC].level/100.;	
	meter[id].cntl.ocRetLevel    = scale * (db[id].pqevt[PQE_OC].level-5)/100.;	// 5%를 복귀시점으로 한다 
	printf("---> OC level=%d, OC ret level=%d\n", meter[id].cntl.ocLevel, meter[id].cntl.ocRetLevel);
	
//	// enable wave capture 
//	pdb->pqevt[PQE_SAG].action = 1;
//	pdb->pqevt[PQE_SWELL].action = 1;
//	pdb->pqevt[PQE_OC].action = 1;
}

//void setSagCondition(float norm, float prop, uint16_t nCyc) {
//	float scale = RMS_MAX/V_FULL*(1<<meter[id].cntl.pga_vgain)*norm;
//	int level = scale*prop/100.;
//	
//	meter[id].cntl.sagLevel = (prop <= 90) ? level/32 : 0;
//	meter[id].cntl.sagCyc = nCyc;
//	
//	write_reg32(0, 0x410, (uint32_t *)&meter[id].cntl.sagLevel);
//	write_reg16(0, 0x48b, &meter[id].cntl.sagCyc);
//}

//void setSwellCondition(float norm, float prop, uint16_t nCyc) {
//	float scale = RMS_MAX/V_FULL*(1<<meter[id].cntl.pga_vgain)*norm;
//	int level = scale*prop/100.;
//	
//	meter[id].cntl.swellLevel = (prop >= 110) ? level/32 : 0xffffff;
//	meter[id].cntl.swellCyc = nCyc;
//	
//	write_reg32(0, 0x414, (uint32_t *)&meter[id].cntl.swellLevel);
//	write_reg16(0, 0x48c, &meter[id].cntl.swellCyc);
//}


uint32_t setNoLoad(float noload_level) {
	const uint32_t xWaTT_Full = 20694066;
	
	return (xWaTT_Full*64/noload_level);
}

// default prop : 5%
uint16_t getZxToutThreshold(float prop) {
	uint32_t vpcf_full = 74532013;

	//float vfull = V_FULL_LV;
//	float atten = (lineFreq == 60) ? 0.81 : 0.86;
	float atten = (pdb->pt.freq == 60) ? 0.81 : 0.86;
	float thrsh = (vpcf_full*atten) / (32 * 256) * prop;
	
	printf("[getZxToutThreshold, prop=%f, thrsh=%f]\n", prop, thrsh);
	return (uint16_t)thrsh;	
}

void initADE9000(uint8_t id)
{
	uint32_t chipId, flag;
	uint16_t version, runCmd=0, wtemp;
	uint32_t rms, stat, dtemp, i, cnt=0;
	float energy;
	int mode= 0;	// Device Mode;
	METERING 	*pmeter = &meter[id].meter;
	CNTL_DATA	*pcntl = &meter[id].cntl;
	SETTINGS	*pdb = &db[id];

	// SWRST
	wtemp = 1;
	write_reg16(id, AD9X_CONFIG1, &wtemp);
	
	osDelayTask(10);
	
	read_reg32(id, AD9X_STATUS0, &stat);		
	write_reg32(id, AD9X_STATUS0, &stat);		
	printf("status0  = %x\n", stat);	

	read_reg32(id, AD9X_STATUS1, &stat);		
	write_reg32(id, AD9X_STATUS1, &stat);		
	printf("status1  = %x\n", stat);	
	
	read_reg32(id, AD9X_PART_ID, &chipId);		
	printf("chipId  = %x\n", chipId&(1<<20));
	
	read_reg16(id, AD9X_VERSION, &version);		
	printf("version = %x\n", version);

	//--------------------------------------------------------------------------------------
	// PGA_GAIN, VGAIN=1
	wtemp = (meter[id].cntl.pga_vgain<<8) | (meter[id].cntl.pga_vgain<<10) | (meter[id].cntl.pga_vgain<<12) | 
	        (meter[id].cntl.pga_igain<<0) | (meter[id].cntl.pga_igain<<2)  | (meter[id].cntl.pga_igain<<4) |
					(meter[id].cntl.pga_ingain<<6);
	write_reg16(id, AD9X_PGA_GAIN, &wtemp);
	read_reg16(id, AD9X_PGA_GAIN, &wtemp);
	printf("PGA = %x\n", wtemp);
	
	// ACCNMODE, SELFREQ=1=60Hz
	wtemp = (1<<8);
	write_reg16(id, AD9X_ACCMODE, &wtemp);
		
	// change Isum mode -> Ig로 사용한다 
	if (pdb->ct.CT2 == CT_5A && (pdb->pt.wiring == WM_3LN3CT || pdb->pt.wiring == WM_1LN1CT || pdb->pt.wiring == WM_1LL2CT)) {
		dtemp = 1;
		read_reg16(id, AD9X_CONFIG0, &wtemp);
	}
		
	// CONFIG1
	// : BURST_EN=1, IRQ0_ON_IRQ1(all the interrupts onto a single interrupt pin, IRQ1)
	// : DIP_SWELL_IRQ_MODE(10)	
	wtemp = (1<<10) | (1<<11) | (1<<12);
	write_reg16(id, AD9X_CONFIG1, &wtemp);
	read_reg16(id, AD9X_CONFIG1, &wtemp);
	printf("CONFIG1 = %x\n", wtemp);	
	
	// PWR_TIME & EGY_TIME, 매 1초 마다 전력, 전력상 생성
	wtemp = 7999;	
	write_reg16(id, AD9X_PWR_TIME, &wtemp);
	write_reg16(id, AD9X_EGY_TIME, &wtemp);
	
	// xTHR
	dtemp = 0x100000;
	write_reg32(id, AD9X_WTHR, &dtemp);
	write_reg32(id, AD9X_VARTHR, &dtemp);
	write_reg32(id, AD9X_VATHR, &dtemp);
	
	// add pulse config
	write_reg16(id, AD9X_CF1DEN, &meter[id].cntl.CFxDEN);
	write_reg16(id, AD9X_CF2DEN, &meter[id].cntl.CFxDEN);
	write_reg16(id, AD9X_CF3DEN, &meter[id].cntl.CFxDEN);
	write_reg16(id, AD9X_CF4DEN, &meter[id].cntl.CFxDEN);
	// CF1: Total active power, CF2: Total Reactive Power, CF3; Total Apparent Power
	wtemp = (0<<0) | (1<<3) | (2<<6);
	write_reg16(id, AD9X_CFMODE, &wtemp);
	// COMPMODE: Phase to include in CFx pulse output, 7: phase A,B,C
	wtemp = (7<<0) | (7<<3) | (7<<6);
	write_reg16(id, AD9X_COMPMODE, &wtemp);
	
	// EP_CFG, set RD_RST_EN, EGY_LD_ACCM, EGY_PWR_EN
	wtemp = (1<<4) | (1<<0);
	write_reg16(id, 0x4b0, &wtemp);
	// temp_cfg
	wtemp = (1<<3) | (1<<2) | (2);
	write_reg16(id, 0x4b6, &wtemp);
	
	runCmd = 1;			
	write_reg16(id, AD9X_RUN, &runCmd);					
	printf("RUN DSP ...\n");
	
	writeGainU(id);
	writeGainI(id);
	writeGainIn(id);	// Neutral Current or Ground Current
	writeGainW(id);	
	writeGainPh(id);
	
	
	// ZX Timeout : 1s(8000)
	wtemp = 1000;
	write_reg16(id, AD9X_ZXTOUT, &wtemp);
	
	wtemp = getZxToutThreshold(0.015);	// 1.5%@848V, 12 ~ 14 사이에서 ZX Detect 발생한다 
	write_reg16(id, AD9X_ZXTHRSH, &wtemp);
	// ZXV를 끄고 Wave Capture만 사용하면 16.67 단위로 인터럽트 발생한다 
	// ZxVx or ZxToVx?? set???? interrupt service routine?? ???? ???.
	// mask0: TEMP_RDY(25), THD_PF(21), PWRRDY(18), PAGE_FULL(17), EGY_RDY(0)
	//dtemp = (1<<21) | (1<<18) | (1<<17) | (1<<0);	
	dtemp = (1<<17);// | (1<<25);	// 
	write_reg32(id, AD9X_MASK0, &dtemp);
	printf("MASK0 = %x\n", dtemp);
	
#define CHIP_ZXTOUT	
#ifdef CHIP_ZXTOUT
	//dtemp = (1<<9) | (1<<6);	// ZXCOMB, ZXTOVA
	//dtemp = (1<<6)|(1<<7)|(1<<8) | (1<<23)|(1<<24)|(1<<25);	// ZXTOVx(6, 7, 8), DIP(23, 24, 25)	
	dtemp = (1<<6) | (1<< 9);		// Volt Phase 1에 대해서 zx를 사용한다 
#else
	dtemp = 0;
#endif
	write_reg32(id, AD9X_MASK1, &dtemp);
	printf("MASK1 = %x\n", dtemp);
	
	// sag, swell 설정
	//setSagLevel(pdb->pt.vnorm, pdb->pqevt[1].level, pdb->pqevt[1].nCyc);
	//setSwellLevel(pdb->pt.vnorm, pdb->pqevt[2].level, pdb->pqevt[2].nCyc);
			
	// Current and voltage channel waveform samples,
	// processed by the DSP (xI_PCF, xV_PCF) at 8 kSPS
	// WFB_PG_IRQEN, set the interrupt at Page 7 and Page 15.
	wtemp = 0x8080;	
	write_reg16(id, AD9X_WFB_PG_IRQEN, &wtemp);	
	//set cfg
	//WF_SRC(xI, xV), WF_MODE(Cont), WF_CAP_SEL(Fixed), BURST_CHAN(All)
	//wtemp = (3<<8) | (3<<6) | (1<<5);	
	//write_reg16(id, AD9X_WFB_CFG, &wtemp);	
	
	// chan 0 : 32k, chan 1: 8k로 변경
//	(id == 0) ? configWave32kCapture(id) : configWave8kCapture(id);

	configWave8kCapture(id);
	enableCapture(id);
	printf("Enable WaveCapture ...\n");
	
#ifdef _IRQ_MBX	
	os_mbx_init(&mbox, sizeof(mbox));
#endif

	setPqEventLevel(id);	// 5%

}

void meterIrqSvc(int id) {
   
	if (tid_meter[id] != 0) {
#ifdef __FREERTOS		
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      	xTaskNotifyFromISR(tid_meter[id], 0x1, eSetBits, &xHigherPriorityTaskWoken);      
      	// Perform a context switch if necessary
      	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#else
		isr_evt_set(0x1, tid_meter[id]);
#endif		
	}	
}


// EINT0(ADE9000 IRQ Svc)
// startup_lpc43xx.s에서 GPIO0_IRQHandler() 대신 PIN_INT0_IRQHandler() 사용
void PIN_INT0_IRQHandler()
{
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(0));
	meterIrqSvc(0);
}

void PIN_INT1_IRQHandler()
{
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(1));
	meterIrqSvc(1);
}


int min_n(uint32_t *val, int n) {
	int min = val[0], i;
	for (i=1; i<3; i++) {
		if (min > val[i]) {
			min = val[i];
		}
	}	
	return min;
}


void putEventCap(PQ_EVT_CAP_Q *pq, uint64_t ts, int type) {
	int fr = (pq->fr+1)%8;
	// Full 검사 
	if (fr != pq->re) {
		pq->Q[pq->fr].Ts = ts;
		pq->Q[pq->fr].eType = type;
		pq->fr = fr;	
	}
}

// meter task에서 호출된다, printf 많이 사용하면 meter의 실행주기에 영향준다 
// swell, swell의 검출은 정상동작 상태에서 시작한다 
// sag, swell 복귀는 지정된 level의 5%로 한다 -> noise 환경으로 부터 과도한 sag, swell 발생 
void checkPqEvent(int id) {
	int i;
	PQ_EVENT_INFO *pSag   = &meter[id].cntl.pqe.inf[PQE_SAG];
	PQ_EVENT_INFO *pSwell = &meter[id].cntl.pqe.inf[PQE_SWELL];
	PQ_EVENT_INFO *pOC    = &meter[id].cntl.pqe.inf[PQE_OC];
		
	if (meter[id].cntl.online == 0) {
		// 12 half cycle = 100ms
		if (meter[id].cntl.zxMonCnt > 12) {	// Power On 시 TransientVoltage Detect 막기 위해 
			if (meter[id].cntl.online == 0) {
				meter[id].cntl.online = 1;
//				Board_LED_On(2);
				//printf("+++ OnLine +++\n");
			}
		}
	}
	else {
		if (meter[id].cntl.zxMonCnt == 0) {
			if (meter[id].cntl.online) {
				//printf("+++ OFFLine +++\n");
//				Board_LED_Off(1);
			}
			meter[id].cntl.online = 0;
		}		
	}
		
	
	// sag, swell 발생 복귀 mask 생성
	meter[id].cntl.sagMask = meter[id].cntl.sagRetMask = meter[id].cntl.swellMask = meter[id].cntl.swellRetMask = 0;
	meter[id].cntl.ocMask  = 0;
	
	for (i=0; db[id].pqevt[PQE_SAG].action && i<3; i++) {
		// detect
		if (ade9000[id].urmsFast[i] < meter[id].cntl.sagLevel) {
			meter[id].cntl.sagMask    |= (1<<i);
		}	
		// recover
		else if (ade9000[id].urmsFast[i] > meter[id].cntl.sagRetLevel) {
			meter[id].cntl.sagRetMask |= (1<<i);
		}				
	}		
	
	for (i=0; db[id].pqevt[PQE_SWELL].action && i<3; i++) {
		if (ade9000[id].urmsFast[i] > meter[id].cntl.swellLevel) {
			meter[id].cntl.swellMask    |= (1<<i);
		}
		else if (ade9000[id].urmsFast[i] < meter[id].cntl.swellRetLevel) {
			meter[id].cntl.swellRetMask |= (1<<i);
		}			
	}
	
	for (i=0; db[id].pqevt[PQE_OC].action && i<3; i++) {
		// 2025-3-20, 범위를 0 ~ 200으로 처리
		if (meter[id].cntl.ocLevel == 0) continue;
			
		if (ade9000[id].irmsFast[i] > meter[id].cntl.ocLevel) {
			meter[id].cntl.ocMask       |= (1<<i);
		}
		else if (ade9000[id].irmsFast[i] < meter[id].cntl.ocRetLevel) {
			meter[id].cntl.ocRetMask    |= (1<<i);
		}			
	}
	
	// sag handler
	// initial status
	if (meter[id].cntl.sagSt == 0) {
		if (meter[id].cntl.sagMask == 0 && meter[id].cntl.online) {
			//printf("+++ Init status(sag) \n");
			meter[id].cntl.sagSt = 1;
		}
	}
	// normal condition
	else if (meter[id].cntl.sagSt == 1) {
		// sag 발생			
		if (meter[id].cntl.sagMask) {			
			if (db[id].pqevt[PQE_SAG].action == 2) {
				putEventCap(&meter[id].cntl.pqe.wQ, sysTick64, E_SAG);
				putEventCap(&meter[id].cntl.pqe.rQ, sysTick64, E_SAG);							
			}	
			pSag->startTs = sysTick64;
			pSag->mask    = meter[id].cntl.sagMask;			
			for (i=0; i<3; i++) {			
				pSag->Val[i] = ade9000[id].urmsFast[i];
			}	
			meter[id].cntl.sagSt = 2;						
			//printf("+++ Sag \n");			
		}
	}
	// sag 
	else if (meter[id].cntl.sagSt == 2) {				
		if (meter[id].cntl.online == 0)  {			
			meter[id].cntl.sagSt = 3;			
			//printf("+++ Interruption \n");
		}
		else {
			if (meter[id].cntl.sagMask) {
				pSag->mask |= meter[id].cntl.sagMask;
				// update min
				for (i=0; i<3; i++) {
					if (pSag->Val[i] > ade9000[id].urmsFast[i]) {
						pSag->Val[i] = ade9000[id].urmsFast[i];
					}
				}
			}
			else if (meter[id].cntl.sagRetMask == 0x7) {			
				pSag->endTs = sysTick64;				
				pSag->type = E_SAG;
				pushEvent(id, pSag);			
				updateEventCount(pSag->mask, pqLog->q10m.var.sag);
				meter[id].cntl.sagSt = 4;
				ppqEvtCnt->sag++;
				printf("SAG ts=%lld, duration=%lld\n", pSag->startTs, pSag->endTs-pSag->startTs);		
			}
		}
	}
	// interrupt
	else if (meter[id].cntl.sagSt == 3) {
		if (meter[id].cntl.sagRetMask == 7) {
			pSag->endTs = sysTick64;
			if ((pSag->endTs - pSag->startTs) < 60000) {
				printf("Short INTR, ts=%lld, duration=%lld\n", pSag->startTs, pSag->endTs-pSag->startTs);
				pSag->type = E_sINTR;
				pushEvent(id,pSag);
				updateEventCount(pSag->mask, pqLog->q10m.var.shortIntr);
			}
			else {
				printf("Long  INTR, ts=%lld, duration=%lld\n", pSag->startTs, pSag->endTs-pSag->startTs);
				pSag->type = E_lINTR;
				pushEvent(id, pSag);
				updateEventCount(pSag->mask, pqLog->q10m.var.longIntr);
			}			
			ppqEvtCnt->intr++;			
			meter[id].cntl.sagSt = 1;
		}
	}
	// wait sag holdOff
	else if (meter[id].cntl.sagSt == 4) {
		if ((sysTick64-pSag->endTs) > pdb->pqevt[PQE_SAG].holdOffCyc) {
			meter[id].cntl.sagSt = 1;
		}
	}
	
	// swell handler
	if (meter[id].cntl.swellSt == 0) {
		if (meter[id].cntl.swellMask == 0 && meter[id].cntl.online) {
			meter[id].cntl.swellSt = 1;							
			//printf("+++ Init status(swell) \n");		
		}
	}
	else if (meter[id].cntl.swellSt == 1) {
		if (meter[id].cntl.swellMask) {
			// noise로 인해 swell이 연속적으로 발생, 복귀에 대비하여 마지막 복귀로 부터 200ms 지나야 wave capture 한다 
			if (db[id].pqevt[PQE_SWELL].action == 2) {
				putEventCap(&meter[id].cntl.pqe.wQ, sysTick64, E_SWELL);
				putEventCap(&meter[id].cntl.pqe.rQ, sysTick64, E_SWELL);							
			}	
						
			pSwell->startTs = sysTick64;
			pSwell->mask    = meter[id].cntl.swellMask;			
			for (i=0; i<3; i++) {			
				pSwell->Val[i] = ade9000[id].urmsFast[i];
			}		
			meter[id].cntl.swellSt = 2;
			//printf("+++ Swell \n");		
			ppqEvtCnt->swell++;						
		}
	}
	else if (meter[id].cntl.swellSt == 2) {	
		if (meter[id].cntl.swellMask) {
			// update swell mask & level
			pSwell->mask |= meter[id].cntl.swellMask;	
			for (i=0; i<3; i++) {	
				if (pSwell->Val[i] < ade9000[id].urmsFast[i]) { // update max
					pSwell->Val[i] = ade9000[id].urmsFast[i];
				}
			}					
		}
		else if (meter[id].cntl.swellRetMask == 7) {
			pSwell->endTs = sysTick64;		
			pSwell->type = E_SWELL;
			printf("SWELL ts=%lld, duration=%lld\n", pSwell->startTs, pSwell->endTs-pSwell->startTs);
			pushEvent(id, pSwell);
			updateEventCount(pSwell->mask, pqLog->q10m.var.swell);			
			meter[id].cntl.swellSt = 3;
		}
	}	
	// swell hold off cycle, 노이즈 또는 설정 이상으로 인한 과부하 방지
	else if (meter[id].cntl.swellSt == 3) {
		if ((sysTick64-pSwell->endTs) > db[id].pqevt[PQE_SWELL].holdOffCyc) {
			meter[id].cntl.swellSt = 1;
		}
	}
	
#define	_OC_SUPPORT	
#ifdef  _OC_SUPPORT
	// over current
	if (meter[id].cntl.ocSt == 0) {
		if (meter[id].cntl.ocMask == 0 && meter[id].cntl.online) {
			meter[id].cntl.ocSt = 1;							
			printf("+++ Init status(OC) \n");		
		}
	}
	else if (meter[id].cntl.ocSt == 1) {
		if (meter[id].cntl.ocMask) {
			// noise로 인해 swell이 연속적으로 발생, 복귀에 대비하여 마지막 복귀로 부터 200ms 지나야 wave capture 한다 
			if (db[id].pqevt[PQE_OC].action == 2) {
				putEventCap(&meter[id].cntl.pqe.wQ, sysTick64, E_OC);
				putEventCap(&meter[id].cntl.pqe.rQ, sysTick64, E_OC);							
			}	
						
			pOC->startTs = sysTick64;
			pOC->mask    = meter[id].cntl.ocMask;			
			for (i=0; i<3; i++) {			
				pOC->Val[i] = ade9000[id].irmsFast[i];
			}		
			meter[id].cntl.ocSt = 2;
			printf("+++ OC \n");		
			ppqEvtCnt->oc++;						
		}
	}
	else if (meter[id].cntl.ocSt == 2) {	
		if (meter[id].cntl.ocMask) {
			// update swell mask & level
			pOC->mask |= meter[id].cntl.ocMask;	
			for (i=0; i<3; i++) {	
				if (pOC->Val[i] < ade9000[id].irmsFast[i]) { // update max
					pOC->Val[i] = ade9000[id].irmsFast[i];
				}
			}					
		}
		else if (meter[id].cntl.ocRetMask == 7) {
			pOC->endTs = sysTick64;		
			pOC->type = E_OC;
			printf("OC ts=%lld, duration=%lld\n", pOC->startTs, pOC->endTs-pOC->startTs);
			pushEvent(id,pOC);
			meter[id].cntl.ocSt = 3;
		}
	}	
	// swell hold off cycle, 노이즈 또는 설정 이상으로 인한 과부하 방지
	else if (meter[id].cntl.ocSt == 3) {
		if ((sysTick64-pOC->endTs) > pdb->pqevt[PQE_OC].holdOffCyc) {
			meter[id].cntl.ocSt = 1;
		}
	}	
#endif		
}



#define	ZXT_MASK ((1<<6) | (1<<7) | (1<<8))

// swell              : 110%(242V) ~
// sag                : 5% ~ 90%(198V)
// short interruption : 5%(11V), < 60s
// long  interruption : 5%(11V), > 60s
uint64_t ts_irq[2], ts_delta[2];

void meter_scan_2(uint8_t id)
{
	uint32_t chipId, flag, zxtMask;
	uint16_t version, runCmd=0, wtemp, fr;
	uint32_t rms, stat0, stat1, vlevel, dtemp, i, cnt=0, mask;
	void *msg;
	uint64_t tick64, zxTo;
#ifdef __FREERTOS		
	uint32_t ulNotificationValue;
	if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, pdMS_TO_TICKS(20)) == 0)
#else
	if (os_evt_wait_and(0x1, 200) == OS_R_TMO) 
#endif	
	{
		printf(">>> ZX timeout ...\n");
		 // wave sampling를 다시시작한다 
 //		if (id == 0) 
 //			w8kQ.fr = w8kQ.re = 0;
 //		else
 //			w32kQ.fr = w32kQ.re = 0;
		wQ[id].fr = wQ[id].re = 0;
	}	
// 	ts_delta[id] = sysTick64 - ts_irq[id];
//	ts_irq[id] = sysTick64;
 
	tick64 = sysTick64;
	read_reg32(id, AD9X_STATUS0, &stat0);	
	read_reg32(id, AD9X_STATUS1, &stat1);	
	// Energy READY, period = 1s
	if (stat0 & (1<<0)) {
		readEnergy(id);
	}			
	// capture Wave Form
	if (stat0 & (1<<17)) {
		//(id ==0) ? readWFB32k_Data(id) : readWFB8k_Data(id);
		readWFB32k_Data(id);		
	}
	// RMS 1 cycle 
	if (stat0 & (1<<19)) {
		readPeriod(id);
		readPhaseFastRMS(id);
		checkPqEvent(id);
	}
	
	// RMS 10/12 cycle
	// 전압은 12 cycle(200ms) 간격으로 수집, (zero cross와 관계 없다)
	if (stat0 & (1<<20)) {
		readRmsAngle(id);
	}
	// PWR_READY (1s 단위로 읽는다 (PWR_TIME : 1s)
	if (stat0 & (1<<18)) {
		readPhasePower(id);
	}	
	// THD READY
	if (stat0 & (1<<21)) {
		readPhaseTHD(id);
		//printf("IRQ: THD & PF ...\n");
	}				
	
	if (stat0 & (1<<25)) {
		readTemp(id);
	}
	// clear status0
	write_reg32(id, AD9X_STATUS0, &stat0);		
	
	//
	//--------------------------------------------------------------------------------
	// Wiring Mode별로 다르게 처리해야 한다, 현재 3P4W 만 처리 함.
	// ZxToV(a,b,c)
	if (stat1 & (1<<9)) {
		if (meter[id].cntl.zxMonCnt == 0) {					
				//printf("ZX DETECT ...\n");
		}
		meter[id].cntl.zxMonCnt++;
	}
		
	if (stat1 & (1<<6)) {
		if (meter[id].cntl.zxMonCnt != 0) {
			//printf("ZXTOUT ...\n");
		}
		meter[id].cntl.zxMonCnt = 0;
	}


	if (meter[id].cntl.rstEvtList == 0x1234) {
		meter[id].cntl.rstEvtList = 0;
		clearEventList();		
	}
	
	// clear status0 & status1	
	write_reg32(id, AD9X_STATUS1, &stat1);	

	if (meter[id].cntl.wCalF[0]) {		
		if (meter[id].cntl.wCalF[1] == 1) {			
			writeGainU(id);
		}
		else if (meter[id].cntl.wCalF[1] == 2) {
			writeGainI(id);
		}
		else if (meter[id].cntl.wCalF[1] == 3) {
			writeGainW(id);
		}
		else if (meter[id].cntl.wCalF[1] == 4) {			
			writeGainPh(id);
		}			
		else if (meter[id].cntl.wCalF[1] == 5) {
			writeGainIn(id);
		}
		meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 0;
	}
}

void meter_scan(uint8_t id)
{
	uint32_t chipId, flag, zxtMask;
	uint16_t version, runCmd=0, wtemp, fr;
	uint32_t rms, stat0, stat1, vlevel, dtemp, i, cnt=0, mask;
	void *msg;
	uint64_t tick64, zxTo;

	//PG_FULL intr: 기본 발생 주기
	// 8K: (Max 8ms) -> Hi/Low 적용시 4ms
	// 32: (Max 32ms) -> Hi/Low 적용시 16ms
#ifdef __FREERTOS		
   uint32_t ulNotificationValue;
	if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, pdMS_TO_TICKS(20)) == 0)
#else
	if (os_evt_wait_and(0x1, 20) == OS_R_TMO) 
#endif	
	{
		printf(">>> ZX timeout ...\n");
		// wave sampling를 다시시작한다 
//		if (id == 0) 
//			w8kQ.fr = w8kQ.re = 0;
//		else
//			w32kQ.fr = w32kQ.re = 0;
		wQ[id].fr = wQ[id].re = 0;
	}	
//	ts_delta[id] = sysTick64 - ts_irq[id];
//	ts_irq[id] = sysTick64;
 
	tick64 = sysTick64;
//	printf("@@@ meter_scan tick, %d\n", tick64);

	// read IRQ stat0 & stat1
	read_reg32(id, AD9X_STATUS0, &stat0);	
//	read_reg32(id, 0x423, &dtemp);
//	if (dtemp != stat0) {
//		printf("@@@ Invalid Read Data(32), %x, %x\n", stat0, dtemp);
//	}
	read_reg32(id, AD9X_STATUS1, &stat1);					
//	read_reg32(id, 0x423, &dtemp);
//	if (dtemp != stat1) {
//		printf("@@@ Invalid Read Data(32), %x, %x\n", stat1, dtemp);
//	}
	
	// Energy READY, period = 1s
	if (stat0 & (1<<0)) {
		readEnergy(id);
		Board_LED_Toggle(1);	
	}			
	// capture Wave Form
	if (stat0 & (1<<17)) {
		//(id ==0) ? readWFB32k_Data(id) : readWFB8k_Data(id);
		readWFB32k_Data(id);		
	}
	
//	Board_LED_On(3);	// 실행시간: 15us
	
	// RMS 1 cycle 
	if (stat0 & (1<<19)) {
		readPeriod(id);
		readPhaseFastRMS(id);
		checkPqEvent(id);
	}
	
	// RMS 10/12 cycle
	// 전압은 12 cycle(200ms) 간격으로 수집, (zero cross와 관계 없다)
	if (stat0 & (1<<20)) {
		readRmsAngle(id);
	}
	// PWR_READY (1s 단위로 읽는다 (PWR_TIME : 1s)
	if (stat0 & (1<<18)) {
		readPhasePower(id);
	}	
	// THD READY
	if (stat0 & (1<<21)) {
		readPhaseTHD(id);
		//printf("IRQ: THD & PF ...\n");
	}				
	
	if (stat0 & (1<<25)) {
		readTemp(id);
	}
	// clear status0
	write_reg32(id, AD9X_STATUS0, &stat0);		
	
	//
	//--------------------------------------------------------------------------------
	// Wiring Mode별로 다르게 처리해야 한다, 현재 3P4W 만 처리 함.
	// ZxToV(a,b,c)
	if (stat1 & (1<<9)) {
		if (meter[id].cntl.zxMonCnt == 0) {					
				//printf("ZX DETECT ...\n");
		}
		meter[id].cntl.zxMonCnt++;
	}
		
	if (stat1 & (1<<6)) {
		if (meter[id].cntl.zxMonCnt != 0) {
			//printf("ZXTOUT ...\n");
		}
		meter[id].cntl.zxMonCnt = 0;
	}


	if (meter[id].cntl.rstEvtList == 0x1234) {
		meter[id].cntl.rstEvtList = 0;
		clearEventList();		
	}
	// if(id==0) {
	// 	if (meter[id].cntl.rstIticList == 0x1234) {
	// 		meter[id].cntl.rstIticList = 0;
	// 		memset(piticlist, 0, sizeof(*piticlist));
	// 	}
	// }
	
	// clear status0 & status1	
	write_reg32(id, AD9X_STATUS1, &stat1);	

	if (meter[id].cntl.wCalF[0]) {		
		if (meter[id].cntl.wCalF[1] == 1) {			
			writeGainU(id);
		}
		else if (meter[id].cntl.wCalF[1] == 2) {
			writeGainI(id);
		}
		else if (meter[id].cntl.wCalF[1] == 3) {
			writeGainW(id);
		}
		else if (meter[id].cntl.wCalF[1] == 4) {			
			writeGainPh(id);
		}			
		else if (meter[id].cntl.wCalF[1] == 5) {
			writeGainIn(id);
		}
		meter[id].cntl.wCalF[0] = meter[id].cntl.wCalF[1] = 0;
	}
	
}


//void Meter1_Scan(uint8_t id)
//{
//	uint32_t chipId, flag;
//	uint16_t version, runCmd=0, wtemp;
//	uint32_t rms, stat0, stat1, vlevel, dtemp, i, cnt=0;
//	uint64_t t1;
//	
//	if (os_evt_wait_and(0x1, 200) == OS_R_TMO) {
//		printf(">>> os_evt_wait_and : timeout ...\n");
//	}
//	
//	// read IRQ stat0 & stat1
//	read_reg32(id, AD9X_STATUS0, &stat0);		
//	// capture Wave Form
//	if (stat0 & (1<<17)) {
//		readWFB32k_Data(id);		
//	}
//	// clear status0
//	write_reg32(id, AD9X_STATUS0, &stat0);		
//	
//	//
//	//					
//	read_reg32(id, AD9X_STATUS1, &stat1);		
//	//printf("status1  = %x\n", stat);	
//	// ZXTOVA
//	if (stat1 & (1<<6)) {
//		meter[id].cntl.online = 0;
//	}	
//	// ZXVA	
//	if (stat1 & (1<<9)) {
//		meter[id].cntl.online = 1;		
//	}	
//	// clear status0 & status1	
//	write_reg32(id, AD9X_STATUS1, &stat1);			
//}

