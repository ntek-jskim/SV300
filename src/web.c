/*----------------------------------------------------------------------------
 * SV300 Web — CycloneTCP HTTP 서버 (confWebApp UI 이식)
 *  참조: d:\PROJECT\confWebApp (Python FastAPI Modbus 웹앱) — 화면/기능 원본.
 *  방식: 다중 페이지(Jinja) → 단일 임베디드 SPA(로그인 + Dashboard + Setup).
 *  구성:
 *   - 인증: 폼 로그인(admin ntek/0300, viewer viewer/0000) + 쿠키 세션.
 *           쓰기(명령/설정)는 admin 권한 게이트(서버측 재확인).
 *   - Dashboard: /api/dashboard(CH1 순시) + /api/sv300/events(3CH 알람/이벤트).
 *   - 데이터: meter[id].meter(METERING) 및 meter[id].alarm/alist/elist 직렬화.
 *   - 정적파일은 S0:(SPI Flash) 폴백(HTTP_SERVER_FS_SUPPORT).
 *----------------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core/net.h"
#include "http/http_server.h"
#include "mdns/mdns_responder.h"

#include "web.h"
#include "meter.h"

extern METER_DEF meter[];

/* modbus.c: 레지스터 주소로 R/W (CH1 base 0, CH2 10000, CH3 20000) */
extern int writeMemCb(uint16_t address, uint16_t value);

static HttpServerContext webCtx;
static HttpConnection    webConns[HTTP_SERVER_MAX_CONNECTIONS];
static MdnsResponderContext webMdns;

/*============================ 세션/인증 =====================================*/
#define WEB_MAX_SESS   8
#define WEB_TOK_LEN    24
#define WEB_SESS_TTL   (8u * 3600u * 1000u)   /* 8시간(ms, systime) */

#define ROLE_NONE   0
#define ROLE_VIEWER 1
#define ROLE_ADMIN  2

typedef struct {
	char     tok[WEB_TOK_LEN + 1];
	uint8_t  role;
	uint32_t exp;                 /* 만료 systime(ms) */
} WebSess;

static WebSess  webSess[WEB_MAX_SESS];
static uint32_t webRandState = 0x12345678u;

static uint32_t webNow(void) { return (uint32_t)osGetSystemTime(); }

/* xorshift32 — 세션 토큰용(LAN 설정도구 수준, 암호강도 목적 아님) */
static uint32_t webRand(void)
{
	uint32_t x = webRandState;
	x ^= x << 13; x ^= x >> 17; x ^= x << 5;
	webRandState = x;
	return x;
}

static void webRandSeed(NetInterface *it)
{
	MacAddr m = it->macAddr;
	webRandState = (uint32_t)osGetSystemTime() ^ 0x9e3779b9u
		^ ((uint32_t)m.b[3] << 16) ^ ((uint32_t)m.b[4] << 8) ^ m.b[5];
	if (webRandState == 0) webRandState = 0xdeadbeefu;
}

static void webMakeToken(char *out)
{
	static const char hex[] = "0123456789abcdef";
	int i;
	for (i = 0; i < WEB_TOK_LEN; i++) out[i] = hex[webRand() & 0xf];
	out[WEB_TOK_LEN] = 0;
}

static WebSess *sessFind(const char *tok)
{
	int i;
	uint32_t now = webNow();
	if (tok == NULL || tok[0] == 0) return NULL;
	for (i = 0; i < WEB_MAX_SESS; i++) {
		if (webSess[i].role && (int32_t)(webSess[i].exp - now) > 0 &&
		    !strcmp(webSess[i].tok, tok))
			return &webSess[i];
	}
	return NULL;
}

static WebSess *sessNew(uint8_t role)
{
	int i, slot = -1;
	uint32_t now = webNow();
	/* 빈/만료 슬롯 우선, 없으면 0번 재사용 */
	for (i = 0; i < WEB_MAX_SESS; i++) {
		if (webSess[i].role == 0 || (int32_t)(webSess[i].exp - now) <= 0) { slot = i; break; }
	}
	if (slot < 0) slot = 0;
	webMakeToken(webSess[slot].tok);
	webSess[slot].role = role;
	webSess[slot].exp  = now + WEB_SESS_TTL;
	return &webSess[slot];
}

/* 요청 쿠키에서 sid=<token> 추출 */
static void cookieToken(HttpConnection *c, char *out, int max)
{
	const char *p = c->request.cookie;
	out[0] = 0;
	while (p && *p) {
		while (*p == ' ' || *p == ';') p++;
		if (!strncmp(p, "sid=", 4)) {
			int o = 0; p += 4;
			while (*p && *p != ';' && *p != ' ' && o < max - 1) out[o++] = *p++;
			out[o] = 0;
			return;
		}
		p = strchr(p, ';');
	}
}

static uint8_t webRole(HttpConnection *c)
{
	char tok[WEB_TOK_LEN + 1];
	WebSess *s;
	cookieToken(c, tok, sizeof(tok));
	s = sessFind(tok);
	return s ? s->role : ROLE_NONE;
}

/*============================ 응답 헬퍼 =====================================*/
static error_t w(HttpConnection *c, const char *s)
{
	return httpWriteStream(c, s, strlen(s));
}

static error_t beginJson(HttpConnection *c)
{
	httpInitResponseHeader(c);
	c->response.contentType     = "application/json";
	c->response.chunkedEncoding = TRUE;
	c->response.noCache         = TRUE;
	return httpWriteHeader(c);
}

/* 상태코드 지정 JSON 1줄 응답(401/403/400 등) */
static error_t webJsonStatus(HttpConnection *c, uint_t code, const char *body)
{
	httpInitResponseHeader(c);
	c->response.statusCode      = code;
	c->response.contentType     = "application/json";
	c->response.chunkedEncoding = TRUE;
	c->response.noCache         = TRUE;
	if (httpWriteHeader(c)) return ERROR_WRITE_FAILED;
	w(c, body);
	return httpCloseStream(c);
}

/*============================ POST 본문/폼 파싱 =============================*/
static int webReadBody(HttpConnection *c, char *buf, int max)
{
	int n = 0;
	size_t r;
	size_t len = c->request.contentLength;
	if (len >= (size_t)max) len = max - 1;
	while ((size_t)n < len) {
		if (httpReadStream(c, buf + n, len - n, &r, 0)) break;
		if (r == 0) break;
		n += (int)r;
	}
	buf[n] = 0;
	return n;
}

static int hex2(int h)
{
	if (h >= '0' && h <= '9') return h - '0';
	if (h >= 'a' && h <= 'f') return h - 'a' + 10;
	if (h >= 'A' && h <= 'F') return h - 'A' + 10;
	return 0;
}

/* application/x-www-form-urlencoded 필드 추출(간이 URL 디코드) */
static int formField(const char *body, const char *key, char *out, int max)
{
	int klen = strlen(key);
	const char *p = body;
	while (p && *p) {
		if (!strncmp(p, key, klen) && p[klen] == '=') {
			const char *v = p + klen + 1;
			int o = 0;
			while (*v && *v != '&' && o < max - 1) {
				if (*v == '%' && v[1] && v[2]) { out[o++] = (char)(hex2(v[1]) * 16 + hex2(v[2])); v += 3; }
				else if (*v == '+')            { out[o++] = ' '; v++; }
				else                           { out[o++] = *v++; }
			}
			out[o] = 0;
			return o;
		}
		p = strchr(p, '&');
		if (p) p++;
	}
	out[0] = 0;
	return -1;
}

/*============================ 인증 엔드포인트 ===============================*/
static const char *roleName(uint8_t r) { return r == ROLE_ADMIN ? "admin" : (r == ROLE_VIEWER ? "viewer" : "none"); }

/* POST /api/login  (username, password) */
static error_t apiLogin(HttpConnection *c)
{
	char body[192], user[40], pass[40];
	uint8_t role = ROLE_NONE;
	WebSess *s;

	if (strcmp(c->request.method, "POST"))
		return webJsonStatus(c, 405, "{\"ok\":false,\"error\":\"method\"}");

	webReadBody(c, body, sizeof(body));
	formField(body, "username", user, sizeof(user));
	formField(body, "password", pass, sizeof(pass));

	if (!strcmp(user, "ntek")   && !strcmp(pass, "0300")) role = ROLE_ADMIN;
	else if (!strcmp(user, "viewer") && !strcmp(pass, "0000")) role = ROLE_VIEWER;

	if (role == ROLE_NONE)
		return webJsonStatus(c, 401, "{\"ok\":false,\"error\":\"invalid\"}");

	s = sessNew(role);

	httpInitResponseHeader(c);
	c->response.contentType     = "application/json";
	c->response.chunkedEncoding = TRUE;
	c->response.noCache         = TRUE;
	snprintf(c->response.setCookie, sizeof(c->response.setCookie),
		"sid=%s; Path=/; Max-Age=28800; HttpOnly", s->tok);
	if (httpWriteHeader(c)) return ERROR_WRITE_FAILED;

	{
		char b[96];
		snprintf(b, sizeof(b), "{\"ok\":true,\"user\":\"%s\",\"role\":\"%s\"}",
			user, roleName(role));
		w(c, b);
	}
	return httpCloseStream(c);
}

/* GET /api/logout */
static error_t apiLogout(HttpConnection *c)
{
	char tok[WEB_TOK_LEN + 1];
	WebSess *s;
	cookieToken(c, tok, sizeof(tok));
	s = sessFind(tok);
	if (s) s->role = ROLE_NONE;

	httpInitResponseHeader(c);
	c->response.contentType     = "application/json";
	c->response.chunkedEncoding = TRUE;
	c->response.noCache         = TRUE;
	strcpy(c->response.setCookie, "sid=; Path=/; Max-Age=0");
	if (httpWriteHeader(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true}");
	return httpCloseStream(c);
}

/* GET /api/me */
static error_t apiMe(HttpConnection *c)
{
	uint8_t r = webRole(c);
	char b[96];
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	if (r == ROLE_NONE) w(c, "{\"ok\":true,\"auth\":false}");
	else {
		snprintf(b, sizeof(b),
			"{\"ok\":true,\"auth\":true,\"user\":\"%s\",\"role\":\"%s\"}",
			r == ROLE_ADMIN ? "ntek" : "viewer", roleName(r));
		w(c, b);
	}
	return httpCloseStream(c);
}

/*============================ Dashboard 데이터 ==============================*/
static float favg3(const float *a) { return (a[0] + a[1] + a[2]) / 3.0f; }

/* GET /api/dashboard — CH1 순시치(confWebApp iDPM300 대시보드) */
static error_t apiDashboard(HttpConnection *c)
{
	char b[192];   /* 작은 버퍼로 분할 기록(태스크 스택 절약) */
	METERING *m = &meter[0].meter;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b),
		"{\"ok\":true,\"data\":{\"freq\":%.2f,\"v_avg\":%.1f,\"i_avg\":%.2f,"
		"\"u\":[%.1f,%.1f,%.1f],\"i\":[%.2f,%.2f,%.2f],",
		m->Freq, m->U[3], m->I[3],
		m->U[0], m->U[1], m->U[2], m->I[0], m->I[1], m->I[2]);
	w(c, b);
	snprintf(b, sizeof(b),
		"\"p\":%.2f,\"q\":%.2f,\"s\":%.2f,\"pf\":%.1f,"
		"\"u_unbal\":%.1f,\"i_unbal\":%.1f,"
		"\"thd_u\":%.1f,\"thd_i\":%.1f,\"tdd_i\":%.1f}}",
		m->P[3] / 1000.0f, m->Q[3] / 1000.0f, m->S[3] / 1000.0f,
		(float)fabs(m->PF[3]) * 100.0f,
		m->Ubal[0], m->Ibal[0],
		favg3(m->THD_U), favg3(m->THD_I), favg3(m->TDD_I));
	w(c, b);
	return httpCloseStream(c);
}

/* GET /api/sv300/events — 3CH 알람상태/알람로그/이벤트로그/요약 */
#define EV_CAP  24   /* 각 목록 응답 상한(임베디드 응답/루프 바운드) */

static error_t apiEvents(HttpConnection *c)
{
	char b[256];
	int i, k, n;

	if (beginJson(c)) return ERROR_WRITE_FAILED;

	/* alarm status: st[32] 중 status!=0 (활성 알람) */
	w(c, "{\"ok\":true,\"status\":[");
	n = 0;
	for (i = 0; i < METER_CH_COUNT; i++) {
		ALARM_STATUS *a = &meter[i].alarm;
		for (k = 0; k < 32 && n < EV_CAP; k++) {
			if (a->st[k].status) {
				snprintf(b, sizeof(b), "%s{\"chn\":%d,\"chan\":%u,\"count\":%u}",
					n ? "," : "", i + 1, (unsigned)a->st[k].chan, (unsigned)a->st[k].count);
				w(c, b); n++;
			}
		}
	}

	/* alarm log: alist.alog[count] */
	w(c, "],\"alarmlog\":[");
	n = 0;
	for (i = 0; i < METER_CH_COUNT; i++) {
		ALARM_LIST *L = &meter[i].alist;
		int cnt = L->count; if (cnt > N_ALARM_LIST) cnt = N_ALARM_LIST;
		for (k = 0; k < cnt && n < EV_CAP; k++) {
			ALARM_LOG *g = &L->alog[k];
			snprintf(b, sizeof(b),
				"%s{\"chn\":%d,\"chan\":%u,\"set\":%u,\"value\":%.2f,\"ts\":%u}",
				n ? "," : "", i + 1, (unsigned)g->chan, (unsigned)g->status,
				g->value, (unsigned)g->ts);
			w(c, b); n++;
		}
	}

	/* event log: elist.elog[count] */
	w(c, "],\"eventlog\":[");
	n = 0;
	for (i = 0; i < METER_CH_COUNT; i++) {
		EVENT_LIST *E = &meter[i].elist;
		int cnt = E->count; if (cnt > N_EVENT_LIST) cnt = N_EVENT_LIST;
		for (k = 0; k < cnt && n < EV_CAP; k++) {
			EVENT_LOG *e = &E->elog[k];
			float lv = e->level[0];
			if (e->level[1] > lv) lv = e->level[1];
			if (e->level[2] > lv) lv = e->level[2];
			snprintf(b, sizeof(b),
				"%s{\"chn\":%d,\"type\":%u,\"ts\":%u,\"dur\":%u,\"mask\":%u,\"level\":%.2f}",
				n ? "," : "", i + 1, (unsigned)e->type, (unsigned)e->startTs,
				(unsigned)e->duration, (unsigned)e->mask, lv);
			w(c, b); n++;
		}
	}

	/* summary: PQ_EVENT_COUNT 합산(3CH) */
	{
		unsigned tv = 0, ti = 0, oc = 0, sag = 0, sw = 0, intr = 0;
		for (i = 0; i < METER_CH_COUNT; i++) {
			PQ_EVENT_COUNT *p = &meter[i].pqEvtCnt;
			tv += p->tvc; ti += p->tcc; oc += p->oc;
			sag += p->sag; sw += p->swell; intr += p->intr;
		}
		snprintf(b, sizeof(b),
			"],\"summary\":{\"tv\":%u,\"ti\":%u,\"oc\":%u,\"sag\":%u,\"swell\":%u,\"intr\":%u}}",
			tv, ti, oc, sag, sw, intr);
		w(c, b);
	}
	return httpCloseStream(c);
}

/* POST /api/sv300/command  {cmd} — admin 전용, 대상 명령 레지스터에 0x1234 기록 */
static error_t apiCommand(HttpConnection *c)
{
	char body[96], cmd[32];
	uint16_t addr = 0;

	if (webRole(c) != ROLE_ADMIN)
		return webJsonStatus(c, 403, "{\"ok\":false,\"error\":\"admin only\"}");

	webReadBody(c, body, sizeof(body));
	/* JSON {"cmd":"..."} 에서 값만 추출(간이) */
	{
		char *p = strstr(body, "\"cmd\"");
		int o = 0;
		cmd[0] = 0;
		if (p) { p = strchr(p, ':'); if (p) p = strchr(p, '"'); }
		if (p) { p++; while (*p && *p != '"' && o < (int)sizeof(cmd) - 1) cmd[o++] = *p++; cmd[o] = 0; }
	}

	if      (!strcmp(cmd, "clear_alarm")) addr = 7503;
	else if (!strcmp(cmd, "clear_event")) addr = 7513;
	else if (!strcmp(cmd, "ack_alarm"))   addr = 7543;
	else if (!strcmp(cmd, "ack_event"))   addr = 7553;
	else return webJsonStatus(c, 400, "{\"ok\":false,\"error\":\"bad cmd\"}");

	writeMemCb(addr, 0x1234);   /* ALL 대상(offset 0) */
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true}");
	return httpCloseStream(c);
}

/*============================ 임베디드 SPA =================================*/
/*  ※ C 이스케이프 최소화: HTML/SVG 속성=작은따옴표, CSS 문자열=작은따옴표, JS=백틱/작은따옴표. */
static const char INDEX_HTML[] =
"<!doctype html><html lang='en' data-theme='dark'><head><meta charset='utf-8'>\n"
"<meta name='viewport' content='width=device-width,initial-scale=1'>\n"
"<link rel='icon' href='data:,'>\n"
"<title>SV300 · Web</title>\n"
"<script>(function(){try{document.documentElement.setAttribute('data-theme',localStorage.getItem('theme')||'dark');}catch(e){}})();</script>\n"
"<style>\n"
":root{--bg:#0f1419;--panel:#1a212b;--panel2:#222c38;--line:#2c3a4a;--fg:#e6edf3;--muted:#8b9bb0;--accent:#3b82f6;--ok:#22c55e;--warn:#f59e0b;--bad:#ef4444;--purple:#8b5cf6;--cyan:#22d3ee}\n"
"html[data-theme=light]{--bg:#eef2f6;--panel:#fff;--panel2:#eef2f7;--line:#e2e8f0;--fg:#0f172a;--muted:#64748b}\n"
"html[data-theme=light] .topbar nav a.on,html[data-theme=light] .sitem.active{color:#0f172a}\n"
"html[data-theme=light] .role.admin{background:#fef3c7;color:#b45309}html[data-theme=light] .role.viewer{background:#e0f2fe;color:#0369a1}\n"
"*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font-family:'Segoe UI',-apple-system,Roboto,'Malgun Gothic',sans-serif;font-size:14px}\n"
"a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}\n"
".mono{font-family:'Consolas',monospace}\n"
/* topbar */
".topbar{display:flex;align-items:center;gap:20px;padding:10px 18px;background:var(--panel);border-bottom:1px solid var(--line)}\n"
".brand{font-weight:800;font-size:17px;color:var(--accent);letter-spacing:.3px}\n"
".topbar nav{display:flex;gap:14px}.topbar nav a{color:var(--fg);padding:4px 8px;border-radius:6px;cursor:pointer}\n"
".topbar nav a:hover{background:var(--panel2);text-decoration:none}\n"
".topbar nav a.on{background:var(--panel2);color:#fff;box-shadow:inset 0 -2px 0 var(--accent)}\n"
".topbar .status{margin-left:auto;display:flex;align-items:center;gap:12px;color:var(--muted)}\n"
".status .clock{font-family:'Consolas',monospace;font-size:13px;color:var(--fg);background:var(--panel2);padding:4px 10px;border-radius:6px}\n"
".role{padding:2px 8px;border-radius:10px;font-size:12px}.role.admin{background:#3b2a12;color:var(--warn)}.role.viewer{background:#12263b;color:#7dd3fc}\n"
".btn-sm{font-size:12px;padding:3px 8px;border:1px solid var(--line);border-radius:6px;color:var(--fg);background:var(--panel2);cursor:pointer}\n"
/* layout/sidebar */
".layout{display:flex;align-items:flex-start;min-height:calc(100vh - 53px)}\n"
".sidebar{width:216px;flex-shrink:0;background:var(--panel);border-right:1px solid var(--line);padding:8px 10px 24px;position:sticky;top:0;align-self:stretch}\n"
".sgroup{font-size:10px;font-weight:700;color:var(--muted);text-transform:uppercase;letter-spacing:.7px;padding:16px 10px 6px}\n"
".sitem{display:flex;align-items:center;gap:10px;padding:9px 11px;border-radius:8px;color:var(--fg);font-size:13.5px;margin:1px 0;border-left:3px solid transparent;cursor:pointer}\n"
".sitem:hover{background:var(--panel2)}.sitem.active{background:var(--panel2);color:#fff;border-left-color:var(--accent);font-weight:600}\n"
".sitem .ico{font-size:15px;width:18px;text-align:center}\n"
".poll-toggle{margin-top:18px;padding:12px 11px 0;border-top:1px solid var(--line)}\n"
".poll-toggle label{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:12.5px;cursor:pointer}\n"
".poll-state{margin-left:auto;background:var(--ok);color:#06281a;font-weight:700;font-size:10px;padding:2px 8px;border-radius:10px}.poll-state.off{background:var(--muted);color:#10202e}\n"
".container{flex:1;min-width:0;margin:22px auto;padding:0 24px;max-width:1180px}\n"
/* card/login/buttons */
".card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:22px;margin-bottom:18px}\n"
".login{max-width:360px;margin:8vh auto}.card h1{margin:0 0 4px;font-size:20px}.sub{color:var(--muted);margin:0 0 16px}\n"
"label{display:block;margin:10px 0;color:var(--muted);font-size:13px}\n"
"input,select{width:100%;margin-top:5px;padding:9px 10px;background:var(--panel2);color:var(--fg);border:1px solid var(--line);border-radius:8px;font-size:14px}\n"
"input:focus,select:focus{outline:none;border-color:var(--accent)}\n"
".btn{padding:9px 16px;border:1px solid var(--line);background:var(--panel2);color:var(--fg);border-radius:8px;cursor:pointer;font-size:14px}\n"
".btn:hover{border-color:var(--accent)}.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}.btn:disabled{opacity:.4;cursor:not-allowed}\n"
".alert.err{background:#3a1620;color:#fca5a5;border:1px solid #5b2230;padding:10px 12px;border-radius:8px;margin:10px 0}\n"
".hint{color:var(--muted);font-size:12px;margin-top:14px}\n"
/* dashboard */
".dash h1{font-size:24px;font-weight:800;margin:4px 0 14px}\n"
".dstatus{color:var(--muted);font-size:12px;margin:0 0 14px;font-family:'Consolas',monospace;display:flex;align-items:center;gap:14px;flex-wrap:wrap}\n"
".dstatus .on{color:var(--ok)}.dstatus .bad{color:var(--bad)}.dstatus .dmeas{color:var(--fg)}.dstatus .dmeas::before{content:'\\1F552 '}\n"
".dgrid{display:grid;grid-template-columns:1.1fr 1fr 1.1fr;gap:18px;align-items:stretch}\n"
".dgrid2{display:grid;grid-template-columns:repeat(3,1fr);gap:18px;margin-top:18px;align-items:stretch}\n"
"@media(max-width:1100px){.dgrid,.dgrid2{grid-template-columns:1fr}}\n"
".dcard{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:22px 26px;display:flex;flex-direction:column;min-height:340px}\n"
".dgrid2 .dcard{min-height:220px}\n"
".dct{display:flex;align-items:center;gap:8px;font-size:16px;font-weight:700;margin-bottom:22px}\n"
".dct::before{content:'';width:4px;height:16px;border-radius:2px;background:var(--accent)}\n"
".c2 .dct::before{background:var(--purple)}.c3 .dct::before{background:var(--bad)}\n"
".e1 .dct::before{background:var(--bad)}.e2 .dct::before{background:var(--cyan)}.e3 .dct::before{background:var(--purple)}\n"
".bigrow{display:flex;justify-content:space-around;text-align:center;margin-bottom:22px}\n"
".bigm .bl{font-size:13px;color:var(--muted);margin-bottom:8px}\n"
".bigm .bv{font-size:30px;font-weight:800;font-family:'Consolas',monospace}\n"
".bigm .bv .dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--ok);margin-right:6px;vertical-align:middle;box-shadow:0 0 6px var(--ok)}\n"
".bigm .bu{font-size:12px;color:var(--muted);margin-left:3px}\n"
".ptabs{display:grid;grid-template-columns:1fr 1fr;gap:14px;flex:1}\n"
".ptab{background:var(--panel2);border-radius:8px;overflow:hidden;display:flex;flex-direction:column}\n"
".ptab .ph{background:#2a3441;color:var(--muted);font-size:13px;font-weight:700;padding:9px 14px}\n"
".ptab .pr{display:flex;justify-content:space-between;padding:11px 14px;font-size:14px;border-top:1px solid var(--line);flex:1}\n"
".ptab .pr .pk{color:var(--muted)}.ptab .pr .pv{font-family:'Consolas',monospace;font-weight:700;color:var(--ok)}\n"
".pfwrap{display:grid;grid-template-columns:auto 1fr;gap:22px;align-items:center;flex:1}\n"
".gauge{position:relative;width:140px;height:140px}\n"
".gauge .gc{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center}\n"
".gauge .gv{font-size:24px;font-weight:800;color:var(--bad);font-family:'Consolas',monospace}\n"
".gauge .gu{font-size:11px;color:var(--muted)}.gauge .gl{font-size:12px;color:var(--muted);margin-top:3px}.gauge .gs{font-size:11px;color:var(--bad);font-weight:600;margin-top:2px}\n"
".pwrcol{display:flex;flex-direction:column;gap:10px}\n"
".pwrbox{background:var(--panel2);border-radius:8px;padding:10px 14px;display:flex;justify-content:space-between;align-items:center}\n"
".pwrbox .wk{font-size:12px;color:var(--muted)}.pwrbox .wv{font-family:'Consolas',monospace;font-weight:800;font-size:17px}.pwrbox .wu{font-size:11px;color:var(--muted);margin-left:3px}\n"
".ut{font-size:12px;color:var(--muted);font-weight:600;margin:2px 0 10px}\n"
".ubar{display:flex;align-items:center;gap:10px;margin-bottom:10px}.ubar .uk{width:42px;font-size:12px;color:var(--muted)}\n"
".ubar .utrack{flex:1;height:14px;background:var(--panel2);border-radius:7px;overflow:hidden}\n"
".ubar .ufill{height:100%;border-radius:7px;background:var(--ok)}.ubar .ufill.hot{background:var(--bad)}\n"
".ubar .uv{width:56px;text-align:right;font-family:'Consolas',monospace;font-size:13px;font-weight:700}.ubar .uv.g{color:var(--ok)}.ubar .uv.r{color:var(--bad)}\n"
".hsec{margin-top:auto;border-top:1px solid var(--line);padding-top:18px}\n"
".hgrid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;text-align:center}\n"
".hcell .hv{font-family:'Consolas',monospace;font-weight:800;font-size:18px}.hcell .hl{font-size:11px;color:var(--muted);margin-top:8px;padding-top:6px;border-top:2px solid}\n"
".hcell:nth-child(1) .hv{color:var(--purple)}.hcell:nth-child(1) .hl{border-color:var(--purple)}\n"
".hcell:nth-child(2) .hv{color:var(--accent)}.hcell:nth-child(2) .hl{border-color:var(--accent)}\n"
".hcell:nth-child(3) .hv{color:var(--cyan)}.hcell:nth-child(3) .hl{border-color:var(--cyan)}\n"
".dbadge{font-size:12px;font-weight:800;color:var(--muted);background:var(--panel2);border-radius:10px;padding:1px 9px;margin-left:auto}.dbadge.hot{color:#fff;background:var(--bad)}\n"
".ebtn{margin-left:10px;font-size:11px;font-weight:600;color:var(--fg);background:var(--panel2);border:1px solid var(--line);border-radius:7px;padding:4px 10px;cursor:pointer}\n"
".ebtn+.ebtn{margin-left:8px}.ebtn:hover{border-color:var(--accent)}.ebtn:disabled{opacity:.45;cursor:not-allowed}\n"
".elist{flex:1;overflow:auto;max-height:300px}\n"
".etbl{width:100%;border-collapse:collapse;font-size:12px}\n"
".etbl th{text-align:left;color:var(--muted);font-weight:600;padding:6px 8px;border-bottom:1px solid var(--line);position:sticky;top:0;background:var(--panel)}\n"
".etbl td{padding:6px 8px;border-bottom:1px solid var(--line)}.etbl .empty{color:var(--muted);text-align:center;padding:14px}\n"
".chchip{font-size:10px;font-weight:800;color:var(--cyan);background:rgba(34,211,238,.12);border-radius:5px;padding:1px 6px}\n"
".st{font-size:10px;font-weight:800;padding:1px 6px;border-radius:5px}.st.set{color:#fff;background:var(--bad)}.st.clr{color:var(--muted);background:var(--panel2)}\n"
".evt{font-size:11px;font-weight:700;padding:1px 7px;border-radius:5px;white-space:nowrap}\n"
".evt.sag{color:#fbbf24;background:rgba(251,191,36,.14)}.evt.swell{color:#f87171;background:rgba(248,113,113,.14)}\n"
".evt.tr{color:#a78bfa;background:rgba(167,139,250,.14)}.evt.oc{color:#22d3ee;background:rgba(34,211,238,.14)}.evt.intr{color:#fb7185;background:rgba(251,113,133,.14)}\n"
".esum{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:14px;border-top:1px solid var(--line);padding-top:14px}\n"
".esum .sb{background:var(--panel2);border-radius:8px;padding:8px 10px;display:flex;flex-direction:column;gap:3px}\n"
".esum .sb span{font-size:11px;color:var(--muted)}.esum .sb b{font-size:18px;font-weight:800;font-family:'Consolas',monospace}\n"
".ph{color:var(--muted)}\n"
".ph2{color:var(--muted);font-size:13px}\n"
".stub{color:var(--muted);padding:30px 4px}\n"
"</style></head><body>\n"
/* ---------- login view ---------- */
"<div id='v-login'>\n"
" <div class='card login'>\n"
"  <h1>SV300 Web</h1><p class='sub'>Sign in</p>\n"
"  <div class='alert err' id='lerr' style='display:none'></div>\n"
"  <form id='loginForm'>\n"
"   <label>Username<input name='u' id='li-u' autofocus autocomplete='username' required></label>\n"
"   <label>Password<input name='p' id='li-p' type='password' autocomplete='current-password' required></label>\n"
"   <button class='btn primary' type='submit' style='width:100%'>Login</button>\n"
"  </form>\n"
"  <p class='hint'>Admin (settings): <b>ntek</b> &middot; Viewer (read-only): <b>viewer</b></p>\n"
" </div>\n"
"</div>\n"
/* ---------- app view ---------- */
"<div id='v-app' style='display:none'>\n"
" <header class='topbar'>\n"
"  <div class='brand'>SV300</div>\n"
"  <nav>\n"
"   <a id='nav-dash' class='on' onclick=\"go('dash')\">Dashboard</a>\n"
"   <a id='nav-setup' onclick=\"go('setup')\">Setup</a>\n"
"  </nav>\n"
"  <div class='status'>\n"
"   <span class='clock' id='clock'>-</span>\n"
"   <span class='role' id='role'>-</span>\n"
"   <button class='btn-sm' id='themeBtn' onclick='toggleTheme()' title='Light / Dark'>&#127769;</button>\n"
"   <a class='btn-sm' onclick='logout()'>Logout</a>\n"
"  </div>\n"
" </header>\n"
" <div class='layout'>\n"
"  <aside class='sidebar'>\n"
"   <div class='sgroup'>Measure</div>\n"
"   <a class='sitem active' id='si-dash' onclick=\"go('dash')\"><span class='ico'>&#128202;</span>Dashboard</a>\n"
"   <div class='sgroup'>Setup</div>\n"
"   <a class='sitem' id='si-setup' onclick=\"go('setup')\"><span class='ico'>&#128421;</span>Main Setting</a>\n"
"   <div class='poll-toggle'><label><input type='checkbox' id='pollToggle' checked style='width:auto;margin:0'> Live polling <span class='poll-state' id='pollState'>ON</span></label></div>\n"
"  </aside>\n"
"  <main class='container'>\n"
/* dashboard page */
"   <div id='p-dash' class='dash'>\n"
"    <h1>Dashboard</h1>\n"
"    <div class='dstatus'><span id='dstat' class='on'>&#9679; live &#8635; 1s</span><span id='dmeas' class='dmeas'>Measured: -</span></div>\n"
"    <div class='dgrid'>\n"
"     <div class='dcard c1'><div class='dct'>Voltage / Current / Frequency</div>\n"
"      <div class='bigrow'>\n"
"       <div class='bigm'><div class='bl'>Avg Voltage</div><div class='bv'><span class='dot'></span><span id='d-vavg'>-</span><span class='bu'>V</span></div></div>\n"
"       <div class='bigm'><div class='bl'>Avg Current</div><div class='bv'><span class='dot'></span><span id='d-iavg'>-</span><span class='bu'>A</span></div></div>\n"
"       <div class='bigm'><div class='bl'>Frequency</div><div class='bv'><span class='dot'></span><span id='d-freq'>-</span><span class='bu'>Hz</span></div></div>\n"
"      </div>\n"
"      <div class='ptabs'>\n"
"       <div class='ptab'><div class='ph'>Voltage</div><div class='pr'><span class='pk'>L1</span><span class='pv' id='d-u1'>-</span></div><div class='pr'><span class='pk'>L2</span><span class='pv' id='d-u2'>-</span></div><div class='pr'><span class='pk'>L3</span><span class='pv' id='d-u3'>-</span></div></div>\n"
"       <div class='ptab'><div class='ph'>Current</div><div class='pr'><span class='pk'>L1</span><span class='pv' id='d-i1'>-</span></div><div class='pr'><span class='pk'>L2</span><span class='pv' id='d-i2'>-</span></div><div class='pr'><span class='pk'>L3</span><span class='pv' id='d-i3'>-</span></div></div>\n"
"      </div>\n"
"     </div>\n"
"     <div class='dcard c2'><div class='dct'>Power Factor &middot; Active Power</div>\n"
"      <div class='pfwrap'>\n"
"       <div class='gauge'><svg width='140' height='140' viewBox='0 0 140 140'>\n"
"        <circle cx='70' cy='70' r='60' fill='none' stroke='#222c38' stroke-width='12'></circle>\n"
"        <circle id='pfArc' cx='70' cy='70' r='60' fill='none' stroke='#ef4444' stroke-width='12' stroke-linecap='round' stroke-dasharray='377' stroke-dashoffset='377' transform='rotate(-90 70 70)'></circle>\n"
"        <circle id='pfDot' cx='70' cy='10' r='5' fill='#ef4444'></circle></svg>\n"
"        <div class='gc'><div class='gv' id='d-pf'>-</div><div class='gu'>%</div><div class='gl'>PF</div><div class='gs' id='d-pfst'></div></div>\n"
"       </div>\n"
"       <div class='pwrcol'>\n"
"        <div class='pwrbox'><span class='wk'>Active Power (P)</span><span><span class='wv' id='d-p'>-</span><span class='wu'>kW</span></span></div>\n"
"        <div class='pwrbox'><span class='wk'>Reactive Power (Q)</span><span><span class='wv' id='d-q'>-</span><span class='wu'>kVar</span></span></div>\n"
"        <div class='pwrbox'><span class='wk'>Apparent Power (S)</span><span><span class='wv' id='d-s'>-</span><span class='wu'>kVA</span></span></div>\n"
"       </div>\n"
"      </div>\n"
"     </div>\n"
"     <div class='dcard c3'><div class='dct'>Unbalance / Harmonics</div>\n"
"      <div class='usec'><div class='ut'>Unbalance</div>\n"
"       <div class='ubar'><span class='uk'>Voltage</span><div class='utrack'><div class='ufill' id='d-vunb-bar'></div></div><span class='uv g' id='d-vunb'>-</span></div>\n"
"       <div class='ubar'><span class='uk'>Current</span><div class='utrack'><div class='ufill' id='d-iunb-bar'></div></div><span class='uv g' id='d-iunb'>-</span></div>\n"
"      </div>\n"
"      <div class='hsec'><div class='ut'>Harmonics</div><div class='hgrid'>\n"
"       <div class='hcell'><div class='hv' id='d-thdu'>-</div><div class='hl'>THD-U</div></div>\n"
"       <div class='hcell'><div class='hv' id='d-thdi'>-</div><div class='hl'>THD-I</div></div>\n"
"       <div class='hcell'><div class='hv' id='d-tddi'>-</div><div class='hl'>TDD-I</div></div>\n"
"      </div></div>\n"
"     </div>\n"
"    </div>\n"
"    <div class='dgrid2'>\n"
"     <div class='dcard e1'><div class='dct'>Alarm Status <span id='as-cnt' class='dbadge'>0</span><button class='ebtn' id='b-ackal' onclick=\"svCmd('ack_alarm')\" disabled>ACK Alarm</button></div><div class='elist'><table class='etbl' id='as-tbl'></table></div></div>\n"
"     <div class='dcard e2'><div class='dct'>Alarm Log<button class='ebtn' id='b-clral' onclick=\"svCmd('clear_alarm')\" disabled>Clear Alarm</button></div><div class='elist'><table class='etbl' id='al-tbl'></table></div></div>\n"
"     <div class='dcard e3'><div class='dct'>Event Log<button class='ebtn' id='b-clrev' onclick=\"svCmd('clear_event')\" disabled>Clear Event</button><button class='ebtn' id='b-ackev' onclick=\"svCmd('ack_event')\" disabled>ACK Event</button></div><div class='elist'><table class='etbl' id='el-tbl'></table></div><div class='esum' id='el-sum'></div></div>\n"
"    </div>\n"
"   </div>\n"
/* setup page (Phase 2에서 구현) */
"   <div id='p-setup' style='display:none'>\n"
"    <div class='card'><h1>Setup</h1><p class='stub'>Main Setting / Channel Setting 화면은 다음 단계에서 제공됩니다.</p></div>\n"
"   </div>\n"
"  </main>\n"
" </div>\n"
"</div>\n"
/* ---------- script ---------- */
"<script>\n"
"var $=function(i){return document.getElementById(i)};\n"
"var IS_ADMIN=false, cur='dash';\n"
"var AC=['Disabled','Temp','Freq','U1','U2','U3','U~','U12','U23','U31','Upp~','V.Unbal Uo','V.Unbal Uu','I1','I2','I3','I~','Itotal','In','P1','P2','P3','Ptotal','Q1','Q2','Q3','Qtot','D1','D2','D3','D','S1','S2','S3','Stot','PF1','PF2','PF3','PFtot','THD U1','THD U2','THD U3','THD U12','THD U23','THD U31','THD I1','THD I2','THD I3','DDmd P+','DDmd P-','DDmd Q+','DDmd Q-','DDmd S','DDmd I1','DDmd I2','DDmd I3','MDmd P+','MDmd P-','MDmd Q+','MDmd Q-','MDmd S','MDmd I1','MDmd I2','MDmd I3','UnderDev U1','UnderDev U2','UnderDev U3','OverDev U1','OverDev U2','OverDev U3','CF U1','CF U2','CF U3','CF U12','CF U23','CF U31','CF I1','CF I2','CF I3','KF I1','KF I2','KF I3','PSt1','PSt2','PSt3','Plt1','Plt2','Plt3','Sig.V1','Sig.V2','Sig.V3'];\n"
"var ET={1:['Sag','sag'],2:['Swell','swell'],3:['S.Intr','intr'],4:['L.Intr','intr'],5:['OC','oc'],6:['RVC','tr'],7:['Trans V','tr'],8:['Trans I','tr'],9:['SOE','oc']};\n"
"function acn(i){return AC[i]||('#'+i)}\n"
"function j(u,o){o=o||{};var ac=('AbortController'in window)?new AbortController():null,tm=null;if(ac){o.signal=ac.signal;tm=setTimeout(function(){ac.abort()},6000);}\n"
" return fetch(u,o).then(function(r){return r.text().then(function(t){if(tm)clearTimeout(tm);return{s:r.status,d:JSON.parse((t||'{}').replace(/-?(inf|nan)/gi,'0'))}})}).catch(function(e){if(tm)clearTimeout(tm);throw e;})}\n"
"function n(v,d){return(v==null||isNaN(v))?'-':Number(v).toFixed(d)}\n"
"function esc(s){return String(s).replace(/[&<>]/g,function(x){return{'&':'&amp;','<':'&lt;','>':'&gt;'}[x]})}\n"
/* theme + clock */
"function toggleTheme(){var h=document.documentElement,t=h.getAttribute('data-theme')==='light'?'dark':'light';h.setAttribute('data-theme',t);try{localStorage.setItem('theme',t)}catch(e){}tbtn()}\n"
"function tbtn(){var b=$('themeBtn');if(b)b.innerHTML=document.documentElement.getAttribute('data-theme')==='light'?'\\u2600\\uFE0F':'\\uD83C\\uDF19'}\n"
"function p2(x){return(x<10?'0':'')+x}\n"
"function fdt(d){return d.getFullYear()+'-'+p2(d.getMonth()+1)+'-'+p2(d.getDate())+' '+p2(d.getHours())+':'+p2(d.getMinutes())+':'+p2(d.getSeconds())}\n"
"function clk(){var e=$('clock');if(e)e.textContent=fdt(new Date())}\n"
/* auth flow */
"function showLogin(){$('v-login').style.display='';$('v-app').style.display='none'}\n"
"function showApp(){$('v-login').style.display='none';$('v-app').style.display=''}\n"
"function me(){return j('/api/me').then(function(r){\n"
" if(r.d.auth){IS_ADMIN=(r.d.role==='admin');\n"
"  $('role').textContent=r.d.user+' ('+(IS_ADMIN?'Admin':'Viewer')+')';$('role').className='role '+(IS_ADMIN?'admin':'viewer');\n"
"  ['b-ackal','b-clral','b-clrev','b-ackev'].forEach(function(id){$(id).disabled=!IS_ADMIN});\n"
"  showApp();go('dash');pollLoop();return true;}\n"
" showLogin();return false;});}\n"
"function logout(){j('/api/logout').then(function(){IS_ADMIN=false;showLogin()})}\n"
"$('loginForm').addEventListener('submit',function(e){e.preventDefault();\n"
" var b='username='+encodeURIComponent($('li-u').value)+'&password='+encodeURIComponent($('li-p').value);\n"
" j('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(function(r){\n"
"  if(r.d.ok){$('lerr').style.display='none';$('li-p').value='';me();}\n"
"  else{$('lerr').textContent='Invalid username or password';$('lerr').style.display='';}\n"
" }).catch(function(){$('lerr').textContent='Login failed';$('lerr').style.display='';});});\n"
/* nav */
"function go(pg){cur=pg;\n"
" $('p-dash').style.display=pg==='dash'?'':'none';$('p-setup').style.display=pg==='setup'?'':'none';\n"
" $('nav-dash').className=pg==='dash'?'on':'';$('nav-setup').className=pg==='setup'?'on':'';\n"
" $('si-dash').className='sitem'+(pg==='dash'?' active':'');$('si-setup').className='sitem'+(pg==='setup'?' active':'');}\n"
"(function(){var t=$('pollToggle');window.__pollOn=true;t.addEventListener('change',function(){window.__pollOn=t.checked;$('pollState').textContent=t.checked?'ON':'OFF';$('pollState').className='poll-state'+(t.checked?'':' off');});})();\n"
/* dashboard */
"function dstat(ok){var e=$('dstat');if(ok){e.className='on';e.innerHTML='&#9679; live &#8635; 1s'}else{e.className='bad';e.innerHTML='&#9679; read error'}}\n"
"function setGauge(pf){var C=377,f=Math.max(0,Math.min(1,pf/100)),col=pf>=90?'#22c55e':'#ef4444',st=pf>=90?'Normal':'Below limit';\n"
" var arc=$('pfArc');arc.setAttribute('stroke',col);arc.setAttribute('stroke-dashoffset',C*(1-f));\n"
" var a=(-90+f*360)*Math.PI/180,dt=$('pfDot');dt.setAttribute('cx',70+60*Math.cos(a));dt.setAttribute('cy',70+60*Math.sin(a));dt.setAttribute('fill',col);\n"
" $('d-pf').style.color=col;$('d-pfst').style.color=col;$('d-pfst').textContent=st;}\n"
"function setBar(id,bid,v){var e=$(id);e.textContent=n(v,1)+'%';e.className='uv '+(v>=100?'r':'g');var b=$(bid);b.style.width=Math.max(1,Math.min(100,v||0))+'%';b.className='ufill'+(v>=100?' hot':'');}\n"
"function loadDash(){return j('/api/dashboard').then(function(r){if(!r.d.ok){dstat(false);return}var d=r.d.data;\n"
"  $('d-vavg').textContent=n(d.v_avg,1);$('d-iavg').textContent=n(d.i_avg,2);$('d-freq').textContent=n(d.freq,2);\n"
"  $('d-u1').textContent=n(d.u[0],1)+' V';$('d-u2').textContent=n(d.u[1],1)+' V';$('d-u3').textContent=n(d.u[2],1)+' V';\n"
"  $('d-i1').textContent=n(d.i[0],2)+' A';$('d-i2').textContent=n(d.i[1],2)+' A';$('d-i3').textContent=n(d.i[2],2)+' A';\n"
"  $('d-p').textContent=n(d.p,2);$('d-q').textContent=n(d.q,2);$('d-s').textContent=n(d.s,2);\n"
"  $('d-pf').textContent=n(d.pf,2);setGauge(d.pf);\n"
"  setBar('d-vunb','d-vunb-bar',d.u_unbal);setBar('d-iunb','d-iunb-bar',d.i_unbal);\n"
"  $('d-thdu').textContent=n(d.thd_u,1)+'%';$('d-thdi').textContent=n(d.thd_i,1)+'%';$('d-tddi').textContent=n(d.tdd_i,1)+'%';\n"
"  $('dmeas').textContent='Measured: '+fdt(new Date());dstat(true);\n"
" }).catch(function(){dstat(false)});}\n"
"function chip(c){return \"<span class='chchip'>CH\"+c+'</span>'}\n"
"function ts2(s){return s?fdt(new Date(s*1000)):'-'}\n"
"function loadEvents(){return j('/api/sv300/events').then(function(r){if(!r.d.ok)return;var d=r.d;\n"
"  $('as-cnt').textContent=d.status.length;$('as-cnt').className='dbadge'+(d.status.length?' hot':'');\n"
"  var h=\"<tr><th>Alarm Channel</th><th>State</th><th>Count</th></tr>\";\n"
"  if(!d.status.length)h+=\"<tr><td class='empty' colspan='3'>\\u2713 No active alarms</td></tr>\";\n"
"  d.status.forEach(function(x){h+='<tr><td>'+chip(x.chn)+' '+esc(acn(x.chan))+\"</td><td><span class='st set'>Set</span></td><td>\"+x.count+'</td></tr>'});$('as-tbl').innerHTML=h;\n"
"  h=\"<tr><th>Alarm Channel</th><th>State</th><th>Value</th><th>Time</th></tr>\";\n"
"  if(!d.alarmlog.length)h+=\"<tr><td class='empty' colspan='4'>No alarm log</td></tr>\";\n"
"  d.alarmlog.forEach(function(x){h+='<tr><td>'+chip(x.chn)+' '+esc(acn(x.chan))+\"</td><td><span class='st \"+(x.set?'set':'clr')+\"'>\"+(x.set?'Set':'Clear')+'</span></td><td>'+n(x.value,2)+\"</td><td class='mono'>\"+ts2(x.ts)+'</td></tr>'});$('al-tbl').innerHTML=h;\n"
"  h=\"<tr><th>Type</th><th>Start</th><th>Dur(ms)</th><th>Phase</th><th>Level</th></tr>\";\n"
"  if(!d.eventlog.length)h+=\"<tr><td class='empty' colspan='5'>No events</td></tr>\";\n"
"  d.eventlog.forEach(function(x){var t=ET[x.type]||['#'+x.type,'oc'],ph=[];if(x.mask&1)ph.push('L1');if(x.mask&2)ph.push('L2');if(x.mask&4)ph.push('L3');\n"
"   h+=\"<tr><td><span class='evt \"+t[1]+\"'>\"+t[0]+'</span> '+chip(x.chn)+\"</td><td class='mono'>\"+ts2(x.ts)+'</td><td>'+x.dur+'</td><td>'+(ph.join(',')||'-')+'</td><td>'+n(x.level,2)+'</td></tr>'});$('el-tbl').innerHTML=h;\n"
"  var s=d.summary,L=[['Transient V',s.tv],['Transient I',s.ti],['Over Current',s.oc],['Sag',s.sag],['Swell',s.swell],['Interruption',s.intr]],e='';\n"
"  L.forEach(function(x){e+=\"<div class='sb'><span>\"+x[0]+'</span><b>'+x[1]+'</b></div>'});$('el-sum').innerHTML=e;\n"
" }).catch(function(){});}\n"
"function svCmd(cmd){var lbl={ack_alarm:'ACK Alarm',clear_alarm:'Clear Alarm',clear_event:'Clear Event',ack_event:'ACK Event'}[cmd];\n"
" if(!confirm(lbl+' - proceed?'))return;\n"
" j('/api/sv300/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:cmd})}).then(function(r){\n"
"  if(r.s===403){alert('Admin only');return}if(!r.d.ok){alert('Command failed');return}loadEvents();});}\n"
/* boot */
"var _busy=false,_tick=0;\n"
"function pollLoop(){if(_busy||window.__pollOn===false||cur!=='dash')return;_busy=true;\n"
" loadDash().then(function(){if(_tick%3===0)return loadEvents();}).catch(function(){}).then(function(){_busy=false;_tick++;});}\n"
"tbtn();clk();setInterval(clk,1000);setInterval(pollLoop,1000);me();\n"
"</script></body></html>\n";

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
 *  CycloneTCP(http_server_misc.c)는 "/"를 defaultDocument("index.html", 슬래시 없음)로
 *  치환해 콜백에 넘긴다 → 루트는 "index.html"(슬래시 없음)로도 매칭.
 *  로그인 필요 API는 세션 없으면 401.
 *----------------------------------------------------------------------------*/
static error_t webRequestCallback(HttpConnection *c, const char_t *uri)
{
	if (!strcmp(uri, "/") ||
	    !strcmp(uri, "index.html") || !strcmp(uri, "/index.html") ||
	    !strcmp(uri, "index.htm")  || !strcmp(uri, "/index.htm"))
		return serveIndex(c);

	/* 인증(무개방) */
	if (!strcmp(uri, "/api/login"))  return apiLogin(c);
	if (!strcmp(uri, "/api/logout")) return apiLogout(c);
	if (!strcmp(uri, "/api/me"))     return apiMe(c);

	/* 쓰기(admin) */
	if (!strcmp(uri, "/api/sv300/command")) return apiCommand(c);

	/* 조회(로그인 필요) */
	if (!strncmp(uri, "/api/", 5)) {
		if (webRole(c) == ROLE_NONE)
			return webJsonStatus(c, 401, "{\"ok\":false,\"error\":\"auth\"}");
		if (!strcmp(uri, "/api/dashboard"))    return apiDashboard(c);
		if (!strcmp(uri, "/api/sv300/events")) return apiEvents(c);
		return webJsonStatus(c, 404, "{\"ok\":false,\"error\":\"notfound\"}");
	}

	return ERROR_NOT_FOUND;
}

/*----------------------------------------------------------------------------
 * mDNS responder — http://sv300-<MAC하위3바이트>.local/
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
	if (e) { printf("[WEB] mDNS init failed (%d)\n", (int)e); return; }

	mac = interface->macAddr;
	snprintf(host, sizeof(host), "sv300-%02x%02x%02x", mac.b[3], mac.b[4], mac.b[5]);
	mdnsResponderSetHostname(&webMdns, host);

	e = mdnsResponderStart(&webMdns);
	if (e) { printf("[WEB] mDNS start failed (%d)\n", (int)e); return; }
	printf("[WEB] mDNS: http://%s.local/\n", host);
}

/*----------------------------------------------------------------------------
 * 서버 기동 — 계측 우선 위해 LOW 우선순위.
 *----------------------------------------------------------------------------*/
void webServerStart(NetInterface *interface)
{
	HttpServerSettings s;
	error_t e;
	uint_t i;

	webRandSeed(interface);

	httpServerGetDefaultSettings(&s);
	s.interface      = interface;
	s.port           = 80;
	s.maxConnections = HTTP_SERVER_MAX_CONNECTIONS;
	s.connections    = webConns;
	strcpy(s.rootDirectory, "/");
	strcpy(s.defaultDocument, "index.html");
	s.requestCallback = webRequestCallback;

	s.listenerTask.priority = OS_TASK_PRIORITY_LOW;
	for (i = 0; i < HTTP_SERVER_MAX_CONNECTIONS; i++)
		s.connectionTask[i].priority = OS_TASK_PRIORITY_LOW;

	e = httpServerInit(&webCtx, &s);
	if (e) { printf("[WEB] init failed (%d)\n", (int)e); return; }
	e = httpServerStart(&webCtx);
	if (e) { printf("[WEB] start failed (%d)\n", (int)e); return; }
	printf("[WEB] HTTP on :80 (login: ntek/0300, viewer/0000)\n");

	webMdnsStart(interface);
}
