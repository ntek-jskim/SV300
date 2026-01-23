#ifndef	_WIZNE_THREAD_H

#define	_WIZNE_THREAD_H

#include "stdint.h"
#ifdef __RTX
#include "RTL.h"
#else
#include "cmsis_os2.h"
#endif

int wiznet_unsol_socket(int wsock, uint8_t *dst_addr, int dst_port);
int wiz_sendRecv(int wsock, uint8_t *txb, int nw, uint8_t *rxb, int nr, int to, uint8_t *dst_addr, uint16_t dst_port);

int W5500_Init(void);
void W5500_NetConfig(int ifno);
void W5500_TcpServer();

#ifdef __RTX
#else
extern osThreadId_t tid_unsol[];
#endif
#endif
