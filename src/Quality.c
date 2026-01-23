#ifdef __FREERTOS
   #include "RTL.h"  // for File IO
   #include "os_port.h"
#endif
#include "board.h"
#include "meter.h"
#include "ade9000.h"
#include "math.h"
#include "time.h"
#include "string.h"

extern EVENT_Q eventQ;

// 2nd ~ 25th, 10000 -> 100%
float harmLimit[] = {	
	2,   5,   1,   6,   0.5, 5,   0.5, 1.5, 0.5,	// 2 ~ 10
	3.5, 0.5, 3,   0.5, 0.5, 0.5, 2,   0.5, 1.5, 0.5,	// 11 ~ 20
	0.5, 0.5, 1.5, 0.5, 1.5	// 21 ~ 25
};

// 10초에 하나 증가
extern uint32_t sysTick1s, sysTick10s, sysTick10m;
extern int readQualWeekData(char *path, QualWeek *pqw);

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
void getQualLogFN(char *path) {
	char dstr[16];
	
	getQualStartDate(dstr);
	//sprintf(path, "%s\\QL0_%s.d", LOG_PQ_DIR, dstr);			
	sprintf(path, "%s%s.d", QL_FILE, dstr);			
}

// 현재 시간 기준으로 금주의 시작일을 구한다 또는 Test용으로 오늘 날짜를 구한다 
void getQualWeekFN(char *path) {
	char dstr[16];
		
	getQualStartDate(dstr);
	//sprintf(path, "%s\\QW0_%s.d", LOG_PQ_DIR, dstr);			
	sprintf(path, "%s%s.d", QW_FILE, dstr);			
}

// 현재 시간 기준으로 전주의 시작일을 구한다 
void getQualLastWeekFN(char *path) {
	char dstr[16];
	
	getQualLastStartDate(dstr);
	//sprintf(path, "%s\\QW0_%s.d", LOG_PQ_DIR, dstr);			
	sprintf(path, "%s%s.d", QW_FILE, dstr);			
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
	fi.fileID = 0;      
	if (ffind (fn, &fi)) {
		printf("createQualLog(%s), ...\n", fn);
		fp = fopen(fn, "wb");
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
		fp = fopen(fn, "ab");
		if (fp) {
			fseek(fp, -1, SEEK_END);
			fwrite(bf, size, 1, fp);	// 마지막에 새 record 기록한다 
			fclose(fp);				
		} 
		else {
			printf("~~~ appendQualLog, can't open log file(%s)\n", fn);
		}
	}
}


// 10m 데이터를 파일에 기록한다. 10m을 다 채우지 못하면 기록하지 않는다 
int writeQual10mData(char *path, QualData10m *pq10m) {	
	int		result, woY;
	//FINFO info;
	FILE *fp;
	struct tm lt;
	
	appendQualLog(path, &pq10m->avg, sizeof(pq10m->avg));
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
	
	printf("writeQualWeekData (%s) ...\n", path);
	fp = fopen(path, "wb");
	if (fp == NULL) {
		printf("Can't open file(%s)\n", path);
		return -1;
	}
	
	fwrite(pqw, sizeof(QualWeek), 1, fp);
	fclose(fp);
	
	return 0;
}

int updateQualWeekData(char *path, QualWeek *pqw) {
	FILE *fp;
	
	// event count 갱신
	if (eventQ.count > 0) {
		pqw->evtCount += eventQ.count;
	}
	printf("updateQualWeekData (%s), event count=%d ...\n", path, eventQ.count);
	fp = fopen(path, "r+");
	if (fp == NULL) {
		printf("updateQualWeekData, Can't open file(r+:%s), create file(w)\n", path);		
		fp = fopen(path, "w");
		if (fp == NULL) {
			return -1;
		}
	}	

	fwrite(pqw, sizeof(QualWeek), 1, fp);	
	fclose(fp);	

	
	// 이벤트 로그를 QualWeek 뒷 부분에 추가 한다 
	if (eventQ.count > 0) {		
		fp = fopen(path, "a");
		fwrite(eventQ.eq, sizeof(EVENT_LOG), eventQ.count, fp);
		fclose(fp);		
		eventQ.count = 0;
	}

	
	return 0;
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
	
	printf("readQualWeekData (%s) ...\n", path);
	fp = fopen(path, "rb");
	if (fp == NULL) {
		printf("Can't open file(%s) ...\n", path);		
		return -1;
	}	
	
	fread(pqw, sizeof(QualWeek), 1, fp);
	fclose(fp);
	
	return 0;
}



uint32_t getQualWeekEndTs() {
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
void updateQualReport(QualWeek *pqw, int pos) {
	int i, j, error=0, mask=0;
	float pcent, limit[2];
	EN50160 *prpt = &pRPT[pos];
	
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
void writeLogData(uint32_t ts) {
	int i;
	QualData10m  *pq10m = &pqLog->q10m;	
	HarmonicsData *phm = &pcntl->hmd;
	
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
	pqLog->tsChanged = 1;
}

// 1초 단위로 호출
// 10초 데이터 수집하여 10분 데이터 만든다 
int updateQualData() 
{
	int i, j, bix, woY, doY;
	float	freq;
	QualData10m  *pq10m = &pqLog->q10m;
	QualSumData  *pqsum = &pqLog->qsum;
	struct tm ltm;
	
	pq10m->count10m++;	// 10분 평균 위한 1s 데이터 카운트	
	
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
		
		writeLogData(sysTick1s);
					
		// report 주기 변동 여부 확인 
		if (pqLog->qw.startTs <= sysTick1s && sysTick1s < pqLog->qw.endTs) {
			// 10m Log 쓰기					
			writeQual10mData(pqLog->qlfn, pq10m);
			
			// 주별 통계 데이터 갱신
			updateQualWeek(pq10m, &pqLog->qw);		
			updateQualWeekData(pqLog->qwfn, &pqLog->qw);	// 이벤트 데이터를 뒷 부분에 쓴다 
			// 생성된 데이터를 금주로 복사한다 
			updateQualReport(&pqLog->qw, 0);		
		}
		else {
			// 새로운 로그 시작전에 마지막 데이터를 갱신한다
			// 10m Log 쓰기		
			writeQual10mData(pqLog->qlfn, pq10m);			
			// 새로운 주기 시작전에 마지막 데이터를 갱신한다
			// 주별 통계 데이터 갱신
			updateQualWeek(pq10m, &pqLog->qw);		
			// 생성된 데이터를 전주로 복사한다 
			updateQualReport(&pqLog->qw, 1);						
			updateQualWeekData(pqLog->qwfn, &pqLog->qw);
						
			// 새로운 주기 데이터 준비
			getQualLogFN(pqLog->qlfn);
			getQualWeekFN(pqLog->qwfn);
			getQualLastWeekFN(pqLog->qwfnLast);
			
			memset(&pqLog->qw, 0, sizeof(pqLog->qw));
			pqLog->qw.startTs = sysTick1s;
			pqLog->qw.endTs = getQualWeekEndTs();
//			pqLog->qw.year = pcntl->tod.tm_year;
//			pqLog->qw.woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);
			printf("[[Create QualWeekData: %s]]\n", pqLog->qwfn);			
			createQualWeekData(pqLog->qwfn, &pqLog->qw);		
			// 초기화된 데이터를 금주로 복사한다 
			updateQualReport(&pqLog->qw, 0);						
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


void initPQHeader() {
	FILE *fp;
	struct tm lt;
	int woY, ret=-1, flag=0;
	//char path[64];
	
	// 로그 파일 이름 얻는다 
	getQualLogFN(pqLog->qlfn);
	getQualWeekFN(pqLog->qwfn);
	getQualLastWeekFN(pqLog->qwfnLast);
	printf("{{{Log File : %s, %s, %s}}}\n", pqLog->qlfn, pqLog->qwfn, pqLog->qwfnLast);
		
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
		
		flag = 0;
		fp = fopen("initqual.d", "wb");
		fwrite(&flag, sizeof(flag), 1, fp);
		fclose(fp);
	}
	
	
	// 전주 QualWeekReport를 읽는다 
	if (readQualWeekData(pqLog->qwfnLast, &pqLog->qw) == 0) {		
		updateQualReport(&pqLog->qw, 1);
	}
	
	// 금주 QualWeekReport를 읽는다 
	if (readQualWeekData(pqLog->qwfn, &pqLog->qw) == 0) {
		printf("[[Load readQualWeekData File : %s]]\n", pqLog->qwfn);		
		updateQualReport(&pqLog->qw, 0);

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
		pqLog->qw.endTs = getQualWeekEndTs();
//		pqLog->qw.year = pcntl->tod.tm_year;
//		pqLog->qw.woY = getYear_n_WoY(pcntl->tod.tm_yday, pcntl->tod.tm_wday);		
		createQualWeekData(pqLog->qwfn, &pqLog->qw);
		printf("[[Create readQualWeekData: %s]]\n", pqLog->qwfn);
		uLocalTime(&pqLog->qw.startTs, &lt);
		printf("|| Start: %d-%d-%d\n", lt.tm_year, lt.tm_mon, lt.tm_mday);
		uLocalTime(&pqLog->qw.endTs, &lt);
		printf("|| End: %d-%d-%d\n", lt.tm_year, lt.tm_mon, lt.tm_mday);
	}
		
	// time stamp 초기화 
	memset(&pqLog->q10m, 0, sizeof(pqLog->q10m));
	pqLog->q10m.ts10s = sysTick10s;
	pqLog->q10m.ts10m = sysTick10m;	
	pqLog->q10m.avg.startTs = sysTick1s;		
}
