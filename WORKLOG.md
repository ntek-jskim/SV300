# SV300 Work Log

대상: SV300 — LPC4330/4357 + ADE9000 ×3(M0~M2) 3채널(9피더) 전력품질 계측기
상세 이력(파일·함수 단위): [FWUPDATE](FWUPDATE) · 남은 작업: [TODO.md](TODO.md) · 설정/튜닝: [README.md](README.md)

> 2026-02-19(V00.00) ~ 2026-10-07(V00.03) 기록은 [src/README.TXT](src/README.TXT) 를 이 형식으로 옮겨 간소화한 것이다. README.TXT 에 없던 09/07·09/10·09/23·09/28 은 [FWUPDATE](FWUPDATE) 에서 보충했다.
> 형식: 날짜 `#` → 항목 `##` → 세부. 최신 날짜가 위. ★ = 홀딩/오동작 근본수정.
> **web 브랜치 사본**(2026-11-03, main `stash@{0}` 에서 옮김). 10/16 이전 항목의 Modbus 주소는 **당시 배치**(명령 7438~ 등) 그대로다 — 현재 주소는 [README.md](README.md) §8·§9.

---

# 2026-11-23
버전: **V00.05**(펌웨어 무변경) — 실기 시험·문서 정리

## 1. 실기 A단계(신호 없이 확인) 모두 합격
- 장치 시계 UTC: 콘솔 `TIME` app=RTC, 전원 재투입 후 유지(+9h 앞섬 재발 없음), Set Time(PC·수동 입력)
- 정적파일 차단: 로그인 없이 NOR 파일 경로 18종(대소문자·역슬래시·`../`·`%00`·`S0:` 포함) 모두 404, `/api/*` 401
- PQ Report 설정 UI: Configuration > Meter > PQ Report 에 CH1 카드만, Start Day Wed → 6845 = 3
- Modbus RX(7093): 요청 1건 = +1(워드 수 무관), 웹 조회 미포함. 사내망 연결 시 초당 ~5.7 증가는 외부 요청(1:1 직결 시 정상) — RS-485/다른 PC 미구분
- COMM LED: Modbus TCP·SNMP 응답마다 반전(Keil 콜스택·GPIO 출력 래치로 확인). 처음 '계속 켜짐'은 옆 SV400 LED 를 본 것
- SNMPv3 SHA-256/AES128: 정상 응답, SHA-1·틀린 Auth·틀린 Priv·v2c 거부
- AutoIP: 이전 시험 완료

## 2. 문서
- TODO.md 맨 앞에 **진행 순서** 추가(B 교정 → C 계측 → D 결선 → E 50Hz → F 장시간 → SV400 실기, 결정 필요·기능), 합격 항목 삭제
- README 마감 검토: §3 메모리 현황·§4 태스크 구조/우선순위·감시 규칙 신설(이후 절 번호 +2), 시각 규약 UTC 반영
- 메모리맵 외부용 사본 `ext_SV300 memory map_261120.xlsx`(ToDo&Log 시트 삭제) — 앞으로 맵 수정 시 함께 생성

## 다음
- B단계 교정(표준 전원 3P4W 220V·5A, 시작 전 교정값 백업) → C·D·E·F, SV400 실기(COMM LED 계속 켜짐 확인 포함)
- 결정 필요: RTU RX 가 다른 장치 ID 프레임까지 셈, `SNMPCFG` 비밀번호 평문 출력, board.c LED 루프 `i<4`, MQTT 최소 주기 10초

---

# 2026-11-20
버전: **V00.05**(변경 없음, 빌드일 26/11/20) — 1차 커밋 93f533a 이후 분까지 포함

## 1. ★장치 시계 UTC 운영
- 내부 시계·기록 시각·Modbus 시각을 UTC 로, 화면·파일 날짜·일/주/월 경계는 Timezone 을 더한 현지 시각
- 재부팅하면 9시간 앞서던 문제(외장 RTC 를 현지 시각으로 저장) 수정 — 콘솔 `time` 의 app·RTC 일치 확인

## 2. 계측·통신
- **Max/Min 자동 초기화** 구현: None·Daily·Weekly(PQ Report Start Day)·Monthly(기본) — 종전엔 동작하지 않았음. 맵 _261120
- **FFT(THD·고조파) 3초 주기 고정**(종전 쉬지 않고 계산) — 웹 고조파·파형은 5초 갱신
- **Modbus RX 카운트**를 요청 1건당 1회로(웹 접속만으로 늘던 문제)
- FS 장시간 검색 중 다른 태스크 감시 초과로 무한 리부팅 → 검색 중 양보·감시 15초
- ★FTP 디렉터리 목록을 접속끼리 공유해 목록이 빠지던 문제(PQ 파일 1개만 보임) + 잘못된 메모리 해제 수정
- EN50160 리포트 상별(L2·L3) 값에 L1 을 넣던 문제 수정 — 현재 시험 장비는 3상 전압 동일 입력이라 차이 안 보임

## 3. 내장웹 SV900 마무리
- Configuration: SV900 카드(Metering·Communication·MQTT·SNMP / Device Info·Remote Link), MQTT 용어 통일, Test Mode → Administration
- Administration: 4열, 24시간 Set Time·장비 시각(현지/UTC), 상단 시계 아이콘에서 시각 맞춤, System Status
- Power Quality: 고조파 상별 차트·Table/Chart, 파형 CH 탭(한 채널 전압·전류), SV900 상 색
- Report: **This Week / Last Week** 선택(메모리의 전주 리포트)
- 해/달 아이콘, 로그인 글꼴

## 4. SV400 · confWebApp 동기화
- SV400 에 11/18~20 변경 전부 이식(명령·상태 주소 SV400 대응), SV400 메모리맵 _261120, SV400 사용자 매뉴얼 신설
- confWebApp: UTC 시각 정합·SV900 화면 전체·Calibrate 화면·Report 주 선택(이전 주는 장비 파일)·이력 자동 이어 읽기, 설치 파일(Inno Setup) 배포 준비
- 사용자 매뉴얼 v0.2(화면 개편 반영)

## 5. 빌드
- `SV300_3CH_app_16M` 0E/0W, 357,748B(남음 34.6KB)

## 다음
- 실기: UTC 시각·재부팅 유지, Max/Min 자동 초기화, Modbus RX·COMM LED, SHA-256, Report 상별 값(상마다 다른 전압으로)
- SV400 최신 소스 빌드·실기, confWebApp 설치판 실행 확인·코드 서명 인증서

---

# 2026-11-19
버전: **V00.05**(변경 없음, 빌드일 26/11/19) — 11/18 미커밋 작업(SNMPv3 SHA-256) 포함

## 1. 내장웹을 SV900 템플릿으로 (제품 간 화면 통일 1단계)
- 색·카드·레이아웃을 SV900(Tailwind) 값으로: 사이드바가 왼쪽 전체 높이 + 맨 위 로고, 설정 카드 라벨 위·입력 아래
- 메뉴를 SV900 구성으로: Dashboard · Feeder · Meter · Power Quality · Alarm/Event · Report · IO · **Configuration**(General/Meter/Feeder) · **Administration**(System·System Status·Firmware·Logo)
- **Feeder 페이지**: 피더 상태 칸(On/Off/미등록) + 선택 피더의 계측·에너지(Import/Export/Total/Net)·벡터도, 한 화면에
- 벡터도(Feeder·Meter Phase) SV900 색·표기, Reboot·Factory Reset 버튼 SV900 형식
- **사이드바 로고 업로드**(PNG/JPG/GIF/WEBP ≤64KB, 없으면 글자 'SV300')

## 2. 알람·이벤트 이력 스크롤 조회
- Top/Up/Down/Bottom 대신 저장된 이력(채널당 1024건)을 스크롤로 이어 보기, Viewer 도 전체 열람
- 웹이 Modbus 이력 창을 움직이지 않아 SCADA 폴링과 충돌 없음

## 3. 정리·중복 제거
- VQ·Monthly 탭, Save Set(Save & Apply 와 중복), Alarm Def 탭(Alarm Settings 로 합침) 삭제, Status 카드는 Administration 으로

## 4. ★보안: 장비 파일 무인증 노출 차단
- 웹이 처리하지 않는 주소가 NOR 파일(세션 키·설정·펌웨어)을 로그인 없이 내려주던 경로를 404 로 막음 — SV400 은 아직 같은 문제

## 5. 메모리맵 _261119 · SNMPv3 (11/18)
- Simple 피더 블록에 kWh/kvarh **Export** 추가(50080·50088, 이후 주소 이동 → 상위 시스템 갱신 필요), 10분 평균 전력 이름을 실제 단위(W)로 정정
- SNMPv3 인증 SHA-256 선택(7297, 기본 SHA-1) — 실기 확인 남음
- `SV300_3CH_app_16M` 0E/0W, 353,048B(남음 39.2KB)

## 다음(11/20)
- 설정 화면 마무리 후, 11/19 작업을 **SV400·confWebApp 에 동일 적용**

---

# 2026-11-17
버전: **V00.05**(변경 없음)

## 1. ★첫 접속 — DHCP 실패 시 AutoIP + Current IP
- 납품 기본 DHCP 라 PC 1:1·데이지체인에서는 장비 IP 가 없어 라벨 주소(`sv300-<SN>.local`)로 접속 불가였음
- DHCP 응답이 15초 없으면 **169.254.x.x 자동 IP**(장비마다 다른 주소), DHCP 는 계속 재시도·받으면 자동 IP 해제
- **PC 1:1(PC 자동 IP)로 `.local` 접속 실기 확인**. 체인·공유기 나중 연결 시험은 [TODO.md](TODO.md) 3.15
- 내장웹 Communication 카드에 **Current IP**(실제 사용 주소) 표시

## 2. COMM LED·PQ Report 설정·진단 정리
- COMM LED: Modbus TCP·SNMP(응답·Trap)·MQTT 송신마다 반전(종전 부팅 후 계속 켜짐) — TODO 3.16
- PQ Report 설정: Start Day 드롭다운, CH1 카드만(EN50160 은 CH1 기준) — TODO 3.17
- `[ETHDIAG]` 자동 출력 → 콘솔 `ETH` 조회, SPIFI 진단 출력 정리(4.1 종결)
- 00:03 에너지 월 아카이브 정리를 루프마다 나눠 웹 무응답(약 10초) 해소 — TODO 4.2

## 3. 문서
- 사용자 매뉴얼 간결판 하나로 정리(`document/SV300_웹서버_사용자매뉴얼_v0.1.docx`, 8쪽) — 첫 접속·표시등·IP 설정·모니터링
- `document/WEBSERVER_종합비교표.pdf`, README·TODO·snmp.md·WEBSERVER_PORTING 현행화(웹서버 제한 원칙은 재검토 후 유지)
- `SV300_3CH_app` 0E/0W, 345,956B

---

# 2026-11-16
버전: **V00.05**

## 1. ★데이터 튐 대비 (현장 멀티채널 제품의 전압·전류·전력량 튐 사례 대응)
- 사례: 동작전원 3.3V → 3V 이하 저하 시 CPU 는 동작하나 플래시가 안 돼 전력량 저장 실패, 복귀 시 튐
- **전력량 증분 정격 검사**: ADE 에너지 레지스터는 매 1초분(EGY_LD_ACCM) → 상별 W/var/VA 가 `vnorm × CT1 × 3` 을 넘으면 그 1초 증분 폐기(`[EGY] spike discarded`), 3초 연속이면 실제 부하로 통과(정격 오설정 대비)
- **ADE9000 리셋·설정 소실 감지**: STATUS1.RSTDONE 즉시 또는 1초마다 RUN·EGY_TIME 확인, 2회 연속 불일치면 재초기화·첫 증분 폐기(`[ADE] re-init`)
- **에너지 FRAM 교대 저장**: 종전엔 같은 내용을 두 사본에 연달아 써서 기록 중 전원 이상 시 둘 다 오염 가능 → 마지막 정상 슬롯 반대편(0/4096)에만 순번+1 로 쓰고 다시 읽어 확인, 부팅 시 정상 슬롯 중 순번 큰 쪽 로드. 실패 시 10초 뒤 재시도(종전 5분)
- **SPI 읽기 검증**: `read_reg16/32` 가 받아 버리던 추가 2바이트를 실측 → CRC 가 아닌 **데이터 반복**. 반복값 검사(모든 읽기) + 전력량 레지스터는 **CRC_SPI(0x4A9)** 별도 읽기(CRC-16-CCITT 초기값 0xFFFF·데이터 4B, 3칩 100% 일치) → 불일치 시 최대 3회 재읽기. 콘솔 `SPICRC`. 동작 중 SPI 오류 0건(칩당 약 2만 4천 읽기)
- 실기 확인 항목 → [TODO.md](TODO.md) 3.14

## 2. 내장웹 `----` 5~10초 끊김 — 원인은 사내망
- 진단 추가: `[LOWLAG]`(웹과 같은 LOW 태스크 1초 이상 기아), `[FSLAG]`(FS_task 구간 1초 이상), `[WEBDIAG]`(HTTP 연결 슬롯·처리 지연, 기본 OFF), `[ETHDIAG]`(이더넷 수신 누락)
- 결과: LOWLAG·FSLAG 없음(CPU 기아 아님), ping·Modbus TCP 정상, **PC 1:1 직결 시 미발생**, SV400 도 동일 증상 → 사내망 원인(당시 인터넷 장애)
- `[ETHDIAG]` 로 **브로드캐스트 폭주** 확인: 10~30초 간격으로 100ms 당 최대 63 프레임을 수신 버퍼(6개) 부족으로 버림 → 섞여 버려진 TCP 의 재전송 대기가 `----` 의 실제 원인. 출력은 1분 요약으로 변경(폭주 중 HIGH 태스크 printf 몰림 방지)
- 수신 버퍼 6→12개 증설은 보류(긴 폭주는 못 막음, 네트워크 측 해결 필요)

## 3. 플래시 쓰기 실패(TODO 4.1) 측정
- 11/13~16 71시간 실패 14회 모두 `verified ok`(피해 없음), ROM 쓰기 25~39ms·칩은 쉬는 상태. SPIFI 클럭 40.8MHz 확인(불일치 가설 제외)
- 성공한 쓰기도 ROM 5ms 이상이면 `slow ok` 출력 — 태스크에 밀림(a) / ROM 완료 미인지(b) 판별용

## 4. SV400 이식
- 위 1·2(진단)·3 을 SV400 에 같은 코드로 적용(SPI 검증은 4칩). SV400 에서 `SPICRC` 로 CRC 일치 확인 필요

- 실수 수정: `[ETHDIAG]`·SPICRC 주기 출력을 처음에 `#if 0` 으로 꺼진 `Energy_Task` 에 넣어 실행되지 않던 것 → `Metering_Task` 로 이동
- `SV300_3CH`·`SV300_3CH_app` 0E/0W, 필드 타깃 0x51DE4/0x60000

## 5. 원격 대시보드 서명링크 (Ed25519 + 장비 nonce)
- `/auth/remoteNonce` → `/dashboard/{ntek|sv300}?nonce=&sig=` 검증 성공 시 로그인 쿠키 + 302. 기존 ID/PW 로그인은 그대로
- **7420 Remote Signed Link**(기본 OFF, OFF면 404). 접속 기록 20건(`/api/remotelog`, General 'Remote Link' 카드)
- 계측 영향 최소화: 검증 0.22초 동안 우선순위 낮춤 + 수 ms마다 양보, 요청 횟수 제한
- ★공개키는 연구소 **시험키** — 필드 배포 전 운영키 교체 필요. 실기 시험 통과

## 6. 납품 준비 — 내장웹 정리·사용자 매뉴얼
- 내장웹: 레지스터 주소·내부 용어 문구 삭제, CT 교정값 4종 감춤, Transient(미지원)·Disturbance 탭 제외, Trend Version 감춤·채널 드롭다운, Waveform Pre 입력 시 Post 자동(합 30사이클), Trans V/I 이벤트 이름 삭제
- 사용자 매뉴얼 `document/SV300_웹서버_사용자매뉴얼_v0.3.docx`(접속·IP 설정·모니터링 간이판)
- 메모리맵 `_261116`: 7420 추가, 미지원 항목 이름·설명 비움(#1~#3CH)
- ★남은 결정: 납품 기본 DHCP + 라벨 `sv300-XXXXXXXXX.local` — **PC 1:1 직결 시 장비에 IP가 없어 첫 접속 불가**(AutoIP 꺼짐). DHCP 실패 시 고정 IP 전환 / AutoIP 중 선택 후 FW·매뉴얼 3장 반영
- `SV300_3CH_app` 0E/0W, ROM 344,576B

---

# 2026-11-13
버전: **V00.04**

## 1. 플래시 쓰기 실패(TODO 4.1) — ROM 의 거짓 실패 보고로 판단, 우회 수정
- 11/12~13 로그(219V 입력, 17시간): 실패 1회. 진단 출력상 `needErase=0`, 실패 후 플래시 내용 = 쓰려던 값 → **쓰기는 성공했는데 ROM 만 시간 초과(0x20003) 보고**
- 그 뒤 실패(`sys_demand.d1` 이름 기록)는 블록 앞쪽 빈 자리 첫 기록 + 칩이 쉬는데 `wait_busy` 도 시간 초과 → 위치·다시 쓰기와 무관, ROM 의 칩 상태 읽기 자체가 간헐 실패
- 제외: 용량 부족, ROM 몰래 섹터 지우기, FTP 끼어듦(접속 없음), 내부 태스크 끼어듦
- 우회: 실패 보고 시 칩 완료 대기 후 다시 읽어 같으면 성공 처리(`verified ok`), 다르면 불일치 위치부터 1회 재기록 → 실기에서 `verified ok` 동작 확인
- 진단 추가: 실패 시 ROM 쓰기·`wait_busy` 걸린 시간과 상태 레지스터, 부팅 시 실제 SPIFI 클럭 레지스터(ROM 에 넘기는 40MHz 와 실제 약 60MHz 불일치 의심) → [TODO.md](TODO.md) 4.1

## 2. 파형 진단 코드 정리
- 파형 스파이크 원인분석용 진단 코드(WV_STAGED·WV_EN_*·WV_DIAG·WV_NO_M12_WFB·WV_WINDESPIKE, 콘솔 `WVF`) 삭제. 정식 운전 경로 변경 없음

## 3. 리모트 로그인 연동 검토
- 통합서버 연동 방식 중 **2번(HMAC 서명 링크)** 권고 — 기존 웹 로그인(HMAC 서명 쿠키)과 같은 원리. 서버 측과 서명 형식·만료·키 관리 확인 회신 작성

- `SV300_3CH_app_16M`·`SV300_3CH` 0E/0W, 필드 타깃 0x5111C/0x60000(여유 ~60KB)

---

# 2026-11-12
버전: **V00.04**

## 1. ★단상(1P2W) 피더 4~9 미배정 수정 (SV400 실기에서 발견, 같은 코드)
- 현상: PT 1~9 를 1P2W L1 로 저장·재부팅해도 CH1 만 전압 위상 0/0/0, CH2~3 은 0/240/120(3상 그대로). 내장웹 Feeder 화면은 3개만 표시
- 원인: `buildFeederMap` 이 `feeder_cnt`(기본값 3)개만 스캔. 실제 값 재계산(`copySimpleMap`)이 부팅 시 칩 초기화(ADC_REDIRECT·전압 게인 선택)보다 늦어 피더 1~3 만 배정
- 수정: `buildFeederMap` 이 PT 9개 전체를 스캔하고 NOT_USED 는 건너뜀. `/api/feeders` 는 피더 단위(최대 9)로 출력, 단상 피더는 해당 상 자리에만 값
- `SV300_3CH_app_16M` 리빌드 0E/0W, 실기 확인 필요 → [TODO.md](TODO.md) 3.11

## 2. 고조파(Goertzel) 계산 최적화 (SV400 `3a7762f` 이식)
- SV400 측정으로 전압·전류 인가 시 CPU 70%대의 원인이 FFT_Task 고조파 계산으로 확인됨. SV300 도 같은 코드라 3채널 전류 시 처리 한계 초과 추정(약 119%)
- 고조파 8개 묶음·레지스터 상태로 변경 → 1상 29ms → 7ms(SV400 FFTTEST), 결과 동일. SV400 CPU 72~76% → 33~34%
- `SV300_3CH_app_16M` 빌드 0E/0W, SV300 실기 확인 필요 → [TODO.md](TODO.md) 3.12

## 3. SV400 에서 확인한 수정 이식 (DI·온도·내장웹)
- DI: 부팅 직후 가짜 상태 변화(초기값 반전) 수정, Debounce 기본 16ms(저장값 0 자동 보정)
- 온도(2CH 전용): NTC ℃ 변환(GEMS3500 과 같은 NTC 계수, 분압 3.0V), Temp Cali 를 칩 온도 → IO NTC 로. TEMP4 오프셋은 METER_CAL 패딩 자리(`ntcOfs4`) 사용 — 크기 836B 그대로
- 내장웹 Feeder·IO 글꼴을 SV400 과 동일하게(3열 유지)
- 2CH 온도 코드는 미컴파일 → [TODO.md](TODO.md) 3.13

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
