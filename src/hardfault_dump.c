/**
 * @file hardfault_dump.c
 * @brief HardFault 시 SCB(CFSR/HFSR/BFAR 등)와 예외 스택 일부를 UART(USART3, debug_putchar)로 1회 출력.
 *
 * startup의 PUBWEAK HardFault_Handler 보다 강한 심볼로 링크되면 기존 B . 루프를 대체합니다.
 * Keil 프로젝트에 이 소스 파일을 추가해야 합니다.
 *
 * 주의: printf/RTOS 락은 쓰지 않음. UART는 debug.c의 debug_putchar(폴링)만 사용.
 *       FPU 확장 스택이면 PC 위치가 word[6]이 아닐 수 있어 STK0~ 일괄 덤프함.
 */

#include "board.h"
#include <stdint.h>

extern void debug_putchar(int c);

static void hf_putc(int c)
{
	debug_putchar(c);
}

static void hf_puts(const char *s)
{
	while (*s)
		hf_putc((unsigned char)*s++);
}

static void hf_hex32(uint32_t v)
{
	static const char hex[] = "0123456789ABCDEF";
	unsigned i;

	for (i = 0; i < 8; i++) {
		unsigned sh = 28U - (i * 4U);
		hf_putc(hex[(v >> sh) & 15U]);
	}
}

/* r0 = MSP 또는 PSP가 가리키는 예외 스택, r1 = 진입 시 LR(EXC_RETURN) */
void HardFault_Handler_C(uint32_t *sp, uint32_t exc_return)
{
	uint32_t cfsr, hfsr, dfsr, mmfar, bfar, afsr;
	unsigned i;

	__disable_irq();

	hf_puts("\r\n*** HardFault ***\r\nEXC_RETURN=");
	hf_hex32(exc_return);
	hf_puts(" SP=");
	hf_hex32((uint32_t)sp);
	hf_puts("\r\nCFSR=");
	cfsr = SCB->CFSR;
	hf_hex32(cfsr);
	hf_puts(" HFSR=");
	hfsr = SCB->HFSR;
	hf_hex32(hfsr);
	hf_puts(" DFSR=");
	dfsr = SCB->DFSR;
	hf_hex32(dfsr);
	hf_puts("\r\nMMFAR=");
	mmfar = SCB->MMFAR;
	hf_hex32(mmfar);
	hf_puts(" BFAR=");
	bfar = SCB->BFAR;
	hf_hex32(bfar);
	hf_puts(" AFSR=");
	afsr = SCB->AFSR;
	hf_hex32(afsr);
	hf_puts("\r\nSTK[0..11] (std frame: [6]=PC): ");
	for (i = 0; i < 12U; i++) {
		if (i)
			hf_putc(' ');
		hf_hex32(sp[i]);
	}
	hf_puts("\r\n*** end ***\r\n");

	for (;;) {
		/* WDT/재부팅 전에 여기서 멈춤 */
	}
}

#if (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6000000)) || defined(__GNUC__)
/* ARM Compiler 6 (armclang) 또는 GCC: naked asm */
__attribute__((naked)) void HardFault_Handler(void)
{
	__asm volatile(
		"mov r1, lr           \n"
		"tst lr, #4           \n"
		"ite eq               \n"
		"mrseq r0, msp        \n"
		"mrsne r0, psp        \n"
		"b      HardFault_Handler_C \n");
}

#elif defined(__CC_ARM)
/* ARM Compiler 5 (Keil legacy): 인라인 asm은 C 심볼을 모름 → IMPORT 필요 */
__asm void HardFault_Handler(void)
{
	PRESERVE8
	IMPORT HardFault_Handler_C
	mov r1, lr
	tst lr, #4
	ite eq
	mrseq r0, msp
	mrsne r0, psp
	b HardFault_Handler_C
}

#else
#error "hardfault_dump.c: 지원하지 않는 툴체인 — ARMCC5/ARMCC6 또는 GCC용 분기 추가"
#endif
