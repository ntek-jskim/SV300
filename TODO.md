# TODO — SV300 미해결 / 별건 작업

> 운영 규칙: 완료한 항목은 이 목록에서 **삭제**하고, 결과는 [WORKLOG.md](WORKLOG.md)·[FWUPDATE](FWUPDATE) 에 남긴다.
> 출처: 2026-10-16 위상교정 조사, 차주 계획(08/28), FWUPDATE 의 "실기 검증 필요/예정/보류" 표기, web 브랜치 `snmp.md` §8.
> 기준: main = web `2b0f399`(26/11/06 통합) + `stash@{0}`(10/16~19 교정 수정, 미커밋)

---

## Priority: 브랜치 정리
### `stash@{0}` 삭제
- 11/06 `uGainSel`·`energy_scan` 1P2W 전력량·web.c L2 색 반영(미커밋). PGA 2x·`setGainW` 누적식은 제외 결정(PGA 1x 유지, 전력 게인은 √(P²+Q²) 덮어쓰기 유지), `WV_M0_ONLY` 제외
- **할 일**: 아래 실기 검증(uGainSel·1P2W 전력량) 통과 후 `git stash drop stash@{0}`

---

## Priority: 교정(Calibration) — 출하 품질 직결

### 보정값 누적 계산으로 변경 (V·I·Phase·Power)
- **상태**: 미해결 — 현재는 "Init → 대기 → Cali 1회" 로 회피 (Power 는 11/06 결정으로 덮어쓰기 유지)
- **문제**: `setGainU/Upp/I/W/Ph` 모두 '기존 보정 0' 전제의 절대값 → Init 직후 바로 Cali 하면 옛 보정이 섞인 측정값으로 계산, Cali 를 두 번 누르면 잔여오차(≈0)로 덮어써 원복
- **해결**: 기존 값에 합성 — 게인은 `(1+old)·(ref/meas)−1`, PHCAL 은 각도 합산 후 재계산
- **위치**: [ade9000.c:316 setGainU](src/ade9000.c#L316) · [:406 setGainI](src/ade9000.c#L406) · [:437 setGainW](src/ade9000.c#L437) · [:476 setGainPh](src/ade9000.c#L476)

### 교정 진단 로그·조건 가드
- 위상교정 시 상별 Wh·VARh·오차각·PHCAL 출력(전압/전류는 `vg:`/`ig:` 로그가 있음)
- VAR 부호가 − 이면(진상) 교정 거부 + 경고
- ACCMODE `SELFREQ=60Hz` 고정([ade9000.c:2455](src/ade9000.c#L2455)) — 50Hz 계통 교정·계측 영향 확인

### confWebApp 교정 연동 정리 (confWebApp 저장소)
- Maintenance > Other Commands(GEMS7100/iDPM200): Calibration command·V/I/In-ref 가 0x1234 로 기록됨 — 값/코드 입력 방식으로 수정
- Maintenance Set Time 을 전 제품 로컬시각(Date.UTC)으로 통일 — GEMS7000/7100/7300/9000/SV500 실기 확인

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
| 1P2W 2·3번 슬롯 전력(`2b0f399`)·전력량(`energy_scan`, 11/06 미커밋) 복구 | 11/06 | 전력 W/PF 는 #2·#3 확인됨. 남은 확인: kWh 누적·수요, 채널 합계(P[3]) |
| 전압 보정 슬롯별 Z·PT2·3P3W (`uGainSel`, 11/06 미커밋) | 11/06 | 피더 L1/L2/L3 혼합 결선에서 각 피더 V 가 기준값 이내, 콘솔 `vg:` 로그의 L 표시 |
| confWebApp AUTO CAL (`9f1af4b`) | 11/06 | 13단계 전체 1회 통과(재부팅 4회 대기·판정 멈춤), REPORT 엑셀 |

---

## Priority: 안정성 (장시간 운전)

### SPIFI program FAIL (3~4시간 주기)
- **상태**: 미해결 (11/05~06 콘솔 5회, rc=0x20003, sz 4/8)
- **현상**: 실패 주소가 전부 64KB 블록 마지막 4KB(EFS 할당레코드 영역), 쓰기 주체(egy15·demand·PQ) 무관 → EFS/드라이버 계층 추정
- **관련 발견(11/06)**: `EraseSector` 가 8/21(`9894b95`)부터 소거 실패 시 return 없이 함수 끝으로 빠짐(주석 처리한 printf 때문에 return 이 else 몸체가 됨) → 반환값 미정, 소거 실패가 성공으로 보고됐을 수 있음. 11/06 수정(미커밋) — 이후에도 program FAIL 이 나는지 재관찰
- **할 일**: rc 의미 확인, FAIL 시 대상 영역의 기존 플래시 내용 출력 진단 추가
- **참고**: 새 메인보드는 초기 부품 2MB SPI 플래시(MX id 0x15) 실장 상태 → 16MB 로 교체 예정. 교체 후 재관찰
- **위치**: [FS_SPIFI_FlashPrg.c:184](src/FS_SPIFI_FlashPrg.c#L184)

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

## Priority: 청소 (영향 없음)
- 남은 진단 코드: `IRQ_PHASE_DIAG`(ade9000.c)·`WV_DIAG`(meter.h, [WVF] 요약·lag late 카운터) — M1·M2 IRQ 겹침 검증 완료(11/09~10 하룻밤), 이제 0 으로 끄기 가능. `WV_STAGED` 단계기동 스위치(meter.h·app_main.c, 현재 OFF)는 재진단용으로 보존 여부 결정
- 메모리맵 xlsx 에 캡처 구조체(V+I)·에너지 월포맷 변경 반영 여부 확인 (최신 기준 `memorymap/` 날짜 최신 파일)
- (deferred) M1/M2 WFB 파형 커플링 저감 · GEMS THD 비교
