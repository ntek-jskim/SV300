#ifndef	_MODBUS_H

#define _MODBUS_H

#include "lpc_types.h"

// sem protocol
//#define	_USE_SEM_PRT/



extern uint16_t gencrc_modbus(uint8_t * ptr, int len);
extern void dprtbuffer(int dbgF, char *title, uint8_t *pbuf, int count);
extern int	readMem(uint8_t *ptx, uint16_t start, uint16_t count);
extern int	readMem2(uint8_t *ptx, uint16_t start, uint16_t count);
extern int	writeMem(uint8_t *prx, uint8_t *ptx, uint16_t start, uint16_t count);
extern int readMem3500(uint8_t *ptx, uint16_t start, uint16_t count, int proto);
extern int readMem3600(uint8_t *ptx, uint16_t start, uint16_t count, int proto);

extern int modbusSlvChkFrame(uint8_t id, uint8_t fc, uint16_t start, uint16_t count, uint8_t *perrCode, int proto);
extern int makeExceptFrame(uint8_t addr, uint8_t fc, uint8_t errCode, uint8_t *pBuf);
extern int modbusSlvProcFrame(uint8_t *prx, uint16_t rxsize, uint8_t *ptx, int proto);
extern int modbusMstProcFrame(uint8_t id, uint8_t fc, uint16_t s, uint16_t c, uint8_t *gb);
extern uint16_t onionEventFrame(uint8_t *ptx);

#endif
