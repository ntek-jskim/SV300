
#include "proxy.h"
#include "os_port.h"

PROXY_BUF proxyQ __attribute__ ((section ("EXT_RAM"), zero_init));


void osEventFlagsWait(int flag, int timeout) {
	os_evt_wait_and(flag, timeout);
}

void osEventFlagsSet(OsTaskId tid, int flag) {
	os_evt_set(flag, tid);
}	

void pushSvrCmd(int cmd) {
	int fr = proxyQ.rQ.fr;
	proxyQ.rQ.que[fr].cmd = cmd;
	proxyQ.rQ.que[fr].length = 0;
	proxyQ.rQ.fr = (fr + 1) % N_PROXY_Q;
	//printf("%s: c=%d, f=%d, r=%d\n", __FUNCTION__, cmd, proxyQ.rQ.fr, proxyQ.rQ.re);
	osThreadFlagsSet(proxyQ.rQ.tid, 0x1);
}

void pushSvrData(int cmd, uint8_t *buf, size_t length) {	
	int fr = proxyQ.rQ.fr;
	proxyQ.rQ.que[fr].cmd = cmd;
	proxyQ.rQ.que[fr].length = length;
	memcpy(proxyQ.rQ.que[fr].buf, buf, length);
	proxyQ.rQ.fr = (fr + 1) % N_PROXY_Q;	
	//printf("%s: c=%d, l=%d, f=%d, r=%d\n", __FUNCTION__, cmd, length, proxyQ.rQ.fr, proxyQ.rQ.re);
	osThreadFlagsSet(proxyQ.rQ.tid, 0x1);
}

int popSvrData(int *cmd, uint8_t *buf, size_t *length) {
	int re = proxyQ.rQ.re;
	if (proxyQ.rQ.fr != re) {		
		*cmd = proxyQ.rQ.que[re].cmd;
		*length = proxyQ.rQ.que[re].length;
		memcpy(buf, proxyQ.rQ.que[re].buf, *length);
		proxyQ.rQ.re = (re + 1) % N_PROXY_Q;
		//printf("%s:  c=%d, l=%d, f=%d, r=%d\n", __FUNCTION__, *cmd, *length, proxyQ.rQ.fr, proxyQ.rQ.re);
		return 0;
	}
	else
		return -1;
}

int isSvrQFull() {
	int fr = (proxyQ.rQ.fr + 1) % N_PROXY_Q;
	return (fr == proxyQ.rQ.re) ? 1 : 0;
}

//
//
//

void pushCliData(int cmd, uint8_t *buf, size_t length) {
	int fr = proxyQ.tQ.fr;
	proxyQ.tQ.que[fr].cmd = cmd;
	proxyQ.tQ.que[fr].length = length;
	memcpy(proxyQ.tQ.que[fr].buf, buf, length);
	proxyQ.tQ.fr = (fr + 1) % N_PROXY_Q;
	//printf("%s: c=%d, l=%d, f=%d, r=%d\n", __FUNCTION__, cmd, length, proxyQ.rQ.fr, proxyQ.rQ.re);
	osThreadFlagsSet(proxyQ.tQ.tid, 0x1);
}

void pushCliCmd(int cmd) {
	int fr = proxyQ.tQ.fr;
	proxyQ.tQ.que[fr].cmd = cmd;
	proxyQ.tQ.que[fr].length = 0;
	proxyQ.tQ.fr = (fr + 1) % N_PROXY_Q;
	//printf("%s: c=%d, f=%d, r=%d\n", __FUNCTION__, cmd, proxyQ.rQ.fr, proxyQ.rQ.re);
	osThreadFlagsSet(proxyQ.tQ.tid, 0x1);
}

int popCliData(int *cmd, uint8_t *buf, size_t *length) {
	int re = proxyQ.tQ.re;
	if (proxyQ.tQ.fr != re) {		
		*cmd = proxyQ.tQ.que[re].cmd;
		*length = proxyQ.tQ.que[re].length;
		memcpy(buf, proxyQ.tQ.que[re].buf, *length);
		proxyQ.tQ.re = (re + 1) % N_PROXY_Q;
		//printf("%s:  c=%d, l=%d, f=%d, r=%d\n", __FUNCTION__, *cmd, *length, proxyQ.rQ.fr, proxyQ.rQ.re);
		return 0;
	}
	else
		return -1;
}

int isCliQFull() {
	int fr = (proxyQ.tQ.fr + 1) % N_PROXY_Q;
	return (fr == proxyQ.tQ.re) ? 1 : 0;
}
// server to client, task id(client) to wake up
void setProxyClientTid() {
	proxyQ.rQ.tid = os_tsk_self();
}

// client to server, task id(server) to wake up
void setProxyServerTid() {
	proxyQ.tQ.tid = os_tsk_self();
}

//
void initProxyAgent() {
	memset(&proxyQ, 0, sizeof(proxyQ));
}




