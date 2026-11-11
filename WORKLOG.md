# SV300 Work Log

대상: SV300 — LPC4330/4357 + ADE9000 ×3(M0~M2) 3채널(9피더) 전력품질 계측기
상세 이력(파일·함수 단위): [FWUPDATE](FWUPDATE) · 남은 작업: [TODO.md](TODO.md) · 설정/튜닝: [README.md](README.md)

> 2026-02-19(V00.00) ~ 2026-10-07(V00.03) 기록은 [src/README.TXT](src/README.TXT) 를 이 형식으로 옮겨 간소화한 것이다. README.TXT 에 없던 09/07·09/10·09/23·09/28 은 [FWUPDATE](FWUPDATE) 에서 보충했다.
> 형식: 날짜 `#` → 항목 `##` → 세부. 최신 날짜가 위. ★ = 홀딩/오동작 근본수정.
> **web 브랜치 사본**(2026-11-03, main `stash@{0}` 에서 옮김). 10/16 이전 항목의 Modbus 주소는 **당시 배치**(명령 7438~ 등) 그대로다 — 현재 주소는 [README.md](README.md) §6·§7.

---

# 2026-11-11
버전: **V00.04** (내장웹만 변경, 펌웨어 계측 로직 변경 없음)

## 1. 내장웹 레이아웃 정리 (`6ead302` update1, 11/10)
- Dashboard 를 한 화면에 맞춤: 상·하 2단 카드가 화면 높이를 꽉 채우고, 1100px 이하에서는 1열·자동 높이
- 값 글자 확대(평균 V/I/Hz, 상별 V/I, Unbalance, THD/TDD, PF 게이지 가변), Alarm/Event 목록은 카드 안 스크롤
- Feeder 카드 3열 고정·값 확대, IO 표 값 확대, 라이트 테마 색 보정, 화면 폭 제한 해제

## 2. Dashboard 채널 선택·전력량 (`7a65987`)
- `/api/dashboard?ch=N` — CH1~3 순시치(없음/범위 밖 → CH1), 응답에 `ch`·`imp_kwh`·`exp_kwh` 추가. IOM 은 CH1 고정(IO 페이지 호환)
- 제목 `Dashboard - CHn` 옆에 CH1/CH2/CH3 버튼(2채널 모델은 CH3 숨김), 버튼 글자색 = 채널별 활성 알람 OR(적색 있음 / 그린 없음, 3초 주기 갱신)
- Power 카드 → `Power Factor · Power · Energy`, P/Q/S 아래 Imp./Exp. 한 박스, 단위 kWh → MWh → GWh 자동 변환
- 사이드 메뉴 마우스오버 밑줄 제거
- 내장 JS 문법만 확인(node), Keil 빌드·실기 화면 확인 필요 → [TODO.md](TODO.md) 3.8

---

# 2026-11-10
버전: **V00.04** (초기 납품 버전, main = web)

## 1. ★M1·M2 파형 스파이크(고정 순서 스캔) 수정
- 11/06~09 62h 로그: 비트 분리로 ZX timeout·lag late 는 해소, 대신 M2 가 IRQ 위상 0 통과 후 직전 IRQ 와 짝지어 늦게 읽혀 `pgLag` 매 IRQ 증가, 위상차 ≈2.3ms 에서 M2 >3% 스파이크(~2h35m 주기)
- `Meter12_Task` 가 0x1|0x2 OR 대기로 도착한 칩만 처리, 칩별 50ms 무통보 시 ZX timeout 처리(`meter_scan_proc`·`meterZxTimeout` 분리)
- 실기 23.6h: 리셋·ZX timeout·lag late 0, 2.3ms 통과·IRQ 겹침 9회 모두 스파이크 없음(남은 >3% 는 M0 동시 = 신호)

## 2. 납품 정리
- 진단 끔: `IRQ_PHASE_DIAG`=0, `WV_DIAG` 주석
- 필드 타깃 `SV300_3CH_app` 0E/0W, 플래시 여유 ~5.2KB
- 보드 V082(MAIN·PT) 재교정·위상·1P2W/3P4W 확인 완료. 2MB SPI 플래시는 가득 차면 PQ 저장 실패 → 16MB 교체 예정

---

# 2026-11-06
버전: **V00.04** (main = web 통합)

## 1. 새 메인보드 시험 + main/web 통합
- 새 메인보드(M0~M2 칩별 개별 크리스털)에서 전압 위상 오차 해소. 전류 미계측은 저항 미실장(하드웨어)으로 확인
- 튜닝 코드가 main·web 동일함을 확인하고 main 을 web 으로 fast-forward 통합 → 이후 브랜치 분리 불필요

## 2. 교정(Calibration) 수정 (커밋 2b0f399 + 마감 커밋)
- 1P2W 피더 #2·#3(슬롯1·2) 유효전력·PF 0 표시 → `calcPower` 옛 '2·3 지움' 제거, `energy_scan` 전력량도 동일 복구
- Power Cali 를 60°(PF 0.5)에서도 가능하게: 기대전력 V·I·cos(θ), θ=atan2(Q,P). 게인식 `기대/측정−1` 로 정정
- 전압 게인 Z·PT2·3P3W 를 피더맵 기준 슬롯별로(`uGainSel`, stash@{0} 선별 반영) — L1/L2/L3 혼합 결선 대응
- 무전류 상 Phase Cali 시 기존 PHCAL 유지(0 으로 지워지던 문제)
- PGA 2x·Power 누적식은 반영 안 함(결정)
- confWebApp: AUTO CAL(결선 L1/L2/L3 전압교정 → 혼합 전류·위상·전력 → 3P4W 확인 → REPORT, 실패 시 멈춤), 커밋 9f1af4b
- 실기: 60° 입력에서 W·VAR·VA 정상(역산 위상 오차 0.1° 이내). AUTO CAL·uGainSel 은 실기 대기

## 3. ★M1·M2 ZX timeout / lag late 원인 확정·수정
- `[IRQPH]` 진단(커밋 4736f1e)으로 확인: M1→M2 IRQ 위상이 1분에 ~108us 이동(~2.5h 주기), 200us 이내로 겹친 ~4분에만 ZX timeout·lag late·M1/M2 파형 스파이크(M0 무영향)
- 원인: 두 칩 IRQ 를 같은 이벤트 비트 0x1 로 통보 → 겹치면 하나가 사라짐
- 수정: M1=0x1·M2=0x2, `meter_scan` 이 자기 비트만 대기. 하룻밤 로그로 재확인 예정

## 4. ★에너지 FRAM 이중저장 겹침 + 플래시 소거 반환 버그
- `ENERGY_NVRAM`(1168B)을 FRAM 0/512 에 써서 두 사본이 겹침 → 부팅마다 1번째 CRC 오류, 이중화 무의미 → FM24CL64(8KB) 기준 2번째를 4096 으로 이동, 옛 위치에서 1회 이관. **실기 확인**
- `EraseSector` 가 소거 실패 시 return 없이 빠지던 버그(8/21 주석 처리 부작용) 수정

## 5. 정리
- 비활성 진단 로그 삭제([WR]/[QCAP]/[FLASH], WV_DIAG_METER, WV_M0_ONLY, PQ/FS 타이밍 로그). IRQ_PHASE_DIAG·WV_DIAG 는 재확인 후 끔
- TODO.md 정리: 실기 확인 끝난 항목 삭제(사용자 확인), 안정성 섹션 신설
- 새 보드 SPI 플래시는 초기 부품 MX25L1636E(2MB) → 128Mbit 로 교체 예정

---

# 2026-11-04
버전: **V00.04** (web)

## 1. ★10분마다 네트워크·콘솔이 ~25초 멈추던 문제 근본수정
- 원인: PQ 10분 기록이 REALTIME 미터 태스크에서 NOR 파일 검색(`ffind`, 1회 ~3.6초)을 반복 → SNMP timeout·콘솔 엔터 지연. 플래시 "쓰기"가 아니라 "검색"
- PQ 파일 기록을 FS_task(LOW)로 이관, 검색 제거(fopen/fread), 오래된 파일 정리는 하루 1회(매시 5~8분)·최대 8개
- 실측: 10분 경계 SNMP 지연 없음, 기록 ~0.9초(LOW 태스크)

## 2. 오래된 로그 파일이 지워지지 않던 버그
- 플랫 파일시스템에서 삭제 경로가 `pq_pq_…` 로 잘못 만들어져 PQ·에너지 아카이브·트렌드 정리가 항상 실패 → 파일 누적 → 검색이 점점 느려짐
- 수정 후 2017년 파일 삭제 확인

## 3. 15분 수요전력 저장 이관
- HIGH 태스크에서 15분마다 같은 파일을 9번(~2.3초) 쓰던 것을 FS_task 에서 채널당 1번으로
- 상태: 빌드 0E/0W, **실기 미검증** (내일 로그 확인)

## 4. 문서·기타
- snmp.md 시험 항목별 장비/Paessler 설정 정리, SNMPv3 TC-11·12 실기 합격
- 남은 것: 에너지 로그 저장(HIGH, 채널당 ~250ms), 로그 흔적 없는 단발 SNMP timeout(패킷 유실 추정), 진단 로그 OFF

---

# 2026-11-03
버전: **V00.04** (web)

## 1. README.md / TODO.md / WORKLOG.md 를 web 브랜치로 복구
- 세 문서는 10/16 main 에서 만들고 커밋하지 않았는데, 10/28 main 되돌리기(`git stash push -u`) 때 교정 수정과 함께 `stash@{0}` 에 들어가 web 에서 보이지 않았다
- `stash@{0}^3` 에서 꺼내 web 주소(메모리맵 `_261028`)로 갱신: 명령 7438~ → **7788~**(reboot 7798·save 7799·교정 7800·ref 7810~7813·init 7814), FREQ 7520, Timezone 7402, README §7 통신 설정 표 신설
- main 전용 교정 수정(uGainSel·1P2W 전력·setGainW VA·PGA 2x)은 web 미적용으로 표시. snmp.md 는 web 쪽이 최신이라 덮어쓰지 않음
- 펌웨어 코드 변경 없음

---

# 2026-10-28
버전: **V00.04** (web, 커밋 557ed25)

## 1. 통신 설정 410워드 재배치 (메모리맵 _261028)
- Modbus 영역에 Ethernet 7110 → Modbus 7150 → MQTT 7160 → SNMP 7290 → SNTP 7400 → 예약 7420~7519 순으로 모음, DAQ 삭제
- Modbus TCP Enable(7150) 신규, SNMP 문자열·v3 비밀번호를 snmp.d → 통신 설정으로 이동(snmp.d 는 engineBoots 만), Timezone ETC 7401 → SNTP 7402, SNTP 서버 IP/도메인
- FREQ 이후 +350 이동(FREQ 7520, PT 7522, CT 7594, ETC 7738, IOM 7760, 명령 7788~). 주소는 컴파일 타임 검사로 고정
- ※ settings.dat 레이아웃이 바뀌어 업데이트 후 통신 설정 재확인 필요

## 2. MQTT 추가
- CycloneTCP MQTT 3.1.1 퍼블리셔(mqtt_app.c), 페이로드 GEMS35x JSON 배열(공통 + `fN_*` 피더), 기본 ClientID `sv300-<SN>`·토픽 `sv300/<SN>/data`, TLS 미지원
- 필드 타깃 플래시 확보: SNMP MD5/DES 제외, SNMP·FTP·DNS·SLAAC 로그 WARNING (여유 약 4.3KB)
- 상태: 빌드 0E/0W, **실기 미검증**

---

# 2026-10-08 ~ 10-12 (web)
## SNMP 보안·설정·알람 Trap, SNMPv3(USM) + IANA PEN 67140
- 상세는 [snmp.md](snmp.md)·FWUPDATE. 실기 TC-12 확인 후 FW_VER 상향 예정

---

# 2026-10-19 (main `stash@{0}`, web 미적용)
## 교정 수정 — CT_100mA 전류 PGA 2x, setGainW VA 기준 + 누적
- 재교정 후 Power Cali 로 W 0.5% 이내 확인. 상세 README §5.6

---

# 2026-10-16
버전: **V00.03** (main)

## 1. 작업 문서를 README.TXT → WORKLOG.md / TODO.md / README.md 로 전환
- src/README.TXT 의 날짜별 이력을 이 파일로 옮겨 간소화. 미완 항목은 TODO.md, 빌드·설정·교정 방법은 README.md 로 분리
- **src/README.TXT 는 이력 보존용으로 두고 더 이상 갱신하지 않는다.** 이후 요약 이력은 이 파일, 상세는 FWUPDATE
- 펌웨어 코드 변경 없음

## 2. 위상(Phase) 교정 실패 원인 조사 (코드 변경 없음)
- 증상: Calibration 에서 전압·전류는 튜닝되는데 위상이 안 맞음
- 원인(시험조건): 소스 전류위상 "60°" 가 **진상(I 가 V 보다 60° 앞섬)** — 파형 정점 간격·PHASE(I)=+60°·VAR 음수로 확인. 펌웨어 `setGainPh()` 는 **지상 60°(VAR +)** 전제 → 120° 오차로 계산
- 코드상 함께 확인된 문제(→ TODO): Cali 반복 시 PHCAL 이 누적이 아닌 덮어쓰기라 되돌아감, 무전류 상 PHCAL 이 NaN→0 으로 지워짐, 위상교정 로그 없음
- 올바른 절차는 README.md §5 정리

---

# 2026-10-07
버전: **V00.03**

## 1. ★app 타깃 부팅 홀딩 — `sysTick64` volatile 누락
- 증상: SV300_3CH_app_16M 이 `[WEB] mDNS: ...` 출력 직후 영구 정지(디버그 타깃은 정상)
- 원인: ISR 이 갱신하는 `sysTick64` 가 비volatile → -O2(app 프로젝트)에서 `cpuCalibIdle()` busy-wait 의 `(sysTick64 - t0)` 가 상수 0 으로 접혀 무한루프. 디버그 프로젝트는 -O1 이라 우연히 동작
- 조치: 선언 5곳(app_main.c 정의, meter.h, ade9000.c/fft.c/fft_radix2_czt.c 중복 extern) volatile 통일. 디스어셈블로 루프 내 재읽기 확인, 실기 3채널·웹·셸 정상

## 2. 빌드 설정
- SV300_3CH_app_16M 에 `_WDT_EN` 추가(홀딩과 무관함 실기 확인), FW_BUILD 26/10/07, 빌드옵션표에 _16M 타깃 2종 추가
- ※ src/fft.c 는 CP949 — UTF-8 재인코딩 시 한글 주석 파손 주의

---

# 2026-09-28
버전: **V00.03**

## 1. web 브랜치 → main 병합 (--no-ff)
- main(refactor/ade9000-bus-thread 18커밋) + web(공장초기화 기본 네트워크값) 병합. 충돌은 FWUPDATE 1건(날짜순 정렬로 양쪽 보존)
- 병합 후 FW_BUILD_DAY 21 → 28

---

# 2026-09-23
버전: **V00.03** (refactor/ade9000-bus-thread)

## 1. ★실환경 파형 '스파이크' 종결 — 측정 아티팩트로 규명
- ① WV_DIAG 덤프 printf 가 Meter 스레드를 ~113ms 막아 WFB 덮어씀(관측이 대상을 교란) ② 웹 auto-scale 이 무부하 노이즈 확대 ③ 검출 임계가 신호의 0.26% 로 과민
- 진단을 FFT_Task(LLOW)로 이전(`wvfDetect`), 임계를 신호피크 비례로 → 정상운전 중 >2% 검출 0. 크로스토크·특정칩 하드웨어 기각
- 셸 `WVF`(스파이크 통계)/`WVF C`(클리어) 추가. NOR flash 16MB 동적대응

## 2. RTOS 쓰레드 통합 + CPU 부하율
- 계측 3태스크(PostScan/RMSLog/Energy) → `Metering_Task` 1개, Trend_Task → FS_task 흡수(flash write 창구 단일화, 태스크 4개·RAM ~10KB 절감)
- 셸 `CPU`: idle 카운터 기반 부하율 + 태스크별 호출누적(실기 load≈25%)
- 근거 문서: [document/SV300_쓰레드구조_분석.md](document/SV300_쓰레드구조_분석.md)

---

# 2026-09-21
버전: **V00.03**

## 1. 공장초기화 기본 네트워크값 조정
- 서브넷 255.255.255.0 → **255.255.0.0**, 게이트웨이 192.168.9.1 → **192.168.1.1** (기본 IP 192.168.9.124 와 같은 대역)

## 2. [확인·코드 무변경] 시리얼/초기화 명령
- 시리얼번호 설정은 `MACSET` 이 아니라 **`SNSET`** (pcal->sn). MAC 은 부팅마다 CPU UID 로 생성
- `INITDB` = 설정(settings.dat) 공장초기화 후 리셋. FRAM METER_CAL·almSet·pqeDef·에너지는 보존

---

# 2026-09-15
버전: **V00.03**

## 1. ★트렌드 고속기록 자동 OFF + interval 경계 버그
- `trdTime[]` 경계 `>= 8` 오류로 60분 선택 시 실제 10분 간격 기록(쓰기 6배) → `TRD_TIME_N` 으로 정정
- 10분 미만 간격은 60분까지만 기록 후 active=0 (1분 간격 시 월 35MB 로 예산 초과하던 경로 차단)

## 2. `time` 콘솔 명령 시각 표시 2건 + 내장웹 Maintenance 보강
- 연도 이중보정(3926년 표시)·EXT_RTC 빌드에서 빈 내장 RTC 읽던 문제 수정
- Maintenance: PC Time·Device 현재시각·SNTP 주의문구·FW Update Clear, 2열 그리드

---

# 2026-09-10
버전: **V00.03**

## 1. ★CH3(M2) 전류 고조파 전 차수 ~100% 간헐 발생
- 원인: 파형에 섞인 단일 스파이크(임펄스는 전 차수 동일 크기라 정규화 시 100%). 상대 미터 폴링이 readWFB 8페이지 버스트 사이에 끼어들어 ADE9000 동시변환 교란
- 조치: 버스트 전체가 `ssp1_mutex` 보유하도록 락 범위 확대 (※실기 검증 필요 → TODO)

## 2. 내장웹 정리
- 0/1 설정 토글 버튼화(DHCP·SNTP·Trend·PQ Report 연동 비활성), Status 카드 1초 갱신, Auto Rotation 제거
- 웹은 `g_meterReady` 대기 없이 즉시 조회(Modbus·쓰기는 게이트 유지)
- 로그 로테이션 오삭제 2건 수정, 미사용 WIZnet 5개 파일 프로젝트 제외
- Main Setting 'Command' 탭 → 사이드바 독립 **Maintenance** 페이지 (GEMS3500·confWebApp 과 통일)

---

# 2026-09-09
버전: **V00.03**

## 1. ★재부팅 후 재접속 불가(read error 고착)
- 세션을 RAM 테이블 → **무상태 HMAC-SHA1 서명 쿠키**로 전환(장치키 `\system\websec.d`) — 재부팅해도 재로그인 불필요
- SPA 는 401 시 로그인 화면 복귀, serveIndex noCache, mDNS 를 `g_meterReady` 대기 앞으로 이동(.local 접속 지연 해소)

## 2. 내장웹 General 레지스터 off-by-one
- hwModel(7086) 삽입 후 밀린 14개 필드 정정(hwVer 7087·fwVer 7088·Status 7092~7101) — FW Version 0 표시 해소

---

# 2026-09-07
버전: **V00.03**

## 1. METER_CAL 2CH/3CH 동일 레이아웃(부트로더 호환)
- 배열 차원 `METER_CH_COUNT` → `CAL_CH_MAX(=3)` 고정 → 양쪽 836B(mac@820 sn@824 crc@834), 크기 컴파일타임 검사 추가

---

# 2026-09-02
버전: **V00.03**

## 1. 내장웹 ITIC(CBEMA) 커브에 이벤트 점 표시
- itic2 리스트를 정격(PT1/CT1) 대비 % 로 정규화해 플롯(web.c·confWebApp 동일). 대시보드 Event Log 요약을 타입별 5종으로 정리

## 2. [진단] 랜케이블 착탈 시 리셋 = 펌웨어 무관
- RJ45 실드 종단 R214/CB221 미실장 → 착탈 ESD/그라운드바운스. HW 조치 필요(실드 스너버·접지)

---

# 2026-09-01
버전: **V00.03**

## 1. 전력량 아카이브 15분화
- ENERGY_LOG15(96슬롯) 별도전역 `egy15` 월아카이브, 일변경 +3분 지연·today 정각+2분 저장(수요 15분 flash 버스트 분리), 6개월 보존, 부팅 stale-day 복구

## 2. 이벤트 Dur 65535ms 클램프 (long interruption wrap 제거)

---

# 2026-08-31
버전: **V00.03**

## 1. PQ 이벤트/RMS 캡처 안정화
- 이벤트 FIFO 재부팅 유실, RMS 캡처 미발화, FS 대용량버퍼 파일손상, RMS 트리거 정렬, 인터럽션 WINT/DINT 구분

## 2. 설정 연결·정합
- PQ Report Active/StartDay·INTERRUPTION Level/Action 실제반영, 월간 이벤트로그 24B, 내장웹 알람/이벤트 시각 로컬화(getUTC*)

---

# 2026-08-28
버전: **V00.03**

## 1. 로그 로테이션 + 에너지 월단위 아카이브
- PQ 캡처 계열별 50이벤트·ql/qw 90일·에너지 24개월('새 로그 생성 시 오래된 것 1개', RTC 미설정 시 비활성)
- 에너지 `e<YYYYMM>_m{id}.d` append(예산 1MB), Energy_Task 타임스탬프 초기화 전채널 수정

## 2. 이벤트 캡처 전압+전류 동시저장 + FTP 견고화
- WAVE_LF_CAP/RMS_CAP 에 전류 추가(모든 이벤트 V+I), fsReadDir 깨진 파일명 skip, SIZE 경로 수정
- confWebApp: PQ 뷰어 COMTRADE·캡처 V+I 2패널·에너지 월뷰

---

# 2026-08-25
버전: **V00.03**

## 1. THD 정확화 — 두 문제 동시 해결
- (A) **표시 10× 오류**: 고조파는 %×100 단위인데 웹이 /10 → 15% 가 150% 로 표시. web.c·confWebApp 모두 /100
- (B) **측정 정확도**: 비동기 DFT/CZT 스펙트럼 누설(깨끗한 입력에도 ~1.8%) → **Goertzel(측정주파수)** 엔진, 1~63차 단일패스(CZT 대비 ~15배)

## 2. 파형 찌그러짐 원인분석 — '보정 없이도 측정 정확' 확인
- 워커(RMSLog/PostScan/Energy/Trend/IOM) 단계시험 → 전원 무죄, THD 정상. M1/M2 WFB 가 파형 표시에 약간 커플링(측정 무관)
- despike/QCAP 보정은 원인을 가리므로 **OFF 유지**(WV_NO_DESPIKE). 정식운전 복원(WV_STAGED OFF)

---

# 2026-08-24
버전: **V00.02**

## 1. FW 브라우저 업로드 + 명령맵 off-by-one 일괄수정
- 웹 Setup→Command: .bin 선택→검증→Apply→부트로더 굽기(admin)
- clear/ack/Init/Clear PI 주소 정정 — **Clear PI 가 factory reset 으로 동작하던 문제** 포함

## 2. 대시보드·계측 정리
- PF·CT 모듈카드, 에너지 5분저장, THD 조건부계산(동작전압/전류), Start I 표시, nTurns 전류비율, ct_dir 위상 180°

---

# 2026-08-21
버전: **V00.02**

## 1. ★파형/THD 근본해결 — Quiet Capture (워커·flash 조용한 창에서만 캡처) + M2 3채널 계측 활성 + THD ~2.6초 갱신
## 2. SPI 20MHz 상향 + 칩 1개 실패해도 web/Modbus 통신(10초 폴백, 부분고장 진단 가능)

---

# 2026-08-18
버전: **V00.02**

## 1. 파형 표시 분산 — `waveDispIndex`(ZX 기준 구간을 표시 160점에 균등분산) + 윈도우 위상고정
## 2. 윈도우 despike(A안) 폐기 — 소스 잔여가 고립 글리치가 아닌 광범위로 규명, median 복구는 실신호 왜곡 → OFF(코드 보존)

---

# 2026-08-14
버전: **V00.02**

## 1. 파형 seam 근본원인 — SPI 로 WFB 읽는 행위가 ADE9000 동시변환 교란(버스트당 ~1샘플) → readWFB 6채널 median 검출·선형보간 복구
## 2. Modbus METERING 스냅샷 더블버퍼(torn read 방지) + EN50160 CH2/3 전압공유 복사 + Modbus 쓰기 미준비/무효주소 예외

---

# 2026-08-13
버전: **V00.02**

## 1. inorm(정격전류) CT1 대비 %화 + 설정 기본값 정규화
- In = CT1 × inorm/100(기본 50%), 0 설정 → default(I_start1·inorm50·CT1 100·PT 220), freq enum 0:60/1:50(구버전 자동 마이그레이션)
- Temp-ref(7463) 명령 신설·명령 +1 시프트
- ※ CT1≠100 유닛은 업데이트 후 inorm 재설정 필요

## 2. 웹·confWebApp
- read error 시 계측값 "----", CT I Nominal(%)·Start I 소수 3자리, mDNS 호스트명 SN 기반
- confWebApp 설정 백업/복원(JSON 레지스터블록, 7449 영속) — 실기 라운드트립 PASS

---

# 2026-08-12
버전: **V00.02**

## 1. Calibration 지원
- confWebApp Calibration 화면(ADMIN): V/A/Power/Phase/IN 교정(명령 7450 code) + **Temp 교정**(METER_CAL tempOfs, 기준 25℃, code 17/18) + 피더 실측 검증표 + 엑셀 리포트
- 교정은 칩 단위(Feeder 1~9 → 칩 0~2)

## 2. 260812 맵 — 피더 Simple 에 Angle U1~3·I1~3 추가(SMP_CHANNEL 90→100w)
## 3. HWMODEL 복원(7086, 0=3CH/1=2CH) — METER_CAL 저장, 콘솔 `MODEL <n>`
- ※ METER_CAL 구조 변경으로 업데이트 후 **교정값 재초기화 필요**

---

# 2026-08-11
버전: **V00.02**

## 1. ★홀딩 근본수정 재확정 — OS_STKSIZE
- 정적스택 미제공 시 RTX 가 per-task 크기를 무시하고 OS_STKSIZE 를 전 태스크에 적용 → httpConnectionTask 스택 초과·Meter0 침범. **OS_STKSIZE 375→640**
- Buffer Ready(M0~M2) 전 웹·Modbus 차단(`g_meterReady`)

## 2. Energy Edit 영속화 — `applyEregEdit`, FLASH 저장은 Save Set(7449)에서만
## 3. 페이저 IEC 색상·WIRING 배지, IO(DI/TEMP) 별도화면, DI active-low 보정

---

# 2026-08-10
버전: **V00.02**

## 1. Energy 웹 전체편집 + IOM DATA 개편 + PI 기능(DI Type별 펄스카운트·디바운스·PI Scale), 로그인 viewer → sv300
## 2. 빌드 최적화 — 부트로더 -O2(SV300_boot 별도), 웹 UI 영문화로 flash 확보
## 3. 메모리맵 260810 + METER_CAL 4D화(vgain/vppgain[chip][phase][결선Z][PT2], 로고스키 R_igain/R_phcal) + METER_INFO 상태필드
## 4. ★Meter1_Task 스택오버플로우 — 스택 상향 (08/11 OS_STKSIZE 로 최종 해결)

---

# 2026-08-07
버전: **V00.02**

## 1. Main Setting 확장(Device Info·Command Load) + Channel Setting 7섹션 신규
## 2. 전압공유(M0~M2) 반영 — PQ Event·Transient Voltage CH2/3 숨김, Harmonics 전압은 CH1
## 3. 명령맵(260807) 정렬(ALL+#1~3) — Alarm/Event/ITIC Load·Clear 를 측정화면으로 이동
## 4. Min/Max·Energy·Demand Clear, 대시보드 IOM DATA(7050~7069)

---

# 2026-08-06
버전: **V00.02**

## 1. Channel Waveform / Report(EN50160·ITIC) / Monthly 탭 → Channel 11탭 완성
## 2. 고조파 Table/Chart·에너지 매트릭스·수요 프로파일 차트 마감

---

# 2026-08-05
버전: **V00.02**

## 1. 내장웹 Feeder·Channel 화면 신규 — Channel 8탭 confWebApp 동일화
## 2. ★Channel 홀딩 — HTTP persistent connection(keep-alive) 활성화로 커넥션 재사용

---

# 2026-02-19
버전: **V00.00**

## 1. 프로젝트 시작
