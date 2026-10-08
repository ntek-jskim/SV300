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
	/* SNMPv3 — 자리만 확보(§4.5-③에서 사용). 비밀번호 원문 대신 키(SHA-1 20B)만 저장 */
	char     v3User[SNMP_CFG_STR_LEN + 1];
	uint8_t  v3AuthKey[20];
	uint8_t  v3PrivKey[20];
	uint8_t  v3Flags, _r1[7];
} SNMP_CFG;

/* 부팅 시 1회 — 파일이 없거나 깨졌으면 기본값(community "public", 나머지 빈 값) */
void snmpCfgLoad(void);

/* 현재 저장된(=다음 부팅에 적용될) 설정. 부팅 시 적용값은 snmpCfgActive() */
const SNMP_CFG *snmpCfgGet(void);
const SNMP_CFG *snmpCfgActive(void);

/* 웹 저장 — 변경 내용을 RAM에 반영하고 FS 큐로 파일 기록(다음 부팅부터 적용). NULL 인자는 유지 */
void snmpCfgSave(const char *community, const char *contact, const char *location);

/* 공장초기화 — 파일 삭제 + RAM 기본값 */
void snmpCfgFactoryReset(void);

#endif /* SNMP_CFG_H */
