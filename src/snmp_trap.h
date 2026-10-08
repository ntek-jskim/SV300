/*----------------------------------------------------------------------------
 * SV300 SNMP Trap — alarmProc 알람 발생/복귀·알람 리셋 통보 (snmp.md §5.4)
 *----------------------------------------------------------------------------*/
#ifndef SNMP_TRAP_H
#define SNMP_TRAP_H

#include <stdint.h>

/* main.c SNMP 에이전트 초기화 중(snmpAgentStart 전) 호출 — SV300-MIB 로드·enterprise OID·송신 태스크 생성.
 * ctx = SnmpAgentContext* */
void snmpTrapInit(void *ctx);

/* storeAlarmLog()에서 호출 — 큐에 넣기만 한다(계측 태스크 비차단). status 1=발생, 0=복귀 */
void snmpTrapAlarm(int id, int slot, int status, int chan, int cond,
	float level, float value, uint32_t ts, const char *chanName);

/* 알람 리셋 명령(rstAlmList=0x1234) 처리 시 호출 — svAlarmReset 1건 */
void snmpTrapAlarmReset(int id);

#endif /* SNMP_TRAP_H */
