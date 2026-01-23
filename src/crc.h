#ifndef _CRC_H

#define	_CRC_H

uint16_t gencrc_modbus(uint8_t * ptr, int len);
uint32_t gencrc_crc32(const uint8_t *mem, uint32_t size, uint32_t CRC);
void makeCRC32table(void);

#endif
