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
/* mid=2(CH3)까지 각 미터가 독립 TX/RX 버퍼를 갖도록 per-meter 배열 사용 */
#ifdef CH3
static uint8_t _tb[3][520], _rb[3][520];
#else
static uint8_t _tb[2][520], _rb[2][520];
#endif

OsTaskId	t_meter[2];

/* CH3: Meter1_Task·Meter2_Task가 SSP1을 공유하므로 직렬화 mutex */
#ifdef HWV1
static OsMutex ssp1_mutex;
/* [파형 A수정] readWFB 8페이지 버스트를 통째로 잠가 M1↔M2 페이지단위 인터리브를 순차 버스트로.
   인터리브(연속 SSP1 활동+CS급전환)가 M0 ADC를 교란 → M1/M2 각자 깨끗한 단일버스트로 분리. */
static OsMutex ssp1_wfb_mutex;
#endif

/* FreeRTOS: DMA 완료를 task notification(0x10) 대신 전용 이진 세마포어로 전달.
 * meterIrqSvc 의 0x1 알림이 xTaskNotifyWait 를 조기에 깨워 DMA 채널이
 * 미완료 상태로 남는 버그를 방지하기 위함이다. */
#ifdef __FREERTOS
static SemaphoreHandle_t ssp_dma_sem[2];
#endif

/* meter id(0,1,2,...) -> SSP bus index(0 or 1) */
static uint8_t meterToSspBus(uint8_t mid)
{
	return (mid == 0) ? 0 : 1;
}


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
		/* 전용 세마포어로 DMA 완료를 통보: task notification(0x1 등)과 분리 */
		xSemaphoreGiveFromISR(ssp_dma_sem[chn], &xHigherPriorityTaskWoken);
#else
		if (t_meter[chn] != 0) isr_evt_set(0x10, t_meter[chn]);
#endif
	}

	chn = 1;
	if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChTx[chn]) == SUCCESS) {
		isDmaTxfCompleted[chn] = 1;
	}
	if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChRx[chn]) == SUCCESS) {
		isDmaRxfCompleted[chn] = 1;
#ifdef __FREERTOS
		xSemaphoreGiveFromISR(ssp_dma_sem[chn], &xHigherPriorityTaskWoken);
#else
		if (t_meter[chn] != 0) isr_evt_set(0x10, t_meter[chn]);
#endif
	}

#ifdef __FREERTOS
	/* 완료된 태스크가 있으면 즉시 컨텍스트 스위치 */
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
}

void Board_DMA_Init() {
	/* Initialize GPDMA controller */
	Chip_GPDMA_Init(LPC_GPDMA);
	/* Setting GPDMA interrupt */
	NVIC_DisableIRQ(DMA_IRQn);
	/* FreeRTOS: FromISR API 사용 가능 범위(configMAX_SYSCALL_INTERRUPT_PRIORITY)를
	 * 준수하도록 DMA IRQ 우선순위를 PININT 와 동일한 레벨(6)로 설정한다. */
	NVIC_SetPriority(DMA_IRQn, 6);
	NVIC_EnableIRQ(DMA_IRQn);		   
	
	dmaChTx[0] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP0_Tx);
	dmaChRx[0] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP0_Rx);
	
	dmaChTx[1] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Tx);
	dmaChRx[1] = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Rx);	
	
	//memset(_tb, 0xff, 0xff);

#ifdef HWV1
	osCreateMutex(&ssp1_mutex);
	osCreateMutex(&ssp1_wfb_mutex);
#endif
#ifdef __FREERTOS
	ssp_dma_sem[0] = xSemaphoreCreateBinary();
	ssp_dma_sem[1] = xSemaphoreCreateBinary();
#endif
}

/* [파형 A수정] readWFB_Data가 8페이지 루프 전체를 감싸 M1↔M2 readWFB를 순차 버스트로 직렬화.
   bus==1(SSP1: M1/M2)만.

   [스파이크 수정] 종전엔 wfb락이 '상대 미터의 readWFB'만 배제하고, 버스는 dma_read32n이
   페이지 단위로 잡았다 놓았다 → 페이지와 페이지 사이로 상대 미터의 폴링 레지스터 읽기
   (read_reg16/32·write_reg*, STATUS0/1·RMS·Power…)가 그대로 끼어들어 버스트 한복판에서
   SSP1 클럭·상대 CS가 토글됐다. 이 활동이 ADE9000 동시변환을 교란해 '버스트당 ~1샘플'
   손상(단일 스파이크)으로 나타났고, 고조파는 mag[h]/mag[1] 정규화라 큰 임펄스가 하나만
   섞여도 전 차수가 같은 크기가 되어 THD/고조파가 100% 부근으로 뭉갠다.
   → 이제 버스트 전체가 ssp1_mutex도 함께 보유해 SSP1을 독점한다. 상대 미터의 레지스터
   읽기는 버스트(~2.4ms)가 끝날 때까지 대기하는데, page-full 주기 16ms 안이라 여유 있다.

   교착 없음: ssp1_mutex를 잡은 뒤 wfb락을 잡는 경로가 없어 락 순서 역전이 생기지 않는다.
   재귀 취득 회피: 버스트 중임을 ssp1_burst로 표시해 dma_read32n이 안쪽에서 다시 잡지 않게 한다
   (이 플래그는 ssp1_mutex 보유자만 갱신하고, 상대 미터는 wfb락에 막혀 readWFB에 못 들어온다). */
#ifdef HWV1
static volatile uint8_t ssp1_burst;	/* 1 = readWFB 버스트가 ssp1_mutex를 이미 보유 */
#endif

void ssp1WfbLock(uint8_t bus)
{
#ifdef HWV1
	if (bus == 1) {
		osAcquireMutex(&ssp1_wfb_mutex);	/* 상대 미터의 readWFB 배제 */
		osAcquireMutex(&ssp1_mutex);		/* 버스트 동안 SSP1 독점 */
		ssp1_burst = 1;
	}
#endif
}
void ssp1WfbUnlock(uint8_t bus)
{
#ifdef HWV1
	if (bus == 1) {
		ssp1_burst = 0;
		osReleaseMutex(&ssp1_mutex);
		osReleaseMutex(&ssp1_wfb_mutex);
	}
#endif
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

#ifndef __FREERTOS
	/* RTX 경로: DMA 완료 ISR 통보 대상을 현재 태스크로 갱신 */
	t_meter[chn] = os_tsk_self();
#endif

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
	/* 전용 세마포어로 대기: meterIrqSvc 의 0x1 알림이 섞여 조기 반환되던 버그 방지.
	 * 타임아웃 시 stuck DMA 채널을 강제 종료하여 다음 호출이 정상 시작되도록 한다. */
	if (xSemaphoreTake(ssp_dma_sem[chn], pdMS_TO_TICKS(100)) == pdFALSE) {
		printf("DMA_TIMEOUT, %d ...\n", chn);
		Chip_GPDMA_Stop(LPC_GPDMA, dmaChTx[chn]);
		Chip_GPDMA_Stop(LPC_GPDMA, dmaChRx[chn]);
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) == SET) Chip_SSP_ReceiveFrame(pSSP);
	}
#else
	if (os_evt_wait_and(0x10, 100) == OS_R_TMO) {
		printf("DMA_TIMEOUT, %d ...\n", chn);
		Chip_GPDMA_Stop(LPC_GPDMA, dmaChTx[chn]);
		Chip_GPDMA_Stop(LPC_GPDMA, dmaChRx[chn]);
		while (Chip_SSP_GetStatus(pSSP, SSP_STAT_RNE) == SET) Chip_SSP_ReceiveFrame(pSSP);
	}
#endif

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
	uint8_t bus = meterToSspBus(mid);
	/* per-meter 독립 버퍼 사용: mid=1,2 가 같은 bus를 쓰더라도 버퍼 충돌 방지 */
	uint8_t *ptb = _tb[mid], *prb = _rb[mid];
	int i, ix;

#ifdef HWV1
	/* CH3: SSP1(bus=1) 공유 — mutex 취득 후 해당 미터 CS를 Assert(LOW).
	 * 폴링 SPI(read_reg16/32 등)도 동일 mutex를 쓰므로 MISO 버스 충돌이 방지된다.
	 * mutex 범위: CS LOW → DMA 완료 → CS HIGH, 한 페이지 단위로 취득/반환한다.
	 * 단 readWFB 버스트 중이면 ssp1WfbLock이 이미 보유 중이라 다시 잡지 않는다(재귀 회피). */
	if (bus == 1) {
		if (!ssp1_burst) osAcquireMutex(&ssp1_mutex);
		selectMeter(mid);
	}
#endif

	ptb[2] = c >> 8;
	ptb[3] = c;	
	spiIO_DMA(_sspBase[bus], &ptb[2], 2, &prb[2], n*sizeof(uint32_t));

#ifdef HWV1
	if (bus == 1) {
		deSelectMeter(mid);
		if (!ssp1_burst) osReleaseMutex(&ssp1_mutex);
	}
#endif

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
	uint8_t bus = meterToSspBus(mid);
	uint16_t crc, c = (cmd << 4) | (1<<3);
	
	*(uint16_t *)tb = __REV16(c);

#ifdef HWV1
	if (bus == 1) osAcquireMutex(&ssp1_mutex);
#endif
	selectMeter(mid);
	spiIO8_Polling(_sspBase[bus], tb, 2, rb, 4);
	deSelectMeter(mid);
#ifdef HWV1
	if (bus == 1) osReleaseMutex(&ssp1_mutex);
#endif
	
	*pdata = __REV16(*(uint16_t *)rb);
	crc = __REV16(*(uint16_t *)&rb[2]);
	return crc;
}

int read_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata)
{
	uint8_t tb[10], rb[10];
	uint8_t bus = meterToSspBus(mid);
	uint16_t crc, c = (cmd << 4) | (1<<3);

	*(uint16_t *)tb = __REV16(c);

#ifdef HWV1
	if (bus == 1) osAcquireMutex(&ssp1_mutex);
#endif
	selectMeter(mid);
	spiIO8_Polling(_sspBase[bus], tb, 2, rb, 6);	
	deSelectMeter(mid);
#ifdef HWV1
	if (bus == 1) osReleaseMutex(&ssp1_mutex);
#endif

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
	uint8_t bus = meterToSspBus(mid);
	uint16_t c = (cmd << 4);

	tb[0] = c>>8;
	tb[1] = c;
	tb[2] = *pdata>>8;
	tb[3] = *pdata;

#ifdef HWV1
	if (bus == 1) osAcquireMutex(&ssp1_mutex);
#endif
	selectMeter(mid);
	spiIO8_Polling(_sspBase[bus], tb, 4, rb, 0);	
	deSelectMeter(mid);
#ifdef HWV1
	if (bus == 1) osReleaseMutex(&ssp1_mutex);
#endif
	
	return 1;
}


int write_reg32(uint8_t mid, uint16_t cmd, uint32_t *pdata)
{
	uint8_t tb[8], rb[8];	
	uint8_t bus = meterToSspBus(mid);
	uint16_t c = (cmd << 4);
	
	tb[2] = c>>8;
	tb[3] = c;
	*(uint32_t *)&tb[4] = __REV(*pdata);

#ifdef HWV1
	if (bus == 1) osAcquireMutex(&ssp1_mutex);
#endif
	selectMeter(mid);
	spiIO8_Polling(_sspBase[bus], &tb[2], 6, rb, 0);
	deSelectMeter(mid);
#ifdef HWV1
	if (bus == 1) osReleaseMutex(&ssp1_mutex);
#endif
	
	return 1;
}

/* [WIZnet 제거 2026/09/10] W5500 SPI 콜백(wizchip_select/deselect/read/write/burstread/
   burstwrite)과 W5500_SPI_Init 삭제 — 유일한 사용처였던 wiznet_thread.c를 프로젝트에서
   제외해 고아가 됐다. 특히 wizchip_select()는 selectMeter(1)로 CH3 보드의 M1_CS를
   내리는 2CH 시절 배선 코드라, 되살릴 경우 M1 ADE9000을 선택하게 되는 위험이 있었다.
   W5500 하드웨어를 다시 쓸 일이 생기면 git 이력(이 커밋 이전)에서 복구할 것. */
