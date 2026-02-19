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

extern METER_DEF meter[2];
extern int getYear_n_WoY(int doY, int doW);
extern void assertAlarmOutput(int point, int state);
	
//int getYear_n_WoY(int *pYear, int *woY);

COMP_TBL	almTbl[MAX_ALARM_CH] __attribute__ ((section ("EXT_RAM"), zero_init));
//COMP_TBL almTbl[100];

TREND_INFO trdInf[4];
TREND_RECORD	row[4];	// trend data
//extern MGEM3600_DATA	gems3600[];

void setAlarmChannel(int ix, char *pname, float norm, float *src) {
	almTbl[ix].nm = pname; 	
	almTbl[ix].norm = norm; 
	almTbl[ix++].src = src;
}

void initAlarmTable() {
	int ix=0;
	float temp;
	
	memset(almTbl, 0, sizeof(almTbl));
	// [0..2]
	setAlarmChannel(ix++, "-", 0, NULL);
	setAlarmChannel(ix++, "Temp.", 100, &pmeter->Temp);
	setAlarmChannel(ix++, "Freq.", pdb->pt.freq, &pmeter->Freq);
	// [3..6]
	temp = pdb->pt.vnorm;
	setAlarmChannel(ix++, "U1",  temp, &pmeter->U[0]);
	setAlarmChannel(ix++, "U2",  temp, &pmeter->U[1]);
	setAlarmChannel(ix++, "U3",  temp, &pmeter->U[2]);
	setAlarmChannel(ix++, "U~",  temp, &pmeter->U[3]);
	// [7..10]
	temp = (pdb->pt.wiring == WM_3LN3CT) ? pdb->pt.vnorm*sqrt(3) : pdb->pt.vnorm; 
	setAlarmChannel(ix++, "U12", temp, &pmeter->Upp[0]);
	setAlarmChannel(ix++, "U23", temp, &pmeter->Upp[1]);
	setAlarmChannel(ix++, "U31", temp, &pmeter->Upp[2]);
	setAlarmChannel(ix++, "Upp~", temp, &pmeter->Upp[3]);
	// [11..12]
	temp = 100;
	setAlarmChannel(ix++, "Uu", temp, &pmeter->Ubal[0]);
	setAlarmChannel(ix++, "Uo", temp, &pmeter->Ubal[1]);
	// [13..18]
	temp = pdb->ct.inorm;
	setAlarmChannel(ix++, "I1", temp, &pmeter->I[0]);
	setAlarmChannel(ix++, "I2", temp, &pmeter->I[1]);
	setAlarmChannel(ix++, "I3", temp, &pmeter->I[2]);
	setAlarmChannel(ix++, "I~", temp, &pmeter->I[3]);
	setAlarmChannel(ix++, "Itotal", temp*3, &pmeter->I[4]);
	setAlarmChannel(ix++, "In", temp, &pmeter->In);
	// [19..22]
	temp = pdb->pt.vnorm*pdb->ct.inorm;
	setAlarmChannel(ix++, "P1", temp, &pmeter->P[0]);
	setAlarmChannel(ix++, "P2", temp, &pmeter->P[1]);
	setAlarmChannel(ix++, "P3", temp, &pmeter->P[2]);
	setAlarmChannel(ix++, "Ptotal", temp*3, &pmeter->P[3]);
	// [23..26]
	temp = pdb->pt.vnorm*pdb->ct.inorm;
	setAlarmChannel(ix++, "Q1", temp, &pmeter->Q[0]);
	setAlarmChannel(ix++, "Q2", temp, &pmeter->Q[1]);
	setAlarmChannel(ix++, "Q3", temp, &pmeter->Q[2]);
	setAlarmChannel(ix++, "Q4", temp*3, &pmeter->Q[3]);
	// [27..30]
	temp = pdb->pt.vnorm*pdb->ct.inorm;
	setAlarmChannel(ix++, "D1", temp*3, &pmeter->D[0]);
	setAlarmChannel(ix++, "D2", temp*3, &pmeter->D[1]);
	setAlarmChannel(ix++, "D3", temp*3, &pmeter->D[2]);
	setAlarmChannel(ix++, "Dtotal", temp, &pmeter->D[3]);
	// [31..34]
	temp = pdb->pt.vnorm*pdb->ct.inorm;
	setAlarmChannel(ix++, "S1", temp, &pmeter->S[0]);
	setAlarmChannel(ix++, "S2", temp, &pmeter->S[1]);
	setAlarmChannel(ix++, "S3", temp, &pmeter->S[2]);
	setAlarmChannel(ix++, "Stotal", temp*3, &pmeter->S[3]);
	// [35..38]
	temp = 100;
	setAlarmChannel(ix++, "PF1", temp, &pmeter->PF[0]);
	setAlarmChannel(ix++, "PF2", temp, &pmeter->PF[1]);
	setAlarmChannel(ix++, "PF3", temp, &pmeter->PF[2]);
	setAlarmChannel(ix++, "PFtotal", temp, &pmeter->PF[3]);
	// [39..41]
	temp = 100;
	setAlarmChannel(ix++, "THD U1", temp, &pmeter->THD_U[0]);
	setAlarmChannel(ix++, "THD U2", temp, &pmeter->THD_U[1]);
	setAlarmChannel(ix++, "THD U3", temp, &pmeter->THD_U[2]);
	// [42..44]
	temp = 100;
	setAlarmChannel(ix++, "THD U12", temp, &pmeter->THD_Upp[0]);
	setAlarmChannel(ix++, "THD U23", temp, &pmeter->THD_Upp[1]);
	setAlarmChannel(ix++, "THD U31", temp, &pmeter->THD_Upp[2]);
	// [45..47]
	temp = 100;
	setAlarmChannel(ix++, "THD I1", temp, &pmeter->THD_I[0]);
	setAlarmChannel(ix++, "THD I2", temp, &pmeter->THD_I[1]);
	setAlarmChannel(ix++, "THD I3", temp, &pmeter->THD_I[2]);
	// [49..52]
	temp = pdb->pt.vnorm*pdb->ct.inorm;
	setAlarmChannel(ix++, "DD P+", temp, &pdm->DD_P[0]);
	setAlarmChannel(ix++, "DD P-", temp, &pdm->DD_P[1]);
	setAlarmChannel(ix++, "DD Q-L", temp, &pdm->DD_Q[0]);
	setAlarmChannel(ix++, "DD Q-C", temp, &pdm->DD_Q[1]);
	setAlarmChannel(ix++, "DD S", temp, &pdm->DD_S);
	// [53..55]
	temp = pdb->ct.inorm;
	setAlarmChannel(ix++, "DD I1", temp, &pdm->DD_I[0]);
	setAlarmChannel(ix++, "DD I2", temp, &pdm->DD_I[1]);
	setAlarmChannel(ix++, "DD I3", temp, &pdm->DD_I[2]);
	// [56..60]
	temp = pdb->pt.vnorm*pdb->ct.inorm;
	setAlarmChannel(ix++, "MD P+", temp, &pdm->MD_P[0].value);
	setAlarmChannel(ix++, "MD P-", temp, &pdm->MD_P[1].value);
	setAlarmChannel(ix++, "MD Q-L", temp, &pdm->MD_Q[0].value);
	setAlarmChannel(ix++, "MD Q-C", temp, &pdm->MD_Q[1].value);	
	setAlarmChannel(ix++, "MD S", temp, &pdm->MD_S.value);
	// [61..63]
	temp = pdb->ct.inorm;
	setAlarmChannel(ix++, "MD I1", temp, &pdm->MD_I[0].value);
	setAlarmChannel(ix++, "MD I2", temp, &pdm->MD_I[1].value);
	setAlarmChannel(ix++, "MD I3", temp, &pdm->MD_I[2].value);
	// [64..65]
	setAlarmChannel(ix++, "UN I2", temp, &pmeter->Ibal[0]);
	setAlarmChannel(ix++, "UN I0", temp, &pmeter->Ibal[1]);
	// [66]
	temp = 0;
	setAlarmChannel(ix++, "Summary", temp, &meter[0].almCnt);
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
	setAlarmChannel(ix++, "UDEV_U1", temp, &pmeter->UUndev[0]);	
	setAlarmChannel(ix++, "UDEV_U2", temp, &pmeter->UUndev[1]);	
	setAlarmChannel(ix++, "UDEV_U3", temp, &pmeter->UUndev[2]);	
	// [100..102]
	setAlarmChannel(ix++, "ODEV_U1", temp, &pmeter->UOvdev[0]);	
	setAlarmChannel(ix++, "ODEV_U2", temp, &pmeter->UOvdev[1]);	
	setAlarmChannel(ix++, "ODEV_U3", temp, &pmeter->UOvdev[2]);	
	// [103..105]
	setAlarmChannel(ix++, "CF_U1", temp, &pmeter->CF_U[0]);	
	setAlarmChannel(ix++, "CF_U2", temp, &pmeter->CF_U[1]);	
	setAlarmChannel(ix++, "CF_U3", temp, &pmeter->CF_U[2]);	
	// [106..108]
	setAlarmChannel(ix++, "CF_U12", temp, &pmeter->CF_Upp[0]);	
	setAlarmChannel(ix++, "CF_U23", temp, &pmeter->CF_Upp[1]);	
	setAlarmChannel(ix++, "CF_U31", temp, &pmeter->CF_Upp[2]);	
	// [109..111]
	setAlarmChannel(ix++, "CF_I1", temp, &pmeter->CF_I[0]);	
	setAlarmChannel(ix++, "CF_I2", temp, &pmeter->CF_I[1]);	
	setAlarmChannel(ix++, "CF_I3", temp, &pmeter->CF_I[2]);	
	// [112..114]
	setAlarmChannel(ix++, "KF_I1", temp, &pmeter->KF_I[0]);	
	setAlarmChannel(ix++, "KF_I2", temp, &pmeter->KF_I[1]);	
	setAlarmChannel(ix++, "KF_I3", temp, &pmeter->KF_I[2]);	
	// [115..117]
	setAlarmChannel(ix++, "Pst 1", temp, NULL);	
	setAlarmChannel(ix++, "Pst 2", temp, NULL);	
	setAlarmChannel(ix++, "Pst 3", temp, NULL);	
	// [118..120]
	setAlarmChannel(ix++, "Plt 1", temp, NULL);	
	setAlarmChannel(ix++, "Plt 2", temp, NULL);	
	setAlarmChannel(ix++, "Plt 3", temp, NULL);	
	// [121..123]
	setAlarmChannel(ix++, "Sig. Volt 1", temp, NULL);	
	setAlarmChannel(ix++, "Sig. Volt 2", temp, NULL);	
	setAlarmChannel(ix++, "Sig. Volt 3", temp, NULL);
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


void resetAlarmList() {
	palm->resetTs = palm->updateTs = sysTick1s;
	palist->fr = palist->re = palist->count = 0;
}

int loadAlarmStatus(void) {
	FILE *fp;
	char path[64];
	int i;
	ALARM_DEF *paset = &pdb->alarm;
	
	sprintf(path, "%s", ALARM_ST_FILE);
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
			printf("++ almset(%d) -> stat(%d), chan(%d), cond(%d), level(%f), count(%d)\n", i, 
				palm->st[i].status, palm->st[i].chan, palm->st[i].cond, palm->st[i].level, palm->st[i].count);		
		}
	}	
	return 0;
}

int storeAlarmStatus() {
	FILE *fp;
	int year, woY;
	char path[64];
	
	sprintf(path, "%s", ALARM_ST_FILE);
	fp = fopen(path, "wb");
	if (fp == NULL) {
		return -1;
	}	
	fwrite(palm, sizeof(ALARM_STATUS), 1, fp);
	fclose(fp);
	return 0;
}

int deleteAlarmLog(void) {
	char path[64];	
	int res;
	//sprintf(path, "%s%04d%02d.d", ALARM_LIST_FILE, pcntl->tod.tm_year, pcntl->tod.tm_mon);
	strcpy(path, ALARM_FIFO_FILE);
#ifdef USE_CMSIS_RTOS2	
   res = fdelete(path, NULL);	
#else
	res = fdelete(path);
#endif	
	
	// 2025-3-20, alarm Fifo 지운다 
	memset(pAlmFifo, 0, sizeof(*pAlmFifo));
	printf("---> deleteAlarmLog, result=%d\n", res);
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


int loadAlarmLog(void) {
	FILE *fp;
	int year, woY, nlog, i, n=0;
#ifdef USE_CMSIS_RTOS2	
   fsFileInfo info;	
#else
	FINFO info;
#endif
	char path[64];
#if 1
	ALARM_U	alog;
	int fr, re;
	
	// 시간순으로 읽는다
	fp = fopen(ALARM_FIFO_FILE, "rb");
	if (fp != NULL) {
		fread(&alog, sizeof(alog), 1, fp);
		
		for (i=0; i<alog.head.count; i++) {			
			fread(&pAlmFifo->alog[i], sizeof(alog), 1, fp);			

			pAlmFifo->fr++;
			pAlmFifo->count++;
		}
		fclose(fp);
	}
	
	fetchAlarm(3);
		
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
	//woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
	sprintf(path, "%s%04d%02d.d", ALARM_LIST_FILE, pcntl->tod.tm_year, pcntl->tod.tm_mon);

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
	
	palist->re = 0;
	n = (nlog <= (N_ALARM_LIST)) ? nlog : (N_ALARM_LIST);
	palist->count = n;
	fseek(fp, sizeof(ALARM_LOG)*(nlog-n), SEEK_SET);
	
	for (i=0; i<n; i++) {
		fread(&palist->alog[i], sizeof(ALARM_LOG), 1, fp);
	}

	reverseSortAlarmBuffer(palist);
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

int loadEventLog(void) {
	FILE *fp;
	int year, woY, nlog, i, n=0;
#ifdef USE_CMSIS_RTOS2	
   fsFileInfo info;	
#else
	FINFO info;
#endif
	char path[64];
#if 1
	EVENT_LOG	_itic;
#endif
	
#if 1	// 2025-3-13, cskang, FIFO로 변경
	EVENT_U	elog;
	int fr, re;
	
	// 시간순으로 읽는다 (최신 알람이 끝에 저장된다)
	fp = fopen(EVENT_FIFO_FILE, "rb");
	if (fp != NULL) {
		fread(&elog, sizeof(EVENT_U), 1, fp);
		for (i=0; i<elog.head.count; i++) {
			fread(&pEvtFifo->elog[i], sizeof(EVENT_U), 1, fp);
			pEvtFifo->fr++;
			pEvtFifo->count++;
		}
		fclose(fp);
	}
	
	// 읽은 내용을 이벤트, ITIC 리스트 갱신한다
	fetchEvent(3);	// top
	fetchItic(3);	
	fetchItic2(3);	
	
#else	

	//woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
	//sprintf(path, "%s%04d%02d.d", EVENT_LIST_FILE, pcntl->tod.tm_year, pcntl->tod.tm_mon);
	sprintf(path, "%s%04d.d", EVENT_LIST_FILE, pcntl->tod.tm_year);

	info.fileID = 0;      
	if (ffind (path, &info)) {
		return -1;
	}
	
	nlog = info.size/sizeof(EVENT_LOG);
	printf("[[Event List File(%s) Size = %d, #event=%d\n", path, info.size, nlog);
	fp = fopen(path, "rb"); 
	if (fp == NULL) {
		return -1;
	}
	
	pelist->re = 0;
	n = (nlog <= (N_EVENT_LIST)) ? nlog : (N_EVENT_LIST);
	pelist->count = n;
#if 1
	fseek(fp, sizeof(ITIC_LOG)*(nlog-n), SEEK_SET);	
	for (i=0; i<n; i++) {
		fread(&_itic, sizeof(ITIC_LOG), 1, fp);
		memcpy(&pitic->elog[i], &_itic, sizeof(ITIC_LOG));
		memcpy(&pelist->elog[i], &_itic, sizeof(EVENT_LOG));
		printf("%d -> %d, %d.%d\n", i, pelist->elog[i].type, pelist->elog[i].startTs, pelist->elog[i].msec);
	}
	reverseSortEventBuffer(pelist);
	reverseSortITICBuffer(pitic);

#else	
	fseek(fp, sizeof(EVENT_LOG)*(nlog-n), SEEK_SET);	
	for (i=0; i<n; i++) {
		fread(&pelist->elog[i], sizeof(EVENT_LOG), 1, fp);
		printf("%d -> %d, %d.%d\n", i, pelist->elog[i].type, pelist->elog[i].startTs, pelist->elog[i].msec);
	}
#endif	
	n = (nlog <= (N_ITIC_BUF)) ? nlog : (N_ITIC_BUF);
	pitic->count = n;
	printf("--> loadITICevent, size=%d, count=%d\n", n, pitic->count);

	fclose(fp);
#endif		
	return 0;
}


int storeAlarmLog(int ix, int status, float value, int doSel) {
	FILE *fp;
	char path[64];
	uint16_t	do_action;
	int		i;
	ALARM_U	alog;
		
	pcntl->alog.ts = sysTick1s;
	pcntl->alog.chan = palm->st[ix].chan;
	pcntl->alog.cond = palm->st[ix].cond;
	pcntl->alog.level = palm->st[ix].level;
	pcntl->alog.value = value;
	pcntl->alog.status = status;

	//woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
	sprintf(path, "%s%04d%02d.d", ALARM_LIST_FILE, pcntl->tod.tm_year, pcntl->tod.tm_mon);
	fp = fopen(path, "ab"); 
	if (fp != NULL) {
		fwrite(&pcntl->alog, sizeof(ALARM_LOG), 1, fp);
		fclose(fp);
	}
#if 1	// 2025-3-13, alarm Fifo
	printf("ALARM_FIFO_FILE:%s\n", ALARM_FIFO_FILE);
	fp = fopen(ALARM_FIFO_FILE, "r+b");
	if (fp == NULL) {
		// create header
		memset(&alog, 0, sizeof(alog));
		alog.head.magic = 0x1234abcd;
		alog.head.fr = 1;		
		alog.head.count = 1;
		alog.head.ts = sysTick1s;
		fp = fopen(ALARM_FIFO_FILE, "wb");
		fwrite(&alog, sizeof(alog), 1, fp);
		fwrite(&pcntl->alog, sizeof(ALARM_LOG), 1, fp);
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
		fwrite(&pcntl->alog, sizeof(ALARM_LOG), 1, fp);
		fclose(fp);
	}
	
	// 2025-3-20, Alarm FiFo에 추가한다
	memcpy(&pAlmFifo->alog[pAlmFifo->fr], &pcntl->alog, sizeof(pcntl->alog));	
	if (pAlmFifo->count < N_ALARM_FIFO) {
		pAlmFifo->count++;
		if (++pAlmFifo->fr >= N_ALARM_FIFO) pAlmFifo->fr = 0;
	}
	else {
		// Full 발생하면, fr, re 모두 이동한다
		if (++pAlmFifo->re >= N_ALARM_FIFO) pAlmFifo->re = 0;
		if (++pAlmFifo->fr >= N_ALARM_FIFO) pAlmFifo->fr = 0;
	}
	
	// 현재 알람 페이지 갱신
	fetchAlarm(0);
#else
	for(i=N_ALARM_LIST-1; i>0; --i) {
		palist->alog[i] = palist->alog[i-1];
	}
	memcpy(&palist->alog[0], &pcntl->alog, sizeof(ALARM_LOG));
	if (palist->count < (N_ALARM_LIST+1)) {
		palist->count++;
	}
#endif	
	
	printf("alarm log [Ts=%d, st=%d, value=%f, chan=%d, cond=%d, doPnt = %d]\n", 
		pcntl->alog.ts, pcntl->alog.status, pcntl->alog.value, pcntl->alog.chan, pcntl->alog.cond, doSel);	

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
void fetchAlarm(int cmd) {
	int i, n, ix;
	ALARM_FIFO *pFifo = pAlmFifo;
	ALARM_LIST *plist = palist;

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
		plist->re = (plist->count-1)/N_EVENT_LIST * N_EVENT_LIST;
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
void fetchEvent(int cmd) {
	int i, n, ix;
	EVENT_FIFO *pFifo = pEvtFifo;
	EVENT_LIST *plist = pelist;
	
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


void _fetchItic(int mode, int cmd) {
	// int i, n, ix;
	// EVENT_FIFO *pFifo = pEvtFifo;
	// ITIC_EVT_LIST *plist = (mode == 0) ? piticlist : piticlist2;
	
	// plist->count = pFifo->count;
	
	// // update
	// if (cmd == 0) {
	// }
	// // page down
	// else if (cmd == 1) {
	// 	if (plist->re + N_ITIC_LIST < pFifo->count) 
	// 		plist->re += N_ITIC_LIST;	
	// }
	// // page up
	// else if (cmd == 2) {
	// 	if (plist->re - N_ITIC_LIST >= 0) 
	// 		plist->re -= N_ITIC_LIST;
	// }
	// // top
	// else if (cmd == 3) {
	// 	plist->re = 0;
	// }
	
	// // src
	// ix = (pFifo->fr - plist->re) >= 0 ? (pFifo->fr - plist->re) : (pFifo->fr - plist->re) + N_EVENT_FIFO;
	// // dst
	// plist->fr = plist->re;
	// for (i=0; i<N_ITIC_LIST; i++) {
	// 	if (plist->fr < pFifo->count) {
	// 		if (--ix < 0) ix = N_EVENT_FIFO-1;
	// 		memcpy(&plist->elog[i], &pFifo->elog[ix], sizeof(ITIC_LOG));			
	// 		// dst
	// 		plist->fr++;
	// 	}
	// 	else {
	// 		// fill zero
	// 		memset(&plist->elog[i], 0, sizeof(ITIC_LOG));
	// 	}
	// }
}

void fetchItic(int cmd) {
//	_fetchItic(0, cmd);
}

void fetchItic2(int cmd) {
//	_fetchItic(1, cmd);
}
#endif

// 매 1초 마다 호출되어야 한다 
int alarmProc() {
	int i, chan, cond, change=0, result, almCount=0, doPoint=0;
	float level, *src;
	ALARM_DEF *paset = &pdb->alarm;
	
	// 시작 후 모든 값이 안정화 될때 까지 기다린다(5s)
	if (pcntl->online2++ < 5) return 0;
		
	for (i=0; i<32; i++) {		
		if (paset->set[i].chan == 0) continue;
//		if (paset->set[i].chan >= MAX_ALARM_CH) continue;
		
		cond = paset->set[i].cond & 1;
		chan = paset->set[i].chan;
//		doPoint = paset->set[i].do_action;
		src  = almTbl[chan].src;
		if (src == 0) {
			printf("@@@ Bad src, channel = %d\n", chan);
			continue;
		}

		if (palm->st[i].status == 0) {					
			//level = almTbl[chan].norm*paset->set[i].level/100.;				
			level = palm->st[i].level;
			if(cond !=2)
				result = (cond == 0) ? (*src < level) : (*src > level);
			else {
				result = (*src == level);
			}
				
			if (result) {
				if (pcntl->almTimer[i]++ >= paset->delay) {	
					// alarm 상태 
					palm->st[i].status = 1;	
					// alarm count 증가
					palm->st[i].count++;
					// alarm log 기록
					storeAlarmLog(i, palm->st[i].status, *src, doPoint);					
					change++;
				}
			}
			else {
				pcntl->almTimer[i] = 0;
			}
		}
		else {	
			// > (limit + deadband)
			if(cond !=2){
				if (cond == 0) {
#if 1	// 2025-3-18, 정격을 알수 없으므로 % of level로 처리한다				
					level = palm->st[i].level + palm->st[i].level*paset->set[i].dband/100.;
#else				
					level = palm->st[i].level + almTbl[chan].norm*paset->set[i].dband/100.;
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
					level = palm->st[i].level - almTbl[chan].norm*paset->set[i].dband/100.;
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
				if (pcntl->almTimer[i]++ >= paset->delay) {
					// normal 상태 
					palm->st[i].status = 0;	
					// alarm이 복귀될때 리스트에 추가된다 
					storeAlarmLog(i, 0, *src,doPoint);
					change++;
				}
			}
			else {
				pcntl->almTimer[i] = 0;
			}
		}	

		if (palm->st[i].status) {
			almCount++;
		}
	}
	
	palm->almCount = almCount;	
	meter[0].almCnt = almCount;
	return change;
}





//		uint16_t chan, cond, dband, action;
//		float level;

void buildAlarmSettings() {
	int i=0, chan;
	float pcent;
	
	ALARM_DEF *paset = &pdb->alarm;
	
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
			printf(">> almset(%d) -> chan(%d), cond(%d), level(%f)\n", i, palm->st[i].chan, palm->st[i].cond, palm->st[i].level);
		}
		else if(chan > MAX_ALARM_CH)
			printf(">> almset(%d) -> chan(%d) Error!!!!\n", i, palm->st[i].chan);
	}
#endif
}


void buildTrendSetting() {
	TREND_DEF *ptrd = pdb->trend;
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

void getTrendData(int gid,  uint16_t *chan) {
	int i;
	
	row[gid].ts = sysTick1s;
	row[gid].valid = 1;
	
	for (i=0; i<16; i++) {
		if (chan[i] == 0) {
			row[gid].pen[i] = 0;
		}
		else {
			row[gid].pen[i] = *almTbl[chan[i]].src;
		}
	}	
}

// 그룹-년-월로 구성된다. 즉 월마다 파일이 생성된다.
void getTrendFile(char *str, int g) {
	sprintf(str, "%s%d_%04d%02dV%d.d", TREND_FILE, g, pcntl->tod.tm_year, pcntl->tod.tm_mon, 0);
}

void getTrendBackupFile(char *str, int g) {
	sprintf(str, "trd%d_%04d%02dV%d_%02d.d", g, pcntl->tod.tm_year, pcntl->tod.tm_mon, 0, pcntl->tod.tm_mday);
}

// 2020-4-8, Trend Header와 현재 Trend 설정을 비교하여 다르면 기존파일 이름에 수정날짜 이름 추가하여 변경한다 
int appendTrendRcrd(int g) {
	FILE *fp;
#ifdef USE_CMSIS_RTOS2	
   fsFileInfo fi;	
#else
	FINFO fi;
#endif
	int j;
	char fn[64];
	
	getTrendFile(fn, g);
	
	fi.fileID = 0;      
	if (ffind (fn, &fi)) {
		printf("createTrendFile(%s) ...\n", fn);
		fp = fopen(fn, "wb");
		if (fp) {
			trdInf[g].ts = sysTick1s;;
			trdInf[g].type = 1;
			trdInf[g].version = pdb->trend[g].version;
			for (j=0; j<16; j++) {
				trdInf[g].channel[j] = pdb->trend[g].chan[j];
			}								
			fwrite(&trdInf[g], sizeof(trdInf[0]), 1, fp);	// 마지막에 새 record 기록한다 
			fwrite(&row[g], sizeof(row[0]), 1, fp);			
			fclose(fp);				
		}		
	}
	else {	
		printf("appendTrendFile(%s), g=%d, ts=%d, size = %d\n", fn, g, row[g].ts, fi.size);
		fp = fopen(fn, "ab");
		if (fp) {
			fwrite(&row[g], sizeof(TREND_INFO), 1, fp);			
			fclose(fp);				
		}
	}
}

// trend file header와 trend 설정을 비교한다 
void checkTrendHeader() {
	int i, j, err=0;
	char fn[64], fnew[64];
	FILE *fp;
	
	for (i=0; i<4; i++) {
		if (pdb->trend[i].active == 0) 
			continue;
		
		getTrendFile(fn, i);
		fp = fopen(fn, "rb");
		if (fp) {
			fread(&trdInf[i], sizeof(trdInf), 1, fp);			
			fclose(fp);	

			for (err=0, j=0; j<16; j++) {
				if (trdInf[i].channel[j] != pdb->trend[i].chan[j]) {
					printf("### Trend info changed, trdInf[%d][%d] = %d, %d\n", i, j, trdInf[i].channel[j], pdb->trend[i].chan[j]);
					err++;
				}
			}
			
			if (err) {
				getTrendBackupFile(fnew, i);	// 경로는 제거하고 파일이름만 넣는다 
				frename(fn, fnew);
				printf("### rename %s to %s\n", fn, fnew);
			}
		}
	}
}

void Trend_Task(void *arg)		
{
	int i, j, itv;	
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
		
		// trend data 생성
		for (i=0; i<4; i++) {			
			if (pdb->trend[i].active == 0) {
				continue;
			}
				
			itv = (pdb->trend[i].interval >= 8) ? 10 : trdTime[pdb->trend[i].interval];
			if ((pcntl->tod.tm_min % itv) == 0) {				
				getTrendData(i, pdb->trend[i].chan);
			}
		}
			
		// 저장 기간 최대 1년
		for (i=0; i<4; i++) {
			if (row[i].valid) {
				appendTrendRcrd(i);
				row[i].valid = 0;
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