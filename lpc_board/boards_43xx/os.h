#ifndef OS_H

#define	OS_H

#ifdef __FREERTOS
	#include "os_port.h"
	#define	 BUSY_WAIT(x)	osDelayTask(x)
#else
//	#include "cmsis_os2.h"
//	#include "rl_fs.h"
//	#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE

//	#define	os_dly_wait(x) osDelay(x)
//	#define	  BUSY_WAIT(x)	osDelay(x)
#endif

#endif
