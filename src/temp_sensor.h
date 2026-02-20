/**
 * @brief Temperature sensor driver using ADC0 channels 1~4 (ADC0_1 ~ ADC0_4)
 */
#ifndef TEMP_SENSOR_H_
#define TEMP_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>

/** Number of ADC channels (ADC0_1, ADC0_2, ADC0_3, ADC0_4) */
#define TEMP_SENSOR_NUM_CHANNELS  4

/** Channel index for ADC0_1 ~ ADC0_4 (0 ~ 3) */
#define TEMP_SENSOR_CH_ADC0_1  0
#define TEMP_SENSOR_CH_ADC0_2  1
#define TEMP_SENSOR_CH_ADC0_3  2
#define TEMP_SENSOR_CH_ADC0_4  3

/**
 * @brief Initialize ADC0 and enable channels 1~4 (ADC0_1 ~ ADC0_4)
 */
void TempSensor_Init(void);

/**
 * @brief Read temperature in °C for the given channel
 * @param channel Channel index 0~3 (ADC0_1 ~ ADC0_4)
 * @param temp_c  Output temperature in °C
 * @return true on success, false on failure
 */
bool TempSensor_ReadTempC(int channel, int16_t *temp_c);

/**
 * @brief Read temperature in °C x 10 for the given channel (e.g. 253 = 25.3°C)
 * @param channel  Channel index 0~3 (ADC0_1 ~ ADC0_4)
 * @param temp_c10 Output temperature * 10
 * @return true on success, false on failure
 */
bool TempSensor_ReadTempCx10(int channel, int16_t *temp_c10);

#endif /* TEMP_SENSOR_H_ */
