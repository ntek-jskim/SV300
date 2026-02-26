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

/** NTC: 10kΩ 직렬 저항, 25°C 기준 10kΩ NTC, Steinhart-Hart 1/T = C1A + C2A*ln(R) + C3A*ln(R)³, T[K]=1/(...) */
#define NTC_ADC_MAX          1024.0f   /* 10-bit ADC */
/** 분압 회로 공급 전압 = ADC 기준 전압 (3V) */
#define NTC_VREF             3.0f
/* 10k@25°C NTC 전형값 (데이터시트에 맞게 조정 가능) */
// #define NTC_C1A              0.001129148f
// #define NTC_C2A              0.000234125f
// #define NTC_C3A              0.0000000876743f
#define NTC_C1A              0.002775575
#define NTC_C2A              0.00024804
#define NTC_C3A              4.90786E-07
#define NTC_SERIES_R_OHM     10000.0
#define NTC_K_TEMP           273.15
/* 접촉/노이즈 시 비정상 값 억제: 다중 샘플 평균, 표시 범위 제한, 급격 변화 제한 */
#define NTC_SAMPLE_COUNT      4
#define NTC_TEMP_MIN         -20.0f
#define NTC_TEMP_MAX         100.0f
#define NTC_MAX_STEP_C       15.0f   /* 1회 갱신당 최대 변화량(°C), 그 이상이면 이전값 유지 */

static ADC_CLOCK_SETUP_T adcSetup;
static bool s_initialized = false;
static float s_lastTemp[TEMP_SENSOR_NUM_CHANNELS];
static bool s_lastValid[TEMP_SENSOR_NUM_CHANNELS];

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
	uint32_t avg_sum = 0;
	uint32_t cr_save, dr;

	if (adcChannel > ADC_CH7 || !raw)
		return false;
#if 0
	// 1. 해당 채널만 선택 (하위 8비트)
    LPC_ADC0->CR = (LPC_ADC0->CR & ~0xFFUL) | (1UL << adcChannel);

	// 2. 변환 시작
	LPC_ADC0->CR |= (1UL << 24); // START NOW (ADC_START_NOW)

	// 3. 완료 대기 (해당 채널의 DONE 비트 확인)
	// 개별 채널 데이터 레지스터(DR[adcChannel])를 직접 모니터링
	while (!((LPC_ADC0->DR[adcChannel]) & (1UL << 31)) && --timeout);

	if (timeout == 0) return false;

	// 4. 데이터 추출 (중요!)
	// LPC43xx의 10비트 데이터는 DR 레지스터의 [15:6] 비트에 위치합니다.
	dr = LPC_ADC0->DR[adcChannel];
	*raw = (uint16_t)((dr >> 6) & 0x3FF); // 6비트 시프트 후 10비트 마스킹
#else
	/* 비버스트 모드: 여러 채널이 선택되면 LPC43xx는 최하위 채널만 변환하므로,
	   변환할 채널만 선택 (나머지 채널 비트 제거) */
	cr_save = LPC_ADC0->CR;
	LPC_ADC0->CR = (cr_save & ~ADC_CR_START_MASK & ~(0xFFUL)) | ADC_CR_CH_SEL(adcChannel);

	/* 4채널 Enable 시 1채널만 Enable할 때보다 계측 차이 나는 이유:
	 * 내부 멀티플렉서가 여러 채널 비트에서 한 채널로 바뀐 직후, S/H 커패시터가
	 * 해당 채널 전압으로 충분히 충전되기 전에 변환하면 이전 채널(0V 등) 잔류 영향.
	 * 미연결 채널이 0V여도, MUX 정착 시간 부족 시 39k 등 연결 채널 값이 틀어짐. */
	{
		uint32_t ch_bits = cr_save & 0xFFUL;
		uint32_t n_ch = 0;
		while (ch_bits) { n_ch += (ch_bits & 1u); ch_bits >>= 1; }
		{ volatile uint32_t d = (n_ch > 1) ? 800u : 40u; while (d--) (void)0; }
	}

	Chip_ADC_SetStartMode(LPC_ADC0, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
	// [추가] 첫 번째 변환은 버림 (Dummy Read)
    while (Chip_ADC_ReadStatus(LPC_ADC0, adcChannel, ADC_DR_DONE_STAT) != SET);
    Chip_ADC_ReadValue(LPC_ADC0, adcChannel, &data); 
    
    // [재시작] 실제 사용할 변환 시작
    Chip_ADC_SetStartMode(LPC_ADC0, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    while (Chip_ADC_ReadStatus(LPC_ADC0, adcChannel, ADC_DR_DONE_STAT) != SET);

	Chip_ADC_ReadValue(LPC_ADC0, adcChannel, &data);
	*raw = data;
#endif
	return true;
}

void TempSensor_Init(void)
{
	if (s_initialized)
		return;

	memset(&adcSetup, 0, sizeof(adcSetup));
	Chip_ADC_Init(LPC_ADC0, &adcSetup);
//	Chip_ADC_SetSampleRate(LPC_ADC0, &adcSetup, 400000);
	Chip_ADC_SetSampleRate(LPC_ADC0, &adcSetup, 50000);
	Chip_ADC_SetResolution(LPC_ADC0, &adcSetup, ADC_10BITS);

	/* Enable ADC0_1 ~ ADC0_4 (4 channels) */
	Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH1, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH2, ENABLE);
	// Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH3, ENABLE);
	// Chip_ADC_EnableChannel(LPC_ADC0, ADC_CH4, ENABLE);

//	Chip_ADC_SetStartMode(LPC_ADC0, ADC_NO_START, ADC_TRIGGERMODE_RISING);
	{
		int i;
		for (i = 0; i < TEMP_SENSOR_NUM_CHANNELS; i++)
			s_lastValid[i] = false;
	}
	s_initialized = true;
}

bool TempSensor_ReadTempC(int channel, int16_t *temp_c)
{
	uint16_t raw;
	float tVolt, tCurr, tRVal, temp, logval;

	if (!s_initialized || !temp_c || channel < 0 || channel >= TEMP_SENSOR_NUM_CHANNELS)
		return false;

	if (!readAdcRaw((uint8_t)s_adcChannels[channel], &raw))
		return false;

	/* V_adc = Vref*R_ntc/(R_series+R_ntc) → R_ntc = R_series*V_adc/(Vref-V_adc) */
	tVolt = (float)raw / NTC_ADC_MAX * NTC_VREF;

	if (tVolt > 0.0f && tVolt < (float)NTC_VREF - 0.01f) {
		tCurr = tVolt / NTC_SERIES_R_OHM;
		tRVal = NTC_SERIES_R_OHM * tVolt / ((float)NTC_VREF - tVolt);
		/* Steinhart-Hart 계수는 R[Ω] 기준: ln(R) 사용 (ln(kΩ) 아님) */
		logval = (float)log((double)tRVal);
		temp = 1.0f / (NTC_C1A + NTC_C2A * logval + NTC_C3A * logval * logval * logval) - NTC_K_TEMP;
		*temp_c = (int16_t)(temp + (temp >= 0.0f ? 0.5f : -0.5f));
	} else {
		*temp_c = 0;
	}
	return true;
}

bool TempSensor_ReadTempCFloat(int channel, float *temp_c)
{
	uint16_t raw;
	uint32_t sum = 0;
	int i;
	float tVolt, tRVal, logval, t;

	if (!s_initialized || !temp_c || channel < 0 || channel >= TEMP_SENSOR_NUM_CHANNELS)
		return false;

	/* 다중 샘플 평균: 접촉/노이즈로 인한 순간 오차 완화 */
	for (i = 0; i < NTC_SAMPLE_COUNT; i++) {
		if (!readAdcRaw((uint8_t)s_adcChannels[channel], &raw))
			return false;
		sum += raw;
	}
	raw = (uint16_t)(sum / NTC_SAMPLE_COUNT);

	/* V_adc = Vref*R_ntc/(R_series+R_ntc) → R_ntc = R_series*V_adc/(Vref-V_adc) */
	tVolt = (float)raw / NTC_ADC_MAX * NTC_VREF;

	if (tVolt > 0.0f && tVolt < (float)NTC_VREF - 0.01f) {
		tRVal = NTC_SERIES_R_OHM * tVolt / ((float)NTC_VREF - tVolt);
		if (tRVal < 100.0f) {
			if (s_lastValid[channel]) {
				*temp_c = s_lastTemp[channel];
				return true;
			}
			*temp_c = 0.0f;
			return true;
		}
		/* Steinhart-Hart: 1/T = A + B*ln(R) + C*ln(R)³, R은 Ω */
		logval = (float)log((double)tRVal);
		t = 1.0f / (NTC_C1A + NTC_C2A * logval + NTC_C3A * logval * logval * logval) - (float)NTC_K_TEMP;
		printf("ADC0_%d Temp : %d, %.2f°C\n", channel + 1, raw, t);

		if (t != t || t < -200.0f || t > 300.0f) {
			if (s_lastValid[channel]) {
				*temp_c = s_lastTemp[channel];
				return true;
			}
			*temp_c = 0.0f;
			return true;
		}
		if (t < NTC_TEMP_MIN)
			t = NTC_TEMP_MIN;
		else if (t > NTC_TEMP_MAX)
			t = NTC_TEMP_MAX;
		if (s_lastValid[channel]) {
			float prev = s_lastTemp[channel];
			if (t > prev + NTC_MAX_STEP_C)
				t = prev + NTC_MAX_STEP_C;
			else if (t < prev - NTC_MAX_STEP_C)
				t = prev - NTC_MAX_STEP_C;
		}
		s_lastTemp[channel] = t;
		s_lastValid[channel] = true;
		*temp_c = t;
	} else {
		if (s_lastValid[channel])
			*temp_c = s_lastTemp[channel];
		else
			*temp_c = 0.0f;
	}
	return true;
}

bool TempSensor_ReadResistanceOhms(int channel, float *r_ohm)
{
	uint16_t raw;
	uint32_t sum = 0;
	int i;
	float tVolt, r_ntc;

	if (!s_initialized || !r_ohm || channel < 0 || channel >= TEMP_SENSOR_NUM_CHANNELS)
		return false;

	for (i = 0; i < NTC_SAMPLE_COUNT; i++) {
		if (!readAdcRaw((uint8_t)s_adcChannels[channel], &raw))
			return false;
		sum += raw;
	}
	raw = (uint16_t)(sum / NTC_SAMPLE_COUNT);

	tVolt = (float)raw / NTC_ADC_MAX * NTC_VREF;

	if (tVolt <= 0.01f || tVolt >= (float)NTC_VREF - 0.01f) {
		*r_ohm = 0.0f;
		return true;
	}
	/* R_ntc [Ω] = R_series * V_adc / (Vref - V_adc) */
	r_ntc = NTC_SERIES_R_OHM * tVolt / ((float)NTC_VREF - tVolt);
	*r_ohm = r_ntc;
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
