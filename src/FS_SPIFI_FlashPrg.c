/*----------------------------------------------------------------------------
 * CH3 + SPIFI: LPC4357 외부 NOR + NXP ROM SPIFI API → RL-FlashFS sf0_drv [S:]
 * 보드: SPI 핀·클럭은 Board_SystemInit / Board_SPIFI_FlashPinInit 에서 설정됨.
 * NOSDMEM 정의 시 이 드라이브는 컴파일에서 제외된다.
 *---------------------------------------------------------------------------*/
#ifdef NOSDMEM

#include <RTL.h>
#include <string.h>
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

	memset(&s_spifi_obj, 0, sizeof(s_spifi_obj));

	mhz = clk / 1000000UL;
	if (mhz > 80UL) {
		mhz = 80UL;
	}
	if (mhz < 1UL) {
		mhz = 60UL;
	}

	rc = s_rom->spifi_init(&s_spifi_obj, 4, S_FULLCLK | S_RCVCLK, mhz);
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
