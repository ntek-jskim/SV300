#ifndef _SMB_RTU_H

#define _SMB_RTU_H

#include "os.h"
#include "lpc_types.h"

#ifdef __FREERTOS
void SMB_rtu_Task(void);
void gw_sndtask(void);
void gw_rcvtask(void);
#else
void SMB_rtu_Task(void *);
void SMB_rtu_Task2(void *);
void gw_sndtask(void *);
void gw_rcvtask(void *);
#endif

//void putCommand(int addr, int count, uint16_t *pcmd);
//void putSettings(int addr, int count, uint16_t *pdata, void *p);
//void putAlmSettings(int addr, int count, uint16_t *psrc, void *pdst);
int loadFramData1(int offset, int size, uint8_t *pbuf);
void saveFramData1(int offset, int size, uint8_t *pbuf);
int loadFramData2(int offset, int size, int inc, uint8_t *pbuf);
void saveFramData2(int offset, int size, int inc, uint8_t *pbuf);

#endif
