/*----------------------------------------------------------------------------
 * SV300 SNMP Trap — alarmProc 알람 발생/복귀·알람 리셋 통보 (snmp.md §5.4)
 *  - storeAlarmLog()/알람 리셋 경로는 큐에 넣기만 하고, 송신은 전용 LOW 태스크가 한다.
 *    계측 태스크가 SNMP 에이전트 뮤텍스(요청 처리 중 점유)를 기다리지 않게 하기 위함.
 *  - 수신지: COMM_CFG.trapEn / trap_ip (Modbus 7292~7296, 웹 SNMP 카드). 저장 즉시 반영.
 *  - comm.snmpEn(7290)=0이면 main.c가 에이전트와 함께 이 모듈도 기동하지 않는다(큐 입력 무시).
 *  - Trap 버전·인증(부팅 시 확정): SNMP Version이 v3 only면 v3(통신 설정의 USM 사용자),
 *    그 외는 v2c(부팅 시 적용된 읽기 커뮤니티 — 에이전트와 동일 값).
 *  - varbind 값은 SV300-MIB의 accessible-for-notify 객체로 실린다(GET으로는 읽히지 않음).
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "core/net.h"
#include "snmp/snmp_agent.h"
#include "mibs/mib_common.h"
#include "mibs/snmp_mib_module.h"
#include "encoding/asn1.h"
#include "meter.h"
#include "snmp_trap.h"
#include "snmp_cfg.h"

/* 자사 Enterprise OID 1.3.6.1.4.1.67140 — IANA PEN 67140(NTEKSYSTEM Co., Ltd., 2026-10 발급).
 * 67140 = 4·128² + 12·128 + 68 → BER base-128 {0x84, 0x8C, 0x44} */
#define SV_PEN_OID			0x84, 0x8C, 0x44
#define SV_PEN_OID_LEN		3
#define SV_PEN				67140UL		/* 위 BER과 같은 값 — SNMPv3 engineID용 */

/* sv300(.<PEN>.1) > svNotifications(.5) > 0(알림) / 1(알림 전용 객체) */
#define SV_ENT_OID_LEN		(5 + SV_PEN_OID_LEN + 2)	/* 1.3.6.1.4.1.<PEN>.1.5 */
#define SV_EVT_OID_LEN		(SV_ENT_OID_LEN + 2)		/* ...1.5.1.k */

/* specific trap code → snmpTrapOID = <PEN>.1.5.0.<code> */
#define TRAP_ALARM_RAISED	1
#define TRAP_ALARM_CLEARED	2
#define TRAP_ALARM_RESET	3

#define TRAP_Q_SIZE			16

/*---------------------------- 알림 전용 객체 값 ----------------------------*/
static int32_t  almMeter, almSlot, almChan, almCond;
static char     almChanName[16], almLevel[16], almValue[16];
static size_t   almChanNameLen, almLevelLen, almValueLen;
static uint32_t almTime;

static error_t svMibInit(void) { return NO_ERROR; }
static error_t svMibLoad(void *context) { (void)context; return NO_ERROR; }
static void svMibUnload(void *context) { (void)context; }
static void svMibLock(void) { }
static void svMibUnlock(void) { }

/* OID 오름차순 정렬 필수(snmpFindMibObject 이진탐색) */
static const MibObject svMibObjects[] =
{
	{ "svAlmMeter",    {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 1}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_INTEGER, MIB_ACCESS_FOR_NOTIFY,
	  &almMeter, NULL, sizeof(int32_t), NULL, NULL, NULL },
	{ "svAlmSlot",     {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 2}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_INTEGER, MIB_ACCESS_FOR_NOTIFY,
	  &almSlot, NULL, sizeof(int32_t), NULL, NULL, NULL },
	{ "svAlmChan",     {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 3}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_INTEGER, MIB_ACCESS_FOR_NOTIFY,
	  &almChan, NULL, sizeof(int32_t), NULL, NULL, NULL },
	{ "svAlmChanName", {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 4}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_OCTET_STRING, MIB_ACCESS_FOR_NOTIFY,
	  almChanName, &almChanNameLen, sizeof(almChanName), NULL, NULL, NULL },
	{ "svAlmCond",     {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 5}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_INTEGER, MIB_ACCESS_FOR_NOTIFY,
	  &almCond, NULL, sizeof(int32_t), NULL, NULL, NULL },
	{ "svAlmLevel",    {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 6}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_OCTET_STRING, MIB_ACCESS_FOR_NOTIFY,
	  almLevel, &almLevelLen, sizeof(almLevel), NULL, NULL, NULL },
	{ "svAlmValue",    {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 7}, SV_EVT_OID_LEN,
	  ASN1_CLASS_UNIVERSAL, ASN1_TYPE_OCTET_STRING, MIB_ACCESS_FOR_NOTIFY,
	  almValue, &almValueLen, sizeof(almValue), NULL, NULL, NULL },
	{ "svAlmTime",     {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 8}, SV_EVT_OID_LEN,
	  ASN1_CLASS_APPLICATION, MIB_TYPE_UNSIGNED32, MIB_ACCESS_FOR_NOTIFY,
	  &almTime, NULL, sizeof(uint32_t), NULL, NULL, NULL },
};

static const MibModule sv300MibModule =
{
	"SV300-MIB",
	{43, 6, 1, 4, 1, SV_PEN_OID, 1},
	5 + SV_PEN_OID_LEN + 1,
	svMibObjects,
	arraysize(svMibObjects),
	svMibInit,
	svMibLoad,
	svMibUnload,
	svMibLock,
	svMibUnlock
};

static const uint8_t svEnterpriseOid[] = {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5};

/* sysObjectID = 1.3.6.1.4.1.67140.1 (sv300) — NMS 장비 종류 자동 인식용 */
static const uint8_t svSysObjectId[] = {43, 6, 1, 4, 1, SV_PEN_OID, 1};

/* 알람 발생/복귀 varbind: meter, slot, chan, chanName, cond, level, value, time */
static const SnmpTrapObject almTrapObjs[] =
{
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 1, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 2, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 3, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 4, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 5, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 6, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 7, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 8, 0}, SV_EVT_OID_LEN + 1 },
};

/* 알람 리셋 varbind: meter, time */
static const SnmpTrapObject rstTrapObjs[] =
{
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 1, 0}, SV_EVT_OID_LEN + 1 },
	{ {43, 6, 1, 4, 1, SV_PEN_OID, 1, 5, 1, 8, 0}, SV_EVT_OID_LEN + 1 },
};

/*---------------------------- 송신 큐 ----------------------------*/
typedef struct {
	uint8_t     code;		/* TRAP_ALARM_xxx */
	uint8_t     meter, slot, chan, cond;
	const char *chanName;	/* almTbl[].nm — 정적 문자열 */
	float       level, value;
	uint32_t    ts;
} TRAP_EVT;

static TRAP_EVT trapQ[TRAP_Q_SIZE];
static uint16_t trapQr, trapQw, trapQn;
static OsEvent  trapEvent;
static SnmpAgentContext *trapCtx;
/* 부팅 시 확정 — snmpVer=2(v3 only)면 v3(USM 사용자), 그 외 v2c(커뮤니티). NULL이면 송신 불가 */
static SnmpVersion trapVersion = SNMP_VERSION_2C;
static const char *trapUser;

/* 생산자는 계측 태스크(들), 소비자는 송신 태스크 — 짧은 구간이라 스케줄러 정지로 보호 */
static void trapPush(const TRAP_EVT *e)
{
	if (trapCtx == NULL)
		return;

	osSuspendAllTasks();
	if (trapQn == TRAP_Q_SIZE) {	/* 가득 참 — 가장 오래된 것 폐기(svAlarmTable 폴링으로 복구) */
		trapQr = (trapQr + 1) % TRAP_Q_SIZE;
		trapQn--;
	}
	trapQ[trapQw] = *e;
	trapQw = (trapQw + 1) % TRAP_Q_SIZE;
	trapQn++;
	osResumeAllTasks();

	osSetEvent(&trapEvent);
}

static int trapPop(TRAP_EVT *e)
{
	int ok = 0;

	osSuspendAllTasks();
	if (trapQn > 0) {
		*e = trapQ[trapQr];
		trapQr = (trapQr + 1) % TRAP_Q_SIZE;
		trapQn--;
		ok = 1;
	}
	osResumeAllTasks();
	return ok;
}

static void setStr(char *dst, size_t size, size_t *len, const char *src)
{
	strncpy(dst, src ? src : "", size - 1);
	dst[size - 1] = '\0';
	*len = strlen(dst);
}

static void trapSend(const TRAP_EVT *e)
{
	const uint16_t *ip = pdb->comm.trap_ip;
	IpAddr dst;
	error_t err;

	/* 에이전트 미기동(시작 실패)이면 소켓이 없으므로 보내지 않는다 */
	if (!trapCtx->running || trapUser == NULL || !pdb->comm.trapEn || ip[0] == 0)
		return;

	almMeter = e->meter;
	almSlot  = e->slot;
	almChan  = e->chan;
	almCond  = e->cond;
	almTime  = e->ts;
	setStr(almChanName, sizeof(almChanName), &almChanNameLen, e->chanName);
	snprintf(almLevel, sizeof(almLevel), "%.2f", e->level);
	almLevelLen = strlen(almLevel);
	snprintf(almValue, sizeof(almValue), "%.2f", e->value);
	almValueLen = strlen(almValue);

	dst.length = sizeof(Ipv4Addr);
	dst.ipv4Addr = IPV4_ADDR(ip[0], ip[1], ip[2], ip[3]);

	if (e->code == TRAP_ALARM_RESET)
		err = snmpAgentSendTrap(trapCtx, &dst, trapVersion, trapUser,
			SNMP_TRAP_ENTERPRISE_SPECIFIC, e->code, rstTrapObjs, arraysize(rstTrapObjs));
	else
		err = snmpAgentSendTrap(trapCtx, &dst, trapVersion, trapUser,
			SNMP_TRAP_ENTERPRISE_SPECIFIC, e->code, almTrapObjs, arraysize(almTrapObjs));

	if (err)
		printf("[SNMP] trap(%d) M%d send failed (%d)\n", e->code, e->meter, (int)err);
}

static void snmpTrapTask(void *param)
{
	TRAP_EVT e;

	(void)param;
	while (1) {
		osWaitForEvent(&trapEvent, 1000);
		while (trapPop(&e))
			trapSend(&e);
	}
}

/*---------------------------- 외부 API ----------------------------*/
void snmpTrapInit(void *ctx)
{
	OsTaskParameters params;
	error_t err;

	/* sysObjectID — PEN 정의를 이 파일 하나로 유지하기 위해 여기서 설정(main.c snmpMibInit 이후 호출) */
	memcpy(snmpMibBase.sysGroup.sysObjectID, svSysObjectId, sizeof(svSysObjectId));
	snmpMibBase.sysGroup.sysObjectIDLen = sizeof(svSysObjectId);

	err = snmpAgentLoadMib((SnmpAgentContext *)ctx, &sv300MibModule);
	if (!err)
		err = snmpAgentSetEnterpriseOid((SnmpAgentContext *)ctx,
			svEnterpriseOid, sizeof(svEnterpriseOid));
	if (err || !osCreateEvent(&trapEvent)) {
		printf("[SNMP] trap init failed (%d)\n", (int)err);
		return;
	}

	params = OS_TASK_DEFAULT_PARAMS;
	params.priority = OS_TASK_PRIORITY_LOW;
	if (osCreateTask("snmpTrap", snmpTrapTask, NULL, &params) == OS_INVALID_TASK_ID) {
		printf("[SNMP] trap task create failed\n");
		return;
	}

	/* Trap 버전·인증 확정(부팅 시 설정 기준 — 에이전트 허용 버전과 함께 재부팅으로만 바뀜) */
	trapVersion = SNMP_VERSION_2C;
	trapUser    = snmpCfgActive()->community;
#if (SNMP_V3_SUPPORT == ENABLED)
	if (pdb->comm.snmpVer == 2) {	/* v3 only */
		trapVersion = SNMP_VERSION_3;
		trapUser    = (snmpCfgActive()->v3Flags & SNMP_V3F_AUTH) ? snmpCfgActive()->v3User : NULL;
		if (trapUser == NULL)
			printf("[SNMP] v3 only but no v3 user — trap disabled\n");
	}
#endif
	trapCtx = (SnmpAgentContext *)ctx;
}

void snmpSetEngineId(void *ctx, const uint8_t *mac)
{
#if (SNMP_V3_SUPPORT == ENABLED)
	/* RFC 3411 snmpEngineID: 0x80000000|PEN(4B) + format 3(MAC) + MAC(6B) = 11B, 장비마다 고유 */
	uint8_t id[11];

	id[0] = (uint8_t)(0x80 | ((SV_PEN >> 24) & 0x7F));
	id[1] = (uint8_t)(SV_PEN >> 16);
	id[2] = (uint8_t)(SV_PEN >> 8);
	id[3] = (uint8_t)SV_PEN;
	id[4] = 3;
	memcpy(&id[5], mac, 6);
	snmpAgentSetContextEngine((SnmpAgentContext *)ctx, id, sizeof(id));
#else
	(void)ctx; (void)mac;
#endif
}

void snmpTrapAlarm(int id, int slot, int status, int chan, int cond,
	float level, float value, uint32_t ts, const char *chanName)
{
	TRAP_EVT e;

	e.code     = status ? TRAP_ALARM_RAISED : TRAP_ALARM_CLEARED;
	e.meter    = (uint8_t)(id + 1);
	e.slot     = (uint8_t)(slot + 1);
	e.chan     = (uint8_t)chan;
	e.cond     = (uint8_t)cond;
	e.chanName = chanName;
	e.level    = level;
	e.value    = value;
	e.ts       = ts;
	trapPush(&e);
}

void snmpTrapAlarmReset(int id)
{
	TRAP_EVT e;

	memset(&e, 0, sizeof(e));
	e.code  = TRAP_ALARM_RESET;
	e.meter = (uint8_t)(id + 1);
	e.ts    = sysTick1s;
	trapPush(&e);
}
