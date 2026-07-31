/*----------------------------------------------------------------------------
 * CH3 + SPIFI: LPC4357 외부 NOR + NXP ROM SPIFI API → RL-FlashFS sf0_drv [S:]
 * 보드: SPI 핀·클럭은 Board_SystemInit / Board_SPIFI_FlashPinInit 에서 설정됨.
 * NOSDMEM 정의 시 이 드라이브는 컴파일에서 제외된다.
 *---------------------------------------------------------------------------*/
#ifdef NOSDMEM

#include <RTL.h>
#include <string.h>
#include <stdio.h>
#include <File_Config.h>
#include <LPC43xx.h>
#include "spifi_rom_api_lpc43.h"

#ifndef SF_VIRTUAL_SEC_BYTES
#define SF_VIRTUAL_SEC_BYTES 0x10000UL
#endif

static SPIFIobj s_spifi_obj;
static const SPIFI_RTNS *s_rom;
/* erase/program 시 ROM이 보조 버퍼로 사용할 수 있음 — 부족 시 크기 상향 */
static uint8_t s_spifi_scratch[8192];

void pullMISO(int high)
{
	(void)high;
}

static void spifi_enable_bus_clock(void)
{
	LPC_CCU1->CLK_M4_SPIFI_CFG |= (1UL << 0) | (1UL << 1); /* RUN | AUTO */
	while (!(LPC_CCU1->CLK_M4_SPIFI_STAT & (1UL << 0))) {
	}
}

/* P3_4(SPIFI_IO3 = MX25L12835F 7번핀 RESET#) SCU 값
 *  SPIFI: SCU_PINIO_FAST(0xF0: ZIF|EZI|EHS|INACT) | FUNC3(0x3) = 0xF3
 *  GPIO : EZI(0x40) | FUNC0(0)                                = 0x40 (GPIO1[14]) */
#define P3_4_SFS_SPIFI	0xF3u
#define P3_4_SFS_GPIO	0x40u
#define MX_SR_QE		0x40u	/* Status Register QE bit(6). BP=0, SRWD=0 */

static int32_t spifiRomInit(uint32_t mhz)
{
	memset(&s_spifi_obj, 0, sizeof(s_spifi_obj));
	return s_rom->spifi_init(&s_spifi_obj, 4, S_FULLCLK | S_RCVCLK, mhz);
}

static BOOL Init(U32 adr, U32 clk)
{
	int32_t rc;
	uint32_t mhz;

	(void)adr;
	spifi_enable_bus_clock();

	s_rom = *((const SPIFI_RTNS *const *)SPIFI_ROM_PTR_ADDR);
	if (s_rom == NULL || s_rom->spifi_init == NULL) {
		return (__FALSE);
	}

	mhz = clk / 1000000UL;
	/* 신뢰성 우선: quad read/program 타이밍 마진 확보 위해 40MHz로 제한
	 * (간헐 홀딩·program FAIL 대응. 안정화되면 상향 검토) */
	if (mhz > 40UL) {
		mhz = 40UL;
	}
	if (mhz < 1UL) {
		mhz = 40UL;
	}
	printf("[SPIFI] clk=%uHz use=%uMHz\n", (unsigned)clk, (unsigned)mhz);

	/* 1) 정상 시도: QE=1(비휘발성)이면 pin7=SIO3라 RESET# 없음 → 성공(quad) */
	rc = spifiRomInit(mhz);

	/* 2) 실패 = 신품(QE=0): 단일레인 RDID 중 IO3 low가 RESET#를 걸어 통신 불가.
	 *    IO3를 GPIO-high로 강제해 RESET# 해제 → init → QE=1(1회) → IO3=SPIFI 복귀 → 재init */
	if (rc != 0) {
		printf("[SPIFI] init rc=%d -> recover: force RESET# high, set QE\n", (int)rc);

		LPC_SCU->SFSP3_4 = P3_4_SFS_GPIO;		/* P3_4 -> GPIO1[14] */
		LPC_GPIO_PORT->DIR[1] |= (1UL << 14);
		LPC_GPIO_PORT->SET[1]  = (1UL << 14);	/* RESET# 강제 high */

		rc = spifiRomInit(mhz);
		if (rc == 0) {
			if (s_rom->write_stat != NULL) {
				s_rom->write_stat(&s_spifi_obj, 1, (uint16_t)MX_SR_QE);	/* QE=1 (비휘발성) */
				if (s_rom->wait_busy != NULL) {
					s_rom->wait_busy(&s_spifi_obj, 0);
				}
				printf("[SPIFI] QE=1 written (SR=0x%02x)\n", (unsigned)MX_SR_QE);
			}
			LPC_SCU->SFSP3_4 = P3_4_SFS_SPIFI;	/* P3_4 -> SPIFI_IO3 복귀 */
			rc = spifiRomInit(mhz);				/* QE=1 → RESET# 없음, quad OK */
		}
		else {
			printf("[SPIFI] recover init FAILED rc=%d (HW: 납땜/전원/칩)\n", (int)rc);
		}
	}

	printf("[SPIFI] rc=%d mfger=%02x type=%02x id=%02x devSize=0x%08x memSize=0x%08x\n",
	       (int)rc,
	       (unsigned)s_spifi_obj.mfger, (unsigned)s_spifi_obj.devType, (unsigned)s_spifi_obj.devID,
	       (unsigned)s_spifi_obj.devSize, (unsigned)s_spifi_obj.memSize);

	if (rc != 0) {
		return (__FALSE);
	}
	s_rom->set_mem_mode(&s_spifi_obj);
	return (__TRUE);
}

static BOOL UnInit(void)
{
	if (s_rom != NULL && s_rom->cancel_mem_mode != NULL) {
		s_rom->cancel_mem_mode(&s_spifi_obj);
	}
	return (__TRUE);
}

static BOOL ReadData(U32 adr, U32 sz, U8 *buf)
{
	if (s_rom == NULL || s_rom->set_mem_mode == NULL || buf == NULL) {
		return (__FALSE);
	}
	if (s_spifi_obj.devSize != 0U) {
		if (adr >= s_spifi_obj.devSize || sz > (s_spifi_obj.devSize - adr)) {
			return (__FALSE);
		}
	}
	s_rom->set_mem_mode(&s_spifi_obj);
	memcpy(buf, (const void *)((uintptr_t)s_spifi_obj.base + (uintptr_t)adr), (size_t)sz);
	return (__TRUE);
}

static BOOL ProgramPage(U32 adr, U32 sz, U8 *buf)
{
	SPIFIopers opers;
	int32_t rc;

	if (s_rom == NULL || buf == NULL || sz == 0U) {
		return (__FALSE);
	}

	opers.dest = (char *)(uintptr_t)adr;
	opers.length = (uint32_t)sz;
	opers.scratch = (char *)s_spifi_scratch;
	opers.protect = -1;
	opers.options = S_NO_VERIFY;

	s_rom->cancel_mem_mode(&s_spifi_obj);
	rc = s_rom->spifi_program(&s_spifi_obj, (char *)buf, &opers);
	s_rom->set_mem_mode(&s_spifi_obj);
	if (rc != 0)
		printf("[SPIFI] program FAIL adr=0x%08x sz=%u rc=%d\n", (unsigned)adr, (unsigned)sz, (int)rc);
	return (rc == 0) ? (__TRUE) : (__FALSE);
}

static BOOL EraseSector(U32 adr)
{
	SPIFIopers opers;
	U32 base;
	int32_t rc;

	if (s_rom == NULL) {
		return (__FALSE);
	}

	base = adr & ~(SF_VIRTUAL_SEC_BYTES - 1U);
	opers.dest = (char *)(uintptr_t)base;
	opers.length = SF_VIRTUAL_SEC_BYTES;
	opers.scratch = (char *)s_spifi_scratch;
	opers.protect = -1;
	opers.options = S_ERASE_AS_REQD | S_NO_VERIFY;

	s_rom->cancel_mem_mode(&s_spifi_obj);
	rc = s_rom->spifi_erase(&s_spifi_obj, &opers);
	s_rom->set_mem_mode(&s_spifi_obj);
	if (rc != 0)
		printf("[SPIFI] erase FAIL adr=0x%08x rc=%d\n", (unsigned)base, (int)rc);
	return (rc == 0) ? (__TRUE) : (__FALSE);
}

/*----------------------------------------------------------------------------
 * RL-FlashFS SPI Flash 디바이스 드라이버 블록 — File_lib 가 참조하는 심볼명은 sf0_drv
 *---------------------------------------------------------------------------*/
EFS_DRV sf0_drv = {
	Init,
	UnInit,
	ReadData,
	ProgramPage,
	EraseSector,
	NULL /* EraseChip: 전칩 소거 미사용(SF0 일부 영역만 FAT 일 때) */
};

#endif /* NOSDMEM */
