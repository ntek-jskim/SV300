# SV300 펌웨어 — 빌드 · 설정 · 교정 가이드 & 문서 인덱스

LPC4330(Cortex-M4) + ADE9000 ×3(M0~M2) 기반 3채널(9피더) 전력품질 계측기.
CycloneTCP(웹·Modbus TCP·FTP·mDNS·SNMP·MQTT) + RTX, 내장웹(web.c) + 외부 설정툴 confWebApp.

> **이 문서는 web 브랜치 기준**(메모리맵 `_261028`: 통신 설정 7110~7519, 명령 7788~). main 브랜치 주소는 FREQ 이후가 350 작다(명령 7438~).
> 2026-10-16~19 교정 수정(§5.5·§5.6 — `uGainSel`·1P2W 전력·`setGainW` VA·PGA 2x)은 **main 의 `stash@{0}` 에만 있고 web 에는 아직 없다.**

---

## 1. 문서 인덱스

### 1.1 작업 기록
| 문서 | 내용 |
|---|---|
| [WORKLOG.md](WORKLOG.md) | 날짜별 작업 요약 (최신이 위) |cl
| [TODO.md](TODO.md) | 미해결 / 별건 / 실기 검증 대기 작업 |
| [FWUPDATE](FWUPDATE) | 파일·함수 단위 상세 변경 이력 (커밋마다 갱신) |
| [src/README.TXT](src/README.TXT) | 2026-02 ~ 10-07 버전별 이력 원본 — **보존용, 더 이상 갱신 안 함** |

### 1.2 설계 · 참조 문서
| 문서 | 내용 |
|---|---|
| [WEBSERVER_PORTING.md](WEBSERVER_PORTING.md) | 내장웹(web.c) ↔ confWebApp 역할 분담·제한 원칙·지원 현황 |
| [document/SV300_쓰레드구조_분석.md](document/SV300_쓰레드구조_분석.md) | RTOS 태스크 구조·우선순위 |
| [document/ADE9000_멀티쓰레딩_구조.md](document/ADE9000_멀티쓰레딩_구조.md) · [ADE9000_단일스레드_전환설계.md](document/ADE9000_단일스레드_전환설계.md) · [ADE9000_리팩토링_작업로그.md](document/ADE9000_리팩토링_작업로그.md) | ADE9000 버스/스레드 설계 |
| `memorymap/` | Modbus 메모리맵 엑셀 — **파일명 날짜가 최신인 것이 기준** (코드 주석의 옛 파일명은 무시) |
| [snmp.md](snmp.md) | **SNMP 프로토콜 개발자료** — 설계·MIB·PEN·시험방법(TC-01~12)·TODO §8 (SNMP 펌웨어 코드는 web 브랜치, main 미병합) |
| [mqtt_app.c](src/mqtt_app.c) | MQTT 퍼블리셔 — GEMS35x JSON 배열 페이로드(공통 + `fN_*` 피더), 설정은 §7 |

---

## 2. 빌드

### 2.1 툴체인
- **Keil µVision4 `C:\Keil_v473`** 로 빌드한다. Keil_v5 / v5_29 는 헤더 누락·fat 링크 충돌로 실패
- 부트로더는 별도 프로젝트 `D:\PROJECT\SV300_boot`

### 2.2 프로젝트 · 타깃
| 프로젝트 | 용도 | Target | Define (공통부 생략*) |
|---|---|---|---|
| `keil/SV300.uvproj` | **디버깅**(Keil Download) | `SV300` | `METER_TEST_DATA` |
| | | `SV300_3CH` | `METER_TEST_DATA NOSDMEM HWV1 CH3` |
| | | **`SV300_3CH_16M`** ← 개발 | `NOSDMEM HWV1 CH3 HWV2 DTEST` |
| `keil/SV300_app.uvproj` | **필드 펌웨어**(부트로더 + app) | `SV300_app` | `METER_TEST_DATA _WDT_EN` |
| | | `SV300_3CH_app` | `NOSDMEM HWV1 CH3` |
| | | **`SV300_3CH_app_16M`** ← 시험/출하 | `NOSDMEM HWV1 CH3 _WDT_EN HWV2` |

\* 공통: `LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS`

| Define | 의미 |
|---|---|
| `NOSDMEM` | SD 메모리 없음 |
| `HWV1` / `CH3` | 3채널 HW(미터 칩 셀렉트 3개) |
| `HWV2` | SPI NOR flash 16MB |
| `DTEST` | 디버그 시험용 (공장초기화 기본 IP 끝자리 125) |
| `_WDT_EN` | 워치독 활성 |
| `METER_TEST_DATA` | 시험 데이터 (구형 타깃) |

- 흐름: **SV300_3CH_16M 에서 개발·디버깅 → SV300_3CH_app_16M 으로 시험/출하**
- app 프로젝트는 디버그보다 최적화가 높다(uvproj `Optim` 2 vs 1). ISR 이 갱신하는 변수는 반드시 `volatile` (2026-10-07 부팅 홀딩 사례)
- `src/fft.c` 는 **CP949** — UTF-8 로 저장하면 한글 주석이 깨진다
- 커밋 시 [FWUPDATE](FWUPDATE) 에 `YYYY/MM/DD : 내용` 추가 (CLAUDE.md 규칙). FW 버전·빌드일은 [meter.c](src/meter.c#L15) `FW_VER` / `FW_BUILD_*`

---

## 3. 펌웨어 업데이트
| 방법 | 절차 |
|---|---|
| **내장웹** | Maintenance > Firmware Update → `.bin` 선택 → Upload → Apply & Reboot (admin) |
| **confWebApp** | Maintenance > Firmware Update (SV300 전용) — 같은 동작 |
| 수동 | FTP(`ftpuser` / `7300`) 로 `usrApp.bin` 업로드 → Modbus holding **7798 = 0x1234**(reboot) |

- 업로드 이미지는 크기(1KB~1MB)와 서명(offset 28 = `0x5A5A5A5A`)을 검사한다. 리부팅 후 부트로더가 내부 flash 에 굽는다
- 굽는 동안 전원을 끄지 않는다

---

## 4. 기본 설정 · 접속

### 4.1 공장초기화 기본값
| 항목 | 값 |
|---|---|
| IP / Subnet / Gateway | 192.168.9.**124** (`DTEST` 빌드는 125) / 255.255.0.0 / 192.168.1.1 |
| DNS | 168.126.63.1 |
| Modbus TCP | ON(7150) / 502 |
| MQTT · SNMP | OFF |
| 호스트명(mDNS) | `sv300-<SN>.local` (SN 9자리, 예 `sv300-250102003`) — sysName·DHCP·LLMNR 동일 |

### 4.2 계정
| 대상 | 계정 | 권한 |
|---|---|---|
| 내장웹 | `ntek` / `0300` | admin (설정·명령·FW 업데이트) |
| 내장웹 | `sv300` / `0000` | viewer |
| FTP | `ftpuser` / `7300` | 파일 R/W |

> 웹 세션은 서명 쿠키라 장비를 재부팅해도 재로그인이 필요 없다.

### 4.3 설정 저장 규칙
- 웹/Modbus 로 설정을 쓰면 RAM 에만 반영된다 → **Save Settings(holding 7799 = 0x1234)** 로 flash 영속화
- 일부 설정(네트워크, Modbus TCP 사용, MQTT 사용, SNMP 사용 여부·버전 등)은 **재부팅 후 반영**
- **Init Settings(7814)** / 콘솔 `INITDB` = settings.dat 공장초기화 후 리셋. **교정값(FRAM METER_CAL)·알람설정·PQ 이벤트 정의·에너지는 보존**
- 설정 백업/복원: confWebApp Maintenance > Config Import / Export (Main+Channel 레지스터 블록 JSON)

### 4.4 디버그 콘솔 (UART 115200 8N1)
| 명령 | 용도 |
|---|---|
| `DEVINFO` · `HWVER <n>` | 장치정보 / HW 버전 설정 |
| `SNSET <SN>` | 시리얼번호 설정 (`MACSET` 아님 — MAC 은 CPU UID 로 매 부팅 생성) |
| `MODEL <n>` | HW 모델 0=3CH / 1=2CH (METER_CAL 저장) |
| `TIME` | 시각 확인/설정 |
| `INITDB` · `REBOOT` | 설정 공장초기화 / 재시작 |
| `SAVECAL` | 교정값 FRAM 저장 |
| `CPU` | CPU 부하율(500ms) + 태스크별 호출누적 |
| `WVF` / `WVF C` | 파형 스파이크 추적통계 / 클리어 |
| `THD` · `FFTTEST` | THD 즉시출력 / Goertzel vs CZT 대조 |
| `DIR` · `TYPE` · `DEL` · `FORMAT` | 파일시스템 |
| `IG` · `PH` · `WG` · `CLR*` · `DCOS` | 콘솔 교정(아래 §5 웹 교정 권장) |

---

## 5. 교정(튜닝)

confWebApp **Calibration** 화면(SUPER ADMIN)에서 **채널 단위**로 교정한다.
Channel 1~3 = 미터칩 M0~M2, 각 채널 3상 = Feeder 1~3 / 4~6 / 7~9. 채널 안의 상(L1~L3)은 각자 따로 보정값을 계산한다.

### 5.1 레지스터 · 명령 코드
| 레지스터 | 내용 |
|---|---|
| 7800 / 7801~7803 | 교정 명령 ALL / Channel 1~3 (값 = code) |
| 7810 | V-ref (1 = 1V) |
| 7811 | I-ref (1 = 0.1A) |
| 7812 | In-ref (1 = 0.1A) |
| 7813 | Temp-ref (1 = 1℃, 미설정 시 25℃) |

| code | 동작 | code | 동작 |
|---|---|---|---|
| 0 | CAL MODE ON | 11 | CAL MODE OFF |
| 1 / 5 | V Cali / Init | 12 | 교정값 저장(FRAM) |
| 2 / 6 | I Cali / Init | 13 / 14 | Upp(선간) Cali / Init |
| 3 / 7 | Power Cali / Init | 15 / 16 | DC offset Cali / Init |
| 4 / 8 | Phase Cali / Init | 17 / 18 | Temp Cali / Init |
| 9 / 10 | In Cali / Init | | |

- CAL MODE ON(code 0) 상태에서만 1~10·13~18 이 동작한다. 교정 결과는 RAM → code 12 로 저장
- confWebApp **WRITE** = 7810~7813 기록 + code 0 (Cali/Init 전송 시 자동 선행), **SAVE** = code 12 + code 11

### 5.2 보정값이 저장되는 단위 (절차의 근거)
| 보정 | 저장 단위 | 의미 |
|---|---|---|
| 전압 VGAIN | 채널 × 상 × **전압입력 L1/L2/L3** × PT2 범위 | 같은 상 채널이라도 L1·L2·L3 입력마다 따로 → **1P2W(L1)/(L2)/(L3) 3회 교정** |
| 전류 IGAIN | 채널 × 상 | 결선 무관 → 1회 |
| 위상 PHCAL | 채널 × 상 | 결선 무관 → 1회, 단 각 상 전류가 **자기 상 전압**과 짝지어져야 함 |
| 전력 APGAIN | 채널 × 상 | 결선 무관 → 1회. W·VAR·VA·전력량에 함께 적용 |

- 1P2W(Lx) 는 피더 전류채널에 Lx 전압을 연결(ADC_REDIRECT)하므로 **결선 변경 후 재부팅해야 반영**된다
- PT2 ≥150 / <150 은 별도 게인표다 — 현장에서 쓰는 PT2 범위로 교정한다(둘 다 쓰면 둘 다)
- 3P4W 는 상 j 에 Lj 입력 → 1P2W 3회 교정으로 함께 커버된다

### 5.3 준비
- 정전압·정전류 소스 3상: 전압 정격(예 110V), 전류 정격(예 1A), **전류 위상 60° 지상**(I 가 V 보다 늦음)
  - RCJB-903S 는 **−60°** 로 입력. 소스 표기는 장비마다 다르므로 아래 위상 확인으로 판단한다
  - 소스는 교정 내내 이 상태로 둔다 (전압·전류 교정은 위상과 무관)
- 소스 주파수 = 장비 FREQ 설정(7520), CT 설정(CT1·CT2 종류)이 실제와 같을 것
- 2026-10-19 펌웨어(main `stash@{0}`)부터 CT_100mA 전류 PGA 가 2x 로 바뀌었다 — PGA 가 다른 펌웨어로 교정한 장비는 **전류·위상·전력을 다시 교정**한다(전압은 그대로). **현재 web 빌드는 1x**

### 5.4 절차
> ★ 장비의 CAL MODE·기준값은 RAM 이라 **재부팅하면 지워진다**. confWebApp 은 기준값을 **브라우저에 저장**해 두고
> Cali/Init/ALL CLEAR 를 누를 때마다 **기준값 + CAL MODE ON 을 자동으로 다시 보낸다** — 재부팅 후 WRITE 를 따로 누를 필요 없다.
> (WRITE 는 기준값 확인용. 다른 PC·브라우저에서는 기준값을 다시 입력)
>
> ★ **Init → 2~3초 대기 → Cali 는 1회만.** 보정값은 '기존 보정 0' 전제로 절대값을 계산한다. Init 직후 바로 누르면 옛 보정이 섞인 측정값으로 계산되고, Cali 를 두 번 누르면 잔여오차(≈0)로 덮어써 원래대로 돌아간다. (전압·전류·위상 공통, 개선 예정 → TODO. Power 는 누적 방식이라 예외)

**공통 시작**
1. Calibration 화면 Feeder Range **1~9**, Reference 에 튜닝 전압·전류, Phase **60** 입력

**① 전압 튜닝 (L1 → L2 → L3, 3회)**
1. Main Setting > PT: 피더 1~9 를 모두 **1P2W(L1)** → Save → Maintenance > Reboot
2. 재부팅 후 Calibration > Voltage **Init** → V 값이 바뀔 때까지 2~3초 대기 → **Cali** 1회 → 피더 V 가 기준값 Error(%) 이내인지 확인 → **SAVE**
3. 피더 1~9 를 모두 **1P2W(L2)** 로 바꿔 1~2 반복
4. 피더 1~9 를 모두 **1P2W(L3)** 로 바꿔 1~2 반복

**② 전류 튜닝**
1. Main Setting > PT: 피더를 **1P2W(L1), 1P2W(L2), 1P2W(L3)** 순서로 반복 설정(피더 1·4·7=L1, 2·5·8=L2, 3·6·9=L3) → Save → Reboot
   - 각 상 전류가 자기 상 전압과 짝지어져 ③ 위상 튜닝을 같은 설정으로 이어서 할 수 있다
2. 재부팅 후 Current **Init** → 2~3초 대기 → **Cali** 1회 → I 확인 → **SAVE**
   - 측정값이 기준과 **20% 이상** 차이 나는 상은 보정하지 않고 건너뛴다 → CT 설정·결선부터 확인

**③ 위상 · 전력 튜닝** (② 설정 그대로, 소스 60° 지상 그대로, 재부팅 없음)
1. 교정 전 확인: **PHASE(I) ≈ 300°**(= −60°, 지상) · **VAR 이 +** · W ≈ V·I·0.5 (110V·1A → 약 55W / +95var)
   - PHASE(I) ≈ 60° · VAR − 이면 **진상** → 소스 위상 부호를 바꾼다. 이 상태에서 Cali 하면 120° 오차로 계산돼 PHCAL 이 크게 틀어진다
   - 장비는 반시계(+) 각도 표시: V1=0°, V2≈240°, V3≈120°
2. Phase **Init** → 2~3초 대기 → **Cali** 1회 → **SAVE**
   - 위상이 이미 맞으면(atan(VAR/W) ≈ 60.0°) 눌러도 변화가 거의 없다 — 정상
3. Power **Init** → 2~3초 대기 → **Cali** → **SAVE**
   - 기준 = 피상전력: (V-ref × I-ref) / VA 로 전력 게인을 맞춘다. VA 는 위상과 무관하므로 **60° 상태 그대로** 누른다
   - ※ 이 방식(§5.6)은 main `stash@{0}` 펌웨어. **현재 web 빌드의 `setGainW` 는 옛 방식(V·I/W, PF=1 전용)**이라 60° 에서 누르면 안 된다
   - 기존 게인에 누적되므로 Cali 를 다시 눌러도 원래대로 돌아가지 않는다(누를 때마다 2~3초 간격)
   - 기준 대비 20% 넘게 차이 나는 상은 건너뛴다
4. 검증: **W 가 V·I·cos60° 의 Error(%) 이내**(노란 칸 없음), VA ≈ V·I, VAR ≈ V·I·sin60°
   - 2026-10-19 실기: Power Cali 후 0.5% 이내 확인
   - PHASE(I) 표시는 영점교차 측정이라 ±1~2° 흔들릴 수 있다 — 합격 판정은 W 로 한다
5. 끝나면 피더 결선을 현장 설정으로 되돌리고 Save → Reboot

**④ 그 밖의 교정 (필요 시)**
- Neutral(In): 중성선 기준전류 인가 후 Cali
- Temp: 실온에서 Cali (기준온도 = Temp-ref)
- 마지막에 **REPORT** 로 엑셀 성적서

### 5.5 검토: 상별(L1~L3) 튜닝값 저장 (2026-10-16)
단상 피더는 채널의 어느 CT 슬롯에든 L1·L2·L3 중 아무 전압이나 물릴 수 있으므로, 전압 보정값을 입력 상(L1~L3)별로 나눠 둔다.

**저장 — 정상**
- ① 은 피더 9개를 모두 같은 결선(1P2W Lx)으로 두므로 `setGainU` 가 채널마다 고르는 결선 = Lx 로 맞다 → `vgain[채널][슬롯][Lx][PT2]` 에 슬롯별로 저장
- 1P2W 에서도 슬롯별 전압 U[0..2] 를 그대로 유지(ADC_REDIRECT)하므로 슬롯마다 따로 계산된다
- Voltage **Init** 은 현재 Lx 칸만 지운다 → L2·L3 단계가 앞 단계 결과를 지우지 않는다
- 단계마다 **SAVE** 가 필수 — SAVE 없이 재부팅하면 그 단계 결과가 사라진다
- 전류(`igain[채널][슬롯]`)·위상(`phcal[채널][슬롯]`)은 결선과 무관하게 슬롯별 저장. 위상은 에너지를 0 으로 지우기 **전** 값(`cntl.wh/varh`)으로 계산하므로 1P2W 의 L2·L3 슬롯도 정상

**적용(읽기) — 2026-10-16 수정 (실기 검증 대기, main `stash@{0}` 에만 있음 — web 미적용)**
1. **슬롯별 입력상으로 전압 보정값 선택**: `uGainSel()` 이 `buildFeederMap` 으로 슬롯을 쓰는 피더를 찾아 그 피더의 입력상(Z)·PT2 칸을 고른다. `writeGainU`·`setGainU`·`clrGainU`·`setGainUpp`·`clrGainUpp`·파형 스케일이 같은 칸을 쓴다 → 피더별 L1/L2/L3 혼합 단상 결선에서도 튜닝한 칸이 적용된다 (종전: `db.pt[채널 번호]` 하나로 골라 채널 2·3 은 피더 2·3 결선을 봤다)
2. **1P2W 2·3번 슬롯 전력·전력량 복구**: 1P2W 를 '미터 1개 = 피더 1개'로 보던 옛 코드의 `i != 0` 지움을 삭제([ade9000.c](src/ade9000.c) 전력, [meter.c](src/meter.c) energy_scan) → 피더 2·3·5·6·8·9 의 W·VAR·VA·전력량·수요 표시
   - ※ 이 수정 이전 펌웨어로 운용한 1P2W 피더 2·3·5·6·8·9 의 누적 전력량은 0 이었다

### 5.6 검토: Power 교정이 필요한 이유 (2026-10-19, 수정은 main `stash@{0}` 에만 있음 — web 미적용)
- 위상 교정 후에도 W·VAR·VA 가 모두 약 **−2.3%** 였다. 실제 위상각 atan(VAR/W) 은 60.0±0.06° 로 정상 → 위상이 아니라 **전력 크기** 문제
- 전력 레지스터는 PWR_TIME+1 = 8000 샘플 누적을 8192 로 나눈 값으로 보이며(8000/8192 = 0.977), 이를 보정하는 `*8192/8000` 이 `scalePowerAcc`·`scaleEnergy` 에서 주석 처리돼 있다(GEMS7100 도 동일). 이 차이를 **Power Cali(APGAIN)** 가 흡수한다 — 전력량도 함께 맞는다
- `setGainW` 를 V·I/W(PF=1 전용) → **V·I/VA(위상 무관) + 누적**으로 변경해 60° 상태에서 바로 교정한다
- CT_100mA 전류 PGA 1x → **2x**(GEMS7100 과 같은 부담저항) — 소전류 SNR·위상 표시 흔들림 개선. 표시 스케일은 상수 `I_FULL_100mA` 를 절반으로 해 그대로 유지

---

## 6. 주요 명령 레지스터 (holding, 값 0x1234)
| 주소 | 명령 | 비고 |
|---|---|---|
| 7788 | UTC Time | uint32, 로컬시각을 epoch 처럼 기록(장비 RTC = 로컬) |
| 7798 | Reboot | |
| 7799 | Save Settings | |
| 7814 | Init Settings | 공장초기화 |
| 7816~ / 7820~ / 7824~ | Clear demand / minmax / energy (ALL, #1~3) | energy 는 0x5678 = 편집값 적용 |
| 7828~ / 7832~ | Clear alarm / event | |
| 7844~ / 7848~ | Alarm / Event Ack | |

> 시각 규약: 장치는 시간을 **로컬(KST) epoch** 로 저장한다. 표시는 브라우저 tz 미적용(getUTC*). SNTP 동기 시에만 Timezone(7402) 사용.
> 주소는 `MBAD_SET_TS`(7788) 기준 고정 — [meter.h](src/meter.h) 컴파일 타임 검사(`mbad_cmds_check`)로 구조체가 밀리면 빌드 에러.

---

## 7. 통신 설정 (holding 7110~7519, 2026-10-28 재배치)
| 주소 | 묶음 | 주요 항목 |
|---|---|---|
| 7110~7149 | Ethernet | DHCP 7110, IP 7111, Subnet 7115, Gateway 7119, DNS 7123, 호스트명 7127(16워드) |
| 7150~7159 | Modbus | **TCP 사용 7150**(기본 ON, 재부팅 후 반영), 포트 7151, Unit ID 7152, layoutVer 7159(내부값) |
| 7160~7289 | MQTT | 사용 7160(재부팅 후), 포트 7161, 주기 7162(10~300초), QoS 7163, retain 7164, keepalive 7165, 브로커 7168(32워드, IP/도메인), ClientID 7200(빈 값 = `sv300-<SN>`), user 7216, pass 7232, topic 7248(빈 값 = `sv300/<SN>/data`) |
| 7290~7399 | SNMP | 사용 7290, 버전 7291, Trap 7292, Trap IP 7293, 문자열 7300~7395 — 상세 [snmp.md](snmp.md) |
| 7400~7419 | SNTP | 사용 7400, 주기 7401(분), Timezone 7402(분, 서울 540), 서버 7403(16워드, IP/도메인) |
| 7420~7519 | 예약 | 100워드 |

- 문자열은 1워드 = 2글자, **하위 바이트가 앞 글자**, NUL 종료 (confWebApp 코덱 word_order=big·byte_order=little)
- 비밀번호(MQTT·SNMPv3)도 Modbus 로 읽고 쓸 수 있다. MQTT TLS 미지원
- 설정 필드 주소는 `MBAD_SET_FIELD(comm.xxx)`(offsetof) 로 계산 — 메모리맵 `memorymap/SV300 memory map_261028.xlsx`
