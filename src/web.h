/*----------------------------------------------------------------------------
 * SV300 Web Dashboard — CycloneTCP HTTP 서버 (포트 80)
 *  - 정적 SPA(index.html)는 펌웨어 임베드 + S0:(SPI Flash) FS 폴백
 *  - 실시간 계측은 *.cgx JSON 엔드포인트(gems3500 웹서비스 계약 참고)
 *----------------------------------------------------------------------------*/
#ifndef WEB_H
#define WEB_H

#include "core/net.h"

/* main.c 네트워크 초기화 뒤에서 호출. 계측 방해 방지 위해 LOW 우선순위 태스크로 구동 */
void webServerStart(NetInterface *interface);

/* mDNS 이름 광고 — 웹서버(g_meterReady 게이트)보다 먼저 기동해야 한다.
 * 부팅 중 이름 조회가 실패하면 PC가 그 실패를 캐시해 장비 복귀 후에도 몇 분간
 * ERR_NAME_NOT_RESOLVED가 지속된다. 이름만 먼저 살려두면 그 구간이 '연결 거부'
 * (캐시되지 않음)로 바뀌어 웹서버 기동과 동시에 복구된다. */
void webMdnsStart(NetInterface *interface);

#endif /* WEB_H */
