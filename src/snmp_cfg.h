/*----------------------------------------------------------------------------
 * SV300 SNMP 실행 설정 — 문자열·비밀번호는 통신 설정(COMM_CFG, Modbus 7300~7395)에 있고
 * 부팅 시 여기로 복사해 쓴다. \system\snmp.d 에는 engineBoots(부팅마다 +1)만 저장한다.
 *----------------------------------------------------------------------------*/
#ifndef SNMP_CFG_H
#define SNMP_CFG_H

#include <stdint.h>

#define SNMP_CFG_STR_LEN	31		/* 문자열 최대 길이(NUL 제외) = COMM_STR_S - 1 */

typedef struct {
	char     community[SNMP_CFG_STR_LEN + 1];	/* 읽기 커뮤니티(Trap에도 사용) */
	char     contact[SNMP_CFG_STR_LEN + 1];		/* sysContact */
	char     location[SNMP_CFG_STR_LEN + 1];	/* sysLocation */
	/* SNMPv3 USM 사용자 1명(RO). 키(Ku, SHA-1 20B)는 부팅 시 비밀번호로 계산 —
	 * 장비 engineID와 무관하므로 에이전트가 engineID로 지역화한다 */
	char     v3User[SNMP_CFG_STR_LEN + 1];
	uint8_t  v3AuthKey[20];
	uint8_t  v3PrivKey[20];
	uint8_t  v3Flags, _r1[3];					/* SNMP_V3F_xxx */
	uint32_t engineBoots;						/* snmpEngineBoots — SNMP 기동마다 +1 (RFC 3414 시간창) */
} SNMP_CFG;

#define SNMP_V3F_AUTH		0x01	/* v3AuthKey 유효(HMAC-SHA-1) */
#define SNMP_V3F_PRIV		0x02	/* v3PrivKey 유효(AES-128) — AUTH 없이 단독 불가 */

/* 부팅 시 1회 — 통신 설정(pdb->comm)에서 문자열 복사, v3 키 계산, snmp.d 에서 engineBoots 로드 */
void snmpCfgLoad(void);

/* 부팅 시 적용값(에이전트·Trap 사용) */
const SNMP_CFG *snmpCfgActive(void);

/* SNMP 기동 시 1회 — engineBoots를 +1 해 즉시 파일에 기록하고 그 값을 돌려준다(RFC 3414) */
uint32_t snmpCfgNextBoots(void);

/* 공장초기화 — snmp.d 삭제(engineBoots 초기화) */
void snmpCfgFactoryReset(void);

#endif /* SNMP_CFG_H */
