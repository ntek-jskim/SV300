# TODO — SV300 미해결 / 별건 작업

> 운영 규칙: 완료한 항목은 이 목록에서 **삭제**하고, 결과는 [WORKLOG.md](WORKLOG.md)·[FWUPDATE](FWUPDATE) 에 남긴다.
> 출처: 2026-10-16 위상교정 조사, 차주 계획(08/28), FWUPDATE 의 "실기 검증 필요/예정/보류" 표기, web 브랜치 `snmp.md` §8.
> 기준: web `557ed25`(통신 설정 410워드·MQTT) / main `edc2866`(V00.03) + `stash@{0}`(10/16~19 교정 수정, 미커밋)

---

## Priority: 브랜치 정리
### main `stash@{0}` 교정 수정 적용 위치 결정
- **내용**: `uGainSel`·1P2W 2·3번 슬롯 전력 복구·`setGainW` VA+누적·CT_100mA PGA 2x·Phase L2 색 (+ 진단 `WV_M0_ONLY` 는 제외)
- **할 일**: main 에 커밋할지, web 에 옮길지 결정. web 에 옮기면 필드 타깃 플래시 여유(약 4.3KB) 확인
- 현재 web 빌드는 PGA 1x·옛 setGainW(PF=1 전용) — README §5 절차 중 Power Cali 는 web 빌드에서 60° 로 누르면 안 됨

---

## Priority: 교정(Calibration) — 출하 품질 직결

### 위상 교정 시험조건·절차 확정
- **상태**: 원인 확인(2026-10-16), 재시험 필요
- **현재**: 소스 "60°" 가 진상(I 앞섬)이라 펌웨어 전제(지상 60°, VAR +)와 반대 → PHCAL 오계산
- **할 일**: 소스 전류위상 **−60°(300°, 지상)** 으로 Phase Init → Cali 1회 → 검증. 결과로 README.md §5 절차 확정
- **위치**: [ade9000.c:476 setGainPh](src/ade9000.c#L476)

### 보정값 누적 계산으로 변경 (V·I·Phase — Power 는 10/19 적용)
- **상태**: 미해결 — 현재는 README §5.4 "Init → 대기 → Cali 1회" 로 회피 (Power 는 10/19 누적·VA 기준으로 변경 완료)
- **문제**: `setGainU/Upp/I/W/Ph` 모두 '기존 보정 0' 전제의 절대값 → Init 직후 바로 Cali 하면 옛 보정이 섞인 측정값으로 계산, Cali 를 두 번 누르면 잔여오차(≈0)로 덮어써 원복
- **해결**: 기존 값에 합성 — 게인은 `(1+old)·(ref/meas)−1`, PHCAL 은 각도 합산 후 재계산
- **위치**: [ade9000.c:316 setGainU](src/ade9000.c#L316) · [:406 setGainI](src/ade9000.c#L406) · [:437 setGainW](src/ade9000.c#L437) · [:476 setGainPh](src/ade9000.c#L476)

### 무전류 상 PHCAL 이 0 으로 지워짐
- **상태**: 미해결
- **문제**: 전류 0 인 상은 Wh=VARh=0 → `atan(0/0)`=NaN → 정수변환 0. phcal 은 칩×상 단위라 단상(1P2W Lx) 순차 교정 시 앞 상 결과가 지워진다
- **해결**: |Wh|·|VARh| 가 임계 미만이면 해당 상 skip (setGainI 의 20% 가드처럼)

### 교정 진단 로그·조건 가드
- 위상교정 시 상별 Wh·VARh·오차각·PHCAL 출력(전압/전류는 `vg:`/`ig:` 로그가 있음)
- VAR 부호가 − 이면(진상) 교정 거부 + 경고
- ACCMODE `SELFREQ=60Hz` 고정([ade9000.c:2455](src/ade9000.c#L2455)) — 50Hz 계통 교정·계측 영향 확인

### confWebApp 교정 연동 정리 (confWebApp 저장소)
- Maintenance > Other Commands(GEMS7100/iDPM200): Calibration command·V/I/In-ref 가 0x1234 로 기록됨 — 값/코드 입력 방식으로 수정
- Maintenance Set Time 을 전 제품 로컬시각(Date.UTC)으로 통일 — GEMS7000/7100/7300/9000/SV500 실기 확인
- Calibration W/VAR/VA 를 W 단위 표시로 변경 — 실기 확인

---

## Priority: 파형 캡처 개선 (08/28 차주 계획, 미착수)

### 30사이클 저장 + pre/post 체이닝(wrap 버그)
- **문제**: `wavePreCaptureLF` 가 pprev 한 윈도우(1600샘플=200ms)만 3회 복사 → 유니크 ~12사이클뿐
- **해결**: ① WAVE_WINDOW 링에 600ms 이력이 있는지 조사 후 버퍼 확대 ② pprev→pcur→pnext 체이닝으로 진짜 pre/post 확보(전류 lI 도 동일)
- 참고 벤치마크: PM8240 (사이클당 128/256샘플 × 사이클수, standard/delayed/extended)

### Channel Setting > Waveform 재구성
- pre/post trigger 를 **cycle 설정값**으로(합 ≤30 오류처리), record resolution·parameter 행 삭제
- 펌웨어(설정 레지스터) + web.c + confWebApp channel_setting.html 3곳 + 메모리맵

---

## Priority: 실기 검증 대기 (코드 반영됨)

| 항목 | 반영일 | 확인 내용 |
|---|---|---|
| SSP1 버스트 락 범위 확대 | 09/10 | CH3 전류 고조파 전 차수 100% 재발 여부(장시간) |
| 로그 로테이션·에너지 월단위·캡처 V+I·FTP | 08/28 | 파일 삭제/생성, COMTRADE 6채널, FTP LIST |
| 에너지 15분 아카이브(egy15) | 09/01 | 일변경 +3분 아카이브, 6개월 보존, stale-day 복구 |
| FW 업로드·명령맵·THD·nTurns·ct_dir | 08/24 | 브라우저 업로드→부트로더 굽기, 명령 주소 |
| VQ 전압편차 | 08/26 | 편차값·공칭전압(vnorm) 연동 |
| SNMPv3 (web 브랜치) | 10/12 | snmp.md TC-12, 확인 후 FW_VER 상향 |
| 통신 설정 410워드 재배치 (web) | 10/28 | confWebApp Main Setting 읽기/쓰기, 문자열(호스트명·SNMP), Modbus TCP OFF→재부팅→ON 복구 경로, SNTP 도메인 |
| MQTT 퍼블리시 (web) | 10/28 | 브로커 접속·재접속, JSON 페이로드, 주기·QoS·retain |
| 메모리맵 `_261028` | 10/28 | Excel 에서 한 번 열어 저장(캐시값 생성) 후 주소 확인 |
| (main stash) 1P2W 2·3번 슬롯 전력·전력량 복구 | 10/16 | 피더 2·3·5·6·8·9 W/VAR/VA·kWh 누적·수요, 채널 합계(P[3]) |
| (main stash) CT_100mA 전류 PGA 1x→2x (GEMS7100 동일) | 10/19 | 재교정 후 W 0.5% 이내 확인됨(10/19). 남은 확인: OC·과도 트리거 레벨, 정격 대전류 포화 없음 |
| (main stash) 전압 보정값 슬롯별 입력상 선택(`uGainSel`) | 10/16 | 피더 L1/L2/L3 혼합 결선에서 각 피더 V 가 기준값 이내, 콘솔 `vg:` 로그의 L 표시 |

---

## Priority: 기능 / 정합

### 알람 설정이 재부팅 후에만 적용
- **상태**: 미해결 (snmp.md §8.7 과 동일 항목)
- **문제**: Channel·Cond·Level 변경이 Save Set 후에도 판정값 `alarm.st[]` 에 즉시 반영 안 됨
- **해결**: Save Set 시 `alarm.st[]` 재빌드 또는 almSet 저장/로드 경로 추가

### SNMP 남은 작업 (코드는 web 브랜치 — 상세 [snmp.md](snmp.md) §8)
- 자사 계측 MIB(svFeederTable·svHarmTable·svAlarmTable) + `SV300-MIB.mib`(enterprises 67140, smilint)
- 배포물(MIB 배포 경로·OID/스케일 표·NMS 템플릿), 출하 전 결정(FDB 테이블 노출·CycloneTCP 라이선스) — SNMP_TRACE_LEVEL 은 10/28 WARNING 으로 낮춤
- TC-11·§7.5 부하·§7.6 RSTP 재확인
- (선택) SHA-256 인증

### VQ Flicker(Pi/Pst/Plt) 미구현
- IEC 61000-4-15 플리커미터 미구현으로 0 고정(보류)

### 2026-06-06 기록 TODO — 현행 코드 기준 재확인 필요
- CH0 설정 read(meter flat) vs write(pmset/pqevt) 경로 통일·offsetof 검증
- tid_wave[1/2]·trend M0 고정 등 CH1/CH2 런타임 연결
- Modbus FC3/6/16·명령 큐 3CH 통합 테스트

---

## Priority: 하드웨어

### 랜케이블 착탈 시 리셋 / SWD 통신 깨짐
- 펌웨어 무관 확정(09/02). RJ45 실드 종단 **R214 / CB221(2kV 1000pF) 미실장** → 실드 스너버 실장·실드 접지

---

## Priority: 청소 (영향 없음)
- 진단 스캐폴딩 정리: WV_STAGED·WV_DIAG_METER·WV_M0_ONLY·[WR]/[FLASH]/[QCAP] 로그
- HTTP_TRACE INFO → OFF (확인 후 OFF 예정 표기)
- 메모리맵 xlsx 에 캡처 구조체(V+I)·에너지 월포맷 변경 반영 여부 확인 (최신 기준 `memorymap/` 날짜 최신 파일)
- (deferred) M1/M2 WFB 파형 커플링 저감 · GEMS THD 비교
