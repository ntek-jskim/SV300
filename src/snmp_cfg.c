/*----------------------------------------------------------------------------
 * SV300 SNMP 실행 설정
 *  - 커뮤니티·sysContact/Location·v3 사용자/비밀번호는 통신 설정(COMM_CFG, Modbus 7300~7395)에
 *    있다(2026-10-28, 구 snmp.d에서 이동). 웹·Modbus로 쓰고 Save Settings로 settings.dat에 저장.
 *  - 적용은 부팅 시(snmpCfgLoad → snmpCfgActive). v3 키(Ku)는 이때 비밀번호로 계산(SHA-1 1MB 해시).
 *  - \system\snmp.d 에는 engineBoots만 저장(부팅마다 +1, RFC 3414). 공장초기화(INITDB) 시 삭제.
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "core/net.h"
#include "snmp/snmp_agent.h"
#include "meter.h"
#include "snmp_cfg.h"

#define SNMP_BOOTS_FILE		CONCAT3(SYS_DIR, DIRSEP, "snmp.d")
#define SNMP_BOOTS_MAGIC	0x534E4D42u		/* 'SNMB' — 구 snmp.d(문자열 포함, 'SNMP')와 구분 */

typedef struct {
	uint32_t magic;
	uint32_t engineBoots;
} SNMP_BOOTS;

static SNMP_CFG snmpCfgRun;		/* 부팅 시 적용값(에이전트·Trap 사용) */

static void setField(char *dst, const char *src)
{
	strncpy(dst, src, SNMP_CFG_STR_LEN);
	dst[SNMP_CFG_STR_LEN] = '\0';
}

/* 비밀번호 → RFC 3414 키(Ku). 8자 미만이면 실패(USM 규칙) */
static int genKey(const char *pass, uint8_t *key)
{
	SnmpKey k;

	if (strlen(pass) < 8 || snmpGenerateKey(SNMP_AUTH_PROTOCOL_SHA1, pass, &k) != NO_ERROR)
		return -1;
	memcpy(key, k.b, 20);
	return 0;
}

void snmpCfgLoad(void)
{
	const COMM_CFG *c = &pdb->comm;
	SNMP_BOOTS boots;
	FILE *fp;
	size_t n = 0;

	memset(&snmpCfgRun, 0, sizeof(snmpCfgRun));
	setField(snmpCfgRun.community, c->snmpCommunity);
	setField(snmpCfgRun.contact,   c->snmpContact);
	setField(snmpCfgRun.location,  c->snmpLocation);
	setField(snmpCfgRun.v3User,    c->snmpV3User);
	if (snmpCfgRun.community[0] == '\0')
		strcpy(snmpCfgRun.community, "public");

	/* v3: 사용자와 인증 비밀번호가 있어야 사용. 암호화 단독은 USM상 불가 */
	if (snmpCfgRun.v3User[0] != '\0' && genKey(c->snmpV3AuthPass, snmpCfgRun.v3AuthKey) == 0) {
		snmpCfgRun.v3Flags = SNMP_V3F_AUTH;
		if (genKey(c->snmpV3PrivPass, snmpCfgRun.v3PrivKey) == 0)
			snmpCfgRun.v3Flags |= SNMP_V3F_PRIV;
	}

	fsFileLock();
	fp = fopen(SNMP_BOOTS_FILE, "rb");
	if (fp != NULL) {
		n = fread(&boots, 1, sizeof(boots), fp);
		fclose(fp);
	}
	fsFileUnlock();
	if (n == sizeof(boots) && boots.magic == SNMP_BOOTS_MAGIC)
		snmpCfgRun.engineBoots = boots.engineBoots;
}

const SNMP_CFG *snmpCfgActive(void) { return &snmpCfgRun; }

uint32_t snmpCfgNextBoots(void)
{
	SNMP_BOOTS boots;
	FILE *fp;

	/* RFC 3414: snmpEngineBoots는 재기동마다 증가해야 한다(재생 공격 방지 시간창).
	 * 부팅 초기(FS_task 기동 전)라 FS 큐 대신 직접 기록 */
	if (snmpCfgRun.engineBoots < 0x7FFFFFFFu)
		snmpCfgRun.engineBoots++;
	boots.magic = SNMP_BOOTS_MAGIC;
	boots.engineBoots = snmpCfgRun.engineBoots;

	fsFileLock();
	fp = fopen(SNMP_BOOTS_FILE, "wb");
	if (fp != NULL) {
		fwrite(&boots, sizeof(boots), 1, fp);
		fclose(fp);
	}
	fsFileUnlock();
	if (fp == NULL)
		printf("[SNMP] %s write failed (engineBoots=%u)\n", SNMP_BOOTS_FILE, (unsigned)boots.engineBoots);

	return snmpCfgRun.engineBoots;
}

void snmpCfgFactoryReset(void)
{
	fsFileLock();
#ifdef USE_CMSIS_RTOS2
	(void)fdelete(SNMP_BOOTS_FILE, NULL);
#else
	(void)fdelete(SNMP_BOOTS_FILE);
#endif
	fsFileUnlock();
	snmpCfgRun.engineBoots = 0;
}
