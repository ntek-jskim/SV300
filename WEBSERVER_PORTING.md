# WEBSERVER(web.c 내장웹) 이식 TODO — confWebApp 대비

confWebApp(외부 뷰어)에서 구현했지만 **내장웹(web.c)에는 아직 없는/다른** 기능 목록.
추후 web.c에 옮길 때 참조. (작성 2026/08/31, web 브랜치)

범례: ✅ 이식 권장(대시보드/설정) · ⚠️ 검토(임베디드 제약) · ☑️ 이미 반영됨

---

## ★ 제한 원칙 / 역할 분담 (2026/09/01 확정)

내장 WEBSERVER는 confWebApp의 모든 기능을 이식하지 **않는다**. 장치(LPC4357)는 **실시간 계측이 최우선**이고 RAM/Flash/CPU가 제한되므로 역할을 나눈다.

**WEBSERVER(내장) — 실시간·경량·안전 (RAM/레지스터 기반)**
- 대시보드·채널 라이브 계측·알람/이벤트·기본 설정(레지스터)·**FW 업데이트**
- **소형 라이브 CSV**만 — web.c가 이미 읽는 실시간 데이터. 구현됨: `/api/demand.csv`(수요 96×15분), `/api/energy.csv`(24h 당일/전일 kWh)

**confWebApp(외부) — 파일기반·분석·리치 (FTP + PC 자원)**
- PQ/Trend **파일뷰어**·**아카이브 CSV**(egy·trd)·**COMTRADE**·**PDF**
- 채널설정 편집기·Calibration·Backup/Restore·3단계 권한(SUPER)

**WEBSERVER 금지/제한 (web.c 상단 주석에도 명문화)**
1. **FS 대용량 파일 파싱 금지** — PQ/에너지 월아카이브·Trend·캡처 파일 (`fsFileLock`·SPIFI 활동 → 계측·파형캡처 seam 교란)
2. **CSV는 RAM 라이브만**, 아카이브(과거)는 ✗
3. **PDF 서버생성 금지**(라이브러리·RAM 부적합) — 필요시 브라우저 인쇄
4. **COMTRADE 금지**(캡처 파싱 무거움)
5. 핸들러는 **짧고 빠르게**, FS락/CPU 장시간 보유 금지, 대용량 응답 지양
6. 무거운 UI·3단계 권한은 confWebApp에

> 이력: 2026/09/01 `apiEgyCsv`(FS 아카이브 CSV)를 시도했다가 원칙 #1·#2 위반으로 **철회**. WEBSERVER CSV는 demand/energy 라이브까지만 유지.

---

## 📊 종합 비교표 — WEBSERVER 지원 현황 (confWebApp 전체 기능 대비, 2026/09/01)

범례: **✅ 지원** · **❌ 미지원**(→confWebApp) · **△ 부분/구버전** · **☑️ 반영됨** · **— 해당없음**

### 1) 측정 / 모니터링 (RAM·레지스터 라이브 — WEBSERVER 강점)
| 기능 | WEBSERVER | confWebApp | 비고 |
|---|:---:|:---:|---|
| Dashboard | ✅ | ✅ | CH1 순시 |
| Feeder | ✅ | ✅ | 피더별 계측 |
| Channel · Meter / Phase / Min·Max | ✅ | ✅ | |
| Channel · Harmonics | ✅ | ✅ | 차수 2~63 |
| Channel · Waveform | ✅ | ✅ | V/I 파형 |
| Channel · Report (EN50160+ITIC) | △ | ✅ | web.c EN50160표·ITIC커브 有 / 이벤트점·**PDF**·**CSV**는 confWebApp |
| Channel · VQ (전압편차) | ✅ | ✅ | Flicker 미구현(양쪽) |
| Channel · Monthly (10분평균) | ✅ | ✅ | |
| Channel · Energy (매트릭스+편집) | ✅ | ✅ | |
| Channel · Demand (수요 프로파일) | ✅ | ✅ | |
| Channel · Alarm / Event | ✅ | ✅ | 상태·로그 |
| IO 모듈 (DI/PI/Temp) | ✅ | ✅ | web.c 전용 **IO 페이지**(상단 nav 'IO') + 대시보드 IO DATA 카드: DI/PI/Temp 표시·Type/Deb/Scale 편집·Clear PI (7050~7069) |

### 2) PQ / Trend 파일뷰어 (FTP·FS 파일 기반) — **원칙상 WEBSERVER 미지원**
| 기능 | WEBSERVER | confWebApp | 비고 |
|---|:---:|:---:|---|
| PQ Log (ql VQ트렌드 파일) | ❌ | ✅ | FS 파일뷰어 |
| PQ Report (qw 주간 파일) | ❌ | ✅ | +ITIC점·**PDF**·**CSV** |
| PQ Trigger (캡처 W/D) | ❌ | ✅ | 파형/RMS |
| PQ Energy (egy 월아카이브) | ❌ | ✅ | +**CSV** |
| Trend (trd pen recorder) | ❌ | ✅ | +**CSV**·Device Files |
| Device Files 선택/전체 삭제 | ❌ | ✅ | FTP DELE |
| **COMTRADE** 내보내기 | ❌ | ✅ | 캡처 CFG/DAT |

### 3) 내보내기 (Export)
| 기능 | WEBSERVER | confWebApp | 비고 |
|---|:---:|:---:|---|
| 라이브 CSV (demand·energy) | ✅ | — | web.c 신규 `/api/demand.csv`·`/api/energy.csv` (octet-stream 다운로드) |
| 아카이브 CSV (egy·trd) | ❌ | ✅ | FS 파싱 필요 → confWebApp |
| **PDF** (리포트) | ❌ | ✅ | 서버생성 금지(라이브러리·RAM); 필요시 브라우저 인쇄 |

### 4) 설정 (Settings)
| 기능 | WEBSERVER | confWebApp | 비고 |
|---|:---:|:---:|---|
| Main Setting (Comm/PT/CT/ETC/Freq) | ✅ | ✅ | 레지스터 |
| Channel Setting 편집기 | △ | ✅ | web.c 有(PQEvent/Transient/Waveform/Disturb/Trend/PQReport/AlarmDef/Alarm+PT/CT) / confWebApp은 정리됨(콤보·Active접기·Alarm병합·Transient·Disturb·Version 미사용제거·CH KF I3제한) |
| **Calibration** | ❌ | ✅ | confWebApp SUPER 전용 |
| **Backup / Restore** | ❌ | ✅ | 설정 CSV 백업/복원 |

### 5) 시스템 / 공통
| 기능 | WEBSERVER | confWebApp | 비고 |
|---|:---:|:---:|---|
| 로그인 / 권한 | △ | ✅ | web.c **2단계**(admin ntek·viewer sv300) / confWebApp **3단계**(**SUPER**·admin·viewer) |
| FW Update (웹 업로드/적용) | ✅ | — | web.c 전용(장치 자체 플래시) |
| Manual R/W (레지스터) | ✅ | ✅ | |
| 신규 이벤트/알람 **블링킹** | ❌(불필요) | ✅ | 웹서버는 블링킹 불필요로 결정(2026/09/01)·ACK Alarm/Event 버튼 제거 |
| 시각 로컬화 (getUTC*) | ☑️ | ✅ | web.c 반영됨 |

### 요약 — WEBSERVER **미지원**(=confWebApp 전용) 핵심
- **파일기반 전부**: PQ/Trend 파일뷰어·아카이브 CSV·COMTRADE (원칙: FS 대용량 파싱 금지)
- **PDF** 서버생성 (원칙: 라이브러리·RAM)
- **Calibration**, **Backup/Restore**, **3단계 권한(SUPER)**, 신규 **블링킹**(웹서버 불필요로 미적용, 2026/09/01)
- **△(구버전)**: Channel Setting 편집기(정리 전), Report(ITIC 이벤트점 미표시), 로그인(2단계)

---

## ❌ 1. 이벤트/알람 신규 항목 블링킹 — 웹서버 불필요로 최종 제외(2026/09/01)
> **결정(2026/09/01)**: 웹서버엔 블링킹 불필요. 이식했다가 **전량 원복**하고 대시보드 **ACK Alarm/Event 버튼도 삭제**. 아래는 이력용 스펙.
**동작**: Event/Alarm Log에서 **아직 ACK 안 한(=신규) 행의 Type/State 뱃지만** 깜빡임. ACK 누르면 정지, 새 이벤트 오면 다시 깜빡.
**confWebApp**: `sv300ser.html`
- `getSeen/setSeen`(localStorage, key `sv300blink_ev`/`_al`) — "ACK한 최신 ts" 저장.
- 렌더 시 각 행 `ts > seen` 이면 뱃지에 `newblink` 클래스 추가.
- ACK 버튼(`svCmd('ack_event'/'ack_alarm')`) 성공 시 `setSeen(key, 최신ts)` → 정지.
- CSS `@keyframes newblink{50%{opacity:.28}} .newblink{animation:newblink .85s ...}`
**web.c 이식**:
- `loadEvents()`(web.c JS, ts2/el-tbl·al-tbl 렌더)에 동일 로직 추가. 이벤트 행 `ts2(s)`의 원본 ts(초)로 비교.
- 이벤트/알람 배열이 내림차순이면 `[0].ts`가 최신.
- ACK 명령은 web.c에 이미 있음: **ACK Event=7498, ACK Alarm=7494**(svCmd). ACK 성공 후 seen 저장.
- **주의**: Event STS(7094)·Alarm STS(7093) 레지스터는 값이 불안정(0으로 관측)이라 **쓰지 말 것** → 반드시 **로그 ts 기반 + localStorage seen** 방식 사용.

## ✅ 2. PQ Report 설정 — Active/Start Day 콤보박스 + CH0(M0) 전용 표시
**동작**: EN50160 리포트는 **M0만 산출**(M1/M2는 M0 복사). Active(0=Disable,1=Enable)·Start Day(0=Sun~6=Sat)를 드롭다운으로.
**레지스터(CH상대, M0 기준)**: Active=**6844**, Start Day=**6845** (pqRpt 구조체).
**confWebApp**: `channel_setting.html`(PQ Report 섹션 CH1만 렌더·all-same 숨김) + `main.py _NAME_ENUM`(Active/Start Day 라벨).
**web.c 이식**: Channel Setting의 PQ Report 섹션(CSECT)에서 (a)Active/StartDay를 OPT 드롭다운으로, (b)CH2/CH3 카드 숨김.
**펌웨어측 동작연결은 이미 완료**(Quality.c) — UI만 옮기면 됨.

## ✅ 3. ITIC 리스트 → CBEMA 커브에 이벤트 점 표시
**동작**: ITIC List(전압 sag/swell/intr)의 각 이벤트를 CBEMA 곡선에 `dur(ms)→로그X, level(%)→Y`로 점 찍기(타입별 색).
**confWebApp**: `channels_grid.html` `drawITIC(cv, events)` + `loadReport`가 `/api/itic` 먼저 받아 전달.
**web.c 이식**: web.c의 Report 탭 `drawITIC`에 events 인자 추가, ITIC 창(5780) 읽어 점 렌더.

---

## ⚠️ 4. PQ 파일 뷰어 계열 (confWebApp 전용 — web.c엔 파일뷰어 자체가 없음)
web.c는 **실시간 대시보드**라 FTP 파일뷰어가 없음. 아래는 임베디드에 넣기 무겁고, 넣으려면 FTP/파일파싱 UI 신규 필요 → **원할 때만**.
- **트리거 캡처 표시(W=파형/D=RMS, V+I 2패널)** + **WINT/DINT** 접두어 인식(`_sv_cap_kind`). 신포맷: WAVE_LF_CAP hdr 28B+dense int32, RMS_CAP hdr 16B+float[3][1200].
- **COMTRADE 내보내기**(CFG line freq=7170 참조).
- **에너지 파일 뷰**(egy<YYYYMM>_m{id}.d 월파일, `^egy\d` 인식·400B/일 레코드).
- **PQ 파일 다중/전체 삭제**(FTP DELE, 설정파일 보호 `_sv_pq_deletable`).
- **Trend 파일 뷰어**(FTP \log_trend 72B 레코드).

## ☑️ 5. VQ(전압품질) 페이지 — web.c 반영 완료
web.c에 **VQ 채널 탭 구현됨**(`/api/vq`·`bVq`, `meter[0].vq` Uud/Uuv, 커밋 93a72d9). Flicker(Pi/Pst/Plt)는 양쪽 미구현. (종합 비교표 1)의 ✅와 일치.

---

## ☑️ 6. 이미 web.c에 반영됨 (참고)
- **시각 로컬화(getUTC\*)**: 펌웨어 ts=로컬 epoch → 브라우저 로컬게터 +9h 이중적용 문제. web.c는 `ts2`가 `fdtU`(getUTC\*) 사용하도록 수정 완료(2026/08/31). `clk`/`dmeas`(브라우저 now)는 로컬게터 유지.
- **FREQ(7170) 설정 UI**: web.c GEN[] Main Setting>General>ETC에 이미 추가됨.
- **Harmonics %x100(/100) 스케일**: web.c·confWebApp 양쪽 수정 완료.

---

## 공통 레지스터 메모 (이식 시 참조)
| 기능 | 레지스터 | 비고 |
|---|---|---|
| ACK Event / Alarm | 7498 / 7494 | 명령(값 쓰기). 블링킹 seen과 병행 |
| Clear Event / Alarm | 7482 / 7478 | 로그 삭제 |
| Event STS / Alarm STS | 7094 / 7093 | ⚠️값 불안정 — 블링킹엔 쓰지 말 것 |
| FREQ | 7170 | 0=60Hz,1=50Hz |
| PQ Report Active / Start Day | 6844 / 6845 | CH0(M0) 기준 |
| INTERRUPTION Level / Action | pqevt[3](=PQE_INTR) | Level=%, Action 2=WAVE CAPTURE |

**시각 규약**: 장치는 시간을 **로컬(KST) epoch로 저장·동기**. 표시는 항상 로컬(브라우저 tz 미적용 = getUTC\*). SNTP 동기 시에만 Timezone(7401) 사용.
