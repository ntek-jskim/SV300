#include "RTL.h"
#include "board.h"
#include "math.h"
#include "meter.h"
#include "fft.h"
#include "string.h"

extern uint64_t sysTick64;

//U8 static _sdram_area[0x100000] __attribute__ ((section ("FFT_RAM"), zero_init));
//FFT_BUF *pfft=(FFT_BUF *)_sdram_area;
//float	cosTable[MAX_FFT_LEN] __attribute__ ((section ("EXT_RAM"), zero_init));
//FFT_BUF fftbuf __attribute__ ((section ("EXT_RAM"), zero_init));
//FFT_BUF *pfft = &fftbuf;

extern WAVE8k_PGBUF	w8kQ;
extern float scaleVrms(long val);

int prtFlag;

void readWave8kData()
{
	int i, j, *phase, sx=0, dx=0;
	int32_t *pwv;
		
#ifdef _PREPROCESS	
	for (i=0; i<64; i++) {
		pwv = w8kQ.wb[i];
		sx = 0;
		for (j=0; j<16; j++) {					
			//memcpy(w1024.wv[dx].phase, &pub[sx], sizeof(int32_t)*6);
			wb1k.wv[dx][0] = pwv[sx++];
			wb1k.wv[dx][1] = pwv[sx++];
			wb1k.wv[dx][2] = pwv[sx++];
			wb1k.wv[dx][3] = pwv[sx++];
			wb1k.wv[dx][4] = pwv[sx++];
			wb1k.wv[dx][5] = pwv[sx++];
			dx++;
		}
	}		
	wb1k.fr++;

	if (dx >= 1024) {
		dx = 0;
		if (prtFlag == 0) {
			for (i=0; i<1024; i++) {
				printf("%04d, %8d, %8d, %8d\n", i, 
					wb1k.wv[i][1], wb1k.wv[i][3], wb1k.wv[i][5]);
			}			
			prtFlag = 1;
		}
	}
	//printf("Read WaveSample ...\n");
	
	if (prtFlag == 0) {
		for (i=0; i<1024; i++) {
			printf("%04d, %8d, %8d, %8d\n", i, 
				wb1k.wv[i][1], wb1k.wv[i][3], wb1k.wv[i][5]);
		}			
		prtFlag = 1;
	}	
#endif
}


void init_fftTable(void)
{
	int i;
	
	memset(pfft, 0, sizeof(*pfft));
	pfft->nfft = MAX_FFT_LEN;
	pfft->fres = 8000./MAX_FFT_LEN;
	
	for (i=0; i<MAX_FFT_LEN; i++) {
		cosTable[i] = 0.5 * (1-cos(2.*PI*i/(MAX_FFT_LEN-1)));
//		v = 0.5 * (1-cos(2.*PI*i/(MAX_FFT_LEN-1)));
//		cosTable[i] = 0.5 * (1-arm_cos_f32(2.*PI*i/(MAX_FFT_LEN-1)));
//		if (v != cosTable[i]) 
//			printf("%.7f, %.7f\n", v, cosTable[i]);
	}
}


void bitrev_permute(float *z, uint32_t n)
{
    uint32_t i;
    unsigned int ldn = 0;
    unsigned int rshift;
    
    i = n;
    while (i >>= 1)
        ++ldn;
    rshift = 8 * (unsigned int)sizeof(uint32_t) - ldn;
    for (i = 0; i < n; ++i) {
        uint32_t r;
        r = ((i & 0x55555555) << 1) | ((i & ~0x55555555) >> 1);
        r = ((r & 0x33333333) << 2) | ((r & ~0x33333333) >> 2);
        r = ((r & 0x0f0f0f0f) << 4) | ((r & ~0x0f0f0f0f) >> 4);
        r = ((r & 0x00ff00ff) << 8) | ((r & ~0x00ff00ff) >> 8);
        r = (r << 16) | (r >> 16);
        r >>= rshift;
        if (r > i) {
            float tmp;
            uint32_t i2;
            i2 = i << 1;
            r <<= 1;
            tmp = z[i2]; z[i2] = z[r]; z[r] = tmp;
            tmp = z[i2+1]; z[i2+1] = z[r+1]; z[r+1] = tmp;
        }
    }
    return;
}   /* bitrev_permute() */

void fft_dif_iter(float *z, uint32_t n)
{
    uint32_t i, n2;

    n2 = n << 1;
    for (i = n; i > 1; i >>= 1) {
        float a, b, c, s, t;
        uint32_t i2, j;
        i2 = i << 1;
        t = _2PI / (float)i;
        a = sin(0.5 * t); //a = arm_sin_f32(0.5 * t);				
        a *= 2.0 * a;
        b = sin(t); //b = arm_sin_f32(t);				
        c = 1.0;
        s = 0.0;
        for (j = 0; j < i; j += 2) {
            float tmp;
            uint32_t kr, kmax;
            kmax = n2 + j;
            for (kr = j; kr < kmax; kr += i2) {
                float ur, ui;
                uint32_t ki, mr, mi;
                ki = kr + 1;
                mr = kr + i;
                mi = mr + 1;
                ur = z[kr];
                ui = z[ki];
                z[kr] = ur + z[mr];
                z[ki] = ui + z[mi];
                ur -= z[mr];
                ui -= z[mi];
                z[mr] = ur * c - ui * s;
                z[mi] = ur * s + ui * c;
            }
            tmp = c;
            c -= a * c + b * s;
            s -= a * s - b * tmp;
        }
    }
    return;
}   /* fft_dif_iter() */


// 0.5*(1-cos(2*PI*n/(N-1))

void hanning(float *z)
{
	int i, ix, n;

	n = pfft->nfft;
	for (ix=0, i=0; i<n; i++, ix+=2) {
		//z[ix++] = z[ix] * 0.5 * (1-arm_cos_f32(2.*PI*i/(n-1)));
		z[ix] = z[ix] * cosTable[i];		
	}	
}


int fft(int n, float *z, float *amp)
{
	int i, fi, ix=0;
	float fmax;

	fft_dif_iter(z, n);
	bitrev_permute(z, n);

	for (fmax=0, i=0; i<(n/2); i++, ix+=2) {
		//ix = i*2;
		amp[i] = sqrt(z[ix]*z[ix] + z[ix+1]*z[ix+1]); 
		if (amp[i] > fmax) {
			fmax = amp[i];
			fi = i;
		}
	}

	//printf("FI = %d, fmax=%f, amp=%f\n", fi, fmax, amp[fi]);
	for (i= 0; i<(n/2); i++) {
		//amp[i] = 20.*log10(amp[i]/fmax);
		amp[i] = (amp[i]/fmax);
	}
	
	return fi;
}

// *phd : xHD
// *thds : Total, Even, Odd, Zero, Pos, Neg
// 2nd -> index 1
// 3rd -> index 2
// ...
// 63rd -> index 62

// *phd : xHD
// *thds : Total, Even, Odd, Zero, Pos, Neg
// 2nd -> index 1
// 3rd -> index 2
// ...
// 63rd -> index 62
void getHarmonics(uint16_t *pHD)
{
	int i, fi, sel;
	int freq;
	float	*fftOut = pfft->fft_obuf, ftemp, thd=0;

	freq = (pdb->freq == 0) ? 60 : 50;
	
	// 2 부터 63 고조파 추출한다
	// 0:1st, 1:2nd, 2:3rd, 3:4th, ...
	//phd[0] = 10000;	// 1st: 100%
	for (thd=0, i=2; i<=63; i++) {
		fi = i*freq/pfft->fres;
		if (fftOut[fi] > fftOut[fi-1]) {
			sel = (fftOut[fi] > fftOut[fi+1]) ? fi : fi+1;
		}
		else {
			sel = (fftOut[fi-1]>fftOut[fi+1]) ? fi-1 : fi+1;
		}		
	  // harmoics를 각 차수별(2~63)로 %로 환산하여 저장
		pHD[i] = fftOut[sel]*10000;			
		ftemp  = fftOut[sel]*fftOut[sel];
		thd   += ftemp;
	}
	
	// 기본파(1차) : 100%
	pHD[1] = 10000;
	pHD[0] = sqrt(thd)*10000;
}

float calcCF(float *pSample, int length) {
	float rms=0, v=0, max=0; 
	int i, ix;
	
	for (ix=0, i=0; i<length; i++, ix+=2) {
		v = (pSample[ix]*pSample[ix]);
		if (max < v) max = v;
		rms += v;		
	}
	rms = sqrt(rms/length);
	max = sqrt(max);
		
	v = max/rms;
	return v;
}


void fftSetBuffer(int32_t *pWD, int len, int32_t *pAW, int mode) {
	int i, dx;
	
	if (mode) {
		for (dx=0, i=0; i<len; i++) {
			pfft->fft_ibuf[dx++] = pAW[i] - pWD[i];
			pfft->fft_ibuf[dx++] = 0;
		}
	}
	else {
		for (dx=0, i=0; i<len; i++) {
			pfft->fft_ibuf[dx++] = pWD[i], 
			pfft->fft_ibuf[dx++] = 0;		
		}
	}
}

// - fft input buffer: real[0], img[0], real[1], img[1], ...
// - dc offset 제거 추가
// - Crest Factor 계산
void fftPreprocess(float *pCF)
{
	int i, dx;
	float	dcOffset=0;
	
	for (dx=0, i=0; i<pfft->nfft; i++, dx+=2) {
		dcOffset += pfft->fft_ibuf[dx];
	}
	dcOffset /= pfft->nfft;		
	
	for (dx=0, i=0; i<pfft->nfft; i++, dx+=2) {
		pfft->fft_ibuf[dx] -= dcOffset;
	}		
	
	*pCF = calcCF(pfft->fft_ibuf, pfft->nfft);
}

// K-Factor
// 2017-9-27 : 소수점 자리 조정
float calcKF(uint16_t *hd)
{
	float In_I, I_rms, sum=0;
	int i;
	
	// Ih/I1
	// 1 to 63, odd harmonics 만 계산
	// I_rms를 구한다 
	sum = 0;
	for (i=1; i<=63; i+=2) {
		In_I = (i == 1) ? 1. : hd[i-1]/10000.;
		sum += In_I * In_I;
	}	
	I_rms = sqrt(sum);	
	
	// 차수별 고조파를 I_rms로 나눈다
	// sum = k-factor
	sum = 0;
	for (i=1; i<=63; i+=2) {		
		In_I = (i == 1) ? 1. : hd[i-1]/10000.;
		In_I = In_I / I_rms;
		sum += In_I * In_I * i * i;
	}		
	
	//return sum*1000;
	return sum*100;
}

//
// fftProcess(
// pSD: sampling data
// pCF: Crest Factor
// pTHD: Total Harmonics Distortion
// pHD: 1~63 harmonics
void fftProcess(float *pCF, int16_t *pHD) {
	fftPreprocess(pCF);
	hanning(pfft->fft_ibuf);
	fft(pfft->nfft, pfft->fft_ibuf, pfft->fft_obuf);	
	getHarmonics(pHD);			
}

// thd 연산시간 : 63ms
__task void FFT_task(void) 
{
	uint32_t i, j, ix=0, laststat=0;//, et1, et2;
	WAVE_1024_BUF *pwb = &wb1k;
	float thd;
	uint64_t t1, t2;
	
	init_fftTable();
	
	//et1 = getTickCount();
	while (1) {		
		os_evt_wait_and(0x1, 0xffff);				
		
		t1 = sysTick64;
		
		//readWave8kData();
	
		// 정전상태에서 fft 연산을 하지 않는다, THD값은 지운다 
		if (pcntl->online == 0) {
			if (laststat) {
				for (i=0; i<4; i++) {
					pmeter->THD_U[i] = 0;
					pmeter->THD_I[i] = 0; 
				}
				memset(pHD, 0, sizeof(*pHD));
				laststat = 0;
			}	
		}
		else {
			laststat = pcntl->online;
				
			// wave stream : IA/VA/IB/VB/IC/VC/IN
			// Current
			for (i=0; i<3; i++) {
				if (pmeter->I[i] == 0) {
					memset(pHD->I[i], 0, sizeof(pHD->I[i]));
					memset(pmeter->CF_I, 0, sizeof(pmeter->CF_I));
					memset(pmeter->KF_I, 0, sizeof(pmeter->KF_I));
				}
				else {							
					fftSetBuffer(wb1k.I[i], pfft->nfft, NULL, 0);
					fftProcess(&pmeter->CF_I[i], pHD->I[i]);	
					pmeter->KF_I[i] = calcKF(pHD->I[i]);	
				}
			}

			// U
			for (i=0; i<3; i++) {
				if (pmeter->U[i] == 0) {
					memset(pHD->U[i], 0, sizeof(pHD->U[i]));
					memset(pmeter->CF_U, 0, sizeof(pmeter->CF_U));
				}
				else {				
					fftSetBuffer(wb1k.U[i], pfft->nfft, NULL, 0);
					fftProcess(&pmeter->CF_I[i], pHD->U[i]);
					
//					if (i==0) {
//						for (j=0; j<1024; j++) {
//							pSp->sample[j] = pfft->fft_obuf[j]*10000;
//						}
//					}
				}
			}
			
			// Upp
			for (i=0; i<3; i++) {				
				if (pmeter->Upp[i] == 0) {
					memset(pHD->Upp[i], 0, sizeof(pHD->Upp[i]));
					memset(pmeter->CF_Upp, 0, sizeof(pmeter->CF_Upp));					
				}
				else {				
					fftSetBuffer(wb1k.U[i], pfft->nfft, wb1k.U[(i+1)%3], 1);
					fftProcess(&pmeter->CF_Upp[i], pHD->Upp[i]);
				}
			}			
		}
				
#ifdef _WAVE_DISP				
		// wave 표시창 용 데이터
		// convert 24bit data to 16bit data
		// range: +/-5928256 to +/-23157	
		if (pwb->fr == pwb->re) {			
			for (i=0; i<N_WWIN; i++) {
				for (j=0; j<6; j++) {
					pwv->nwave[i][j] = pwb->wv[i][j]>>8;	
				}
			}
			pwv->fr++;	
			//printf("generate wave, fr=%d\n", pwv->fr);
		}
		
		// modbus wave 요청시에만 생성한다
		if (pwbuf->req) {
			for (i=0; i<N_WV16; i++) {
				for (j=0; j<6; j++) {
					pwbuf->nwave[i][j] = pwb->wv[i][j]>>8;	
				}
			}
			pwbuf->seq++;
			pwbuf->req = 0;
			pwbuf->w_mode = pdb->wiring;
			for (i=0; i<3; i++) pwbuf->ct_dir[i] = pdb->ct_dir[i];
			printf("buildWV16, seq=%d\n", pwbuf->seq);
		}	
				
		pcntl->wre = getNextIdx(pcntl->wre);	
		pcntl->mmChangeF += minmaxTHD();
#endif
		t2 = sysTick64;
		
		printf("VTHD:%f, ITHD:%f, elap = %d\n", pmeter->THD_U[0], pmeter->THD_I[0], (int)(t2-t1));
		wb1k.re = wb1k.fr;
		//et1 = et2;
		//et2 = getTickCount();
		//printf("fft elap Time = %d\n", et2-et1);	
		//incTaskCount(TASK_FFT);		
	}
}
