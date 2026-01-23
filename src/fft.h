#ifndef	_FFT_H

//#include "_arm_math.h"
//#include "meter.h"
#include "board.h"

#define	_FFT_V_2

#define	PI	(3.14159265358979)
#define	_2PI	(PI*2)

//#define	MAX_FFT_LEN	1024
#define	MAX_FFT_LEN	1024

typedef struct {
	uint32_t	nfft;		// fft length 
	uint32_t	srate;	// high speed sampling rate
	float	fres;

	float	fft_ibuf[MAX_FFT_LEN*2];
	float	fft_obuf[MAX_FFT_LEN];
} FFT_BUF;



//extern volatile FFT_BUF *pfft;
extern FFT_BUF *pfft;

int  fft(int n, float *z, float *amp);
void getHarmonics(uint16_t *pHD);
//void fft_process(WV32_BUF *pwv, int ix);
void init_fftTable(void);

#endif
