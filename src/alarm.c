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

#define ALARM_FIFO_MAGIC  0x1234abcd

static void purgeAlarmFifoFile_nolock(int id)
{
	char path[64];

	if (id < 0 || id >= METER_CH_COUNT)
		return;
	getAlarmFifoFileName(id, path);
#ifdef USE_CMSIS_RTOS2
	(void)fdelete(path, NULL);
#else
	(void)fdelete(path);
#endif
	memset(&meter[id].alarmFifo, 0, sizeof(meter[id].alarmFifo));
	printf("---> purge invalid alarm fifo M%d: %s\n", id, path);
}

static void purgeAlarmFifoFile(int id)
{
	fsFileLock();
	purgeAlarmFifoFile_nolock(id);
	fsFileUnlock();
}

static int alarmFifoHeaderOk(const ALARM_U *alog, long fsize)
{
	long minsz;
	int n;

	if (alog == NULL || fsize < (long)sizeof(ALARM_U))
		return 0;
	if (alog->head.magic != ALARM_FIFO_MAGIC)
		return 0;
	n = alog->head.count;
	if (n < 0 || n > LOG_FIFO_SIZE)
		return 0;
	if (alog->head.fr < 0 || alog->head.fr > LOG_FIFO_SIZE)
		return 0;
	minsz = (long)sizeof(ALARM_U) + (long)n * (long)sizeof(ALARM_LOG);
	return (fsize >= minsz);
}

static void alarmFifoSanitizeRam(ALARM_FIFO *pFifo)
{
	if (pFifo == NULL)
		return;
	if (pFifo->count > N_ALARM_FIFO)
		pFifo->count = N_ALARM_FIFO;
	if (pFifo->fr >= N_ALARM_FIFO)
		pFifo->fr = 0;
	if (pFifo->re >= N_ALARM_FIFO)
		pFifo->re = 0;
}

void initAlarmTable(int id) {
	int ix=0;
	float temp, In;
	METERING *pm = &meter[id].meter;
	DEMAND *pdmd = &meter[id].dm;

	if (id < 0 || id >= METER_CH_COUNT)
		return;

	In = CT_INORM_A(id);	/* 정격전류 In(A) = CT1*inorm/100 */
	memset(almTbl[id], 0, sizeof(almTbl[id]));
	// [0..2]
	setAlarmChannel(id, ix++, "-", 0, NULL);
	setAlarmChannel(id, ix++, "Temp.", 100, &pm->Temp);
	setAlarmChannel(id, ix++, "Freq.", FREQ_HZ(pdb->freq), &pm->Freq);
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
	temp = In;
	setAlarmChannel(id, ix++, "I1", temp, &pm->I[0]);
	setAlarmChannel(id, ix++, "I2", temp, &pm->I[1]);
	setAlarmChannel(id, ix++, "I3", temp, &pm->I[2]);
	setAlarmChannel(id, ix++, "I~", temp, &pm->I[3]);
	setAlarmChannel(id, ix++, "Itotal", temp*3, &pm->I[4]);
	setAlarmChannel(id, ix++, "In", temp, &pm->In);
	// [19..22]
	temp = db.pt[id].vnorm*In;
	setAlarmChannel(id, ix++, "P1", temp, &pm->P[0]);
	setAlarmChannel(id, ix++, "P2", temp, &pm->P[1]);
	setAlarmChannel(id, ix++, "P3", temp, &pm->P[2]);
	setAlarmChannel(id, ix++, "Ptotal", temp*3, &pm->P[3]);
	// [23..26]
	temp = db.pt[id].vnorm*In;
	setAlarmChannel(id, ix++, "Q1", temp, &pm->Q[0]);
	setAlarmChannel(id, ix++, "Q2", temp, &pm->Q[1]);
	setAlarmChannel(id, ix++, "Q3", temp, &pm->Q[2]);
	setAlarmChannel(id, ix++, "Q4", temp*3, &pm->Q[3]);
	// [27..30]
	temp = db.pt[id].vnorm*In;
	setAlarmChannel(id, ix++, "D1", temp*3, &pm->D[0]);
	setAlarmChannel(id, ix++, "D2", temp*3, &pm->D[1]);
	setAlarmChannel(id, ix++, "D3", temp*3, &pm->D[2]);
	setAlarmChannel(id, ix++, "Dtotal", temp, &pm->D[3]);
	// [31..34]
	temp = db.pt[id].vnorm*In;
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
	temp = db.pt[id].vnorm*In;
	setAlarmChannel(id, ix++, "DD P+", temp, &pdmd->DD_P[0]);
	setAlarmChannel(id, ix++, "DD P-", temp, &pdmd->DD_P[1]);
	setAlarmChannel(id, ix++, "DD Q-L", temp, &pdmd->DD_Q[0]);
	setAlarmChannel(id, ix++, "DD Q-C", temp, &pdmd->DD_Q[1]);
	setAlarmChannel(id, ix++, "DD S", temp, &pdmd->DD_S);
	// [53..55]
	temp = In;
	setAlarmChannel(id, ix++, "DD I1", temp, &pdmd->DD_I[0]);
	setAlarmChannel(id, ix++, "DD I2", temp, &pdmd->DD_I[1]);
	setAlarmChannel(id, ix++, "DD I3", temp, &pdmd->DD_I[2]);
	// [56..60]
	temp = db.pt[id].vnorm*In;
	setAlarmChannel(id, ix++, "MD P+", temp, &pdmd->MD_P[0].value);
	setAlarmChannel(id, ix++, "MD P-", temp, &pdmd->MD_P[1].value);
	setAlarmChannel(id, ix++, "MD Q-L", temp, &pdmd->MD_Q[0].value);
	setAlarmChannel(id, ix++, "MD Q-C", temp, &pdmd->MD_Q[1].value);	
	setAlarmChannel(id, ix++, "MD S", temp, &pdmd->MD_S.value);
	// [61..63]
	temp = In;
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

	fsFileLock();
	fp = fopen(path, "wb");
	if (fp == NULL) {
		fsFileUnlock();
		return -1;
	}
	fwrite(palm, sizeof(ALARM_STATUS), 1, fp);
	fclose(fp);
	fsFileUnlock();
	return 0;
}

#define ALARM_DEF_MAGIC 0x414c4d44u	/* 'ALMD' */

/* 알람설정(almSet, ALARM_DEF) 3채널을 파일로 영속화 (save settings 시 호출) */
int storeAlarmDef(void) {
	FILE *fp;
	int id;
	uint32_t magic = ALARM_DEF_MAGIC;

	fsFileLock();
	fp = fopen(ALARM_DEF_FILE, "wb");
	if (fp == NULL) {
		fsFileUnlock();
		return -1;
	}
	fwrite(&magic, sizeof(magic), 1, fp);
	for (id = 0; id < METER_CH_COUNT; id++)
		fwrite(&meter[id].almSet, sizeof(ALARM_DEF), 1, fp);
	fclose(fp);
	fsFileUnlock();
	printf("[[Store AlarmDef(%s)]]\n", ALARM_DEF_FILE);
	return 0;
}

/* 부팅 시 알람설정 로드 — 파일 있으면 buildAlarmSettings 기본값을 덮어씀 */
int loadAlarmDef(void) {
	FILE *fp;
	int id;
	uint32_t magic = 0;

	fp = fopen(ALARM_DEF_FILE, "rb");
	if (fp == NULL)
		return -1;
	if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != ALARM_DEF_MAGIC) {
		fclose(fp);
		printf("[[AlarmDef invalid, keep defaults]]\n");
		return -1;
	}
	for (id = 0; id < METER_CH_COUNT; id++) {
		ALARM_DEF tmp;
		if (fread(&tmp, sizeof(ALARM_DEF), 1, fp) != 1) {
			fclose(fp);
			printf("[[AlarmDef short, keep defaults]]\n");
			return -1;
		}
		memcpy(&meter[id].almSet, &tmp, sizeof(ALARM_DEF));
	}
	fclose(fp);
	printf("[[Load AlarmDef(%s)]]\n", ALARM_DEF_FILE);
	return 0;
}

int deleteAlarmLog(int id) {
	char path[64];
	int res;
	ALARM_FIFO *pFifo = &meter[id].alarmFifo;

	getAlarmFifoFileName(id, path);
	fsFileLock();
#ifdef USE_CMSIS_RTOS2
   res = fdelete(path, NULL);
#else
	res = fdelete(path);
#endif
	fsFileUnlock();
	
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
	fsFileLock();
	memset(pFifo, 0, sizeof(*pFifo));
	fp = fopen(path, "rb");
	if (fp != NULL) {
		long fsize;

		if (fseek(fp, 0, SEEK_END) == 0) {
			fsize = ftell(fp);
			rewind(fp);
		} else {
			fsize = -1;
		}
		if (fread(&alog, sizeof(alog), 1, fp) != 1 ||
		    !alarmFifoHeaderOk(&alog, fsize)) {
			fclose(fp);
			purgeAlarmFifoFile_nolock(id);
		} else {
			int n = alog.head.count;

			for (i = 0; i < n; i++) {
				if (fread(&pFifo->alog[i], sizeof(ALARM_LOG), 1, fp) != 1)
					break;
			}
			if (i != n) {
				fclose(fp);
				purgeAlarmFifoFile_nolock(id);
			} else {
				pFifo->count = (uint16_t)n;
				pFifo->fr = (uint16_t)n;
				fclose(fp);
			}
		}
	}
	fsFileUnlock();

	alarmFifoSanitizeRam(pFifo);
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

/* RL-FlashFS: r+b 갱신 시 FAT 손상 — RAM FIFO 스냅샷을 wb로 전체 재기록 (Quality.c qw와 동일) */
static int alarmFifoFileAppend(int id, int cap, const ALARM_LOG *plog)
{
	FILE *fp;
	char fifoPath[64];
	ALARM_U hdr;
	const ALARM_FIFO *pFifo;
	int i, n;

	(void)cap;
	(void)plog;

	if (id < 0 || id >= METER_CH_COUNT)
		return -1;

	pFifo = &meter[id].alarmFifo;
	n = pFifo->count;
	if (n > N_ALARM_FIFO)
		n = N_ALARM_FIFO;

	getAlarmFifoFileName(id, fifoPath);
	fp = fopen(fifoPath, "wb");
	if (fp == NULL)
		return -1;

	memset(&hdr, 0, sizeof(hdr));
	hdr.head.magic = ALARM_FIFO_MAGIC;
	hdr.head.fr = pFifo->fr;
	hdr.head.count = n;
	hdr.head.ts = sysTick1s;
	fwrite(&hdr, sizeof(hdr), 1, fp);
	for (i = 0; i < n; i++)
		fwrite(&pFifo->alog[i], sizeof(ALARM_LOG), 1, fp);
	fclose(fp);
	return 0;
}

int alarmFsDispatch(const FS_MSG *pmsg)
{
	const ALARM_LOG *plog;
	FILE *fp;
	int id, cap;

	if (pmsg == NULL || pmsg->size < (int)sizeof(ALARM_LOG))
		return -1;
	plog = (const ALARM_LOG *)(pmsg->useData ? pmsg->data : pmsg->pbuf);
	if (plog == NULL)
		return -1;

	if (strcmp(pmsg->mode, "al") == 0) {
		fp = fopen(pmsg->fname, "ab");
		if (fp == NULL)
			return -1;
		fwrite(plog, sizeof(ALARM_LOG), 1, fp);
		fclose(fp);
		return 0;
	}
	if (strcmp(pmsg->mode, "af") == 0) {
		id = pmsg->argv & 0xff;
		cap = (pmsg->argv >> 8) & 0xffff;
		return alarmFifoFileAppend(id, cap, plog);
	}
	return -1;
}


int storeAlarmLog(int id, int ix, int status, float value, int doSel) {
	FS_MSG fsmsg;
	CNTL_DATA *pcntlId = &meter[id].cntl;
	ALARM_STATUS *palm = &meter[id].alarm;
	ALARM_FIFO *pFifo = &meter[id].alarmFifo;
		
	pcntlId->alog.ts = sysTick1s;
	pcntlId->alog.chan = palm->st[ix].chan;
	pcntlId->alog.cond = palm->st[ix].cond;
	pcntlId->alog.level = palm->st[ix].level;
	pcntlId->alog.value = value;
	pcntlId->alog.status = status;

	memset(&fsmsg, 0, sizeof(fsmsg));
	sprintf(fsmsg.fname, "%s%04d%02d.d", ALARM_LIST_FILE,
		pcntlId->tod.tm_year, pcntlId->tod.tm_mon);
	strcpy(fsmsg.mode, "al");
	fsmsg.pbuf = &pcntlId->alog;
	fsmsg.size = sizeof(ALARM_LOG);
	putFsQ(&fsmsg);

	getAlarmFifoFileName(id, fsmsg.fname);
	strcpy(fsmsg.mode, "af");
	fsmsg.argv = (LOG_FIFO_SIZE << 8) | (id & 0xff);
	fsmsg.pbuf = &pcntlId->alog;
	fsmsg.size = sizeof(ALARM_LOG);
	putFsQ(&fsmsg);

	memcpy(&pFifo->alog[pFifo->fr], &pcntlId->alog, sizeof(pcntlId->alog));
	if (pFifo->count < N_ALARM_FIFO) {
		pFifo->count++;
		if (++pFifo->fr >= N_ALARM_FIFO)
			pFifo->fr = 0;
	}
	else {
		if (++pFifo->re >= N_ALARM_FIFO)
			pFifo->re = 0;
		if (++pFifo->fr >= N_ALARM_FIFO)
			pFifo->fr = 0;
	}

	printf("alarm log(M%d) [Ts=%d, st=%d, value=%f, chan=%d, cond=%d, doPnt = %d]\n", id,
		pcntlId->alog.ts, pcntlId->alog.status, pcntlId->alog.value,
		pcntlId->alog.chan, pcntlId->alog.cond, doSel);

	assertAlarmOutput(doSel, status);
	return 0;
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

	alarmFifoSanitizeRam(pFifo);
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
		if (plist->count > 0)
			plist->re = (plist->count - 1) / N_ALARM_LIST * N_ALARM_LIST;
		else
			plist->re = 0;
	}
	
	// src
	ix = (pFifo->fr - plist->re) >= 0 ? (pFifo->fr - plist->re) : (pFifo->fr - plist->re) + N_ALARM_FIFO;
	// dst
	plist->fr = plist->re;
	for (i=0; i<N_ALARM_LIST; i++) {
		if (plist->fr < pFifo->count) {
			if (--ix < 0) ix = N_ALARM_FIFO - 1;
			if (ix < 0 || ix >= N_ALARM_FIFO)
				memset(&plist->alog[i], 0, sizeof(ALARM_LOG));
			else
				memcpy(&plist->alog[i], &pFifo->alog[ix], sizeof(ALARM_LOG));
			plist->fr++;
		}
		else {
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
	(void)mode;
	/* itic·itic2 모두 전압 ITIC 이벤트(sag/swell/intr/RVC). itic2는 itic의 2번째 독립 창
	 * — 외부서버(load itic)와 웹(load itic2)이 각자 페이지 커서를 갖도록 분리 */
	return type >= E_SAG && type <= E_RVC;
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
	
	/* 부팅 5s + RMS 초기화 + ZX(online) 없으면 알람·FIFO 기록 안 함 (무전압/미배선 보호) */
	if (pcntlId->online2++ < 5)
		return 0;
	if (!pcntlId->rmsInit)
		return 0;
	if (!pcntlId->online || pcntlId->zxMonCnt == 0)
		return 0;
	{
		float f = meter[id].meter.Freq;
		if (f < 47.0f || f > 63.0f)
			return 0;
	}

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
	if (change > 0)
		fetchAlarm(id, 0);
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

static int parseTrendMonthKey(const char *name) {
	int i;

	for (i = 0; name[i] != 0; i++) {
		if (name[i] == '_' && name[i + 1] >= '0' && name[i + 1] <= '9') {
			int y, m;

			y = (name[i + 1] - '0') * 1000 + (name[i + 2] - '0') * 100 +
				(name[i + 3] - '0') * 10 + (name[i + 4] - '0');
			m = (name[i + 5] - '0') * 10 + (name[i + 6] - '0');
			return y * 100 + m;
		}
	}
	return 999999;
}

static int getTrendMonthKeys(int *currentKey, int *lastMonthKey) {
	struct tm ltm;

	memcpy(&ltm, &pcntl->tod, sizeof(ltm));
	*currentKey = ltm.tm_year * 100 + ltm.tm_mon;
	ltm.tm_mon--;
	if (ltm.tm_mon < 1) {
		ltm.tm_mon = 12;
		ltm.tm_year--;
	}
	*lastMonthKey = ltm.tm_year * 100 + ltm.tm_mon;
	return 0;
}

static int findOldestTrendLog(char *oldestName, uint32_t *oldestSize, uint32_t *totalSize) {
#ifdef USE_CMSIS_RTOS2
	fsFileInfo info;
#else
	FINFO info;
#endif
	char mask[] = CONCAT(LOG_TREND_DIR, "\\trd*.d");
	int currentKey, lastMonthKey;
	int found = 0, oldestKey = 999999;

	getTrendMonthKeys(&currentKey, &lastMonthKey);

	*totalSize = 0;
	oldestName[0] = 0;
	*oldestSize = 0;

	info.fileID = 0;
	while (ffind(mask, &info) == 0) {
		const char *name = (const char *)info.name;
		int key = parseTrendMonthKey(name);

		*totalSize += info.size;
		/* 금월·전월 trd*는 보호 */
		if (key >= lastMonthKey) {
			continue;
		}

		if (!found || key < oldestKey) {
			oldestKey = key;
			strcpy(oldestName, name);
			*oldestSize = info.size;
			found = 1;
		}
	}

	return found;
}

static void trimTrendLogBudget(void) {
	char oldestName[64], path[96];
	uint32_t oldestSize = 0, totalSize = 0;
	int res;

	while (findOldestTrendLog(oldestName, &oldestSize, &totalSize)) {
		if (totalSize <= FLASH_LOG_BUDGET_TREND) {
			break;
		}

		sprintf(path, "%s\\%s", LOG_TREND_DIR, oldestName);
#ifdef USE_CMSIS_RTOS2
		res = fdelete(path, NULL);
#else
		res = fdelete(path);
#endif
		printf("trimTrendLogBudget: delete %s (size=%u, total=%u, res=%d)\n",
			path, oldestSize, totalSize, res);

		if (res != 0) {
			break;
		}
	}
}

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

	fsFileLock();
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
			if (fwrite(&trdInf[mid][g], sizeof(trdInf[0][0]), 1, fp) == 1 &&
				fwrite(&row[mid][g], sizeof(row[0][0]), 1, fp) == 1) {
				trimTrendLogBudget();
			}
			fclose(fp);				
		}		
	}
	else {	
		printf("appendTrendFile(%s), m=%d, g=%d, ts=%d, size = %d\n", fn, mid, g, row[mid][g].ts, fi.size);
		fp = fopen(fn, "ab");
		if (fp) {
			if (fwrite(&row[mid][g], sizeof(TREND_RECORD), 1, fp) == 1) {
				trimTrendLogBudget();
			}
			fclose(fp);				
		}
	}
	fsFileUnlock();
	return 0;
}

// trend file header와 trend 설정을 비교한다 
void checkTrendHeader() {
	int mid, i, j, err=0;
	char fn[64], fnew[64];
	FILE *fp;

	fsFileLock();
	trimTrendLogBudget();

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
	fsFileUnlock();
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