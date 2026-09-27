/*============================================================================
 * fft_radix2_czt.c — 고조파/THD 엔진 + FFT_Task (파형 소비 태스크)
 *----------------------------------------------------------------------------
 * ※ 파일명은 레거시(과거 Radix2 FFT + CZT 시절). 현재 기본 엔진은 Goertzel.
 *   Radix2 FFT(FFT_czt)·CZT는 폴백 경로로만 잔존(g_fftGoertzel=0).
 *
 * [고조파/THD 엔진] — Goertzel(측정주파수) 이 기본(g_fftGoertzel=1)
 *   · 계수 coeff = 2·cos(2π·h·f_meas/8000) 로 8k 창(1600샘플=N_FFT)에서
 *     1~63차 고조파를 '실측 기본주파수 f_meas' 에서 단일패스 동시 평가.
 *   · 45~65Hz 면 실측 f_meas, 그 밖이면 공칭(50/60Hz) 폴백.
 *   · 장점: 비동기 DFT/CZT(고정 bin)의 스펙트럼 누설(순수입력 ~1.8% THD 아티팩트)
 *     제거 + CZT 대비 ~15.7배(204ms→13ms). 리샘플 불필요(FFT_prepare는 원시복사).
 *   · H1 정규화(IEC), 차수별 게인보정 v_harm_gain[freq][h](프론트엔드 RC 롤오프 등).
 *
 * [FFT_Task] — LLOW(최저) 우선순위. os_evt 대기 → 채널별 wbFFT8k[id] 소비.
 *   각 채널 처리 순서:
 *     1) [진단] wvfDetect(id)  — WV_DIAG 시. 스파이크(median 편차) 검출·통계.
 *        (Meter 스레드가 아닌 여기서 printf → 계측 타이밍 오염 없음. shell 'WVF')
 *     2) 고조파/THD  — FFT_prepare(원시복사) → FFT_harmonic(Goertzel) → THD.
 *   전압THD(U/Upp)는 M0만 산출→M1/M2 복사(전압 3칩 공유), 전류THD(I)는 채널별.
 *   Quiet Capture(WV_QCAP): FFT_prepare에서 g_wfbQuiet 양보(flash/워커 동시활동
 *   시 M0 readWFB 교란 회피 — 조용창에서만 처리).
 *
 * [주요 심볼]
 *   FFT_goertzel   : Goertzel 측정freq 엔진(coeff=2cos, 1~63차)
 *   FFT_harmonic   : 엔진 디스패치(g_fftGoertzel? Goertzel : CZT 폴백)
 *   FFT_czt/FFT_postproc : CZT(Bluestein) 폴백(공칭 bin)
 *   FFT_prepare/_pp: wbFFT8k → pFFT->xreal 복사(상전압/선간). 리샘플 안 함.
 *   calcCF         : Crest Factor(peak/rms) — 정현파 판별 보조.
 *   wvfDetect/wvfStatDump/wvfStatClear : [WV_DIAG] 스파이크 추적통계(shell 'WVF')
 *   fft_goertzel_test : shell 'FFTTEST [50|60]' Goertzel vs CZT 대조 검증.
 *============================================================================*/
#include "RTL.h"
#include "board.h"
#include "math.h"
#include "meter.h"
#include "fft.h"
#include "string.h"

/* 차수별 게인 보정 [freq(0:50/1:60)][고조파 1~63]. 프론트엔드 RC 롤오프 등 보정. */
float	v_harm_gain[2][63] = {
// 50Hz
{1,    1,     0.997, 0.996, 0.994, 0.991, 0.989, 0.986, 0.983, 0.980,
0.976, 0.974, 0.972, 0.969, 0.967, 0.965, 0.963, 0.961, 0.959, 0.958,
0.957, 0.956, 0.955, 0.955, 0.955, 0.955, 0.955, 0.956, 0.957, 0.958,
0.959, 0.960, 0.962, 0.964, 0.966, 0.967, 0.969, 0.971, 0.973, 0.975,
0.977, 0.979, 0.981, 0.983, 0.985, 0.987, 0.989, 0.991, 0.994, 0.996,
1, 	   1,     1.004, 1.004, 1.008, 1.008, 1.012, 1.012, 1.016, 1.016,
1.02,  1.02,  1.024},
// 60Hz
{1,    0.999, 0.996, 0.994, 0.990, 0.987, 0.984, 0.981, 0.977, 0.974,
0.971, 0.968, 0.965, 0.963, 0.961, 0.959, 0.957, 0.956, 0.955, 0.955,
0.955, 0.955, 0.955, 0.956, 0.958, 0.960, 0.961, 0.963, 0.965, 0.968,
0.969, 0.972, 0.975, 0.976, 0.979, 0.981, 0.983, 0.986, 0.988, 0.991,
0.994, 0.997, 1.001, 1.005, 1.010, 1.015, 1.020, 1.025, 1.030, 1.032,
1.035, 1.040, 1.045, 1.050, 0,     0,     0,     0,     0,     0,
0, 0, 0}
};

float	i_harm_gain[2][63] = {
//1	   2      3      4      5      6      7      8      9      10
// 50Hz
{1,    1,     1,     1.001, 1.001, 1.001, 1.002, 1.002, 1.003, 1.004,
1.005, 1.007, 1.008, 1.009, 1.010, 1.011, 1.011, 1.013, 1.014, 1.015,
1.016, 1.017, 1.019, 1.020, 1.022, 1.024, 1.026, 1.028, 1.031, 1.033,
1.036, 1.039, 1.042, 1.045, 1.048, 1.051, 1.054, 1.057, 1.060, 1.063,
1.066, 1.069, 1.072, 1.074, 1.078, 1.081, 1.084, 1.087, 1.090, 1.094,
1.105, 1.109, 1.113, 1.118, 1.123, 1.128, 1.132, 1.137, 1.141, 1.147,
1.157, 1.180, 1.234},
// 60Hz
{1,    1,     1,     1.001, 1.001, 1.002, 1.003, 1.003, 1.004, 1.007,
1.008, 1.009, 1.010, 1.011, 1.013, 1.014, 1.015, 1.017, 1.018, 1.020,
1.022, 1.025, 1.027, 1.030, 1.033, 1.036, 1.040, 1.043, 1.046, 1.050,
1.054, 1.057, 1.061, 1.065, 1.068, 1.072, 1.075, 1.078, 1.082, 1.086,
1.090, 1.095, 1.100, 1.105, 1.110, 1.117, 1.123, 1.129, 1.134, 1.138,
1.142, 1.146, 1.150, 1.20, 0, 0, 0, 0, 0, 0,
0, 0, 0}
};


extern FFT_CZT *pFFT;

extern uint64_t sysTick64;
//extern float lineFreq;
extern void maxMinTHD(int id);

void init_Radix2() {
	int i;

	for (i = 0; i < (M_FFT/2); i++) {
		pFFT->cos_rdx[i] = cos(_2PI * i / M_FFT);
		pFFT->sin_rdx[i] = sin(_2PI * i / M_FFT);
	}
}

void init_CZT() {
	int i, temp, n=N_FFT;
	float angle;

	// Trignometric tables
	for (i = 0; i < n; i++) {
		temp = i * i;
		temp %= n * 2;
		angle = PI * temp / n;
		// Less accurate version if long long is unavailable: float angle = M_PI * i * i / n;
		pFFT->cos_czt[i] = cos(angle);
		pFFT->sin_czt[i] = sin(angle); 
	}
}

//

uint32_t ReverseBits(int val)
{
	int result = 0, i;
	for (i = 0; i < 32; i++, val >>= 1)
		result = (result << 1) | (val & 1);
	return result;
}

int FFT_radix2(float real[], float imag[], uint32_t n) {
	// Length variables
	int status = -1;
	int levels = 0;  // Compute levels = floor(log2(n))
	uint32_t temp, size, i, j, k;
	float *pCos = pFFT->cos_rdx;
	float *pSin = pFFT->sin_rdx;
	
	for (temp = n; temp > 1U; temp >>= 1)
		levels++;
		
	if (1U << levels != n)
		return status;  // n is not a power of 2
	
	//init_CosSinRadix2(n);
	
	// Bit-reversed addressing permutation
	for (i = 0; i < n; i++) {
		//j = reverse_bits(i, levels);
		j = ((uint32_t)ReverseBits(i) >> (32 - levels));
		if (j > i) {
			float temp = real[i];
			real[i] = real[j];
			real[j] = temp;
			temp = imag[i];
			imag[i] = imag[j];
			imag[j] = temp;
		}
	}
	
	// Cooley-Tukey decimation-in-time radix-2 FFT
	for (size = 2; size <= n; size *= 2) {
		uint32_t halfsize = size / 2;
		uint32_t tablestep = n / size;

		for (i = 0; i < n; i += size) {
			for (j = i, k = 0; j < i + halfsize; j++, k += tablestep) {
				int l = j + halfsize;
				float tpre =  real[l] * pCos[k] + imag[l] * pSin[k];
				float tpim = -real[l] * pSin[k] + imag[l] * pCos[k];
				real[l] = real[j] - tpre;
				imag[l] = imag[j] - tpim;
				real[j] += tpre;
				imag[j] += tpim;
			}
		}
		if (size == n)  // Prevent overflow in 'size *= 2'7
			break;
	}
	status = 0;	
	return status;
}


int convolve(float xreal[], float ximag[], float yreal[], float yimag[], float outreal[], float outimag[], uint32_t n) {	
	int status = -1, i;

	FFT_radix2(xreal, ximag, n);

	FFT_radix2(yreal, yimag, n);
	
	for (i = 0; i < n; i++) {
		float temp = xreal[i] * yreal[i] - ximag[i] * yimag[i];
		ximag[i] = ximag[i] * yreal[i] + xreal[i] * yimag[i];
		xreal[i] = temp;
	}
	// inverse transform
	FFT_radix2(ximag, xreal, n);
	
	for (i = 0; i < n; i++) {  // Scaling (because this FFT implementation omits it)
		outreal[i] = xreal[i] / n;
		outimag[i] = ximag[i] / n;
	}
	status = true;
	
	return status;
}


float calcCF(float sample[], int length) {
	float rms=0, v=0, max=0; 
	int i;
	
	for (i=0; i<length; i++) {
		v = (sample[i]*sample[i]);
		rms += v;		
		if (max < v) max = v;		
	}
	rms = sqrt(rms/length);
	max = sqrt(max);	
	v = max/rms;
	
	return v;
}



/* [Goertzel 측정freq] 리샘플·CZT 폐기 — 고조파를 실측주파수(coeff=2cos(2π·h·f/8000))에서 직접 평가.
   FFT_prepare는 원시 샘플만 복사. CZT는 FFT_harmonic 폴백(g_fftGoertzel=0)에서만(공칭 bin). */

void FFT_prepare(int32_t sample[], int n) {
	int i;
#ifdef WV_QCAP
	while (g_wfbQuiet) osDelayTask(5);	/* [Quiet Capture] M0 캡처 중엔 무거운 FFT 계산 양보(교란 방지) */
#endif
	/* [Goertzel] 리샘플 불필요 — 원시 샘플 그대로(Goertzel이 측정주파수 coeff로 직접 평가). */
	for (i=0; i<n; i++) {
		pFFT->xreal[i] = (float)sample[i];
		pFFT->ximag[i] = 0;
	}
}

void FFT_prepare_pp(int32_t s1[], int32_t s2[], int n) {
	int i;
#ifdef WV_QCAP
	while (g_wfbQuiet) osDelayTask(5);	/* [Quiet Capture] M0 캡처 중엔 무거운 FFT 계산 양보(교란 방지) */
#endif
	/* [Goertzel] 리샘플 불필요 — 선간(s2-s1) 원시 차분 그대로. */
	for (i=0; i<n; i++) {
		pFFT->xreal[i] = (float)s2[i] - (float)s1[i];
		pFFT->ximag[i] = 0;
	}
}


int FFT_czt(float real[], float imag[], int n, int m) {
	int status = -1;
	uint32_t i, temp;
	float *pCos = pFFT->cos_czt;
	float *pSin = pFFT->sin_czt;
	
	//while (m < n * 2 + 1)
	//	m *= 2;

	//init_CZT(n);
	
	// Temporary vectors and preprocessing
	for (i = 0; i < n; i++) {
//		pFFT->areal[i] =  real[i] * pCos[i] + imag[i] * pSin[i];
//		pFFT->aimag[i] = -real[i] * pSin[i] + imag[i] * pCos[i];
		pFFT->areal[i] =  real[i] * pCos[i];
		pFFT->aimag[i] = -real[i] * pSin[i];		
	}
	// 남은 영역을 지운다
	for (; i < m; i++) {
		pFFT->areal[i] = pFFT->aimag[i] = 0;
	}
	
	pFFT->breal[0] = pCos[0];
	pFFT->bimag[0] = pSin[0];
	for (i = 1; i < n; i++) {
		pFFT->breal[i] = pFFT->breal[m - i] = pCos[i];
		pFFT->bimag[i] = pFFT->bimag[m - i] = pSin[i];
	}
	// 남은 영역을 지운다
	for (; i <= (m-n); i++) {
		pFFT->breal[i] = pFFT->bimag[i] = 0;
	}
	
	// Convolution
	convolve(pFFT->areal, pFFT->aimag, pFFT->breal, pFFT->bimag, pFFT->creal, pFFT->cimag, m);

	// Postprocessing
	for (i = 0; i < n; i++) {
		real[i] =  pFFT->creal[i] * pCos[i] + pFFT->cimag[i] * pSin[i];
		imag[i] = -pFFT->creal[i] * pSin[i] + pFFT->cimag[i] * pCos[i];
	}
	status = 0;
	
	return status;
}


// 고조파 크기 계산방법
// 1) magnitude[i]/기본파
// 2) magnitude[i]/RMS-total
float FFT_postproc(int n, int sel, uint16_t *pHD) {
	int i, ix;
	float max=0, *pX=pFFT->xreal, *pY=pFFT->ximag, thd=0, gain;

	for (i=0; i<n/2; i++) {
		pFFT->amp[i] = sqrt(pX[i]*pX[i] + pY[i]*pY[i]);
		if (max < pFFT->amp[i]) max = pFFT->amp[i];
	}
	
	for (i=0; i<n/2; i++) {
		pFFT->amp[i] /= max;
	}
	
	// 고조파의 크기: 
	// 각 차수별 크기 = amplitude[i] / amplitude[1]
	// total = sqrt(power(h1, 2) + power(h2,2) + ...)
	// 실제 차수별 크기 = 차수별 amplitude/total amplitude
	
	for (i=2; i<=63; i++) {
	  // harmoics를 각 차수별(2~63)로 %로 환산하여 저장
		ix = i*FREQ_HZ(db.freq)/5;	/* [폴백CZT] i차 고조파 = 공칭 bin i*k1(60→12,50→10) */
		if (ix >= n/2) { pHD[i] = 0; continue; }	/* Nyquist 초과 차수 제외 */
		if(sel == 2) {
			if(FREQ_HZ(db.freq)==50)
				gain = i_harm_gain[0][i];
			else
				gain = i_harm_gain[1][i];
		}
		else {
			if(FREQ_HZ(db.freq)==50)
				gain = v_harm_gain[0][i];
			else
				gain = v_harm_gain[1][i];

		}

//		ix = i*lineFreq/5;
		thd   += pFFT->amp[ix]*pFFT->amp[ix]*gain*gain;
		pHD[i] = pFFT->amp[ix]*gain*10000;			
	}
	
	thd = sqrt(thd);
	pHD[0] = thd*10000;	// 100.00 
	pHD[1] = 0;
	
	return thd;
}
	
// K = Sum((Ih*n)^2) / Sum(Ih^2)
float calcKF() {
	float IhSqSum=0, IhNSqSum=0;
	int i, ix, res;

	res = FREQ_HZ(db.freq)/5;		/* [폴백CZT] 공칭 기본파 bin 간격 k1(60→12,50→10) */

	// irms = h1^2+h2^2+ ....h63^2)
	for (i=1; i<=63; i++) {
		ix = i*res;
		if (ix >= N_FFT/2) continue;	/* [리샘플] Nyquist 초과 제외 */
		IhSqSum += (pFFT->amp[ix]*pFFT->amp[ix]);
		IhNSqSum += (i*i)*(pFFT->amp[ix]*pFFT->amp[ix]);
	}	
	
	return IhNSqSum/IhSqSum;
}


void makeUpp(WAVE_8K_BUF *w8k, int n) {
	int32_t *sa, *sb, *dst;
	int i, j, x;
	
	for (j=0; j<3; j++) {
		x = (j+1)%3;
		dst = w8k->Upp[j];
		sa = w8k->U[j];
		sb = w8k->U[x];
		// Upp 데이터 생성한다 	
		for (i=0; i<N_FFT; i++) {
			dst[i] = sb[i] - sa[i];
		}	
	}
}


void fft_average(int id, int cond) {
	int i, j;
	HarmonicsData *phm = &pcntl->hmd;
	METERING  *pmeter;
	HARMONICS *pHD;

	phm = &meter[id].cntl.hmd;
	pmeter = &meter[id].meter;
	pHD    = &meter[id].hd;

	if (cond) {
		memset(phm, 0, sizeof(*phm));
	}
	
	//
	phm->count++;
	//
	for (i=0; i<3; i++) {
		phm->Uthdsum[i] += pmeter->THD_U[i];
		phm->Uthd[i]     = phm->Uthdsum[i]/phm->count;
		
		phm->Ucfsum[i]  += pmeter->CF_U[i];
		phm->Ucf[i]      = phm->Ucfsum[i]/phm->count;
		
		for (j=2; j<=25; j++) {
			phm->Uhdsum[i][j-2] += pHD->U[i][j];	// Uhd: 2~25(0 ~ 23)
			phm->Uhd[i][j-2]     = phm->Uhdsum[i][j-2]/phm->count;
		}
	}	
	//	
	for (i=0; i<3; i++) {
		// 2020-2-28 수정 THD_U -> THD_Upp
		phm->Uppthdsum[i] += pmeter->THD_Upp[i];	
		phm->Uppthd[i]     = phm->Uppthdsum[i]/phm->count;
		// 2020-2-28 수정 CF_U -> CF_Upp
		phm->Uppcfsum[i]  += pmeter->CF_Upp[i];
		phm->Uppcf[i]      = phm->Uppcfsum[i]/phm->count;
		
		for (j=2; j<=25; j++) {
			phm->Upphdsum[i][j-2] += pHD->Upp[i][j];	// Uhd: 2~25(0 ~ 23)
			phm->Upphd[i][j-2]     = phm->Upphdsum[i][j-2]/phm->count;
		}
	}	
	//
	for (i=0; i<3; i++) {
		phm->Ithdsum[i] += pmeter->THD_I[i];
		phm->Ithd[i]     = phm->Ithdsum[i]/phm->count;
		
		// 2020-2-28 : thd 평균 잘못 표시되는 문제 수정, Ithdsum -> Itddsum
		phm->Itddsum[i] += pmeter->TDD_I[i];	
		phm->Itdd[i]     = phm->Itddsum[i]/phm->count;
		
		phm->Icfsum[i]  += pmeter->CF_I[i];
		phm->Icf[i]      = phm->Icfsum[i]/phm->count;
		
		phm->Ikfsum[i]  += pmeter->KF_I[i];
		phm->Ikf[i]      = phm->Ikfsum[i]/phm->count;
		
		for (j=2; j<=25; j++) {
			phm->Ihdsum[i][j-2] += pHD->I[i][j];	// Uhd: 2~25(0 ~ 23)
			phm->Ihd[i][j-2]     = phm->Ihdsum[i][j-2]/phm->count;
		}
	}	
}




// 10초 단위로 동작 
/* ── [Goertzel 측정freq 엔진] 8k 창의 63고조파를 실측 h·f에서 직접 평가(coeff=2cos(2π·h·f/8000)). ──
   CZT(157ms×9)+리샘플(K-flip) 대체. 단일패스 63고조파 동시갱신, H1정규화, gain OOB회피 h<63.
   g_fftGoertzel=1(기본), 0=CZT 폴백(공칭 bin). RSTP 검증(CZT==GZ) 기반의 측정주파수 변형. */
static float g_gzCoeff[64];
static float g_gzS1[64], g_gzS2[64];
static float g_gzFreqM = 0;			/* 캐시된 측정주파수(변경시만 계수 재계산) */
int g_fftGoertzel = 1;

#pragma push
#pragma O3
#pragma Otime
float FFT_goertzel(float sample[], int n, float freq, int sel, uint16_t *pHD) {
	int h, i, k1 = FREQ_HZ(db.freq)/5, f50 = (FREQ_HZ(db.freq)==50);
	float mag[64], thd=0, gain, h1;
	float *cf = g_gzCoeff, *s1 = g_gzS1, *s2 = g_gzS2;

	if (g_gzFreqM != freq) {		/* 측정주파수 변경시만: 각 고조파를 실측 h·f에서 평가(리샘플 불요) */
		for (h=1; h<=63; h++)
			cf[h] = 2.0f * (float)cos(2.0*PI*(double)h*(double)freq/8000.0);
		g_gzFreqM = freq;
	}
	for (h=1; h<=63; h++) { s1[h]=0; s2[h]=0; }
	for (i=0; i<n; i++) {			/* [CM4최적화] 샘플 1회 읽고 63고조파 동시 갱신 */
		float x = sample[i];
		for (h=1; h<=63; h++) {
			float s0 = x + cf[h]*s1[h] - s2[h];
			s2[h] = s1[h]; s1[h] = s0;
		}
	}
	for (h=1; h<=63; h++)
		mag[h] = (float)sqrt((double)(s1[h]*s1[h] + s2[h]*s2[h] - cf[h]*s1[h]*s2[h]));
	h1 = (mag[1] > 0) ? mag[1] : 1;	/* H1(기본파) 정규화 */
	for (h=1; h<=63; h++) { mag[h] /= h1; pFFT->amp[h*k1] = mag[h]; }	/* amp[]=calcKF용(공칭 bin 인덱스) */
	for (h=2; h<63; h++) {			/* THD/HD (postproc 동일 공식, gain OOB 회피 h<63) */
		gain = (sel==2) ? (f50 ? i_harm_gain[0][h] : i_harm_gain[1][h])
		                : (f50 ? v_harm_gain[0][h] : v_harm_gain[1][h]);
		thd += mag[h]*mag[h]*gain*gain;
		pHD[h] = (uint16_t)(mag[h]*gain*10000);
	}
	thd = (float)sqrt((double)thd);
	pHD[0] = (uint16_t)(thd*10000);
	pHD[1] = 0;
	pHD[63] = 0;					/* 63차는 gain 배열([63]=0~62) 없음 → 0 */
	return thd;
}
#pragma pop

/* Goertzel vs CZT 검증: 합성 고조파 주입 → 대조 (shell 'FFTTEST [50|60]') */
static int32_t s_fftTest[N_FFT] __attribute__((section("EXT_RAM"), zero_init));
void fft_goertzel_test(int freq) {
	uint16_t hdc[70], hdg[70];
	float thc, thg;
	int i, sf;
	uint64_t tc0, tc1, tg0, tg1;

	for (i=0; i<N_FFT; i++) {		/* 8ksps: H1=10000, H3=1000(10%), H5=500(5%), H7=300(3%) */
		double t = (double)i / 8000.0;
		s_fftTest[i] = (int32_t)( 10000.0*sin(2*PI*freq*t) + 1000.0*sin(2*PI*3*freq*t)
		                        +   500.0*sin(2*PI*5*freq*t) +  300.0*sin(2*PI*7*freq*t) );
	}
	sf = db.freq; db.freq = (freq==50) ? 1 : 0;
	FFT_prepare(s_fftTest, N_FFT);
	tg0 = sysTick64;
	thg = FFT_goertzel(pFFT->xreal, N_FFT, (float)freq, 0, hdg)*100;	/* Goertzel 먼저(xreal 유지) */
	tg1 = sysTick64;
	tc0 = sysTick64;
	FFT_czt(pFFT->xreal, pFFT->ximag, N_FFT, M_FFT);				/* xreal 덮어씀 */
	thc = FFT_postproc(N_FFT, 0, hdc)*100;
	tc1 = sysTick64;
	db.freq = sf;

	printf("\n=== FFT test freq=%d (H3=10%% H5=5%% H7=3%%, 단위 0.01%%) ===\n", freq);
	printf("       H3    H5    H7    THD\n");
	printf("CZT : %5u %5u %5u %5u\n", hdc[3], hdc[5], hdc[7], (unsigned)(thc*100));
	printf("GZ  : %5u %5u %5u %5u\n", hdg[3], hdg[5], hdg[7], (unsigned)(thg*100));
	printf("time: CZT=%ums  GZ=%ums (채널당). CZT==GZ 여야 정상\n", (unsigned)(tc1-tc0), (unsigned)(tg1-tg0));
}

/* 고조파/THD 엔진 스위치. sel:0=U,1=Upp,2=I. fMeas=측정주파수. 입력=pFFT->xreal(FFT_prepare가 채움). */
static float FFT_harmonic(int sel, uint16_t *pHD, float fMeas) {
	if (g_fftGoertzel)
		return FFT_goertzel(pFFT->xreal, N_FFT, fMeas, sel, pHD);	/* [기본] Goertzel 측정freq */
	FFT_czt(pFFT->xreal, pFFT->ximag, N_FFT, M_FFT);	/* [폴백] CZT 공칭 bin */
	return FFT_postproc(N_FFT, sel, pHD);
}

#ifdef WV_DIAG
/* ── [WV_DIAG] 스파이크 검출 (FFT_Task = LLOW 우선순위에서 실행 → Meter 타이밍 오염 없음) ──
 *  wbFFT8k[id].U[3]/I[3] (각 1600샘플)에서 median 편차>임계 스파이크 검출.
 *  전압·전류가 같은 샘플위치(±2)에 동시 스파이크인지 판별:
 *    coinc=동시(공통원인:전원/GND/REF/ADC타이밍), vOnly=전압만(공유노드), iOnly=전류만(개별).
 *  printf는 FFT_Task(LLOW)라 계측 무영향. */
#define WVF_N        1600
/* [개선] 절대 임계는 신호 대비 너무 낮았음(50000=신호의 0.26%). → 신호 피크 비례 임계로 전환.
 *  각 상 피크를 측정하고 median 편차를 피크 대비 %로 판정. 부하 크기 무관 일관.
 *  임계 하나로 고정하지 않고 배율별 히스토그램(0.5/1/2/3%)으로 분포를 보여줘 실제 스파이크 크기대 확인. */

/* [추적관찰] 칩별 누적통계 — shell(WVF 명령)에서 조회/클리어. 매프레임 printf 폭주 방지 위해
 *  상세는 여기 누적하고, 콘솔 자동로그는 WVF_LOG_EVERY 프레임마다 요약만. clear로 부팅과도분 제거 후 순수 관측. */
typedef struct {
	uint32_t frm;			/* 처리 프레임수 */
	uint32_t s05, s1, s2, s3;	/* 편차>피크의 0.5/1/2/3% 인 샘플 누적 */
	uint32_t worstDev;		/* 최대 편차(raw) */
	uint16_t worstPct100;		/* 그때 피크대비 %×100 */
	uint16_t worstPh, worstK;	/* 그때 상/샘플위치 */
	uint32_t worstPk;		/* 그때 피크 */
} WVF_STAT;
WVF_STAT wvfStat[3];			/* 전역: FS.c(WVF 명령)에서 extern 접근 */
#define WVF_LOG_EVERY  500u		/* 콘솔 자동 요약로그 주기(프레임). 0이면 자동로그 OFF */

/* [캡처 밀림 추적] ade9000.c readWFB_Data에서 갱신하는 전역(사용자 가설: 캡처 밀림→torn 스파이크?) */
extern uint32_t wvfLagFrm[3], wvfLagCnt[3], wvfLagMax[3];
extern uint8_t  wvfPageLast[3];
extern uint32_t wvfTsPrev[3], wvfGapMax[3], wvfGapLate[3], wvfGapSum[3];

void wvfStatClear(void) {		/* shell: WVF C — 누적 초기화(부팅과도분 제거) */
	int i;
	for (i = 0; i < 3; i++) {
		wvfStat[i].frm = wvfStat[i].s05 = wvfStat[i].s1 = wvfStat[i].s2 = wvfStat[i].s3 = 0;
		wvfStat[i].worstDev = wvfStat[i].worstPct100 = wvfStat[i].worstPh = wvfStat[i].worstK = 0;
		wvfStat[i].worstPk = 0;
		/* 밀림 계측도 리셋(단 TsPrev는 유지: 간격 연속성). */
		wvfLagFrm[i] = wvfLagCnt[i] = wvfLagMax[i] = 0;
		wvfGapMax[i] = wvfGapLate[i] = wvfGapSum[i] = 0;
	}
}
void wvfStatDump(void) {		/* shell: WVF — 현재 누적 통계 출력 */
	int i;
	printf("[WVF STAT] (신호피크 대비 편차 배율별 누적샘플; >2%%~3%%가 유의하면 진짜 스파이크)\n");
	for (i = 0; i < 3; i++) {
		WVF_STAT *w = &wvfStat[i];
		uint32_t f = w->frm ? w->frm : 1;
		printf(" M%d frm=%u | >0.5%%=%u >1%%=%u >2%%=%u >3%%=%u | per1k: %u.%u/%u.%u/%u.%u/%u.%u"
		       " | 누적Max=%u(%u.%02u%% pk%u ph%u k%u)\n",
		       i, w->frm, w->s05, w->s1, w->s2, w->s3,
		       w->s05*1000/f, (w->s05*10000/f)%10, w->s1*1000/f, (w->s1*10000/f)%10,
		       w->s2*1000/f, (w->s2*10000/f)%10, w->s3*1000/f, (w->s3*10000/f)%10,
		       w->worstDev, w->worstPct100/100, w->worstPct100%100, w->worstPk, w->worstPh, w->worstK);
	}
	/* 캡처 밀림 지표: gapAvg/Max=진입간격(ms, 정상16), late=24ms초과횟수, pageLag=page밀림. M2만 크면 밀림 확증. */
	printf("[WVF LAG] (캡처 밀림; 정상 gap~16ms. M2만 크면 SSP1 공유버스 밀림→torn 가설 확증)\n");
	for (i = 0; i < 3; i++) {
		uint32_t lf = wvfLagFrm[i] ? wvfLagFrm[i] : 1;
		printf(" M%d rd=%u | gap avg=%u.%u max=%u ms late(>24)=%u | pageLag cnt=%u max=%up(%ums) lastPg=%u\n",
		       i, wvfLagFrm[i],
		       wvfGapSum[i]/lf, (wvfGapSum[i]*10/lf)%10, wvfGapMax[i], wvfGapLate[i],
		       wvfLagCnt[i], wvfLagMax[i], wvfLagMax[i]*2, wvfPageLast[i]);
	}
}

static void wvfDetect(int id) {
	extern WAVE_8K_BUF wbFFT8k[];
	WVF_STAT *w;
	int k, ph;
	int32_t pkV = 1;			/* 전압 3상 최대 피크(|샘플|) */
	int c05=0, c1=0, c2=0, c3=0;		/* 편차 > 피크의 0.5/1/2/3% 인 샘플 수 */
	int maxDev = 0, maxPct100 = 0, maxPh = -1, maxK = 0;	/* 최대편차, 그때 피크대비 %(×100) */

	if (id < 0 || id >= 3) return;
	w = &wvfStat[id];

	/* 1) 전압 3상 피크 측정(비례 임계 기준) */
	for (ph = 0; ph < 3; ph++) {
		int32_t *s = wbFFT8k[id].U[ph];
		for (k = 0; k < WVF_N; k++) {
			int32_t v = s[k]; if (v < 0) v = -v;
			if (v > 100000000) continue;		/* 포화 제외 */
			if (v > pkV) pkV = v;
		}
	}
	/* 2) median 편차를 피크 대비 %로 판정 + 배율별 카운트 */
	for (ph = 0; ph < 3; ph++) {
		int32_t *s = wbFFT8k[id].U[ph];
		for (k = 1; k < WVF_N-1; k++) {
			int a = s[k-1], b = s[k], d = s[k+1], lo, hi, med;
			int64_t dev, pct100;
			if (b > 100000000 || b < -100000000) continue;
			lo = a<d?a:d; hi = a<d?d:a; med = b<lo?lo:(b>hi?hi:b);
			dev = b-med; if (dev<0) dev=-dev;
			pct100 = dev * 10000 / pkV;		/* 피크 대비 %×100 (예 250 = 2.50%) */
			if (pct100 >  50) c05++;
			if (pct100 > 100) c1++;
			if (pct100 > 200) c2++;
			if (pct100 > 300) c3++;
			if (dev > maxDev) { maxDev=(int)dev; maxPct100=(int)pct100; maxPh=ph; maxK=k; }
		}
	}

	/* [분포 누적] 전역 통계에 누적(shell WVF로 조회/클리어). worst=역대 최대편차 시점 정보 보존. */
	w->frm++;
	w->s05 += (uint32_t)c05; w->s1 += (uint32_t)c1; w->s2 += (uint32_t)c2; w->s3 += (uint32_t)c3;
	if ((uint32_t)maxDev > w->worstDev) {
		w->worstDev = (uint32_t)maxDev; w->worstPct100 = (uint16_t)maxPct100;
		w->worstPh = (uint16_t)maxPh; w->worstK = (uint16_t)maxK; w->worstPk = (uint32_t)pkV;
	}
	/* 콘솔 자동 요약로그(주기). 상세추적은 shell WVF 명령으로.
	 *  worstMax=관측 시작 이후 누적 최대편차(raw, 신호대비%, 피크, 위치). 진짜 스파이크면 %가 큼.
	 *  lag=캡처밀림(gap late>24ms 횟수/최대ms, pageLag). 스파이크와 밀림을 한 줄에서 대조. */
	if (WVF_LOG_EVERY && (w->frm % WVF_LOG_EVERY) == 0u) {
		printf("[WVF M%d] frm=%u >0.5%%=%u >1%%=%u >2%%=%u >3%%=%u | 누적Max=%u(%u.%02u%% pk%u ph%u k%u)"
		       " | lag late=%u gapMx=%ums pgLag=%u\n",
		       id, w->frm, w->s05, w->s1, w->s2, w->s3,
		       w->worstDev, w->worstPct100/100, w->worstPct100%100,
		       w->worstPk, w->worstPh, w->worstK,
		       wvfGapLate[id], wvfGapMax[id], wvfLagCnt[id]);
	}
}
#endif /* WV_DIAG */

void FFT_Task(void)
{
	METERING  *pmeter= &meter[0].meter;
	CNTL_DATA	*pcntl = &meter[0].cntl;
	HARMONICS *pHD   = &meter[0].hd;
	uint32_t i, j, k, ix=0, laststat=0;//, et1, et2;
//	WAVE_8K_BUF *pwb = &wbFFT8k;
	float thd, fMeas;
	uint64_t t1, t2;
	int id = 0;

	printf("FFT_CZT:%x, size=%d\n", (uint32_t)pFFT, sizeof(*pFFT));
	
	memset(pFFT, 0, sizeof(*pFFT));
	init_Radix2();
	init_CZT();
	// enable 시 wdt reset	
//	_enableTaskMonitor(Tid_FFT, 50);

	while (1) {		
#ifdef __FREERTOS
      uint32_t notificationValue;
      xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);
#else      
		os_evt_wait_and(0x1, 0xffff);				
#endif		
		pcntl->wdtTbl[Tid_FFT].count++;

		/* [수정] 라운드로빈(1채널/notify) → 데이터 있는 채널 모두 처리. M1 파형 off 등으로
		   빈 채널이 슬롯 낭비해 M0 THD 갱신이 느려지던 문제 해결. */
		for (id=0; id<ACTIVE_METER_CH_COUNT; id++)
		if (wbFFT8k[id].fr != wbFFT8k[id].re) {
			pmeter = &meter[id].meter;
			pcntl  = &meter[id].cntl;
			pHD    = &meter[id].hd;

#ifdef WV_DIAG
			wvfDetect(id);	/* [진단] 스파이크 검출·로그(FFT_Task LLOW → Meter 오염 없음) */
#endif

			/* [Goertzel 측정freq] 채널 측정 기본파(45~65Hz면 실측, 밖이면 공칭)로 각 고조파를
			   h·f에서 직접 평가(리샘플 없음). FFT_harmonic에 fMeas 전달. */
			fMeas = (pmeter->Freq >= 45.0f && pmeter->Freq <= 65.0f) ? pmeter->Freq : (float)FREQ_HZ(db.freq);

			// 계산시간 : 160 ms/phase, U/Upp/I 모두 처리하는데  1440ms 소요된다

			t1 = sysTick64;
		if (id == 0) {
			for (i=0; i<3; i++) {
				if (db.pt[id].wiring == WM_3LL3CT || db.pt[id].wiring == WM_3LL2CT) {
					k = (i == 0) ? 0 : (i == 1) ? 2 : 1;
				}
				else
					k = i;

				if (pmeter->U[i] == 0 || meter[id].cntl.online == 0) {	/* 최소 동작전압 미만(오프라인) → 전압 THD 미계산 */
					pmeter->THD_U[i] = pmeter->CF_U[i] = 0;
					memset(pHD->U[i], 0, sizeof(pHD->U)/3);
				}
				else {
					FFT_prepare(wbFFT8k[id].U[k], N_FFT);
					pmeter->CF_U[i] = calcCF(pFFT->xreal, N_FFT);
					/* CZT 직접호출 제거 — 고조파는 아래 FFT_harmonic(Goertzel 측정freq). CZT는 폴백(g_fftGoertzel=0)시 내부에서만 */
					pmeter->THD_U[i] = FFT_harmonic(0, pHD->U[i], fMeas)*100;	// Goertzel 측정freq
				}
			}

			// phase-to-phase voltage
			for (i=0; i<3; i++) {
				if (db.pt[id].wiring == WM_3LL3CT || db.pt[id].wiring == WM_3LL2CT) {
					if(i==0)
						k =0;
					else if(i==1)
						k=2;
					else
						k =1;
					pmeter->CF_Upp[i] = pmeter->CF_U[i];
					pmeter->THD_Upp[i] = pmeter->THD_U[i];
					memcpy(pHD->Upp[i], pHD->U[i], sizeof(pHD->U)/3);
				}
				else {
					if (pmeter->Upp[i] == 0 || meter[id].cntl.online == 0) {	/* 최소 동작전압 미만 → L-L 전압 THD 미계산 */
						pmeter->CF_Upp[i] = pmeter->THD_Upp[i] = 0;
						memset(pHD->Upp[i], 0, sizeof(pHD->Upp)/3);
					}
					else {
						FFT_prepare_pp(wbFFT8k[id].U[i], wbFFT8k[id].U[(i+1)%3], N_FFT);
						pmeter->CF_Upp[i] = calcCF(pFFT->xreal, N_FFT);
						/* CZT 직접호출 제거 — 고조파는 아래 FFT_harmonic(Goertzel 측정freq). CZT는 폴백(g_fftGoertzel=0)시 내부에서만 */
						pmeter->THD_Upp[i] = FFT_harmonic(1, pHD->Upp[i], fMeas)*100;	// Goertzel 측정freq
					}
				}
			}
		}
		else {
			/* M0~M2가 동일 전압 입력 — 전압 고조파(U/Upp)는 M0에서만 FFT 수행하고
			 * 결과를 M1/M2로 복사한다(채널당 U·Upp 6-phase FFT ~960ms 절감).
			 * I 고조파는 아래 루프에서 채널별로 계속 계산. hmd 누산은 복사값 기준으로 fft_average()가 처리.
			 * 전제: M1/M2 wiring이 M0와 동일(전압 공통 입력). */
			METERING  *pm0  = &meter[0].meter;
			HARMONICS *pHD0 = &meter[0].hd;
			for (i=0; i<3; i++) {
				pmeter->THD_U[i]   = pm0->THD_U[i];
				pmeter->CF_U[i]    = pm0->CF_U[i];
				pmeter->THD_Upp[i] = pm0->THD_Upp[i];
				pmeter->CF_Upp[i]  = pm0->CF_Upp[i];
			}
			memcpy(pHD->U,   pHD0->U,   sizeof(pHD->U));
			memcpy(pHD->Upp, pHD0->Upp, sizeof(pHD->Upp));
		}

			for (i=0; i<3; i++) {
				if (pmeter->I[i] < meter[id].cntl.I_start) {	/* 최소 동작전류(시동전류) 미만 → 전류 THD 미계산 */
					pmeter->CF_I[i] = pmeter->KF_I[i] = pmeter->THD_I[i] = pmeter->TDD_I[i] = 0;
					memset(pHD->I[i], 0, sizeof(pHD->I)/3);					
				}
				else {
					float nt, nh, ih;	// nt:단위토탈전류, nh:단위고조파전류, ih:실효고조파전류
					FFT_prepare(wbFFT8k[id].I[i], N_FFT);				
					pmeter->CF_I[i] = calcCF(pFFT->xreal, N_FFT);
					/* CZT 직접호출 제거 — 고조파는 아래 FFT_harmonic(Goertzel 측정freq). CZT는 폴백(g_fftGoertzel=0)시 내부에서만 */				
					pmeter->THD_I[i] = FFT_harmonic(2, pHD->I[i], fMeas)*100;	// Goertzel 측정freq, 단위 고조파 전류
					pmeter->KF_I[i] = calcKF();			
				}
			}			
			t2 = sysTick64;
						
			fft_average(id,pcntl->hmd.ts10m != sysTick10m);
			pcntl->hmd.ts10m = sysTick10m;
			
			maxMinTHD(id);
								
			//printf("VTHD:%f, ITHD:%f, elap = %d\n", pmeter->THD_U[0], pmeter->THD_I[0], (int)(t2-t1));
			wbFFT8k[id].re = wbFFT8k[id].fr;
		}
		//et1 = et2;
		//et2 = getTickCount();
		//printf("fft elap Time = %d\n", et2-et1);	
		//incTaskCount(TASK_FFT);		
	}
}
