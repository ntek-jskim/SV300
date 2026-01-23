#include "board.h"
#include "stdio.h"
#include "os.h"

#define LPC_GPDMA_SSP_TX  GPDMA_CONN_SSP0_Tx
#define LPC_GPDMA_SSP_RX  GPDMA_CONN_SSP0_Rx

//// SSP Buffer (최대 크기는 Wave buffer page (128 DWORD)로 정한다)
//static uint16_t _sspTxb[4];
//static uint16_t _sspRxb[(128+4)*2];	
	
__IO uint8_t isDmaTxfCompleted[2];
__IO uint8_t isDmaRxfCompleted[2];

static uint8_t dmaChTx[2], dmaChRx[2];
static LPC_SSP_T* _sspBase[] = {LPC_SSP0, LPC_SSP1};
static uint8_t _tb[2][520], _rb[2][520];

OsTaskId	t_meter[2];


#ifdef _SSP_INTR

typedef struct {
	int tti, tri, tc;
	int rti, rri, rc;
	uint16_t *tb, *rb;
} SSP_INT_XFER;

static SSP_INT_XFER xfer;

void SSP0_IRQHandler(void)
{
	Chip_SSP_Int_Disable(LPC_SSP0);	/* Disable all interrupt */
	Chip_SSP_Int_RWFrames16Bits(LPC_SSP0, &xf_setup);

	if ((xfer.rx_cnt != xf_setup.length) || (xf_setup.tx_cnt != xf_setup.length)) {
		Chip_SSP_Int_Enable(LPC_SSP0);	/* enable all interrupts */
	}
	else {
		isXferCompleted = 1;
	}
}

/** SSP macro: read 1 bytes from FIFO buffer */
STATIC void SSP_Read2Fifo(LPC_SSP_T *pSSP, SSP_INT_XFER *xfer)
{
	uint16_t rDat;

	while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) == SET) {
		if (xfer->tri < xfer->tc) {
			Chip_SSP_ReceiveFrame(pSSP);
			xfer->tri++;
		}
		else if (xfer->rri < xfer->rc) {
			xfer->rb[xfer->rri++] = Chip_SSP_ReceiveFrame(pSSP);
		}
	}
}


/* SSP Interrupt Read/Write with 16-bit frame width */
Status spiIO_Int(LPC_SSP_T *pSSP, uint16_t *tb, int tc, uint16_t *rb, int rc)
{
	int ri=0, ti=0;
	uint16_t rDat;
	
	xfer.tti = xfer.tri = 0;
	xfer.rti = xfer.rri = 0;
	xfer.tc = tc;
	xfer.rc = rc;
	xfer.tb = tb;
	xfer.rb = rb;
	
	/* Check overrun error in RIS register */
	if (Chip_SSP_GetRawIntStatus(pSSP, SSP_RORRIS) == SET) {
		return ERROR;
	}

	do {
		if (Chip_SSP_GetStatus(pSSP, SSP_STAT_TNF)) {
			if (xfer.tti < xfer.tc) {
				Chip_SSP_SendFrame(pSSP, xfer.tb[xfer.tti++]);
			}
			else if (xfer.rti < xfer.rc) {
				Chip_SSP_SendFrame(pSSP, 0xffff);
				xfer.rti++;
			}
		}

		/* Check overrun error in RIS register */
		if (Chip_SSP_GetRawIntStatus(pSSP, SSP_RORRIS) == SET) {
			return ERROR;
		}

		/*  Check for any data available in RX FIFO			 */
		SSP_Read2Fifo(pSSP, &xfer);
	} while (xfer.tti < xfer.tc || xfer.tri < xfer.tc || xfer.rti < xfer.rc || xfer.rri < xfer.rc);

	return SUCCESS;
}
#endif


void DMA_IRQHandler(void)
{
	int chn;
#ifdef __FREERTOS   
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
#endif
   
	chn = 0;
	if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChTx[chn]) == SUCCESS) {
		isDmaTxfCompleted[chn] = 1;
	}

	if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChRx[chn]) == SUCCESS) {		
    	isDmaRxfCompleted[chn] = 1;
#ifdef __FREERTOS		
      	if(t_meter[chn] != 0) xTaskNotifyFromISR(t_meter[chn], 0x10, eSetBits, &xHigherPriorityTaskWoken);
#else
		if(t_meter[chn] != 0) isr_evt_set(0x10, t_meter[chn]);
#endif		      
	}
		
	chn = 1;
	if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChTx[chn]) == SUCCESS) {
		isDmaTxfCompleted[chn] = 1;
	}

	if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChRx[chn]) == SUCCESS) {		
		isDmaRxfCompleted[chn] = 1;
#ifdef __FREERTOS	
      	if (t_meter[chn] != 0) xTaskNotifyFromISR(t_meter[chn], 0x10, eSetBits, &xHigherPriorityTaskWoken);
#else
		if (t_meter[chn] != 0) isr_evt_set(0x10, t_meter[chn]);
#endif		
	}	
}

void Board_DMA_Init() {
	/* Initialize GPDMA controller */
	Chip_GPDMA_Init(LPC_GPDMA);
	/* Setting GPDMA interrupt */
	NVIC_DisableIRQ(DMA_IRQn);
	//NVIC_SetPriority(DMA_IRQn, ((0x01 << 3) | 0x01));
#if 1   
	// FreeRTOS 사용시 priority는 6보다 크거나 같아야 한다
	NVIC_SetPriority(PIN_INT3_IRQn, 6);
#endif   
	NVIC_EnableIRQ(DMA_IRQn);		   
	
	dmaChTx[0] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP0_Tx);
	dmaChRx[0] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP0_Rx);
	
	dmaChTx[1] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Tx);
	dmaChRx[1] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Rx);	
	
	//memset(_tb, 0xff, 0xff);
}


int spiIO_DMA(LPC_SSP_T *pSSP, uint8_t *tb, int tc, uint8_t *rb, int rc) 
{
	int n= tc+rc, chn, sspTx, sspRx, temp=0;
   uint32_t notificationValue;
	
	if (pSSP == LPC_SSP0) {
		chn = 0;
		sspTx = GPDMA_CONN_SSP0_Tx;
		sspRx = GPDMA_CONN_SSP0_Rx;		
	}
	else {
		chn = 1;
		sspTx = GPDMA_CONN_SSP1_Tx;
		sspRx = GPDMA_CONN_SSP1_Rx;				
	}
	
	
	isDmaTxfCompleted[chn] = isDmaRxfCompleted[chn] = 0;
	
	if (t_meter[chn] == 0) {
#ifdef __FREERTOS		
		t_meter[chn] = xTaskGetCurrentTaskHandle();
#else
		t_meter[chn] = os_tsk_self();
#endif		
	}	

	while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) == SET) {
		Chip_SSP_ReceiveFrame(pSSP);
		temp++;
	}	
	if (temp) {
		printf(">>> spiIO_DMA, FIFO NOT EMPTY: %d, %d \n", chn, temp);
	}	
		
	/* data Tx_Buf --> SSP */
	Chip_GPDMA_Transfer(LPC_GPDMA, dmaChTx[chn],
						(uint32_t)tb,
						sspTx,
						GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA,
						n);
	/* data SSP --> Rx_Buf */
	Chip_GPDMA_Transfer(LPC_GPDMA, dmaChRx[chn],
						sspRx,
						(uint32_t)rb,
						GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA,
						n);
	
	// Enable DMA
	Chip_SSP_DMA_Enable(pSSP);

#ifdef __FREERTOS
	//if (os_evt_wait_and(0x10, 100) == OS_R_TMO) 
   if (xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, pdMS_TO_TICKS(100)) == 0)
#else
	if (os_evt_wait_and(0x10, 100) == OS_R_TMO) 
#endif	
	{
		printf("DMA_TIMEOUT, %d ...\n", chn);
	}
	//while (!isDmaTxfCompleted || !isDmaRxfCompleted) {}
	
	// Disable DMA
	Chip_SSP_DMA_Disable(pSSP);	
}


// 8 byte 이하 (FiFo 크기 이하)
int spiIO8(LPC_SSP_T *pSSP, uint8_t *tb, int tc, uint8_t *rb, int rc) {
	int i, ri=0, n=tc+rc, temp=0;
	
	// clear RX buffer
	while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) == SET) {
		Chip_SSP_ReceiveFrame(pSSP);
		temp++;
	}
	
	if (temp) {
		printf(">>> SSP FIFO NOT EMPTY: %d, %d \n", (pSSP == LPC_SSP0) ? 0 : 1, temp);
	}
	
	for (i=0; i<tc; i++) {
		Chip_SSP_SendFrame(pSSP, tb[i]);
	}
	for (; i<n; i++) {
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_TNF) != SET) ;
		Chip_SSP_SendFrame(pSSP, 0xffff);
	}
	
	while (Chip_SSP_GetStatus(pSSP, SSP_STAT_BSY) == SET) ;
			
	for (i=0; i<n; i++) {
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) != SET) ;
		if (i < tc) 
			temp = Chip_SSP_ReceiveFrame(pSSP);
		else	
			rb[ri++] = Chip_SSP_ReceiveFrame(pSSP);
	}
	
	return 0;	
}


// low speed 방식, FiFo를 사용하지 않는다 하나 쓰고 하나 읽는 방식을 사용한다 
int spiIO8_Polling(LPC_SSP_T *pSSP, uint8_t *tb, int tc, uint8_t *rb, int rc) {
	int i, ri=0, n=tc+rc, temp=0;
	
	// clear RX buffer
	while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) == SET) {
		Chip_SSP_ReceiveFrame(pSSP);
		temp++;
	}
	
	if (temp) {
		printf(">>>spiIO8_Polling, FiFo not empty %d, %d\n", (pSSP==LPC_SSP0)?0:1, temp);
	}
	
	for (i=0; i<n; i++) {
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_TNF) != SET) ;		
		if (i<tc) 
			Chip_SSP_SendFrame(pSSP, tb[i]);
		else
			Chip_SSP_SendFrame(pSSP, 0xffff);	
		
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) != SET) ;
		if (i<tc) 
			Chip_SSP_ReceiveFrame(pSSP);
		else
			rb[i-tc] = Chip_SSP_ReceiveFrame(pSSP);
	}
			
	return 0;	
}



int spiIO16(LPC_SSP_T *pSSP, uint16_t *tb, int tc, uint16_t *rb, int rc) {
	int i, ti=0, ri=0, n=tc+rc; 
	
	do {
		if (Chip_SSP_GetStatus(pSSP, SSP_STAT_TNF)) {
			if (ti < tc) {
				Chip_SSP_SendFrame(pSSP, tb[ti++]);
			}
			else if (ti < n) {
				Chip_SSP_SendFrame(pSSP, 0xffff); 
				ti++;
			}			
		}		
		
		if (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE)) {
			if (ri < tc) {
				Chip_SSP_ReceiveFrame(pSSP); 
				ri++;
			}
			else if (ri < n){
				rb[ri-tc] = Chip_SSP_ReceiveFrame(pSSP); 
				ri++;
			}
		}
	} while (ti < n || ri < n);
	
	return 0;
}

//int spiIO_8n(LPC_SSP_T *pSSP, uint8_t *tb, int tc, uint8_t *rb, int rc) {
//	int i, n=rc*4, c, ri=0; 
//	uint16_t temp[8];
//	
//	for (i=0; i<tc; i++) {
//		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_TNF) != SET) ;
//		Chip_SSP_SendFrame(pSSP, tb[i]);
//		
//		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) != SET) ;		
//		Chip_SSP_ReceiveFrame(pSSP);
//	}
//	
//		
//	// 128 DWORD 읽을 때 기존 방법(2.68) 보다 시간 단축한다(2.12)
//	while (n > 0) {
//		if (Chip_SSP_GetStatus(pSSP, SSP_STAT_TFE)) {
//			c = (n > 8) ? 8 : n;
//			for (i=0; i<c; i++) {
//				Chip_SSP_SendFrame(pSSP, 0xffff);
//			}			
//			for (i=0; i<8; i++) {
//				while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) != SET) ;
//				rb[ri++] = Chip_SSP_ReceiveFrame(pSSP);
//			}			
//			n -= c;
//		}
//	}
//	
//	return 0;
//}



int spiIO_32n(LPC_SSP_T *pSSP, uint16_t *tb, int tc, uint32_t *rb, int rc) {
	int i, n=rc*2, c, ri=0; 
	uint16_t temp[8];
	
	for (i=0; i<tc; i++) {
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_TNF) != SET) ;
		Chip_SSP_SendFrame(pSSP, tb[i]);
		
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) != SET) ;		
		Chip_SSP_ReceiveFrame(pSSP);
	}
			
	// 128 DWORD 읽을 때 기존 방법(2.68) 보다 시간 단축한다(2.12)
	while (n > 0) {
		if (Chip_SSP_GetStatus(pSSP, SSP_STAT_TFE)) {
			c = (n > 8) ? 8 : n;
			for (i=0; i<c; i++) {
				Chip_SSP_SendFrame(pSSP, 0xffff);
			}			
			for (i=0; i<8; i++) {
				while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) != SET) ;
				temp[i] = Chip_SSP_ReceiveFrame(pSSP);
			}
			// Word Swap
			for (i=0; i<8; i+=2) {
				rb[ri++] = __ROR(*(uint32_t *)&temp[i], 16);				
			}
			
			n -= c;
		}
	}
	
	return 0;
}


//int dma_read16(uint8_t mid, uint16_t cmd, uint16_t *pdata) {
//	uint8_t tb[8], rb[8];
//	uint16_t c = (cmd<<4) | (1<<3);
//		
//	tb[0] = c >> 8;
//	tb[1] = c; 
//	
//	spiIO_DMA(LPC_SSP0, &tb[0], 2, &rb[0], 2); 
//	
//	*pdata = rb[2]<<8 | rb[3];
//	return 0;
//}

//int dma_read32(uint8_t mid, uint16_t cmd, uint32_t *pdata) {
//	uint8_t tb[8], rb[8];
//	uint16_t c = (cmd<<4) | (1<<3);
//	
//	// long word alignment 
//	tb[2] = c >> 8;
//	tb[3] = c; 
//	
//	// rb[4..7] : data
//	spiIO_DMA(LPC_SSP0, &tb[2], 2, &rb[2], 4); 
//	
//	*pdata = __REV(*(uint32_t *)&rb[4]);	
//	return 0;
//}


// 128*8 = 1024 LongWord(32bit) 읽을때 3ms 소요(endiag 변환 포함)
int dma_read32n(uint8_t mid, uint16_t cmd, uint32_t *buf, int n) 
{	
	uint16_t c = (cmd << 4) | (1<<3); 
	uint8_t *ptb = _tb[mid], *prb = _rb[mid];
	int i, ix;
	
	ptb[2] = c >> 8;
	ptb[3] = c;	
	spiIO_DMA(_sspBase[mid], &ptb[2], 2, &prb[2], n*sizeof(uint32_t));
	
	// sampling data 타입이 다르기 떄문에 endian 변환 루틴은 App. 에서 처리한다.
	for (ix=4, i=0; i<n; i++, ix+=4) {
		buf[i] = __REV(*(uint32_t *)&prb[ix]);
	}
	
	return 0;
}


//int dma_write16(uint8_t mid, uint16_t cmd, uint16_t *pdata)
//{
//	uint8_t tb[8], rb[8];	
//	uint16_t c = (cmd << 4);
//	
//	tb[0] = c>>8;
//	tb[1] = c;
//	tb[2] = *pdata>>8;
//	tb[3] = *pdata;

//	spiIO_DMA(LPC_SSP0, tb, 4, rb, 0);
//		
//	return 1;
//}


//int dma_write32(uint8_t mid, uint16_t cmd, uint32_t *pdata)
//{
//	uint8_t tb[8], rb[8];	
//	uint16_t c = (cmd << 4);
//	
//	tb[2] = c>>8;
//	tb[3] = c;
//	*(uint32_t *)&tb[4] = __REV(*pdata);

//	spiIO_DMA(LPC_SSP0, &tb[2], 6, &rb[2], 0);
//	
//	return 1;
//}

int read_reg16(uint8_t mid, uint16_t cmd, uint16_t *pdata) {
	uint8_t tb[8], rb[8];
	uint16_t crc, c = (cmd << 4) | (1<<3);
	
	*(uint16_t *)tb = __REV16(c);
	
	selectMeter(mid);
	spiIO8_Polling(_sspBase[mid], tb, 2, rb, 4);
	deSelectMeter(mid);
	
	*pdata = __REV16(*(uint16_t *)rb);
	crc = __REV16(*(uint16_t *)&rb[2]);
	return crc;
}

int read_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata)
{
	uint8_t tb[10], rb[10];
	uint16_t crc, c = (cmd << 4) | (1<<3);

	*(uint16_t *)tb = __REV16(c);
	
	selectMeter(mid);
	spiIO8_Polling(_sspBase[mid], tb, 2, rb, 6);	
	deSelectMeter(mid);	

	*pdata = __REV(*(uint32_t *)rb);
	crc = __REV16(*(uint16_t *)&rb[4]);	
	return crc;
}


//int read_reg32n(uint8_t mid, uint8_t cmd, uint32_t *buf, int n) 
//{	
//	uint8_t tb[10], *prb = (uint8_t *)buf;
//	uint16_t c = (cmd << 4) | (1<<3);
//	int i, ix;
//	
//	tb[0] = c>>8;
//	tb[1] = c;

//	selectMeter(mid);
//	spiIO_8n(_sspBase[mid], tb, 2, (uint8_t *)buf, n);	
//	deSelectMeter(mid);	
//	
//	
//	for (ix=0, i=0; i<n; i++, ix+=4) {
//		buf[i] = __REV(*(uint32_t*)&prb[ix]);
//	}
//	
//	return 0;
//}


int write_reg16(uint8_t mid, uint16_t cmd, uint16_t *pdata)
{
	uint8_t tb[8], rb[8];
	uint16_t c = (cmd << 4);

	tb[0] = c>>8;
	tb[1] = c;
	tb[2] = *pdata>>8;
	tb[3] = *pdata;

	selectMeter(mid);
	spiIO8_Polling(_sspBase[mid], tb, 4, rb, 0);	
	deSelectMeter(mid);
	
	return 1;
}


int write_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata)
{
	uint8_t tb[8], rb[8];	
	uint16_t c = (cmd << 4);
	
	tb[2] = c>>8;
	tb[3] = c;
	*(uint32_t *)&tb[4] = __REV(*pdata);

	selectMeter(mid);
	spiIO8_Polling(_sspBase[mid], &tb[2], 6, rb, 0);
	deSelectMeter(mid);
	
	return 1;
}

// const GPIO_ID SSEL_GPIO[] = {
// 	{7, 16},
// 	{7, 19},
// };
void  wizchip_select(void)
{	
//	GPIO_PinWrite(1, 27, 0);	// SSEL(CS)
	selectMeter(1);
}

void  wizchip_deselect(void)
{
//	GPIO_PinWrite(1, 27, 1);	// SSEL(CS)
	deSelectMeter(1);
}

uint8_t wizchip_read()
{
	uint8_t rb;
#ifdef LPC1788
	//Chip_SSP_ReadFrames_Blocking(LPC_SSP0, &rb, 1);
	while ((LPC_SSP1->SR & SSP_SR_TNF) == 0) ;	// Not Full
	SSP_SendData(LPC_SSP1, 0xff);
	while ((LPC_SSP1->SR & SSP_SR_RNE) == 0) ; 	// Not Empty		
	rb = SSP_ReceiveData(LPC_SSP1);	
#else	
	uint8_t tb = 0xff;
	spiIO8_Polling(LPC_SSP1, 0, 0, &rb, 1);
#endif
	return rb;
}

void  wizchip_write(uint8_t wb)
{
#ifdef LPC1788
	//Chip_SSP_WriteFrames_Blocking(LPC_SSP0, &wb, 1);
	while ((LPC_SSP1->SR & SSP_SR_TNF) == 0) ;	// Not Full
	SSP_SendData(LPC_SSP1, wb);

	while ((LPC_SSP1->SR & SSP_SR_RNE) == 0) ; 	// Not Empty		
	SSP_ReceiveData(LPC_SSP1);
#else
	uint8_t rb;
	spiIO8_Polling(LPC_SSP1, &wb, 1, 0, 0);
#endif
}

void wizchip_burstread(uint8_t* pBuf, uint16_t len)
{
#ifdef LPC1788
	spi(LPC_SSP1, 0, 0, pBuf, len);
#else
	spiIO8_Polling(LPC_SSP1, 0, 0, pBuf, len);
#endif
}

void  wizchip_burstwrite(uint8_t* pBuf, uint16_t len)
{
#ifdef LPC1788
	spi(LPC_SSP1, pBuf,len, 0, 0);
#else
	spiIO8_Polling(LPC_SSP1, pBuf,len, 0, 0);
#endif
}

void W5500_SPI_Init()
{
	Board_SSP_Init(LPC_SSP1, 8, 0, 16000000);	// 0:8bit, 0:Manual/1:Auto
}
