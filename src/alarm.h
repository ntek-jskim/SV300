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
int storeAlarmDef(void);
int loadAlarmDef(void);
int deleteAlarmLog(int id);
void buildTrendSetting();
int loadEventLog(void);
int alarmFsDispatch(const FS_MSG *pmsg);
/* [태스크 통합] Trend_Task → FS_task 흡수. FS_task 시작부 checkTrendHeader() 1회 +
 *  루프마다 trendTick()(1분 1회 실제 동작, 그 외 즉시 return) 호출용 */
void checkTrendHeader(void);
void trendTick(void);

#endif


