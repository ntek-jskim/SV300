중요 수정사항

26/02/19 FW V00.00
  1) start

26/08/05 FW V00.02
  1) Web: Feeder·Channel 화면 신규 — Channel 8탭(Meter/Phase/Min·Max/Harmonics/Energy/Demand/Alarm/Event) confWebApp 동일화
  2) Channel 홀딩 근본수정 — HTTP_SERVER_PERSISTENT_CONN_SUPPORT 활성화(keep-alive)로 커넥션 재사용

26/08/06 FW V00.02
  1) Web: Channel Waveform / Report(EN50160·ITIC) / Monthly 탭 추가 → confWebApp Channel 11탭 완성
  2) Web: 고조파 Table/Chart·에너지 매트릭스·수요 프로파일 차트 등 탭별 세부 마감

26/08/07 FW V00.02
  1) Web: Main Setting 확장 — Device Info(S/N·MAC·FW Build Date)·Command Load 그룹 추가, Channel Setting 7섹션(PQ Event/Transient/Waveform/Disturbance/Trend/PQ Report/Alarm Def) 신규(그룹핑)
  2) Web: 전압공유(M0~M2) 반영 — PQ Event Sag/Swell/Intr·Transient Voltage는 CH2/3 숨김, Harmonics는 CH1 Chart+Table 동시(전류만 CH1~3)
  3) FW/Web: 명령맵(260807) 정렬(ALL+#1~3) + Alarm/Event/ITIC의 Load(페이지)·Clear를 Command→측정화면으로 이동, ITIC는 웹=itic2·외부=itic 분리
  4) FW/Web: Min/Max·Energy·Demand Clear 버튼, 대시보드 IOM DATA(7050~7069) 표시, Energy 영역 R/W(FW 쓰기경로) — Energy 웹 전체편집은 flash 초과로 보류

26/08/10 FW V00.02
  1) Web/FW: Energy 웹 전체편집(U32 R/W) 완료 + 대시보드 IOM DATA 개편(DI+Temp 가로배치·설정 통합편집·Clear PI/Ack) + PI 기능 구현(DI Type별 펄스카운트·디바운스 4~64ms/기본16·PI Scale 적용), 로그인 viewer→sv300
  2) 빌드 최적화: 필드앱 -O1(-O2 홀딩 회피·코드 68KB↓), 부트로더 -O2(72→67KB, SV300_boot 별도), 디버그 -O0 유지; flash 확보 위해 웹 UI 영문화·jp/sn/pbar 헬퍼 통합
  3) FW/Web: 메모리맵 260810(Command -8 시프트) + METER_CAL 4D화(vgain/vppgain[chip][phase][결선Z][PT2]·R_igain/R_phcal 로고스키·hwModel 삭제·hwVer/fwVer U16, GEMS3500식) + METER_INFO 상태필드(HEART BIT~TOT STS)·mBusRxCnt·checkIO(3s). web General HWMODEL 제거+Status 표시, IOM DATA를 별도 IO(DI/TEMP) 화면으로 분리
  4) ★홀딩 근본수정: 웹 화면 빠른전환 시 Meter1_Task 스택오버플로우(os_error STACK_OVF) — meter_scan_2 PQM경로+웹 이더넷 인터럽트 프레임이 2048B 초과 → Meter1/Meter2 스택 512→1024워드·Meter0 256→384 상향 (실기 재빌드·플래시·검증 필요)

26/08/11 FW V00.02
  1) ★홀딩 근본수정 재확정(OS_STKSIZE): osCreateTask가 stack=NULL(정적스택 미제공)이면 per-task stackSize를 무시하고 RTX 기본 OS_STKSIZE를 전 태스크에 적용(OS_PRIVCNT=0) → httpConnectionTask(/api/channels 부동소수 snprintf)가 375워드(1500B) 초과·인접 Meter0 침범→os_error(STACK_OVF). OS_STKSIZE 375→640(2560B)로 해소(web+confWebApp 2서버 1시간+ 안정). 초기화 게이팅: Buffer Ready(M0~M2) 전 웹 listen·Modbus 통신 차단(g_meterReady)
  2) Energy Edit 영속화: 웹 Energy 편집이 표시미러(Ereg32)에만 기록돼 리부팅 원복되던 문제 — applyEregEdit(미러→egyNvr 역복사) 신설, FLASH 저장은 Main Setting/Command/Save Set(7449)에서만(Write 후 "Save to FLASH?" 확인창→OK 시 저장, clear energy 명령값 0x5678=적용/0x1234=클리어)
  3) web.c Channel: WIRING 배지(3P4W식) + 페이저 IEC색(전압 갈/흑/회 실선·전류 갈/흑/회/흑 점선·LN 벡터합), IOM DATA를 IO(DI/TEMP) 별도화면 분리. iom.c DI active-low 보정(무신호=1)·3CH TEMP 제외. SD_File.c 제거(FS.c 중복)

NOSDMEM : SD MEMORY Y/N
HWV1 : CH3(METER CHIP SELECT)
HWV2 : SPI FLASH 16MB

// debug
SV300\keil\SV300.uvproj\SV300              : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS METER_TEST_DATA
SV300\keil\SV300.uvproj\SV300_3CH          : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS METER_TEST_DATA NOSDMEM HWV1 CH3 HWV2
// release
SV300\keil\SV300_app.uvproj\SV300_app      : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS _WDT_EN
SV300\keil\SV300_app.uvproj\SV300_3CH_app  : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS NOSDMEM HWV1 CH3 _WDT_EN HWV2
 

 