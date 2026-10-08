/*----------------------------------------------------------------------------
 * SV300 Web Dashboard — CycloneTCP HTTP 서버 (포트 80)
 *  - 정적 SPA(index.html)는 펌웨어 임베드 + S0:(SPI Flash) FS 폴백
 *  - 실시간 계측은 *.cgx JSON 엔드포인트(gems3500 웹서비스 계약 참고)
 *----------------------------------------------------------------------------*/
#ifndef WEB_H
#define WEB_H

#include "core/net.h"

/* main.c 네트워크 초기화 뒤에서 호출. 계측 준비(g_meterReady)를 기다리지 않고 즉시 기동한다
 * — 웹은 데이터 확인용이라 빠른 접속이 우선(Modbus는 종전대로 계측 준비 후에만 응답).
 * 계측 방해 방지 위해 LOW 우선순위 태스크로 구동 */
void webServerStart(NetInterface *interface);

/* mDNS 이름 광고 — 웹서버와 함께 네트워크 기동 직후 호출.
 * 부팅 중 이름 조회가 실패하면 PC가 그 실패를 캐시해 장비 복귀 후에도 몇 분간
 * ERR_NAME_NOT_RESOLVED가 지속되므로, 이름은 늦게 띄우지 말 것. */
void webMdnsStart(NetInterface *interface);

/* 장치 호스트명 "sv300-<SN>" — mDNS·인터페이스 호스트명(LLMNR/DHCP)·SNMP sysName 공용.
 * pcal(db_init)이 로드된 뒤 호출할 것 */
void webGetHostname(char *buf, size_t size);

/* SNMP sysDescr 문자열 "SV300 FW Vxx.xx (YY/MM/DD)" — setMeterInfo(db_init) 뒤 호출 */
void webGetSysDescr(char *buf, size_t size);

#endif /* WEB_H */
