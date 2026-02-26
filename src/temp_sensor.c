/**
 * @brief Temperature sensor driver using LPC43xx ADC0 channels 1~4 (ADC0_1 ~ ADC0_4)
 *        NTC thermistor (10k @ 25°C) with 10k series resistor, Steinhart-Hart.
 *
 * [미연결 채널 영향]
 * - 1채널에만 NTC를 연결하고 나머지는 비어 있으면, 다른 채널 값이 1채널에 영향을 받는 것처럼 보일 수 있음.
 * - 원인 1: ADC0_2~4 핀이 플로팅이면 전압이 불안정하고, 인접한 ADC0_1 노이즈/전압에 유도됨.
 * - 원인 2: 내부 멀티플렉서/샘플홀드에 이전 채널(1번) 변환 전압이 잔류한 상태에서 다음 채널 변환 시
 *           그 값이 섞여 나올 수 있음 (채널 전환 직후 변환 시).
 * - 권장: 미사용 채널 핀은 GND 또는 Vref/2에 10k~100k 저항으로 연결하여 플로팅 제거.
 */
#include "board.h"
#include "chip.h"
#include "temp_sensor.h"
#include <string.h>
#include <math.h>

/** NTC: 10kΩ 직렬 저항, 25°C 기준 10kΩ NTC, Steinhart-Hart 계수 */
#define NTC_SERIES_R_OHM    10000.0f
#define NTC_VREF             3.3f
#define NTC_ADC_MAX          1024.0f   /* 10-bit ADC */
// #define NTC_C1A              0.001129148f
// #define NTC_C2A              0.000234125f
// #define NTC_C3A              0.0000000876743f
#define NTC_C1A              0.002775575
#define NTC_C2A              0.00024804
#define NTC_C3A              0.002775575
#define NTC_K_TEMP           273.15f

static ADC_CLOCK_SETUP_T adcSetup;
static bool s_initialized = false;

/** ADC0 channel 1~4 (ADC_CH1 ~ ADC_CH4) */
static const ADC_CHANNEL_T s_adcChannels[TEMP_SENSOR_NUM_CHANNELS] = {
	ADC_CH1,  /* ADC0_1 */
	ADC_CH2,  /* ADC0_2 */
	ADC_CH3,  /* ADC0_3 */
	ADC_CH4   /* ADC0_4 */
};

static bool readAdcRaw(uint8_t adcChannel, uint16_t *raw)
{
	uint32_t timeout = 100000;
	uint16_t data;
	uint32_t cr_save;

	if (adcChannel > ADC_CH7 || !raw)
		return false;

	/* 비버스트 모드: 여러 채널이 선택되면 LPC43xx는 최하위 채널만 변환하므로,
	   변환할 채널만 선택 (나머지 채널 비트 제거) */
	cr_save = LPC_ADC0->CR;
	LPC_ADC0->CR = (cr_save & ~ADC_CR_START_MASK & ~(0xFFUL)) | ADC_CR_CH_SEL(adcChannel);

	/* 채널 전환 후 멀티플렉서/입력 정착 시간 (미연결 채널은 이전 채널 전압 잔류 가능) */
	{ volatile uint32_t d = 20; while (d--) (void)0; }

	Chip_ADC_SetStartMode(LPC_ADC0, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
	while (Chip_ADC_ReadStatus(LPC_ADC0, adcChannel, ADC_DR_DONE_STAT) != SET && --timeout)
		;
	LPC_ADC0->CR = cr_save;  /* 채널 선택 복원 (ADC0_1~4 모두 활성) */
	if (!timeout)
		return false;
	if (Chip_ADC_ReadValue(LPC_ADC0, adcChannel, &data) != SUCCESS)
		return false;
	*raw = data;
	return true;
}

void TempSensor_Init(void)
{
	if (s_initialized)
		return;
	memset(&adcSetup, 0, sizeof(adcSetup));
	Chip_ADC_Init(LPC_ADC0, &adcSetup);
	Chip_ADC_SetSampleRate(LPC_ADC0, &adcSetup, 400000);
	Chip_ADC_SetResolution(LPC_ADC0, &adcSetup, ADC_10BITS);

	/* Enable ADC0_1 ~ ADC0_4 (4 channels) */
	Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH1, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH2, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH3, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH4, ENABLE);

#if 1
	// Burst Mode
	// 하드웨어 자동화: 한 번 시작(Enable)하면 CPU가 "변환 시작" 명령을 매번 내릴 필요가 없습니다.
	// 순차 스캔: SEL 비트마스크로 선택된 모든 채널을 낮은 번호부터 높은 번호 순서대로 변환합니다.
	// 반복 수행: 마지막 채널의 변환이 끝나면 다시 첫 번째 채널로 돌아가 무한 반복합니다.
	Chip_ADC_SetBurstCmd(LPC_ADC0, ENABLE);
#else	
	Chip_ADC_SetStartMode(LPC_ADC0, ADC_NO_START, ADC_TRIGGERMODE_RISING);
#endif	
	s_initialized = true;
}

bool TempSensor_ReadTempC(int channel, int16_t *temp_c)
{
	uint16_t raw;
	uint32_t sum = 0;
	int i, n=10;
	float tVolt, tCurr, tRVal, fval, temp, logval;

	if (!s_initialized || !temp_c || channel < 0 || channel >= TEMP_SENSOR_NUM_CHANNELS)
		return false;

#if 1
	// 10 회 연속으로 읽고 평균
	/* Waiting for A/D conversion complete */
	for (i=0; i<n; i++) {
		while (Chip_ADC_ReadStatus(LPC_ADC0, s_adcChannels[channel], ADC_DR_DONE_STAT) != SET) {}
		/* Read ADC value */
		Chip_ADC_ReadValue(LPC_ADC0, s_adcChannels[channel], &raw);
		sum += raw;
	}				
   raw = sum / n;
#else	
	if (!readAdcRaw((uint8_t)s_adcChannels[channel], &raw))
		return false;
#endif
	
#if 1
	*temp_c = raw;
#else
	/* NTC: V = ADC * 3.3/1024, I = V/10k, R_ntc = 3.3/I - 10k, Steinhart-Hart */
	tVolt = (float)raw / NTC_ADC_MAX * NTC_VREF;
	tCurr = tVolt / NTC_SERIES_R_OHM;

	if (tCurr > 0.0f) {
		tRVal = NTC_VREF / tCurr - NTC_SERIES_R_OHM;
		fval = tRVal / 1000.0f;   /* kΩ */
		logval = (float)log((double)fval);
		temp = 1.0f / (NTC_C1A + NTC_C2A * logval + NTC_C3A * logval * logval * logval) - NTC_K_TEMP;
		*temp_c = (int16_t)(temp + (temp >= 0.0f ? 0.5f : -0.5f));
	} else {
		*temp_c = 0;
	}
#endif
	return true;
}

bool TempSensor_ReadTempCx10(int channel, int16_t *temp_c10)
{
	int16_t temp_c;

	if (!TempSensor_ReadTempC(channel, &temp_c))
		return false;
	*temp_c10 = temp_c * 10;
	return true;
}
