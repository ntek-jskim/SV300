#ifndef _SSPLIB_H

#define	_SSPLIB_H

int read_reg16(uint8_t mid, uint16_t cmd, uint16_t *pdata);
int read_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata);
int read_reg32n(uint8_t mid, uint16_t cmd, uint32_t *buf, int n);
int write_reg16(uint8_t mid, uint16_t cmd, uint16_t *pdata);
int write_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata);

int dma_read16(uint8_t mid, uint16_t cmd, uint16_t *pdata);
int dma_read32(uint8_t mid, uint16_t cmd, uint32_t *pdata);
int dma_read32n(uint8_t mid, uint16_t cmd, uint32_t *buf, int n);
int dma_write16(uint8_t mid, uint16_t cmd, uint16_t *pdata);
int dma_write32(uint8_t mid, uint16_t cmd, uint32_t *pdata);

/* W2(26/09/22): ssp1WfbLock/Unlock 제거 — SSP1을 Meter12 단일 스레드가 전담하므로 불요. */

void Board_DMA_Init();


#endif
