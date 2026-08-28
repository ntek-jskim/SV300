/*----------------------------------------------------------------------------
 * SV300 Web — CycloneTCP HTTP 서버 (confWebApp UI 이식)
 *  참조: d:\PROJECT\confWebApp (Python FastAPI Modbus 웹앱) — 화면/기능 원본.
 *  방식: 다중 페이지(Jinja) → 단일 임베디드 SPA(로그인 + Dashboard + Setup).
 *  구성:
 *   - 인증: 폼 로그인(admin ntek/0300, viewer sv300/0000) + 쿠키 세션.
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
extern int readMemCb(uint16_t address, uint16_t *value);

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
	/* 폴링 엔드포인트는 keep-alive 유지(커넥션 재사용 → TIME_WAIT 소켓 처닝 방지).
	 * idle 점유는 HTTP_SERVER_TIMEOUT(3s)로 완화. */
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
	c->response.keepAlive       = FALSE;
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
	else if (!strcmp(user, "sv300") && !strcmp(pass, "0000")) role = ROLE_VIEWER;

	if (role == ROLE_NONE)
		return webJsonStatus(c, 401, "{\"ok\":false,\"error\":\"invalid\"}");

	s = sessNew(role);

	httpInitResponseHeader(c);
	c->response.contentType     = "application/json";
	c->response.chunkedEncoding = TRUE;
	c->response.noCache         = TRUE;
	c->response.keepAlive       = FALSE;
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
	c->response.keepAlive       = FALSE;
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
		int nch = getHwCh();	/* HWMODEL 채널 수(2/3) — 배열 크기(METER_CH_COUNT) 초과 방지 캡 */
		if (nch > METER_CH_COUNT) nch = METER_CH_COUNT;
		snprintf(b, sizeof(b),
			"{\"ok\":true,\"auth\":true,\"user\":\"%s\",\"role\":\"%s\",\"nch\":%d}",
			r == ROLE_ADMIN ? "ntek" : "sv300", roleName(r), nch);
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
		"\"p\":%.2f,\"q\":%.2f,\"s\":%.2f,\"pf\":%.2f,"
		"\"u_unbal\":%.1f,\"i_unbal\":%.1f,"
		"\"thd_u\":%.1f,\"thd_i\":%.1f,\"tdd_i\":%.1f,",
		m->P[3] / 1000.0f, m->Q[3] / 1000.0f, m->S[3] / 1000.0f,
		(float)fabs(m->PF[3]),	/* PF는 이미 %(fabs(P/S*100)) — 중복 ×100 제거(9977→99.77) */
		m->Ubal[0], m->Ibal[0],
		favg3(m->THD_U), favg3(m->THD_I), favg3(m->TDD_I));
	w(c, b);
	{
		IOM_DATA *io = &meter[0].iom;   /* IOM DATA 7050~7069: DI status/pulse/TEMP */
		IO_CFG   *cf = &meter[0].setting.iom;   /* dt=diType(0=DI,1=PI), db=debounce, sc=piConst */
		snprintf(b, sizeof(b),
			"\"iom\":{\"di\":[%u,%u,%u,%u],\"pi\":[%u,%u,%u,%u],\"temp\":[%.1f,%.1f,%.1f,%.1f],",
			(unsigned)io->diStatus[0], (unsigned)io->diStatus[1], (unsigned)io->diStatus[2], (unsigned)io->diStatus[3],
			(unsigned)io->piData[0], (unsigned)io->piData[1], (unsigned)io->piData[2], (unsigned)io->piData[3],
			io->aiData[0], io->aiData[1], io->aiData[2], io->aiData[3]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"dt\":[%u,%u,%u,%u],\"db\":[%u,%u,%u,%u],\"sc\":[%u,%u,%u,%u]}}}",
			(unsigned)cf->diType[0], (unsigned)cf->diType[1], (unsigned)cf->diType[2], (unsigned)cf->diType[3],
			(unsigned)cf->debounce[0], (unsigned)cf->debounce[1], (unsigned)cf->debounce[2], (unsigned)cf->debounce[3],
			(unsigned)cf->piConst[0], (unsigned)cf->piConst[1], (unsigned)cf->piConst[2], (unsigned)cf->piConst[3]);
		w(c, b);
	}
	return httpCloseStream(c);
}

/* HWMODEL 채널 수(측정 루프 바운드) — getHwCh(2/3)를 배열 크기(METER_CH_COUNT)로 캡 */
static int webNch(void) { int n = getHwCh(); return (n > METER_CH_COUNT) ? METER_CH_COUNT : n; }

/* GET /api/feeders — 채널(피더)별 3상 계측 + 전력·에너지 */
static error_t apiFeeders(HttpConnection *c)
{
	char b[256];
	int i, first = 1;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"feeders\":[");
	for (i = 0; i < webNch(); i++) {
		METERING *m = &meter[i].meter;
		double kwh  = (double)meter[i].egy.Ereg64.cell[0][0][0] / 1000.0;   /* TOTAL,KWH,IMPORT (Wh→kWh) */
		double kwhM = (double)meter[i].egy.Ereg32[1].cell[0][0][0] * 0.1;   /* THIS_MONTH (0.1kWh) */
		unsigned wiring = (unsigned)meter[0].setting.pt[i].wiring;

		if (wiring == NOT_USED) continue;	/* feeder_cnt 반영: 미사용(NOT_USED) 채널 제외 */

		snprintf(b, sizeof(b),
			"%s{\"n\":%d,\"wiring\":%u,\"freq\":%.2f,"
			"\"u\":[%.1f,%.1f,%.1f],\"upp\":[%.1f,%.1f,%.1f],"
			"\"i\":[%.2f,%.2f,%.2f],\"in\":%.2f,",
			first ? "" : ",", i + 1, wiring, m->Freq,
			m->U[0], m->U[1], m->U[2], m->Upp[0], m->Upp[1], m->Upp[2],
			m->I[0], m->I[1], m->I[2], m->In);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"thdu\":[%.1f,%.1f,%.1f],\"thdi\":[%.1f,%.1f,%.1f],\"tddi\":[%.1f,%.1f,%.1f],"
			"\"pf\":%.3f,\"p\":%.3f,\"q\":%.3f,\"s\":%.3f,\"uunb\":%.1f,\"iunb\":%.1f,"
			"\"kwh\":%.2f,\"kwh_m\":%.2f,\"st\":%u}",
			m->THD_U[0], m->THD_U[1], m->THD_U[2], m->THD_I[0], m->THD_I[1], m->THD_I[2],
			m->TDD_I[0], m->TDD_I[1], m->TDD_I[2],
			m->PF[3], m->P[3] / 1000.0f, m->Q[3] / 1000.0f, m->S[3] / 1000.0f,
			m->Ubal[0], m->Ibal[0], kwh, kwhM, (unsigned)(m->meterStatus ? 1 : 0));
		w(c, b);
		first = 0;
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/channels — 채널별 전체 계측(Meter) + 위상각(Phase) */
static error_t apiChannels(HttpConnection *c)
{
	char b[256];
	int i;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		METERING *m = &meter[i].meter;
		CNTL_DATA *cn = &meter[i].cntl;
		snprintf(b, sizeof(b),
			"%s{\"n\":%d,\"wiring\":%u,\"st\":%u,\"freq\":%.2f,\"temp\":%.1f,"
			"\"u\":[%.1f,%.1f,%.1f,%.1f],\"upp\":[%.1f,%.1f,%.1f,%.1f],",
			i ? "," : "", i + 1, (unsigned)meter[0].setting.pt[i].wiring,
			(unsigned)(m->meterStatus ? 1 : 0), m->Freq, m->Temp,
			m->U[0], m->U[1], m->U[2], m->U[3], m->Upp[0], m->Upp[1], m->Upp[2], m->Upp[3]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"uu\":%.2f,\"uo\":%.2f,\"uzs\":[%.2f,%.1f],\"ups\":[%.2f,%.1f],\"uns\":[%.2f,%.1f],"
			"\"uang\":[%.1f,%.1f,%.1f],",
			m->Ubal[0], m->Ubal[1], m->Uzs[0], m->Uzs[1], m->Ups[0], m->Ups[1], m->Uns[0], m->Uns[1],
			m->Uangle[0], m->Uangle[1], m->Uangle[2]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"uundev\":[%.1f,%.1f,%.1f],\"uovdev\":[%.1f,%.1f,%.1f],"
			"\"i\":[%.2f,%.2f,%.2f,%.2f],\"itot\":%.2f,\"in\":%.2f,\"isum\":%.2f,\"ig\":%.2f,",
			m->UUndev[0], m->UUndev[1], m->UUndev[2], m->UOvdev[0], m->UOvdev[1], m->UOvdev[2],
			m->I[0], m->I[1], m->I[2], m->I[3], m->Itot, m->In, m->Isum, m->Ig);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"izs\":[%.2f,%.1f],\"ips\":[%.2f,%.1f],\"ins\":[%.2f,%.1f],"
			"\"iang\":[%.1f,%.1f,%.1f],\"iu\":%.2f,\"io\":%.2f,",
			m->Izs[0], m->Izs[1], m->Ips[0], m->Ips[1], m->Ins[0], m->Ins[1],
			m->Iangle[0], m->Iangle[1], m->Iangle[2], m->Ibal[0], m->Ibal[1]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"p\":[%.2f,%.2f,%.2f,%.2f],\"q\":[%.2f,%.2f,%.2f,%.2f],\"s\":[%.2f,%.2f,%.2f,%.2f],",
			m->P[0]/1000.0f, m->P[1]/1000.0f, m->P[2]/1000.0f, m->P[3]/1000.0f,
			m->Q[0]/1000.0f, m->Q[1]/1000.0f, m->Q[2]/1000.0f, m->Q[3]/1000.0f,
			m->S[0]/1000.0f, m->S[1]/1000.0f, m->S[2]/1000.0f, m->S[3]/1000.0f);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"fp\":[%.2f,%.2f,%.2f,%.2f],\"fq\":[%.2f,%.2f,%.2f,%.2f],\"fs\":[%.2f,%.2f,%.2f,%.2f],",
			m->fP[0]/1000.0f, m->fP[1]/1000.0f, m->fP[2]/1000.0f, m->fP[3]/1000.0f,
			m->fQ[0]/1000.0f, m->fQ[1]/1000.0f, m->fQ[2]/1000.0f, m->fQ[3]/1000.0f,
			m->fS[0]/1000.0f, m->fS[1]/1000.0f, m->fS[2]/1000.0f, m->fS[3]/1000.0f);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"pf\":[%.3f,%.3f,%.3f,%.3f],\"dpf\":[%.3f,%.3f,%.3f,%.3f],"
			"\"d\":[%.2f,%.2f,%.2f,%.2f],\"pang\":[%.1f,%.1f,%.1f,%.1f],",
			m->PF[0], m->PF[1], m->PF[2], m->PF[3], m->dPF[0], m->dPF[1], m->dPF[2], m->dPF[3],
			m->D[0]/1000.0f, m->D[1]/1000.0f, m->D[2]/1000.0f, m->D[3]/1000.0f,
			m->Pangle[0], m->Pangle[1], m->Pangle[2], m->Pangle[3]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"thdu\":[%.2f,%.2f,%.2f],\"thdupp\":[%.2f,%.2f,%.2f],"
			"\"cfu\":[%.2f,%.2f,%.2f],\"cfupp\":[%.2f,%.2f,%.2f],",
			m->THD_U[0], m->THD_U[1], m->THD_U[2], m->THD_Upp[0], m->THD_Upp[1], m->THD_Upp[2],
			m->CF_U[0], m->CF_U[1], m->CF_U[2], m->CF_Upp[0], m->CF_Upp[1], m->CF_Upp[2]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"thdi\":[%.2f,%.2f,%.2f],\"tddi\":[%.2f,%.2f,%.2f],"
			"\"kfi\":[%.2f,%.2f,%.2f],\"cfi\":[%.2f,%.2f,%.2f],",
			m->THD_I[0], m->THD_I[1], m->THD_I[2], m->TDD_I[0], m->TDD_I[1], m->TDD_I[2],
			m->KF_I[0], m->KF_I[1], m->KF_I[2], m->CF_I[0], m->CF_I[1], m->CF_I[2]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"fu\":[%.1f,%.1f,%.1f,%.1f],\"fi\":[%.2f,%.2f,%.2f,%.2f],"
			"\"uiang\":[%.1f,%.1f,%.1f]}",
			m->fU[0], m->fU[1], m->fU[2], m->fU[3], m->fI[0], m->fI[1], m->fI[2], m->fI[3],
			cn->UIangle[0], cn->UIangle[1], cn->UIangle[2]);
		w(c, b);
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/minmax — 채널별 Max/Min(+타임스탬프) */
static void mmEmit(HttpConnection *c, const char *nm, MAXMIN_DATA *d, int first)
{
	char b[128];
	snprintf(b, sizeof(b), "%s{\"nm\":\"%s\",\"mx\":%.2f,\"mxt\":%u,\"mn\":%.2f,\"mnt\":%u}",
		first ? "" : ",", nm, d->max, (unsigned)d->max_ts, d->min, (unsigned)d->min_ts);
	w(c, b);
}

static error_t apiMinmax(HttpConnection *c)
{
	char b[64];
	int i;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		MAXMIN *mm = &meter[i].maxmin;
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"reset_ts\":%u,\"m\":[",
			i ? "," : "", i + 1, (unsigned)mm->rstTime);
		w(c, b);
		mmEmit(c, "Frequency", &mm->Freq, 1);  mmEmit(c, "Temp", &mm->Temp, 0);
		mmEmit(c, "U1", &mm->U[0], 0); mmEmit(c, "U2", &mm->U[1], 0); mmEmit(c, "U3", &mm->U[2], 0); mmEmit(c, "U~", &mm->U[3], 0);
		mmEmit(c, "U12", &mm->Upp[0], 0); mmEmit(c, "U23", &mm->Upp[1], 0); mmEmit(c, "U31", &mm->Upp[2], 0); mmEmit(c, "Upp~", &mm->Upp[3], 0);
		mmEmit(c, "I1", &mm->I[0], 0); mmEmit(c, "I2", &mm->I[1], 0); mmEmit(c, "I3", &mm->I[2], 0); mmEmit(c, "I~", &mm->I[3], 0);
		mmEmit(c, "I total", &mm->Itot, 0); mmEmit(c, "In", &mm->In, 0); mmEmit(c, "Isum", &mm->Isum, 0);
		mmEmit(c, "P1", &mm->P[0], 0); mmEmit(c, "P2", &mm->P[1], 0); mmEmit(c, "P3", &mm->P[2], 0); mmEmit(c, "P total", &mm->P[3], 0);
		mmEmit(c, "Q1", &mm->Q[0], 0); mmEmit(c, "Q2", &mm->Q[1], 0); mmEmit(c, "Q3", &mm->Q[2], 0); mmEmit(c, "Q total", &mm->Q[3], 0);
		mmEmit(c, "S1", &mm->S[0], 0); mmEmit(c, "S2", &mm->S[1], 0); mmEmit(c, "S3", &mm->S[2], 0); mmEmit(c, "S total", &mm->S[3], 0);
		mmEmit(c, "PF L1", &mm->PF[0], 0); mmEmit(c, "PF L2", &mm->PF[1], 0); mmEmit(c, "PF L3", &mm->PF[2], 0); mmEmit(c, "PF total", &mm->PF[3], 0);
		mmEmit(c, "THD U1", &mm->THD_U[0], 0); mmEmit(c, "THD U2", &mm->THD_U[1], 0); mmEmit(c, "THD U3", &mm->THD_U[2], 0);
		mmEmit(c, "THD U12", &mm->THD_Upp[0], 0); mmEmit(c, "THD U23", &mm->THD_Upp[1], 0); mmEmit(c, "THD U31", &mm->THD_Upp[2], 0);
		mmEmit(c, "THD I1", &mm->THD_I[0], 0); mmEmit(c, "THD I2", &mm->THD_I[1], 0); mmEmit(c, "THD I3", &mm->THD_I[2], 0);
		w(c, "]}");
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* 2워드(리틀워드) → float 재해석 */
static float w2f(uint16_t lo, uint16_t hi)
{
	union { uint32_t u; float f; } x;
	x.u = (uint32_t)lo | ((uint32_t)hi << 16);
	return x.f;
}

/* GET /api/energynow — 현재/당월/전월 × Total/L1~3 × 5종 에너지 매트릭스(0.1kWh) */
static const char *E_PNAME[3] = { "Current", "This Month", "Last Month" };
static const char *E_GNAME[4] = { "Total", "L1", "L2", "L3" };

static error_t apiEnergyNow(HttpConnection *c)
{
	char b[224];
	int i, pi, gi;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		ENERGY *e = &meter[i].egy;
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"periods\":[", i ? "," : "", i + 1);
		w(c, b);
		for (pi = 0; pi < 3; pi++) {
			ENERGY_REG32 *R = &e->Ereg32[pi];
			snprintf(b, sizeof(b), "%s{\"name\":\"%s\",\"groups\":[", pi ? "," : "", E_PNAME[pi]);
			w(c, b);
			for (gi = 0; gi < 4; gi++) {
				snprintf(b, sizeof(b),
					"%s{\"name\":\"%s\",\"import_kwh\":%.1f,\"export_kwh\":%.1f,"
					"\"import_kvarh\":%.1f,\"export_kvarh\":%.1f,\"kvah\":%.1f}",
					gi ? "," : "", E_GNAME[gi],
					R->cell[gi][0][0] * 0.1, R->cell[gi][0][1] * 0.1,
					R->cell[gi][1][0] * 0.1, R->cell[gi][1][1] * 0.1,
					R->cell[gi][2][0] * 0.1);
				w(c, b);
			}
			w(c, "]}");
		}
		w(c, "]}");
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/energylog — 채널별 시간별(24h) 당일/전일 kWh (elog 블록) */
static error_t apiEnergyLog(HttpConnection *c)
{
	char b[64];
	int i, k;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		uint16_t *E = (uint16_t *)&meter[i].elog[0];   /* elog[0]=전일, elog[1]=당일 */
		uint32_t lts = (uint32_t)E[0]   | ((uint32_t)E[1]   << 16);
		uint32_t tts = (uint32_t)E[200] | ((uint32_t)E[201] << 16);
		double lt = 0, tt = 0;

		snprintf(b, sizeof(b), "%s{\"n\":%d,\"last_ts\":%u,\"this_ts\":%u,\"last\":[",
			i ? "," : "", i + 1, (unsigned)lts, (unsigned)tts);
		w(c, b);
		for (k = 0; k < 24; k++) { float v = w2f(E[2 + 8 * k], E[3 + 8 * k]); lt += v; snprintf(b, sizeof(b), "%s%.2f", k ? "," : "", v); w(c, b); }
		w(c, "],\"this\":[");
		for (k = 0; k < 24; k++) { float v = w2f(E[202 + 8 * k], E[203 + 8 * k]); tt += v; snprintf(b, sizeof(b), "%s%.2f", k ? "," : "", v); w(c, b); }
		snprintf(b, sizeof(b), "],\"last_total\":%.1f,\"this_total\":%.1f}", lt, tt);
		w(c, b);
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/demandlog — 채널별 96점(15분) P 수요 프로파일(kW) */
static error_t apiDemandLog(HttpConnection *c)
{
	char b[64];
	int i, k;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		DEMAND *d = &meter[i].dm;
		double sum = 0, pk = 0;
		int pki = 0;
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"ts\":%u,\"vals\":[",
			i ? "," : "", i + 1, (unsigned)d->dmdLogTs);
		w(c, b);
		for (k = 0; k < 96; k++) {
			float v = d->DP_P_Log[k] / 1000.0f;
			if (v > pk) { pk = v; pki = k; }
			sum += v;
			snprintf(b, sizeof(b), "%s%.2f", k ? "," : "", v);
			w(c, b);
		}
		snprintf(b, sizeof(b), "],\"peak\":%.2f,\"peaki\":%d,\"avg\":%.2f}", pk, pki, sum / 96.0);
		w(c, b);
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/harmonics?opt=pv|lv|cur — 채널별 고조파(차수 2~63, %=raw/10) */
static error_t apiHarmonics(HttpConnection *c)
{
	char b[48];
	int i, ph, o, sel = 0;   /* 0=상전압(U), 1=선간(Upp), 2=전류(I) */
	const char *q = c->request.queryString;

	if (q) { if (strstr(q, "opt=lv")) sel = 1; else if (strstr(q, "opt=cur")) sel = 2; }

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b), "{\"ok\":true,\"opt\":%d,\"orders\":[", sel);
	w(c, b);
	for (o = 2; o <= 63; o++) { snprintf(b, sizeof(b), "%s%d", o > 2 ? "," : "", o); w(c, b); }
	w(c, "],\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		HARMONICS *hd = &meter[i].hd;
		uint16_t (*arr)[64] = (sel == 1) ? hd->Upp : (sel == 2) ? hd->I : hd->U;
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"ph\":[", i ? "," : "", i + 1);
		w(c, b);
		for (ph = 0; ph < 3; ph++) {
			w(c, ph ? ",[" : "[");
			for (o = 2; o <= 63; o++) { snprintf(b, sizeof(b), "%s%.1f", o > 2 ? "," : "", arr[ph][o] / 100.0f); w(c, b); }	/* hd=비율×10000=%×100 → /100=% (기존 /10은 10×오류) */
			w(c, "]");
		}
		w(c, "]}");
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/vq — 전압품질: 전압편차(Under/Over Deviation·Value) CH1만(전압 M0~M2 공유).
 *  Flicker(Pi/Pst/Plt)는 미구현이라 미포함. meter[0].vq(=Quality.c에서 채움). */
static error_t apiVq(HttpConnection *c)
{
	char b[48];
	int ph;
	VQDATA *vq = &meter[0].vq;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"rows\":[");
	w(c, "{\"name\":\"U Under Deviation\",\"unit\":\"%\",\"l\":[");
	for (ph = 0; ph < 3; ph++) { snprintf(b, sizeof(b), "%s%.3f", ph ? "," : "", vq->Uud[ph]); w(c, b); }
	w(c, "]},{\"name\":\"U Under Value\",\"unit\":\"V\",\"l\":[");
	for (ph = 0; ph < 3; ph++) { snprintf(b, sizeof(b), "%s%.3f", ph ? "," : "", vq->Uuv[ph]); w(c, b); }
	w(c, "]},{\"name\":\"U Over Deviation\",\"unit\":\"%\",\"l\":[");
	for (ph = 0; ph < 3; ph++) { snprintf(b, sizeof(b), "%s%.3f", ph ? "," : "", vq->Uod[ph]); w(c, b); }
	w(c, "]},{\"name\":\"U Over Value\",\"unit\":\"V\",\"l\":[");
	for (ph = 0; ph < 3; ph++) { snprintf(b, sizeof(b), "%s%.3f", ph ? "," : "", vq->Uov[ph]); w(c, b); }
	w(c, "]}]}");
	return httpCloseStream(c);
}

/* GET /api/waveform — 채널별 V/I 파형(상별 160샘플). wv는 read-trigger라 갱신 후 직렬화 */
static error_t apiWaveform(HttpConnection *c)
{
	char b[48];
	int i, ph, k;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"channels\":[");
	for (i = 0; i < webNch(); i++) {
		WAVEFORM_L16 *wv;
		float vs, is;
		copyModbusWaveData(i);   /* 최신 파형 캡처(Modbus 읽기 경로와 동일) */
		wv = &meter[i].wv;
		vs = wv->vscale ? wv->vscale : 1.0f;
		is = wv->iscale ? wv->iscale : 1.0f;
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"v\":[", i ? "," : "", i + 1);
		w(c, b);
		for (ph = 0; ph < 3; ph++) {
			w(c, ph ? ",[" : "[");
			for (k = 0; k < 160; k++) { snprintf(b, sizeof(b), "%s%.1f", k ? "," : "", wv->U[ph][k] * vs); w(c, b); }
			w(c, "]");
		}
		w(c, "],\"i\":[");
		for (ph = 0; ph < 3; ph++) {
			w(c, ph ? ",[" : "[");
			for (k = 0; k < 160; k++) { snprintf(b, sizeof(b), "%s%.2f", k ? "," : "", wv->I[ph][k] * is); w(c, b); }
			w(c, "]");
		}
		w(c, "]}");
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/en50160 — 채널별 EN50160 준수(rpt[0]=금주). 값=compliance%(=raw/100), p=준수여부 */
static void enEmit(HttpConnection *c, int first, int pass, float v0, float v1, float v2, int nvals)
{
	char b[96], s[48];
	if (nvals == 1) snprintf(s, sizeof(s), "%.2f,null,null", v0);
	else            snprintf(s, sizeof(s), "%.2f,%.2f,%.2f", v0, v1, v2);
	snprintf(b, sizeof(b), "%s{\"v\":[%s],\"p\":%d}", first ? "" : ",", s, pass);
	w(c, b);
}

/* ITIC 목록(itic=전압 sag/swell/intr, itic2=과도) 현재 페이지 직렬화 */
static void iticEmit(HttpConnection *c, ITIC_EVT_LIST *L)
{
	char b[128];
	int k, m = 0, first = 1;
	for (k = 0; k < N_ITIC_LIST && m < N_ITIC_LIST; k++) {
		ITIC_LOG *e = &L->elog[k];
		float lv;
		if (e->type == 0) continue;
		lv = e->level[0];
		if (e->level[1] > lv) lv = e->level[1];
		if (e->level[2] > lv) lv = e->level[2];
		snprintf(b, sizeof(b),
			"%s{\"type\":%u,\"ts\":%u,\"dur\":%u,\"mask\":%u,\"level\":%.2f}",
			first ? "" : ",", (unsigned)e->type, (unsigned)e->startTs,
			(unsigned)e->duration, (unsigned)e->mask, lv);
		w(c, b); first = 0; m++;
	}
}

static error_t apiEn50160(HttpConnection *c)
{
	char b[128];
	int i;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		EN50160 *r = &meter[i].rpt[0];
		uint32_t cm = r->compliance;
#define ENP(ri) (((cm >> (ri)) & 1u) ? 0 : 1)   /* 비트 set = FAIL */
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"start\":%u,\"end\":%u,\"rows\":[",
			i ? "," : "", i + 1, (unsigned)r->sTime, (unsigned)r->eTime);
		w(c, b);
		enEmit(c, 1, ENP(0), r->Fvar1 / 100.0f, 0, 0, 1);
		enEmit(c, 0, ENP(1), r->Fvar2 / 100.0f, 0, 0, 1);
		enEmit(c, 0, ENP(2), r->Volt1[0] / 100.0f, r->Volt1[1] / 100.0f, r->Volt1[2] / 100.0f, 3);
		enEmit(c, 0, ENP(3), r->Volt2[0] / 100.0f, r->Volt2[1] / 100.0f, r->Volt2[2] / 100.0f, 3);
		enEmit(c, 0, ENP(4), r->Voltbal / 100.0f, 0, 0, 1);
		enEmit(c, 0, ENP(5), r->VoltThd[0] / 100.0f, r->VoltThd[1] / 100.0f, r->VoltThd[2] / 100.0f, 3);
		enEmit(c, 0, ENP(6), r->VoltHd[0] / 100.0f, r->VoltHd[1] / 100.0f, r->VoltHd[2] / 100.0f, 3);
		enEmit(c, 0, ENP(7), r->Plt[0] / 100.0f, r->Plt[1] / 100.0f, r->Plt[2] / 100.0f, 3);
		snprintf(b, sizeof(b), "],\"info\":[%u,%u,%u,%.1f]}",
			(unsigned)(r->sag[0] + r->sag[1] + r->sag[2] + r->sag[3]),
			(unsigned)(r->swell[0] + r->swell[1] + r->swell[2] + r->swell[3]),
			(unsigned)(r->shortIntr[0] + r->shortIntr[1] + r->shortIntr[2] + r->shortIntr[3]),
			r->Svolt[0] / 100.0f);
		w(c, b);
#undef ENP
	}
	/* CH1 ITIC 목록 — 웹은 itic2 창 사용(외부서버는 itic 창을 Modbus로 직접 사용) */
	w(c, "],\"itic2\":[");
	iticEmit(c, &meter[0].itic2);
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/monthly — 채널별 10분평균(log): 스칼라 + 고조파(9계열×24차, %=raw/100) */
static void hmEmit(HttpConnection *c, const char *label, uint16_t *a, int first)
{
	char b[48];
	int k;
	snprintf(b, sizeof(b), "%s{\"label\":\"%s\",\"vals\":[", first ? "" : ",", label);
	w(c, b);
	for (k = 0; k < 24; k++) { snprintf(b, sizeof(b), "%s%.1f", k ? "," : "", a[k] / 100.0f); w(c, b); }
	w(c, "]}");
}

static error_t apiMonthly(HttpConnection *c)
{
	char b[224];
	int i;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"ch\":[");
	for (i = 0; i < webNch(); i++) {
		LOG_DATA *L = &meter[i].log;
		snprintf(b, sizeof(b), "%s{\"n\":%d,\"ts\":%u,\"s\":{", i ? "," : "", i + 1, (unsigned)L->ts);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"U1\":%.2f,\"U2\":%.2f,\"U3\":%.2f,\"U THD1\":%.2f,\"U THD2\":%.2f,\"U THD3\":%.2f,\"U Unbal\":%.2f,",
			L->U[0], L->U[1], L->U[2], L->Uthd[0], L->Uthd[1], L->Uthd[2], L->Ubal[0]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"I1\":%.2f,\"I2\":%.2f,\"I3\":%.2f,\"In\":%.2f,\"I THD1\":%.2f,\"I THD2\":%.2f,\"I THD3\":%.2f,\"I TDD1\":%.2f,\"I TDD2\":%.2f,\"I TDD3\":%.2f,\"I Unbal\":%.2f,",
			L->I[0], L->I[1], L->I[2], L->In, L->Ithd[0], L->Ithd[1], L->Ithd[2], L->Itdd[0], L->Itdd[1], L->Itdd[2], L->Ibal[0]);
		w(c, b);
		snprintf(b, sizeof(b),
			"\"P\":%.2f,\"Q\":%.2f,\"S\":%.2f,\"PF\":%.3f,\"K-factor1\":%.2f,\"temperature\":%.1f},\"harm\":[",
			L->kw[0], L->kvar[0], L->kVA, L->PF, L->kf[0], L->temp[0]);
		w(c, b);
		hmEmit(c, "V1",  L->Uhd[0],   1); hmEmit(c, "V2",  L->Uhd[1],   0); hmEmit(c, "V3",  L->Uhd[2],   0);
		hmEmit(c, "V12", L->Upphd[0], 0); hmEmit(c, "V23", L->Upphd[1], 0); hmEmit(c, "V31", L->Upphd[2], 0);
		hmEmit(c, "I1",  L->Ihd[0],   0); hmEmit(c, "I2",  L->Ihd[1],   0); hmEmit(c, "I3",  L->Ihd[2],   0);
		w(c, "]}");
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/sv300/events — 3CH 알람상태/알람로그/이벤트로그/요약 */
#define EV_CAP  32   /* 각 목록 응답 상한 = 페이지 크기(N_ALARM_LIST/N_EVENT_LIST) */

static error_t apiEvents(HttpConnection *c)
{
	char b[256];
	int i, k, m, first;

	if (beginJson(c)) return ERROR_WRITE_FAILED;

	/* alarm status: st[32] 중 status!=0 (활성 알람). 상한은 채널별(EV_CAP) — 전역 합산 아님 */
	w(c, "{\"ok\":true,\"status\":[");
	first = 1;
	for (i = 0; i < webNch(); i++) {
		ALARM_STATUS *a = &meter[i].alarm;
		m = 0;
		for (k = 0; k < 32 && m < EV_CAP; k++) {
			if (a->st[k].status) {
				snprintf(b, sizeof(b), "%s{\"chn\":%d,\"chan\":%u,\"count\":%u}",
					first ? "" : ",", i + 1, (unsigned)a->st[k].chan, (unsigned)a->st[k].count);
				w(c, b); first = 0; m++;
			}
		}
	}

	/* alarm log: alist.alog[count] */
	w(c, "],\"alarmlog\":[");
	first = 1;
	for (i = 0; i < webNch(); i++) {
		ALARM_LIST *L = &meter[i].alist;
		int cnt = L->count; if (cnt > N_ALARM_LIST) cnt = N_ALARM_LIST;
		m = 0;
		for (k = 0; k < cnt && m < EV_CAP; k++) {
			ALARM_LOG *g = &L->alog[k];
			snprintf(b, sizeof(b),
				"%s{\"chn\":%d,\"chan\":%u,\"set\":%u,\"value\":%.2f,\"ts\":%u}",
				first ? "" : ",", i + 1, (unsigned)g->chan, (unsigned)g->status,
				g->value, (unsigned)g->ts);
			w(c, b); first = 0; m++;
		}
	}

	/* event log: elist.elog[count] */
	w(c, "],\"eventlog\":[");
	first = 1;
	for (i = 0; i < webNch(); i++) {
		EVENT_LIST *E = &meter[i].elist;
		int cnt = E->count; if (cnt > N_EVENT_LIST) cnt = N_EVENT_LIST;
		m = 0;
		for (k = 0; k < cnt && m < EV_CAP; k++) {
			EVENT_LOG *e = &E->elog[k];
			float lv = e->level[0];
			if (e->level[1] > lv) lv = e->level[1];
			if (e->level[2] > lv) lv = e->level[2];
			snprintf(b, sizeof(b),
				"%s{\"chn\":%d,\"type\":%u,\"ts\":%u,\"dur\":%u,\"mask\":%u,\"level\":%.2f}",
				first ? "" : ",", i + 1, (unsigned)e->type, (unsigned)e->startTs,
				(unsigned)e->duration, (unsigned)e->mask, lv);
			w(c, b); first = 0; m++;
		}
	}

	/* summary: PQ_EVENT_COUNT 합산(3CH) */
	{
		unsigned tv = 0, ti = 0, oc = 0, sag = 0, sw = 0, intr = 0;
		for (i = 0; i < webNch(); i++) {
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

	if      (!strcmp(cmd, "clear_alarm")) addr = 7478;   /* clear alarm ALL (260812 맵: +1 시프트) */
	else if (!strcmp(cmd, "clear_event")) addr = 7482;   /* clear event ALL */
	else if (!strcmp(cmd, "ack_alarm"))   addr = 7494;   /* alarm ack ALL */
	else if (!strcmp(cmd, "ack_event"))   addr = 7498;   /* event ack ALL */
	else return webJsonStatus(c, 400, "{\"ok\":false,\"error\":\"bad cmd\"}");

	writeMemCb(addr, 0x1234);   /* ALL 대상(offset 0) */
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true}");
	return httpCloseStream(c);
}

/* [FW UPDATE] 브라우저 업로드 청크버퍼(동시 1개 가정, 스택 절약) */
static uint8_t s_fwUpBuf[1024];

/* POST /api/fwupload — admin, 브라우저에서 선택한 .bin 원시바디를 usrApp.bin으로 FS에 스트리밍 저장 후 검증 */
static error_t apiFwUpload(HttpConnection *c)
{
	void *fh;
	size_t total, got = 0, r;
	int rc, ok;
	char buf[80];

	if (webRole(c) != ROLE_ADMIN)
		return webJsonStatus(c, 403, "{\"ok\":false,\"error\":\"admin only\"}");
	total = c->request.contentLength;
	if (total < 1024 || total > (1024UL * 1024UL))
		return webJsonStatus(c, 400, "{\"ok\":false,\"error\":\"bad size\"}");
	fh = fwBeginWrite();
	if (fh == NULL)
		return webJsonStatus(c, 500, "{\"ok\":false,\"error\":\"fs open failed\"}");
	while (got < total) {
		size_t want = total - got;
		if (want > sizeof(s_fwUpBuf)) want = sizeof(s_fwUpBuf);
		if (httpReadStream(c, s_fwUpBuf, want, &r, 0)) break;
		if (r == 0) break;
		if (fwWrite(fh, s_fwUpBuf, (int)r) != (int)r) break;
		got += r;
	}
	ok = (got == total);
	fwEndWrite(fh, ok);			/* ok=0이면 부분파일 삭제 */
	if (!ok)
		return webJsonStatus(c, 400, "{\"ok\":false,\"error\":\"upload failed\"}");
	rc = fwCheckUsrApp(NULL);	/* 저장 후 서명/크기 검증 */
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(buf, sizeof(buf), "{\"ok\":true,\"size\":%u,\"valid\":%s,\"err\":%d}",
	         (unsigned)got, (rc == 0) ? "true" : "false", rc);
	w(c, buf);
	return httpCloseStream(c);
}

/* GET /api/fwstatus — 업로드된 usrApp.bin 검증 상태 */
static error_t apiFwStatus(HttpConnection *c)
{
	char buf[96];
	uint32_t sz = 0;
	int rc;
	if (webRole(c) != ROLE_ADMIN)
		return webJsonStatus(c, 403, "{\"ok\":false,\"error\":\"admin only\"}");
	rc = fwCheckUsrApp(&sz);
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(buf, sizeof(buf), "{\"ok\":true,\"valid\":%s,\"err\":%d,\"size\":%u}",
	         (rc == 0) ? "true" : "false", rc, (unsigned)sz);
	w(c, buf);
	return httpCloseStream(c);
}

/* POST /api/fwapply — admin, usrApp.bin 검증 통과 시 리부팅(부트로더가 내부플래시에 굽기) */
static error_t apiFwApply(HttpConnection *c)
{
	char buf[80];
	int rc;
	if (webRole(c) != ROLE_ADMIN)
		return webJsonStatus(c, 403, "{\"ok\":false,\"error\":\"admin only\"}");
	rc = fwCheckUsrApp(NULL);
	if (rc != 0) {
		snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"invalid firmware\",\"err\":%d}", rc);
		return webJsonStatus(c, 400, buf);
	}
	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"rebooting\":true}");
	reqReboot(0x1234);	/* 지연 리부팅(플래그) → 응답 flush 후 부트로더가 usrApp.bin 굽기 */
	return httpCloseStream(c);
}

/*============================ Setup: 레지스터 R/W ==========================*/
/* JSON 본문에서 정수/문자열 값 추출(간이 파서) */
static unsigned long jsonNum(const char *body, const char *key)
{
	char pat[20];
	const char *p;
	snprintf(pat, sizeof(pat), "\"%s\"", key);
	p = strstr(body, pat);
	if (!p) return 0;
	p = strchr(p + strlen(pat), ':');
	if (!p) return 0;
	return strtoul(p + 1, NULL, 10);   /* u32 및 음수(2's complement wrap) 처리 */
}

/* 쿼리스트링 정수 파라미터(없으면 -1) */
static int qparam(const char *qs, const char *key)
{
	char pat[16];
	const char *p;
	snprintf(pat, sizeof(pat), "%s=", key);
	p = strstr(qs ? qs : "", pat);
	if (!p) return -1;
	return atoi(p + strlen(pat));
}

static uint16_t rdReg(uint16_t a) { uint16_t v = 0; readMemCb(a, &v); return v; }

/* 설정 필드 테이블(General 탭) — 주소는 펌웨어 ground truth(CH1 base) */
enum { WT_U16, WT_U32, WT_I16, WT_BOOL, WT_IP, WT_STR, WT_SERIAL, WT_MAC, WT_FWDATE };
static const char *wtName(uint8_t t)
{
	switch (t) {
	case WT_U32:  return "u32";
	case WT_I16:  return "i16";
	case WT_BOOL: return "bool";
	case WT_IP:   return "ip";
	case WT_STR:  case WT_SERIAL: case WT_MAC: case WT_FWDATE: return "str";  /* 읽기전용 표시 */
	default:      return "u16";
	}
}

typedef struct {
	const char *k, *label;
	uint16_t    addr;
	uint8_t     type, card, ro, nw;
} GField;

/* card: 0=Device Information, 1=Communication, 2=ETC, 3=Status */
static const GField GEN[] = {
	{ "serial",          "Serial Number",        7070, WT_SERIAL, 0, 1, 0 },
	{ "mac",             "MAC Address",          7076, WT_MAC,    0, 1, 0 },
	{ "hw_version",      "HW Version",           7086, WT_U16,    0, 1, 0 },
	{ "fw_version",      "FW Version",           7087, WT_U16,    0, 1, 0 },
	{ "fw_build",        "FW Build Date",        7088, WT_FWDATE, 0, 1, 0 },
	{ "timezone",        "Timezone",             7401, WT_I16,    0, 0, 0 },
	{ "heartbit",        "Heart Bit",            7091, WT_U16,  3, 1, 0 },
	{ "mbus_rx",         "Modbus RX",            7092, WT_U16,  3, 1, 0 },
	{ "alarm_sts",       "Alarm STS",            7093, WT_U16,  3, 1, 0 },
	{ "event_sts",       "Event STS",            7094, WT_U16,  3, 1, 0 },
	{ "rstp_sts0",       "RSTP STS #0",          7095, WT_U16,  3, 1, 0 },
	{ "rstp_sts1",       "RSTP STS #1",          7096, WT_U16,  3, 1, 0 },
	{ "sntp_sts",        "SNTP STS",             7097, WT_U16,  3, 1, 0 },
	{ "dev_sts",         "DEV STS",              7098, WT_U16,  3, 1, 0 },
	{ "net_sts",         "NET STS",              7099, WT_U16,  3, 1, 0 },
	{ "tot_sts",         "TOT STS",              7100, WT_U16,  3, 1, 0 },
	{ "device_id",       "Device ID",            7110, WT_U16,  1, 0, 0 },
	{ "tcp_port",        "TCP Port",             7111, WT_U16,  1, 0, 0 },
	{ "dhcp",            "DHCP",                 7154, WT_BOOL, 1, 0, 0 },
	{ "ip",              "IP Address",           7114, WT_IP,   1, 0, 0 },
	{ "subnet",          "Subnet Mask",          7118, WT_IP,   1, 0, 0 },
	{ "gateway",         "Gateway",              7122, WT_IP,   1, 0, 0 },
	{ "dns",             "DNS Server",           7126, WT_IP,   1, 0, 0 },
	{ "sntp",            "SNTP",                 7112, WT_BOOL, 1, 0, 0 },
	{ "sntp_ip",         "SNTP Server",          7130, WT_IP,   1, 0, 0 },
	{ "sntp_interval",   "SNTP Interval (min)",  7113, WT_U16,  1, 0, 0 },
	{ "va_type",         "VA Type",              7388, WT_U16,  2, 0, 0 },
	{ "pf_sign",         "PF Sign",              7389, WT_U16,  2, 0, 0 },
	{ "demand_interval", "Demand Interval (min)",7390, WT_U16,  2, 0, 0 },
	{ "target_demand",   "Target Demand (W)",    7392, WT_U32,  2, 0, 0 },
	{ "auto_rotation",   "Auto Rotation",        7400, WT_BOOL, 2, 0, 0 },
	{ "test_mode",       "Test Mode",            7403, WT_BOOL, 2, 0, 0 },
	{ "update_interval", "Update Interval (sec)",7404, WT_U16,  2, 0, 0 },
	{ "minmax_reset",    "Max/Min Reset",        7402, WT_U16,  2, 0, 0 },
};
#define GEN_N ((int)(sizeof(GEN) / sizeof(GEN[0])))

/* GET /api/general — 설정 필드 배열(값 포함) */
static error_t apiGeneral(HttpConnection *c)
{
	char b[200], val[72];
	int i, k;

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, "{\"ok\":true,\"fields\":[");
	for (i = 0; i < GEN_N; i++) {
		const GField *f = &GEN[i];
		if (f->type == WT_SERIAL) {
			METER_INFO *inf = &meter[0].info;
			snprintf(val, sizeof(val), "\"%02x%02x%02x%02x%02x%02x\"",
				inf->sn[0] & 0xff, inf->sn[1] & 0xff, inf->sn[2] & 0xff,
				inf->sn[3] & 0xff, inf->sn[4] & 0xff, inf->sn[5] & 0xff);
		} else if (f->type == WT_MAC) {
			METER_INFO *inf = &meter[0].info;
			snprintf(val, sizeof(val), "\"%02x:%02x:%02x:%02x:%02x:%02x\"",
				inf->mac_msb[0] & 0xff, inf->mac_msb[1] & 0xff, inf->mac[0] & 0xff,
				inf->mac[1] & 0xff, inf->mac[2] & 0xff, inf->mac[3] & 0xff);
		} else if (f->type == WT_FWDATE) {
			METER_INFO *inf = &meter[0].info;
			snprintf(val, sizeof(val), "\"%04u-%02u-%02u\"",
				(unsigned)inf->fwBuildYear, (unsigned)inf->fwBuildMon, (unsigned)inf->fwBuildDay);
		} else if (f->type == WT_IP) {
			snprintf(val, sizeof(val), "\"%u.%u.%u.%u\"",
				rdReg(f->addr), rdReg(f->addr + 1), rdReg(f->addr + 2), rdReg(f->addr + 3));
		} else if (f->type == WT_STR) {
			char s[40];
			int o = 0;
			for (k = 0; k < f->nw && o < 38; k++) {
				uint16_t wd = rdReg(f->addr + k);
				char c0 = (char)(wd & 0xff), c1 = (char)((wd >> 8) & 0xff);
				if (c0 < 0x20 || c0 > 0x7e || c0 == '"' || c0 == '\\') { if (c0 == 0) break; c0 = ' '; }
				s[o++] = c0;
				if (c1 < 0x20 || c1 > 0x7e || c1 == '"' || c1 == '\\') { if (c1 == 0) break; c1 = ' '; }
				s[o++] = c1;
			}
			s[o] = 0;
			snprintf(val, sizeof(val), "\"%s\"", s);
		} else if (f->type == WT_U32) {
			uint32_t v = rdReg(f->addr) | ((uint32_t)rdReg(f->addr + 1) << 16);
			snprintf(val, sizeof(val), "%u", v);
		} else if (f->type == WT_I16) {
			snprintf(val, sizeof(val), "%d", (int)(int16_t)rdReg(f->addr));
		} else {
			snprintf(val, sizeof(val), "%u", rdReg(f->addr));
		}
		snprintf(b, sizeof(b),
			"%s{\"k\":\"%s\",\"label\":\"%s\",\"addr\":%u,\"type\":\"%s\",\"card\":%u,\"ro\":%u,\"val\":%s}",
			i ? "," : "", f->k, f->label, f->addr, wtName(f->type), f->card, f->ro, val);
		w(c, b);
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* GET /api/regs?addr=A&n=N — 원시 워드 블록(PT/CT/IO는 클라이언트에서 디코드) */
static error_t apiRegs(HttpConnection *c)
{
	char b[48];
	int addr = qparam(c->request.queryString, "addr");
	int n    = qparam(c->request.queryString, "n");
	int i;

	if (addr < 0 || n <= 0 || n > 256)
		return webJsonStatus(c, 400, "{\"ok\":false,\"error\":\"range\"}");

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	snprintf(b, sizeof(b), "{\"ok\":true,\"addr\":%d,\"words\":[", addr);
	w(c, b);
	for (i = 0; i < n; i++) {
		snprintf(b, sizeof(b), "%s%u", i ? "," : "", rdReg((uint16_t)(addr + i)));
		w(c, b);
	}
	w(c, "]}");
	return httpCloseStream(c);
}

/* POST /api/setreg  {addr,type,val} — admin, 단일/2워드 레지스터 쓰기 */
static error_t apiSetReg(HttpConnection *c)
{
	char body[128], type[8];
	long addr;
	unsigned long val;
	int r;

	if (webRole(c) != ROLE_ADMIN)
		return webJsonStatus(c, 403, "{\"ok\":false,\"error\":\"admin only\"}");

	webReadBody(c, body, sizeof(body));
	addr = (long)jsonNum(body, "addr");
	val  = jsonNum(body, "val");
	{ /* type 문자열 추출 */
		char *p = strstr(body, "\"type\"");
		int o = 0; type[0] = 0;
		if (p) p = strchr(p, ':');
		if (p) p = strchr(p, '"');
		if (p) { p++; while (*p && *p != '"' && o < (int)sizeof(type) - 1) type[o++] = *p++; type[o] = 0; }
	}
	if (addr <= 0)
		return webJsonStatus(c, 400, "{\"ok\":false,\"error\":\"addr\"}");

	r = writeMemCb((uint16_t)addr, (uint16_t)(val & 0xffff));
	if (!strcmp(type, "u32"))
		writeMemCb((uint16_t)(addr + 1), (uint16_t)((val >> 16) & 0xffff));

	if (beginJson(c)) return ERROR_WRITE_FAILED;
	w(c, r == 0 ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"write rejected\"}");
	return httpCloseStream(c);
}

/* POST /api/savecfg — admin, save settings(7449) 명령으로 설정 영속화 */
static error_t apiSaveCfg(HttpConnection *c)
{
	if (webRole(c) != ROLE_ADMIN)
		return webJsonStatus(c, 403, "{\"ok\":false,\"error\":\"admin only\"}");
	writeMemCb(7449, 0x1234);
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
".btn.warn{background:var(--warn);border-color:var(--warn);color:#1a212b;font-weight:600}\n"
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
".pgh{font-size:24px;font-weight:800;margin:4px 0 10px}\n"
".fgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:16px}\n"
".fcard{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px 18px}\n"
".fcard.off{opacity:.5}\n"
".fch{display:flex;align-items:center;gap:10px;margin-bottom:12px}\n"
".fch .ft{font-size:15px;font-weight:700}.fch .ff{margin-left:auto;font-family:'Consolas',monospace;color:var(--muted);font-size:13px}\n"
".wb{font-size:11px;font-weight:700;color:var(--accent);background:var(--panel2);border-radius:6px;padding:2px 8px}\n"
".ftbl{width:100%;border-collapse:collapse;font-size:13px;margin-bottom:12px}\n"
".ftbl th,.ftbl td{padding:5px 6px;text-align:right;border-bottom:1px solid var(--line)}\n"
".ftbl th{color:var(--muted);font-weight:600;font-size:11px}\n"
".ftbl td{font-family:'Consolas',monospace}\n"
".ftbl td.pk,.ftbl th:first-child{text-align:left;color:var(--muted);font-family:inherit}\n"
".fkv{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}\n"
".kvi{background:var(--panel2);border-radius:8px;padding:7px 10px;display:flex;flex-direction:column;gap:2px}\n"
".kvi .kk{font-size:11px;color:var(--muted)}.kvi .kvv{font-family:'Consolas',monospace;font-weight:700;font-size:14px}\n"
".kvi .kvv small{color:var(--muted);font-weight:400;font-size:10px}\n"
".chgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}\n"
"@media(max-width:1000px){.chgrid{grid-template-columns:1fr}}\n"
".chcard{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px 16px;min-width:0}\n"
".chh{font-size:14px;font-weight:700;color:var(--accent);margin-bottom:10px}\n"
".ctbl{width:100%;border-collapse:collapse;font-size:12.5px}\n"
".ctbl th,.ctbl td{padding:5px 6px;text-align:right;border-bottom:1px solid var(--line)}\n"
".ctbl th{color:var(--muted);font-weight:600;font-size:11px}\n"
".ctbl td{font-family:'Consolas',monospace}\n"
".ctbl td.rk,.ctbl th:first-child{text-align:left;color:var(--muted);font-family:inherit}\n"
".ctbl .ts{font-size:9px;color:var(--muted)}\n"
".ctbl .empty{text-align:center;color:var(--muted)}\n"
/* [METER] 데이터 유무와 무관하게 3채널 카드 높이 동일하게 — 고정 레이아웃(값 폭에 라벨이 눌려 줄바꿈→카드 커짐/리플로우 방지) */
".mtbl{table-layout:fixed}\n"
".mtbl th:first-child,.mtbl td:first-child{width:40%}\n"
".mtbl td.rk{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}\n"
".cmeta{margin-top:8px;font-size:11px;color:var(--muted);font-family:'Consolas',monospace}\n"
".hsub{font-size:11px;color:var(--muted);font-weight:700;margin:10px 0 4px}\n"
".ctbl.hsm td,.ctbl.hsm th{padding:3px 5px;font-size:11px}\n"
".hcv{width:100%;height:180px;display:block}\n"
".pgctl{display:flex;gap:6px;flex-wrap:wrap;margin:2px 0 10px}.pgctl .btn{padding:3px 9px;font-size:12px}\n"
".dmcv{width:100%;height:200px;display:block}\n"
".wcv{width:100%;height:160px;display:block;margin-bottom:6px}\n"
".itcv{width:100%;height:240px;display:block}\n"
".enb{font-size:10px;font-weight:700;padding:1px 6px;border-radius:5px}\n"
".enb.pass{color:#06281a;background:var(--ok)}.enb.fail{color:#fff;background:var(--bad)}.enb.info{color:var(--muted);background:var(--panel2)}\n"
".mcols{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-bottom:8px}\n"
".mcol .srow{padding:5px 0}.mcol .hsub{margin-top:2px}\n"
".phbox{display:flex;justify-content:center;margin-bottom:10px;position:relative}\n"
".phsvg{width:100%;max-width:240px;height:auto}\n"
".phleg{position:absolute;left:4px;top:50%;transform:translateY(-50%);display:flex;flex-direction:column;gap:7px;font-size:11px;color:#c9d3de;pointer-events:none}\n"
".phleg span{display:flex;align-items:center;gap:5px;white-space:nowrap}\n"
".phleg i{width:14px;height:3px;border-radius:2px;display:inline-block}\n"
".ph-bg{fill:none;stroke:var(--line);stroke-width:1}\n"
".ph-grid{fill:none;stroke:var(--line);stroke-width:.5;opacity:.6}\n"
".ph-mk{fill:var(--muted)}\n"
".mmtbl td.mk{color:var(--muted);font-size:10px;text-align:left}\n"
".mmtbl td.rk{border-right:1px solid var(--line)}\n"
".stub{color:var(--muted);padding:30px 4px}\n"
".gp-head{display:flex;align-items:center;gap:12px;margin:4px 0 10px}\n"
".gp-head h1{font-size:22px;font-weight:800;margin:0}\n"
".gp-badge{background:var(--panel2);color:var(--muted);font-size:12px;font-weight:600;padding:4px 12px;border-radius:8px}\n"
".gp-actions{margin-left:auto;display:flex;gap:10px;align-items:center}\n"
".gp-tabs{display:flex;gap:2px;border-bottom:1px solid var(--line);margin-bottom:16px;flex-wrap:wrap}\n"
".gp-tab{padding:10px 14px;color:var(--muted);border-bottom:2px solid transparent;cursor:pointer;font-size:14px}\n"
".gp-tab:hover{color:var(--fg)}.gp-tab.on{color:var(--accent);border-bottom-color:var(--accent);font-weight:700}\n"
".setgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:16px}\n"
"@media(max-width:1000px){.setgrid{grid-template-columns:1fr}}\n"
".setcard{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:0 18px 14px}\n"
".setcard h3{font-size:15px;font-weight:700;padding:14px 0 10px;margin:0 0 6px;border-bottom:1px solid var(--line)}\n"
".srow{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:8px 0;font-size:13px;border-bottom:1px solid var(--line)}\n"
".srow:last-child{border-bottom:none}.srow .sk{color:var(--muted)}\n"
".sgrp{font-size:11px;font-weight:800;text-transform:uppercase;letter-spacing:.6px;color:var(--accent);padding:14px 0 6px;margin-top:6px;border-top:1px solid var(--line)}\n"
".setcard h3+.sgrp{margin-top:0;border-top:none;padding-top:6px}\n"
".srow .sv{font-family:'Consolas',monospace}\n"
".srow input,.srow select{width:160px;margin:0;padding:6px 8px;font-size:13px}\n"
".srow input[type=checkbox]{width:auto}\n"
"/* PT/CT 설정 표: 행=번호, 열=필드 */\n"
".settbl-wrap{width:100%;overflow-x:auto}\n"
".settbl{width:100%;border-collapse:collapse;font-size:13px}\n"
".settbl th{background:var(--panel);color:var(--muted);font-weight:600;font-size:11px;text-align:left;padding:9px 10px;border-bottom:1px solid var(--line);white-space:nowrap}\n"
".settbl td{padding:6px 10px;border-bottom:1px solid var(--line)}\n"
".settbl td.rk{font-weight:700;white-space:nowrap}\n"
".settbl input,.settbl select{width:100%;margin:0;padding:6px 8px;font-size:13px;box-sizing:border-box}\n"
".srow .chg{border-color:var(--warn)}\n"
".rtag{font-size:10px;background:var(--panel2);color:var(--muted);padding:1px 6px;border-radius:5px;margin-left:6px}\n"
".setnote{color:var(--muted);font-size:12px;margin:12px 2px}\n"
".atbl{width:100%;border-collapse:collapse;font-size:12px}\n"
".atbl th,.atbl td{padding:4px 6px;border-bottom:1px solid var(--line);text-align:center}\n"
".atbl th{color:var(--muted);font-weight:600}\n"
".atbl tr.aoff{opacity:.45}.atbl tr.aoff:hover,.atbl tr.aoff:focus-within{opacity:1}\n"
".atbl input,.atbl select{width:100%;min-width:54px;margin:0;padding:3px 4px;font-size:12px}\n"
".atbl select.awide{min-width:120px}\n"
".atbl .chg{border-color:var(--warn)}\n"
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
"  <p class='hint'>Admin (settings): <b>ntek</b> &middot; Viewer (read-only): <b>sv300</b></p>\n"
" </div>\n"
"</div>\n"
/* ---------- app view ---------- */
"<div id='v-app' style='display:none'>\n"
" <header class='topbar'>\n"
"  <nav>\n"
"   <a id='nav-dash' class='on' onclick=\"go('dash')\">Dashboard</a>\n"
"   <a id='nav-feeder' onclick=\"go('feeder')\">Feeder</a>\n"
"   <a id='nav-channel' onclick=\"go('channel')\">Channel</a>\n"
"   <a id='nav-io' onclick=\"go('io')\">IO</a>\n"
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
"   <a class='sitem' id='si-feeder' onclick=\"go('feeder')\"><span class='ico'>&#128268;</span>Feeder</a>\n"
"   <a class='sitem' id='si-channel' onclick=\"go('channel')\"><span class='ico'>&#128225;</span>Channel</a>\n"
"   <a class='sitem' id='si-io' onclick=\"go('io')\"><span class='ico'>&#128268;</span>IO (DI/TEMP)</a>\n"
"   <div class='sgroup'>Setup</div>\n"
"   <a class='sitem' id='si-setup' onclick=\"go('setup')\"><span class='ico'>&#128421;</span>Main Setting</a>\n"
"   <a class='sitem' id='si-chset' onclick=\"go('setup','chan')\"><span class='ico'>&#9881;</span>Channel Setting</a>\n"
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
"   <div id='p-feeder' style='display:none'>\n"
"    <h1 class='pgh'>Feeder</h1>\n"
"    <div class='dstatus'><span id='fstat' class='on'>&#9679; live &#8635; 1s</span></div>\n"
"    <div id='fgrid' class='fgrid'></div>\n"
"   </div>\n"
"   <div id='p-channel' style='display:none'>\n"
"    <div class='gp-head'><h1>Channel</h1><span class='gp-actions'><span id='chstat' class='ph2 on'>&#9679; live &#8635; 1s</span></span></div>\n"
"    <div class='gp-tabs'>\n"
"     <a class='gp-tab on' id='ct-meter' onclick=\"chSetTab('meter')\">Meter</a>\n"
"     <a class='gp-tab' id='ct-phase' onclick=\"chSetTab('phase')\">Phase</a>\n"
"     <a class='gp-tab' id='ct-minmax' onclick=\"chSetTab('minmax')\">Min/Max</a>\n"
"     <a class='gp-tab' id='ct-harmonics' onclick=\"chSetTab('harmonics')\">Harmonics</a>\n"
"     <a class='gp-tab' id='ct-waveform' onclick=\"chSetTab('waveform')\">Waveform</a>\n"
"     <a class='gp-tab' id='ct-report' onclick=\"chSetTab('report')\">Report</a>\n"
"     <a class='gp-tab' id='ct-vq' onclick=\"chSetTab('vq')\">VQ</a>\n"
"     <a class='gp-tab' id='ct-monthly' onclick=\"chSetTab('monthly')\">Monthly</a>\n"
"     <a class='gp-tab' id='ct-energy' onclick=\"chSetTab('energy')\">Energy</a>\n"
"     <a class='gp-tab' id='ct-demand' onclick=\"chSetTab('demand')\">Demand</a>\n"
"     <a class='gp-tab' id='ct-alarm' onclick=\"chSetTab('alarm')\">Alarm</a>\n"
"     <a class='gp-tab' id='ct-event' onclick=\"chSetTab('event')\">Event</a>\n"
"    </div>\n"
"    <div id='chctl' style='display:none;margin:0 0 10px;align-items:center;gap:8px'>\n"
"     <span class='ph2'>Option</span>\n"
"     <select id='hopt' style='width:150px' onchange=\"harmOpt=this.value;pollLoop()\"><option value='pv'>Phase Voltage</option><option value='lv'>Line Voltage</option><option value='cur'>Current</option></select>\n"
"     <span class='ph2' style='margin-left:6px'>Voltage: CH1 (M0~M2 shared) &middot; Current: CH1~3</span>\n"
"    </div>\n"
"    <div id='chbody'></div>\n"
"   </div>\n"
"   <div id='p-io' style='display:none'>\n"
"    <h1 class='pgh'>IO (DI / TEMP)</h1>\n"
"    <div class='dcard'><div class='dct'>IO DATA <span class='ph2'>7050~7069</span></div><div id='d-iom'></div></div>\n"
"   </div>\n"
"   <div id='p-setup' style='display:none'>\n"
"    <div class='gp-head'><h1>Settings</h1><span class='gp-badge' id='setBadge'>Main Setting</span>\n"
"     <span class='gp-actions'><button class='btn primary' id='setSave' onclick='saveSet()'>Save &amp; Apply</button><span class='ph2' id='setRes'></span></span></div>\n"
"    <div class='gp-tabs' id='setTabs'></div>\n"
"    <div id='setBody'></div>\n"
"    <div class='setnote' id='setNote'></div>\n"
"   </div>\n"
"  </main>\n"
" </div>\n"
"</div>\n"
/* ---------- script ---------- */
"<script>\n"
"var $=function(i){return document.getElementById(i)};\n"
"var IS_ADMIN=false, cur='dash', NCH=3;\n"
"var AC=['Disabled','Temp','Freq','U1','U2','U3','U~','U12','U23','U31','Upp~','V.Unbal Uo','V.Unbal Uu','I1','I2','I3','I~','Itotal','In','P1','P2','P3','Ptotal','Q1','Q2','Q3','Qtot','D1','D2','D3','D','S1','S2','S3','Stot','PF1','PF2','PF3','PFtot','THD U1','THD U2','THD U3','THD U12','THD U23','THD U31','THD I1','THD I2','THD I3','DDmd P+','DDmd P-','DDmd Q+','DDmd Q-','DDmd S','DDmd I1','DDmd I2','DDmd I3','MDmd P+','MDmd P-','MDmd Q+','MDmd Q-','MDmd S','MDmd I1','MDmd I2','MDmd I3','UnderDev U1','UnderDev U2','UnderDev U3','OverDev U1','OverDev U2','OverDev U3','CF U1','CF U2','CF U3','CF U12','CF U23','CF U31','CF I1','CF I2','CF I3','KF I1','KF I2','KF I3','PSt1','PSt2','PSt3','Plt1','Plt2','Plt3','Sig.V1','Sig.V2','Sig.V3'];\n"
"var ET={1:['Sag','sag'],2:['Swell','swell'],3:['S.Intr','intr'],4:['L.Intr','intr'],5:['OC','oc'],6:['RVC','tr'],7:['Trans V','tr'],8:['Trans I','tr'],9:['SOE','oc']};\n"
"function acn(i){return AC[i]||('#'+i)}\n"
"function j(u,o){o=o||{};var ac=('AbortController'in window)?new AbortController():null,tm=null;if(ac){o.signal=ac.signal;tm=setTimeout(function(){ac.abort()},6000);}\n"
" return fetch(u,o).then(function(r){return r.text().then(function(t){if(tm)clearTimeout(tm);return{s:r.status,d:JSON.parse((t||'{}').replace(/-?\\b(inf|nan)\\b/gi,'0'))}})}).catch(function(e){if(tm)clearTimeout(tm);throw e;})}\n"
"function jp(u,o){return j(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});}\n"
"function sn(a){$('setNote').textContent=IS_ADMIN?a:'Viewer: read-only.';}\n"
"function n(v,d){return(v==null||isNaN(v))?'----':Number(v).toFixed(d)}\n"
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
" if(r.d.auth){IS_ADMIN=(r.d.role==='admin');if(r.d.nch)NCH=r.d.nch;\n"
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
"function go(pg,tab){cur=pg;ioEd=0;\n"
" ['dash','feeder','channel','io','setup'].forEach(function(p){var pe=$('p-'+p);if(pe)pe.style.display=(p===pg)?'':'none';var ne=$('nav-'+p);if(ne)ne.className=(p===pg)?'on':'';var se=$('si-'+p);if(se)se.className='sitem'+(p===pg?' active':'');});\n"
" if(pg!=='setup')$('si-chset').className='sitem';\n"
" if(pg==='setup')setEnter(tab);else if(pg==='channel')chEnter();else pollLoop();}\n"
"(function(){var t=$('pollToggle');window.__pollOn=true;t.addEventListener('change',function(){window.__pollOn=t.checked;$('pollState').textContent=t.checked?'ON':'OFF';$('pollState').className='poll-state'+(t.checked?'':' off');});})();\n"
/* dashboard */
"function dstat(ok){var e=$('dstat');if(ok){e.className='on';e.innerHTML='&#9679; live &#8635; 1s'}else{e.className='bad';e.innerHTML='&#9679; read error'}}\n"
"function setGauge(pf){var C=377,f=Math.max(0,Math.min(1,pf/100)),col=pf>=90?'#22c55e':'#ef4444',st=pf>=90?'Normal':'Below limit';\n"
" var arc=$('pfArc');arc.setAttribute('stroke',col);arc.setAttribute('stroke-dashoffset',C*(1-f));\n"
" var a=(-90+f*360)*Math.PI/180,dt=$('pfDot');dt.setAttribute('cx',70+60*Math.cos(a));dt.setAttribute('cy',70+60*Math.sin(a));dt.setAttribute('fill',col);\n"
" $('d-pf').style.color=col;$('d-pfst').style.color=col;$('d-pfst').textContent=st;}\n"
"function setBar(id,bid,v){var e=$(id);e.textContent=n(v,1)+'%';e.className='uv '+(v>=100?'r':'g');var b=$(bid);b.style.width=Math.max(1,Math.min(100,v||0))+'%';b.className='ufill'+(v>=100?' hot':'');}\n"
"var DBLANK={u:[null,null,null],i:[null,null,null]};\n"
"function renderDash(d){\n"
"  $('d-vavg').textContent=n(d.v_avg,1);$('d-iavg').textContent=n(d.i_avg,2);$('d-freq').textContent=n(d.freq,2);\n"
"  $('d-u1').textContent=n(d.u[0],1)+' V';$('d-u2').textContent=n(d.u[1],1)+' V';$('d-u3').textContent=n(d.u[2],1)+' V';\n"
"  $('d-i1').textContent=n(d.i[0],2)+' A';$('d-i2').textContent=n(d.i[1],2)+' A';$('d-i3').textContent=n(d.i[2],2)+' A';\n"
"  $('d-p').textContent=n(d.p,2);$('d-q').textContent=n(d.q,2);$('d-s').textContent=n(d.s,2);\n"
"  $('d-pf').textContent=n(d.pf,2);setGauge(d.pf==null?0:d.pf);\n"
"  setBar('d-vunb','d-vunb-bar',d.u_unbal);setBar('d-iunb','d-iunb-bar',d.i_unbal);\n"
"  $('d-thdu').textContent=n(d.thd_u,1)+'%';$('d-thdi').textContent=n(d.thd_i,1)+'%';$('d-tddi').textContent=n(d.tdd_i,1)+'%';}\n"
"function loadDash(){return j('/api/dashboard').then(function(r){if(!r.d.ok){renderDash(DBLANK);dstat(false);return}\n"
"  renderDash(r.d.data);$('dmeas').textContent='Measured: '+fdt(new Date());dstat(true);\n"
" }).catch(function(){renderDash(DBLANK);dstat(false)});}\n"
"function loadIom(){return j('/api/dashboard').then(function(r){if(!r.d.ok)return;var d=r.d.data;if(d&&d.iom){iomDat=d.iom;if(!ioEd)renderIom();}}).catch(function(){});}\n"
"var ioEd=0,iomDat=null;\n"
"function renderIom(){var io=iomDat;if(!io)return;var k,ed=ioEd&&IS_ADMIN,di=\"<div class='hsub'>DI</div><table class='ctbl hsm'><tr><th>P</th><th>Type</th><th>Deb</th><th>Scale</th><th>St</th><th>Pulse</th></tr>\";\n"
" for(k=0;k<4;k++){var ty,de,sc;if(ed){ty=\"<select data-addr='\"+(7412+k)+\"' data-type='u16' data-orig='\"+io.dt[k]+\"'><option value='0'\"+(io.dt[k]==0?' selected':'')+\">DI</option><option value='1'\"+(io.dt[k]==1?' selected':'')+\">PI</option></select>\";de=\"<input type='number' min='4' max='64' data-addr='\"+(7416+k)+\"' data-type='u16' data-orig='\"+io.db[k]+\"' value='\"+io.db[k]+\"' style='width:50px'>\";sc=\"<input type='number' data-addr='\"+(7420+k)+\"' data-type='u16' data-orig='\"+io.sc[k]+\"' value='\"+io.sc[k]+\"' style='width:50px'>\";}else{ty=(io.dt[k]==1?'PI':'DI');de=io.db[k];sc=io.sc[k];}di+=\"<tr><td class='rk'>#\"+k+\"</td><td>\"+ty+\"</td><td>\"+de+\"</td><td>\"+sc+\"</td><td>\"+io.di[k]+\"</td><td>\"+io.pi[k]+\"</td></tr>\";}di+='</table>';\n"
" var hasPI=(io.dt[0]==1||io.dt[1]==1||io.dt[2]==1||io.dt[3]==1);\n"
" if(IS_ADMIN)di+=ed?\"<div class='pgctl'><button class='btn primary' onclick='ioSave()'>Save</button><button class='btn' onclick='ioEd=0;renderIom()'>Cancel</button></div>\":\"<div class='pgctl'><button class='btn' onclick='ioEd=1;renderIom()'>Edit</button><button class='btn warn' onclick='clearPI()'\"+(hasPI?'':' disabled')+\">Clear PI</button></div>\";\n"
" var tp='';if(NCH!==3){tp=\"<div class='hsub'>Temperature</div><table class='ctbl hsm'><tr><th>P</th><th>Temp(&#8451;)</th></tr>\";for(k=0;k<4;k++)tp+=\"<tr><td class='rk'>#\"+k+\"</td><td>\"+n(io.temp[k],1)+\"</td></tr>\";tp+='</table>';}\n"
" $('d-iom').innerHTML=\"<div style='display:flex;gap:14px;flex-wrap:wrap'><div style='flex:1;min-width:300px'>\"+di+\"</div>\"+(tp?\"<div style='min-width:150px'>\"+tp+\"</div>\":'')+\"</div>\";}\n"
"function ioSave(){var els=document.querySelectorAll('#d-iom [data-addr]'),c=[],i=0;els.forEach(function(e){if(e.value!==e.getAttribute('data-orig'))c.push(e);});if(!c.length){ioEd=0;renderIom();return;}(function w(){if(i>=c.length){ioEd=0;j('/api/savecfg',{method:'POST'}).then(function(){setTimeout(pollLoop,300);});return;}var e=c[i++],v=+e.value,mn=e.getAttribute('min'),mx=e.getAttribute('max');if(mn)v=Math.max(+mn,v);if(mx)v=Math.min(+mx,v);jp('/api/setreg',{addr:+e.getAttribute('data-addr'),type:'u16',val:v}).then(w).catch(w);})();}\n"
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
" jp('/api/sv300/command',{cmd:cmd}).then(function(r){\n"
"  if(r.s===403){alert('Admin only');return}if(!r.d.ok){alert('Command failed');return}loadEvents();});}\n"
/* setup */
"var OPT={va_type:{0:'RMS (S=VA)',1:'Vector'},pf_sign:{0:'IEC',1:'IEEE'},demand_interval:{0:'1 min',1:'5 min',2:'15 min',3:'30 min',4:'60 min'},minmax_reset:{0:'Daily',1:'Weekly',2:'Monthly'},timezone:{'-720':'UTC-12:00','-600':'Hawaii -10:00','-540':'Alaska -09:00','-480':'Pacific -08:00','-420':'Mountain -07:00','-360':'Central -06:00','-300':'Eastern -05:00','-180':'-03:00','0':'UTC +00:00','60':'Berlin +01:00','120':'Athens +02:00','180':'Moscow +03:00','210':'Tehran +03:30','240':'Dubai +04:00','300':'Karachi +05:00','330':'India +05:30','345':'Nepal +05:45','360':'Dhaka +06:00','420':'Bangkok +07:00','480':'Shanghai +08:00','540':'Seoul +09:00','570':'Adelaide +09:30','600':'Sydney +10:00','720':'Auckland +12:00'}};\n"
"var CARDN=['Device Information','Communication','ETC','Status'],setCur='general';\n"
"var WM={0:'not used',1:'3P4W',2:'3P3W(2CT)',3:'3P3W(3CT)',4:'1P2W(L1)',5:'1P2W(L2)',6:'1P2W(L3)',7:'1P3W',8:'SIM'};\n"
"var CT2M={0:'5A',1:'100mA/333mV',2:'Rogowski'},ZCTM={1:'200mV:100mV',2:'200mV:1.5mA',3:'200mV:0.1mA'},DIM={0:'DI',1:'PI'},DIRM={0:'+',1:'-'};\n"
"var PT_F=[{o:0,l:'Wiring Mode',t:'u16',opt:WM},{o:2,l:'V Nominal',t:'u32'},{o:4,l:'PT1',t:'u32'},{o:6,l:'PT2',t:'u16'}];\n"
"var CT_F=[{o:0,l:'I Nominal (%)',t:'u16'},{o:1,l:'CT1',t:'u16'},{o:2,l:'CT2',t:'u16',opt:CT2M},{o:3,l:'Turns',t:'u16'},{o:4,l:'Start I',t:'u16',sc:1000},{o:5,l:'CT1 Dir',t:'u16',opt:DIRM},{o:6,l:'CT2 Dir',t:'u16',opt:DIRM},{o:7,l:'CT3 Dir',t:'u16',opt:DIRM},{o:8,l:'Rogowski',t:'u16'},{o:9,l:'Phase Ofs 1',t:'i16'},{o:10,l:'Phase Ofs 2',t:'i16'},{o:11,l:'Phase Ofs 3',t:'i16'},{o:12,l:'ZCT Type',t:'u16',opt:ZCTM},{o:13,l:'ZCT Scale',t:'u16'}];\n"
"var CMDS=[{g:'System',l:'Reboot',addr:7448,danger:1,note:'reboot'},{g:'System',l:'Save Set',addr:7449},{g:'System',l:'Init Settings',addr:7464,danger:2},{g:'System',l:'Set Time',addr:7438,utc:1}];\n"
"var MAIN_TABS=[['general','General'],['pt','PT'],['ct','CT'],['command','Command']];\n"
"var CHAN_TABS=[['pqevent','PQ Event'],['transient','Transient'],['waveform','Waveform'],['disturb','Disturbance'],['trend','Trend'],['pqreport','PQ Report'],['almdef','Alarm Def'],['alarm','Alarm Settings']];\n"
"var setMode='main';\n"
"function setEnter(arg){setMode=(arg==='chan')?'chan':'main';$('setBadge').textContent=(setMode==='chan')?'Channel Setting':'Main Setting';$('si-setup').className='sitem'+(setMode==='main'?' active':'');$('si-chset').className='sitem'+(setMode==='chan'?' active':'');setTab(((setMode==='chan')?CHAN_TABS:MAIN_TABS)[0][0]);}\n"
"function setTab(t){setCur=t;var L=(setMode==='chan')?CHAN_TABS:MAIN_TABS,h='';L.forEach(function(p){h+=\"<a class='gp-tab\"+(p[0]===t?' on':'')+\"' data-t='\"+p[0]+\"'>\"+esc(p[1])+'</a>';});var st=$('setTabs');st.innerHTML=h;st.onclick=function(e){var a=e.target.closest('a[data-t]');if(a)setTab(a.getAttribute('data-t'));};\n"
" $('setSave').style.display=((t!=='command')&&IS_ADMIN)?'':'none';$('setRes').textContent='';$('setBody').innerHTML=\"<p class='stub'>loading...</p>\";reloadSet();}\n"
"function reloadSet(){var t=setCur;if(t==='general')loadGeneral();else if(t==='pt')loadGroup('pt');else if(t==='ct')loadGroup('ct');else if(t==='command')loadCommand();else if(t==='alarm')loadAlarm();else loadCSect(t);}\n"
"var CSECT={pqevent:[[6700,'Level','Over Current'],[6701,'Cycle','Over Current'],[6702,'Trigger Action','Over Current'],[6703,'holdOff Cycle','Over Current'],[6706,'Level(%)','Sag',1],[6707,'Cycle','Sag',1],[6708,'Trigger Action','Sag',1],[6709,'holdOff Cycle','Sag',1],[6712,'Level(%)','Swell',1],[6713,'Cycle','Swell',1],[6714,'Trigger Action','Swell',1],[6715,'holdOff Cycle','Swell',1],[6718,'Level(%)','Interruption',1],[6719,'Time(s)','Interruption',1],[6720,'Trigger Action','Interruption',1],[6721,'holdOff Cycle','Interruption',1]],transient:[[6724,'HoldOff(ms)','Voltage',1],[6725,'Abs Peak(%)','Voltage',1],[6726,'Fast Change','Voltage',1],[6727,'Trigger Action','Voltage',1],[6728,'HoldOff','Current'],[6729,'Abs Peak','Current'],[6730,'Fast Change','Current'],[6731,'Trigger Action','Current']],waveform:[[6744,'Resolution(32k/8k)'],[6745,'Param(U/I)'],[6746,'Pre trig(0.1s)'],[6747,'Post trig(1s)']],disturb:[[6748,'Resolution(HalfCyc)'],[6749,'Param(U/I)'],[6750,'Pre trig(smp)'],[6751,'Post trig(smp)']],pqreport:[[6844,'Active'],[6845,'Start Day']],almdef:[[6854,'compare Time Delay(s)']]};\n"
"(function(){var t=[];[[1,6764,6766,6767],[2,6783,6785,6786],[3,6802,0,6805],[4,6821,0,6824]].forEach(function(g){var gn='Group '+g[0];t.push([g[1],'Active',gn]);t.push([g[1]+1,'Interval',gn]);if(g[2])t.push([g[2],'Version',gn]);for(var k=0;k<16;k++)t.push([g[3]+k,'Ch'+(k+1),gn]);});CSECT.trend=t;})();\n"
"function loadCSect(key){var f=CSECT[key];if(!f){$('setBody').innerHTML=\"<p class='stub'>-</p>\";return;}var lo=f[0][0],hi=f[0][0];f.forEach(function(p){if(p[0]<lo)lo=p[0];if(p[0]>hi)hi=p[0];});var nn=hi-lo+1;\n"
" var _chs=[];for(var _k=1;_k<=NCH;_k++)_chs.push(_k);\n"
" Promise.all(_chs.map(function(ch){return j('/api/regs?addr='+(lo+(ch-1)*10000)+'&n='+nn).then(function(r){return (r.d&&r.d.ok)?r.d.words:null;}).catch(function(){return null;});})).then(function(bl){if(setCur!==key)return;\n"
"  var h=\"<div class='setgrid'>\";for(var ch=1;ch<=NCH;ch++){var wds=bl[ch-1];h+=\"<div class='setcard'><h3>CH\"+ch+'</h3>';\n"
"   if(!wds){h+=\"<p class='stub'>no data</p>\";}else{var pg=null;f.forEach(function(p){if(ch>1&&p[3])return;var g=p[2]||'';if(g&&g!==pg){h+=\"<div class='sgrp'>\"+esc(g)+\"</div>\";}pg=g;var addr=p[0]+(ch-1)*10000,v=wds[p[0]-lo]||0,ctl;if(p[1]==='Trigger Action'){ctl=\"<select data-addr='\"+addr+\"' data-type='u16' data-orig='\"+v+\"' \"+(IS_ADMIN?'':'disabled')+\" onchange='setMark(this)'><option value='0'\"+(v==0?' selected':'')+\">NONE</option><option value='1'\"+(v==1?' selected':'')+\">EVENT</option><option value='2'\"+(v==2?' selected':'')+\">WAVE CAPTURE</option></select>\";}else{ctl=\"<input type='number' data-addr='\"+addr+\"' data-type='u16' data-orig='\"+v+\"' value='\"+v+\"' \"+(IS_ADMIN?'':'disabled')+\" oninput='setMark(this)'>\";}h+=\"<div class='srow'><span class='sk'>\"+esc(p[1])+\"</span>\"+ctl+\"</div>\";});}\n"
"   h+='</div>';}h+='</div>';$('setBody').innerHTML=h;sn('Save & Apply.');});}\n"
"function fmtV(f){return (f.type==='bool')?(f.val?'On':'Off'):esc(f.val)}\n"
"function ctlOf(f){var d=\"data-addr='\"+f.addr+\"' data-type='\"+f.type+\"' data-orig='\"+esc(f.val)+\"'\",dis=IS_ADMIN?'':'disabled';\n"
" if(f.ro)return \"<span class='sv'>\"+fmtV(f)+\"</span><span class='rtag'>R</span>\";\n"
" if(OPT[f.k]){var o=OPT[f.k],s=\"<select \"+d+' '+dis+\" onchange='setMark(this)'>\";for(var k in o)s+=\"<option value='\"+k+\"'\"+((''+k)===(''+f.val)?' selected':'')+'>'+esc(o[k])+'</option>';return s+'</select>';}\n"
" if(f.type==='bool')return \"<input type='checkbox' \"+d+' '+dis+' '+(f.val?'checked':'')+\" onchange='setMark(this)'>\";\n"
" var tp=(f.type==='ip'||f.type==='str')?'text':'number';\n"
" return \"<input type='\"+tp+\"' \"+d+\" value='\"+esc(f.val)+\"' \"+dis+\" oninput='setMark(this)'>\";}\n"
"function loadGeneral(){return j('/api/general').then(function(r){if(!r.d.ok){$('setBody').innerHTML=\"<p class='stub'>load error</p>\";return;}\n"
" var cs=[[],[],[],[]];r.d.fields.forEach(function(f){cs[f.card].push(f)});var h=\"<div class='setgrid'>\";\n"
" cs.forEach(function(fs,ci){h+=\"<div class='setcard'><h3>\"+CARDN[ci]+'</h3>';fs.forEach(function(f){h+=\"<div class='srow'><span class='sk'>\"+esc(f.label)+'</span>'+ctlOf(f)+'</div>';});h+='</div>';});\n"
" h+='</div>';$('setBody').innerHTML=h;sn('Save & Apply.');});}\n"
"function setMark(el){var v=(el.type==='checkbox')?(el.checked?'1':'0'):el.value;if((''+v)!==el.getAttribute('data-orig'))el.classList.add('chg');else el.classList.remove('chg');}\n"
"function setRes(m){$('setRes').textContent=m}\n"
"function saveSet(){var els=document.querySelectorAll('#setBody [data-addr]'),chg=[];\n"
" els.forEach(function(el){var v=(el.type==='checkbox')?(el.checked?'1':'0'):el.value;if((''+v)!==el.getAttribute('data-orig'))chg.push({el:el,v:v});});\n"
" if(!chg.length){setRes('No changes');return;}if(!confirm(chg.length+' change(s). Save?'))return;setRes('Writing...');\n"
" var i=0;function wr(a,t,v){return jp('/api/setreg',{addr:a,type:t,val:v});}\n"
" function next(){if(i>=chg.length){j('/api/savecfg',{method:'POST'}).then(function(){setRes('Saved ('+chg.length+')');reloadSet();});return;}\n"
"  var it=chg[i++],addr=+it.el.getAttribute('data-addr'),type=it.el.getAttribute('data-type');\n"
"  if(type==='ip'){var p=(it.v||'0.0.0.0').split('.'),k=0;(function nip(){if(k>=4){next();return;}wr(addr+k,'u16',+(p[k]||0)).then(function(){k++;nip();});})();}\n"
"  else if(type==='float'){var wf=fToU16(+it.v||0),k2=0;(function nf(){if(k2>=2){next();return;}wr(addr+k2,'u16',wf[k2]).then(function(){k2++;nf();});})();}\n"
"  else{var scl=+(it.el.getAttribute('data-scale')||1);wr(addr,type,Math.round((+it.v||0)*scl)).then(function(rr){if(rr.s===403){setRes('Admin only');return;}next();});}}\n"
" next();}\n"
/* setup: PT/CT/IO/Command */
"function decodeF(wds,idx,t){if(t==='u32')return((wds[idx]||0)|((wds[idx+1]||0)<<16))>>>0;if(t==='i16'){var v=wds[idx]||0;return v>32767?v-65536:v;}return wds[idx]||0;}\n"
"function grpCtl(f,addr,val){var sc=f.sc||1,dv=(sc!==1)?(val/sc):val,d=\"data-addr='\"+addr+\"' data-type='\"+f.t+\"' data-scale='\"+sc+\"' data-orig='\"+dv+\"'\",dis=IS_ADMIN?'':'disabled';\n"
" if(f.opt){var s=\"<select \"+d+' '+dis+\" onchange='setMark(this)'>\";for(var k in f.opt)s+=\"<option value='\"+k+\"'\"+((''+k)===(''+val)?' selected':'')+'>'+esc(f.opt[k])+'</option>';return s+'</select>';}\n"
" return \"<input type='number' \"+(sc!==1?(\"step='\"+(1/sc)+\"' \"):'')+d+\" value='\"+dv+\"' \"+dis+\" oninput='setMark(this)'>\";}\n"
"function loadGroup(kind){var cfg=(kind==='pt')?{base:7172,step:8,fields:PT_F,ti:'PT'}:{base:7244,step:16,fields:CT_F,ti:'CT'};\n"
" var _npt=NCH*3;\n"
" return j('/api/regs?addr='+cfg.base+'&n='+(cfg.step*_npt)).then(function(r){if(!r.d.ok){$('setBody').innerHTML=\"<p class='stub'>load error</p>\";return;}var wds=r.d.words,s,sb,so;\n"
"  var h;\n"
"  if(kind==='ct'){\n"
"   h=\"<div class='setgrid'>\";\n"
"   for(s=0;s<_npt;s++){sb=cfg.base+s*cfg.step;so=s*cfg.step;h+=\"<div class='setcard'><h3>CT #\"+(s+1)+'</h3>';cfg.fields.forEach(function(f){h+=\"<div class='srow'><span class='sk'>\"+esc(f.l)+\"</span><span>\"+grpCtl(f,sb+f.o,decodeF(wds,so+f.o,f.t))+'</span></div>';});h+='</div>';}\n"
"   h+='</div>';$('setBody').innerHTML=h;sn('Save & Apply.');return;}\n"
"  h=\"<div class='settbl-wrap'><table class='settbl'><thead><tr><th>\"+cfg.ti+'</th>';cfg.fields.forEach(function(f){h+='<th>'+esc(f.l)+'</th>';});h+='</tr></thead><tbody>';\n"
"  for(s=0;s<_npt;s++){sb=cfg.base+s*cfg.step;so=s*cfg.step;h+=\"<tr><td class='rk'>#\"+(s+1)+'</td>';cfg.fields.forEach(function(f){h+='<td>'+grpCtl(f,sb+f.o,decodeF(wds,so+f.o,f.t))+'</td>';});h+='</tr>';}\n"
"  h+='</tbody></table></div>';$('setBody').innerHTML=h;sn('Save & Apply.');});}\n"
"function tgtOpts(id){var s=\"<select id='t_\"+id+\"' style='width:80px'><option value='0'>ALL</option>\";for(var t=1;t<=NCH;t++)s+=\"<option value='\"+t+\"'>#\"+t+'</option>';return s+'</select>';}\n"
"function loadCommand(){var gr={},od=[];CMDS.forEach(function(c){if(!gr[c.g]){gr[c.g]=[];od.push(c.g);}gr[c.g].push(c);});var h='';\n"
" od.forEach(function(g){h+=\"<div class='setcard'><h3>\"+g+'</h3>';gr[g].forEach(function(c){var gi=CMDS.indexOf(c),id='c'+gi,ctl='';\n"
"  if(c.utc)ctl=\"<input lang='en-US' type='datetime-local' step='1' id='u_\"+id+\"' style='width:230px'>\";else if(c.tgt)ctl=tgtOpts(id);\n"
"  h+=\"<div class='srow'><span class='sk'>\"+c.l+\"</span><span style='display:flex;gap:8px;align-items:center'>\"+ctl+\"<button class='btn\"+(c.danger?' warn':'')+\"' \"+(IS_ADMIN?'':'disabled')+\" onclick='runCmd(\"+gi+\")'>Run</button></span></div>\";});h+='</div>';});\n"
" if(IS_ADMIN)h+=\"<div class='setcard'><h3>Firmware Update</h3><div class='srow'><span class='sk'>File (.bin)</span><span style='display:flex;gap:8px;align-items:center'><input type='file' id='fwFile' accept='.bin' style='display:none' onchange='fwFileSel()'><button class='btn' \"+(IS_ADMIN?'':'disabled')+\" onclick='fwPick()'>Choose File</button><span id='fwFName' style='color:var(--muted)'>No file selected</span></span></div><div class='srow'><span class='sk'>Status</span><span class='sv' id='fwStat'>-</span></div><div class='srow'><span></span><span style='display:flex;gap:8px'><button class='btn' \"+(IS_ADMIN?'':'disabled')+\" onclick='fwUpload()'>Upload</button><button class='btn warn' id='fwApplyBtn' disabled onclick='fwApply()'>Apply &amp; Reboot</button></span></div></div>\";\n"
" $('setBody').innerHTML=\"<div class='setgrid'>\"+h+'</div>';$('setNote').textContent=IS_ADMIN?'Reboot/Init.':'Viewer: no execute.';\n"
" $('setBody').querySelectorAll('[type=datetime-local]').forEach(function(e){e.value=fdt(new Date()).replace(' ','T');});}\n"
"function waitReboot(){var tr=0;setTimeout(function pl(){tr++;fetch('/api/me',{cache:'no-store'}).then(function(){location.reload();}).catch(function(){if(tr<40)setTimeout(pl,2000);else location.reload();});},8000);}\n"
"function cmdDone(r,c){if(r.s===403){alert('Admin only');return;}if(!r.d.ok){alert('Command failed');return;}setRes(c.l+' done');if(c.note==='reboot'){$('setNote').textContent='Rebooting...';waitReboot();}}\n"
"function fwPick(){$('fwFile').click();}\n"
"function fwFileSel(){var fi=$('fwFile'),n=(fi&&fi.files&&fi.files[0])?fi.files[0].name:'No file selected';$('fwFName').textContent=n;var ab=$('fwApplyBtn');if(ab)ab.disabled=true;var e=$('fwStat');if(e){e.textContent='-';e.style.color='';}}\n"
"function fwUpload(){if(!IS_ADMIN){alert('Admin only');return;}var fi=$('fwFile'),f=fi&&fi.files&&fi.files[0];if(!f){alert('Select a .bin file');return;}var e=$('fwStat');e.textContent='uploading '+((f.size/1024)|0)+' KB...';e.style.color='';fetch('/api/fwupload',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f}).then(function(rs){return rs.json().then(function(d){return{s:rs.status,d:d};},function(){return{s:rs.status,d:null};});}).then(function(r){if(r.s===403){e.textContent='Admin only';e.style.color='var(--bad)';return;}var ab=$('fwApplyBtn');if(r.d&&r.d.ok&&r.d.valid){e.textContent='uploaded & valid, '+((r.d.size/1024)|0)+' KB';e.style.color='var(--ok)';if(ab)ab.disabled=false;}else{e.textContent='failed'+(r.d&&r.d.error?': '+r.d.error:'')+(r.d&&r.d.err!==undefined&&r.d.err!==0?' (sign err '+r.d.err+')':'');e.style.color='var(--bad)';if(ab)ab.disabled=true;}}).catch(function(){e.textContent='upload error';e.style.color='var(--bad)';});}\n"
"function fwApply(){if(!IS_ADMIN){alert('Admin only');return;}if(!confirm('Apply uploaded usrApp.bin and reboot?\\nDevice will reflash firmware via bootloader.'))return;$('setNote').textContent='Validating firmware...';jp('/api/fwapply',{}).then(function(r){if(r.s===403){alert('Admin only');$('setNote').textContent='';return;}if(!r.d.ok){alert('Apply failed (err '+(r.d.err!==undefined?r.d.err:'?')+').\\nUpload a valid usrApp.bin first.');$('setNote').textContent='';return;}$('setNote').textContent='Firmware apply: rebooting & flashing...';waitReboot();});}\n"
/* Feeder */
"function fbadge(w){return `<span class='wb'>${WM[w]||('mode '+w)}</span>`;}\n"
"function kv(k,v,u){return `<div class='kvi'><span class='kk'>${k}</span><span class='kvv'>${v}<small> ${u}</small></span></div>`;}\n"
"var lastFeed=null;\n"
"function fCards(list){var h='';list.forEach(function(f){\n"
"  h+=`<div class='fcard${f.st?'':' off'}'><div class='fch'><span class='ft'>Feeder #${f.n}</span>${fbadge(f.wiring)}<span class='ff'>${n(f.freq,2)} Hz</span></div>`;\n"
"  h+=`<table class='ftbl'><tr><th></th><th>Phase V</th><th>Line V</th><th>Current</th><th>THD-U</th><th>THD-I</th></tr>`;\n"
"  ['L1','L2','L3'].forEach(function(ph,k){h+=`<tr><td class='pk'>${ph}</td><td>${n(f.u[k],1)}</td><td>${n(f.upp[k],1)}</td><td>${n(f.i[k],2)}</td><td>${n(f.thdu[k],1)}%</td><td>${n(f.thdi[k],1)}%</td></tr>`;});\n"
"  h+=`</table><div class='fkv'>`+kv('In',n(f.in,2),'A')+kv('PF',n(f.pf,3),'')+kv('P',n(f.p,2),'kW')+kv('Q',n(f.q,2),'kVar')+kv('S',n(f.s,2),'kVA')+kv('U-unb',n(f.uunb,1),'%')+kv('I-unb',n(f.iunb,1),'%')+kv('Import',n(f.kwh,2),'kWh')+kv('Month',n(f.kwh_m,2),'kWh')+`</div></div>`;\n"
"  });return h;}\n"
"function fErr(){$('fstat').className='bad';$('fstat').innerHTML='&#9679; read error';if(lastFeed)$('fgrid').innerHTML=fCards(chBlank(lastFeed));}\n"
"function loadFeeder(){return j('/api/feeders').then(function(r){\n"
" if(!r.d.ok){fErr();return;}\n"
" $('fstat').className='on';$('fstat').innerHTML='&#9679; live &#8635; 1s';lastFeed=r.d.feeders;$('fgrid').innerHTML=fCards(r.d.feeders);\n"
" }).catch(fErr);}\n"
/* Channel */
"var chTabCur='meter',CHN=[{n:1},{n:2},{n:3}];\n"
"var harmOpt='pv',harmView='table',PCOL=['#8b5cf6','#38bdf8','#84cc16'];\n"
"function chEnter(){chSetTab(chTabCur);}\n"
"function chSetTab(t){chTabCur=t;egyEd=0;['meter','phase','minmax','harmonics','waveform','report','vq','monthly','energy','demand','alarm','event'].forEach(function(x){var e=$('ct-'+x);if(e)e.className='gp-tab'+(x===t?' on':'');});var cc=$('chctl');if(cc)cc.style.display=(t==='harmonics')?'flex':'none';syncHv();$('chbody').innerHTML=\"<p class='stub'>loading...</p>\";pollLoop();}\n"
"function chStat(ok){var e=$('chstat');if(e){e.className='ph2 '+(ok?'on':'bad');e.innerHTML=ok?'&#9679; live &#8635; 1s':'&#9679; read error';}}\n"
"function chFail(){chStat(false);var b=$('chbody');if(b&&b.innerHTML.indexOf('loading')>=0)b.innerHTML=\"<p class='stub'>read error - retrying...</p>\";}\n"
"function chCards(list,fn){var h=\"<div class='chgrid'>\";list.forEach(function(x){h+=`<div class='chcard'><div class='chh'>CH${x.n}${x.wiring!==undefined?' '+fbadge(x.wiring):''}</div>${fn(x)}</div>`;});return h+'</div>';}\n"
"var lastCh=null;\n"
"function nv(o){if(Array.isArray(o))return o.map(nv);if(o&&typeof o==='object'){var r={};for(var k in o)r[k]=nv(o[k]);return r;}return(typeof o==='number')?null:o;}\n"
"function chBlank(list){return(list||[]).map(function(x){var o={};for(var k in x)o[k]=(k==='n'||k==='wiring')?x[k]:nv(x[k]);return o;});}\n"
"function chErr(fn){chStat(false);if(lastCh)$('chbody').innerHTML=chCards(chBlank(lastCh),fn);else{var b=$('chbody');if(b&&b.innerHTML.indexOf('loading')>=0)b.innerHTML=\"<p class='stub'>read error - retrying...</p>\";}}\n"
"function r4(nm,a,d){return `<tr><td class='rk'>${nm}</td><td>${n(a[0],d)}</td><td>${n(a[1],d)}</td><td>${n(a[2],d)}</td><td>${n(a[3],d)}</td></tr>`;}\n"
"function r3(nm,a,d){return `<tr><td class='rk'>${nm}</td><td>${n(a[0],d)}</td><td>${n(a[1],d)}</td><td>${n(a[2],d)}</td><td></td></tr>`;}\n"
"function rs(nm,v,d){return `<tr><td class='rk'>${nm}</td><td></td><td></td><td></td><td>${n(v,d)}</td></tr>`;}\n"
"function bMeter(x){var h=\"<table class='ctbl mtbl'><tr><th></th><th>L1</th><th>L2</th><th>L3</th><th>Total/Avg</th></tr>\";\n"
" h+=rs('Frequency (Hz)',x.freq,2)+rs('Temp (&deg;C)',x.temp,1)+r4('U (V)',x.u,1)+r4('U L-L (V)',x.upp,1);\n"
" h+=rs('U unbal Uu (%)',x.uu,2)+rs('U unbal Uo (%)',x.uo,2);\n"
" h+=rs('U Zero Seq Mag',x.uzs[0],2)+rs('U Zero Seq Ang',x.uzs[1],1)+rs('U Pos Seq Mag',x.ups[0],2)+rs('U Pos Seq Ang',x.ups[1],1)+rs('U Neg Seq Mag',x.uns[0],2)+rs('U Neg Seq Ang',x.uns[1],1);\n"
" h+=r3('Angle U (&deg;)',x.uang,1)+r3('U UnderDev (%)',x.uundev,1)+r3('U OverDev (%)',x.uovdev,1);\n"
" h+=r4('I (A)',x.i,2)+rs('I total (A)',x.itot,2)+rs('In meas (A)',x.in,2)+rs('In calc (A)',x.isum,2)+rs('Ig (A)',x.ig,2);\n"
" h+=rs('I Zero Seq Mag',x.izs[0],2)+rs('I Zero Seq Ang',x.izs[1],1)+rs('I Pos Seq Mag',x.ips[0],2)+rs('I Pos Seq Ang',x.ips[1],1)+rs('I Neg Seq Mag',x.ins[0],2)+rs('I Neg Seq Ang',x.ins[1],1);\n"
" h+=r3('Angle I (&deg;)',x.iang,1)+rs('I unbal Iu (%)',x.iu,2)+rs('I unbal Io (%)',x.io,2);\n"
" h+=r4('P (kW)',x.p,2)+r4('fP (kW)',x.fp,2)+r4('Q (kVar)',x.q,2)+r4('fQ (kVar)',x.fq,2)+r4('S (kVA)',x.s,2)+r4('fS (kVA)',x.fs,2);\n"
" h+=r4('PF',x.pf,3)+r4('dPF',x.dpf,3)+r4('Deformed D (kW)',x.d,2)+r4('Power Angle (&deg;)',x.pang,1);\n"
" h+=r3('THD V (%)',x.thdu,2)+r3('THD V L-L (%)',x.thdupp,2)+r3('CF V',x.cfu,2)+r3('CF V L-L',x.cfupp,2);\n"
" h+=r3('THD I (%)',x.thdi,2)+r3('TDD I (%)',x.tddi,2)+r3('K-Factor I',x.kfi,2)+r3('CF I',x.cfi,2);\n"
" h+=r4('fU (V)',x.fu,1)+r4('fI (A)',x.fi,2);\n"
" return h+'</table>';}\n"
"function phVec(cx,cy,ang,len,col,mk,dash){var r=ang*Math.PI/180,x=(cx+len*Math.cos(r)).toFixed(1),y=(cy-len*Math.sin(r)).toFixed(1);return `<line x1='${cx}' y1='${cy}' x2='${x}' y2='${y}' stroke='${col}' stroke-width='2.2'${dash?\" stroke-dasharray='6,4'\":\"\"} marker-end='url(#${mk})'/>`;}\n"
"function phasorSVG(x){var cx=130,cy=130,R=110,mk='ar'+x.n,a,k,r,PHV=['#B5651D','#FFFFFF','#7E8896'],PHI=['#B5651D','#FFFFFF','#7E8896','#1E64FF'],um=Math.max(Math.abs(x.u[0]),Math.abs(x.u[1]),Math.abs(x.u[2]))||1,im=Math.max(Math.abs(x.i[0]),Math.abs(x.i[1]),Math.abs(x.i[2]))||1;\n"
" var s=`<svg class='phsvg' viewBox='0 0 260 260'><defs><marker id='${mk}' markerWidth='8' markerHeight='8' refX='6' refY='3' orient='auto'><path d='M0,0 L6,3 L0,6 Z' class='ph-mk'/></marker></defs>`;\n"
" s+=`<circle cx='${cx}' cy='${cy}' r='${R}' class='ph-bg'/><circle cx='${cx}' cy='${cy}' r='55' class='ph-grid'/>`;\n"
" for(a=0;a<360;a+=30){r=a*Math.PI/180;s+=`<line x1='${cx}' y1='${cy}' x2='${(cx+R*Math.cos(r)).toFixed(1)}' y2='${(cy-R*Math.sin(r)).toFixed(1)}' class='ph-grid'/>`;}\n"
" [[0,'0°'],[120,'120°'],[240,'240°']].forEach(function(m){var mr=m[0]*Math.PI/180,tx=(cx+(R+13)*Math.cos(mr)).toFixed(1),ty=(cy-(R+13)*Math.sin(mr)+3).toFixed(1);s+=`<text x='${tx}' y='${ty}' text-anchor='middle' fill='#8b9bb0' font-size='10'>${m[1]}</text>`;});\n"
" for(k=0;k<3;k++)s+=phVec(cx,cy,x.uang[k],R*0.92*Math.abs(x.u[k])/um,PHV[k],mk,0);\n"
" for(k=0;k<3;k++)s+=phVec(cx,cy,x.iang[k],R*0.6*Math.abs(x.i[k])/im,PHI[k],mk,1);\n"
" var ix=0,iy=0;for(k=0;k<3;k++){var rr=x.iang[k]*Math.PI/180,mm=Math.abs(x.i[k]);ix+=mm*Math.cos(rr);iy+=mm*Math.sin(rr);}var iM=Math.sqrt(ix*ix+iy*iy);if(iM>0.001)s+=phVec(cx,cy,Math.atan2(-iy,-ix)*180/Math.PI,R*0.6*iM/im,PHI[3],mk,1);\n"
" return s+'</svg>';}\n"
"function bPhase(x){var PL=['#B5651D','#FFFFFF','#7E8896','#1E64FF'],lg=['U1','U2','U3','In'].map(function(l,i){return \"<span><i style='background:\"+PL[i]+\"'></i>\"+l+\"</span>\";}).join('');var h=\"<div class='phbox'><div class='phleg'>\"+lg+\"</div>\"+phasorSVG(x)+\"</div><table class='ctbl'><tr><th></th><th>L1</th><th>L2</th><th>L3</th><th></th></tr>\";\n"
" h+=r3('U mag (V)',x.u,1)+r3('U angle (&deg;)',x.uang,1)+r3('I mag (A)',x.i,2)+r3('I angle (&deg;)',x.iang,1);return h+'</table>';}\n"
"function bMm(x){var h=clrBtn('clrMinmax',x.n)+`<div class='cmeta'>Reset: ${ts2(x.reset_ts)}</div><table class='ctbl mmtbl'><tr><th>Metric</th><th></th><th>Value</th><th>Time</th></tr>`;\n"
" x.m.forEach(function(m){h+=`<tr><td class='rk' rowspan='2'>${m.nm}</td><td class='mk'>Max</td><td>${n(m.mx,2)}</td><td class='ts'>${ts2(m.mxt)}</td></tr><tr><td class='mk'>Min</td><td>${n(m.mn,2)}</td><td class='ts'>${ts2(m.mnt)}</td></tr>`;});return h+'</table>';}\n"
"function bEgyNow(x,ed){var C=[['import_kwh','Imp kWh'],['export_kwh','Exp kWh'],['import_kvarh','Imp kVarh'],['export_kvarh','Exp kVarh'],['kvah','kVAh']];\n"
" var h=\"<table class='ctbl hsm'><tr><th></th>\";C.forEach(function(c){h+='<th>'+c[1]+'</th>';});h+='</tr>';\n"
" x.periods.forEach(function(pr,pi){h+=`<tr><td class='rk' colspan='6' style='color:var(--accent)'>${pr.name}</td></tr>`;pr.groups.forEach(function(g,gi){h+=`<tr><td class='rk'>${g.name}</td>`;C.forEach(function(c,ti){var v=g[c[0]];if(ed){var a=240+pi*48+gi*12+ti*2+(x.n-1)*10000;h+=\"<td><input class='ein' data-a='\"+a+\"' data-o='\"+n(v,1)+\"' value='\"+n(v,1)+\"' style='width:58px'></td>\";}else h+='<td>'+n(v,1)+'</td>';});h+='</tr>';});});return h+'</table>';}\n"
"function bEgyLog(lg){var h=`<div class='hsub'>Hourly kWh (This ${n(lg.this_total,1)} / Last ${n(lg.last_total,1)})</div><table class='ctbl hsm'><tr><th>H</th><th>This</th><th>Last</th></tr>`;\n"
" for(var k=0;k<24;k++){h+=`<tr><td class='rk'>${k}</td><td>${n(lg.this[k],2)}</td><td>${n(lg.last[k],2)}</td></tr>`;}return h+'</table>';}\n"
"function drawDemand(id,vals,peak){var cv=$(id);if(!cv)return;var w=cv.clientWidth||300;cv.width=w;cv.height=200;var g=cv.getContext('2d'),pl=36,pb=18,pt=10,nn=96,ymax=(peak>0?peak:1)*1.15,i;\n"
" var gy=function(v){return 200-pb-(v/ymax)*(200-pt-pb);};g.clearRect(0,0,w,200);\n"
" g.strokeStyle='rgba(128,140,155,.25)';g.fillStyle='#8b9bb0';g.font='8px sans-serif';\n"
" for(var s=0;s<=4;s++){var yv=ymax*s/4,y=gy(yv);g.beginPath();g.moveTo(pl,y);g.lineTo(w,y);g.stroke();g.fillText(yv.toFixed(yv<10?1:0),2,y+3);}\n"
" var slot=(w-pl)/nn,bw=slot>1.5?slot*0.7:slot,y0=200-pb;\n"
" for(i=0;i<nn;i++){var v=vals[i]||0;g.fillStyle=(peak>0&&v>=peak-1e-6)?'#ef4444':'rgba(59,130,246,.65)';g.fillRect(pl+i*slot,gy(v),bw,y0-gy(v));}\n"
" g.fillStyle='#8b9bb0';[0,6,12,18,24].forEach(function(hh){g.fillText(hh+'h',pl+hh*4*slot-4,199);});}\n"
/* Waveform / Report(EN50160+ITIC) / Monthly */
"function drawLines(id,series){var cv=$(id);if(!cv)return;var w=cv.clientWidth||300;cv.width=w;cv.height=160;var g=cv.getContext('2d'),pad=30,L=series[0].length,mx=-1e9,mn=1e9,si,k;\n"
" for(si=0;si<3;si++)for(k=0;k<L;k++){var v=series[si][k];if(v>mx)mx=v;if(v<mn)mn=v;}if(mx===mn){mx+=1;mn-=1;}\n"
" g.clearRect(0,0,w,160);var gy=function(v){return 160-pad-(v-mn)/(mx-mn)*(160-2*pad);};\n"
" g.strokeStyle='rgba(128,140,155,.2)';g.fillStyle='#8b9bb0';g.font='8px sans-serif';\n"
" [0,0.5,1].forEach(function(f){var yv=mn+(mx-mn)*f,y=gy(yv);g.beginPath();g.moveTo(pad,y);g.lineTo(w,y);g.stroke();g.fillText(yv.toFixed(0),2,y+3);});\n"
" for(si=0;si<3;si++){g.strokeStyle=PCOL[si];g.lineWidth=1.4;g.beginPath();for(k=0;k<L;k++){var x=pad+(w-pad-6)*k/(L-1),y=gy(series[si][k]);if(k===0)g.moveTo(x,y);else g.lineTo(x,y);}g.stroke();}}\n"
"var EN_ROWS=[['Frequency Variation 1','(+1%~-1%),99.5%/Wk',99.5],['Frequency Variation 2','(+4%~-6%),100%/Wk',100],['Voltage Variation 1','(+10%~-10%),95%/Wk',95],['Voltage Variation 2','(+10~-15%),100%/Wk',100],['Voltage Unbalance','(<2%),100%/Wk',100],['THD','(<8%),95%/Wk',95],['Harmonics','(0.5%~6%),95%/Wk',95],['Plt','(1),95%/Wk',95]],EN_INFO=['Voltage Sag','Voltage Swell','Short Interruption','Signaling Volt.'];\n"
"function passOf(v,thr){var ok=true,any=false,i;for(i=0;i<3;i++){if(v[i]!=null){any=true;if(v[i]<thr)ok=false;}}return (any&&ok)?1:0;}\n"
"function enNv(v){return (v==null)?'-':Number(v).toFixed(1);}\n"
"function bVq(rows){var h=\"<div class='chcard'><div class='chh'>CH1 · Voltage Quality</div><table class='ctbl'><tr><th>Metric</th><th>L1</th><th>L2</th><th>L3</th></tr>\";(rows||[]).forEach(function(r){h+=\"<tr><td class='rk'>\"+r.name+(r.unit?' ('+r.unit+')':'')+\"</td><td>\"+(+r.l[0]).toFixed(3)+\"</td><td>\"+(+r.l[1]).toFixed(3)+\"</td><td>\"+(+r.l[2]).toFixed(3)+\"</td></tr>\";});return h+'</table></div>';}\n"
"function bReport(x){var okAll=x.rows.every(function(r,ri){return passOf(r.v,EN_ROWS[ri][2])===1;});\n"
" var h=`<div class='cmeta'>${ts2(x.start)} ~ ${ts2(x.end)} · <span class='enb ${okAll?'pass':'fail'}'>${okAll?'Compliant':'Non-compliant'}</span></div><table class='ctbl hsm'><tr><th>Parameter</th><th>L1</th><th>L2</th><th>L3</th><th>Comp</th><th>Requirement</th></tr>`;\n"
" x.rows.forEach(function(r,ri){var p=passOf(r.v,EN_ROWS[ri][2]);h+=`<tr><td class='rk'>${EN_ROWS[ri][0]}</td><td>${enNv(r.v[0])}</td><td>${enNv(r.v[1])}</td><td>${enNv(r.v[2])}</td><td><span class='enb ${p?'pass':'fail'}'>${p?'Pass':'Fail'}</span></td><td class='ph2'>${EN_ROWS[ri][1]}</td></tr>`;});\n"
" x.info.forEach(function(v,k){h+=`<tr><td class='rk'>${EN_INFO[k]}</td><td>${n(v,k<3?0:1)}</td><td>-</td><td>-</td><td><span class='enb info'>Info</span></td><td class='ph2'>Info Only</td></tr>`;});\n"
" return h+`</table><div class='hsub'>ITIC (CBEMA) Curve</div><canvas id='it${x.n}' class='itcv'></canvas>`;}\n"
"var ITIC_UP=[[0.0001,500],[0.001,200],[0.003,140],[0.003,120],[10,120],[10,110],[100,110]],ITIC_LO=[[0.0001,0],[0.02,0],[0.02,70],[0.5,70],[0.5,80],[10,80],[10,90],[100,90]];\n"
"function drawITIC(id){var cv=$(id);if(!cv)return;var w=cv.clientWidth||400;cv.width=w;cv.height=240;var g=cv.getContext('2d'),pl=40,pr=10,pt=12,pb=22,h=240;\n"
" var X=function(t){return pl+(Math.log(t)/Math.LN10+4)/6*(w-pl-pr);},Y=function(p){return h-pb-(p/500)*(h-pt-pb);};\n"
" g.clearRect(0,0,w,h);g.strokeStyle='rgba(128,140,155,.22)';g.fillStyle='#8b9bb0';g.font='8px sans-serif';\n"
" for(var e=-4;e<=2;e++){var x=X(Math.pow(10,e));g.beginPath();g.moveTo(x,pt);g.lineTo(x,h-pb);g.stroke();g.fillText('1e'+e,x-7,h-pb+9);}\n"
" for(var p=0;p<=500;p+=100){var y=Y(p);g.beginPath();g.moveTo(pl,y);g.lineTo(w-pr,y);g.stroke();g.fillText(p,2,y+3);}\n"
" function poly(pp,col){g.strokeStyle=col;g.lineWidth=2;g.beginPath();pp.forEach(function(q,i){var x=X(q[0]),y=Y(q[1]);if(i===0)g.moveTo(x,y);else g.lineTo(x,y);});g.stroke();}\n"
" poly(ITIC_UP,'#3b82f6');poly(ITIC_LO,'#ef4444');}\n"
"function mGroup(nm){if(/^(U\\d|U THD|U Unbal)/.test(nm))return 'V';if(/^(I\\d|In|I THD|I TDD|K-factor|I Unbal)/.test(nm))return 'I';if(/^temperature/i.test(nm))return 'T';return 'P';}\n"
"function munit(nm){if(/thd|tdd|unbal/i.test(nm))return '%';if(/^temperature/i.test(nm))return '\\u2103';if(/^U/.test(nm))return 'V';if(/^(I|In)/.test(nm))return 'A';if(nm==='P')return 'kW';if(nm==='Q')return 'kVar';if(nm==='S')return 'kVA';return '';}\n"
"function bMonthly(x){var G={V:[],I:[],P:[],T:[]},k;for(k in x.s){G[mGroup(k)].push([k,x.s[k]]);}\n"
" function col(t,arr){var h=\"<div class='mcol'><div class='hsub'>\"+t+'</div>';arr.forEach(function(kv){h+=`<div class='srow'><span class='sk'>${kv[0]}</span><span class='sv'>${n(kv[1],2)} ${munit(kv[0])}</span></div>`;});return h+'</div>';}\n"
" var h=\"<div class='mcols'>\"+col('Voltage',G.V)+col('Current',G.I)+col('Power / Temp',G.P.concat(G.T))+'</div>';\n"
" h+=\"<div class='hsub'>Harmonics (% · orders 2\\u201325)</div><table class='ctbl hsm'><tr><th>Ord</th>\";x.harm.forEach(function(s){h+='<th>'+s.label+'</th>';});h+='</tr>';\n"
" for(var o=0;o<24;o++){h+=\"<tr><td class='rk'>\"+(o+2)+'</td>';x.harm.forEach(function(s){var v=s.vals[o];h+='<td>'+(v?n(v,1):'\\u00b7')+'</td>';});h+='</tr>';}return h+'</table>';}\n"
"function harmTable(x,ord){var h=\"<table class='ctbl hsm'><tr><th>Ord</th><th>L1</th><th>L2</th><th>L3</th></tr>\";ord.forEach(function(o,k){h+=`<tr><td class='rk'>${o}</td><td>${n(x.ph[0][k],1)}</td><td>${n(x.ph[1][k],1)}</td><td>${n(x.ph[2][k],1)}</td></tr>`;});return h+'</table>';}\n"
"function drawHarm(id,ph,ord){var cv=$(id);if(!cv)return;var w=cv.clientWidth||300;cv.width=w;cv.height=180;var g=cv.getContext('2d'),pad=22,nn=ord.length,mx=1,i,p;\n"
" for(p=0;p<3;p++)for(i=0;i<nn;i++){if(ph[p][i]>mx)mx=ph[p][i];}\n"
" g.clearRect(0,0,w,180);var gw=(w-pad-4)/nn,bw=gw/3;\n"
" for(i=0;i<nn;i++){for(p=0;p<3;p++){var v=ph[p][i]||0,bh=(180-2*pad)*(v>0?v:0)/mx;g.fillStyle=PCOL[p];g.fillRect(pad+i*gw+p*bw,180-pad-bh,bw>1?bw-0.5:bw,bh);}\n"
"  if(ord[i]%5===0||ord[i]===2){g.fillStyle='#8b9bb0';g.font='8px sans-serif';g.fillText(ord[i],pad+i*gw,178);}}\n"
" g.strokeStyle='rgba(128,140,155,.3)';g.beginPath();g.moveTo(pad,180-pad);g.lineTo(w,180-pad);g.stroke();}\n"
"function syncHv(){var a=$('hvt'),b=$('hvc');if(a)a.className='btn'+(harmView==='table'?' primary':'');if(b)b.className='btn'+(harmView==='chart'?' primary':'');}\n"
"function bAlarm(x,ev){var st=ev.status.filter(function(s){return s.chn===x.n;}),lg=ev.alarmlog.filter(function(s){return s.chn===x.n;});\n"
" var h=`<div class='hsub'>Alarm Status <span class='ph2'>(${st.length} active)</span></div><table class='ctbl'><tr><th>Alarm Channel</th><th>State</th><th>Count</th></tr>`;\n"
" if(!st.length)h+=\"<tr><td colspan='3' class='empty'>No active alarms</td></tr>\";st.forEach(function(s){h+=`<tr><td class='rk'>${esc(acn(s.chan))}</td><td><span class='st set'>Set</span></td><td>${s.count}</td></tr>`;});\n"
" h+=\"</table><div class='hsub'>Alarm Log</div>\"+pbar('pgAlarm',x.n,'clrAlarm','ackAlarm');\n"
" h+=\"<table class='ctbl'><tr><th>Channel</th><th>State</th><th>Value</th><th>Time</th></tr>\";\n"
" if(!lg.length)h+=\"<tr><td colspan='4' class='empty'>No alarm log</td></tr>\";lg.forEach(function(s){h+=`<tr><td class='rk'>${esc(acn(s.chan))}</td><td><span class='st ${s.set?'set':'clr'}'>${s.set?'Set':'Clear'}</span></td><td>${n(s.value,2)}</td><td class='ts'>${ts2(s.ts)}</td></tr>`;});return h+'</table>';}\n"
"function bEvent(x,ev){var el=ev.eventlog.filter(function(s){return s.chn===x.n;});\n"
" var h=pbar('pgEvent',x.n,'clrEvent','ackEvent');\n"
" h+=\"<table class='ctbl'><tr><th>Type</th><th>Start Time</th><th>Dur(ms)</th><th>Phase</th><th>Level</th></tr>\";\n"
" if(!el.length)h+=\"<tr><td colspan='5' class='empty'>No events</td></tr>\";el.forEach(function(e){var t=ET[e.type]||['#'+e.type,'oc'],ph=[];if(e.mask&1)ph.push('L1');if(e.mask&2)ph.push('L2');if(e.mask&4)ph.push('L3');h+=`<tr><td><span class='evt ${t[1]}'>${t[0]}</span></td><td class='ts'>${ts2(e.ts)}</td><td>${e.dur}</td><td>${ph.join(',')||'-'}</td><td>${n(e.level,2)}</td></tr>`;});return h+'</table>';}\n"
"function wrCmd(a,v){if(!IS_ADMIN){alert('Admin only');return;}jp('/api/setreg',{addr:a,type:'u16',val:v}).then(function(){setTimeout(pollLoop,300);});}\n"
"function pgAlarm(n,c){wrCmd(7490+n,c);}\n"
"function clrAlarm(n){if(confirm('Clear Alarm CH'+n+'?'))wrCmd(7478+n,4660);}\n"
"function pgEvent(n,c){wrCmd(7486+n,c);}\n"
"function clrEvent(n){if(confirm('Clear Event CH'+n+'?'))wrCmd(7482+n,4660);}\n"
"function pgItic2(n,c){wrCmd(7503,c);}\n"
"function clrBtn(fn,n){return IS_ADMIN?\"<div class='pgctl'><button class='btn warn' onclick='\"+fn+\"(\"+n+\")'>Clear</button></div>\":'';}\n"
"function clrMinmax(n){if(confirm('Clear Min/Max CH'+n+'?'))wrCmd(7470+n,4660);}\n"
"function clrEnergy(n){if(confirm('Clear Energy CH'+n+'?'))wrCmd(7474+n,4660);}\n"
"function clrDemand(n){if(confirm('Clear Demand CH'+n+'?'))wrCmd(7466+n,4660);}\n"
"function ackAlarm(n){wrCmd(7494+n,4660);}\n"
"function ackEvent(n){wrCmd(7498+n,4660);}\n"
"function clearPI(){if(confirm('Clear PI?'))wrCmd(7465,4660);}\n"
"var egyEd=0,egyD=null;\n"
"function egyRender(){if(!egyD)return;var b=IS_ADMIN?(egyEd?\"<div class='pgctl'><button class='btn primary' onclick='egyWr()'>Write</button><button class='btn' onclick='egyEd=0;egyRender()'>Cancel</button></div>\":\"<div class='pgctl'><button class='btn' onclick='egyEd=1;egyRender()'>Edit</button></div>\"):'';$('chbody').innerHTML=chCards(egyD.now,function(x){var lg=egyD.log.filter(function(e){return e.n===x.n;})[0];return b+(egyEd?'':clrBtn('clrEnergy',x.n))+bEgyNow(x,egyEd)+(lg?bEgyLog(lg):'');});}\n"
"function egyWr(){var els=document.querySelectorAll('#chbody .ein'),c=[],i=0,chs={};els.forEach(function(e){if(e.value!==e.getAttribute('data-o'))c.push(e);});if(!c.length){egyEd=0;egyRender();return;}(function w(){if(i>=c.length){Object.keys(chs).forEach(function(ch){wrCmd(7474+parseInt(ch),0x5678);});if(confirm('Save energy edit to FLASH now? (OK=persist via Save Set / Cancel=RAM only, revert on reboot)'))setTimeout(function(){wrCmd(7449,0x1234);},1200);egyEd=0;setTimeout(pollLoop,400);return;}var e=c[i++],v=Math.round(parseFloat(e.value)*10);if(!isFinite(v)||v<0)v=0;var a=+e.getAttribute('data-a');chs[Math.floor(a/10000)+1]=1;jp('/api/setreg',{addr:a,type:'u32',val:v}).then(w).catch(w);})();}\n"
"function pbar(fn,n,cf,af){if(!IS_ADMIN)return '';var s=\"<div class='pgctl'>\",L=[[3,'&#8593;&#8593; Top'],[2,'&#8593; Up'],[1,'&#8595; Down'],[4,'&#8595;&#8595; Bottom']];L.forEach(function(t){s+=\"<button class='btn' onclick='\"+fn+\"(\"+n+\",\"+t[0]+\")'>\"+t[1]+\"</button>\";});if(af)s+=\"<button class='btn' onclick='\"+af+\"(\"+n+\")'>Ack</button>\";if(cf)s+=\"<button class='btn warn' onclick='\"+cf+\"(\"+n+\")'>Clear</button>\";return s+'</div>';}\n"
"function bItic(list,title,fn){var h=\"<div class='hsub'>\"+title+\"</div>\"+pbar(fn,1)+\"<table class='ctbl'><tr><th>Type</th><th>Start Time</th><th>Dur(ms)</th><th>Phase</th><th>Level</th></tr>\";\n"
" if(!list||!list.length)h+=\"<tr><td colspan='5' class='empty'>No data</td></tr>\";(list||[]).forEach(function(e){var t=ET[e.type]||['#'+e.type,'oc'],ph=[];if(e.mask&1)ph.push('L1');if(e.mask&2)ph.push('L2');if(e.mask&4)ph.push('L3');h+=\"<tr><td><span class='evt \"+t[1]+\"'>\"+t[0]+\"</span></td><td class='ts'>\"+ts2(e.ts)+\"</td><td>\"+e.dur+\"</td><td>\"+(ph.join(',')||'-')+\"</td><td>\"+n(e.level,2)+\"</td></tr>\";});return h+'</table>';}\n"
"function loadChannelTab(){var t=chTabCur;\n"
" if(t==='meter'||t==='phase'){var fn=(t==='meter'?bMeter:bPhase);return j('/api/channels').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chErr(fn);return;}chStat(true);lastCh=r.d.ch;$('chbody').innerHTML=chCards(r.d.ch,fn);}).catch(function(){if(chTabCur===t)chErr(fn);});}\n"
" if(t==='minmax')return j('/api/minmax').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chErr(bMm);return;}chStat(true);lastCh=r.d.ch;$('chbody').innerHTML=chCards(r.d.ch,bMm);}).catch(function(){if(chTabCur==='minmax')chErr(bMm);});\n"
" if(t==='energy'){if(egyEd)return Promise.resolve();return j('/api/energynow').then(function(r){if(chTabCur!==t||egyEd)return null;if(!r.d.ok){chStat(false);return null;}return j('/api/energylog').then(function(r2){return{now:r.d.ch,log:(r2.d&&r2.d.ok)?r2.d.ch:[]};});}).then(function(d){if(!d||chTabCur!=='energy'||egyEd)return;egyD=d;chStat(true);egyRender();}).catch(chFail);}\n"
" if(t==='demand')return j('/api/demandlog').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);\n"
"  $('chbody').innerHTML=chCards(r.d.ch,function(x){var pm=x.peaki*15,ph=Math.floor(pm/60),pmm=pm%60;return clrBtn('clrDemand',x.n)+`<div class='cmeta'>peak ${n(x.peak,2)} kW @ ${p2(ph)}:${p2(pmm)} · avg ${n(x.avg,2)} kW · ${ts2(x.ts)}</div><canvas id='dm${x.n}' class='dmcv'></canvas>`;});\n"
"  r.d.ch.forEach(function(x){drawDemand('dm'+x.n,x.vals,x.peak);});\n"
" }).catch(chFail);\n"
" if(t==='harmonics')return j('/api/harmonics?opt='+harmOpt).then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);\n"
"  if(harmOpt==='cur'){$('chbody').innerHTML=chCards(r.d.ch,function(x){return \"<div class='hsub'>Chart</div><canvas id='hc\"+x.n+\"' class='hcv'></canvas><div class='hsub' style='margin-top:10px'>Table</div>\"+harmTable(x,r.d.orders);});r.d.ch.forEach(function(x){drawHarm('hc'+x.n,x.ph,r.d.orders);});return;}\n"
"  var x=r.d.ch.filter(function(c){return c.n===1;})[0];\n"
"  if(!x){$('chbody').innerHTML=\"<p class='stub'>no data</p>\";return;}\n"
"  $('chbody').innerHTML=\"<div class='chcard'><div class='chh'>CH1</div><div class='hsub'>Chart</div><canvas id='hc1' class='hcv'></canvas><div class='hsub' style='margin-top:10px'>Table</div>\"+harmTable(x,r.d.orders)+\"</div>\";\n"
"  drawHarm('hc1',x.ph,r.d.orders);\n"
" }).catch(chFail);\n"
" if(t==='waveform')return j('/api/waveform').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);\n"
"  $('chbody').innerHTML=chCards(r.d.channels,function(x){return \"<div class='hsub'>Voltage</div><canvas id='wv\"+x.n+\"' class='wcv'></canvas><div class='hsub'>Current</div><canvas id='wi\"+x.n+\"' class='wcv'></canvas>\";});\n"
"  r.d.channels.forEach(function(x){drawLines('wv'+x.n,x.v);drawLines('wi'+x.n,x.i);});}).catch(chFail);\n"
" if(t==='report')return j('/api/en50160').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);var x=r.d.ch[0];if(!x)return;$('chbody').innerHTML=\"<div class='chcard'><div class='chh'>CH\"+x.n+\" · EN50160</div>\"+bReport(x)+bItic(r.d.itic2,'ITIC List (Voltage)','pgItic2')+'</div>';drawITIC('it'+x.n);}).catch(chFail);\n"
" if(t==='vq')return j('/api/vq').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);$('chbody').innerHTML=bVq(r.d.rows);}).catch(chFail);\n"
" if(t==='monthly')return j('/api/monthly').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);$('chbody').innerHTML=chCards(r.d.ch,bMonthly);}).catch(chFail);\n"
" if(t==='alarm'||t==='event')return j('/api/sv300/events').then(function(r){if(chTabCur!==t)return;if(!r.d.ok){chStat(false);return;}chStat(true);$('chbody').innerHTML=chCards(CHN,function(x){return (t==='alarm'?bAlarm:bEvent)(x,r.d);});}).catch(chFail);\n"
" return Promise.resolve();}\n"
"function runCmd(ci){var c=CMDS[ci],id='c'+ci;\n"
" if(c.utc){var v=$('u_'+id).value;if(!v){alert('Enter time');return;}var m=v.split(/[-T:]/),ep=Math.floor(Date.UTC(+m[0],+m[1]-1,+m[2],+m[3],+m[4],+m[5]||0)/1000);if(!confirm('Set Time to '+v.replace('T',' ')+' ?'))return;jp('/api/setreg',{addr:c.addr,type:'u32',val:ep}).then(function(r){cmdDone(r,c);});return;}\n"
" var addr=c.addr+(c.tgt?(+$('t_'+id).value):0),msg=c.l+' run?';if(c.danger)msg=c.l+' - '+(c.danger==2?'Factory reset':(c.note==='reboot'?'Device will reboot':'Irreversible'))+'. Continue?';\n"
" if(!confirm(msg))return;if(c.danger==2&&!confirm('Really reset?'))return;\n"
" jp('/api/setreg',{addr:addr,type:'u16',val:4660}).then(function(r){cmdDone(r,c);});}\n"
/* setup: Channel Setting - Alarm Settings 32행 */
"function u16toF(lo,hi){var b=new ArrayBuffer(4),d=new DataView(b);d.setUint16(0,lo,true);d.setUint16(2,hi,true);return d.getFloat32(0,true);}\n"
"function fToU16(f){var b=new ArrayBuffer(4),d=new DataView(b);d.setFloat32(0,f,true);return [d.getUint16(0,true),d.getUint16(2,true)];}\n"
"function fnum(x){return String(Math.round(x*100)/100);}\n"
"var alCh=1,COND_OPT=\"<option value='0'>&lt;</option><option value='1'>&gt;</option>\",ACT_OPT=\"<option value='0'>None</option><option value='1'>Relay</option><option value='2'>Beep</option>\";\n"
"function alSel(c){alCh=c;loadAlarm();}\n"
"function alSelCell(addr,val,opts,cls){return \"<select class='\"+cls+\"' data-addr='\"+addr+\"' data-type='u16' data-orig='\"+val+\"' data-val='\"+val+\"' \"+(IS_ADMIN?'':'disabled')+\" onchange='setMark(this)'>\"+opts+'</select>';}\n"
"function alNum(addr,val){return \"<input type='number' data-addr='\"+addr+\"' data-type='u16' data-orig='\"+val+\"' value='\"+val+\"' \"+(IS_ADMIN?'':'disabled')+\" oninput='setMark(this)'>\";}\n"
"function alFloat(addr,val){var s=fnum(val);return \"<input type='number' step='0.1' data-addr='\"+addr+\"' data-type='float' data-orig='\"+s+\"' value='\"+s+\"' \"+(IS_ADMIN?'':'disabled')+\" oninput='setMark(this)'>\";}\n"
"function loadAlarm(){var base=(alCh-1)*10000+6858;\n"
" return j('/api/regs?addr='+base+'&n=192').then(function(r){if(!r.d.ok){$('setBody').innerHTML=\"<p class='stub'>load error</p>\";return;}var w=r.d.words;\n"
"  var h=\"<div style='margin-bottom:12px'>\";for(var c=1;c<=NCH;c++)h+=\"<button class='btn\"+(c===alCh?' primary':'')+\"' style='margin-right:6px' onclick='alSel(\"+c+\")'>CH\"+c+'</button>';h+='</div>';\n"
"  var acOpt='';for(var i=0;i<AC.length;i++)acOpt+=\"<option value='\"+i+\"'>\"+esc(AC[i])+'</option>';\n"
"  h+=\"<div style='overflow-x:auto'><table class='atbl'><tr><th>#</th><th>Channel</th><th>Cond</th><th>Hyst(%)</th><th>Action</th><th>Level</th></tr>\";\n"
"  for(var n=0;n<32;n++){var b=base+6*n,o=6*n,chan=w[o],en=chan>0;\n"
"   h+=\"<tr class='\"+(en?'aon':'aoff')+\"'><td>\"+(n+1)+'</td><td>'+alSelCell(b,chan,acOpt,'awide')+'</td><td>'+alSelCell(b+1,w[o+1],COND_OPT,'')+'</td><td>'+alNum(b+2,w[o+2])+'</td><td>'+alSelCell(b+3,w[o+3],ACT_OPT,'')+'</td><td>'+alFloat(b+4,u16toF(w[o+4],w[o+5]))+'</td></tr>';}\n"
"  h+='</table></div>';$('setBody').innerHTML=h;\n"
"  document.querySelectorAll('#setBody select[data-val]').forEach(function(s){s.value=s.getAttribute('data-val');});\n"
"  sn('Channel>0=on. Save & Apply.');});}\n"
/* boot */
"var _busy=false,_tick=0,_busyT=0;\n"
"function pollFin(){_busy=false;_tick++;}\n"
"function pollLoop(){if(_busy){if(Date.now()-_busyT>8000){_busy=false;}else{return;}}if(window.__pollOn===false)return;_busyT=Date.now();\n"
" if(cur==='dash'){_busy=true;loadDash().then(function(){if(_tick%3===0)return loadEvents();}).catch(function(){}).then(pollFin);}\n"
" else if(cur==='feeder'){_busy=true;loadFeeder().catch(function(){}).then(pollFin);}\n"
" else if(cur==='channel'){_busy=true;loadChannelTab().catch(function(){}).then(pollFin);}\n"
" else if(cur==='io'){_busy=true;loadIom().catch(function(){}).then(pollFin);}\n"
"}\n"
"tbtn();clk();setInterval(clk,1000);setInterval(pollLoop,1000);me();\n"
"</script></body></html>\n";

/* 임베디드 SPA 서빙(고정 길이) */
static error_t serveIndex(HttpConnection *c)
{
	error_t e;
	httpInitResponseHeader(c);
	c->response.contentType   = "text/html";
	c->response.contentLength = sizeof(INDEX_HTML) - 1;
	c->response.keepAlive     = FALSE;
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
	if (!strcmp(uri, "/api/setreg"))        return apiSetReg(c);
	if (!strcmp(uri, "/api/savecfg"))       return apiSaveCfg(c);
	if (!strcmp(uri, "/api/fwupload"))      return apiFwUpload(c);
	if (!strcmp(uri, "/api/fwstatus"))      return apiFwStatus(c);
	if (!strcmp(uri, "/api/fwapply"))       return apiFwApply(c);

	/* 조회(로그인 필요) */
	if (!strncmp(uri, "/api/", 5)) {
		if (webRole(c) == ROLE_NONE)
			return webJsonStatus(c, 401, "{\"ok\":false,\"error\":\"auth\"}");
		if (!strcmp(uri, "/api/dashboard"))    return apiDashboard(c);
		if (!strcmp(uri, "/api/sv300/events")) return apiEvents(c);
		if (!strcmp(uri, "/api/feeders"))      return apiFeeders(c);
		if (!strcmp(uri, "/api/channels"))     return apiChannels(c);
		if (!strcmp(uri, "/api/minmax"))       return apiMinmax(c);
		if (!strcmp(uri, "/api/energynow"))    return apiEnergyNow(c);
		if (!strcmp(uri, "/api/energylog"))    return apiEnergyLog(c);
		if (!strcmp(uri, "/api/demandlog"))    return apiDemandLog(c);
		if (!strcmp(uri, "/api/harmonics"))    return apiHarmonics(c);
		if (!strcmp(uri, "/api/waveform"))     return apiWaveform(c);
		if (!strcmp(uri, "/api/en50160"))      return apiEn50160(c);
		if (!strcmp(uri, "/api/vq"))           return apiVq(c);
		if (!strcmp(uri, "/api/monthly"))      return apiMonthly(c);
		if (!strcmp(uri, "/api/general"))      return apiGeneral(c);
		if (!strncmp(uri, "/api/regs", 9))     return apiRegs(c);
		return webJsonStatus(c, 404, "{\"ok\":false,\"error\":\"notfound\"}");
	}

	return ERROR_NOT_FOUND;
}

/*----------------------------------------------------------------------------
 * mDNS responder — http://sv300-<SN 하위3바이트>.local/
 *----------------------------------------------------------------------------*/
static void webMdnsStart(NetInterface *interface)
{
	MdnsResponderSettings s;
	char host[24];
	error_t e;

	mdnsResponderGetDefaultSettings(&s);
	s.interface = interface;
	e = mdnsResponderInit(&webMdns, &s);
	if (e) { printf("[WEB] mDNS init failed (%d)\n", (int)e); return; }

	/* SN(Serial Number, pcal->sn) 하위 4바이트 기반 호스트명 (3바이트는 타제품과 중복 우려) */
	snprintf(host, sizeof(host), "sv300-%08x", (unsigned)pcal->sn[1]);
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
	printf("[WEB] HTTP on :80 (login: ntek/0300, sv300/0000)\n");

	webMdnsStart(interface);
}
