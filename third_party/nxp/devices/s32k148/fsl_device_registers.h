/*
 * Copyright 2014-2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2018 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __FSL_DEVICE_REGISTERS_H__
#define __FSL_DEVICE_REGISTERS_H__

/*
 * Include the cpu specific register header files.
 *
 * The CPU macro should be declared in the project or makefile.
 */
#if (defined(CPU_S32K148HAT0MLLT) || defined(CPU_S32K148HAT0MLQT) || \
    defined(CPU_S32K148HAT0MLUT) || defined(CPU_S32K148HAT0MMHT) || \
    defined(CPU_S32K148UGT0VLQT) || defined(CPU_S32K148UIT0VLQT) || \
    defined(CPU_S32K148UJT0VLLT) || defined(CPU_S32K148UJT0VLQT) || \
    defined(CPU_S32K148UJT0VLUT) || defined(CPU_S32K148UJT0VMHT))

#define S32K148_SERIES

/* CMSIS-style register definitions */
#include "S32K148.h"
/* CPU specific feature definitions */
#include "S32K148_features.h"

#else
    #error "No valid CPU defined!"
#endif

#endif /* __FSL_DEVICE_REGISTERS_H__ */

/*******************************************************************************
 * EOF
 ******************************************************************************/
