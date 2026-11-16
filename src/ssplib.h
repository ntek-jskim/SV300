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

/* [26/11/16] ADE9000 SPI 읽기 검증 통계(칩별). w=0:16bit, 1:32bit
 *  echo: 데이터 뒤 추가 2바이트(=데이터 상위 16bit 반복) 일치 건수
 *  crc : CRC_SPI(0x4A9) 별도 읽기 검증(read_reg32_crc 사용처만) — 계산 후보별 일치 건수
 *        crcOk[0]=초기값 0xFFFF·데이터4B, [1]=초기값 0·4B, [2]=0xFFFF·데이터+반복 6B, [3]=0·6B */
typedef struct {
	uint32_t reads[2], echoOk[2];
	uint32_t crcReads, crcOk[4];
	uint32_t retry, fail;			/* SPI_CRC_MODE=1: 재시도 횟수, 재시도 소진 실패 */
	uint16_t lastCmd, lastRx;		/* 마지막 echo 불일치 */
	uint32_t lastData;
	uint16_t crcLastCmd, crcLastRx, crcLastCalc;	/* 마지막 CRC 불일치(0xFFFF 기준) */
	uint32_t crcLastData;
} SPI_CRC_STAT;

int read_reg32_crc(uint8_t mid, uint16_t cmd, uint32_t *pdata);
void spiCrcReport(int force);

/* W2(26/09/22): ssp1WfbLock/Unlock 제거 — SSP1을 Meter12 단일 스레드가 전담하므로 불요. */

void Board_DMA_Init();


#endif
