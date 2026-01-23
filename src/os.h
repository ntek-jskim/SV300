#ifndef _OS_H

#define _OS_H

#include "os_port.h"

#define  os_dly_wait(x) osDelayTask(x)
#define	BUSY_WAIT(x)	osDelayTask(x)

#endif //_OS_H

