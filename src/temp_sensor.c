/**
 * @brief Temperature sensor driver using LPC43xx ADC0 channels 1~4 (ADC0_1 ~ ADC0_4)
 */
#include "board.h"
#include "chip.h"
#include "temp_sensor.h"
#include <string.h>

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

	/* 한 번에 한 채널만 선택하여 변환 (비버스트 모드) */
	cr_save = LPC_ADC0->CR;
	LPC_ADC0->CR = (cr_save & ~ADC_CR_START_MASK) | ADC_CR_CH_SEL(adcChannel);

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

	Chip_ADC_SetStartMode(LPC_ADC0, ADC_NO_START, ADC_TRIGGERMODE_RISING);
	s_initialized = true;
}

bool TempSensor_ReadTempC(int channel, int16_t *temp_c)
{
	uint16_t raw;

	if (!s_initialized || !temp_c || channel < 0 || channel >= TEMP_SENSOR_NUM_CHANNELS)
		return false;

	if (!readAdcRaw((uint8_t)s_adcChannels[channel], &raw))
		return false;

	/* Simple linear conversion: 10-bit ADC, assume 0~3.3V -> 0~100°C scale
	 * temp_c = (raw * 100) / 1024; calibrate per hardware if needed */
	*temp_c = (int16_t)((raw * 100) / 1024);
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
