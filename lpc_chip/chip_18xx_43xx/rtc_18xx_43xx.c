/*
 * @brief LPC18xx/43xx RTC driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "chip.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
 // HITEX port: these should be defined elsewhere but aren't!
#define CREG_CREG0_ALARMCTRL_Pos      (6)                                                       /*!< CREG CREG0: ALARMCTRL Position      */
#define CREG_CREG0_ALARMCTRL_Msk      (0x03UL << CREG_CREG0_ALARMCTRL_Pos)                      /*!< CREG CREG0: ALARMCTRL Mask          */
#define CREG_CREG0_SAMPLECTRL_Pos     (12)                                                      /*!< CREG CREG0: ALARMCTRL Position      */
#define CREG_CREG0_SAMPLECTRL_Msk     (0x03UL << CREG_CREG0_ALARMCTRL_Pos)                      /*!< CREG CREG0: ALARMCTRL Mask          */

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Initialize the RTC peripheral */
int Chip_RTC_Init(LPC_RTC_T *pRTC)
{
#if _RTC_OLD_CODE
	Chip_Clock_RTCEnable();

	/* Disable RTC */
	Chip_RTC_Enable(pRTC, DISABLE);

	/* Disable Calibration */
	Chip_RTC_CalibCounterCmd(pRTC, DISABLE);

	/* Reset RTC Clock */
	Chip_RTC_ResetClockTickCounter(pRTC);

	/* Clear counter increment and alarm interrupt */
	pRTC->ILR = RTC_IRL_RTCCIF | RTC_IRL_RTCALF;
	while (pRTC->ILR != 0) {}

	/* Clear all register to be default */
	pRTC->CIIR = 0x00;
	pRTC->AMR = 0xFF;
	pRTC->CALIBRATION = 0x00;
#else
	uint32_t atimer;

	atimer = LPC_ATIMER->DOWNCOUNTER;
	Chip_Clock_RTCEnable();

	// HITEX port: According to the LPC datasheet, we must now wait for 2 seconds after
	// enabling the 32kHz clock before writing to registers.  This is accomplished using
	// the RITimer as no RTOS timer is available at this point.
	// NOTE: this means the RITimer must have been initialised by this point!
	Chip_RIT_SetTimerInterval(LPC_RITIMER, 2000);   // Interval is in ms
	while (!Chip_RIT_GetIntStatus(LPC_RITIMER))
	{
		;
	}
	
	// OSC32가 동작하지 않으므로 RTC 초기화를 중단한다 
	if (atimer == LPC_ATIMER->DOWNCOUNTER) {
		return -1;
	}

	// HITEX RG: disable interrputs to stop FreeRTOS or anything else disturbing
	// critical time while RTC peripheral is initialised
	__disable_irq();
    
	/* Disable RTC */
//	Chip_RTC_Enable(LPC_RTC, DISABLE);

	/* Disable Calibration */
//	Chip_RTC_CalibCounterCmd(LPC_RTC, DISABLE);

	/* Reset RTC Clock */
//	Chip_RTC_ResetClockTickCounter(LPC_RTC);

	/* Clear counter increment and alarm interrupt */
	LPC_RTC->ILR = RTC_IRL_RTCCIF | RTC_IRL_RTCALF;
	while (LPC_RTC->ILR != 0) {}

	/* Clear all register to be default */
	LPC_RTC->CIIR = 0x00;
	LPC_RTC->AMR = 0xFF;
	LPC_RTC->CALIBRATION = 0x00;

	/* VBAT current consumption due to RTC_ALARM and SAMPLE pins can be 
		 lowered significantly by configuring the RTC_ALARM pin and SAMPLE 
		 pins as "Inactive" by setting the ALARMCTRL 7:6 field in CREG0 to 
		 0x3 and the SAMPLECTRL 13:12 field in CREG0 to 0x3. These bits persist
		 through power cycles and reset, as long as VBAT is present. */
	LPC_CREG->CREG0 |= (CREG_CREG0_ALARMCTRL_Msk);
	LPC_CREG->CREG0 |= (CREG_CREG0_SAMPLECTRL_Msk);

	__enable_irq();     // Re-enable interrupts if they were enabled before
	return 0;
#endif		
}

/*De-initialize the RTC peripheral */
void Chip_RTC_DeInit(LPC_RTC_T *pRTC)
{
	pRTC->CCR = 0x00;
}

/* Reset clock tick counter in the RTC peripheral */
void Chip_RTC_ResetClockTickCounter(LPC_RTC_T *pRTC)
{
#ifdef _RTC_OLD_CODE
	do {
		/* Reset RTC clock*/
		pRTC->CCR |= RTC_CCR_CTCRST;
	} while ((pRTC->CCR & RTC_CCR_CTCRST) != RTC_CCR_CTCRST);

	do {
		/* Finish resetting RTC clock */
		pRTC->CCR &= (~RTC_CCR_CTCRST) & RTC_CCR_BITMASK;
	} while (pRTC->CCR & RTC_CCR_CTCRST);
#else
  pRTC->CCR |= RTC_CCR_CTCRST;     // HITEX port RG: bug fix, shouldn't repeatedly write!
	do {
		/* Reset RTC clock*/
	} while ((pRTC->CCR & RTC_CCR_CTCRST) != RTC_CCR_CTCRST);

    pRTC->CCR &= (~RTC_CCR_CTCRST) & RTC_CCR_BITMASK;     // HITEX port RG: bug fix, shouldn't repeatedly write!
	do {
		/* Finish resetting RTC clock */
	} while (pRTC->CCR & RTC_CCR_CTCRST);	
#endif	
}

/* Start/Stop RTC peripheral */
void Chip_RTC_Enable(LPC_RTC_T *pRTC, FunctionalState NewState)
{
#ifdef _RTC_OLD_CODE	
	if (NewState == ENABLE) {
		do {
			pRTC->CCR |= RTC_CCR_CLKEN;
		} while ((pRTC->CCR & RTC_CCR_CLKEN) == 0);
	}
	else {
		pRTC->CCR &= (~RTC_CCR_CLKEN) & RTC_CCR_BITMASK;
	}
#else
	if (NewState == ENABLE) {
		pRTC->CCR |= RTC_CCR_CLKEN;     // HITEX port RG: bug fix, shouldn't repeatedly write!
		do {
		} while ((pRTC->CCR & RTC_CCR_CLKEN) == 0);
	}
	else {
		pRTC->CCR &= (~RTC_CCR_CLKEN) & RTC_CCR_BITMASK;
	}	
#endif
}

/* Enable/Disable Counter increment interrupt for a time type in the RTC peripheral */
void Chip_RTC_CntIncrIntConfig(LPC_RTC_T *pRTC, uint32_t cntrMask, FunctionalState NewState)
{
	if (NewState == ENABLE) {
		pRTC->CIIR |= cntrMask;
	}

	else {
		pRTC->CIIR &= (~cntrMask) & RTC_AMR_CIIR_BITMASK;
		while (pRTC->CIIR & cntrMask) {}
	}
}

/* Enable/Disable Alarm interrupt for a time type in the RTC peripheral */
void Chip_RTC_AlarmIntConfig(LPC_RTC_T *pRTC, uint32_t alarmMask, FunctionalState NewState)
{
	if (NewState == ENABLE) {
		pRTC->AMR &= (~alarmMask) & RTC_AMR_CIIR_BITMASK;
	}
	else {
		pRTC->AMR |= (alarmMask);
		while ((pRTC->AMR & alarmMask) == 0) {}
	}
}

/* Set full time in the RTC peripheral */
void Chip_RTC_SetFullTime(LPC_RTC_T *pRTC, RTC_TIME_T *pFullTime)
{
	RTC_TIMEINDEX_T i;
	uint32_t ccr_val = pRTC->CCR;

	/* Temporarily disable */
	if (ccr_val & RTC_CCR_CLKEN) {
		pRTC->CCR = ccr_val & (~RTC_CCR_CLKEN) & RTC_CCR_BITMASK;
	}

	/* Date time setting */
	for (i = RTC_TIMETYPE_SECOND; i < RTC_TIMETYPE_LAST; i++) {
		pRTC->TIME[i] = pFullTime->time[i];
	}

	/* Restore to old setting */
	pRTC->CCR = ccr_val;
}

/* Get full time from the RTC peripheral */
void Chip_RTC_GetFullTime(LPC_RTC_T *pRTC, RTC_TIME_T *pFullTime)
{
	RTC_TIMEINDEX_T i;
	uint32_t secs = 0xFF;

	/* Read full time, but verify second tick didn't change during the read. If
	   it did, re-read the time again so it will be consistent across all fields. */
	while (secs != pRTC->TIME[RTC_TIMETYPE_SECOND]) {
		secs = pFullTime->time[RTC_TIMETYPE_SECOND] = pRTC->TIME[RTC_TIMETYPE_SECOND];
		for (i = RTC_TIMETYPE_MINUTE; i < RTC_TIMETYPE_LAST; i++) {
			pFullTime->time[i] = pRTC->TIME[i];
		}
	}
}

/* Set full alarm time in the RTC peripheral */
void Chip_RTC_SetFullAlarmTime(LPC_RTC_T *pRTC, RTC_TIME_T *pFullTime)
{
	RTC_TIMEINDEX_T i;

	for (i = RTC_TIMETYPE_SECOND; i < RTC_TIMETYPE_LAST; i++) {
		pRTC->ALRM[i] = pFullTime->time[i];
	}
}

/* Get full alarm time in the RTC peripheral */
void Chip_RTC_GetFullAlarmTime(LPC_RTC_T *pRTC, RTC_TIME_T *pFullTime)
{
	RTC_TIMEINDEX_T i;

	for (i = RTC_TIMETYPE_SECOND; i < RTC_TIMETYPE_LAST; i++) {
		pFullTime->time[i] = pRTC->ALRM[i];
	}
}

/* Enable/Disable calibration counter in the RTC peripheral */
void Chip_RTC_CalibCounterCmd(LPC_RTC_T *pRTC, FunctionalState NewState)
{
#ifdef _RTC_OLD_CODE
	if (NewState == ENABLE) {
		do {
			pRTC->CCR &= (~RTC_CCR_CCALEN) & RTC_CCR_BITMASK;
		} while (pRTC->CCR & RTC_CCR_CCALEN);
	}
	else {
		pRTC->CCR |= RTC_CCR_CCALEN;
	}
#else
	if (NewState == ENABLE) {
		pRTC->CCR &= (~RTC_CCR_CCALEN) & RTC_CCR_BITMASK;   // HITEX port RG: bug fix, shouldn't repeatedly write!
		do {
		} while (pRTC->CCR & RTC_CCR_CCALEN);
	}
	else {
		pRTC->CCR |= RTC_CCR_CCALEN;
	}	
#endif	
}

#if RTC_EV_SUPPORT
/* Get first timestamp value */
void Chip_RTC_EV_GetFirstTimeStamp(LPC_RTC_T *pRTC, RTC_EV_CHANNEL_T ch, RTC_EV_TIMESTAMP_T *pTimeStamp)
{
	pTimeStamp->sec = RTC_ER_TIMESTAMP_SEC(pRTC->ERFIRSTSTAMP[ch]);
	pTimeStamp->min = RTC_ER_TIMESTAMP_MIN(pRTC->ERFIRSTSTAMP[ch]);
	pTimeStamp->hour = RTC_ER_TIMESTAMP_HOUR(pRTC->ERFIRSTSTAMP[ch]);
	pTimeStamp->dayofyear = RTC_ER_TIMESTAMP_DOY(pRTC->ERFIRSTSTAMP[ch]);
}

/* Get last timestamp value */
void Chip_RTC_EV_GetLastTimeStamp(LPC_RTC_T *pRTC, RTC_EV_CHANNEL_T ch, RTC_EV_TIMESTAMP_T *pTimeStamp)
{
	pTimeStamp->sec = RTC_ER_TIMESTAMP_SEC(pRTC->ERLASTSTAMP[ch]);
	pTimeStamp->min = RTC_ER_TIMESTAMP_MIN(pRTC->ERLASTSTAMP[ch]);
	pTimeStamp->hour = RTC_ER_TIMESTAMP_HOUR(pRTC->ERLASTSTAMP[ch]);
	pTimeStamp->dayofyear = RTC_ER_TIMESTAMP_DOY(pRTC->ERLASTSTAMP[ch]);
}

#endif /*RTC_EV_SUPPORT*/

