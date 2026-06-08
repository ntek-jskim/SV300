#ifdef __FREERTOS
   #include "RTL.h"
   #include "os_port.h"
#endif
#include "board.h"
#include "meter.h"
#include "ade9000.h"
#include "math.h"
#include "time.h"
#include "string.h"
#include "meter.h"
#include "alarm.h"

extern int getYear_n_WoY(int doY, int doW);
extern void assertAlarmOutput(int point, int state);
	
//int getYear_n_WoY(int *pYear, int *woY);

COMP_TBL	almTbl[METER_CH_COUNT][MAX_ALARM_CH] __attribute__ ((section ("EXT_RAM"), zero_init));
//COMP_TBL almTbl[100];

TREND_INFO trdInf[METER_CH_COUNT][4];
TREND_RECORD	row[METER_CH_COUNT][4];	// trend data per meter CH
//extern MGEM3600_DATA	gems3600[];

static void setAlarmChannel(int id, int ix, char *pname, float norm, float *src) {
	almTbl[id][ix].nm = pname;
	almTbl[id][ix].norm = norm;
	almTbl[id][ix].src = src;
}

static void getAlarmFifoFileName(int id, char *path) {
	if (id == 0) {
		strcpy(path, ALARM_FIFO_FILE);
	} else {
		sprintf(path, "%s\\alog%s_fifo_m%d.d", ALARM_DIR, ALOG_VER, id);
	}
}

static const char *getAlarmStatusFileName(int id) {
	switch (id) {
	case 0: return ALARM_ST_FILE;
	case 1: return ALARM_ST_FILE1;
	case 2: return ALARM_ST_FILE2;
	default: return ALARM_ST_FILE;
	}
}

void initAlarmTable(int id) {
	int ix=0;
	float temp;
	METERING *pm = &meter[id].meter;
	DEMAND *pdmd = &meter[id].dm;

	if (id < 0 || id >= METER_CH_COUNT)
		return;

	memset(almTbl[id], 0, sizeof(almTbl[id]));
	// [0..2]
	setAlarmChannel(id, ix++, "-", 0, NULL);
	setAlarmChannel(id, ix++, "Temp.", 100, &pm->Temp);
	setAlarmChannel(id, ix++, "Freq.", pdb->freq, &pm->Freq);
	// [3..6]
	temp = db.pt[id].vnorm;
	setAlarmChannel(id, ix++, "U1",  temp, &pm->U[0]);
	setAlarmChannel(id, ix++, "U2",  temp, &pm->U[1]);
	setAlarmChannel(id, ix++, "U3",  temp, &pm->U[2]);
	setAlarmChannel(id, ix++, "U~",  temp, &pm->U[3]);
	// [7..10]
	temp = (db.pt[id].wiring == WM_3LN3CT) ? db.pt[id].vnorm*sqrt(3) : db.pt[id].vnorm; 
	setAlarmChannel(id, ix++, "U12", temp, &pm->Upp[0]);
	setAlarmChannel(id, ix++, "U23", temp, &pm->Upp[1]);
	setAlarmChannel(id, ix++, "U31", temp, &pm->Upp[2]);
	setAlarmChannel(id, ix++, "Upp~", temp, &pm->Upp[3]);
	// [11..12]
	temp = 100;
	setAlarmChannel(id, ix++, "Uu", temp, &pm->Ubal[0]);
	setAlarmChannel(id, ix++, "Uo", temp, &pm->Ubal[1]);
	// [13..18]
	temp = db.ct[id].inorm;
	setAlarmChannel(id, ix++, "I1", temp, &pm->I[0]);
	setAlarmChannel(id, ix++, "I2", temp, &pm->I[1]);
	setAlarmChannel(id, ix++, "I3", temp, &pm->I[2]);
	setAlarmChannel(id, ix++, "I~", temp, &pm->I[3]);
	setAlarmChannel(id, ix++, "Itotal", temp*3, &pm->I[4]);
	setAlarmChannel(id, ix++, "In", temp, &pm->In);
	// [19..22]
	temp = db.pt[id].vnorm*db.ct[id].inorm;
	setAlarmChannel(id, ix++, "P1", temp, &pm->P[0]);
	setAlarmChannel(id, ix++, "P2", temp, &pm->P[1]);
	setAlarmChannel(id, ix++, "P3", temp, &pm->P[2]);
	setAlarmChannel(id, ix++, "Ptotal", temp*3, &pm->P[3]);
	// [23..26]
	temp = db.pt[id].vnorm*db.ct[id].inorm;
	setAlarmChannel(id, ix++, "Q1", temp, &pm->Q[0]);
	setAlarmChannel(id, ix++, "Q2", temp, &pm->Q[1]);
	setAlarmChannel(id, ix++, "Q3", temp, &pm->Q[2]);
	setAlarmChannel(id, ix++, "Q4", temp*3, &pm->Q[3]);
	// [27..30]
	temp = db.pt[id].vnorm*db.ct[id].inorm;
	setAlarmChannel(id, ix++, "D1", temp*3, &pm->D[0]);
	setAlarmChannel(id, ix++, "D2", temp*3, &pm->D[1]);
	setAlarmChannel(id, ix++, "D3", temp*3, &pm->D[2]);
	setAlarmChannel(id, ix++, "Dtotal", temp, &pm->D[3]);
	// [31..34]
	temp = db.pt[id].vnorm*db.ct[id].inorm;
	setAlarmChannel(id, ix++, "S1", temp, &pm->S[0]);
	setAlarmChannel(id, ix++, "S2", temp, &pm->S[1]);
	setAlarmChannel(id, ix++, "S3", temp, &pm->S[2]);
	setAlarmChannel(id, ix++, "Stotal", temp*3, &pm->S[3]);
	// [35..38]
	temp = 100;
	setAlarmChannel(id, ix++, "PF1", temp, &pm->PF[0]);
	setAlarmChannel(id, ix++, "PF2", temp, &pm->PF[1]);
	setAlarmChannel(id, ix++, "PF3", temp, &pm->PF[2]);
	setAlarmChannel(id, ix++, "PFtotal", temp, &pm->PF[3]);
	// [39..41]
	temp = 100;
	setAlarmChannel(id, ix++, "THD U1", temp, &pm->THD_U[0]);
	setAlarmChannel(id, ix++, "THD U2", temp, &pm->THD_U[1]);
	setAlarmChannel(id, ix++, "THD U3", temp, &pm->THD_U[2]);
	// [42..44]
	temp = 100;
	setAlarmChannel(id, ix++, "THD U12", temp, &pm->THD_Upp[0]);
	setAlarmChannel(id, ix++, "THD U23", temp, &pm->THD_Upp[1]);
	setAlarmChannel(id, ix++, "THD U31", temp, &pm->THD_Upp[2]);
	// [45..47]
	temp = 100;
	setAlarmChannel(id, ix++, "THD I1", temp, &pm->THD_I[0]);
	setAlarmChannel(id, ix++, "THD I2", temp, &pm->THD_I[1]);
	setAlarmChannel(id, ix++, "THD I3", temp, &pm->THD_I[2]);
	// [49..52]
	temp = db.pt[id].vnorm*db.ct[id].inorm;
	setAlarmChannel(id, ix++, "DD P+", temp, &pdmd->DD_P[0]);
	setAlarmChannel(id, ix++, "DD P-", temp, &pdmd->DD_P[1]);
	setAlarmChannel(id, ix++, "DD Q-L", temp, &pdmd->DD_Q[0]);
	setAlarmChannel(id, ix++, "DD Q-C", temp, &pdmd->DD_Q[1]);
	setAlarmChannel(id, ix++, "DD S", temp, &pdmd->DD_S);
	// [53..55]
	temp = db.ct[id].inorm;
	setAlarmChannel(id, ix++, "DD I1", temp, &pdmd->DD_I[0]);
	setAlarmChannel(id, ix++, "DD I2", temp, &pdmd->DD_I[1]);
	setAlarmChannel(id, ix++, "DD I3", temp, &pdmd->DD_I[2]);
	// [56..60]
	temp = db.pt[id].vnorm*db.ct[id].inorm;
	setAlarmChannel(id, ix++, "MD P+", temp, &pdmd->MD_P[0].value);
	setAlarmChannel(id, ix++, "MD P-", temp, &pdmd->MD_P[1].value);
	setAlarmChannel(id, ix++, "MD Q-L", temp, &pdmd->MD_Q[0].value);
	setAlarmChannel(id, ix++, "MD Q-C", temp, &pdmd->MD_Q[1].value);	
	setAlarmChannel(id, ix++, "MD S", temp, &pdmd->MD_S.value);
	// [61..63]
	temp = db.ct[id].inorm;
	setAlarmChannel(id, ix++, "MD I1", temp, &pdmd->MD_I[0].value);
	setAlarmChannel(id, ix++, "MD I2", temp, &pdmd->MD_I[1].value);
	setAlarmChannel(id, ix++, "MD I3", temp, &pdmd->MD_I[2].value);
	// [64..65]
	setAlarmChannel(id, ix++, "UN I2", temp, &pm->Ibal[0]);
	setAlarmChannel(id, ix++, "UN I0", temp, &pm->Ibal[1]);
	// [66]
	temp = 0;
	setAlarmChannel(id, ix++, "Summary", temp, &meter[id].almCnt);
	// [67..76]
	// temp = 0;
	// setAlarmChannel(ix++, "INT DI1", temp, &meter.dim[0]);
	// setAlarmChannel(ix++, "INT DI2", temp, &meter.dim[1]);
	// setAlarmChannel(ix++, "INT DI3", temp, &meter.dim[2]);
	// setAlarmChannel(ix++, "INT DI4", temp, &meter.dim[3]);
	// setAlarmChannel(ix++, "INT DI5", temp, &meter.dim[4]);
	// setAlarmChannel(ix++, "INT DI6", temp, &meter.dim[5]);
	// setAlarmChannel(ix++, "INT DI7", temp, &meter.dim[6]);
	// setAlarmChannel(ix++, "INT DI8", temp, &meter.dim[7]);
	// setAlarmChannel(ix++, "INT DO1", temp, &meter.dom[0]);
	// setAlarmChannel(ix++, "INT DO2", temp, &meter.dom[1]);	
	// // [77..86]
	// temp = 0;
	// setAlarmChannel(ix++, "iPSM#1 COMM", temp, &pcntl->gems_comm_sts[0]);	
	// setAlarmChannel(ix++, "iPSM#1 DI", temp, &pcntl->gems_int_sts[0][8]);	
	// setAlarmChannel(ix++, "iPSM#1 TEMP", temp, &pcntl->gems_int_sts[0][9]);	
	// setAlarmChannel(ix++, "iPSM#1 IG", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#1 R SWELL", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#1 S SWELL", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#1 T SWELL", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#1 R SAG", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#1 S SAG", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#1 T SAG", temp, NULL);	
	// // [87..96]
	// temp = 0;
	// setAlarmChannel(ix++, "iPSM#2 COMM", temp, &pcntl->gems_comm_sts[1]);	
	// setAlarmChannel(ix++, "iPSM#2 DI", temp,  &pcntl->gems_int_sts[1][8]);
	// setAlarmChannel(ix++, "iPSM#2 TEMP", temp, &pcntl->gems_int_sts[1][9]);
	// setAlarmChannel(ix++, "iPSM#2 IG", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#2 R SWELL", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#2 S SWELL", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#2 T SWELL", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#2 R SAG", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#2 S SAG", temp, NULL);	
	// setAlarmChannel(ix++, "iPSM#2 T SAG", temp, NULL);	
	// [97..99]
	temp = 0;
	setAlarmChannel(id, ix++, "UDEV_U1", temp, &pm->UUndev[0]);	
	setAlarmChannel(id, ix++, "UDEV_U2", temp, &pm->UUndev[1]);	
	setAlarmChannel(id, ix++, "UDEV_U3", temp, &pm->UUndev[2]);	
	// [100..102]
	setAlarmChannel(id, ix++, "ODEV_U1", temp, &pm->UOvdev[0]);	
	setAlarmChannel(id, ix++, "ODEV_U2", temp, &pm->UOvdev[1]);	
	setAlarmChannel(id, ix++, "ODEV_U3", temp, &pm->UOvdev[2]);	
	// [103..105]
	setAlarmChannel(id, ix++, "CF_U1", temp, &pm->CF_U[0]);	
	setAlarmChannel(id, ix++, "CF_U2", temp, &pm->CF_U[1]);	
	setAlarmChannel(id, ix++, "CF_U3", temp, &pm->CF_U[2]);	
	// [106..108]
	setAlarmChannel(id, ix++, "CF_U12", temp, &pm->CF_Upp[0]);	
	setAlarmChannel(id, ix++, "CF_U23", temp, &pm->CF_Upp[1]);	
	setAlarmChannel(id, ix++, "CF_U31", temp, &pm->CF_Upp[2]);	
	// [109..111]
	setAlarmChannel(id, ix++, "CF_I1", temp, &pm->CF_I[0]);	
	setAlarmChannel(id, ix++, "CF_I2", temp, &pm->CF_I[1]);	
	setAlarmChannel(id, ix++, "CF_I3", temp, &pm->CF_I[2]);	
	// [112..114]
	setAlarmChannel(id, ix++, "KF_I1", temp, &pm->KF_I[0]);	
	setAlarmChannel(id, ix++, "KF_I2", temp, &pm->KF_I[1]);	
	setAlarmChannel(id, ix++, "KF_I3", temp, &pm->KF_I[2]);	
	// [115..117]
	setAlarmChannel(id, ix++, "Pst 1", temp, NULL);	
	setAlarmChannel(id, ix++, "Pst 2", temp, NULL);	
	setAlarmChannel(id, ix++, "Pst 3", temp, NULL);	
	// [118..120]
	setAlarmChannel(id, ix++, "Plt 1", temp, NULL);	
	setAlarmChannel(id, ix++, "Plt 2", temp, NULL);	
	setAlarmChannel(id, ix++, "Plt 3", temp, NULL);	
	// [121..123]
	setAlarmChannel(id, ix++, "Sig. Volt 1", temp, NULL);	
	setAlarmChannel(id, ix++, "Sig. Volt 2", temp, NULL);	
	setAlarmChannel(id, ix++, "Sig. Volt 3", temp, NULL);
}


//typedef struct {
//	uint16_t status;
//	uint16_t count;
//} ALARM_DATA;

//typedef struct {
//	uint16_t seq, r0[3];
//	ALARM_DATA almSt[32];
//	uint16_t r1[32];
//} ALARM_STATUS;

//typedef struct {
//	uint16_t delay, hyst, r0[2];
//	struct {
//		uint16_t active, chan, cond, action;
//		float level;
//	} alarm[32];
//	uint16_t r1[4];
//} ALARM_DEF;

__inline int _almCompFnc(float val, float limit, int cond, int dband, float norm) {
	float	db = norm*dband/100.;
	if (cond == 0) {
		return val < (limit-db);
	}
	else {
		return val > limit;
	}
}

//__inline int _compare(float val, float limit, int cond) {
//	if (cond == 0) {
//		return (val < limit);
//	}
//	else {
//		return (val > limit);
//	}
//}


void resetAlarmList(int id) {
	ALARM_STATUS *palm = &meter[id].alarm;
	ALARM_LIST *plist = &meter[id].alist;

	palm->resetTs = palm->updateTs = sysTick1s;
	plist->fr = plist->re = plist->count = 0;
}

int loadAlarmStatus(int id) {
	FILE *fp;
	int i;
	ALARM_DEF *paset = &meter[id].almSet;
	ALARM_STATUS *palm = &meter[id].alarm;
	const char *path = getAlarmStatusFileName(id);

	fp = fopen(path, "rb");
	if (fp != NULL) {
		fread(palm, sizeof(ALARM_STATUS), 1, fp);
		fclose(fp);
	}
	
	for (i=0; i<32; i++) {
		// 채널이 변경되면 기존 alarm count를 지운다 
		if (palm->st[i].chan != paset->set[i].chan) {
			palm->st[i].chan = paset->set[i].chan;
			palm->st[i].count = 0;
		}
		palm->st[i].cond   = paset->set[i].cond;			
		palm->st[i].level  = paset->set[i].level;
		palm->st[i].status = 0;
		
		if (palm->st[i].chan) {
			printf("++ almset(M%d,%d) -> stat(%d), chan(%d), cond(%d), level(%f), count(%d)\n", id, i, 
				palm->st[i].status, palm->st[i].chan, palm->st[i].cond, palm->st[i].level, palm->st[i].count);		
		}
	}	
	return 0;
}

int storeAlarmStatus(int id) {
	FILE *fp;
	const char *path = getAlarmStatusFileName(id);
	ALARM_STATUS *palm = &meter[id].alarm;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return -1;
	}	
	fwrite(palm, sizeof(ALARM_STATUS), 1, fp);
	fclose(fp);
	return 0;
}

int deleteAlarmLog(int id) {
	char path[64];
	int res;
	ALARM_FIFO *pFifo = &meter[id].alarmFifo;

	getAlarmFifoFileName(id, path);
#ifdef USE_CMSIS_RTOS2	
   res = fdelete(path, NULL);	
#else
	res = fdelete(path);
#endif	
	
	memset(pFifo, 0, sizeof(*pFifo));
	printf("---> deleteAlarmLog(M%d), result=%d\n", id, res);
	return res;
}

void reverseSortAlarmBuffer(ALARM_LIST* rb) {
	// 임시 배열에 버퍼 내용을 복사
	ALARM_LOG* tempArray = (ALARM_LOG*)malloc(rb->count * sizeof(ALARM_LOG));
	int index = rb->fr;
	int	i, j;
	for (i = 0; i < rb->count; i++) {
		tempArray[i] = rb->alog[index];
		index = (index + 1) % N_ALARM_LIST;
	}

	// 배열을 역순으로 정렬
	for (i = 0; i < rb->count - 1; i++) {
		for (j = 0; j < rb->count - i - 1; j++) {
			if (tempArray[j].ts < tempArray[j + 1].ts) {
				ALARM_LOG temp = tempArray[j];
				tempArray[j] = tempArray[j + 1];
				tempArray[j + 1] = temp;
			}
		}
	}

	// 정렬된 배열을 다시 링 버퍼에 복사
	index = rb->fr;
	for (i = 0; i < rb->count; i++) {
		rb->alog[index] = tempArray[i];
		index = (index + 1) % N_ALARM_LIST;
	}

	free(tempArray);
}


int loadAlarmLog(int id) {
	FILE *fp;
	int year, woY, nlog, i, n=0;
#ifdef USE_CMSIS_RTOS2	
   fsFileInfo info;	
#else
	FINFO info;
#endif
	char path[64];
	ALARM_FIFO *pFifo = &meter[id].alarmFifo;
	CNTL_DATA *pcntlId = &meter[id].cntl;
	ALARM_LIST *plist = &meter[id].alist;
#if 1
	ALARM_U	alog;
	int fr, re;

	if (id == 0) {
		/* 이번 배포 1회: 손상 가능한 알람 FIFO 삭제(센티넬 있으면 스킵) */
		fp = fopen(ALARM_FIFO_PURGE_SENTINEL, "rb");
		if (fp == NULL) {
#ifdef USE_CMSIS_RTOS2
			(void)fdelete(ALARM_FIFO_FILE, NULL);
#else
			(void)fdelete(ALARM_FIFO_FILE);
#endif
			memset(pFifo, 0, sizeof(*pFifo));
			fp = fopen(ALARM_FIFO_PURGE_SENTINEL, "wb");
			if (fp != NULL) {
				const char mark = '1';
				fwrite(&mark, 1, 1, fp);
				fclose(fp);
			}
			printf("---> one-shot purge: removed %s (sentinel %s)\n", ALARM_FIFO_FILE, ALARM_FIFO_PURGE_SENTINEL);
		} else {
			fclose(fp);
		}
	}

	getAlarmFifoFileName(id, path);
	// 시간순으로 읽는다
	fp = fopen(path, "rb");
	if (fp != NULL) {
		fread(&alog, sizeof(alog), 1, fp);
		
		for (i=0; i<alog.head.count; i++) {			
			fread(&pFifo->alog[i], sizeof(alog), 1, fp);			

			pFifo->fr++;
			pFifo->count++;
		}
		fclose(fp);
	}
	
	fetchAlarm(id, 3);
		
//		re = pAlmFifo->re;
//		for (i=0; i<pAlmFifo->count; i++) {
//			if (i < N_ALARM_LIST) {				
//				memcpy(&palist->alog[i], &pAlmFifo->alog[re], sizeof(alog));
//				if(++re == N_ALARM_FIFO) re = 0;
//				
//				palist->fr++;
//				palist->count++;
//				palist->seq++;
//			}
//			else {
//				break;
//			}	
// 	}
#else	
	//woY = getYear_n_WoY(pcntlId->tod.tm_yday, pcntlId->tod.tm_wday);
	sprintf(path, "%s%04d%02d.d", ALARM_LIST_FILE, pcntlId->tod.tm_year, pcntlId->tod.tm_mon);

	info.fileID = 0;      
	if (ffind (path, &info)) {
		return -1;
	}
	
	nlog = info.size/sizeof(ALARM_LOG);
	printf("[[Alarm List File(%s) Size = %d, #alarm=%d\n", path, info.size, nlog);
	  
	fp = fopen(path, "rb"); 
	if (fp == NULL) {
		return -1;
	}
	
	plist->re = 0;
	n = (nlog <= (N_ALARM_LIST)) ? nlog : (N_ALARM_LIST);
	plist->count = n;
	fseek(fp, sizeof(ALARM_LOG)*(nlog-n), SEEK_SET);
	
	for (i=0; i<n; i++) {
		fread(&plist->alog[i], sizeof(ALARM_LOG), 1, fp);
	}

	reverseSortAlarmBuffer(plist);
	fclose(fp);
#endif
	return 0;
}

#if 0
void reverseSortEventBuffer(EVENT_LIST* rb) {
	// 임시 배열에 버퍼 내용을 복사
	EVENT_LOG* tempArray = (EVENT_LOG*)malloc(rb->count * sizeof(EVENT_LOG));
	int index = rb->fr;
	int	i, j;
	for (i = 0; i < rb->count; i++) {
		tempArray[i] = rb->elog[index];
		index = (index + 1) % N_EVENT_LIST;
	}

	// 배열을 역순으로 정렬
	for (i = 0; i < rb->count - 1; i++) {
		for (j = 0; j < rb->count - i - 1; j++) {
			if (tempArray[j].startTs < tempArray[j + 1].startTs) {
				EVENT_LOG temp = tempArray[j];
				tempArray[j] = tempArray[j + 1];
				tempArray[j + 1] = temp;
			}
		}
	}

	// 정렬된 배열을 다시 링 버퍼에 복사
	index = rb->fr;
	for (i = 0; i < rb->count; i++) {
		rb->elog[index] = tempArray[i];
		index = (index + 1) % N_EVENT_LIST;
	}

	free(tempArray);
}

void reverseSortITICBuffer(EVENT_FIFO* rb) {
   // 임시 배열에 버퍼 내용을 복사
   ITIC_LOG* tempArray = (ITIC_LOG*)malloc(rb->count * sizeof(ITIC_LOG));
   int index = rb->fr;
   int 	i,j;

   for (i = 0; i < rb->count; i++) {
	   tempArray[i] = rb->elog[index];
	   index = (index + 1) % N_ITIC_BUF;
   }

   // 배열을 역순으로 정렬
   for (i = 0; i < rb->count - 1; i++) {
	   for (j = 0; j < rb->count - i - 1; j++) {
		   if (tempArray[j].startTs < tempArray[j + 1].startTs) {
			   ITIC_LOG temp = tempArray[j];
			   tempArray[j] = tempArray[j + 1];
			   tempArray[j + 1] = temp;
		   }
	   }
   }

   // 정렬된 배열을 다시 링 버퍼에 복사
   index = rb->fr;
   for (i = 0; i < rb->count; i++) {
	   rb->elog[index] = tempArray[i];
	   index = (index + 1) % N_ITIC_BUF;
   }

   free(tempArray);
}
#endif

int loadEventLog(void)
{
	int id;

	for (id = 0; id < METER_CH_COUNT; id++)
		loadEventFifo(id);
	return 0;
}


int storeAlarmLog(int id, int ix, int status, float value, int doSel) {
	FILE *fp;
	char path[64];
	char fifoPath[64];
	uint16_t	do_action;
	int		i;
	ALARM_U	alog;
	CNTL_DATA *pcntlId = &meter[id].cntl;
	ALARM_STATUS *palm = &meter[id].alarm;
	ALARM_FIFO *pFifo = &meter[id].alarmFifo;
		
	pcntlId->alog.ts = sysTick1s;
	pcntlId->alog.chan = palm->st[ix].chan;
	pcntlId->alog.cond = palm->st[ix].cond;
	pcntlId->alog.level = palm->st[ix].level;
	pcntlId->alog.value = value;
	pcntlId->alog.status = status;

	//woY = getYear_n_WoY(pcntlId->tod.tm_yday, pcntlId->tod.tm_wday);
	sprintf(path, "%s%04d%02d.d", ALARM_LIST_FILE, pcntlId->tod.tm_year, pcntlId->tod.tm_mon);
	fp = fopen(path, "ab"); 
	if (fp != NULL) {
		fwrite(&pcntlId->alog, sizeof(ALARM_LOG), 1, fp);
		fclose(fp);
	}
#if 1	// 2025-3-13, alarm Fifo
	getAlarmFifoFileName(id, fifoPath);
	printf("ALARM_FIFO_FILE(M%d):%s\n", id, fifoPath);
	fp = fopen(fifoPath, "r+b");
	if (fp == NULL) {
		// create header
		memset(&alog, 0, sizeof(alog));
		alog.head.magic = 0x1234abcd;
		alog.head.fr = 1;		
		alog.head.count = 1;
		alog.head.ts = sysTick1s;
		fp = fopen(fifoPath, "wb");
		fwrite(&alog, sizeof(alog), 1, fp);
		fwrite(&pcntlId->alog, sizeof(ALARM_LOG), 1, fp);
		fclose(fp);
	}
	else {
		// update header
		fread(&alog, sizeof(alog), 1, fp);
		if (alog.head.count < LOG_FIFO_SIZE) {
			alog.head.count++;
		}
		if (++alog.head.fr > LOG_FIFO_SIZE) {
			alog.head.fr = 1;
		}	
		// update header
		fseek(fp, 0, SEEK_SET);
		fwrite(&alog, sizeof(alog), 1, fp);						
		// append or update data
		fseek(fp, sizeof(alog)*alog.head.fr, SEEK_SET);
		fwrite(&pcntlId->alog, sizeof(ALARM_LOG), 1, fp);
		fclose(fp);
	}
	
	// 2025-3-20, Alarm FiFo에 추가한다
	memcpy(&pFifo->alog[pFifo->fr], &pcntlId->alog, sizeof(pcntlId->alog));	
	if (pFifo->count < N_ALARM_FIFO) {
		pFifo->count++;
		if (++pFifo->fr >= N_ALARM_FIFO) pFifo->fr = 0;
	}
	else {
		// Full 발생하면, fr, re 모두 이동한다
		if (++pFifo->re >= N_ALARM_FIFO) pFifo->re = 0;
		if (++pFifo->fr >= N_ALARM_FIFO) pFifo->fr = 0;
	}
	
	fetchAlarm(id, 0);
#else
	ALARM_LIST *plist = &meter[id].alist;
	for(i=N_ALARM_LIST-1; i>0; --i) {
		plist->alog[i] = plist->alog[i-1];
	}
	memcpy(&plist->alog[0], &pcntlId->alog, sizeof(ALARM_LOG));
	if (plist->count < (N_ALARM_LIST+1)) {
		plist->count++;
	}
#endif	
	
	printf("alarm log(M%d) [Ts=%d, st=%d, value=%f, chan=%d, cond=%d, doPnt = %d]\n", id,
		pcntlId->alog.ts, pcntlId->alog.status, pcntlId->alog.value, pcntlId->alog.chan, pcntlId->alog.cond, doSel);	

#if 1
	// doSel과 관계없이 호출한다(cskang)
	// if(status && doSel !=0)
	assertAlarmOutput(doSel, status);
#endif
}


// FS Task로 부터 호출된다 
int storeEventLog(EVENT_LOG *pelog) {	
	FILE *fp;
	char path[64];
	int year, woY;

	//woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
	sprintf(path, "%s%04d%02d.d", EVENT_LIST_FILE, pcntl->tod.tm_year, pcntl->tod.tm_mon);
	//sprintf(path, "%s%04d.d", EVENT_LIST_FILE, pcntl->tod.tm_year);
	fp = fopen(path, "ab"); 
	if (fp != NULL) {
		fwrite(pelog, sizeof(EVENT_LOG), 1, fp);
		fclose(fp);
	}
}


#if 1
void fetchAlarm(int id, int cmd) {
	int i, n, ix;
	ALARM_FIFO *pFifo = &meter[id].alarmFifo;
	ALARM_LIST *plist = &meter[id].alist;

	plist->count = pFifo->count;
	
	// update
	if (cmd == 0) {
	}
	// page down
	else if (cmd == 1) {
		if (plist->re + N_ALARM_LIST < pFifo->count) 
			plist->re += N_ALARM_LIST;	
	}
	// page up
	else if (cmd == 2) {
		if (plist->re - N_ALARM_LIST >= 0) 
			plist->re -= N_ALARM_LIST;
	}
	// top
	else if (cmd == 3) {
		plist->re = 0;
	}
	// bottom
	else if (cmd == 4) {
		plist->re = (plist->count-1)/N_ALARM_LIST * N_ALARM_LIST;
	}
	
	// src
	ix = (pFifo->fr - plist->re) >= 0 ? (pFifo->fr - plist->re) : (pFifo->fr - plist->re) + N_ALARM_FIFO;
	// dst
	plist->fr = plist->re;
	for (i=0; i<N_ALARM_LIST; i++) {
		if (plist->fr < pFifo->count) {
			if (--ix < 0) ix = N_ALARM_FIFO-1;
			memcpy(&plist->alog[i], &pFifo->alog[ix], sizeof(ALARM_LOG));			
			// dst
			plist->fr++;
		}
		else {
			// fill zero
			memset(&plist->alog[i], 0, sizeof(ALARM_LOG));
		}
	}	
}

// 시간순으로 나열된 리스트를 역순으로 저장한다
void fetchEvent(int id, int cmd) {
	int i, n, ix;
	EVENT_FIFO *pFifo = &meter[id].eventFifo;
	EVENT_LIST *plist = &meter[id].elist;
	
	plist->count = pFifo->count;
	
	// update (현재 페이지 로드)
	if (cmd == 0) {
	}
	// page down
	else if (cmd == 1) {
		if (plist->re + N_EVENT_LIST < pFifo->count) 
			plist->re += N_EVENT_LIST;	
	}
	// page up
	else if (cmd == 2) {
		if (plist->re - N_EVENT_LIST >= 0) 
			plist->re -= N_EVENT_LIST;
	}
	// top
	else if (cmd == 3) {
		plist->re = 0;
	}
	// bottom
	else if (cmd == 4) {
		plist->re = (plist->count-1)/N_EVENT_LIST * N_EVENT_LIST;
	}
	
	// src
	ix = (pFifo->fr - plist->re) >= 0 ? (pFifo->fr - plist->re) : (pFifo->fr - plist->re) + N_EVENT_FIFO;
	// dst
	plist->fr = plist->re;
	for (i=0; i<N_EVENT_LIST; i++) {
		if (plist->fr < pFifo->count) {
			if (--ix < 0) ix = N_EVENT_FIFO-1;
			memcpy(&plist->elog[i], &pFifo->elog[ix], sizeof(EVENT_LOG));			
			// dst
			plist->fr++;
		}
		else {
			// fill zero
			memset(&plist->elog[i], 0, sizeof(EVENT_LOG));
		}
	}
}


static int iticTypeMatches(int mode, uint16_t type)
{
	if (mode == 0)
		return type >= E_SAG && type <= E_RVC;
	return type == E_TrV || type == E_TrC;
}

static void fetchIticCommon(int id, int mode, int cmd)
{
	int i, n, ix, slot, seen, dst, matchTotal;
	EVENT_FIFO *pFifo;
	ITIC_EVT_LIST *plist;

	if (id < 0 || id >= METER_CH_COUNT)
		return;

	pFifo = &meter[id].eventFifo;
	plist = (mode == 0) ? &meter[id].itic : &meter[id].itic2;

	matchTotal = 0;
	for (i = 0; i < pFifo->count; i++) {
		if (pFifo->count < EVENT_LOG_CAP)
			slot = i;
		else
			slot = (pFifo->re + i) % EVENT_LOG_CAP;
		if (iticTypeMatches(mode, pFifo->elog[slot].type))
			matchTotal++;
	}
	plist->count = matchTotal;

	if (cmd == 1) {
		if (plist->re + N_ITIC_LIST < matchTotal)
			plist->re += N_ITIC_LIST;
	}
	else if (cmd == 2) {
		if (plist->re >= N_ITIC_LIST)
			plist->re -= N_ITIC_LIST;
		else
			plist->re = 0;
	}
	else if (cmd == 3) {
		plist->re = 0;
	}
	else if (cmd == 4) {
		if (matchTotal > 0)
			plist->re = (matchTotal - 1) / N_ITIC_LIST * N_ITIC_LIST;
		else
			plist->re = 0;
	}

	for (i = 0; i < N_ITIC_LIST; i++)
		memset(&plist->elog[i], 0, sizeof(ITIC_LOG));

	ix = pFifo->fr;
	seen = 0;
	dst = 0;
	for (n = 0; n < pFifo->count && dst < N_ITIC_LIST; n++) {
		if (--ix < 0)
			ix = EVENT_LOG_CAP - 1;
		if (!iticTypeMatches(mode, pFifo->elog[ix].type))
			continue;
		if (seen < plist->re) {
			seen++;
			continue;
		}
		memcpy(&plist->elog[dst], &pFifo->elog[ix], sizeof(ITIC_LOG));
		dst++;
		seen++;
	}
	plist->fr = plist->re + dst;
}

void fetchItic(int id, int cmd)
{
	fetchIticCommon(id, 0, cmd);
}

void fetchItic2(int id, int cmd)
{
	fetchIticCommon(id, 1, cmd);
}
#endif

// 매 1초 마다 호출되어야 한다 
int alarmProc(int id) {
	int i, chan, cond, change=0, result, almCount=0, doPoint=0;
	float level, *src;
	ALARM_DEF *paset = &meter[id].almSet;
	ALARM_STATUS *palm = &meter[id].alarm;
	CNTL_DATA *pcntlId = &meter[id].cntl;
	
	// 시작 후 모든 값이 안정화 될때 까지 기다린다(5s)
	if (pcntlId->online2++ < 5) return 0;
		
	for (i=0; i<32; i++) {		
		if (paset->set[i].chan == 0) continue;
		if (paset->set[i].chan >= MAX_ALARM_CH) continue;
		
		cond = paset->set[i].cond & 1;
		chan = paset->set[i].chan;
//		doPoint = paset->set[i].do_action;
		src  = almTbl[id][chan].src;
		if (src == 0) {
			printf("@@@ Bad src(M%d), channel = %d\n", id, chan);
			continue;
		}

		if (palm->st[i].status == 0) {					
			//level = almTbl[id][chan].norm*paset->set[i].level/100.;				
			level = palm->st[i].level;
			if(cond !=2)
				result = (cond == 0) ? (*src < level) : (*src > level);
			else {
				result = (*src == level);
			}
				
			if (result) {
				if (pcntlId->almTimer[i]++ >= paset->delay) {	
					// alarm 상태 
					palm->st[i].status = 1;	
					// alarm count 증가
					palm->st[i].count++;
					// alarm log 기록
					storeAlarmLog(id, i, palm->st[i].status, *src, doPoint);					
					change++;
				}
			}
			else {
				pcntlId->almTimer[i] = 0;
			}
		}
		else {	
			// > (limit + deadband)
			if(cond !=2){
				if (cond == 0) {
#if 1	// 2025-3-18, 정격을 알수 없으므로 % of level로 처리한다				
					level = palm->st[i].level + palm->st[i].level*paset->set[i].dband/100.;
#else				
					level = palm->st[i].level + almTbl[id][chan].norm*paset->set[i].dband/100.;
#endif					
					if(*src >= level)
						result = 1;
					else
						result = 0;
				}	
				
				// < (limit - deadband)
				else {
#if 1	// 2025-3-18, 정격을 알수 없으므로 % of level로 처리한다	
					level = palm->st[i].level - palm->st[i].level*paset->set[i].dband/100.;
#else					
					level = palm->st[i].level - almTbl[id][chan].norm*paset->set[i].dband/100.;
#endif					
					if(*src <= level)
						result = 1;
					else
						result = 0;
				}
			}
			else {
				level = palm->st[i].level;
				
				if(*src == level)
					result = 0;
				else
					result = 1;	
			}									
			if (result) {
				if (pcntlId->almTimer[i]++ >= paset->delay) {
					// normal 상태 
					palm->st[i].status = 0;	
					// alarm이 복귀될때 리스트에 추가된다 
					storeAlarmLog(id, i, 0, *src, doPoint);
					change++;
				}
			}
			else {
				pcntlId->almTimer[i] = 0;
			}
		}	

		if (palm->st[i].status) {
			almCount++;
		}
	}
	
	palm->almCount = almCount;	
	meter[id].almCnt = almCount;
	return change;
}





//		uint16_t chan, cond, dband, action;
//		float level;

void buildAlarmSettings(int id) {
	int i=0, chan;
	float pcent;
	
	ALARM_DEF *paset = &meter[id].almSet;
	ALARM_STATUS *palm = &meter[id].alarm;
	
	paset->delay = 1;

	// test용
#ifdef	METER_TEST_DATA
	paset->set[i].cond = 0;	// less than
	paset->set[i].chan = 3;	// U1
	paset->set[i].level = 90;	// 90%@norm
	paset->set[i].dband = 1;	// 1%
	paset->set[i].do_action = 2;
	i++;

	paset->set[i].cond = 0;	// less than
	paset->set[i].chan = 4;	// U2
	paset->set[i].level = 90;	// 90%@norm
	paset->set[i].dband = 1;	// 1%
	paset->set[i].do_action = 2;
	i++;

	paset->set[i].cond = 0;	// less than
	paset->set[i].chan = 5;	// U3
	paset->set[i].level = 90;	// 90%@norm
	paset->set[i].dband = 1;	// 1%
	paset->set[i].do_action = 2;
	i++;

	for (; i<32; i++) {
		memset(&paset->set[i], 0, sizeof(paset->set[0]));
	}
#else
// loadAlarmStatus로 이동	
	for (i=0; i<32; i++) {
		chan = paset->set[i].chan;		
		if ((chan > 0) && (chan <= MAX_ALARM_CH)) {
			pcent = paset->set[i].level/10000.;
			palm->st[i].chan = chan;
			palm->st[i].cond = paset->set[i].cond;			
			palm->st[i].level = paset->set[i].level;
			printf(">> almset(M%d,%d) -> chan(%d), cond(%d), level(%f)\n", id, i, palm->st[i].chan, palm->st[i].cond, palm->st[i].level);
		}
		else if(chan > MAX_ALARM_CH)
			printf(">> almset(M%d,%d) -> chan(%d) Error!!!!\n", id, i, palm->st[i].chan);
	}
#endif
}


void buildTrendSetting() {
	TREND_DEF *ptrd = meter[0].trend;
	int ix=0;
	
	ptrd[0].active = 1;
	ptrd[0].interval = 1;	// 1min
	ptrd[0].chan[ix++] = 3;
	ptrd[0].chan[ix++] = 4;
	ptrd[0].chan[ix++] = 5;	
	ptrd[0].chan[ix++] = 13;
	ptrd[0].chan[ix++] = 14;
	ptrd[0].chan[ix++] = 15;
}




uint16_t trdTime[] = {1,2,3,5,10,15,20,30,60};

void getTrendData(int mid, int gid, uint16_t *chan) {
	int i;

	if (mid < 0 || mid >= METER_CH_COUNT)
		return;

	row[mid][gid].ts = sysTick1s;
	row[mid][gid].valid = 1;
	
	for (i=0; i<16; i++) {
		if (chan[i] == 0) {
			row[mid][gid].pen[i] = 0;
		}
		else if (chan[i] < MAX_ALARM_CH && almTbl[mid][chan[i]].src != NULL) {
			row[mid][gid].pen[i] = *almTbl[mid][chan[i]].src;
		}
	}	
}

// 그룹-년-월(·CH)로 구성된다. 즉 월마다 파일이 생성된다.
void getTrendFile(char *str, int mid, int g) {
	if (mid == 0)
		sprintf(str, "%s%d_%04d%02dV%d.d", TREND_FILE, g, pcntl->tod.tm_year, pcntl->tod.tm_mon, 0);
	else
		sprintf(str, "%s%d_%04d%02dM%d.d", TREND_FILE, g, pcntl->tod.tm_year, pcntl->tod.tm_mon, mid);
}

void getTrendBackupFile(char *str, int mid, int g) {
	if (mid == 0)
		sprintf(str, "trd%d_%04d%02dV%d_%02d.d", g, pcntl->tod.tm_year, pcntl->tod.tm_mon, 0, pcntl->tod.tm_mday);
	else
		sprintf(str, "trd%d_%04d%02dM%d_%02d.d", g, pcntl->tod.tm_year, pcntl->tod.tm_mon, mid, pcntl->tod.tm_mday);
}

// 2020-4-8, Trend Header와 현재 Trend 설정을 비교하여 다르면 기존파일 이름에 수정날짜 이름 추가하여 변경한다 
int appendTrendRcrd(int mid, int g) {
	FILE *fp;
#ifdef USE_CMSIS_RTOS2	
   fsFileInfo fi;	
#else
	FINFO fi;
#endif
	int j;
	char fn[64];

	if (mid < 0 || mid >= METER_CH_COUNT)
		return 0;
	
	getTrendFile(fn, mid, g);
	
	fi.fileID = 0;      
	if (ffind (fn, &fi)) {
		printf("createTrendFile(%s) ...\n", fn);
		fp = fopen(fn, "wb");
		if (fp) {
			trdInf[mid][g].ts = sysTick1s;;
			trdInf[mid][g].type = 1;
			trdInf[mid][g].version = meter[mid].trend[g].version;
			for (j=0; j<16; j++) {
				trdInf[mid][g].channel[j] = meter[mid].trend[g].chan[j];
			}								
			fwrite(&trdInf[mid][g], sizeof(trdInf[0][0]), 1, fp);
			fwrite(&row[mid][g], sizeof(row[0][0]), 1, fp);			
			fclose(fp);				
		}		
	}
	else {	
		printf("appendTrendFile(%s), m=%d, g=%d, ts=%d, size = %d\n", fn, mid, g, row[mid][g].ts, fi.size);
		fp = fopen(fn, "ab");
		if (fp) {
			fwrite(&row[mid][g], sizeof(TREND_RECORD), 1, fp);			
			fclose(fp);				
		}
	}
	return 0;
}

// trend file header와 trend 설정을 비교한다 
void checkTrendHeader() {
	int mid, i, j, err=0;
	char fn[64], fnew[64];
	FILE *fp;
	
	for (mid = 0; mid < ACTIVE_METER_CH_COUNT; mid++) {
		for (i=0; i<4; i++) {
			if (meter[mid].trend[i].active == 0) 
				continue;
			
			getTrendFile(fn, mid, i);
			fp = fopen(fn, "rb");
			if (fp) {
				fread(&trdInf[mid][i], sizeof(trdInf[mid][i]), 1, fp);			
				fclose(fp);	

				for (err=0, j=0; j<16; j++) {
					if (trdInf[mid][i].channel[j] != meter[mid].trend[i].chan[j]) {
						printf("### Trend info changed M%d trdInf[%d][%d] = %d, %d\n",
							mid, i, j, trdInf[mid][i].channel[j], meter[mid].trend[i].chan[j]);
						err++;
					}
				}
				
				if (err) {
					getTrendBackupFile(fnew, mid, i);
					frename(fn, fnew);
					printf("### rename %s to %s\n", fn, fnew);
				}
			}
		}
	}
}

void Trend_Task(void *arg)		
{
	int mid, i, itv;	
	int lastsec=pcntl->tod.tm_sec;
	int lastmin=pcntl->tod.tm_min;
//	FINFO info;
//	FILE *fp;
	
	// 기존 트랜드 파일의 header와 trend 설정을 비교한다. 
	checkTrendHeader();
	_enableTaskMonitor(Tid_Trend, 50);
	while (pcntl->runFlag) {
		pcntl->wdtTbl[Tid_Trend].count++;
		// 1분 경과 대기
		if (lastmin == pcntl->tod.tm_min) {
         osDelayTask(1000);
			continue;
		}
		
		lastmin  = pcntl->tod.tm_min;
		
		// trend data 생성 (meter CH × trend group)
		for (mid = 0; mid < ACTIVE_METER_CH_COUNT; mid++) {
			for (i=0; i<4; i++) {			
				if (meter[mid].trend[i].active == 0) {
					continue;
				}
					
				itv = (meter[mid].trend[i].interval >= 8) ? 10 : trdTime[meter[mid].trend[i].interval];
				if ((pcntl->tod.tm_min % itv) == 0) {				
					getTrendData(mid, i, meter[mid].trend[i].chan);
				}
			}
		}
			
		// 저장 기간 최대 1년
		for (mid = 0; mid < ACTIVE_METER_CH_COUNT; mid++) {
			for (i=0; i<4; i++) {
				if (row[mid][i].valid) {
					appendTrendRcrd(mid, i);
					row[mid][i].valid = 0;
				}
			}
		}
	}
	
	printf("Trend_task stopped ...\n");
#ifdef __FREERTOS	
	vTaskSuspend(NULL);
#else
	os_evt_wait_and(0xffff, 0xffff);
#endif
}