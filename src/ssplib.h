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

/* [파형 A수정] readWFB 8페이지 버스트를 M1↔M2 순차화(bus==1만).
   [스파이크 수정] 버스트 동안 ssp1_mutex까지 보유해 SSP1을 독점 — 상대 미터의 폴링
   레지스터 읽기가 페이지 사이로 끼어들어 ADE9000 변환을 교란하던 경로를 막는다. */
void ssp1WfbLock(uint8_t bus);
void ssp1WfbUnlock(uint8_t bus);

void Board_DMA_Init();


#endif
