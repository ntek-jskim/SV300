#ifndef _SMB_H
#define _SMB_H

#ifdef __FREERTOS
#include "RTL.h"
#include "os_port.h"
#endif

#include "board.h"

#define	N_CONNECTION	8

#define	STATE_INIT		0
#define	STATE_IDLE		1
#define	STATE_ACTIVE 	2

typedef struct {
	int sock;
	int status;
	int pollTimer;
	int nr, nw;
	int port;
	int	longFrame;
	uint8_t rxb[300], txb[300], *ptx;
} SMB_INFO;

extern SMB_INFO	session[];

int slaveModbusTcp(uint8_t *psrc, int nRead, uint8_t *pdst, int *nWrite);
int modbusSlvChkTcpHeader(uint8_t *pBuf, uint16_t nr);
int modbusMakeTcpFrame(uint8_t *prx, uint8_t *ptx, uint16_t length);

void init_smb_session(void);
void init_mcast_session(void);
void init_udp_session(void);
void smb_tcp(void);

#ifdef __FREERTOS
U16 tcp_callback (U8 soc, U8 event, U8 *ptr, U16 par) ;
void TCPtimer_task (void);
#else
uint16_t tcp_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) ;
void TCPtimer_task (void *);
#endif

#endif /* __HELLO_WORLD_H__ */
