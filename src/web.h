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

#endif /* WEB_H */
