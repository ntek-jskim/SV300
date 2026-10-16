/*----------------------------------------------------------------------------
 * SV300 SNMP 문자열·비밀값 설정 — \system\snmp.d (Modbus 맵 밖, 웹 SNMP 카드에서만 설정)
 *----------------------------------------------------------------------------*/
#ifndef SNMP_CFG_H
#define SNMP_CFG_H

#include <stdint.h>

#define SNMP_CFG_STR_LEN	32		/* 문자열 최대 길이(NUL 제외) */

typedef struct {
	uint32_t magic;
	uint16_t ver, _r0;
	char     community[SNMP_CFG_STR_LEN + 1];	/* 읽기 커뮤니티(Trap에도 사용) */
	char     contact[SNMP_CFG_STR_LEN + 1];		/* sysContact */
	char     location[SNMP_CFG_STR_LEN + 1];	/* sysLocation */
	/* SNMPv3 USM 사용자 1명(RO). 비밀번호 원문 대신 RFC 3414 키(Ku, SHA-1 20B)만 저장 —
	 * 키는 장비 engineID와 무관하므로 부팅 시 에이전트가 engineID로 지역화한다 */
	char     v3User[SNMP_CFG_STR_LEN + 1];
	uint8_t  v3AuthKey[20];
	uint8_t  v3PrivKey[20];
	uint8_t  v3Flags, _r1[3];					/* SNMP_V3F_xxx */
	uint32_t engineBoots;						/* snmpEngineBoots — SNMP 기동마다 +1 (RFC 3414 시간창) */
} SNMP_CFG;

#define SNMP_V3F_AUTH		0x01	/* v3AuthKey 유효(HMAC-SHA-1) */
#define SNMP_V3F_PRIV		0x02	/* v3PrivKey 유효(AES-128) — AUTH 없이 단독 불가 */

/* 부팅 시 1회 — 파일이 없거나 깨졌으면 기본값(community "public", 나머지 빈 값) */
void snmpCfgLoad(void);

/* 현재 저장된(=다음 부팅에 적용될) 설정. 부팅 시 적용값은 snmpCfgActive() */
const SNMP_CFG *snmpCfgGet(void);
const SNMP_CFG *snmpCfgActive(void);

/* 웹 저장 — 변경 내용을 RAM에 반영하고 FS 큐로 파일 기록(다음 부팅부터 적용). NULL 인자는 유지 */
void snmpCfgSave(const char *community, const char *contact, const char *location);

/* 웹 저장(v3) — user NULL이면 유지, authKey/privKey NULL이면 해당 키 유지(키는 20B Ku) */
void snmpCfgSaveV3(const char *user, const uint8_t *authKey, const uint8_t *privKey);

/* SNMP 기동 시 1회 — engineBoots를 +1 해 즉시 파일에 기록하고 그 값을 돌려준다(RFC 3414) */
uint32_t snmpCfgNextBoots(void);

/* 공장초기화 — 파일 삭제 + RAM 기본값 */
void snmpCfgFactoryReset(void);

#endif /* SNMP_CFG_H */
