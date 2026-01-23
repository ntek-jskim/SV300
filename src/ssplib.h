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

void Board_DMA_Init();

// wiznet
void  wizchip_select(void);
void  wizchip_deselect(void);
uint8_t wizchip_read(void);
void  wizchip_write(uint8_t wb);
void wizchip_burstread(uint8_t* pBuf, uint16_t len);
void  wizchip_burstwrite(uint8_t* pBuf, uint16_t len);
void W5500_SPI_Init(void);


#endif
