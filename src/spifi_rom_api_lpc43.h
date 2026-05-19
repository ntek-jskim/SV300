/***********************************************************************
 * LPC43xx SPIFI ROM API — RL-FlashFS용 최소 선언 (NXP/pyocd 헤더 축약).
 * ROM 함수 테이블 포인터: 0x10400118 (LPC43xx Family UM).
 ***********************************************************************/
#ifndef SPIFI_ROM_API_LPC43_H
#define SPIFI_ROM_API_LPC43_H

#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#define LONGEST_PROT 68

typedef uint8_t uc;

typedef struct {
	uint32_t base;
	uc flags;
	int8_t log2;
	uint16_t rept;
} protEnt;

typedef union {
	uint16_t hw;
	uc byte[2];
} stat_t;

typedef struct {
	uint32_t base, regbase, devSize, memSize;
	uc mfger, devType, devID, busy;
	stat_t stat;
	uint16_t reserved;
	uint16_t set_prot, write_prot;
	uint32_t mem_cmd, prog_cmd;
	uint16_t sectors, protBytes;
	uint32_t opts, errCheck;
	uc erase_shifts[4], erase_ops[4];
	protEnt *protEnts;
	char prot[LONGEST_PROT];
} SPIFIobj;

typedef struct {
	char *dest;
	uint32_t length;
	char *scratch;
	int32_t protect;
	uint32_t options;
} SPIFIopers;

#define S_ERASE_AS_REQD    0
#define S_ERASE_NOT_REQD   8
#define S_VERIFY_PROG      0x10
#define S_VERIFY_ERASE     0x20
#define S_NO_VERIFY        0
#define S_RCVCLK           0x80
#define S_INTCLK           0
#define S_FULLCLK          0x40
#define S_HALFCLK          0

typedef struct {
	int32_t (*spifi_init)(SPIFIobj *obj, uint32_t csHigh, uint32_t options, uint32_t mhz);
	int32_t (*spifi_program)(SPIFIobj *obj, char *source, SPIFIopers *opers);
	int32_t (*spifi_erase)(SPIFIobj *obj, SPIFIopers *opers);
	void (*cancel_mem_mode)(SPIFIobj *obj);
	void (*set_mem_mode)(SPIFIobj *obj);
	int32_t (*checkAd)(SPIFIobj *obj, SPIFIopers *opers);
	int32_t (*setProt)(SPIFIobj *obj, SPIFIopers *opers, char *change, char *saveProt);
	int32_t (*check_block)(SPIFIobj *obj, char *source, SPIFIopers *opers, uint32_t check_program);
	int32_t (*send_erase_cmd)(SPIFIobj *obj, uint8_t op, uint32_t addr);
	uint32_t (*ck_erase)(SPIFIobj *obj, uint32_t *addr, uint32_t length);
	int32_t (*prog_block)(SPIFIobj *obj, char *source, SPIFIopers *opers, uint32_t *left_in_page);
	uint32_t (*ck_prog)(SPIFIobj *obj, char *source, char *dest, uint32_t length);
	void (*setSize)(SPIFIobj *obj, int32_t value);
	int32_t (*setDev)(SPIFIobj *obj, uint32_t opts, uint32_t mem_cmd, uint32_t prog_cmd);
	uint32_t (*cmd)(uc op, uc addrLen, uc intLen, uint16_t len);
	uint32_t (*readAd)(SPIFIobj *obj, uint32_t cmd, uint32_t addr);
	void (*send04)(SPIFIobj *obj, uc op, uc len, uint32_t value);
	void (*wren_sendAd)(SPIFIobj *obj, uint32_t cmd, uint32_t addr, uint32_t value);
	int32_t (*write_stat)(SPIFIobj *obj, uc len, uint16_t value);
	int32_t (*wait_busy)(SPIFIobj *obj, uc prog_or_erase);
} SPIFI_RTNS;

#define SPIFI_ROM_PTR_ADDR (0x10400118UL)

#endif /* SPIFI_ROM_API_LPC43_H */
