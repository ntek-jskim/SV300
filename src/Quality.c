#ifdef __FREERTOS
   #include "RTL.h"  // for File IO
   #include "os_port.h"
   #include "FreeRTOS.h"
   #include "task.h"
#endif
#include "board.h"
#include "meter.h"
#include "ade9000.h"
#include "math.h"
#include "time.h"
#include "string.h"

void getQualStartDate(char *str);
void getQualLastStartDate(char *str);

// 2nd ~ 25th, 10000 -> 100%
float harmLimit[] = {	
	2,   5,   1,   6,   0.5, 5,   0.5, 1.5, 0.5,	// 2 ~ 10
	3.5, 0.5, 3,   0.5, 0.5, 0.5, 2,   0.5, 1.5, 0.5,	// 11 ~ 20
	0.5, 0.5, 1.5, 0.5, 1.5	// 21 ~ 25
};

// 10초에 하나 증가
extern uint32_t sysTick1s, sysTick10s, sysTick10m;
extern int readQualWeekData(char *path, QualWeek *pqw);

#define QW_TAIL_MAX	4096

static uint8_t qw_tail_buf[QW_TAIL_MAX];

static void pqFsLock(void) {
	fsFileLock();
}

static void pqFsUnlock(void) {
	fsFileUnlock();
}

static int qualWeekReadTail(const char *path, uint32_t *ptailSize) {
#ifdef USE_CMSIS_RTOS2
	fsFileInfo fi;
#else
	FINFO fi;
#endif
	FILE *fp;
	uint32_t tailSize, n, total = 0;

	*ptailSize = 0;
	fi.fileID = 0;
	if (ffind(path, &fi) != 0 || fi.size <= sizeof(QualWeek)) {
		return 0;
	}

	tailSize = fi.size - (uint32_t)sizeof(QualWeek);
	if (tailSize > sizeof(qw_tail_buf)) {
		printf("updateQualWeekData, tail too large (%u)\n", tailSize);
		return -1;
	}

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return -1;
	}
	fseek(fp, (long)sizeof(QualWeek), SEEK_SET);
	while (total < tailSize) {
		n = tailSize - total;
		if (n > 256) {
			n = 256;
		}
		if (fread(qw_tail_buf + total, 1, n, fp) != n) {
			fclose(fp);
			return -1;
		}
		total += n;
	}
	fclose(fp);
	*ptailSize = tailSize;
	return 0;
}

static int parseDateKey8(const char *name) {
	const char *p;
	int i, key;

	if (name == NULL)
		return 99999999;

	/* 파일명: ql1_20260628_m1.d / qw1_20260628.d → '_' 뒤 YYYYMMDD */
	for (p = name; *p != 0; p++) {
		if (*p == '_' && p[1] >= '0' && p[1] <= '9') {
			key = 0;
			for (i = 0; i < 8 && p[1 + i] >= '0' && p[1 + i] <= '9'; i++) {
				key = key * 10 + (p[1 + i] - '0');
			}
			if (i == 8)
				return key;
		}
	}

	/* dstr: "20260628" */
	key = 0;
	for (i = 0; name[i] != 0 && i < 8; i++) {
		if (name[i] < '0' || name[i] > '9')
			return 99999999;
		key = key * 10 + (name[i] - '0');
	}
	if (i == 8)
		return key;
	return 99999999;
}

static int findOldestQualLog(char *oldestName, uint32_t *oldestSize, uint32_t *totalSize) {
	char dstr[16];
	int currentKey;
#ifdef USE_CMSIS_RTOS2
	fsFileInfo info;
#else
	FINFO info;
#endif
	char maskQl[] = CONCAT(LOG_PQ_DIR, "\\ql*.d");
	char maskQw[] = CONCAT(LOG_PQ_DIR, "\\qw*.d");
	int found = 0, oldestKey = 99999999;

	getQualStartDate(dstr);
	currentKey = parseDateKey8(dstr);

	*totalSize = 0;
	oldestName[0] = 0;
	*oldestSize = 0;

	info.fileID = 0;
	while (ffind(maskQl, &info) == 0) {
		const char *name = (const char *)info.name;
		int key = parseDateKey8(name);

		*totalSize += info.size;
		// PQ 통합 예산에서 금주(현재 주 시작일) ql*는 보호
		if (key >= currentKey) {
			continue;
		}

		if (!found || key < oldestKey) {
			oldestKey = key;
			strcpy(oldestName, name);
			*oldestSize = info.size;
			found = 1;
		}
	}

	// qw* — 금주만 보호 (전주 qw는 용량 부족 시 trim 가능)
	info.fileID = 0;
	while (ffind(maskQw, &info) == 0) {
		const char *name = (const char *)info.name;
		int key = parseDateKey8(name);

		*totalSize += info.size;
		if (key >= currentKey) {
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

static void trimQualLogBudget_nolock(void) {
	char oldestName[64], path[96];
	uint32_t oldestSize = 0, totalSize = 0;
	int res;

	while (findOldestQualLog(oldestName, &oldestSize, &totalSize)) {
		if (totalSize <= FLASH_LOG_BUDGET_PQ) {
			break;
		}

		sprintf(path, "%s\\%s", LOG_PQ_DIR, oldestName);
#ifdef USE_CMSIS_RTOS2
		res = fdelete(path, NULL);
#else
		res = fdelete(path);
#endif
		printf("trimQualLogBudget: delete %s (size=%u, total=%u, res=%d)\n",
			path, oldestSize, totalSize, res);

		if (res != 0) {
			break;
		}
	}
	if (totalSize > FLASH_LOG_BUDGET_PQ) {
		printf("trimQualLogBudget: PQ flash still over budget (%u > %u)\n",
			totalSize, (unsigned)FLASH_LOG_BUDGET_PQ);
	}
}

static void ensureLogPqDir(void) {
	FILE *fp;

	fp = fopen(CONCAT(LOG_PQ_DIR, TEMP_FILE), "ab");
	if (fp != NULL) {
		fclose(fp);
	}
}

static FILE *pqFopenLocked(const char *path, const char *mode) {
	FILE *fp;

	ensureLogPqDir();
	fp = fopen(path, mode);
	if (fp == NULL) {
		trimQualLogBudget_nolock();
		fp = fopen(path, mode);
	}
	return fp;
}

static void trimQualLogBudget(void) {
	pqFsLock();
	trimQualLogBudget_nolock();
	pqFsUnlock();
}

/* [나이 로테이션] ql/qw 중 날짜키 < cutoffKey 인 가장 오래된 1개 삭제(호출당 최대 1개).
 *  cutoffKey==0(RTC 미설정)이면 삭제 없음. findOldestQualLog가 금주(현재 주) 파일은 보호. */
static void trimQualLogAge_nolock(uint32_t cutoffKey) {
	char oldestName[64], path[96];
	uint32_t oldestSize = 0, totalSize = 0;
	int res;

	if (cutoffKey == 0)
		return;
	if (!findOldestQualLog(oldestName, &oldestSize, &totalSize))
		return;
	if ((uint32_t)parseDateKey8(oldestName) >= cutoffKey)
		return;		/* 최古도 보존기간 내 → 삭제 없음 */

	sprintf(path, "%s\\%s", LOG_PQ_DIR, oldestName);
#ifdef USE_CMSIS_RTOS2
	res = fdelete(path, NULL);
#else
	res = fdelete(path);
#endif
	printf("trimQualLogAge: delete %s (cutoff=%u, res=%d)\n", path, cutoffKey, res);
}

static void trimQualLogAge(uint32_t cutoffKey) {
	pqFsLock();
	trimQualLogAge_nolock(cutoffKey);
	pqFsUnlock();
}

void uLocalTime(const uint32_t *utc, struct tm *ptm) {
	if (*utc == 0) {
		memset(ptm, 0, sizeof(*ptm));
	}
	else {
		localtime_r(utc, ptm);
		ptm->tm_year += 1900;
		ptm->tm_mon += 1;
	}
}

//// 요일을 반환한다 
//int dayOfWeek() {
//	struct tm lt;
//	
//	localtime_r(&sysTick1s, &lt);	
//	return lt.tm_wday;
//}

// 오늘이 올해 몇번째 주인가? (tm_ydata, tm_wday)
int getYear_n_WoY(int doY, int doW) {
	time_t t;
	int doW1, temp, woY;
		
	// 올해 1-1 요일 구한다
	temp = (doY-1)%7;
	doW1 = (temp > doW) ? 7+(temp-doW) : temp-doW;
			
	woY = ((doY + 6) / 7);
	if (doW < doW1) woY++;
	return woY;
}

//int getPQBinIndex() {
//	int idx;
//	struct tm lt;
//	localtime_r((time_t *)&sysTick1s, &lt);

//	//idx = lt.tm_wday*144 + lt.tm_hour*6 + lt.tm_min/10;
//	idx = lt.tm_wday*144 + lt.tm_hour*6 + lt.tm_min;
//	
//	printf("BIN Index=%d\n", idx);
//	return idx;
//}

// Qual_Test :현재 시간 기준 오늘 날짜 구한다
// Normal    :현재 시간 기준 금주의 시작일 구한다 
void getQualStartDate(char *str) {
	struct tm ltm;
	uint32_t t;
	
#ifdef	_QUAL_TEST	
	uLocalTime(&sysTick1s, &ltm);		
	sprintf(str, "%04d%02d%02d", ltm.tm_year, ltm.tm_mon, ltm.tm_mday);			
#else	
	uLocalTime(&sysTick1s, &ltm);			
	t = sysTick1s-(ltm.tm_wday*24*3600);
	uLocalTime(&t, &ltm);			
	sprintf(str, "%04d%02d%02d", ltm.tm_year, ltm.tm_mon, ltm.tm_mday);			
#endif	
}


// 요일 변경 될때 사용한다. 금주가 전주로 바뀌기 때문에 마직막 데이터 갱신위해 필요하다
// Qual_Test :현재 시간 기준 어제 날짜 구한다
// Normal    :현재 시간 기준 전주의 시작일 구한다 
void getQualLastStartDate(char *str) {
	struct tm ltm;
	uint32_t t;
		
#ifdef	_QUAL_TEST	
	t = sysTick1s-(24*3600);	// 현재에서 하루 뺀다
	uLocalTime(&t, &ltm);		
	sprintf(str, "%04d%02d%02d", ltm.tm_year, ltm.tm_mon, ltm.tm_mday);			
#else	
	uLocalTime(&sysTick1s, &ltm);			
	t = sysTick1s-(ltm.tm_wday+7)*24*3600;
	uLocalTime(&t, &ltm);			
	sprintf(str, "%04d%02d%02d", ltm.tm_year, ltm.tm_mon, ltm.tm_mday);			
#endif	
}


// 현재 시간 기준으로 금주의 시작일을 구한다 또는 Test용으로 오늘 날짜를 구한다 
void getQualLogFN(char *path, int id) {
	char dstr[16];
	
	getQualStartDate(dstr);
	sprintf(path, "%s%s_m%d.d", QL_FILE, dstr, id);		/* 전 채널 통일: M0=_m0 */
}

// 현재 시간 기준으로 금주의 시작일을 구한다 또는 Test용으로 오늘 날짜를 구한다 
void getQualWeekFN(char *path, int id) {
	char dstr[16];
		
	getQualStartDate(dstr);
	sprintf(path, "%s%s_m%d.d", QW_FILE, dstr, id);		/* 전 채널 통일: M0=_m0 */
}

// 현재 시간 기준으로 전주의 시작일을 구한다 
void getQualLastWeekFN(char *path, int id) {
	char dstr[16];
	
	getQualLastStartDate(dstr);
	sprintf(path, "%s%s_m%d.d", QW_FILE, dstr, id);		/* 전 채널 통일: M0=_m0 */
}


//// 현재 시간 기준으로 전주의 시작일을 구한다 
//void getQualLastLogFN(char *path) {
//	char dstr[16];
//	
//	getQualLastStartDate(dstr);
//	sprintf(path, "%s\\QL0_%s.d", LOG_PQ_DIR, dstr);			
//}

int appendQualLog(char *fn, void *bf, int size) {
	FILE *fp;
#ifdef USE_CMSIS_RTOS2
   fsFileInfo fi;	
#else
	FINFO fi;
#endif

	/* [진단로그 OFF] printf("[WR] appendQualLog %s sz=%d\n", fn, size); */	/* 진단: 0x100000(FAT) 처닝원 추적 */
	pqFsLock();
	fi.fileID = 0;
	if (ffind (fn, &fi)) {
		printf("createQualLog(%s), ...\n", fn);
		fp = pqFopenLocked(fn, "wb");
		if (fp) {
			fwrite(bf, size, 1, fp);	// 마지막에 새 record 기록한다 
			fclose(fp);				
		}
		else {
			printf("~~~ appendQualLog, can't open log file(%s)\n", fn);
		}
	}
	else {	
		printf("appendQualLog(%s), size = %d\n", fn, fi.size);
		fp = pqFopenLocked(fn, "ab");
		if (fp) {
			fseek(fp, -1, SEEK_END);
			fwrite(bf, size, 1, fp);	// 마지막에 새 record 기록한다 
			fclose(fp);				
		} 
		else {
			printf("~~~ appendQualLog, can't open log file(%s)\n", fn);
		}
	}
	pqFsUnlock();
	return 0;
}


// 10m 데이터를 파일에 기록한다. 10m을 다 채우지 못하면 기록하지 않는다 
int writeQual10mData(int id, char *path, QualData10m *pq10m) {	
	struct tm lt;
	
	appendQualLog(path, &pq10m->avg, sizeof(pq10m->avg));
	if (id == 0) {
		trimQualLogBudget();
		trimQualLogAge(logCutoffKeyDays(QUAL_KEEP_DAYS));	/* 90일 경과 ql/qw 1개 정리 */
	}
	uLocalTime(&sysTick1s, &lt);
	printf("{{Qual10m(%s) TS[%d-%d-%d %d:%d:%d], C[%d] U[%f,%f,%f]}\n", 
		path, lt.tm_year, lt.tm_mon, lt.tm_mday, 
		lt.tm_hour, lt.tm_min, lt.tm_sec,
		pq10m->count10m,	pq10m->avg.U[0], pq10m->avg.U[1],	pq10m->avg.U[2]);
		
	return 0;
}

//// 10m 데이터를 파일에 기록한다. 10m을 다 채우지 못하면 기록하지 않는다 
//int writeQual10mLastData(QualData10m *pq10m) {	
//	int		result, woY;
//	//FINFO info;
//	FILE *fp;
//	struct tm lt;
//	char path[64];
//	
//	//woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
//	//sprintf(pcntl->path, "%s%04d%02d.d", QL_FILE, pcntl->tod.tm_year, woY);
//	//sprintf(pcntl->path, "%s", QL_TEMP_FILE);
//	
//	getQualLastLogFN(path);
//	appendQualLog(path, &pq10m->avg, sizeof(pq10m->avg));
////	fp = fopen(pcntl->path, "ab");
////	fwrite(&pq10m->avg, sizeof(pq10m->avg), 1, fp);	// 마지막에 새 record 기록한다 
////	fclose(fp);
//	
//	uLocalTime(&sysTick1s, &lt);
//	printf("{{Qual10m(%s) TS[%d-%d-%d %d:%d:%d], C[%d] U[%f,%f,%f]}\n", 
//		path, 
//		lt.tm_year, lt.tm_mon, lt.tm_mday, 
//		lt.tm_hour, lt.tm_min, lt.tm_sec, 
//		pq10m->count10m,	pq10m->avg.U[0], pq10m->avg.U[1],	pq10m->avg.U[2]);
//		
//	return 0;
//}
 

//int writeQualLastWeekData(QualWeek *pqw) {
//	FILE *fp;
//	char path[64];
//	
//	getQualLastWeekFN(path);
//	fp = fopen(path, "wb");
//	if (fp == NULL) {
//		printf("Can't open file(%s)\n", path);
//		return -1;
//	}
//	
//	fwrite(pqw, sizeof(QualWeek), 1, fp);
//	fclose(fp);
//	
//	return 0;	
//}

int createQualWeekData(char *path, QualWeek *pqw) {
	FILE *fp;
	int res = 0;
	
	printf("writeQualWeekData (%s) ...\n", path);
	pqFsLock();
	fp = pqFopenLocked(path, "wb");
	if (fp == NULL) {
		printf("Can't open file(%s)\n", path);
		res = -1;
	} else {
		if (fwrite(pqw, sizeof(QualWeek), 1, fp) != 1) {
			res = -1;
		}
		fclose(fp);
	}
	pqFsUnlock();
	return res;
}

int updateQualWeekData(int id, char *path, QualWeek *pqw) {
	FILE *fp;
	EVENT_Q *pevQ = &meter[id].eventQ;
	QualWeek weekSnap;
	uint32_t tailSize = 0;
	int evCount = pevQ->count;
	int res = 0;

	weekSnap = *pqw;
	if (evCount > 0) {
		weekSnap.evtCount += evCount;
	}
	printf("updateQualWeekData[m%d] (%s), event count=%d ...\n", id, path, evCount);

	pqFsLock();
	if (qualWeekReadTail(path, &tailSize) != 0) {
		printf("updateQualWeekData, tail read failed(%s)\n", path);
		pqFsUnlock();
		return -1;
	}

	/* RL-FlashFS: r+/r+b 갱신 실패 회피 — wb 재기록 + 기존 이벤트 tail 보존 */
	fp = pqFopenLocked(path, "wb");
	if (fp == NULL) {
		printf("updateQualWeekData, Can't open file(%s)\n", path);
		pqFsUnlock();
		return -1;
	}

	if (fwrite(&weekSnap, sizeof(QualWeek), 1, fp) != 1) {
		printf("updateQualWeekData, fwrite failed(%s)\n", path);
		fclose(fp);
		pqFsUnlock();
		return -1;
	}
	if (tailSize > 0 && fwrite(qw_tail_buf, 1, tailSize, fp) != tailSize) {
		printf("updateQualWeekData, tail fwrite failed(%s)\n", path);
		fclose(fp);
		pqFsUnlock();
		return -1;
	}
	fclose(fp);

	if (evCount > 0) {
		fp = pqFopenLocked(path, "ab");
		if (fp == NULL) {
			printf("updateQualWeekData, Can't append events(%s)\n", path);
			res = -1;
		} else if (fwrite(pevQ->eq, sizeof(EVENT_LOG), evCount, fp) != (size_t)evCount) {
			printf("updateQualWeekData, event fwrite failed(%s)\n", path);
			fclose(fp);
			res = -1;
		} else {
			fclose(fp);
		}
	}

	if (res == 0) {
		*pqw = weekSnap;
		pevQ->count = 0;
	}
	pqFsUnlock();
	return res;
}

//int readQualLastWeekData(QualWeek *pqw) {
//	FILE *fp;
//	int i, woY;
//	char path[64];
//	
//	getQualLastWeekFN(path);
//	printf("readQualLastWeekData (%s) ...\n", path);
//	fp = fopen(path, "rb");
//	if (fp == NULL) {
//		printf("Can't open file(%s) ...\n", path);		
//		return -1;
//	}		
//	fread(pqw, sizeof(QualWeek), 1, fp);
//	fclose(fp);
//	
//	return 0;
//}

int readQualWeekData(char *path, QualWeek *pqw) {
	FILE *fp;
	int res = 0;

	printf("readQualWeekData (%s) ...\n", path);
	pqFsLock();
	fp = fopen(path, "rb");
	if (fp == NULL) {
		printf("readQualWeekData, no file(%s)\n", path);
		res = -1;
	} else {
		if (fread(pqw, sizeof(QualWeek), 1, fp) != 1)
			res = -1;
		fclose(fp);
	}
	pqFsUnlock();
	return res;
}



uint32_t getQualWeekEndTs(CNTL_DATA *pcntl) {
	struct tm ltm;
	uint32_t utc;
	
#ifdef _QUAL_TEST
	memcpy(&ltm, &pcntl->tod, sizeof(struct tm));
	ltm.tm_year -= 1900;	// since 1900
	ltm.tm_mon -= 1;			// 0 ~ 11
	ltm.tm_hour = ltm.tm_min = ltm.tm_sec = 0;
	utc = mktime(&ltm);
	utc += 86400;	// 다음날 
	uLocalTime(&utc, &ltm);
#else	
	memcpy(&ltm, &pcntl->tod, sizeof(struct tm));
	ltm.tm_year -= 1900;	// since 1900
	ltm.tm_mon -= 1;			// 0 ~ 11
	ltm.tm_hour = ltm.tm_min = ltm.tm_sec = 0;
	
	utc = mktime(&ltm);
	utc += (6 - pcntl->tod.tm_wday + 1) * 86400;	// 다음 주기 시작일 계산 	
	uLocalTime(&utc, &ltm);
#endif	

	printf("getQualWeekEndTs [%d-%d-%d, %d:%d:%d]\n", ltm.tm_year, ltm.tm_mon, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
	
	return utc;
}


void QualVarWeekUpdate(QualVariation *pqvar, int errcnt, int count) {
	pqvar->err += errcnt;
	pqvar->var = (float)pqvar->err/count;		
}

int QualVarUpdate(QualVariation *pqvar, float val, float llimit, float hlimit, int count) {
	int i;
	
	if (val < llimit || val > hlimit) {
		pqvar->err++;
		pqvar->var = (float)pqvar->err/count;
		return 1;
	}		
	
	return 0;
}


int getVarPercent(QualVariation *pqv) {
	return (1-pqv->var)*10000;
}


// 0: OK, 1: failed
int QualVarCompliance1(QualVariation *pqvar, float limit, int mask, int *compliance) {
	float pcent = (1. - pqvar->var);
	if (pcent < limit) {
		*compliance |= (1<<mask);
	}
	return ++mask;
}

// 0: OK, 1: failed
int QualVarCompliance3(QualVariation *pqvar, float limit, int mask, int *compliance) {
	int i;
	float pcent;
	
	for (i=0; i<3; i++, mask++) {
		pcent = (1. - pqvar->var);
		if (pcent < limit) {
			*compliance |= (1<<mask);
		}
	}
	return mask;
}


#define	MAX_LOG_CNT_10M	(6*24*7)

// 10분 또는 1분 데이터를 Week에 누적한다 
// event 발생 이력을 쓴다 
void updateQualWeek(QualData10m *pq10m, QualWeek *pqw) {
	int i, j, compliance=0, mask=0;
	QualVarData *psrc=&pq10m->var, *pdst=&pqw->var;
	
	if (pqw->startTs == 0) {
		pqw->startTs = sysTick1s;
	}
	
	pqw->count10m++;	// # of 10m Record
	pqw->count10s += pq10m->avg.count10s;
	//pqw->dcount += pq10m->count;		
	if (pqw->count10m >= MAX_LOG_CNT_10M) {
		pqw->complete = 1;
	}
	
#ifdef VQ_VAR_V0	
	for (i=0; i<12; i++) {
		pdst->evtCount[i] += psrc->evtCount[i];
	}
#else
	for (i=0; i<4; i++) {
		pdst->sag[i] += psrc->sag[i];
		pdst->swell[i] +=  psrc->swell[i];
		pdst->shortIntr[i] +=  psrc->shortIntr[i];
		pdst->longIntr[i] +=  psrc->longIntr[i];
		pdst->rvc[i] += psrc->rvc[i];
	}
#endif	
	
	// Freq Var
	QualVarWeekUpdate(&pdst->Freq1, psrc->Freq1.err, pqw->count10s);
	QualVarWeekUpdate(&pdst->Freq2, psrc->Freq2.err, pqw->count10s);
	// Voltage Balance
	QualVarWeekUpdate(&pdst->Voltbal, psrc->Voltbal.err, pqw->count10m);
		
	// Volt Var 1,2
	for (i=0; i<3; i++) {
		QualVarWeekUpdate(&pdst->Volt1[i],  psrc->Volt1[i].err, pqw->count10m);
		QualVarWeekUpdate(&pdst->Volt2[i],  psrc->Volt2[i].err, pqw->count10m);
		QualVarWeekUpdate(&pdst->VoltThd[i],psrc->VoltThd[i].err, pqw->count10m);
		QualVarWeekUpdate(&pdst->VoltHd[i], psrc->VoltHd[i].err, pqw->count10m);
		QualVarWeekUpdate(&pdst->Pst[i],    psrc->Pst[i].err, pqw->count10m);
		QualVarWeekUpdate(&pdst->Plt[i],    psrc->Plt[i].err, pqw->count10m);
		QualVarWeekUpdate(&pdst->Svolt[i],  psrc->Svolt[i].err, pqw->count10m);
	}	
	
	mask = QualVarCompliance1(&pqw->var.Freq1, 0.995, mask, &compliance);
	mask = QualVarCompliance1(&pqw->var.Freq2, 1., mask, &compliance);
	mask = QualVarCompliance3(pqw->var.Volt1, 0.95, mask, &compliance);
	mask = QualVarCompliance3(pqw->var.Volt2, 1., mask, &compliance);
	mask = QualVarCompliance1(&pqw->var.Voltbal, 1., mask, &compliance);			
	mask = QualVarCompliance3(pqw->var.VoltThd, 0.95, mask, &compliance);
	mask = QualVarCompliance3(pqw->var.VoltHd, 0.95, mask, &compliance);
	mask = QualVarCompliance3(pqw->var.Pst, 0.95, mask, &compliance);
	mask = QualVarCompliance3(pqw->var.Plt, 0.95, mask, &compliance);
	mask = QualVarCompliance3(pqw->var.Svolt, 0.95, mask, &compliance);
	pqw->compliance = compliance;
		// 6*24*7
	if (pqw->count10m >= 1008) {
		pqw->complete = 1;
		pqw->compliance |= (1<<31);
	}	
}

// meter 영역으로 복사한다 
void updateQualReport(int meterIdx, QualWeek *pqw, int pos) {
	int i, j, error=0, mask=0;
	float pcent, limit[2];
	EN50160 *prpt = &meter[meterIdx].rpt[pos];
	
	prpt->sTime = pqw->startTs;
	prpt->eTime = pqw->endTs;
	prpt->compliance = (pqw->complete) ? (1<<31) : 0;
		
	// Compliance 가 OK 이면 set 아니면 reset 한다 
	// Freq Var

	// Freq var 1
	prpt->Fvar1 = getVarPercent(&pqw->var.Freq1);
	prpt->Fvar2 = getVarPercent(&pqw->var.Freq2);
	prpt->Voltbal = getVarPercent(&pqw->var.Voltbal);
	
	for (i=0; i<3; i++) {
		prpt->Volt1[i] = getVarPercent(pqw->var.Volt1);
		prpt->Volt2[i] = getVarPercent(pqw->var.Volt2);
		prpt->VoltThd[i] = getVarPercent(pqw->var.VoltThd);
		prpt->VoltHd[i] = getVarPercent(pqw->var.VoltHd);
		prpt->Pst[i] = getVarPercent(pqw->var.Pst);
		prpt->Plt[i] = getVarPercent(pqw->var.Plt);
		prpt->Svolt[i] = getVarPercent(pqw->var.Svolt);
	}
		
	for (i=0; i<4; i++) {
		prpt->sag[i] = pqw->var.sag[i];
		prpt->swell[i] = pqw->var.swell[i];
		prpt->shortIntr[i] = pqw->var.shortIntr[i];
		prpt->longIntr[i] = pqw->var.longIntr[i];
		prpt->rvc[i] = pqw->var.rvc[i];
	}
	
	prpt->compliance = pqw->compliance;
}


// log data를 modbus에 쓴다 
void writeLogData(int id, uint32_t ts) {
	int i;
	QualData10m  *pq10m = &meter[id].qdLog.q10m;	
	HarmonicsData *phm = &meter[id].cntl.hmd;
	LOG_DATA *pld = &meter[id].log;
	
	// rms
	pld->ts = ts;	
	for (i=0; i<3; i++) {
		pld->U[i]    = pq10m->avg.U[i];
		pld->I[i]    = pq10m->avgExp.I[i];
	}		
	//pld->In = pld->In;	// bug
	pld->In = pq10m->avgExp.In;
	
	// unbalance
	for (i=0; i<2; i++) {
		pld->Ubal[i] = pq10m->avg.Ubal[i];
		pld->Ibal[i] = pq10m->avgExp.Ibal[i];
	}
	// 역률
	pld->PF  = pq10m->avgExp.PF;
	pld->dPF = pq10m->avgExp.dPF;
	// 온도
	for (i=0; i<5; i++) {
		pld->temp[i] = pq10m->avgExp.temp[i];
	}
	// 전력
	for (i=0; i<2; i++) {
		pld->kw[i]   = pq10m->avgExp.P[i];
		pld->kvar[i] = pq10m->avgExp.Q[i];		
	}	
	pld->kVA = pq10m->avgExp.S;
	//
	for (i=0; i<3; i++) {
		pld->Uthd[i]   = phm->Uthd[i];		
		pld->Uppthd[i] = phm->Uppthd[i];		
		pld->Ithd[i]   = phm->Ithd[i];
		pld->Itdd[i]   = phm->Itdd[i];
		pld->kf[i]     = phm->Ikf[i];				
	}
	// HD
	memcpy(pld->Uhd, phm->Uhd, sizeof(pld->Uhd));
	memcpy(pld->Upphd, phm->Upphd, sizeof(pld->Upphd));
	memcpy(pld->Ihd, phm->Ihd, sizeof(pld->Ihd));
}


#ifdef	_QUAL_TEST
	#define	N_FREQ	6	// 6@min
#else
	#define	N_FREQ	60	// 60@10min
#endif


void timeStampChanged(void) {
	int id;
	for (id = 0; id < METER_CH_COUNT; id++)
		meter[id].qdLog.tsChanged = 1;
}

// 1초 단위로 호출 (미터 태스크/스캔당 id 전달)
// 10초 데이터 수집하여 10분 데이터 만든다 
int updateQualData(int id) 
{
	int i, j, bix, woY, doY;
	float	freq;
	QualLogData *pqLog = &meter[id].qdLog;
	METERING *pmeter = &meter[id].meter;
	CNTL_DATA *pcntl = &meter[id].cntl;
	QualData10m  *pq10m = &pqLog->q10m;
	QualSumData  *pqsum = &pqLog->qsum;
	struct tm ltm;
	
	pq10m->count10m++;	// 10분 평균 위한 1s 데이터 카운트	
	
	/* [VQ 편차] 실시간 전압 부족/초과 편차 — 공칭전압 db.pt[id].vnorm 기준(EN50160 uLo/uHi와 동일 소스).
	 *  Flicker(Pi/Pst/Plt)는 IEC 61000-4-15 미구현이라 0 유지(보류). Uuv/Uov=해당조건 시 전압값. */
	{
		VQDATA *pvq = &meter[id].vq;
		float vnorm = (float)db.pt[id].vnorm;
		float u, dev;
		for (i = 0; i < 3; i++) {
			if (pcntl->online && vnorm > 0) {
				u = pmeter->U[i];
				dev = (u - vnorm) / vnorm * 100.0f;
				pvq->Uud[i] = (dev < 0) ? -dev : 0;
				pvq->Uod[i] = (dev > 0) ?  dev : 0;
				pvq->Uuv[i] = (dev < 0) ? u : 0;
				pvq->Uov[i] = (dev > 0) ? u : 0;
			} else {
				pvq->Uud[i] = pvq->Uod[i] = pvq->Uuv[i] = pvq->Uov[i] = 0;
			}
		}
	}

	// 주파수 평균 계산위한 summation
	pqsum->freq += pmeter->Freq;
	pq10m->count10s++;	
	
	// 10초 단위로 주파수 변동률 검사, 시각동기화로 인해 영향 받을 수 있다 
	if (pq10m->ts10s != sysTick10s || pqLog->tsChanged) {		
		//
		if (pq10m->count10s < 10) {
			printf("--> skip mean freq, count=%d\n", pq10m->count10s);
		}

		// 10초 평균 주파수 
		freq = pqsum->freq/pq10m->count10s;				
		pq10m->ts10s = sysTick10s;
		
		// time이 변경되도 60개 까지 저장한다 
		if (pq10m->avg.count10s < N_FREQ) {
			j = pq10m->avg.count10s++;					
			pq10m->avg.ts10[j] = j;
			pq10m->avg.Freq[j] = freq;
		}				
		
		// 주파수 변동률 계산
		QualVarUpdate(&pq10m->var.Freq1, freq, pcntl->freqLo[0], pcntl->freqHi[0], pq10m->avg.count10s);
		QualVarUpdate(&pq10m->var.Freq2, freq, pcntl->freqLo[1], pcntl->freqHi[1], pq10m->avg.count10s);		
		//printf("((pq10m->count=%d, pq10m->avg.count10s=%d, sysTick10s=%d, inx=%d, freq=%.2f))\n", pq10m->count10m, pq10m->avg.count10s, sysTick10s, j, freq);
		
		// harmonics 복사
		// 전압 2 부터 25 차수에 대해 평균 계산한다 
		for (i=0; i<3; i++) {
			pq10m->avg.Uthd[i] = pcntl->hmd.Uthd[i];
		}
		memcpy(pq10m->avg.Uhd, pcntl->hmd.Uhd, sizeof(pq10m->avg.Uhd));		
		
		// clear freq temp sum & count
		pqsum->freq = pq10m->count10s = 0;		
	}
	//
	// 1초 마다 평균값 계산한다 
	// 전압 THD
	for (i=0; i<3; i++) {
		pqsum->U[i]    += pcntl->U_1012[i];
		pqsum->I[i]    += pmeter->I[i];		
	}	
	pqsum->In  += pmeter->In;
	pqsum->dPF += pmeter->dPF[3];
	pqsum->PF  += pmeter->PF[3];	
	// 불평형
	for (i=0; i<2; i++) {
		pqsum->Ubal[i] += pmeter->Ubal[i];	
		pqsum->Ibal[i] += pmeter->Ibal[i];	
	}	
	// 온도 
	pqsum->temp[0] += pmeter->Temp;	// 장치 온도 
	// 변압기 온도 
	// for (i=0; i<4; i++) {
	// 	pqsum->temp[i+1] += piom->io[0].aiData[i];	// 첫번째 IOM
	// }	
	//전력
	if (pmeter->P[3] >= 0) 
		pqsum->P[0] += pmeter->P[3];
	else
		pqsum->P[1] += -pmeter->P[3];
	
	if (pmeter->Q[3] >= 0) 
		pqsum->Q[0] += pmeter->Q[3];
	else
		pqsum->Q[1] += -pmeter->Q[3];		
	pqsum->S += pmeter->S[3];

	//
	// running average 계산		
	//
	for (i=0; i<3; i++) {
		pq10m->avg.U[i]    = pqsum->U[i]/pq10m->count10m;
		pq10m->avgExp.I[i] = pqsum->I[i]/pq10m->count10m;	
	}	
	pq10m->avgExp.In  = pqsum->In/pq10m->count10m;	
	pq10m->avgExp.PF  = pqsum->PF/pq10m->count10m;	
	pq10m->avgExp.dPF = pqsum->dPF/pq10m->count10m;	
	
	// 불평형률
	for (i=0; i<2; i++) {
		pq10m->avg.Ubal[i]    = pqsum->Ubal[i]/pq10m->count10m;		
		pq10m->avgExp.Ibal[i] = pqsum->Ibal[i]/pq10m->count10m;		
	}
	// 온도 
	for (i=0; i<5; i++) {
		pq10m->avgExp.temp[i] = pqsum->temp[i]/pq10m->count10m;		
	}
	// 전력
	for (i=0; i<2; i++) {
		pq10m->avgExp.P[i] = pqsum->P[i]/pq10m->count10m;
		pq10m->avgExp.Q[i] = pqsum->Q[i]/pq10m->count10m;		
	}
	pq10m->avgExp.S = pqsum->S/pq10m->count10m;
	

	// 새로운 10분 평균값 가지고 변동률 계산한다 
	if (pq10m->ts10m != sysTick10m || pqLog->tsChanged) {		
		if (pqLog->tsChanged) printf("--> Time Changed ...\n");
		pqLog->tsChanged = 0;
		pq10m->avg.endTs = sysTick1s;
							
		// 전압 변동률 검사
		for (i=0; i<3; i++) {
			QualVarUpdate(&pq10m->var.Volt1[i], pq10m->avg.U[i], pcntl->uLo[0], pcntl->uHi[0], pq10m->count10m);
			QualVarUpdate(&pq10m->var.Volt2[i], pq10m->avg.U[i], pcntl->uLo[1], pcntl->uHi[1], pq10m->count10m);
		}
		// 불평형률 2% 초과검사
		QualVarUpdate(&pq10m->var.Voltbal, pq10m->avg.Ubal[0], 0, 2, pq10m->count10m);
		// 고조파 함유율 초과 검사 (8%)
		for (i=0; i<3; i++) {
			QualVarUpdate(&pq10m->var.VoltThd[i], pq10m->avg.Uthd[i], 0, 8, pq10m->count10m);
		}		
		// 각 차수별 고조파 초과 검사 
		for (i=0; i<3; i++) {
			for (j=2; j<=25; j++) {
				if (QualVarUpdate(&pq10m->var.VoltHd[i], pq10m->avg.Uhd[i][j-2], 0, harmLimit[j-2], pq10m->count10m)) {
					break;
				}
			}
		}
		
		writeLogData(id, sysTick1s);
					
		// report 주기 변동 여부 확인 
		if (id != 0) {	/* M0~M2 동일전압: EN50160(전압품질)은 M0만 산출 → M1/M2는 M0 결과 복사(ql1/qw1 write·QualWeek 중복 제거). 전류/전력(avgExp·writeLogData)은 위에서 채널별 유지 */
			meter[id].rpt[0] = meter[0].rpt[0];
			meter[id].rpt[1] = meter[0].rpt[1];
		}
		else if (pqLog->qw.startTs <= sysTick1s && sysTick1s < pqLog->qw.endTs) {
			// 10m Log 쓰기					
			writeQual10mData(id, pqLog->qlfn, pq10m);
			
			// 주별 통계 데이터 갱신
			updateQualWeek(pq10m, &pqLog->qw);		
			updateQualWeekData(id, pqLog->qwfn, &pqLog->qw);	// 이벤트 데이터를 뒷 부분에 쓴다 
			// 생성된 데이터를 금주로 복사한다 
			updateQualReport(id, &pqLog->qw, 0);		
		}
		else {
			// 새로운 로그 시작전에 마지막 데이터를 갱신한다
			// 10m Log 쓰기		
			writeQual10mData(id, pqLog->qlfn, pq10m);			
			// 새로운 주기 시작전에 마지막 데이터를 갱신한다
			// 주별 통계 데이터 갱신
			updateQualWeek(pq10m, &pqLog->qw);		
			// 생성된 데이터를 전주로 복사한다 
			updateQualReport(id, &pqLog->qw, 1);						
			updateQualWeekData(id, pqLog->qwfn, &pqLog->qw);
						
			// 새로운 주기 데이터 준비
			getQualLogFN(pqLog->qlfn, id);
			getQualWeekFN(pqLog->qwfn, id);
			getQualLastWeekFN(pqLog->qwfnLast, id);
			
			memset(&pqLog->qw, 0, sizeof(pqLog->qw));
			pqLog->qw.startTs = sysTick1s;
			pqLog->qw.endTs = getQualWeekEndTs(pcntl);
//			pqLog->qw.year = pcntl->tod.tm_year;
//			pqLog->qw.woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
			printf("[[Create QualWeekData: %s]]\n", pqLog->qwfn);			
			createQualWeekData(pqLog->qwfn, &pqLog->qw);		
			// 초기화된 데이터를 금주로 복사한다 
			updateQualReport(id, &pqLog->qw, 0);						
		}
				
		memset(&pq10m->avg, 0, sizeof(pq10m->avg));	
		memset(&pq10m->var, 0, sizeof(pq10m->var));	
		// 타이머 재설정
		pq10m->count10m = 0;		// 1s count		
		pq10m->ts10m = sysTick10m;
		pq10m->avg.count10s = 0;	// 10s count			
		pq10m->avg.startTs = sysTick1s;			
		// 
		memset(pqsum, 0, sizeof(*pqsum));			
		
		return 1;
	}
	
	return 0;
}


void initPQHeader(int id) {
	FILE *fp;
	struct tm lt;
	int woY, ret=-1, flag=0;
	QualLogData *pqLog = &meter[id].qdLog;
	CNTL_DATA *pcntl = &meter[id].cntl;
	//char path[64];
	
	// 로그 파일 이름 얻는다 
	getQualLogFN(pqLog->qlfn, id);
	getQualWeekFN(pqLog->qwfn, id);
	getQualLastWeekFN(pqLog->qwfnLast, id);
	printf("{{{Log File[m%d] : %s, %s, %s}}}\n", id, pqLog->qlfn, pqLog->qwfn, pqLog->qwfnLast);
		
	// log 파일 지우고 새로 시작하기 
	fp = fopen("initqual.d", "rb");
	if (fp) {
		fread(&flag, sizeof(flag), 1, fp);
		fclose(fp);
	}
	
	if (flag) {		
#ifdef USE_CMSIS_RTOS2
      fdelete(pqLog->qlfn, NULL);		
#else
		fdelete(pqLog->qlfn);
#endif
		printf("delete log file(%s) ...\n", pqLog->qlfn);
		
#ifdef USE_CMSIS_RTOS2				
      fdelete(pqLog->qwfn, NULL);		
#else
		fdelete(pqLog->qwfn);
#endif
		printf("delete report file(%s) ...\n", pqLog->qwfn);
		
		/* M0만 삭제 후 플래그를 지우지 않음 — M1/M2 initPQHeader에서도 동일 삭제 */
		if (id == ACTIVE_METER_CH_COUNT - 1) {
			flag = 0;
			fp = fopen("initqual.d", "wb");
			fwrite(&flag, sizeof(flag), 1, fp);
			fclose(fp);
		}
	}
	
	
	/* EN50160(QualWeek)은 전압 기반 → M0~M2 동일. M0만 파일 로드/생성하고
	 * M1/M2는 방금 로드된 M0의 rpt를 복사한다(중복 파일 I/O 제거, 부팅 직후부터 리포트 일치).
	 * initPQHeader는 Meter0_Task에서 m0→m1→m2 직렬 호출이라 meter[0].rpt 로드 완료가 보장됨.
	 * 런타임 updateQualData도 10분마다 M0 rpt를 M1/M2로 복사(Quality.c:966)해 정합 유지. */
	if (id == 0) {
		// 전주 QualWeekReport를 읽는다
		if (readQualWeekData(pqLog->qwfnLast, &pqLog->qw) == 0) {
			updateQualReport(id, &pqLog->qw, 1);
		}

		// 금주 QualWeekReport를 읽는다
		if (readQualWeekData(pqLog->qwfn, &pqLog->qw) == 0) {
			printf("[[Load readQualWeekData File : %s]]\n", pqLog->qwfn);
			updateQualReport(id, &pqLog->qw, 0);

			uLocalTime(&pqLog->qw.startTs, &lt);
			printf("|| Start: %d-%d-%d\n", lt.tm_year, lt.tm_mon, lt.tm_mday);
			uLocalTime(&pqLog->qw.endTs, &lt);
			printf("|| End: %d-%d-%d\n", lt.tm_year, lt.tm_mon, lt.tm_mday);
			printf("|| count10s: %d\n", pqLog->qw.count10s);
			printf("|| count10m: %d\n", pqLog->qw.count10m);
		}
		// QualWeek File이 없으면 새로 만든다
		else {
			memset(&pqLog->qw, 0, sizeof(pqLog->qw));
			pqLog->qw.startTs = sysTick1s;
			pqLog->qw.endTs = getQualWeekEndTs(pcntl);
//			pqLog->qw.year = pcntl->tod.tm_year;
//			pqLog->qw.woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
			createQualWeekData(pqLog->qwfn, &pqLog->qw);
			printf("[[Create readQualWeekData: %s]]\n", pqLog->qwfn);
			uLocalTime(&pqLog->qw.startTs, &lt);
			printf("|| Start: %d-%d-%d\n", lt.tm_year, lt.tm_mon, lt.tm_mday);
			uLocalTime(&pqLog->qw.endTs, &lt);
			printf("|| End: %d-%d-%d\n", lt.tm_year, lt.tm_mon, lt.tm_mday);
		}
	}
	else {
		/* M1/M2: QualWeek 파일 로드/생성 없이 M0 EN50160 리포트 복사 */
		meter[id].rpt[0] = meter[0].rpt[0];
		meter[id].rpt[1] = meter[0].rpt[1];
		printf("[[Copy EN50160 report M0->M%d (voltage shared)]]\n", id);
	}

	// time stamp 초기화 
	memset(&pqLog->q10m, 0, sizeof(pqLog->q10m));
	pqLog->q10m.ts10s = sysTick10s;
	pqLog->q10m.ts10m = sysTick10m;	
	pqLog->q10m.avg.startTs = sysTick1s;		

	// 부팅 시 1회 용량 정리
	trimQualLogBudget();
}
