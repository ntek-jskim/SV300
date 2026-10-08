/*----------------------------------------------------------------------------
 * SV300 SNMP 문자열·비밀값 설정 — \system\snmp.d
 *  - COMM_CFG(Modbus 맵)에 두면 인증 없는 Modbus로 커뮤니티·비밀번호가 평문 노출되므로 별도 파일.
 *  - 적용은 부팅 시(snmpCfgActive). 웹 저장은 RAM 갱신 + FS 큐 기록만 한다(핸들러에서 FS 락 미보유).
 *  - 공장초기화(INITDB) 시 파일 삭제(장비 재배치·반납 시 비밀값 잔존 방지).
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "meter.h"
#include "snmp_cfg.h"

#define SNMP_CFG_FILE	CONCAT3(SYS_DIR, DIRSEP, "snmp.d")
#define SNMP_CFG_MAGIC	0x534E4D50u		/* 'SNMP' */
#define SNMP_CFG_VER	1

static SNMP_CFG snmpCfg;		/* 저장값(웹 표시·다음 부팅 적용) */
static SNMP_CFG snmpCfgRun;		/* 부팅 시 적용값(에이전트·Trap 사용) */
static SNMP_CFG snmpCfgWr;		/* FS 큐 기록 버퍼(pbuf 참조 — FS_task가 쓸 때까지 유지) */

static void cfgDefault(SNMP_CFG *c)
{
	memset(c, 0, sizeof(*c));
	c->magic = SNMP_CFG_MAGIC;
	c->ver   = SNMP_CFG_VER;
	strcpy(c->community, "public");
}

/* 파일 손상 대비 — 모든 문자열 NUL 종단 보장 */
static void cfgTerminate(SNMP_CFG *c)
{
	c->community[SNMP_CFG_STR_LEN] = '\0';
	c->contact[SNMP_CFG_STR_LEN]   = '\0';
	c->location[SNMP_CFG_STR_LEN]  = '\0';
	c->v3User[SNMP_CFG_STR_LEN]    = '\0';
}

void snmpCfgLoad(void)
{
	FILE *fp;
	size_t n = 0;

	fsFileLock();
	fp = fopen(SNMP_CFG_FILE, "rb");
	if (fp != NULL) {
		n = fread(&snmpCfg, 1, sizeof(snmpCfg), fp);
		fclose(fp);
	}
	fsFileUnlock();

	if (n != sizeof(snmpCfg) || snmpCfg.magic != SNMP_CFG_MAGIC || snmpCfg.ver != SNMP_CFG_VER) {
		if (fp != NULL)
			printf("[SNMP] %s invalid (n=%u) — defaults\n", SNMP_CFG_FILE, (unsigned)n);
		cfgDefault(&snmpCfg);
	}
	cfgTerminate(&snmpCfg);
	if (snmpCfg.community[0] == '\0')
		strcpy(snmpCfg.community, "public");

	snmpCfgRun = snmpCfg;
}

const SNMP_CFG *snmpCfgGet(void)    { return &snmpCfg; }
const SNMP_CFG *snmpCfgActive(void) { return &snmpCfgRun; }

static void setField(char *dst, const char *src)
{
	if (src == NULL)
		return;
	strncpy(dst, src, SNMP_CFG_STR_LEN);
	dst[SNMP_CFG_STR_LEN] = '\0';
}

void snmpCfgSave(const char *community, const char *contact, const char *location)
{
	FS_MSG msg;

	setField(snmpCfg.community, community);
	setField(snmpCfg.contact, contact);
	setField(snmpCfg.location, location);
	snmpCfg.magic = SNMP_CFG_MAGIC;
	snmpCfg.ver   = SNMP_CFG_VER;

	snmpCfgWr = snmpCfg;
	memset(&msg, 0, sizeof(msg));
	strcpy(msg.fname, SNMP_CFG_FILE);
	strcpy(msg.mode, "wb");
	msg.pbuf = &snmpCfgWr;
	msg.size = sizeof(snmpCfgWr);
	putFsQ(&msg);
}

void snmpCfgFactoryReset(void)
{
	fsFileLock();
#ifdef USE_CMSIS_RTOS2
	(void)fdelete(SNMP_CFG_FILE, NULL);
#else
	(void)fdelete(SNMP_CFG_FILE);
#endif
	fsFileUnlock();
	cfgDefault(&snmpCfg);
}
