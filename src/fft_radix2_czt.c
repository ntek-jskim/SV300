#include "RTL.h"
#include "board.h"
#include "math.h"
#include "meter.h"
#include "fft.h"
#include "string.h"

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



void FFT_prepare(int32_t sample[], int n) {
	int i;
	//float freq = 8000./lineFreq, dcOffset=0;
	
	for (i=0; i<n; i++) {
		// Test Wave Signal
		//pFFT->xreal[i] = 10000 * sin(2 * PI * i / freq) + 
		//1000 * sin(6 * PI * i / freq) + 
		//1000 * sin(10 * PI * i / freq) + 
		//1000 * sin(14 * PI * i / freq);			
		pFFT->xreal[i] = sample[i];
		pFFT->ximag[i] = 0;
	}		
}

void FFT_prepare_pp(int32_t s1[], int32_t s2[], int n) {
	int i;
	//float freq = 8000./lineFreq;
	
	for (i=0; i<n; i++) {
		// Test Wave Signal
		//pFFT->xreal[i] = 10000 * sin(2 * PI * i / freq) + 
		//1000 * sin(6 * PI * i / freq) + 
		//1000 * sin(10 * PI * i / freq) + 
		//1000 * sin(14 * PI * i / freq);			
		pFFT->xreal[i] = (s2[i] - s1[i]);
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
		//ix = (i*50)/5;
		ix = i*pdb->pt.freq/5;
		if(sel == 2) {
			if(pdb->pt.freq==50)
				gain = i_harm_gain[0][i];
			else
				gain = i_harm_gain[1][i];
		}
		else {
			if(pdb->pt.freq==50)
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

	res = pdb->pt.freq/5;
//	res = lineFreq/5;

	// irms = h1^2+h2^2+ ....h63^2)
	for (i=1; i<=63; i++) {
		ix = i*res;
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
void FFT_Task(void) 
{
	METERING  *pmeter= &meter[0].meter;
	CNTL_DATA	*pcntl = &meter[0].cntl;
	HARMONICS *pHD   = &meter[0].hd;
	uint32_t i, j, k, ix=0, laststat=0;//, et1, et2;
//	WAVE_8K_BUF *pwb = &wbFFT8k;
	float thd;
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

		if (wbFFT8k[id].fr != wbFFT8k[id].re) {
			pmeter = &meter[id].meter;
			pcntl  = &meter[id].cntl;
			pHD    = &meter[id].hd;
			
			// 계산시간 : 160 ms/phase, U/Upp/I 모두 처리하는데  1440ms 소요된다 

			t1 = sysTick64;							
			for (i=0; i<3; i++) {
				if (pdb->pt.wiring == WM_3LL3CT || pdb->pt.wiring == WM_3LL2CT) {
					k = (i == 0) ? 0 : (i == 1) ? 2 : 1;
				}
				else
					k = i;

				if (pmeter->U[i] == 0) {
					pmeter->THD_U[i] = pmeter->CF_U[i] = 0;
					memset(pHD->U[i], 0, sizeof(pHD->U)/3);
				}
				else {
					FFT_prepare(wbFFT8k[id].U[k], N_FFT);				
					pmeter->CF_U[i] = calcCF(pFFT->xreal, N_FFT);
					FFT_czt(pFFT->xreal, pFFT->ximag, N_FFT, M_FFT);	// 157 ms				
					pmeter->THD_U[i] = FFT_postproc(N_FFT, 0, pHD->U[i])*100;		
				}
			}
			
			// phase-to-phase voltage
			for (i=0; i<3; i++) {
				if (pdb->pt.wiring == WM_3LL3CT || pdb->pt.wiring == WM_3LL2CT) {
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
					if (pmeter->Upp[i] == 0) {
						pmeter->CF_Upp[i] = pmeter->THD_Upp[i] = 0;
						memset(pHD->Upp[i], 0, sizeof(pHD->Upp)/3);					
					}
					else {
						FFT_prepare_pp(wbFFT8k[id].U[i], wbFFT8k[id].U[(i+1)%3], N_FFT);				
						pmeter->CF_Upp[i] = calcCF(pFFT->xreal, N_FFT);
						FFT_czt(pFFT->xreal, pFFT->ximag, N_FFT, M_FFT);	// 157 ms				
						pmeter->THD_Upp[i] = FFT_postproc(N_FFT, 1, pHD->Upp[i])*100;							
					}
				}
			}
			
			for (i=0; i<3; i++) {
				if (pmeter->I[i] == 0) {
					pmeter->CF_I[i] = pmeter->KF_I[i] = pmeter->THD_I[i] = pmeter->TDD_I[i] = 0;
					memset(pHD->I[i], 0, sizeof(pHD->I)/3);					
				}
				else {
					float nt, nh, ih;	// nt:단위토탈전류, nh:단위고조파전류, ih:실효고조파전류
					FFT_prepare(wbFFT8k[id].I[i], N_FFT);				
					pmeter->CF_I[i] = calcCF(pFFT->xreal, N_FFT);
					FFT_czt(pFFT->xreal, pFFT->ximag, N_FFT, M_FFT);	// 157 ms				
					pmeter->THD_I[i] = FFT_postproc(N_FFT, 2, pHD->I[i])*100;	// 단위 고조파 전류
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
		if (++id >= 2) {
			id = 0;
		}
		//et1 = et2;
		//et2 = getTickCount();
		//printf("fft elap Time = %d\n", et2-et1);	
		//incTaskCount(TASK_FFT);		
	}
}
