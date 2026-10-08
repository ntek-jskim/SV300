# SV300 SNMP 적용 검토

> 작성: 2026-10-08 · 대상: SV300 (LPC4330 + CycloneTCP 2.4.4 + Keil RTX, 3채널 ADE9000)
> 목적: SV300에 SNMP 프로토콜을 추가할 때 필요한 배경지식, 업계 현황, 적용 사례, 가능 여부와 제약사항 정리

---

## 0. 요약

- **SNMP 에이전트는 이미 SV300 펌웨어에서 동작하고 있다.** CycloneTCP RSTP 데모 코드(`src/main.c:630-672`)가 그대로 남아 있어 v1/v2c, UDP 161 포트, `public`(RO)/`private`(RW) 커뮤니티로 응답한다.
- 다만 노출되는 MIB는 **SNMPv2-MIB(system 그룹)·BRIDGE-MIB·RSTP-MIB뿐이고 계측값은 하나도 없다.** sysDescr/sysName도 `"Description"`, `"Name"` 같은 데모 기본값이다.
- **보안상 바로 조치할 항목이 있다.** 현재 `private` 커뮤니티로 BRIDGE/RSTP-MIB를 쓸 수 있어서(`dot1dStpPortEnable` 등 15개 RW 객체), 원격에서 스위치 포트를 비활성화해 장비를 네트워크에서 끊을 수 있다.
- 결론: **적용 가능하다(리소스·스택 모두 충분).** 필요한 작업은 ① 기본 설정 정리와 보안 조치, ② 자사 Enterprise MIB(계측값) 작성, ③ Trap(알람/이벤트) 연동, ④ 웹 설정 UI, ⑤ SNMPv3이다.
- **1차 범위 확정(2026-10-08)**: 읽기 전용 + Trap(SET 없음), SNMPv3 포함, BRIDGE/RSTP-MIB는 RO로 유지, 계측 항목은 Modbus Simple 맵(50000~) 기준, IANA PEN은 신규 신청. → §4.6, §5, §6 참고
- **진행 상황**: FW V00.04(26/10/08)에 §4.5-①(private 삭제, sysDescr/sysName 실값, 호스트명 `sv300-<SN>` 통일)을 반영했다. 위 첫 세 항목은 V00.03 이하 펌웨어 기준이다. 26/10/08 빌드에 SNMP 설정(사용 여부·버전·커뮤니티·sysContact/Location, 웹 SNMP 카드)과 알람 Trap(§5.4)을 추가했고 실기 Trap 수신을 확인했다. FW_VER는 SNMPv3 시험 후 올린다. 시험 방법은 §7 참고

---

## 1. 정의: SNMP란 무엇인가

### 1.1 개요
**SNMP(Simple Network Management Protocol)** 는 IP 네트워크에 연결된 장비의 상태를 조회·설정하고 이벤트를 통보하기 위한 IETF 표준 관리 프로토콜이다. 원래 라우터·스위치 관리용으로 만들어졌지만, 지금은 UPS·PDU·항온항습기·전력계 같은 **설비(Facility) 장비 감시**에도 널리 쓰인다.

### 1.2 구성 요소

| 구성 요소 | 역할 | SV300에서 |
|---|---|---|
| **Manager (NMS)** | 장비를 폴링하고 Trap을 수신하는 감시 서버 | Zabbix, PRTG, LibreNMS, SolarWinds, 고객 DCIM/EMS |
| **Agent** | 장비 안에서 요청에 응답하고 Trap을 발신 | SV300 펌웨어(CycloneTCP `snmp_agent`) |
| **MIB** | 장비가 제공하는 객체(변수)의 정의서(ASN.1/SMIv2 텍스트) | 표준 MIB + **자사 Enterprise MIB(신규 작성 필요)** |
| **OID** | MIB 객체의 고유 주소(트리 경로). 예: `1.3.6.1.2.1.1.5.0` = sysName | 자사 트리는 `1.3.6.1.4.1.<PEN>.…` |

### 1.3 동작 방식(PDU)

| 동작 | 방향 | 설명 |
|---|---|---|
| `GET` / `GETNEXT` | Manager → Agent | 값 1개 또는 다음 값 조회 (walk) |
| `GETBULK` (v2c+) | Manager → Agent | 테이블 일괄 조회 |
| `SET` | Manager → Agent | 값 쓰기(설정 변경) |
| `TRAP` | Agent → Manager | 비동기 이벤트 통보(응답 없음, 유실 가능) |
| `INFORM` (v2c+) | Agent → Manager | 응답을 확인하는 Trap(재전송 있음) |

- 전송: **UDP 161**(요청/응답), **UDP 162**(Trap 수신)
- 데이터형: INTEGER, Counter32/64, Gauge32, TimeTicks, OCTET STRING, IpAddress 등. **부동소수점(float) 타입은 없다**(계측기에 중요한 제약, §4.4 참고).

### 1.4 버전 비교

| 버전 | 표준 | 인증/보안 | 비고 |
|---|---|---|---|
| **v1** | RFC 1157 (1990) | 커뮤니티 문자열(평문) | 32bit 카운터만 지원. 레거시 |
| **v2c** | RFC 1901/3416 | 커뮤니티 문자열(평문) | GETBULK, Counter64, INFORM 추가. **현재 가장 많이 쓰이는 버전** |
| **v3** | RFC 3411~3418 (STD 62) | USM(사용자 인증 MD5/SHA, 암호화 DES/AES), VACM(접근제어) | 보안 요구가 있는 현장의 표준. 공공·금융·통신사 납품 시 요구되는 경우가 많다 |

---

## 2. 현재 SNMP 운영 현황

### 2.1 업계 전반
- **IT 네트워크 장비**: 여전히 사실상 표준이다. 다만 대형 네트워크에서는 성능 모니터링용으로 NETCONF/YANG, gNMI(스트리밍 텔레메트리)가 늘어나는 추세이고, SNMP는 상태 감시와 Trap 위주로 쓰인다.
- **데이터센터·통신국사 설비 감시**: UPS(RFC 1628 UPS-MIB), PDU, 항온항습기, 발전기, 전력계를 DCIM/NMS에서 하나로 묶는 **공통 언어**로 쓰인다. 전력 계측기가 SNMP를 지원하면 별도 게이트웨이 없이 기존 NMS에 바로 붙일 수 있다는 점이 판매 포인트다.
- **전력·PQ 분야**: 주력 프로토콜은 Modbus TCP/RTU, IEC 61850, DNP3, BACnet, 최근에는 MQTT다. SNMP는 **"IT 쪽 감시 시스템과의 연동 옵션"** 이라는 위치에 있다.

### 2.2 보안 동향
- v1/v2c 커뮤니티 문자열은 평문이라 스니핑에 노출된다. 기본값 `public`/`private`은 대표적인 취약점 점검 항목이다(KISA 주요정보통신기반시설 취약점 점검 가이드, CIS 벤치마크 등).
- 공공기관·금융·통신사 납품 시 흔한 요구사항:
  - 기본 커뮤니티 변경 또는 비활성화
  - SNMP 기능 ON/OFF 설정
  - v3(authPriv: SHA + AES) 지원
  - 접근 허용 IP 제한
- **SET(쓰기)은 대부분 쓰지 않는다.** 실무에서는 읽기 전용 + Trap 구성이 일반적이다.

### 2.3 자사(SV300) 현황 — 코드 확인 결과

| 항목 | 현재 상태 | 근거 |
|---|---|---|
| 에이전트 | **빌드에 포함, 부팅 시 자동 시작** | `src/main.c:630-672`, `SV300_app.uvproj`에 `snmp_agent*.c` 포함 |
| 지원 버전 | v1 ~ v2c (v3 비활성) | `main.c:633-635`, `snmp_common.h:53` `SNMP_V3_SUPPORT DISABLED` |
| 커뮤니티 | `public`(RO), `private`(RW) **하드코딩** | `main.c:658-663` |
| 로드된 MIB | SNMPv2-MIB, BRIDGE-MIB, RSTP-MIB | `main.c:651-655` |
| system 그룹 값 | sysDescr=`"Description"`, sysName=`"Name"`, sysContact=`"Contact"`, sysObjectID=`0` | `cyclone_tcp/mibs/snmp_mib_impl.c:69-100` (데모 기본값) |
| 호스트명 | `APP_HOST_NAME "rstp-bridge-demo"` | `main.c:103` |
| 계측값 MIB | **없음** | — |
| Trap 발신 | 사용하는 곳 없음(`snmp_agent_trap.o`가 링크되지 않음) | `SV300_3CH_app.map` |
| 원격 쓰기 위험 | `private`로 `dot1dStpPortEnable`, `dot1dStpPriority` 등 **RW 객체 15개** 쓰기 가능 | `cyclone_stp/mibs/bridge_mib_module.c`(9), `rstp_mib_module.c`(6) |
| 설정 UI | 없음(웹·Modbus·Shell 어디서도 설정 불가) | `src/` 전체 검색 |
| 미사용 MIB 소스 | MIB-II, IF-MIB, IP/TCP/UDP-MIB, LLDP-MIB, SNMPv3 USM/VACM-MIB 등이 저장소에만 있음 | `cyclone_tcp/mibs/` |
| 라이선스 | `GPL_LICENSE_TERMS_ACCEPTED` 정의로 빌드 중 | `SV300_app.uvproj:358` |

> ⚠ **즉시 조치 권고**: 지금 출하되는 펌웨어는 같은 네트워크의 누구든 `snmpset -c private`로 KSZ8863 스위치 포트를 비활성화할 수 있다(장비 통신 두절). SNMP 개발 일정과 별개로 **`private` 커뮤니티 삭제(또는 RO로 변경)** 를 먼저 반영하는 것이 좋다.

---

## 3. 멀티채널 계측기·PQM의 SNMP 적용 사례

> 아래 내용은 공개된 제품 자료 기준이다. 모델·옵션 모듈·펌웨어 버전에 따라 지원 범위가 다르므로, 벤치마크에 쓰기 전에 최신 데이터시트로 다시 확인할 것.

### 3.1 주요 제품

| 분류 | 제조사 / 제품(예) | SNMP 적용 형태 |
|---|---|---|
| 고급 PQM | Schneider PowerLogic ION9000 / PM8000 | 이더넷 포트 내장 SNMP, 자사 MIB 제공, 알람 Trap |
| 범용 전력계 | Schneider PowerLogic PM5500 계열 | 이더넷 모델에서 SNMP(v1/v2c) 조회 |
| PQM / 에너지계 | Janitza UMG 604-PRO, UMG 96-PA, UMG 509/512 | SNMP v1/v2c/v3 지원, Janitza 전용 MIB |
| 범용 전력계 | Accuenergy Acuvim II + AXM-WEB2 모듈 | 옵션 이더넷 모듈에서 SNMP v1/v2c/v3 |
| 전력계(데이터센터) | Eaton Power Xpert Meter 계열 | SNMP 조회와 Trap, 웹 설정 |
| **멀티채널(분기회로) 계측** | 데이터센터용 BCM(Branch Circuit Monitor), 지능형 PDU(Raritan, Vertiv/Geist, ServerTech 등) | **채널(회로)별 테이블 MIB**로 전류·전력·전력량을 제공하고 회로별 임계치 Trap 발신. 멀티채널 SNMP의 사실상 표준 형태 |
| UPS | 각사 UPS + 네트워크 카드 | 표준 UPS-MIB(RFC 1628) + 자사 MIB |

### 3.2 공통 설계 패턴(벤치마크 포인트)

1. **채널/회로 테이블 구조**: `circuitTable`처럼 채널 인덱스 × 상(phase) 인덱스로 행을 구성한다. 채널 수가 바뀌어도 MIB은 그대로 쓸 수 있다.
2. **스케일 정수 표현**: float이 없으므로 `Integer32`에 ×10, ×100을 곱하거나(예: 전압 0.1V 단위) 단위·소수점 자리를 MIB `DESCRIPTION`/`UNITS` 절에 명시한다. 문자열(`DisplayString`)을 병행 제공하는 제품도 있다.
3. **전력량은 Counter64**(v2c 이상)이다. 32bit는 금방 넘친다.
4. **알람은 Trap/Inform으로 보낸다**: 과전압·저전압·과전류·Sag/Swell·THD 초과·DI 변화 등. Trap 수신지를 2~4개까지 설정할 수 있게 하는 경우가 많다.
5. **임계치 설정은 웹에서**: SNMP SET을 지원하더라도 실무에서는 대부분 웹 UI에서 설정한다.
6. **표준 MIB 함께 제공**: SNMPv2-MIB(system), IF-MIB/MIB-II(인터페이스 상태)를 같이 구현해 NMS 자동 인식(장비 분류·템플릿 매칭)이 되게 한다.
7. **MIB 파일 배포**: 홈페이지·웹 UI에서 `.mib` 파일을 받을 수 있게 하고, Zabbix/PRTG용 템플릿까지 제공하면 경쟁력이 커진다.

### 3.3 SV300에 대응시킨 MIB 구조(초안 — 확정안은 §5)

```
1.3.6.1.4.1.<PEN>                         -- 자사 Enterprise (IANA PEN 필요)
 └─ .1 sv300
     ├─ .1 svIdent        모델, 시리얼, FW 버전, 채널 수, 결선방식
     ├─ .2 svChannelTable INDEX { chIdx }           -- 채널(ADE9000 M0~M2)별 합계값
     │      kW, kVAR, kVA, PF, Hz, 총 THD …
     ├─ .3 svPhaseTable   INDEX { chIdx, phaseIdx } -- 채널×상별 값
     │      V, I, kW, PF, THD-V, THD-I, Sag/Swell 카운트 …
     ├─ .4 svEnergyTable  INDEX { chIdx }           -- Counter64 kWh/kVARh (수전/송전)
     ├─ .5 svAlarm        현재 알람 상태 비트맵, 마지막 이벤트 시각
     └─ .6 svTraps        svAlarmRaised / svAlarmCleared / svPowerEvent …
```

---

## 4. 자사 제품(SV300) 적용 가능 여부

### 4.1 결론: **가능하다**

SNMP 에이전트 스택은 이미 링크되어 동작 중이다. 새로 만들 것은 **자사 MIB 모듈(계측값 연결)과 설정/보안**이다. 하드웨어 변경은 필요 없다.

### 4.2 리소스 영향(현재 맵 파일 `SV300_3CH_app.map` 기준)

| 항목 | 현재 | 비고 |
|---|---|---|
| SNMP 에이전트 코드 | 약 **9 KB** (agent*.o + snmp_mib + mib_common + asn1) | 이미 포함됨 |
| BRIDGE/RSTP-MIB | 약 **8 KB** 코드 + 4.6 KB const | 삭제하면 그만큼 확보 |
| 자사 MIB 추가 예상 | 코드 5~10 KB + OID 테이블 수 KB | 전체 ROM 약 350 KB 대비 여유 있음 |
| RAM | 에이전트 컨텍스트(메시지 버퍼 1452B × 송수신) + 태스크 스택 2.5 KB | RTX `OS_STKSIZE=640워드`가 모든 태스크에 강제 적용됨(`document/SV300_쓰레드구조_분석.md` §0) |
| 소켓 | UDP 1개(요청) + Trap 송신 | `SOCKET_MAX_COUNT 16` 안에서 해결 |
| CPU | 폴링 시에만 짧게 처리 | 실측 부하율 약 25%. NORMAL 우선순위라 계측(REALTIME) 태스크에 영향 없음 |
| SNMPv3 암호화 | md5/sha1/hmac/aes/des/cbc/cfb가 **이미 프로젝트에 포함**됨 | v3을 켜도 추가 crypto 코드는 거의 없음 |

### 4.3 장점
- **스택이 이미 있어서 개발 리스크와 공수가 적다.** CycloneTCP의 MIB 모듈 구조(`snmp_mib_module.c`/`_impl.c`)를 그대로 따라 작성하면 된다.
- **데이터센터·통신·IDC 시장 진입**: 기존 NMS/DCIM에 게이트웨이 없이 연결된다. 멀티채널 SV300은 BCM/PDU 감시 수요와 잘 맞는다.
- **Trap 기반 즉시 통보**: 폴링 주기를 기다리지 않고 알람이 바로 NMS에 전달된다(기존 `alarm.c` 이벤트를 재활용).
- **기존 데이터 재사용**: Modbus·웹과 같은 `meter[]` 구조체를 데이터 소스로 쓰므로 계측 로직 변경이 없다.
- 입찰 사양서의 "SNMP 지원" 항목을 충족한다.

### 4.4 단점·제한사항

| 구분 | 내용 | 대응 |
|---|---|---|
| **보안** | v1/v2c는 평문 커뮤니티. 현재 `private` RW 노출 상태(§2.3) | RW 커뮤니티 삭제, 커뮤니티를 웹에서 설정, SNMP ON/OFF, 허용 IP 필터, 필요 시 v3(authPriv) |
| **float 타입 없음** | 계측값 표현이 번거롭다 | Integer32 스케일(V×10, I×1000, PF×1000 등)과 MIB `UNITS` 명시. 스케일 정의를 Modbus 맵과 맞추면 문서화가 쉽다 |
| **데이터 일관성** | GETBULK 도중 1초 계산 주기가 바뀌면 상 간 값이 섞일 수 있다 | Modbus Simple 맵의 이중버퍼 스냅샷 `psmpMap`(`modbus.c:268` `copySimpleMap`, 500ms, writer 1개)을 그대로 읽는다. 요청 1건 처리 동안 포인터를 한 번만 잡으면 된다 |
| **Enterprise OID** | 자사 PEN이 필요하다 | IANA PEN 신청(무료, 보통 수일~수주). 회사에 이미 PEN이 있는지 먼저 확인 |
| **Trap 신뢰성** | UDP라 유실될 수 있다 | 중요 이벤트는 INFORM 또는 반복 발신, NMS 주기 폴링과 병행 |
| **대용량 데이터 부적합** | 파형·고조파 전 차수·트렌드 이력은 SNMP에 맞지 않는다 | 실시간 대표값과 알람만 SNMP로 제공. 파형·이력은 기존 FTP/웹 경로 유지 |
| **SET 지원 범위** | 원격 설정 변경은 보안과 검증 부담이 크다 | 1차는 **읽기 전용 + Trap**만. SET은 요구가 있을 때 제한적으로 검토 |
| **태스크/RAM** | SNMP 태스크 1개(2.5 KB 스택) 상시 존재 | 이미 동작 중이라 추가 부담 없음. BRIDGE/RSTP-MIB를 빼면 오히려 줄어든다 |
| **RSTP 동작과의 관계** | 현재 RSTP 브리지 자체는 SNMP와 별개로 동작 | BRIDGE/RSTP-MIB를 빼도 RSTP 기능은 유지된다(MIB 노출만 제거). 링 토폴로지 감시가 필요하면 RO로만 유지 |
| **라이선스** | CycloneTCP를 `GPL_LICENSE_TERMS_ACCEPTED`로 빌드 중 | SNMP 전용 이슈는 아니지만 상용 배포 조건(Oryx 상용 라이선스 보유 여부와 범위)을 확인할 것 |
| **검증** | NMS 호환성(MIB 컴파일, walk, Trap 파싱) | net-snmp(`snmpwalk`/`snmptrapd`), Zabbix/PRTG로 상호운용 시험 |

### 4.5 구현 단계(확정 범위 기준)

1. **보안·기본값 정리** — PEN 없이 바로 진행 가능, 우선 반영
   - `private` RW 커뮤니티 삭제(`main.c:662`). CycloneTCP는 요청자의 커뮤니티/사용자 권한으로 SET을 막으므로, RW 권한이 하나도 없으면 **BRIDGE/RSTP-MIB를 포함한 모든 MIB가 자동으로 RO**가 된다. BRIDGE/RSTP-MIB 로드(`main.c:653-655`)는 그대로 둔다.
   - sysDescr/sysName/sysContact/sysLocation을 실제 제품 정보로 설정하고 `APP_HOST_NAME`을 정리한다. sysObjectID는 PEN을 받은 뒤 설정한다.
   → 검증: `snmpwalk -v2c -c public <ip> system` 값 확인, `snmpset -v2c -c private <ip> 1.3.6.1.2.1.17.2.15.1.4.1 i 2` 실패 확인
2. **설정 항목 + 웹 UI**: SNMP 사용 여부, 허용 버전(v2c+v3 / v3 전용), RO 커뮤니티, v3 사용자(이름·인증 비밀번호·암호화 비밀번호), Trap 수신지 2개(IP·버전)
   - 저장 위치는 **`METER_DEF`/`SETTINGS` 밖의 별도 구조**로 한다. `METER_DEF`는 Modbus 맵 오버레이라 중간 크기가 바뀌면 뒤 레지스터가 밀리고, 비밀번호가 Modbus로 노출될 위험도 있다.
3. **SNMPv3**
   - `net_config.h`에 `SNMP_V3_SUPPORT ENABLED` 추가. `snmp_agent_usm.c`는 이미 프로젝트에 있다(현재 비활성이라 480B).
   - 사용자는 `snmpAgentCreateUser(…, SNMP_ACCESS_READ_ONLY, …)`로 **RO 전용** 생성. VACM(`SNMP_AGENT_VACM_SUPPORT`)은 1차에 끈다(사용자 단위 RO로 충분).
   - 기본 보안 수준은 **authPriv = HMAC-SHA-1 + AES-128**. 필요한 sha1/hmac/aes/cfb가 이미 링크되어 있다. SHA-256을 원하면 `sha256.c` 추가 + `SNMP_SHA256_SUPPORT ENABLED`.
   - engineID는 RFC 3411 형식(`0x80000000 | PEN` + MAC)으로 장비마다 고유하게 만든다 → **PEN이 필요**하다. 발급 전에는 임시값으로 개발한다.
   - 비밀번호→키 변환(1MB 해시)은 무겁다. 부팅 시와 설정 변경 시에만 1회 수행하고 키를 보관한다.
4. **자사 MIB 구현**(§5): `SV300-MIB.mib` 텍스트 + CycloneTCP MIB 모듈(`sv300_mib_module.c`/`_impl.c`)
   → 검증: `smilint` 통과, `snmpwalk` 값이 Modbus 50000~ 맵을 읽은 값과 일치
5. **Trap 연동**: `alarm.c` 알람 발생/해제 → 큐 → SNMP 쪽에서 `snmpAgentSendTrap()` 호출(계측/FS 태스크를 막지 않게). v2c는 커뮤니티로, v3는 RO 사용자로 송신. INFORM은 1차 범위에서 제외(`SNMP_AGENT_INFORM_SUPPORT` 비활성 유지).
   → 검증: `snmptrapd`로 v2c/v3 Trap 수신 확인
6. **배포물**: `SV300-MIB.mib`, 매뉴얼의 OID·스케일 표, (선택) Zabbix/PRTG 템플릿

### 4.6 확정 사항 (2026-10-08)

| 항목 | 결정 | 반영 위치 |
|---|---|---|
| 1차 범위 | **읽기 전용 + Trap**, SET 미포함 | RW 커뮤니티/사용자 없음, §4.5-1·5 |
| SNMPv3 | **포함** (authPriv SHA-1 + AES-128, RO 사용자) | §4.5-3 |
| IANA PEN | **없음 → 신규 신청** | §6 |
| BRIDGE/RSTP-MIB | **RO로 노출 유지** | §4.5-1 |
| 계측 항목·스케일 | **Modbus 맵 기준**(Simple 맵 50000~) | §5 |
| Trap 대상 | **`alarmProc`(alarm.c) 알람 항목** — 발생·복귀 모두 | §5.4 |
| 미사용 피더 | **행 숨김** | §5.1 |
| 고조파 테이블 | **1차 포함** | §5.3 |

---

## 5. 자사 MIB 설계 — Modbus Simple 맵 기준

### 5.1 원칙
- **항목·순서·단위는 Modbus Simple 맵(`meter.h:1947-2021`, `memorymap/SV300 memory map_260812.xlsx`의 `'#1,2,3 Simple'` 시트)을 그대로 따른다.** 메모리맵은 파일명 날짜가 가장 최신인 엑셀이 기준이다(2026-10-08 현재 `_260812`). Modbus 매뉴얼과 SNMP 매뉴얼의 항목이 1:1로 대응된다.
- **데이터 소스는 `psmpMap` 스냅샷**(PostScan_Task가 500ms마다 이중버퍼로 발행)이다. 계측 코드는 바꾸지 않고, 웹/Modbus와 값이 항상 같다.
- 채널 인덱스 = Simple 맵의 피더 번호 `#1~#9`(`MAX_CH`=9). 3상은 #1~#3, 단상은 #1~#9를 쓴다.
- **미사용 피더는 행을 숨긴다(확정).** 피더 배정이 유효한 피더(`buildFeederMap()`의 `valid`, 결선 설정이 NOT_USED가 아닌 피더)만 `svFeederTable`·`svHarmTable`의 행으로 응답한다. 숨긴 피더는 GET에 `noSuchInstance`, walk에서는 건너뛴다. 결선 설정을 바꾸면 다음 스냅샷(500ms)부터 행 구성이 바뀐다.

### 5.2 스케일 — ⚠ Modbus와 그대로 같을 수 없는 부분
Simple 맵의 아날로그 값은 **IEEE754 float32**인데 **SNMP에는 float 타입이 없다.** 그래서 다음 두 가지로 나눈다.

| 구분 | Modbus 타입 | SNMP 표현 |
|---|---|---|
| 정수 항목(utc, 버전, 상태, 전력량, 고조파) | U16/U32/S16 | **Modbus 원값 그대로**(스케일·단위 동일) |
| float 항목(V, I, P, PF, THD 등) | float32 | **고정소수점 Integer32**(아래 배율) + MIB `UNITS`/`DESCRIPTION`에 명시 |

float 항목 배율(2026-10-08 확정):

| 항목 | 단위 | 배율 | 예 |
|---|---|---|---|
| Freq | 0.01 Hz | ×100 | 60.00 Hz → 6000 |
| U, Upp (평균 포함) | 0.01 V | ×100 | 220.55 V → 22055 |
| I (평균 포함) | 0.01 A | ×100 | 12.34 A → 1234 |
| In (누설전류) | 0.001 A | ×1000 | 0.123 A → 123 |
| P, Ptot / Qtot / Stot | 1 W / var / VA | ×1 | Integer32 범위 ±2.1 GW로 충분 |
| PFtot | 0.001 | ×1000 | 0.985 → 985 (부호 유지) |
| Uangle, Iangle | 0.1 ° | ×10 | |
| Uunb, Iunb, THD_U, THD_I, TDD_I | 0.01 % | ×100 | |
| temp | 0.1 ℃ | ×10 | |

- 전력량(kWh_imp 등)은 Modbus U32 원값 그대로 **Gauge32(Unsigned32)** 로 둔다. Counter32는 "감소하지 않음"이 전제인데, 이번달/전월 값이나 에너지 클리어 명령(0x1234)으로 값이 줄어들 수 있기 때문이다. 단위는 맵 시트를 따른다(`web.c:427`은 0.1 kWh로 해석).
- 고조파는 Modbus S16 원값(`hd.U/hd.I`) 그대로 둔다. 단위는 맵 시트를 따른다.

### 5.3 OID 트리(안)

```
1.3.6.1.4.1.<PEN>                              -- 자사 Enterprise (IANA PEN, §6)
 └─ .1 sv300
     ├─ .1 svCommon          (scalar)  ← SMP_COMMON  50000~
     │     .1 svUtc  .2 svHeartbit  .3 svHwModel  .4 svHwVer  .5 svFwVer
     │     .6 svFwBuildDate(DisplayString "YY/MM/DD")  .7 svSerial
     │     .8 svStatus  .9 svTemp(×10)  .10 svFeederCount
     ├─ .2 svFeederTable     INDEX { svFdIdx(1..9) }  ← SMP_CHANNEL 50020+100n
     │     cbType, cbStatus, Freq,
     │     U1,U2,U3,Uavg, U12,U23,U31,Uppavg, Uangle1..3,
     │     I1,I2,I3,Iavg, In, Iangle1..3,
     │     P1,P2,P3, Ptot, Qtot, Stot, PFtot,
     │     kWhImp, kWhImpThisMon, kWhImpLastMon, kVARhImp, kVAh,
     │     Uunb, Iunb, THD_U1..3, THD_I1..3, TDD_I1..3
     ├─ .3 svHarmTable       INDEX { svFdIdx, svHarmType(1=U,2=I), svHarmPhase(1..3), svHarmOrder(1..41) }
     │     svHarmValue (S16 원값)                     ← SMP_HARMONICS 50920+246n
     ├─ .4 svAlarmTable      INDEX { svAlmMeter(1..3), svAlmSlot(1..32) }   ← ALARM_DEF/ALARM_STATUS
     │     svAlmChan, svAlmChanName, svAlmCond, svAlmLevel, svAlmStatus, svAlmCount
     ├─ .5 svNotifications   svAlarmRaised / svAlarmCleared (§5.4)
     └─ .6 svConformance     MODULE-COMPLIANCE / OBJECT-GROUP
```

- `svHarmTable`은 **1차에 포함한다(확정).** 최대 9×2×3×41 = 2,214행이다. GETBULK로 문제없이 읽히지만, NMS 기본 템플릿에서는 THD/TDD만 폴링하고 고조파는 필요할 때만 walk하도록 안내한다.
- `svAlarmTable`은 설정된 알람 슬롯(`chan != 0`)만 행으로 응답한다. Trap을 놓쳤을 때 NMS가 현재 알람 상태를 폴링으로 다시 맞출 수 있게 하는 용도다.
- Modbus 맵이 바뀌면(예: 260812 Angle 추가) MIB도 **컬럼을 끝에 추가**하는 방식으로만 확장한다. 기존 OID 번호는 바꾸지 않는다(SMIv2 규칙).

### 5.4 Trap 설계 — `alarmProc` 알람 연동

**발생 원천(코드 확인 결과)**
- `alarmProc(id)`(`alarm.c:1011`)가 1초마다 물리 미터별(M1~M3, `id`=0~2)로 실행되어, 알람 설정 32슬롯(`ALARM_DEF.set[32]`: chan·cond·level·dband)을 평가한다.
- 상태가 바뀌는 지점은 두 곳뿐이며 **둘 다 `storeAlarmLog(id, slot, status, value)`(`alarm.c:757`)를 거친다.**
  - 발생: 조건 성립이 `delay`초 이상 지속 → `status=1`
  - 복귀: 데드밴드를 벗어나 `delay`초 이상 지속 → `status=0`
- 감시 대상 항목은 알람 채널 표 `almTbl`(최대 124개, 메모리맵 `'Alarm Channel'` 시트)이다. `almTbl[].nm`이 항목 이름이다(예: `Temp.`, `Freq.`).
- 부팅 5초 이내, RMS 미초기화, 무전압(ZX 없음), 주파수 47~63Hz 범위 밖에서는 알람을 평가하지 않는다. 그래서 이 구간에서는 Trap도 나가지 않는다.

**Trap 정의**

| Notification | 발생 시점 | OID(안) |
|---|---|---|
| svAlarmRaised | `storeAlarmLog(..., status=1, ...)` | `.<PEN>.1.5.0.1` |
| svAlarmCleared | `storeAlarmLog(..., status=0, ...)` | `.<PEN>.1.5.0.2` |

varbind(두 Trap 공통):

| 객체 | 형식 | 원천 |
|---|---|---|
| svAlmMeter | Integer32 (1~3) | `id + 1` |
| svAlmSlot | Integer32 (1~32) | `slot + 1` |
| svAlmChan | Integer32 | `ALARM_DATA.chan` (Alarm Channel 시트 번호) |
| svAlmChanName | DisplayString | `almTbl[id][chan].nm` |
| svAlmCond | INTEGER { under(0), over(1), equal(2) } | `ALARM_DATA.cond` |
| svAlmLevel | DisplayString (예: `"242.00"`) | `ALARM_DATA.level` |
| svAlmValue | DisplayString (예: `"245.31"`) | `storeAlarmLog`의 `value` |
| svAlmTime | Unsigned32 | 이벤트 시각(`alog.ts`) |

- **설정값·측정값은 DisplayString(문자열)로 보낸다.** 알람 채널은 전압·전류·전력·%·Hz·℃ 등 단위가 124가지로 섞여 있어서, §5.2처럼 항목마다 배율을 정하면 NMS가 채널별 배율표를 알아야 한다. 문자열은 NMS 화면과 메일 알림에 그대로 읽히는 값으로 표시된다.
- **알람은 피더가 아니라 물리 미터 기준이다.** 단상 결선에서는 미터 1개가 피더 3개로 나뉘지만 알람은 미터 단위로 평가된다. 그래서 Trap 인덱스는 `svFdIdx`가 아니라 `svAlmMeter`를 쓴다.

**구현 방식**
- `storeAlarmLog()` 끝에서 Trap 요청을 **큐에 넣기만 한다**(메터링 경로를 막지 않음). 실제 송신은 SNMP 쪽 태스크가 큐를 비우며 `snmpAgentSendTrap()`으로 한다.
- 수신지 2개(§4.5-2 설정)에 v2c(커뮤니티) 또는 v3(RO 사용자)로 보낸다. INFORM(수신 확인·재전송)은 1차에서 제외한다.
- 큐가 가득 차면 가장 오래된 요청부터 버린다. 놓친 상태는 NMS가 `svAlarmTable`을 폴링해 복구한다.

**알람 리셋 명령 — A안 적용(2026-10-08 확정)**
- 알람 리셋 명령(`cntl.rstAlmList = 0x1234`, `meter.c`)은 `resetAlarm()`으로 알람 상태를 한꺼번에 지우며 `storeAlarmLog()`를 거치지 않는다. 그래서 리셋 시 별도 Trap **`svAlarmReset`**(varbind: svAlmMeter, svAlmTime) 1건을 보낸다.

**구현 (2026-10-08, `src/snmp_trap.c`)**

| 항목 | 내용 |
|---|---|
| 연결 지점 | `storeAlarmLog()` 끝(발생·복귀), 알람 리셋 처리 2곳(`meter.c`) → `snmpTrapAlarm()` / `snmpTrapAlarmReset()`는 큐(16건)에 넣기만 함 |
| 송신 | 전용 태스크 `snmpTrap`(LOW 우선순위)가 큐를 비우며 `snmpAgentSendTrap()` 호출. SNMPv2c, 커뮤니티 `public`, UDP 162 |
| 숫자형 설정 (COMM_CFG) | 구 DAQ 필드 삭제 후 예약 영역 재배치, 구조체 60워드(7110~7169) 불변 — 컴파일 타임 검사 `comm_cfg_layout_check`. SNMP 6워드는 구 DAQ 설정 자리에 연속 배치: **7155 `snmpEn`**(0:OFF, 1:ON, **부팅 시 반영**), **7156 `trapEn`**(0:OFF, 1:ON, 저장 즉시 반영), **7157~7160 `trap_ip`**. **7153 `snmpVer`**(0:v1/v2c, 1:v1/v2c/v3, 2:v3 only, 부팅 시 반영 — v3 미지원 FW는 값과 무관하게 v1/v2c, 웹에서 v3 항목은 비활성 표시, 2 초과 시 부팅 때 0으로 정리). 7150~7152·7162~7169 예약. **`dhcpEn`(7154, 제품군·confWebApp 공통)과 `layoutVer`(7161, 내부값)는 이동 금지**. Save Set 시 `layoutVer`는 펌웨어 값으로 강제 저장. 부팅 시 `snmpEn`/`trapEn`이 0/1이 아니면(구 DAQ 잔존값) SNMP 6워드를 0으로 정리 |
| SNMP 사용 여부 | OFF면 에이전트·Trap 모두 기동하지 않음(UDP 161 닫힘). 값 그대로 운영하므로 **업데이트한 기존 기기·공장초기화 기기는 0 = OFF** — SNMP를 쓰려면 웹에서 ON 후 Save Set + Reboot |
| 문자열 설정 (snmp.d) | `\system\snmp.d`(`src/snmp_cfg.c`): 읽기 커뮤니티(Trap 공용), sysContact, sysLocation(각 최대 32자, `"`·`\` 제외 ASCII) + SNMPv3 사용자·키 자리(§4.5-③에서 사용). Modbus 맵 밖이라 Modbus로 읽히지 않음. 파일 없으면 커뮤니티 `public`. **부팅 시 적용**. 웹 저장은 FS 큐로 기록(핸들러에서 FS 락 미보유). **공장초기화(INITDB) 시 파일 삭제** |
| 웹 화면 | Setup > General 탭 **"SNMP" 카드**: SNMP / SNMP Version / Trap / Trap Server(레지스터). SNMP가 OFF면 아래 항목 전부 잠금 + Community / sysContact / sysLocation(`/api/snmp`). 커뮤니티는 값을 내려주지 않고(기본값 여부만) 새로 입력할 때만 저장. 저장은 관리자만 |
| 알림 전용 객체 | SV300-MIB `.<PEN>.1.5.1.{1..8}` = svAlmMeter, svAlmSlot, svAlmChan, svAlmChanName, svAlmCond, svAlmLevel, svAlmValue, svAlmTime (accessible-for-notify — GET으로는 읽히지 않음) |
| Trap OID | `.<PEN>.1.5.0.1` svAlarmRaised, `.0.2` svAlarmCleared, `.0.3` svAlarmReset |
| svAlmTime | 이벤트 시각 `alog.ts`(= `sysTick1s`, 알람 로그와 같은 시각값) |
| 임시 PEN | **32473**(RFC 5612 문서용 예시 번호). `SV_PEN_OID`(BER 바이트) 매크로만 IANA 발급 번호로 교체 — **출하 전 필수** |
| 메모리맵 | `memorymap` 최신 엑셀의 COMM 영역 갱신 필요: 7150~7153 DAQ server ip → 예약, 7155~7160 DAQ 설정 → SNMP(snmpEn, trapEn, trap_ip) |
| sysORTable | SV300-MIB가 추가 로드되어 제공 MIB 목록이 4개(SNMPv2-MIB, BRIDGE-MIB, RSTP-MIB, SV300-MIB)가 된다 → §7 TC-02 기대값은 다음 FW 버전에서 갱신 |

---

## 6. IANA PEN(Private Enterprise Number) 신청 방법

PEN은 `1.3.6.1.4.1.<번호>` 아래의 자사 OID 트리를 쓰기 위한 번호다. 자사 MIB OID, sysObjectID, SNMPv3 engineID에 쓰인다.

### 6.1 신청 전 확인
1. **기존 등록 여부 검색**: <https://www.iana.org/assignments/enterprise-numbers/> 에서 회사명(영문 상호, 과거 상호, 모회사·계열사 포함)을 검색한다. 이미 회사 이름으로 등록된 번호가 있으면 새로 신청하지 말고 등록 담당자에게 하위 할당을 받는 것이 IANA 권고다.
2. **조직당 1개 원칙**: 제품별로 따로 받지 않는다. 회사 번호 하나를 받아 그 아래를 제품별로 나눈다(예: `.<PEN>.1` = SV300, `.<PEN>.2` = 다음 제품).
3. **신청 명의**: 개인이 아닌 **회사 명의**로, 연락처는 퇴사해도 유지되는 **공용 메일**(예: 대표 기술지원 메일)로 한다. 이 정보는 IANA 레지스트리에 공개된다.

### 6.2 신청 절차
1. 신청 페이지 접속: <https://www.iana.org/assignments/enterprise-numbers/assignment/apply>
2. 입력 항목
   - **Assignee**: 이름, 조직명(영문 법인명), 조직 주소, 국가, 전화
   - **Contact**: 담당자 이름, 주소, 국가, 전화, 팩스(선택), **이메일(필수)**
3. 개인정보 처리방침·이용약관에 동의하고 제출한다.
4. 입력한 이메일로 **확인 요청 메일**이 오면 응답한다(응답해야 처리가 시작된다).
5. 확인 후 보통 **7일 이내**에 번호가 배정되어 메일로 통보된다.
- **비용: 무료**
- 문의: iana-pen@iana.org
- 등록 정보(담당자, 이메일 등)가 바뀌면 같은 사이트에서 수정을 요청할 수 있다.

### 6.3 발급 전후 개발 진행
- 발급 전: 코드에서 PEN을 **매크로 하나**(`SV_ENTERPRISE_PEN`)로 두고 임시값으로 개발·시험한다. 임시값은 사내 시험망에서만 쓰고 **출하 펌웨어에는 절대 넣지 않는다.**
- 발급 후: 매크로 값만 바꾸면 MIB OID, sysObjectID(`1.3.6.1.4.1.<PEN>.1`), engineID에 한꺼번에 반영된다. `SV300-MIB.mib`의 `enterprises <PEN>` 줄도 함께 수정한다.

> 참고: [IANA PEN 신청 페이지](https://www.iana.org/assignments/enterprise-numbers/assignment/apply), [RFC 9371 — PEN Registration](https://www.rfc-editor.org/rfc/rfc9371.html)

---

## 7. SNMP 시험 방법 (FW V00.04, 26/10/08 빌드)

이 장은 SV300 SNMP 기능을 시험하거나 NMS(감시 시스템)에 연동하는 사람이 그대로 따라 할 수 있도록 작성했다. 조회 시험은 **Paessler SNMP Tester**(무료), Trap 시험은 **Wireshark**(무료) 기준이다.

> FW_VER는 SNMPv3 시험 완료 후 올릴 예정이라 V00.04 그대로다. 아래 내용은 **26/10/08 빌드**(SNMP 설정 카드·Trap 포함) 기준이며, 그 이전 V00.04 빌드에는 SNMP 설정·Trap이 없다.

### 7.1 SNMP 기능 범위

| 항목 | 현재 펌웨어 | 비고 |
|---|---|---|
| SNMP 사용 | **설정(기본 OFF)** | 웹 SNMP 카드 / Modbus 7155. 변경 후 재부팅 |
| SNMP 버전 | **v1, v2c** | 버전 선택(7153)은 있으나 v3는 다음 FW에서 지원 |
| 포트 | **UDP 161**(조회), **UDP 162**(Trap 송신) | |
| 커뮤니티 | **설정값(기본 `public`) — 읽기 전용** | 쓰기(SET) 가능한 커뮤니티 없음. Trap에도 같은 값 사용 |
| SET(쓰기) | **미지원** | 모든 SET 요청은 응답 없이 폐기된다 |
| 제공 MIB | SNMPv2-MIB(RFC 3418) system·snmp 그룹, BRIDGE-MIB(RFC 4188), RSTP-MIB(RFC 4318), SV300-MIB(Trap 알림 객체만) | 표준 MIB는 대부분의 NMS에 기본 내장 |
| 계측값 MIB(SV300-MIB) | 미제공 | 이후 버전에서 제공 예정(§5) |
| Trap | **SNMPv2c, 알람 발생/복귀/리셋 3종** | 수신지 1곳(웹 SNMP 카드 / Modbus 7156~7160). 저장 즉시 반영 |
| sysContact / sysLocation | **설정** | 웹 SNMP 카드, 최대 32자 ASCII. 재부팅 후 반영 |
| sysName / 호스트명 | `sv300-<SN>` | mDNS(`sv300-<SN>.local`)·LLMNR·DHCP 호스트명과 같은 값 |
| 임시 Enterprise 번호 | **32473** | RFC 5612 문서용 예시 번호. IANA PEN 발급 후 교체 |

`<SN>`은 장비 시리얼 번호에서 만든 9자리 숫자다(예: `sv300-250102003`). 부팅 콘솔 로그 `[WEB] mDNS: http://sv300-xxxxxxxxx.local/` 줄이나 웹 화면에서 확인한다.

### 7.2 시험 환경

```
  [시험 PC] ──── [스위치] ──── [SV300 ETH1]
   Paessler                     SV300 ETH2 ── (RSTP 링 시험 시에만 스위치에 추가 연결, §7.6)
```

- **네트워크**: 시험 PC와 SV300이 같은 서브넷에 있고 UDP 161 통신이 막혀 있지 않아야 한다(PC 방화벽, 중간 방화벽 확인).
- **표기**: `<IP>`는 SV300 IP 주소, `<SN>`은 위 시리얼 9자리, `<COMM>`은 장비에 설정한 커뮤니티(기본 `public`)다.

#### 7.2.0 시험 전 장비 설정 (필수)

SNMP는 기본 OFF다. 시험 전에 장비 웹에서 켠다.

1. 장비 웹 로그인(관리자) → **Setup → General 탭 → SNMP 카드**
2. **SNMP** ON → (선택) Community / sysContact / sysLocation 입력 → **Save**
3. Setup → Command → **Reboot** — SNMP ON/OFF, Version, Community, sysContact/Location은 재부팅 후 반영
4. Trap 시험(TC-10)을 할 경우: SNMP 카드에서 **Trap** ON, **Trap Server**에 시험 PC IP 입력 → Save(재부팅 불필요)

- SNMP가 OFF면 아래 항목(Version, Trap, Community 등)은 잠겨서 수정할 수 없다.
- 커뮤니티는 화면에 현재 값을 보여 주지 않는다(기본값이면 "public (default) - change!", 바꾼 적이 있으면 "(unchanged)").

#### 7.2.1 시험 도구

| 도구 | 비용·라이선스 | 조회(GET/Walk) | 쓰기(SET) 요청 | 용도 |
|---|---|---|---|---|
| ① **Paessler SNMP Tester** | 무료 | O | X | **이 장의 시험 기준 도구** |
| ② WSL + Net-SNMP | 무료(오픈소스) | O | O | SET 요청을 실제로 보내는 정식 쓰기 거부 시험(TC-05 참고) |
| ③ iReasoning MIB Browser | 무료판은 **개인 용도 전용**, 업무용은 유료 | O | O | 유료판을 보유한 경우 ②를 대신할 수 있음 |
| ④ **Wireshark** | 무료(오픈소스, 업무 사용 가능) | — | — | **Trap 수신 확인(TC-10)** — Paessler는 Trap을 받지 못한다 |

- **① 설치**: <https://www.paessler.com/tools/snmptester> 에서 **Free Download**(이메일 입력을 요구할 수 있음) → `Paessler SNMP Tester Setup.exe` 실행
- **② 설치**: 관리자 권한 PowerShell에서 `wsl --install` 실행 후 재부팅 → Ubuntu 터미널에서 `sudo apt update && sudo apt install snmp snmp-mibs-downloader`
- **③ 안내**: <https://ireasoning.com/browser/help.shtml>. 무료판은 업무·상업 용도로 쓸 수 없고 MIB 파일도 10개까지만 불러올 수 있다.
- **④ 설치**: <https://www.wireshark.org> → 설치 시 **Npcap** 함께 설치. 랜카드에서 패킷을 직접 잡으므로 Windows 방화벽 설정 없이 Trap이 보인다.

### 7.3 Paessler SNMP Tester 사용법

#### 7.3.1 화면 설정

**1. Set SNMP Settings**

| 칸 | 값 |
|---|---|
| Local IP | SV300과 같은 서브넷의 PC 네트워크 카드 IP |
| Device IP/Port | SV300 IP / `161`, `v4` 선택 |
| SNMP Version | `SNMP v2c` (시험 항목에 따라 변경) |
| Community | `public` (시험 항목에 따라 변경) |
| Timeout | `2` (초) |

**2. Select Request Type**에서 요청 종류를 고르고, **오른쪽 입력칸에 OID를 넣은 뒤** **Start**를 누른다. 결과는 **3. Run Test** 창에 표시되고 **Save Log to File**로 저장할 수 있다.

| 요청 종류 | 입력칸 | 동작 |
|---|---|---|
| Custom OID | 값 하나의 OID (예: `1.3.6.1.2.1.1.1.0`) | 값 1개 조회 |
| Walk | 시작 OID (예: `1.3.6.1.2.1.1`) | 시작 OID부터 차례로 조회 |
| Read Device Uptime | (없음) | 장비 업타임 조회 |
| Repeat every □ seconds | (체크 + 초) | 위 요청을 지정 간격으로 반복 |

- 입력칸을 비워 두면 `Please enter a valid OID in 'Custom OID'.` 오류가 난다.

#### 7.3.2 Paessler 사용 시 알아 둘 점

- **Walk는 지정한 OID에서 멈추지 않고 MIB 끝까지 계속 조회한다.** 판정에 필요한 줄이 나오면 **Stop**으로 멈추고, 해당 OID로 시작하는 줄만 보고 판정한다.
- **MAC 주소의 `00` 바이트를 `20`으로 표시한다**(예: 실제 `1E:35:10:00:A0:21` → `1E 35 10 20 A0 21`). 일부 MAC은 `???`처럼 깨져 보인다. 장비 문제가 아니므로 MAC 값은 판정에 쓰지 않는다.
- **재전송 횟수를 지정할 수 없다.** 그래서 거부 카운터는 정확히 1이 아니라 **증가했는지**로 판정한다.
- **쓰기(SET) 요청 기능이 없다.** 쓰기 거부는 TC-05의 방법으로 확인한다.

#### 7.3.3 올바른 응답을 판단하는 기준

시험 요청은 두 종류다. 정상 요청에는 **값이 돌아와야** 하고, 거부되어야 하는 요청에는 **아무 값도 돌아오지 않아야** 한다.

**① 정상 요청 → 값 응답이 올바른 응답**

Run Test 창에 다음 형식의 줄이 나오면 장비가 응답한 것이다. `<값>`이 항목별 기대값과 같으면 합격이다.
```
<시각> (<경과 ms>) : <OID> = "<값>" [<데이터형>]
```
```
2026-10-08 10:33:33 (29 ms) : 1.3.6.1.2.1.1.1.0 = "SV300 FW V00.04 (26/10/08)" [ASN_OCTET_STR]
```
- 데이터형: `ASN_OCTET_STR`=문자열, `ASN_INTEGER`=정수, `ASN_COUNTER`=카운터, `ASN_TIMETICKS`=시간(1/100초), `ASN_OBJECT_ID`=OID

**② 거부되어야 하는 요청 → 응답 없음(timeout)이 올바른 응답**

SV300은 허용되지 않은 요청(잘못된 커뮤니티, 지원하지 않는 버전, 쓰기 요청)에 **오류 응답을 보내지 않고 요청을 버린다.** 그래서 OID 값 줄이 나오지 않고, Timeout 시간이 지난 뒤 응답 없음(timeout) 오류가 표시된다(오류 문구는 Paessler 버전에 따라 다를 수 있다).
- 응답 없음만으로는 네트워크 문제와 구분되지 않는다. 반드시 **바로 앞뒤에 정상 요청을 보내 장비가 응답하는지 확인**하고, 해당하는 거부 카운터가 **증가했는지**를 함께 본다.

| 거부 카운터 | OID | 증가하는 경우 |
|---|---|---|
| snmpInBadCommunityNames | `1.3.6.1.2.1.11.4.0` | 등록되지 않은 커뮤니티(예: `private`)로 요청 |
| snmpInBadCommunityUses | `1.3.6.1.2.1.11.5.0` | 읽기 전용 커뮤니티로 쓰기(SET) 요청 |

- 지원하지 않는 버전(v3) 요청은 카운터를 올리지 않고 버려진다. `snmpInBadVersions`(`1.3.6.1.2.1.11.3.0`)는 v3 요청에도 0 그대로다. v3 거부는 응답 없음과 앞뒤 정상 응답으로만 판정한다.

### 7.4 시험 항목

#### TC-01 기본 응답 / 펌웨어 확인
- 설정: v2c / public
- 요청: Custom OID = `1.3.6.1.2.1.1.1.0` → Start
- 올바른 응답:
  ```
  1.3.6.1.2.1.1.1.0 = "SV300 FW V00.04 (26/10/08)" [ASN_OCTET_STR]
  ```
- 판정: 응답이 오고 버전·날짜가 시험 대상 펌웨어와 같으면 합격.

#### TC-02 system 그룹
- 설정: v2c / public
- 요청: Walk = `1.3.6.1.2.1.1` → Start, `1.3.6.1.2.1.11.`로 시작하는 줄이 나오면 Stop
- 올바른 응답:
  ```
  1.3.6.1.2.1.1.1.0 = "SV300 FW V00.04 (26/10/08)" [ASN_OCTET_STR]   ← sysDescr
  1.3.6.1.2.1.1.2.0 = "0.0" [ASN_OBJECT_ID]                          ← sysObjectID (자사 OID 발급 후 변경 예정)
  1.3.6.1.2.1.1.3.0 = "<숫자>" [ASN_TIMETICKS]                       ← sysUpTime (다시 조회하면 증가)
  1.3.6.1.2.1.1.4.0 = "<설정한 sysContact>" [ASN_OCTET_STR]         ← sysContact (미설정 시 "")
  1.3.6.1.2.1.1.5.0 = "sv300-<SN>" [ASN_OCTET_STR]                   ← sysName
  1.3.6.1.2.1.1.6.0 = "<설정한 sysLocation>" [ASN_OCTET_STR]        ← sysLocation (미설정 시 "")
  1.3.6.1.2.1.1.7.0 = "4" [ASN_INTEGER]                              ← sysServices
  1.3.6.1.2.1.1.9.1.3.1 = "SNMPv2-MIB" [ASN_OCTET_STR]               ← 제공 MIB 목록
  1.3.6.1.2.1.1.9.1.3.2 = "BRIDGE-MIB" [ASN_OCTET_STR]
  1.3.6.1.2.1.1.9.1.3.3 = "RSTP-MIB" [ASN_OCTET_STR]
  1.3.6.1.2.1.1.9.1.3.4 = "SV300-MIB" [ASN_OCTET_STR]
  ```
- 판정: 위 값과 같으면 합격. `"Description"`, `"Name"`, `"Contact"`, `"Location"`이 보이면 V00.03 이하 펌웨어이므로 불합격. 제공 MIB가 3개(SV300-MIB 없음)면 26/10/08 이전 빌드다.
- 아래 시험의 설정 `public`은 장비에 설정한 커뮤니티 `<COMM>`으로 바꿔 읽는다.

#### TC-03 SNMP 버전별 동작

| 순서 | 설정 | 요청 | 올바른 응답 |
|---|---|---|---|
| 1 | `SNMP v1` / public | Custom OID `1.3.6.1.2.1.1.5.0` | `1.3.6.1.2.1.1.5.0 = "sv300-<SN>"` |
| 2 | `SNMP v2c` / public | Custom OID `1.3.6.1.2.1.1.5.0` | `1.3.6.1.2.1.1.5.0 = "sv300-<SN>"` |
| 3 | `SNMP v3` / 아무 사용자명(예: `test`), Authentication `MD5`, Password `12345678` | Custom OID `1.3.6.1.2.1.1.5.0` | **응답 없음(timeout)** — V00.04는 v3 미지원 |
| 4 | `SNMP v2c` / public | Custom OID `1.3.6.1.2.1.1.5.0` | 다시 `sv300-<SN>` 응답(3번의 무응답이 네트워크 문제가 아님을 확인) |

- 판정: 위 4단계가 모두 맞으면 합격.

#### TC-04 잘못된 커뮤니티 거부

| 순서 | 설정 | 요청 | 올바른 응답 |
|---|---|---|---|
| 1 | v2c / `public` | Custom OID `1.3.6.1.2.1.11.4.0` | 현재값 기록 → (A) |
| 2 | v2c / **`private`** | Custom OID `1.3.6.1.2.1.1.5.0` | **응답 없음(timeout)** |
| 3 | v2c / `public` | Custom OID `1.3.6.1.2.1.11.4.0` | (A)보다 **증가** |

- 판정: 2번이 응답 없음이고 카운터가 증가하면 합격. 2번에서 `sv300-<SN>`이 돌아오면 불합격(V00.03 이하 펌웨어).
- 커뮤니티를 `private`으로 설정한 장비라면 2번에는 설정값과 다른 아무 문자열을 쓴다.

#### TC-05 쓰기(SET) 거부

Paessler에는 SET 기능이 없으므로 다음 세 가지가 모두 확인되면 **"쓰기 불가"로 판정**한다.
1. TC-04가 합격이다(쓰기 권한이 있던 `private` 커뮤니티가 없다).
2. `public`은 읽기 전용이다(펌웨어 사양, §7.1).
3. 아래 값이 시험 전체 기간 동안 바뀌지 않는다(시험 시작과 끝에 각각 조회).

| 확인 객체 | 설정 / 요청 | 올바른 응답 |
|---|---|---|
| sysName | v2c / public, Custom OID `1.3.6.1.2.1.1.5.0` | `sv300-<SN>` 유지 |
| 포트1 활성 상태 | v2c / public, Custom OID `1.3.6.1.2.1.17.2.15.1.4.1` | `1` 유지 |
| 포트2 활성 상태 | v2c / public, Custom OID `1.3.6.1.2.1.17.2.15.1.4.2` | `1` 유지 |
| 쓰기 거부 카운터 | v2c / public, Custom OID `1.3.6.1.2.1.11.5.0` | 기록(정식 시험의 기준값) |

> **정식 쓰기 거부 시험**(외부 인증·납품 시험에 권장): SET 요청을 실제로 보낼 수 있는 도구(§7.2.1 ② 또는 ③)로 sysName(`1.3.6.1.2.1.1.5.0`)과 포트1 활성 상태(`1.3.6.1.2.1.17.2.15.1.4.1`, 값 `2`=비활성화)에 쓰기를 시도한다. 올바른 응답은 **응답 없음(timeout)** 이고, 이후 Paessler로 조회했을 때 값이 그대로이며 쓰기 거부 카운터(`1.3.6.1.2.1.11.5.0`)가 증가해 있어야 한다. Net-SNMP 예: `snmpset -v2c -c public -r 0 -t 2 <IP> 1.3.6.1.2.1.17.2.15.1.4.1 i 2`

#### TC-06 BRIDGE/RSTP-MIB 조회
- 설정: v2c / public
- 요청: Walk = `1.3.6.1.2.1.17` → Start, 판정할 줄이 나오면 Stop
- 올바른 응답(주요 줄):
  ```
  1.3.6.1.2.1.17.1.2.0 = "2" [ASN_INTEGER]          ← 포트 수
  1.3.6.1.2.1.17.2.1.0 = "3" [ASN_INTEGER]          ← 프로토콜: ieee8021d
  1.3.6.1.2.1.17.2.2.0 = "32768" [ASN_INTEGER]      ← 브리지 우선순위(기본값)
  1.3.6.1.2.1.17.2.4.0 = "<숫자>" [ASN_COUNTER]     ← 토폴로지 변경 횟수
  1.3.6.1.2.1.17.2.15.1.3.1 = "5" [ASN_INTEGER]     ← 포트1 상태
  1.3.6.1.2.1.17.2.15.1.3.2 = "6" [ASN_INTEGER]     ← 포트2 상태
  1.3.6.1.2.1.17.2.15.1.4.1 = "1" [ASN_INTEGER]     ← 포트1 활성
  1.3.6.1.2.1.17.2.15.1.4.2 = "1" [ASN_INTEGER]     ← 포트2 활성
  1.3.6.1.2.1.17.2.16.0 = "2" [ASN_INTEGER]         ← STP 버전: rstp
  ```
- 포트 상태 값: 1=disabled, 2=blocking, 3=listening, 4=learning, **5=forwarding**(케이블 연결·통신 중), **6=broken**(케이블 미연결). 위 예는 ETH1만 연결한 경우다.
- 판정: 위 값과 맞으면 합격. 일반 NMS에서는 RSTP-MIB 객체(`1.3.6.1.2.1.17.2.16` 이후)가 이름 없이 숫자 OID로 보일 수 있으며 정상이다.

#### TC-07 전체 조회
- 설정: v2c / public
- 요청: Walk = `1.3.6.1.2.1` → Start (Stop 하지 않고 끝까지)
- 올바른 응답: `1.3.6.1.2.1.1.` → `1.3.6.1.2.1.11.` → `1.3.6.1.2.1.17.` 순으로 값이 이어지다가 오류 없이 스스로 멈춘다.
- 판정: 중간에 timeout 없이 끝나면 합격. 같은 LAN에 MAC이 많으면 포워딩 테이블(`1.3.6.1.2.1.17.4.3`) 때문에 1분 이상 걸릴 수 있으며 정상이다.

#### TC-08 호스트명 일치
- Paessler에는 이 기능이 없으므로 Windows 명령 프롬프트를 쓴다.
  ```
  ping sv300-<SN>.local
  ping sv300-<SN>
  ```
- 올바른 응답: 두 명령 모두 `<SV300 IP>의 응답`(영문 Windows는 `Reply from <SV300 IP>`)이 나오고, `<SN>`이 TC-02의 sysName과 같다.
- DHCP 모드라면 DHCP 서버(공유기)의 임대 목록에 호스트명 `sv300-<SN>`이 표시되는지도 확인한다.

#### TC-09 재부팅 후 복귀
1. 설정: v2c / public, 요청 종류 **Read Device Uptime**, **Repeat every** 체크 후 `1` seconds → Start
2. SV300 전원을 껐다 켠다.
3. 올바른 응답: 전원이 꺼져 있는 동안 응답 없음(timeout)이 반복되다가, 부팅 후 업타임 값이 다시 표시되고 작은 숫자부터 다시 증가한다.
- 판정: 응답이 돌아오고 업타임이 처음부터 다시 증가하면 합격. 전원을 켠 시각과 첫 응답 시각의 차이를 복귀 시간으로 기록한다.

#### TC-10 Trap (알람 발생 / 복귀 / 리셋)

**준비**
1. 시험 PC에서 Wireshark 실행 → SV300과 같은 네트워크의 랜카드 선택 → 표시 필터 `snmp` (또는 `udp.port == 162`)
2. 장비 SNMP 카드: **Trap** ON, **Trap Server** = 시험 PC IP → Save (§7.2.0)
3. 알람 설정: 장비 웹 Settings(Channel Setting) → **Alarm Settings** 탭에서 슬롯 하나 설정 → Save & Apply → **장비 재부팅**
   - Channel·Cond·Level은 **재부팅 후 적용**된다(현재 펌웨어 동작). Hyst(%)와 Alarm Def 탭의 `compare Time Delay(s)`(기본 1초)는 즉시 적용된다.
   - 알람은 **전압이 들어와 있고 주파수가 47~63Hz일 때만** 판정된다. 전압을 완전히 끄는 방식으로는 발생·복귀 모두 판정되지 않는다.
   - 예(110V 인가 상태): **U1 `>` 50** → 인가 상태에서 바로 발생. 복귀까지 보려면 전압을 낮출 수 있는 가변 전원(예: 30V)이나 전류 채널(**I1 `>` 0.5**, 부하 전류 인가/차단)을 쓴다.

**시험 항목**

| # | 시험 | 방법 | 올바른 결과 |
|---|---|---|---|
| TR-01 | 알람 발생 | 조건 성립 상태 유지 → delay 경과 | `svAlarmRaised` 1건 |
| TR-02 | 알람 복귀 | 조건 해제(Level·Hyst 범위 밖) → delay 경과 | `svAlarmCleared` 1건 |
| TR-03 | 알람 리셋 | 장비 웹 대시보드 **Clear Alarm**(전체) | `svAlarmReset` 3건(M1~M3 각 1건). 채널별 Clear는 1건. 조건이 계속 성립하면 곧이어 `svAlarmRaised`가 다시 온다 |
| TR-04 | Trap OFF | Trap OFF → Save → 알람 발생 | Trap 없음 |
| TR-05 | SNMP OFF | SNMP OFF → Save → Reboot → 알람 발생 | Trap 없음 |

**Wireshark에서 확인할 값** (Trap 패킷 → `Simple Network Management Protocol` 펼침)

| 항목 | 올바른 값 |
|---|---|
| 송신 → 수신 | SV300 IP → 시험 PC IP, UDP 162 |
| version / community | `v2c (1)` / `<COMM>` |
| PDU | `snmpV2-trap` |
| varbind 1 | `1.3.6.1.2.1.1.3.0` (sysUpTime) |
| varbind 2 | `1.3.6.1.6.3.1.1.4.1.0` (snmpTrapOID) = `1.3.6.1.4.1.32473.1.5.0.`**1**(발생) / **2**(복귀) / **3**(리셋) |

발생·복귀 Trap의 나머지 varbind(리셋은 미터·시각 2개만):

| OID | 항목 | 예 |
|---|---|---|
| `1.3.6.1.4.1.32473.1.5.1.1.0` | 미터 번호(1~3) | `1` |
| `1.3.6.1.4.1.32473.1.5.1.2.0` | 알람 슬롯(1~32) | `1` |
| `1.3.6.1.4.1.32473.1.5.1.3.0` | 알람 채널 번호 | `3` |
| `1.3.6.1.4.1.32473.1.5.1.4.0` | 알람 채널 이름 | `U1` |
| `1.3.6.1.4.1.32473.1.5.1.5.0` | 조건 (0:under, 1:over, 2:equal) | `1` |
| `1.3.6.1.4.1.32473.1.5.1.6.0` | 설정값(문자열) | `"50.00"` |
| `1.3.6.1.4.1.32473.1.5.1.7.0` | 측정값(문자열) | `"110.12"` |
| `1.3.6.1.4.1.32473.1.5.1.8.0` | 이벤트 시각(초) | 장비 알람 로그 시각과 같음 |

- 판정: TR-01~05가 표대로 나오고 varbind 값이 장비 웹 Alarm Log와 일치하면 합격.
- 보조 확인: 장비 콘솔에 송신마다 `Sending SNMP message to <PC IP> port 162`, 실패 시 `[SNMP] trap(N) M1 send failed (...)`가 출력된다.
- Trap은 UDP라 수신 확인·재전송이 없다. 알람이 한꺼번에 많이 나면 큐(16건)를 넘는 오래된 요청은 버려진다.

#### TC-11 SNMP 설정 반영

| # | 방법 | 올바른 결과 |
|---|---|---|
| 1 | SNMP 카드에서 Community를 새 값(예: `ntek-test1`)으로 → Save → Reboot | Paessler에서 새 값으로 조회하면 응답, `public`으로는 응답 없음(timeout). Trap의 community도 새 값 |
| 2 | sysContact / sysLocation 입력 → Save → Reboot | TC-02의 `1.3.6.1.2.1.1.4.0` / `.6.0`이 입력값 |
| 3 | SNMP OFF → Save → Reboot | 모든 SNMP 요청 응답 없음, Trap 없음 |
| 4 | 공장초기화(Init Settings) | SNMP OFF, 재부팅 후 다시 ON하면 Community `public`·Contact/Location 빈 값(설정 파일 삭제 확인) |
| 5 | SNMP OFF 상태에서 SNMP 카드 확인 | Version·Trap·Trap Server·Community·sysContact·sysLocation 칸이 잠김 |

### 7.5 공존·부하 시험

SNMP 폴링이 계측·Modbus·웹 기능에 영향을 주지 않는지 확인한다. Walk는 MIB 끝까지 조회하므로 반복 시험에는 **Custom OID + Repeat every**를 쓴다.

1. Modbus TCP 폴링(기존 시험 도구)과 웹 대시보드를 켜 둔다.
2. Paessler 창을 2개 실행해 1시간 동안 반복 조회한다.
   - 창 1: Custom OID `1.3.6.1.2.1.1.3.0`(업타임), Repeat every `1` seconds
   - 창 2: Custom OID `1.3.6.1.2.1.17.2.4.0`(토폴로지 변경 횟수), Repeat every `1` seconds
3. 시험 중 10분마다 세 번째 창에서 Walk = `1.3.6.1.2.1.17.4.3`(포워딩 테이블)을 끝까지 한 번씩 실행한다. 포워딩 테이블은 한 행을 읽을 때마다 스위치 칩의 MAC 테이블 전체를 다시 읽기 때문에, 이 부하를 따로 확인한다.
4. 확인 항목
   - 창 1·2에서 1시간 동안 timeout 없이 매번 값이 오는지
   - 업타임이 중간에 작아지지 않았는지(재부팅/WDT 리셋 없음)
   - Modbus 응답 오류·지연이 늘지 않았는지
   - 웹 대시보드 계측값이 계속 갱신되는지
   - 콘솔 `CPU` 명령의 부하율이 평소(약 25%)와 비교해 크게 늘지 않았는지
- 판정: 리셋·통신 오류가 없고 계측 갱신이 유지되면 합격.

> 참고(사내): 현재 `net_config.h`의 `SNMP_TRACE_LEVEL`이 INFO라서 SNMP 요청마다 콘솔에 디버그 로그가 찍힌다. 부하 시험 중 콘솔 출력이 많아지는 것은 정상이며, 출하 전에 로그 레벨을 낮출지 검토한다.

### 7.6 RSTP 링 시험

SNMP 변경 후에도 RSTP 링 이중화가 정상 동작하는지 확인한다. 외부 LAN의 토폴로지 변경 영향을 받지 않도록 **분리된 시험망**에서 한다.

```
            ┌────────── [RSTP 지원 관리형 스위치] ──── [시험 PC]
            │ 포트A                 포트B │
        SV300 ETH1               SV300 ETH2
```

1. **준비**: 스위치에서 RSTP를 켜고, SV300의 두 포트를 스위치의 서로 다른 포트에 연결해 루프를 만든다. RSTP가 동작하지 않으면 이 구성에서 브로드캐스트 스톰이 발생하므로, 통신이 정상이라는 것 자체가 1차 확인이다.
2. **초기 상태**: Paessler Custom OID로 다음 값을 기록한다.
   - 포트1 상태 `1.3.6.1.2.1.17.2.15.1.3.1`, 포트2 상태 `1.3.6.1.2.1.17.2.15.1.3.2`
   - 토폴로지 변경 횟수 `1.3.6.1.2.1.17.2.4.0`
   - 올바른 응답: 루프를 끊기 위해 SV300 또는 스위치의 포트 하나가 blocking(`2`) 상태다. SV300 쪽 두 포트가 모두 `5`라면 스위치 쪽 포트 하나가 blocking이다.
3. **절체**: 명령 프롬프트에서 `ping -t <IP>`를 켜 둔 상태로 forwarding(`5`) 중인 쪽 케이블을 뽑는다.
4. **확인**: ping 손실 개수(≈ 절체 시간, 초)를 기록하고, Paessler로 다시 조회한다.
   - 올바른 응답: 뽑은 포트는 `6`(broken), 남은 포트는 `5`(forwarding), 토폴로지 변경 횟수는 증가
5. **복구**: 케이블을 다시 꽂고 ping 손실과 포트 상태를 같은 방법으로 기록한다.
6. 3~5를 반대쪽 포트에 대해서도 반복한다.
- 판정: 통신이 자동으로 복구되고(손실은 수 초 이내) 브로드캐스트 스톰이 발생하지 않으면 합격. 절체·복구 시간은 측정값을 기록한다.

### 7.7 문제 발생 시 확인 사항

| 증상 | 확인 |
|---|---|
| 모든 요청이 timeout | 장비 SNMP가 ON이고 ON 후 재부팅했는지(기본 OFF), SV300 IP 주소, 같은 서브넷 여부, Local IP 선택, PC/중간 방화벽의 UDP 161 차단, Community 철자(대소문자 구분·장비 설정값), SNMP Version `v1`/`v2c` 여부 |
| Trap이 Wireshark에 안 보임 | SNMP ON+재부팅, Trap ON, Trap Server = 시험 PC IP, 알람 조건(전압 인가·47~63Hz, 알람 설정 후 재부팅), 장비 콘솔의 Trap 송신 로그 |
| 알람 설정을 바꿨는데 반영되지 않음 | Alarm Settings의 Channel·Cond·Level은 재부팅 후 적용(현재 펌웨어) |
| `Please enter a valid OID` 오류 | 요청 종류 오른쪽 입력칸에 OID를 넣지 않음 |
| TC-02에서 데모 값(`Description` 등)이 보임 | 펌웨어 버전이 V00.04 이상인지 확인 |
| `private`로 응답이 옴 | V00.03 이하 펌웨어. V00.04 이상으로 업데이트 |
| MAC 주소의 `00`이 `20`으로 보이거나 `???`로 깨짐 | Paessler 표시 문제(§7.3.2). 장비는 정상 |
| Walk가 지정한 OID를 넘어 계속 진행됨 | Paessler 동작(§7.3.2). 필요한 줄이 나오면 Stop |
| 전체 조회가 1분 이상 걸림 | 정상. 포워딩 테이블이 LAN의 MAC 수만큼 커지고 행마다 스위치 칩을 다시 읽는다 |
| RSTP-MIB 객체가 숫자로만 보임 | NMS에 RSTP-MIB(RFC 4318) 파일이 없는 경우. 동작에는 문제없음 |
| sysName이 `sv300-000000000` | 장비 시리얼 번호가 설정되지 않음(콘솔 `SNSET`으로 설정) |

### 7.8 시험 결과 기록표

| 시험 일자 | 펌웨어 | 장비 SN | 시험자 |
|---|---|---|---|
| | SV300 FW V00.04 (26/10/08 빌드) | | |

| 항목 | 결과(합격/불합격) | 측정값·비고 |
|---|---|---|
| TC-01 기본 응답 | | |
| TC-02 system 그룹 | | |
| TC-03 버전별 동작 | | |
| TC-04 잘못된 커뮤니티 거부 | | 카운터 (A)→(B) |
| TC-05 쓰기(SET) 거부 | | Paessler 대체 / 정식 시험 여부 |
| TC-06 BRIDGE/RSTP-MIB 조회 | | 포트 상태 |
| TC-07 전체 조회 | | 소요 시간 __초 |
| TC-08 호스트명 일치 | | |
| TC-09 재부팅 후 복귀 | | 복귀 시간 __초 |
| TC-10 Trap (TR-01~05) | | 수신 Trap 수·varbind 일치 |
| TC-11 SNMP 설정 반영 | | |
| 7.5 공존·부하 1시간 | | CPU __% |
| 7.6 RSTP 링 절체/복구 | | 절체 __초 / 복구 __초 |


---

## 8. 추후 작업 (TODO)

> 운영 규칙: SNMP 관련 남은 작업을 여기서 관리한다. **완료한 항목은 이 목록에서 삭제**하고, 결과는 해당 본문 절(§4~§7)과 FWUPDATE에 남긴다.
> 기준: 2026-10-08, SV300 커밋 `7a761dd` / confWebApp `6513bab`

### 8.1 SNMPv3 (§4.5-③) — 다음 개발 단계
- [ ] `net_config.h`에 `SNMP_V3_SUPPORT ENABLED`, 빌드 크기·RAM 확인
- [ ] USM 사용자 1개(RO): 사용자 이름 + 인증(HMAC-SHA-1) + 암호화(AES-128). 비밀번호는 저장 시 키(20B)로 변환해 `snmp.d`의 예약 필드(`v3User`·`v3AuthKey`·`v3PrivKey`)에 저장, 원문 미보관. 비밀번호 8자 이상 검사
- [ ] engineID = RFC 3411 형식(`0x80000000|PEN` + MAC). PEN 발급 전 임시값
- [ ] snmpEngineBoots 영구 저장(부팅마다 +1) — 미저장 시 NMS 시간창(notInTimeWindow) 문제
- [ ] SNMP Version 선택 활성화: 장비 웹·confWebApp의 `v1/v2c/v3`·`v3 only` 항목 `(next FW)`·disabled 해제 (`web.c` OPTDIS, `channel_general.html`)
- [ ] `v3 only`일 때 Community 칸 잠금 / v1·v2c일 때 v3 칸 잠금(웹 SNMP 카드)
- [ ] v3 Trap: Version 설정에 따라 Trap도 v3(RO 사용자)로 송신
- [ ] (선택) SHA-256 인증 — `sha256.c` 추가 + `SNMP_SHA256_SUPPORT`
- [ ] §7에 v3 시험 항목 추가(TC-03 v3 기대값 변경 포함) → 자체 시험 완료 후 **FW_VER 상향**

### 8.2 IANA PEN (§6)
- [ ] 발급 확인(신청서 작성 2026-10-08, 확인 메일 답장 여부 확인)
- [ ] 발급 후 교체: `snmp_trap.c` `SV_PEN_OID`(BER 바이트), sysObjectID(`1.3.6.1.4.1.<PEN>.1`), engineID, `SV300-MIB.mib`의 `enterprises` — **출하 전 필수**(현재 임시 32473)
- [ ] snmp.md §5.4·§7의 `32473` 표기 갱신

### 8.3 자사 MIB — 계측값 (§4.5-④, §5)
- [ ] CycloneTCP MIB 모듈 확장(현재 `snmp_trap.c`의 SV300-MIB는 Trap 알림 객체만): `svCommon`, `svFeederTable`(미사용 피더 행 숨김), `svHarmTable`(1차 포함), `svAlarmTable`(Trap 유실 시 폴링 복구용)
- [ ] 데이터 소스 `psmpMap` 스냅샷, 스케일 §5.2(U·Upp·I ×100, In ×1000 등), 전력량·고조파는 Modbus 원값
- [ ] `SV300-MIB.mib`(SMIv2) 작성 + `smilint` 검사, Trap(NOTIFICATION-TYPE) 정의 포함
- [ ] §7에 MIB 조회 시험 추가

### 8.4 배포물 (§4.5-⑤)
- [ ] MIB 파일 배포 경로(홈페이지·장비 웹 다운로드 등) 결정
- [ ] 매뉴얼용 OID·스케일 표
- [ ] (선택) Zabbix/PRTG 템플릿
- [ ] 외부 연동 가이드(사용자 요청 시) — snmp.md의 사내 내용(구버전 취약점·소스 경로·라이선스·미확인 경쟁사 정보) 제외, §1·§5·§7 기반

### 8.5 출하 전 결정·정리
- [ ] `net_config.h` `SNMP_TRACE_LEVEL`(현재 INFO, 요청마다 콘솔 로그) 낮출지 결정
- [ ] BRIDGE-MIB 포워딩 테이블(`dot1dTpFdbTable`) — 조회 시 행마다 스위치 칩 전체 재조회(O(n²)). 부하 시험(§7.5) 결과 보고 노출 유지/숨김 결정
- [ ] 웹 SNMP 카드 `Community` 표기에 설명 추가 여부(예: `Community (password)`)
- [ ] CycloneTCP 상용 라이선스 범위 확인(`GPL_LICENSE_TERMS_ACCEPTED`로 빌드 중)

### 8.6 시험 (26/10/08 빌드)
- [ ] TC-11 SNMP 설정 반영(Community 변경·Contact/Location·SNMP OFF·공장초기화·화면 잠금) 실기 확인
- [ ] §7.5 공존·부하, §7.6 RSTP 링 — SNMP·Trap 추가 빌드로 재확인

### 8.7 관련 항목 (SNMP 외, Trap 시험에 영향)
- [ ] 알람 설정(Channel·Cond·Level)이 재부팅 후에만 적용되는 문제 — Save Set 시 판정값(`alarm.st[]`) 갱신 여부 결정
- [ ] 메모리맵 엑셀 `memorymap/SV300 memory map_261008.xlsx` 커밋(SNMP 주소 7153·7155~7160 반영본)
