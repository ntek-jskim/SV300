#ifndef _PROXY_H

#define	_PROXY_H

#include "os_port.h"

#define	N_PROXY_Q	16
#define	N_PROXY_BUF	2048

#define	PROXY_ACCEPT	1
#define	PROXY_CLOSE		2
#define	PROXY_S_TO_C	3
#define	PROXY_C_TO_S	4

typedef struct {
	OsTaskId tid;
	int fr, re;
	struct Q {
		uint16_t	cmd;
		uint16_t	length;
		uint8_t	buf[N_PROXY_BUF];
	} que[N_PROXY_Q];
} PROXY_RING;

typedef struct {
	PROXY_RING	tQ;
	PROXY_RING	rQ;
	uint8_t 		buf[N_PROXY_BUF];
	int			length;
} PROXY_BUF;

extern PROXY_BUF proxyQ;
void pushSvrData(int cmd, uint8_t *buf, size_t length);
void pushSvrCmd(int cmd);
int  popCliData(int *cmd, uint8_t *buf, size_t *length);
int  isSvrQFull();

void pushCliData(int cmd, uint8_t *buf, size_t length);
void pushCliCmd(int cmd);
int  popSvrData(int *cmd, uint8_t *buf, size_t *length);
int  isCliQFull();

void setProxyServerTid(void);
void setProxyClientTid(void);

void initProxyAgent(void);

#endif
