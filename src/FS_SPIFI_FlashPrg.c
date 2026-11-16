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

extern volatile uint8_t g_sdBusy;	/* [Quiet Capture] flash program/erase 활동 표시(XIP정지→M0 교란). Wave_Task가 감시. meter.c 정의 */

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
	/* [TODO 4.1 진단] 실제 SPIFI 클럭 확인용: ROM 에 넘긴 mhz 와 실제(BASE_SPIFI_CLK 소스·IDIVA 분주)가 다른지 */
	printf("[SPIFI] core=%uHz BASE_SPIFI_CLK=0x%08x IDIVA_CTRL=0x%08x PLL1_CTRL=0x%08x\n",
	       (unsigned)SystemCoreClock, (unsigned)LPC_CGU->BASE_SPIFI_CLK,
	       (unsigned)LPC_CGU->IDIVA_CTRL, (unsigned)LPC_CGU->PLL1_CTRL);

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

	/* [동적 용량 판별] 실제 칩(devSize/devID)과 컴파일 섹터맵(SF0_SIZE) 대조.
	 *  컴파일은 최대칩(HWV2=16MB) 고정, 실제 소용량 칩이면 위 콜백 가드가 초과영역 차단.
	 *  devID = JEDEC 용량코드: 0x18=16MB(MX25L12835F), 0x15=2MB(MX25L1636E). size=1<<devID. */
#define SF_CFG_SIZE  0x1000000UL	/* 컴파일 섹터맵 크기(항상 16MB, File_Config.c SF0_SIZE와 일치) */
	if (rc == 0 && s_spifi_obj.devSize != 0U) {
		unsigned long dv = (unsigned long)s_spifi_obj.devSize;
		if (dv == SF_CFG_SIZE)
			printf("[SPIFI] 용량 일치: 실제=cfg=%luMB\n", dv >> 20);
		else if (dv < SF_CFG_SIZE)
			printf("[SPIFI] ★소용량칩: 실제=%luMB < cfg=%luMB → %luMB 초과영역 차단(콜백가드). 재빌드 불필요\n",
			       dv >> 20, SF_CFG_SIZE >> 20, dv >> 20);
		else
			printf("[SPIFI] ★대용량칩: 실제=%luMB > cfg=%luMB → cfg %luMB만 사용(섹터맵 확장 필요)\n",
			       dv >> 20, SF_CFG_SIZE >> 20, SF_CFG_SIZE >> 20);
	}

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

/* [TODO 4.1 진단] program FAIL(rc=0x20003) 원인 확인용.
 *  options=0 은 S_ERASE_AS_REQD 와 같아서, 대상에 0→1 이 필요한 비트가 있으면 ROM 이 섹터를 지우고 다시 쓴다.
 *  쓰기 전 기존 내용을 확인해 "지우기 필요" 여부와 실패 시 기존/새/쓴 후 내용을 출력한다. */
#define PRG_DIAG_N	16

static void prgDiagDump(const char *tag, const volatile uint8_t *p, uint32_t n)
{
	uint32_t i;
	printf("[SPIFI]   %s:", tag);
	for (i = 0; i < n; i++)
		printf(" %02x", (unsigned)p[i]);
	printf("\n");
}

/* [TODO 4.1 진단] ROM 호출 시간 측정 — DWT CYCCNT(코어 클럭). ade9000.c IPD 진단과 같은 레지스터(켜기만 함) */
#define PRG_DEMCR	(*(volatile uint32_t *)0xE000EDFCUL)
#define PRG_DWTCTRL	(*(volatile uint32_t *)0xE0001000UL)
#define PRG_CYCCNT	(*(volatile uint32_t *)0xE0001004UL)
#define PRG_CYC_ON()	do { PRG_DEMCR |= (1UL << 24); PRG_DWTCTRL |= 1UL; } while (0)
#define PRG_US(cyc)	((cyc) / (SystemCoreClock / 1000000UL))

#define PRG_SLOW_US	5000u		/* 성공한 쓰기라도 ROM 호출이 이 이상 걸리면 slow ok 출력 */
static uint32_t s_prgCnt, s_prgSlow, s_prgMaxUs;	/* ROM 쓰기 호출 수 / slow 수 / 최대 시간(us) */

/* 플래시(메모리 모드) 내용과 buf 의 첫 불일치 위치. 모두 같으면 sz */
static U32 prgMismatch(const volatile uint8_t *p, const U8 *buf, U32 sz)
{
	U32 i;
	for (i = 0; i < sz; i++)
		if (p[i] != buf[i])
			break;
	return i;
}

/* 칩 내부 쓰기가 끝날 때까지 대기(명령 모드 필요) 후 메모리 모드 복귀. 반환: wait_busy rc */
static int32_t prgWaitBusy(void)
{
	int32_t rc = 0;
	s_rom->cancel_mem_mode(&s_spifi_obj);
	if (s_rom->wait_busy != NULL)
		rc = s_rom->wait_busy(&s_spifi_obj, 0);
	s_rom->set_mem_mode(&s_spifi_obj);
	return rc;
}

static BOOL ProgramPage(U32 adr, U32 sz, U8 *buf)
{
	SPIFIopers opers;
	int32_t rc, wb;
	uint8_t oldv[PRG_DIAG_N];
	const volatile uint8_t *mp;
	uint32_t i, n, needErase = 0, off0, off;
	uint32_t c0, c1, tus;
	uint16_t st0;
	uint8_t bz0;

	if (s_rom == NULL || buf == NULL || sz == 0U) {
		return (__FALSE);
	}
	/* [동적 용량 가드] 컴파일 섹터맵은 최대칩(16MB) 기준이나 실제 칩이 소용량(예 2MB)일 수 있음.
	 *  실 용량(devSize) 초과 주소 쓰기는 SPIFI 주소 wrap→기존 데이터 손상 유발 → 차단. */
	if (s_spifi_obj.devSize != 0U) {
		if (adr >= s_spifi_obj.devSize || sz > (s_spifi_obj.devSize - adr)) {
			printf("[SPIFI] program OOR adr=0x%08x sz=%u > devSize=0x%08x (소용량칩)\n",
			       (unsigned)adr, (unsigned)sz, (unsigned)s_spifi_obj.devSize);
			return (__FALSE);
		}
	}

	opers.dest = (char *)(uintptr_t)adr;
	opers.length = (uint32_t)sz;
	opers.scratch = (char *)s_spifi_scratch;
	opers.protect = -1;
	opers.options = S_NO_VERIFY;

	/* [TODO 4.1 진단] 쓰기 전 기존 내용 확인(메모리 모드에서 읽음) */
	s_rom->set_mem_mode(&s_spifi_obj);
	mp = (const volatile uint8_t *)((uintptr_t)s_spifi_obj.base + (uintptr_t)adr);
	n = (sz < PRG_DIAG_N) ? sz : PRG_DIAG_N;
	for (i = 0; i < sz; i++) {
		uint8_t o = mp[i];
		if (i < n)
			oldv[i] = o;
		if ((o & buf[i]) != buf[i])
			needErase++;
	}

	g_sdBusy = 1;		/* [Quiet Capture] flash 쓰기 활동 표시 */
	PRG_CYC_ON();
	s_rom->cancel_mem_mode(&s_spifi_obj);
	c0 = PRG_CYCCNT;
	rc = s_rom->spifi_program(&s_spifi_obj, (char *)buf, &opers);
	c1 = PRG_CYCCNT;
	st0 = s_spifi_obj.stat.hw; bz0 = s_spifi_obj.busy;	/* ROM 이 마지막으로 읽은 상태(set_mem_mode 전) */
	s_rom->set_mem_mode(&s_spifi_obj);
	/* [TODO 4.1 진단 11/16] 성공한 쓰기도 ROM 호출이 오래 걸렸으면 출력 — 실패는 모두 25~39ms(칩은 WIP=0).
	 *  성공인데 수~20ms 걸린 쓰기가 자주 보이면 = 우선순위 높은 태스크에 밀린 것(가설 a),
	 *  성공은 늘 1ms 안팎이고 실패만 25ms 이상이면 = ROM 이 완료를 못 알아챔(가설 b) */
	tus = PRG_US(c1 - c0);
	s_prgCnt++;
	if (tus > s_prgMaxUs)
		s_prgMaxUs = tus;
	if (rc == 0 && tus >= PRG_SLOW_US) {
		s_prgSlow++;
		printf("[SPIFI] slow ok adr=0x%08x sz=%u t=%uus (slow %u/%u, max %uus)\n",
		       (unsigned)adr, (unsigned)sz, (unsigned)tus,
		       (unsigned)s_prgSlow, (unsigned)s_prgCnt, (unsigned)s_prgMaxUs);
	}
	if (rc != 0) {
		printf("[SPIFI] program FAIL adr=0x%08x sz=%u rc=%d needErase=%u\n",
		       (unsigned)adr, (unsigned)sz, (int)rc, (unsigned)needErase);
		prgDiagDump("old", oldv, n);
		prgDiagDump("new", buf, n);
		prgDiagDump("now", mp, n);
		/* [TODO 4.1 진단 11/13] ROM 이 본 칩 상태·걸린 시간: 즉시 실패인지, 기다리다 시간 초과인지,
		 *  상태 레지스터(MX25L: bit0 WIP, bit1 WEL, bit6 QE)가 어떻게 읽혔는지 */
		printf("[SPIFI]   rom: t=%uus stat=0x%04x busy=%u errCheck=0x%08x opts=0x%08x\n",
		       (unsigned)tus, (unsigned)st0, (unsigned)bz0,
		       (unsigned)s_spifi_obj.errCheck, (unsigned)s_spifi_obj.opts);
		/* [TODO 4.1] 11/13 실측: rc=0x20003(시간 초과)인데 플래시 내용은 쓰려던 값과 같음(거짓 실패).
		 *  다시 읽어 같으면 성공 처리, 다르면 불일치 위치부터 1회 재기록 후 다시 확인 → 파일시스템이 거짓 실패로 쓰기를 중단하지 않게 함.
		 *  SV400 11/12 21:29 실측: 페이지 경계 뒤가 비어 있는 부분 기록도 있음. 칩이 아직 쓰는 중일 때 읽거나
		 *  다시 쓰면(ROM 이 지우기 판단) 섹터 손상 위험 → 반드시 wait_busy 후 확인.
		 *  off0=반환 직후 불일치 위치, off=완료 대기 후 불일치 위치 (off0<sz 이고 off==sz 면 "칩이 늦게 끝남") */
		off0 = prgMismatch(mp, buf, sz);
		c0 = PRG_CYCCNT;
		wb = prgWaitBusy();
		c1 = PRG_CYCCNT;
		printf("[SPIFI]   wait_busy: rc=%d t=%uus stat=0x%04x busy=%u\n", (int)wb,
		       (unsigned)PRG_US(c1 - c0), (unsigned)s_spifi_obj.stat.hw, (unsigned)s_spifi_obj.busy);
		off = prgMismatch(mp, buf, sz);
		if (off == sz) {
			printf("[SPIFI] program FAIL->verified ok adr=0x%08x off0=%u wb=%d\n",
			       (unsigned)adr, (unsigned)off0, (int)wb);
			rc = 0;
		}
		else {
			opers.dest = (char *)(uintptr_t)(adr + off);
			opers.length = (uint32_t)(sz - off);
			s_rom->cancel_mem_mode(&s_spifi_obj);
			rc = s_rom->spifi_program(&s_spifi_obj, (char *)(buf + off), &opers);
			s_rom->set_mem_mode(&s_spifi_obj);
			wb = prgWaitBusy();
			if (prgMismatch(mp, buf, sz) == sz) {
				printf("[SPIFI] program retry ok adr=0x%08x off0=%u off=%u rc=%d wb=%d\n",
				       (unsigned)adr, (unsigned)off0, (unsigned)off, (int)rc, (int)wb);
				rc = 0;
			}
			else {
				printf("[SPIFI] program retry FAIL adr=0x%08x off0=%u off=%u rc=%d wb=%d\n",
				       (unsigned)adr, (unsigned)off0, (unsigned)off, (int)rc, (int)wb);
				prgDiagDump("now", mp + off, (sz - off < PRG_DIAG_N) ? (sz - off) : PRG_DIAG_N);
				if (rc == 0)
					rc = -1;	/* 내용 불일치 → 실패 반환 */
			}
		}
	}
	else if (needErase) {
		/* 성공했지만 ROM 이 섹터 지우기를 끼워 넣은 경우 — 가설 A 확인용 */
		printf("[SPIFI] program ERASE_REQD ok adr=0x%08x sz=%u needErase=%u\n",
		       (unsigned)adr, (unsigned)sz, (unsigned)needErase);
		prgDiagDump("old", oldv, n);
		prgDiagDump("new", buf, n);
	}
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
	/* [동적 용량 가드] 실 용량(devSize) 초과 섹터 소거 차단(주소 wrap 방지). 소용량 칩 자동 대응. */
	if (s_spifi_obj.devSize != 0U && base >= s_spifi_obj.devSize) {
		printf("[SPIFI] erase OOR adr=0x%08x > devSize=0x%08x (소용량칩)\n",
		       (unsigned)base, (unsigned)s_spifi_obj.devSize);
		return (__FALSE);
	}
	opers.dest = (char *)(uintptr_t)base;
	opers.length = SF_VIRTUAL_SEC_BYTES;
	opers.scratch = (char *)s_spifi_scratch;
	opers.protect = -1;
	opers.options = S_ERASE_AS_REQD | S_NO_VERIFY;

	g_sdBusy = 1;		/* [Quiet Capture] flash 소거 활동 표시 */

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
