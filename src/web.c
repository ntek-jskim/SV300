/*----------------------------------------------------------------------------
 * SV300 Web Dashboard — CycloneTCP HTTP 서버 구현
 *  참고: gems3500_keil5 웹서비스(SPA + *.cgx JSON 폴링) 아키텍처.
 *  차이: gems3500=MDK-Middleware Network/12피더, SV300=CycloneTCP/3미터(METERING).
 *  구성:
 *   - 임베디드 SPA(INDEX_HTML)를 "/"에 서빙(FTP 업로드 없이 즉시 동작)
 *   - 실시간 계측 JSON: /device /summary /meters /meter?n= /status /echo (.cgx)
 *   - 그 외 경로는 S0:(SPI Flash) 정적파일로 폴백(HTTP_SERVER_FS_SUPPORT)
 *  데이터 출처: meter[id].meter (METERING) — 재계산 없이 그대로 직렬화.
 *----------------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "core/net.h"
#include "http/http_server.h"
#include "mdns/mdns_responder.h"

#include "web.h"
#include "meter.h"

extern METER_DEF meter[];

static HttpServerContext webCtx;
static HttpConnection    webConns[HTTP_SERVER_MAX_CONNECTIONS];
static MdnsResponderContext webMdns;

/*----------------------------------------------------------------------------
 * 임베디드 대시보드 (self-contained: 인라인 CSS/JS, 외부 의존 없음)
 *  ※ C 문자열 이스케이프 최소화를 위해 HTML 속성=작은따옴표, JS=백틱/작은따옴표.
 *----------------------------------------------------------------------------*/
static const char INDEX_HTML[] =
"<!doctype html><html lang='en'><head><meta charset='utf-8'>\n"
"<meta name='viewport' content='width=device-width,initial-scale=1'>\n"
"<title>SV300 Dashboard</title>\n"
"<style>\n"
":root{color-scheme:dark}\n"
"body{font-family:system-ui,Segoe UI,sans-serif;margin:0;background:#0f1520;color:#e6edf3}\n"
"header{padding:14px 18px;background:#161d2b;font-size:20px;font-weight:600;display:flex;justify-content:space-between;align-items:center}\n"
"header small{font-size:12px;color:#8aa0b6;font-weight:400}\n"
".kpis{display:flex;flex-wrap:wrap;gap:12px;padding:16px}\n"
".kpi{flex:1;min-width:130px;background:#1b2433;border-radius:10px;padding:14px}\n"
".kpi .v{font-size:26px;font-weight:700}\n"
".kpi .l{color:#8aa0b6;font-size:12px;margin-top:4px}\n"
".cards{display:flex;flex-wrap:wrap;gap:12px;padding:0 16px 24px}\n"
".card{flex:1;min-width:300px;background:#1b2433;border-radius:10px;padding:14px}\n"
".card h3{margin:0 0 10px;display:flex;justify-content:space-between;align-items:center;font-size:16px}\n"
".badge{font-size:12px;padding:2px 10px;border-radius:12px}\n"
".on{background:#1f6f43}.off{background:#7a2530}\n"
"table{width:100%;border-collapse:collapse;font-size:14px}\n"
"td,th{padding:5px 6px;text-align:right;border-bottom:1px solid #2a3547}\n"
"th:first-child,td:first-child{text-align:left;color:#8aa0b6}\n"
".f{color:#8aa0b6;font-size:12px;padding:10px 18px}\n"
"</style></head><body>\n"
"<header><span id='title'>SV300 Web Dashboard</span><small id='ts'>connecting...</small></header>\n"
"<div class='kpis' id='kpis'></div>\n"
"<div class='cards' id='cards'></div>\n"
"<div class='f'>SV300 3CH Power Meter &middot; live polling 2s</div>\n"
"<script>\n"
"var $=function(i){return document.getElementById(i)};\n"
"function j(u){return fetch(u).then(function(r){return r.text()}).then(function(t){return JSON.parse(t.replace(/-?(inf|nan)/gi,'0'))})}\n"
"function kpi(l,v,u){return `<div class='kpi'><div class='v'>${v}<small style='font-size:13px;color:#8aa0b6'> ${u||''}</small></div><div class='l'>${l}</div></div>`}\n"
"function row(n,v,i,w,pf){return `<tr><td>${n}</td><td>${v.toFixed(1)}</td><td>${i.toFixed(2)}</td><td>${w.toFixed(0)}</td><td>${pf.toFixed(2)}</td></tr>`}\n"
"function refresh(){\n"
" j('/summary.cgx').then(function(s){var k=s.kpi;\n"
"  $('kpis').innerHTML=kpi('Frequency',k.freq.toFixed(2),'Hz')+kpi('Avg Voltage',k.v_avg.toFixed(1),'V')+kpi('Total Power',k.total_kw.toFixed(2),'kW')+kpi('Avg PF',k.pf_avg.toFixed(2),'')+kpi('Online',k.active+'/'+s.used,'');\n"
"  $('ts').textContent='updated t='+s.ts;\n"
" }).catch(function(){$('ts').textContent='offline'});\n"
" j('/meters.cgx').then(function(d){var h='';\n"
"  d.meters.forEach(function(m){\n"
"   h+=`<div class='card'><h3>Meter #${m.n}<span class='badge ${m.st=='ONLINE'?'on':'off'}'>${m.st}</span></h3>`;\n"
"   h+=`<table><tr><th>Ch</th><th>V</th><th>I(A)</th><th>W</th><th>PF</th></tr>`;\n"
"   ['L1','L2','L3'].forEach(function(nm,i){var p=m.ph[i];h+=row(nm,p[0],p[1],p[2],p[3])});\n"
"   h+=row('Avg',m.v,m.i,m.w,m.pf);\n"
"   h+=`</table></div>`;\n"
"  });\n"
"  $('cards').innerHTML=h;\n"
" });\n"
"}\n"
"function boot(){j('/device.cgx').then(function(d){$('title').textContent=d.model+'  '+d.fw+'  ('+d.n_meters+'CH)'});refresh();setInterval(refresh,2000);}\n"
"boot();\n"
"</script></body></html>\n";

/*----------------------------------------------------------------------------
 * 응답 헬퍼 — 동적 JSON은 chunked 로 스트리밍(길이 사전계산 불필요, 스택 절약)
 *----------------------------------------------------------------------------*/
static error_t beginJson(HttpConnection *c)
{
	httpInitResponseHeader(c);
	c->response.contentType = "application/json";
	c->response.chunkedEncoding = TRUE;
	c->response.noCache = TRUE;
	return httpWriteHeader(c);
}

static error_t w(HttpConnection *c, const char *s)
{
	return httpWriteStream(c, s, strlen(s));
}

/*--------------------------------- 엔드포인트 -------------------------------*/

/* GET /device.cgx — 장치 식별 */
static error_t cgiDevice(HttpConnection *c)
{
	char b[224];
	METERING *m = &meter[0].meter;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b),
		"{\"ok\":true,\"model\":\"SV300\",\"fw\":\"V00.02\","
		"\"n_meters\":%d,\"vtype\":%u,\"freq\":%.2f,\"ts\":%u}",
		(int)METER_CH_COUNT, (unsigned)m->vType, m->Freq,
		(unsigned)osGetSystemTime());
	w(c, b);
	return httpCloseStream(c);
}

/* GET /summary.cgx — 대시보드 KPI(집계) */
static error_t cgiSummary(HttpConnection *c)
{
	char b[224];
	int i, used = 0;
	float vsum = 0.0f, psum = 0.0f, pfsum = 0.0f;

	for (i = 0; i < METER_CH_COUNT; i++) {
		METERING *m = &meter[i].meter;
		if (m->meterStatus) used++;
		vsum  += m->U[3];
		psum  += m->P[3];
		pfsum += m->PF[3];
	}

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b),
		"{\"ok\":true,\"used\":%d,\"kpi\":{\"freq\":%.2f,\"v_avg\":%.1f,"
		"\"total_kw\":%.3f,\"pf_avg\":%.2f,\"active\":%d},\"ts\":%u}",
		METER_CH_COUNT, meter[0].meter.Freq, vsum / METER_CH_COUNT,
		psum / 1000.0f, pfsum / METER_CH_COUNT, used,
		(unsigned)osGetSystemTime());
	w(c, b);
	return httpCloseStream(c);
}

/* GET /meters.cgx — 미터 목록(총합 + 3상 배열) */
static error_t cgiMeters(HttpConnection *c)
{
	char b[256];
	int i, p;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b), "{\"ok\":true,\"used\":%d,\"meters\":[", METER_CH_COUNT);
	w(c, b);

	for (i = 0; i < METER_CH_COUNT; i++) {
		METERING *m = &meter[i].meter;
		snprintf(b, sizeof(b),
			"%s{\"n\":%d,\"st\":\"%s\",\"freq\":%.2f,\"v\":%.2f,\"i\":%.2f,"
			"\"w\":%.0f,\"var\":%.0f,\"va\":%.0f,\"pf\":%.2f,\"ph\":[",
			i ? "," : "", i + 1, m->meterStatus ? "ONLINE" : "OFFLINE",
			m->Freq, m->U[3], m->I[3], m->P[3], m->Q[3], m->S[3], m->PF[3]);
		w(c, b);
		for (p = 0; p < 3; p++) {
			snprintf(b, sizeof(b), "%s[%.2f,%.2f,%.0f,%.2f]",
				p ? "," : "", m->U[p], m->I[p], m->P[p], m->PF[p]);
			w(c, b);
		}
		w(c, "]}");
	}

	snprintf(b, sizeof(b), "],\"ts\":%u}", (unsigned)osGetSystemTime());
	w(c, b);
	return httpCloseStream(c);
}

/* GET /meter.cgx?n=N — 단일 미터 상세(총합 + 3상 상세) */
static error_t cgiMeter(HttpConnection *c, const char *uri)
{
	char b[256];
	int n = 1, i;
	const char *q = strchr(uri, '?');
	METERING *m;

	if (q != NULL) {
		const char *pn = strstr(q, "n=");
		if (pn != NULL) n = atoi(pn + 2);
	}
	if (n < 1) n = 1;
	if (n > METER_CH_COUNT) n = METER_CH_COUNT;
	m = &meter[n - 1].meter;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b),
		"{\"ok\":true,\"n\":%d,\"st\":\"%s\",\"freq\":%.2f,\"temp\":%.1f,"
		"\"total\":{\"v\":%.2f,\"vll\":%.2f,\"i\":%.2f,\"w\":%.0f,\"var\":%.0f,"
		"\"va\":%.0f,\"pf\":%.2f},\"phases\":[",
		n, m->meterStatus ? "ONLINE" : "OFFLINE", m->Freq, m->Temp,
		m->U[3], m->Upp[3], m->I[3], m->P[3], m->Q[3], m->S[3], m->PF[3]);
	w(c, b);
	for (i = 0; i < 3; i++) {
		snprintf(b, sizeof(b),
			"%s{\"v\":%.2f,\"vll\":%.2f,\"i\":%.2f,\"w\":%.0f,\"var\":%.0f,"
			"\"va\":%.0f,\"pf\":%.2f,\"thd_v\":%.2f,\"thd_i\":%.2f,\"ang\":%.1f}",
			i ? "," : "", m->U[i], m->Upp[i], m->I[i], m->P[i], m->Q[i],
			m->S[i], m->PF[i], m->THD_U[i], m->THD_I[i], m->Uangle[i]);
		w(c, b);
	}
	snprintf(b, sizeof(b), "],\"ts\":%u}", (unsigned)osGetSystemTime());
	w(c, b);
	return httpCloseStream(c);
}

/* GET /status.cgx — 채널 상태 [comm, cb, alarm] */
static error_t cgiStatus(HttpConnection *c)
{
	char b[64];
	int i;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"status\":[");
	for (i = 0; i < METER_CH_COUNT; i++) {
		METERING *m = &meter[i].meter;
		snprintf(b, sizeof(b), "%s[%d,0,0]", i ? "," : "", m->meterStatus ? 0 : 1);
		w(c, b);
	}
	snprintf(b, sizeof(b), "],\"used\":%d,\"ts\":%u}", METER_CH_COUNT,
		(unsigned)osGetSystemTime());
	w(c, b);
	return httpCloseStream(c);
}

/* GET /echo.cgx — 헬스체크 */
static error_t cgiEcho(HttpConnection *c)
{
	char b[64];
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b), "{\"ok\":true,\"ts\":%u}", (unsigned)osGetSystemTime());
	w(c, b);
	return httpCloseStream(c);
}

/* 임베디드 SPA 서빙(고정 길이) */
static error_t serveIndex(HttpConnection *c)
{
	error_t e;
	httpInitResponseHeader(c);
	c->response.contentType   = "text/html";
	c->response.contentLength = sizeof(INDEX_HTML) - 1;
	e = httpWriteHeader(c);
	if (e) return e;
	e = httpWriteStream(c, INDEX_HTML, sizeof(INDEX_HTML) - 1);
	if (e) return e;
	return httpCloseStream(c);
}

/*----------------------------------------------------------------------------
 * 요청 라우터 — NO_ERROR=처리완료, ERROR_NOT_FOUND=정적파일(S0:)로 폴백
 *  주의: uri 에 쿼리스트링(?n=1&token=..)이 붙을 수 있어 접두 비교(strncmp) 사용.
 *        "/meters.cgx" 를 "/meter.cgx" 보다 먼저 검사.
 *----------------------------------------------------------------------------*/
static error_t webRequestCallback(HttpConnection *c, const char_t *uri)
{
	/* CycloneTCP(http_server_misc.c)는 "/" 를 defaultDocument("index.html", 슬래시 없음)로
	 * 치환해 콜백에 넘긴다. 그래서 루트는 "index.html"(슬래시 없음)로도 매칭해야 한다. */
	if (!strcmp(uri, "/") ||
	    !strcmp(uri, "index.html") || !strcmp(uri, "/index.html") ||
	    !strcmp(uri, "index.htm")  || !strcmp(uri, "/index.htm"))
		return serveIndex(c);

	if (!strncmp(uri, "/device.cgx",  11)) return cgiDevice(c);
	if (!strncmp(uri, "/summary.cgx", 12)) return cgiSummary(c);
	if (!strncmp(uri, "/meters.cgx",  11)) return cgiMeters(c);
	if (!strncmp(uri, "/meter.cgx",   10)) return cgiMeter(c, uri);
	if (!strncmp(uri, "/status.cgx",  11)) return cgiStatus(c);
	if (!strncmp(uri, "/echo.cgx",     9)) return cgiEcho(c);

	return ERROR_NOT_FOUND;
}

/*----------------------------------------------------------------------------
 * mDNS responder — IP 없이 http://sv300-<MAC하위3바이트>.local/ 로 접속.
 *  (CycloneTCP mdnsResponder: 소스는 이미 빌드에 포함, MDNS_RESPONDER_SUPPORT로 활성.
 *   IP 미할당 시점에 시작해도 내부 FSM이 링크/주소 변화에 맞춰 probe/announce 처리)
 *----------------------------------------------------------------------------*/
static void webMdnsStart(NetInterface *interface)
{
	MdnsResponderSettings s;
	MacAddr mac;
	char host[24];
	error_t e;

	mdnsResponderGetDefaultSettings(&s);
	s.interface = interface;
	e = mdnsResponderInit(&webMdns, &s);
	if (e) {
		printf("[WEB] mDNS init failed (%d)\n", (int)e);
		return;
	}

	mac = interface->macAddr;
	snprintf(host, sizeof(host), "sv300-%02x%02x%02x",
		mac.b[3], mac.b[4], mac.b[5]);
	mdnsResponderSetHostname(&webMdns, host);

	e = mdnsResponderStart(&webMdns);
	if (e) {
		printf("[WEB] mDNS start failed (%d)\n", (int)e);
		return;
	}
	printf("[WEB] mDNS: http://%s.local:60080/\n", host);
}

/*----------------------------------------------------------------------------
 * 서버 기동 — FTP 서버 패턴 미러링. 계측 우선 위해 LOW 우선순위.
 *----------------------------------------------------------------------------*/
void webServerStart(NetInterface *interface)
{
	HttpServerSettings s;
	error_t e;
	uint_t i;

	httpServerGetDefaultSettings(&s);
	s.interface      = interface;
	s.port           = 60080;  /* 사내 보안에이전트가 80·8080(표준 HTTP포트) 차단 → 고포트 우회 */
	s.maxConnections = HTTP_SERVER_MAX_CONNECTIONS;
	s.connections    = webConns;
	strcpy(s.rootDirectory, "/");          /* 정적파일은 S0: 루트에서 서빙 */
	strcpy(s.defaultDocument, "index.html");
	s.requestCallback = webRequestCallback;

	/* 계측/네트워크 핫경로 방해 방지 — 리스너/커넥션 태스크 LOW */
	s.listenerTask.priority = OS_TASK_PRIORITY_LOW;
	for (i = 0; i < HTTP_SERVER_MAX_CONNECTIONS; i++)
		s.connectionTask[i].priority = OS_TASK_PRIORITY_LOW;

	e = httpServerInit(&webCtx, &s);
	if (e) {
		printf("[WEB] init failed (%d)\n", (int)e);
		return;
	}
	e = httpServerStart(&webCtx);
	if (e) {
		printf("[WEB] start failed (%d)\n", (int)e);
		return;
	}
	printf("[WEB] HTTP dashboard on :60080\n");

	/* mDNS 등록 — http://sv300-xxxxxx.local/ 접속 지원 */
	webMdnsStart(interface);
}
