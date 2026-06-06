#ifndef _ALARM_H

#define	_ALARM_H

#include "meter.h"

typedef struct {
	char *nm;
	float *src;
	float	norm;
//	uint16_t	*sts;
} COMP_TBL;

extern COMP_TBL almTbl[METER_CH_COUNT][MAX_ALARM_CH];

void initAlarmTable(int id);
void buildAlarmSettings(int id);
int alarmProc(int id);
int loadAlarmLog(int id);
int storeAlarmStatus(int id);
int loadAlarmStatus(int id);
int deleteAlarmLog(int id);
void buildTrendSetting();
int loadEventLog(void);

#endif


