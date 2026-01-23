#ifndef _ALARM_H

#define	_ALARM_H

typedef struct {
	char *nm;
	float *src;
	float	norm;
//	uint16_t	*sts;
} COMP_TBL;

extern COMP_TBL almTbl[];

void initAlarmTable(void);
void buildAlarmSettings(void);
int alarmProc(void);
int loadAlarmLog(void);
int storeAlarmStatus(void);
int deleteAlarmLog(void);
void buildTrendSetting();
int loadAlarmStatus(void);
int loadEventLog(void);

#endif


