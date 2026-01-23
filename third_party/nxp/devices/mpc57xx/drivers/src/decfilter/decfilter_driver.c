/*
 * Copyright 2019 NXP
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY NXP "AS IS" AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NXP OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*!
 * @file decfilter_driver.c
 *
 * @page misra_violations MISRA-C:2012 violations
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 11.4, Conversion between a pointer and integer type
 * The base addresses are provided as integers so they need to be cast to pointers.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 11.6, Cast from unsigned int to pointer
 * The base addresses are provided as integers so they need to be cast to pointers.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.7, External could be made static.
 * Function is defined for usage by application code.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.13, Could be declared as pointing to const.
 * Function prototype dependent on external function pointer typedef.*
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 1.3,  Taking address of near auto variable.
 * The code is not dynamically linked. An absolute stack address is obtained
 * when taking the address of the near auto variable.
 *
 */

#include <stddef.h>
#include "decfilter_driver.h"
#include "edma_driver.h"

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* Table of base addresses for DECFILTER instances */
static DECFILTER_Type * const s_decfilterBase[DECFILTER_INSTANCE_COUNT] = DECFILTER_BASE_PTRS;

#define DECFILTER_INVALID_DMA_CHAN (0xFFu)

static uint8_t dmaInputChans[DECFILTER_INSTANCE_COUNT] = {
    DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN,
#if (DECFILTER_INSTANCE_COUNT > 2U)
    DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN,
    DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN,
#endif
};

static uint8_t dmaOutputChans[DECFILTER_INSTANCE_COUNT] = {
    DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN,
#if (DECFILTER_INSTANCE_COUNT > 2U)
    DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN,
    DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN, DECFILTER_INVALID_DMA_CHAN,
#endif
};

/* cascade mode selection
 *
 * configures the block to work in cascade mode of operation
 *
 * Implements : decfilter_cascade_mode_t_Class
 */
#define DECFILTER_CASCADE_MODE_HEAD  (0x01U)    /* Cascade Mode, Head block config. */
#define DECFILTER_CASCADE_MODE_TAIL  (0x02U)    /* Cascade Mode, Tail block config. */
#define DECFILTER_CASCADE_MODE_MIDLE (0x03U)    /* Cascade Mode, Middle block config. */

#if FEATURE_DECFILTER_HAS_COMBINED_ZIR
#define NUM_INST_PER_DECFIL_xSEL (4u) /* Number of DECFILTER instances per SIU DECFIL(1-3) registers, for ZSEL and HSEL */
#define NUM_INST_PER_DECFIL_SRC  (8u) /* Number of DECFILTER instances per SIU DECFIL(4-5) registers, for TRIG_SRC */
#endif

/*******************************************************************************
 * Private functions
 ******************************************************************************/
static void DECFILTER_ConfigInputDmaBuffer(const uint32_t instance,
                                           const decfilter_dma_input_config_t * const dmaConfig)
{
    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    uint32_t srcAddr, destAddr, srcSizeInBytes;
    srcAddr = (uint32_t)dmaConfig->sourcePtr;
    destAddr = (uint32_t)(&(base->IB));
    srcSizeInBytes = sizeof(dmaConfig->sourcePtr[0u]) * dmaConfig->sourceLength;

    /* Update DECFILTER state with the DMA channel number for input buffer */
    dmaInputChans[instance] = dmaConfig->dmaChan;

    /* Configure EDMA to transfer 1 input sample .
     * After a number of transfers equal with the source buffer length, EDMA shall call the registered callback (if not NULL)
     * EDMA disables the channel after the src buffer has been copied and doesn't start the channel after Init().
     * The user shall call EDMA_DRV_StartChannel to enable the EDMA channel */
    edma_transfer_config_t tCfg;
    edma_loop_transfer_config_t tLoopCfg;
    tCfg.destAddr           = destAddr;
    tCfg.srcAddr            = srcAddr;
    tCfg.srcTransferSize    = EDMA_TRANSFER_SIZE_4B;
    tCfg.destTransferSize   = EDMA_TRANSFER_SIZE_4B;
    tCfg.srcOffset          = (int16_t)sizeof(dmaConfig->sourcePtr[0u]); /* After each transfer, the src address is incremented with the size of buffer element */
    tCfg.destOffset         = 0;
    tCfg.srcLastAddrAdjust  = -(int32_t)srcSizeInBytes; /* Circular src buffer: after the src buffer is copied, src address is set at the beginning of the buffer */
    tCfg.destLastAddrAdjust = 0;
    tCfg.srcModulo          = EDMA_MODULO_OFF;
    tCfg.destModulo         = EDMA_MODULO_OFF;
    tCfg.minorByteTransferCount    = sizeof(dmaConfig->sourcePtr[0u]);
    tCfg.scatterGatherEnable       = false;
    tCfg.scatterGatherNextDescAddr = 0u;
    tCfg.interruptEnable           = true;
    tCfg.loopTransferConfig        = &tLoopCfg;

    tLoopCfg.majorLoopIterationCount = dmaConfig->sourceLength; /* Number of minor loops in a major loop */
    tLoopCfg.srcOffsetEnable         = false;
    tLoopCfg.dstOffsetEnable         = false;
    tLoopCfg.minorLoopOffset         = 0;
    tLoopCfg.minorLoopChnLinkEnable  = false;
    tLoopCfg.minorLoopChnLinkNumber  = 0u;
    tLoopCfg.majorLoopChnLinkEnable  = false;
    tLoopCfg.majorLoopChnLinkNumber  = 0u;

    status_t status;
    status = EDMA_DRV_ConfigLoopTransfer(dmaConfig->dmaChan, &tCfg);
    (void)status;

    EDMA_DRV_DisableRequestsOnTransferComplete(dmaConfig->dmaChan, true);
    /* Do not start the DMA channel -  it needs to be started manually outside the DECFILTER driver */

    if (dmaConfig->callback != NULL)
    {
        status = EDMA_DRV_InstallCallback(dmaConfig->dmaChan, dmaConfig->callback, dmaConfig->callbackParam);
        (void)status;

        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_ERR_INT, true);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_HALF_MAJOR_LOOP_INT, false);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_MAJOR_LOOP_INT, true);
    }
    else
    {
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_ERR_INT, false);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_HALF_MAJOR_LOOP_INT, false);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_MAJOR_LOOP_INT, false);
    }
}

static void DECFILTER_ConfigOutputDmaBuffer(const uint32_t instance,
                                           const decfilter_dma_output_config_t * const dmaConfig)
{
    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    uint32_t srcAddr, destAddr, destSizeInBytes;

    srcAddr = (uint32_t)(&(base->OB));
    destAddr = (uint32_t)dmaConfig->destPtr;
    destSizeInBytes = sizeof(dmaConfig->destPtr[0u]) * dmaConfig->destLength;

    /* Update DECFILTER state with the DMA channel number for current OutputBuffer */
    dmaOutputChans[instance] = dmaConfig->dmaChan;

    /* Configure EDMA to transfer 1 result for each EQADC result.
     * After a number of transfers equal with the destination buffer length,
     * EDMA shall call the registered callback (if not NULL) */
    edma_transfer_config_t tCfg;
    edma_loop_transfer_config_t tLoopCfg;
    tCfg.destAddr           = destAddr;
    tCfg.srcAddr            = srcAddr;
    tCfg.srcTransferSize    = EDMA_TRANSFER_SIZE_4B;
    tCfg.destTransferSize   = EDMA_TRANSFER_SIZE_4B;
    tCfg.srcOffset          = 0;
    tCfg.destOffset         = (int16_t)sizeof(dmaConfig->destPtr[0u]); /* After each transfer, the dest address is incremented with the size of uint32_t */
    tCfg.srcLastAddrAdjust  = 0;
    tCfg.destLastAddrAdjust = -(int32_t)destSizeInBytes; /* Circular dest buffer: after the dest buffer is filled, dest address is decremented with size in bytes. */
    tCfg.srcModulo          = EDMA_MODULO_OFF;
    tCfg.destModulo         = EDMA_MODULO_OFF;
    tCfg.minorByteTransferCount    = sizeof(dmaConfig->destPtr[0u]);
    tCfg.scatterGatherEnable       = false;
    tCfg.scatterGatherNextDescAddr = 0u;
    tCfg.interruptEnable           = true;
    tCfg.loopTransferConfig        = &tLoopCfg;

    tLoopCfg.majorLoopIterationCount = dmaConfig->destLength; /* Number of minor loops in a major loop */
    tLoopCfg.srcOffsetEnable         = false;
    tLoopCfg.dstOffsetEnable         = false;
    tLoopCfg.minorLoopOffset         = 0;
    tLoopCfg.minorLoopChnLinkEnable  = false;
    tLoopCfg.minorLoopChnLinkNumber  = 0u;
    tLoopCfg.majorLoopChnLinkEnable  = false;
    tLoopCfg.majorLoopChnLinkNumber  = 0u;

    status_t status;
    status = EDMA_DRV_ConfigLoopTransfer(dmaConfig->dmaChan, &tCfg);
    (void)status;

    if (dmaConfig->callback != NULL)
    {
        status = EDMA_DRV_InstallCallback(dmaConfig->dmaChan, dmaConfig->callback, dmaConfig->callbackParam);
        (void)status;

        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_ERR_INT, true);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_MAJOR_LOOP_INT, true);

        if (dmaConfig->enHalfDestCallback == true)
        {
            EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_HALF_MAJOR_LOOP_INT, true);
        }
        else
        {
            EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_HALF_MAJOR_LOOP_INT, false);
        }
    }
    else
    {
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_ERR_INT, false);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_HALF_MAJOR_LOOP_INT, false);
        EDMA_DRV_ConfigureInterrupt(dmaConfig->dmaChan, EDMA_CHN_MAJOR_LOOP_INT, false);
    }

    status = EDMA_DRV_StartChannel(dmaConfig->dmaChan);
    (void)status;
}

#if FEATURE_DECFILTER_HAS_COMBINED_ZIR

static void DECFILTER_ConfigSIU_ZSEL_HSEL(const uint32_t instance,
                                          const decfilter_integrator_config_t * const integrator)
{
    const uint32_t shift = (SIU_DECFIL1_ZSELA_WIDTH + SIU_DECFIL1_HSELA_WIDTH) * \
                           (instance % NUM_INST_PER_DECFIL_xSEL);
    volatile uint32_t * decfil_addr;
    if (instance < NUM_INST_PER_DECFIL_xSEL)
    {   /* instances A, B, C, D */
        decfil_addr = &(SIU->DECFIL1);
    }
    else if (instance < (NUM_INST_PER_DECFIL_xSEL * 2u))
    {   /* instances E, F, G, H */
        decfil_addr = &(SIU->DECFIL2);
    }
    else
    {   /* instances I, J, K, L */
        decfil_addr = &(SIU->DECFIL3);
    }
    *decfil_addr &= ~((SIU_DECFIL1_ZSELA_MASK | SIU_DECFIL1_HSELA_MASK) << shift);
    if ((integrator->zeroControlMode != DECFILTER_INTEGRATOR_ZERO_DISABLE) || \
        (integrator->enabled != DECFILTER_INTEGRATOR_CONTROL_DISABLE) || \
        (integrator->outputReadRequestMode != DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_DISABLE))
    {
        *decfil_addr |= SIU_DECFIL1_ZSELA(integrator->zirSelection) << shift;
    }
    if(integrator->haltControl != DECFILTER_INTEGRATOR_HALT_CONTROL_DISABLE)
    {
        *decfil_addr |= SIU_DECFIL1_HSELA(integrator->haltSelection) << shift;
    }
}

static void DECFILTER_ConfigSIU_SRC(const uint32_t instance,
                                    const uint8_t triggerSelection)
{
    uint32_t shift = (SIU_DECFIL4_TRIG_SRCA_WIDTH) * \
                       (instance % NUM_INST_PER_DECFIL_SRC);
    volatile uint32_t * decfil_addr;
    if (instance < NUM_INST_PER_DECFIL_SRC)
    {   /* instances A, B, C, D, E, F, G, H */
        decfil_addr = &(SIU->DECFIL4);
    }
    else
    {   /* instances I, J, K, L */
        decfil_addr = &(SIU->DECFIL5);
        shift += SIU_DECFIL5_TRIG_SRCI_SHIFT; /* in DECFIL5, bitfields are in the upper half */
    }
    *decfil_addr &= ~(((uint32_t)SIU_DECFIL4_TRIG_SRCA_MASK) << shift);
    *decfil_addr |= SIU_DECFIL4_TRIG_SRCA(triggerSelection) << shift;
}

#else

static void DECFILTER_ConfigSIUL2(const uint32_t instance,
                                  const decfilter_integrator_config_t * const integrator)
{
    uint32_t mscr_offset = instance * DECFILTER_MSCR_OFFSET;
    SIUL2_IMCR_BASE[DECFILTER_MSCR_ENABLE_SELECTION + mscr_offset] = 0u;
    SIUL2_IMCR_BASE[DECFILTER_MSCR_HALT_SELECTION + mscr_offset] = 0u;
    SIUL2_IMCR_BASE[DECFILTER_MSCR_ZERO_SELECTION + mscr_offset] = 0u;
    SIUL2_IMCR_BASE[DECFILTER_MSCR_READ_SELECTION + mscr_offset] = 0u;
    if (integrator->zeroControlMode != DECFILTER_INTEGRATOR_ZERO_DISABLE)
    {
        SIUL2_IMCR_BASE[DECFILTER_MSCR_ZERO_SELECTION + mscr_offset] |= SIUL2_IMCR_SSS(integrator->zeroSelection);
    }
    if (integrator->outputReadRequestMode != DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_DISABLE)
    {
        SIUL2_IMCR_BASE[DECFILTER_MSCR_READ_SELECTION + mscr_offset] |= SIUL2_IMCR_SSS(integrator->readSelection);
    }
    if (integrator->enabled != DECFILTER_INTEGRATOR_CONTROL_DISABLE)
    {
        SIUL2_IMCR_BASE[DECFILTER_MSCR_ENABLE_SELECTION + mscr_offset] |= SIUL2_IMCR_SSS(integrator->enableSelection);
    }
    if (integrator->haltControl != DECFILTER_INTEGRATOR_HALT_CONTROL_DISABLE)
    {
        SIUL2_IMCR_BASE[DECFILTER_MSCR_HALT_SELECTION + mscr_offset] |= SIUL2_IMCR_SSS(integrator->haltSelection);
    }
}
#endif

/******************************************************************************
 * Code
 *****************************************************************************/

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_SoftReset
* Description   : decfilter soft-reset procedure
* The Soft-Reset command is requested through the self-negated bit SRES of the
* DECFILTER_MCR register and provides the CPU with the capability to initialize the
* Decimation Filter through the slave-bus interface.
*
* Implements    : DECFILTER_DRV_SoftReset_Activity
* END**************************************************************************/
status_t DECFILTER_DRV_SoftReset(const uint32_t instance, const uint32_t timeout)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];
    status_t status = STATUS_SUCCESS;
    uint32_t currentTime = 0u;

    /* Input Disable */
    DECFILTER_DRV_EnableInputState(instance, false);

    /* this is necessary to cover the case when a sample is left in the input buffer */
    while (((base->MSR & DECFILTER_MSR_BSY_MASK) != 0U) && (currentTime < timeout))
    {
        /* Wait for decfilter to be ready to precess new input data */
        currentTime++;
    }

    if(currentTime >= timeout)
    {
        status = STATUS_TIMEOUT;
    }
    else
    {
        /* It is recommended to clear the IBIE bit before a software reset */
        base->MCR &= ~DECFILTER_MCR_IBIE_MASK;

        /* set software reset */
        base->MCR |= DECFILTER_MCR_SRES_MASK;

        /* Enable the filter input */
        DECFILTER_DRV_EnableInputState(instance, true);
    }

    return status;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_Init
* Description   : Initializes DECFILTER module.
* This function Initializes the DECFILTER module with config values.
* This function should be called before calling any other DECFILTER driver function.
*
* Implements    : DECFILTER_DRV_Init_Activity
* END**************************************************************************/
status_t DECFILTER_DRV_Init(const uint32_t instance,
                        const decfilter_config_t * config)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    DEV_ASSERT(config != NULL);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];
    status_t status = STATUS_SUCCESS;

    uint32_t mcr = 0UL;
    uint32_t mxcr = 0UL;
    uint8_t i = 0U;

    /* Input Disable */
    DECFILTER_DRV_EnableInputState(instance, false);
    mcr |= DECFILTER_MCR_IDIS_MASK;

    switch (config->mode)
    {
#if FEATURE_DECFILTER_HAS_PSI
        case DECFILTER_MODE_NORMAL:
            break;
        case DECFILTER_MODE_PSI_INPUT_MIXED:
            mcr |= DECFILTER_MCR_MIXM_MASK;
            break;
        case DECFILTER_MODE_PSI_OUTPUT_MIXED:
            mcr |= (DECFILTER_MCR_ISEL_MASK | DECFILTER_MCR_MIXM_MASK);
            break;
        case DECFILTER_MODE_CASCADE_HEAD_PSI:
            mcr |= DECFILTER_MCR_CASCD(DECFILTER_CASCADE_MODE_HEAD);
            break;
        case DECFILTER_MODE_CASCADE_TAIL_PSI:
            mcr |= DECFILTER_MCR_CASCD(DECFILTER_CASCADE_MODE_TAIL);
            break;
#endif /* if FEATURE_DECFILTER_HAS_PSI */
        case DECFILTER_MODE_STANDALONE:
            mcr |= DECFILTER_MCR_ISEL_MASK;
            break;
        case DECFILTER_MODE_CASCADE_HEAD:
            mcr |= (DECFILTER_MCR_CASCD(DECFILTER_CASCADE_MODE_HEAD) | DECFILTER_MCR_ISEL_MASK);
            break;
        case DECFILTER_MODE_CASCADE_TAIL:
            mcr |= (DECFILTER_MCR_CASCD(DECFILTER_CASCADE_MODE_TAIL) | DECFILTER_MCR_ISEL_MASK);
            break;
        case DECFILTER_MODE_CASCADE_MIDDLE:
            /* selection of ISEL is ignored when it is configured as cascade middle */
            DEV_ASSERT(DECFILTER_INSTANCE_COUNT > 2U);
            mcr |= DECFILTER_MCR_CASCD(DECFILTER_CASCADE_MODE_MIDLE);
            break;
        case DECFILTER_MODE_FREEZE:
            mcr |= (DECFILTER_MCR_FRZ_MASK | DECFILTER_MCR_FREN_MASK);
            break;
        case DECFILTER_MODE_LOW_POWER:
            mcr |= DECFILTER_MCR_MDIS_MASK;
            break;

        default:
            /* Not supported */
            DEV_ASSERT(false);
            break;
    }

    /*  set filter type and scalling factor */
    mcr |= DECFILTER_MCR_FTYPE(config->typeFilter);
    mcr |= DECFILTER_MCR_SCAL(config->scaleFactor);

    /* setting all interrupts */
    mcr |= DECFILTER_MCR_IDEN(config->inputDataInterruptEnable ? (uint32_t)1UL : (uint32_t)0U) |
           DECFILTER_MCR_ODEN(config->outputDataInterruptEnable ? (uint32_t)1U : (uint32_t)0U) |
           DECFILTER_MCR_ERREN(config->errorInterruptEnable ? (uint32_t)1U : (uint32_t)0U) |
           DECFILTER_MCR_SDIE(config->integratorDataInterruptEnable ? (uint32_t)1U : (uint32_t)0U) |
           DECFILTER_MCR_IBIE(config->inputBufferInterruptRequestEnable ? (uint32_t)1U : (uint32_t)0U) |
           DECFILTER_MCR_OBIE(config->outputBufferInterruptRequestEnable ? (uint32_t)1U : (uint32_t)0U);

    /**** Configure DMA for transferring results from rfifo to system memory ****/
    if (config->dmaConfigInputBuffer != NULL)
    {
        DEV_ASSERT(config->inputBufferInterruptRequestEnable == false);
        DEV_ASSERT(config->outputBufferInterruptRequestEnable == false);
        mcr |= DECFILTER_MCR_DSEL_MASK;
        DECFILTER_ConfigInputDmaBuffer(instance, config->dmaConfigInputBuffer);
    }

    if (config->dmaConfigOutputBuffer != NULL)
    {
        DEV_ASSERT(config->inputBufferInterruptRequestEnable == false);
        DEV_ASSERT(config->outputBufferInterruptRequestEnable == false);
        mcr |= DECFILTER_MCR_DSEL_MASK;
        DECFILTER_ConfigOutputDmaBuffer(instance, config->dmaConfigOutputBuffer);
    }

#if FEATURE_DECFILTER_HAS_TRIGGER
    /* setting trigger enable and mode  */
    if (config->triggeredOutputResultEnable)
    {
        mcr |= DECFILTER_MCR_TORE_MASK;
        mcr |= DECFILTER_MCR_TMODE(config->triggerMode);
#if FEATURE_DECFILTER_HAS_COMBINED_ZIR
        DECFILTER_ConfigSIU_SRC(instance, config->triggerSelection);
#endif /* FEATURE_DECFILTER_HAS_COMBINED_ZIR */
    }
#endif /* FEATURE_DECFILTER_HAS_TRIGGER */

#if FEATURE_DECFILTER_ENHANCED_DEBUG
    mcr |= DECFILTER_MCR_EDME(config->enhancedDebugMonitor ? (uint32_t)0U : (uint32_t)1U);
#endif

    /* setting saturation enable and decimation rate  */
    mcr |= DECFILTER_MCR_SAT(config->saturationEnable ? (uint32_t)1U : (uint32_t)0U);
    mcr |= DECFILTER_MCR_DEC_RATE(config->decimationRateSelection);

    /* settings integrator enables and modes */
    mxcr |= DECFILTER_MXCR_SDMAE(config->integrator.dmaEnable ? (uint32_t)1U : (uint32_t)0U) |
            DECFILTER_MXCR_SSIG(config->integrator.signalFilter ? (uint32_t)1U : (uint32_t)0U) |
            DECFILTER_MXCR_SSAT(config->integrator.saturatedOperation ? (uint32_t)1U : (uint32_t)0U) |
            DECFILTER_MXCR_SCSAT(config->integrator.counterSaturatedOperation ? (uint32_t)1U : (uint32_t)0U) |
            DECFILTER_MXCR_SISEL(config->integrator.inputSelection ? (uint32_t)1U : (uint32_t)0U);
    mxcr |= DECFILTER_MXCR_SZROSEL(config->integrator.zeroControlMode);
    mxcr |= DECFILTER_MXCR_SRQSEL(config->integrator.outputReadRequestMode);
    mxcr |= DECFILTER_MXCR_SENSEL(config->integrator.enabled);
    mxcr |= DECFILTER_MXCR_SHLTSEL(config->integrator.haltControl);

#if FEATURE_DECFILTER_HAS_COMBINED_ZIR
    DECFILTER_ConfigSIU_ZSEL_HSEL(instance, &(config->integrator));
#else
    DECFILTER_ConfigSIUL2(instance, &(config->integrator));
#endif

    /* Input Disable */
    mcr |= DECFILTER_MCR_IDIS_MASK;

    /* program configuration registers*/
    base->MCR = mcr;
    base->MXCR = mxcr;

    /* write all filter coefficient registers DECFILTER_COEFn with the previously calculated values. */
    for (i = 0; i < DECFILTER_COEF_COUNT; i++)
    {
        base->COEF[i] = config->coefficients[i];
    }

    /* run a soft-reset cycle */
    status = DECFILTER_DRV_SoftReset(instance,config->timeout);

    /* the module is ready to receive data from the device slave-bus interface. */

    return status;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_Deinit
* Description   : De-initializes DECFILTER module.
*
* Implements    : DECFILTER_DRV_Deinit_Activity
* END**************************************************************************/
void DECFILTER_DRV_Deinit(const uint32_t instance)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];
    uint8_t i = 0;

    /* Disable the filter input */
    DECFILTER_DRV_EnableInputState(instance, false);

    base->MCR = 0x00;
    base->MXCR = 0x00;

    /* write all filter coefficient registers DECFILTER_COEFn with the previously calculated values. */
    for (i = 0; i < DECFILTER_COEF_COUNT; i++)
    {
        base->COEF[i] = 0x00;
    }

    for (i = 0u; i < DECFILTER_INSTANCE_COUNT; i++)
    {
        status_t status;

        /* Stop DMA virtual channel  */
        if (dmaInputChans[i] != DECFILTER_INVALID_DMA_CHAN)
        {
            status = EDMA_DRV_StopChannel(dmaInputChans[i]);
            (void)status;
            dmaInputChans[i] = DECFILTER_INVALID_DMA_CHAN;
        }

        if (dmaOutputChans[i] != DECFILTER_INVALID_DMA_CHAN)
        {
            status = EDMA_DRV_StopChannel(dmaOutputChans[i]);
            (void)status;
            dmaOutputChans[i] = DECFILTER_INVALID_DMA_CHAN;
        }
    }

    DECFILTER_DRV_ClearStatusFlags(instance, DECFILTER_STATUS_FLAG_ALL);
    DECFILTER_DRV_ClearIntegratorFlags(instance, DECFILTER_INTEGRATOR_FLAG_ALL);
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_EnableInputState
* Description   : This function enable or disables the block input
* Input disabling is needed to change the block configuration to or from cascade mode
* Implements    : DECFILTER_DRV_EnableInputState_Activity
* END**************************************************************************/
void DECFILTER_DRV_EnableInputState(const uint32_t instance,
                                    const bool state)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    if (state)
    {
        base->MCR &= ~DECFILTER_MCR_IDIS_MASK;
    }
    else
    {
        base->MCR |= DECFILTER_MCR_IDIS_MASK;
    }
}

/*FUNCTION**********************************************************************
* Function Name : DECFILTER_DRV_FreezeMode
* Description   : this function enable/disable freeze mode
*                 state=true - enable freeze mode
*                 state=false - enable freeze mode
*
* Implements    : DECFILTER_DRV_FreezeMode_Activity
* END**************************************************************************/
void DECFILTER_DRV_FreezeMode(const uint32_t instance,
                              const bool state)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    if (state)
    {
        base->MCR |= (DECFILTER_MCR_FRZ_MASK | DECFILTER_MCR_FREN_MASK);
    }
    else
    {
        base->MCR &= ~(DECFILTER_MCR_FRZ_MASK | DECFILTER_MCR_FREN_MASK);
    }
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_ClearStatusFlags
* Description   : This function clear the flags enabled in the bitmask for decfilter
*                 use only bitmasks from DECFILTER_STATUS_FLAG
*
* Implements    : DECFILTER_DRV_ClearStatusFlags_Activity
* END**************************************************************************/
void DECFILTER_DRV_ClearStatusFlags(const uint32_t instance,
                                    const uint32_t bitmask)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    DEV_ASSERT((bitmask & ~DECFILTER_STATUS_FLAG_ALL) == 0u);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];
    uint32_t tempBitmask = bitmask;

    /* ensure not exist DEC_COUNTER and BUSY flags */
    tempBitmask &= ~(DECFILTER_STATUS_FLAG_DEC_COUNTER & DECFILTER_STATUS_FLAG_IS_BUSY);

    base->MSR |= (uint32_t)(tempBitmask << 16);
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetStatusFlags
* Description   : This function shall return bitmask with all the status flags for decfilter
*                 use only bitmasks from DECFILTER_STATUS_FLAG
*
* Implements    : DECFILTER_DRV_GetStatusFlags_Activity
* END**************************************************************************/
uint32_t DECFILTER_DRV_GetStatusFlags(const uint32_t instance)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    return base->MSR;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_ClearIntegratorFlags
* Description   : This function shall clear the flags enabled in the bitmask for decfilter integrator
*                 use only bitmasks from DECFILTER_INTEGRATOR_FLAGS
*
* Implements    : DECFILTER_DRV_ClearIntegratorFlags_Activity
* END**************************************************************************/
void DECFILTER_DRV_ClearIntegratorFlags(const uint32_t instance,
                                        const uint32_t bitmask)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    DEV_ASSERT((bitmask & ~DECFILTER_INTEGRATOR_FLAG_ALL) == 0u);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    base->MXSR |= (uint32_t)(bitmask << 16);
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetIntegratorFlags
* Description   : This functions shall return bitmask with all the status flags for decfilter integrator
*
* Implements    : DECFILTER_DRV_GetIntegratorFlags_Activity
* END**************************************************************************/
uint32_t DECFILTER_DRV_GetIntegratorFlags(const uint32_t instance)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    return base->MXSR;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_SetIntegratorOutputMode
* Description   : This functions is used to command the update mode of the integrator output,
* reflected in the registers DECFILTER_FINTVAL and DECFILTER_FINTCNT.
* It may also cause a DMA or interrupt request, depending on the
* DECFILTER_MCR bit SDIE and DECFILTER_MXCR bit SDMAE.
* The SZRO bit is used to zero the integrator sum.
* If bits SRQ and SZRO are both written 1 at the same time, the integrator is reset only after the
* registers DECFILTER_FINTVAL and DECFILTER_FINTCNT are updated.
*
* Implements    : DECFILTER_DRV_SetIntegratorOutputMode_Activity
* END**************************************************************************/
void DECFILTER_DRV_SetIntegratorOutputMode(const uint32_t instance,
                                           const decfilter_integrator_output_operation_t outputOperation)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    switch (outputOperation)
    {
        case DECFILTER_INTEGRATOR_OUTPUT_UPDATE:
            base->MXCR |= DECFILTER_MXCR_SRQ_MASK;
            break;

        case DECFILTER_INTEGRATOR_OUTPUT_RESET:
            base->MXCR |= DECFILTER_MXCR_SZRO_MASK;
            break;

        case DECFILTER_INTEGRATOR_OUTPUT_RESET_SYNCED:
            base->MXCR |= DECFILTER_MXCR_SZRO_MASK | DECFILTER_MXCR_SRQ_MASK;
            break;

        case DECFILTER_INTEGRATOR_OUTPUT_RESET_ALL:
            base->MXCR |= DECFILTER_MXCR_SZRO_MASK;
            base->MXCR |= DECFILTER_MXCR_SRQ_MASK;
            break;

        default:
            /* Not supported */
            DEV_ASSERT(false);
            break;
    }
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetDefaultConfig
* Description   : This function gets the default configuration structure with default values
*
* Implements    : DECFILTER_DRV_GetDefaultConfig_Activity
* END**************************************************************************/
void DECFILTER_DRV_GetDefaultConfig(decfilter_config_t * config)
{
    /* Checks input parameter is not NULL */
    DEV_ASSERT(config != NULL);
    uint8_t i = 0;

    config->mode = DECFILTER_MODE_STANDALONE;
    config->typeFilter = DECFILTER_FILTER_TYPE_BYPASS;
    config->scaleFactor = DECFILTER_SCALING_FACTOR_1;

    config->inputDataInterruptEnable = false;
    config->outputDataInterruptEnable = false;
    config->errorInterruptEnable = false;

    config->integratorDataInterruptEnable = false;
    config->inputBufferInterruptRequestEnable = false;
    config->outputBufferInterruptRequestEnable = false;

    config->dmaConfigInputBuffer = NULL;
    config->dmaConfigOutputBuffer = NULL;

    config->saturationEnable = false;
    config->decimationRateSelection = 0x00;

#if FEATURE_DECFILTER_HAS_TRIGGER
    config->triggeredOutputResultEnable = false;
    config->triggerMode = DECFILTER_TRIGGER_MODE_RISING;
    config->triggerSelection = 0;
#endif

#if FEATURE_DECFILTER_ENHANCED_DEBUG
    config->enhancedDebugMonitor = false;
#endif

    config->integrator.dmaEnable = false;
    config->integrator.signalFilter = false;
    config->integrator.saturatedOperation = false;
    config->integrator.counterSaturatedOperation = false;
    config->integrator.inputSelection = false;
    config->integrator.zeroControlMode = DECFILTER_INTEGRATOR_ZERO_DISABLE;
    config->integrator.haltControl = DECFILTER_INTEGRATOR_HALT_CONTROL_DISABLE;
    config->integrator.outputReadRequestMode = DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_DISABLE;
    config->integrator.enabled = DECFILTER_INTEGRATOR_CONTROL_DISABLE;

    config->integrator.haltSelection = 0;
#if FEATURE_DECFILTER_HAS_COMBINED_ZIR
    config->integrator.zirSelection = 0;
#else
    config->integrator.zeroSelection = 0;
    config->integrator.readSelection = 0;
    config->integrator.enableSelection = 0;
#endif

    for (i = 0; i < DECFILTER_COEF_COUNT; i++)
    {
        config->coefficients[i] = 0x00;
    }

    config->timeout = UINT32_MAX;
}

/*FUNCTION**********************************************************************
*
* Function Name :  DECFILTER_DRV_DisableInterrupts
* Description   : This function disables the interrupt source for a timer channel.
* All set interrupt sources in the bitmask will keep their current state.
*                 use only bitmasks from DECFILTER_INTERRUPT
*
* Implements    : DECFILTER_DRV_DisableInterrupts_Activity
* END**************************************************************************/
void DECFILTER_DRV_DisableInterrupts(const uint32_t instance,
                                     const uint32_t bitmask)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    DEV_ASSERT((bitmask & ~DECFILTER_INTERRUPT_ALL) == 0u);

    DECFILTER_Type * base;

    base = s_decfilterBase[instance];

    base->MCR &= ~(bitmask);
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_EnableInterrupts
* Description   : enablethe interrupts for decfilter integrator, corresponding to the bits
*                 set in mask parameter.
*                 Use only bitmasks from DECFILTER_INTERRUPT
*
* Implements    : DECFILTER_DRV_EnableInterrupts_Activity
* END**************************************************************************/
void DECFILTER_DRV_EnableInterrupts(const uint32_t instance,
                                    const uint32_t bitmask)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    DEV_ASSERT((bitmask & ~DECFILTER_INTERRUPT_ALL) == 0u);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    base->MCR |= (bitmask);
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_WriteInputData
* Description   : is setting the input buffer and prefill or flush commands
*                 cmd can be set to NULL and corresponding to none command
* Implements    : DECFILTER_DRV_WriteInputData_Activity
* END**************************************************************************/
status_t DECFILTER_DRV_WriteInputData(const uint32_t instance,
                                      const uint32_t data,
                                      const decfilter_input_buffer_cmd_t * cmd)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    status_t result = STATUS_SUCCESS;

    if ((DECFILTER_DRV_GetStatusFlags(instance) & DECFILTER_STATUS_FLAG_IS_BUSY) > 0U)
    {
        result = STATUS_BUSY;
    }
    else
    {
        if ((DECFILTER_DRV_GetStatusFlags(instance) & DECFILTER_STATUS_FLAG_ERROR) > 0U)
        {
            result = STATUS_ERROR;
        }
        else
        {
            DECFILTER_Type * base;
            base = s_decfilterBase[instance];
            uint32_t tempIB = 0;

            tempIB |= DECFILTER_IB_INPBUF(data);

            if (cmd != NULL)
            {
                if (cmd->prefill)
                {
                    tempIB |= DECFILTER_IB_PREFILL_MASK;
                }

                if (cmd->flush)
                {
                    tempIB |= DECFILTER_IB_FLUSH_MASK;
                }

#if FEATURE_DECFILTER_HAS_PSI
                tempIB |= DECFILTER_IB_PSIOSEL(cmd->selectedPSI);
#endif
                tempIB |= DECFILTER_IB_INTAG(cmd->inTag);
            }

            base->IB = tempIB;
        }
    }

    return result;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_ReadOutputData
* Description   :  This function return the value output buffer
*
* Implements    : DECFILTER_DRV_ReadOutputData_Activity
* END**************************************************************************/
uint16_t DECFILTER_DRV_ReadOutputData(const uint32_t instance)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    uint32_t tempOB;
    base = s_decfilterBase[instance];

    tempOB = base->OB;
    tempOB = (tempOB & DECFILTER_OB_OUTBUF_MASK) >> DECFILTER_OB_OUTBUF_SHIFT;

    /* must clear output data flag after reading */
    DECFILTER_DRV_ClearStatusFlags(instance, DECFILTER_STATUS_FLAG_OUTPUT_DATA);

    return ((uint16_t)tempOB);
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_ReadOutputInfo
* Description   : This function return the value output buffer, also with outTag and psi bus number
*                 in separate variables in struct of decfilter_output_buffer_t
*
* Implements    : DECFILTER_DRV_ReadOutputInfo_Activity
* END**************************************************************************/
void DECFILTER_DRV_ReadOutputInfo(const uint32_t instance,
                                  decfilter_output_buffer_t* const data)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    uint32_t tempOB;
    base = s_decfilterBase[instance];

    tempOB = base->OB;
    data->data =(uint16_t)((tempOB & DECFILTER_OB_OUTBUF_MASK) >> DECFILTER_OB_OUTBUF_SHIFT);
    data->outTag = (uint8_t)((tempOB & DECFILTER_OB_OUTTAG_MASK) >> DECFILTER_OB_OUTTAG_SHIFT);
#if FEATURE_DECFILTER_HAS_PSI
    if( (tempOB & DECFILTER_OB_PSIOSEL_MASK) > 0u){
        data->selectedPSI = DECFILTER_SELECT_PSI_1;
    }
    else
    {
        data->selectedPSI = DECFILTER_SELECT_PSI_0;
    }
#endif

    /* must clear output data flag after reading */
    DECFILTER_DRV_ClearStatusFlags(instance, DECFILTER_STATUS_FLAG_OUTPUT_DATA);
}

#if FEATURE_DECFILTER_ENHANCED_DEBUG
/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetDebugEnhancedInputData
* Description   : return the value of EDID : Enhanced Debug Input Data
*
* Implements    : DECFILTER_DRV_GetDebugEnhancedInputData_Activity
* END**************************************************************************/
uint32_t DECFILTER_DRV_GetDebugEnhancedInputData(const uint32_t instance)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    return base->EDID;
}

#endif /* if FEATURE_DECFILTER_ENHANCED_DEBUG */

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetFinalValues
* Description   : return the value of FINTVAL : Final Integer Value Register
*                  and  FINTCNT : Final Integer Count Register
* Implements    : DECFILTER_DRV_GetFinalValues_Activity
* END**************************************************************************/
void DECFILTER_DRV_GetFinalValues(const uint32_t instance,
                                  decfilter_integrator_values_t * data)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    data->value = base->FINTVAL;
    data->count = base->FINTCNT;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetCurrentValues
* Description   : return the value of CINTVAL: Current Integer Value Register
*                  and  CINTCNT : Current Integer Count Register
* Implements    : DECFILTER_DRV_GetCurrentValues_Activity
* END**************************************************************************/
void DECFILTER_DRV_GetCurrentValues(const uint32_t instance,
                                    decfilter_integrator_values_t * data)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    data->value = base->CINTVAL;
    data->count = base->CINTCNT;
}

/*FUNCTION**********************************************************************
*
* Function Name : DECFILTER_DRV_GetFilterTap
* Description   : return the value of current tap register
* The TAP register stores, on each filter node, the input sample data the filter intermediary results.
* The Filter TAP registers providing additional debug capabilities to the Decimation Filter block.
* return value is fractional signed values in 2s complement format and is greater than or equal
* to -1 and less than 1
*
* Implements    : DECFILTER_DRV_GetFilterTap_Activity
* END**************************************************************************/
uint32_t DECFILTER_DRV_GetFilterTap(const uint32_t instance,
                                    const uint8_t index)
{
    DEV_ASSERT(instance < DECFILTER_INSTANCE_COUNT);
    DEV_ASSERT(index < DECFILTER_TAP_COUNT);

    DECFILTER_Type * base;
    base = s_decfilterBase[instance];

    return (base->TAP[index] & DECFILTER_TAP_TAPn_MASK) >> DECFILTER_TAP_TAPn_SHIFT;
}

/*******************************************************************************
 * EOF
 ******************************************************************************/
