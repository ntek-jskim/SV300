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

26/08/12 FW V00.02
  1) Calibration 지원: confWebApp Calibration 화면(ADMIN) 신설 — V/A/Power/Phase/IN 교정(명령 7450 code) + Temp 교정 신설(METER_CAL tempOfs·TEMP_CAL_REF 25℃·calibration case17=Temp Cali/18=Temp Init·readTemp 반영) + 피더 실측 검증표 + 엑셀 리포트(튜닝일자/SN/장비/담당자/버전/결과). 타깃 Feeder 1~9(각 칩 3상, 교정은 칩 단위)
  2) 260812 맵: 피더 Simple에 Angle U1~3(V Phase)·Angle I1~3(Cur Phase) 신설 → SMP_CHANNEL 90→100w(harmonics 50830→50920), smpFill3ph/1ph·api_feeders 정합. 벡터도 전류 LN 색상 청색(#1E64FF, I2 검정과 구분)
  3) HWMODEL 복원(common 7086, 0=3CH/1=2CH): METER_CAL(FRAM) 저장 + 콘솔 'MODEL/HWMODEL <n>' 설정 + getHwModel/getHwCh + setMeterInfo 노출, confWebApp이 읽어 3CH면 Temp 숨김·칩범위 적응. METER_CAL 4바이트 정렬(_rsv, 숨은패딩 방지). ※METER_CAL 구조 변경(hwModel/tempOfs)으로 업데이트 후 교정값 재초기화 필요(hwModel은 빌드 기본값 2CH=1/3CH=0)

26/08/13 FW V00.02
  1) inorm(정격전류) CT1 대비 %화(In=CT1×inorm/100·기본50%) + 설정 0→default 정규화(buildSettings: I_start1/inorm50/CT1 100/vnorm·PT 220) + freq enum통일(0:60/1:50, FREQ_HZ 매크로·구버전 Hz저장 자동 마이그레이션) + smb_rtu Temp-ref(7463) 명령 신설·260812 명령 +1 시프트. inorm 사용처(alarm 전류·전력기준/ade9000 TDD분모·PQ OC/meter I_start·파형)를 In(A) 환산. ※CT1≠100 유닛은 업데이트 후 inorm 재설정 필요
  2) Web(web.c): read error 시 계측값 "----" 표시(Dashboard/Channel/Feeder 값만 null화·구조 유지), CT 'I Nominal(%)'·Start I 소수점3자리(표시 raw/1000·저장 ×1000·step0.001), mDNS 호스트명 MAC→SN 4바이트. confWebApp: 설정 백업/복원(Main+Channel 전체 JSON 레지스터블록 Save/Load·블록R/W 고속·Save Settings 7449 영속·admin), Calibration 페이지 전체폭·그룹 정리. 실기 백업/복원 라운드트립 검증 PASS

26/08/14 FW V00.02
  1) 파형 seam 근본원인 규명·복구 — SPI로 ADE9000 WFB 읽는 행위가 칩 동시변환을 교란(전송방식·SPI클럭 무관, 버스트당 ~1샘플), readWFB에서 6채널 median 검출→선형보간 복구(FFT·고조파 반영)
  2) Modbus METERING 스냅샷 더블버퍼(flash 로깅 중 torn read=garbage float 방지) + EN50160 리포트 CH2/3 전압공유 복사 + Modbus 쓰기 미준비/무효주소 예외

26/08/18 FW V00.02
  1) 파형 표시 분산 — copyModbusWaveData에 waveDispIndex(GEMS식: ZX 기준 ix부터 WVDISP_SPAN 구간을 표시 160점에 균등분산)+re=fr-100 윈도우 위상고정 도입: ÷4~10 희석 시 M0 파형 클린. 진단용 WV_STAGED(core만 기동·태스크 단계 ENABLE) 유지 (미완 체크포인트)
  2) 윈도우 despike(A안) 폐기 — ÷2 확대 검증서 소스 잔여가 고립 글리치가 아닌 광범위(윈도우당 20~69샘플)로 규명, 연속윈도우 median 복구는 실신호 왜곡으로 오히려 악화 → WV_WINDESPIKE OFF(코드 보존). 소스 거칠음 원인(고조파/노이즈 vs readWFB 커플링) 추가 규명 예정

26/08/21 FW V00.02
  1) 파형/THD 근본해결(Quiet Capture: 워커·flash 조용한 창에서만 캡처) + M2 3채널 계측 활성 + THD ~2.6초 갱신
  2) SPI 20MHz 상향 + 칩 1개 실패해도 web/Modbus 통신(10초 폴백, 부분고장 진단 가능)

26/08/24 FW V00.02
  1) FW 브라우저 업로드(웹 Setup→Command: .bin 선택→검증→Apply→부트로더 굽기, admin) + 명령맵 off-by-one 일괄수정(clear/ack/Init/Clear PI 등 — Clear PI가 factory reset 오동작하던 것 포함)
  2) 대시보드 PF·CT 모듈카드·메터카드 동일높이·에너지 5분저장·THD 조건부계산(동작전압/전류)·Start I 표시·nTurns 전류비율·ct_dir 위상180°·datetime 영문


NOSDMEM : SD MEMORY Y/N
HWV1 : CH3(METER CHIP SELECT)
HWV2 : SPI FLASH 16MB
DTEST : DEBUG TEST용

// debug
SV300\keil\SV300.uvproj\SV300              : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS METER_TEST_DATA
SV300\keil\SV300.uvproj\SV300_3CH          : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS METER_TEST_DATA NOSDMEM HWV1 CH3 HWV2
// release
SV300\keil\SV300_app.uvproj\SV300_app      : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS _WDT_EN
SV300\keil\SV300_app.uvproj\SV300_3CH_app  : LPC4330 USE_LPCXPRESSO_4337 CMSIS_BITPOSITIONS GPL_LICENSE_TERMS_ACCEPTED CORE_M4 USE_KSZ8863 GEMS9000 EXT_RTC _FTP_SERVER VFRMS NOSDMEM HWV1 CH3 _WDT_EN HWV2
