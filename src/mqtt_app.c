/*----------------------------------------------------------------------------
 * SV300 MQTT 계측값 전송 (2026-10-28)
 *  - 설정: 통신 설정 MQTT 묶음(comm.mqtt*, Modbus 7160~7289). 부팅 시 반영.
 *  - CycloneTCP MQTT 클라이언트(v3.1.1, TCP, TLS 미지원). 끊기면 5~10초 뒤 재접속.
 *  - 페이로드: GEMS35x와 같은 JSON 배열 — [{"ts":ms,"values":{공통}}, {"ts":ms,"values":{fN_...}}, ...]
 *    값은 Modbus Simple 맵(psmpMap, 50000~)과 같은 원천. 전력 W/var/VA, 전력량 kWh/kvarh/kVAh.
 *  - 기본값: Client ID = sv300-<SN>, 토픽 = sv300/<SN>/data (설정이 비어 있을 때)
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "core/net.h"
#include "mqtt/mqtt_client.h"
#include "meter.h"
#include "web.h"
#include "mqtt_app.h"

#define MQTT_PAYLOAD_MAX	4096		/* 공통 + 9피더 ≈ 3.6KB */

/* 크기가 커서 외부 SDRAM에 둔다(태스크 스택은 RTX 고정 2.5KB) */
static MqttClientContext mqttCtx __attribute__ ((section ("EXT_RAM"), zero_init));
static char mqttPayload[MQTT_PAYLOAD_MAX] __attribute__ ((section ("EXT_RAM"), zero_init));
static char mqttTopicBuf[COMM_STR_L + 8];
static char mqttIdBuf[COMM_STR_S];
static int  mqttPos;

static void ap(const char *fmt, ...)
{
	va_list va;
	int n;

	if (mqttPos >= MQTT_PAYLOAD_MAX - 1)
		return;
	va_start(va, fmt);
	n = vsnprintf(mqttPayload + mqttPos, MQTT_PAYLOAD_MAX - mqttPos, fmt, va);
	va_end(va);
	if (n > 0)
		mqttPos += n;
	if (mqttPos > MQTT_PAYLOAD_MAX - 1)
		mqttPos = MQTT_PAYLOAD_MAX - 1;
}

/* 피더 결선(cbType = pt[f].wiring) → GEMS35x 표기 */
static const char *feederType(uint16_t w)
{
	switch (w) {
	case WM_3LN3CT:    return "3P4W";
	case WM_3LL2CT:
	case WM_3LL3CT:    return "3P3W";
	case WM_1LN1CT_L1: return "1P2W_A";
	case WM_1LN1CT_L2: return "1P2W_B";
	case WM_1LN1CT_L3: return "1P2W_C";
	case WM_1LL2CT:    return "1P3W";
	default:           return "SIM";
	}
}

static float avg3(const float *v, int from, int n)
{
	float s = 0;
	int i;
	for (i = 0; i < n; i++)
		s += v[from + i];
	return n ? s / n : 0;
}

/* JSON 페이로드 작성 — 반환: 길이 */
static int buildPayload(void)
{
	const SMP_MAP *m = psmpMap;
	uint32_t utc = sysTick1s - (int32_t)pdb->comm.timezone * 60;	/* 장치 시각=로컬 → UTC */
	uint16_t fw = meter[0].info.fwVer;
	int f, k, nf = 0;

	for (f = 0; f < MAX_CH; f++)
		if (m->ch[f].cbType != NOT_USED)
			nf++;

	mqttPos = 0;
	ap("[{\"ts\":%lu000,\"values\":{\"version\":\"V%02X.%02X\",\"f_type\":\"common\","
	   "\"frequency\":%.3f,\"temperature\":%.1f,\"n_feeder\":%d}}",
	   (unsigned long)utc, (fw >> 8) & 0xff, fw & 0xff, m->ch[0].Freq, m->common.temp, nf);

	for (f = 0; f < MAX_CH; f++) {
		const SMP_CHANNEL *d = &m->ch[f];
		uint16_t w = d->cbType;
		int n = f + 1, p0 = 0, np = 3;
		const float *u = d->U;

		if (w == NOT_USED)
			continue;
		if (w >= WM_1LN1CT_L1 && w <= WM_1LN1CT_L3) {	/* 단상: 해당 상 자리 하나 */
			p0 = w - WM_1LN1CT_L1;
			np = 1;
		}
		else if (w == WM_1LL2CT)
			np = 2;
		else if (w == WM_3LL2CT || w == WM_3LL3CT)
			u = d->Upp;									/* 3P3W: 선간전압 */

		ap(",{\"ts\":%lu000,\"values\":{\"f%d_type\":\"%s\"", (unsigned long)utc, n, feederType(w));
		for (k = 0; k < np; k++)
			ap(",\"f%d_volt%d\":%.3f", n, k + 1, u[p0 + k]);
		for (k = 0; k < np; k++)
			ap(",\"f%d_amp%d\":%.3f", n, k + 1, d->I[p0 + k]);
		ap(",\"f%d_watt\":%.3f,\"f%d_var\":%.3f,\"f%d_VA\":%.3f,\"f%d_PF\":%.3f",
		   n, d->Ptot, n, d->Qtot, n, d->Stot, n, d->PFtot);
		ap(",\"f%d_kwh_imp\":%lu,\"f%d_kvarh_imp\":%lu,\"f%d_kvah\":%lu",
		   n, (unsigned long)d->kWh_imp, n, (unsigned long)d->kVARh_imp, n, (unsigned long)d->kVAh);
		ap(",\"f%d_unbal\":%.3f,\"f%d_thd\":%.3f}}",
		   n, d->Iunb, n, avg3(d->THD_I, p0, np));
	}
	ap("]");
	return mqttPos;
}

static void mqttTask(void *arg)
{
	const COMM_CFG *c = &pdb->comm;
	systime_t period = (systime_t)c->mqttInterval * 1000, last;
	IpAddr ip;
	error_t err;
	int len;

	(void)arg;
	osDelayTask(10000);		/* 링크·DHCP 안정 대기 */

	for (;;) {
		err = getHostByName(NULL, c->mqttHost, &ip, 0);
		if (!err) {
			mqttClientInit(&mqttCtx);
			mqttClientSetVersion(&mqttCtx, MQTT_VERSION_3_1_1);
			mqttClientSetTransportProtocol(&mqttCtx, MQTT_TRANSPORT_PROTOCOL_TCP);
			mqttClientSetTimeout(&mqttCtx, 20000);
			mqttClientSetKeepAlive(&mqttCtx, c->mqttKeepAlive);
			mqttClientSetIdentifier(&mqttCtx, mqttIdBuf);
			if (c->mqttUser[0])
				mqttClientSetAuthInfo(&mqttCtx, c->mqttUser, c->mqttPass);
			err = mqttClientConnect(&mqttCtx, &ip, c->mqttPort, TRUE);
		}
		if (err) {
			printf("[MQTT] connect %s:%u failed (%d)\n", c->mqttHost, c->mqttPort, (int)err);
			mqttClientClose(&mqttCtx);
			osDelayTask(10000);
			continue;
		}
		printf("[MQTT] connected %s:%u id=%s topic=%s\n", c->mqttHost, c->mqttPort, mqttIdBuf, mqttTopicBuf);

		last = osGetSystemTime() - period;		/* 접속 직후 1회 전송 */
		for (;;) {
			if (timeCompare(osGetSystemTime(), last + period) >= 0) {
				last = osGetSystemTime();
				len = buildPayload();
				err = mqttClientPublish(&mqttCtx, mqttTopicBuf, mqttPayload, len,
					(MqttQosLevel)c->mqttQos, c->mqttRetain ? TRUE : FALSE, NULL);
				if (err) {
					printf("[MQTT] publish failed (%d, len=%d)\n", (int)err, len);
					break;
				}
			}
			err = mqttClientTask(&mqttCtx, 1000);	/* Keep-alive·수신 처리(최대 1초) */
			if (err) {
				printf("[MQTT] connection lost (%d)\n", (int)err);
				break;
			}
		}
		mqttClientClose(&mqttCtx);
		osDelayTask(5000);
	}
}

void mqttAppStart(void)
{
	const COMM_CFG *c = &pdb->comm;
	OsTaskParameters params;
	char host[24];

	if (!c->mqttEn || c->mqttHost[0] == '\0') {
		printf("[MQTT] disabled (en=%u, broker=%s)\n", c->mqttEn, c->mqttHost[0] ? "set" : "empty");
		return;
	}

	/* 기본 Client ID·토픽 — 호스트명 sv300-<SN> 기반 */
	webGetHostname(host, sizeof(host));
	strncpy(mqttIdBuf, c->mqttClientId[0] ? c->mqttClientId : host, sizeof(mqttIdBuf) - 1);
	if (c->mqttTopic[0])
		strncpy(mqttTopicBuf, c->mqttTopic, sizeof(mqttTopicBuf) - 1);
	else
		snprintf(mqttTopicBuf, sizeof(mqttTopicBuf), "sv300/%s/data", host + 6);	/* "sv300-" 뒤 SN */

	params = OS_TASK_DEFAULT_PARAMS;
	params.priority = OS_TASK_PRIORITY_LOW;
	if (osCreateTask("MQTT", mqttTask, NULL, &params) == OS_INVALID_TASK_ID)
		printf("[MQTT] task create failed\n");
}
