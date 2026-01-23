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
 * @page misra_violations MISRA-C:2012 violations
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 1.3, Taking address of near auto variable.
 * The code is not dynamically linked. An absolute stack address is obtained
 * when taking the address of the near auto variable. This is not an issue since the DMA
 * API does not return anything through those variables and destruction is desired.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 11.4, Conversion between a pointer and integer type.
 * The cast is required to initialize base pointers with unsigned integer values.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 11.6, Cast from unsigned int to pointer.
 * The cast is required to initialize a pointer with an unsigned long define,
 * representing an address.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 18.1, Possible creation of out-of-bounds pointer.
 * The cast is required to declare a series of 6 registers as an array. This is a limitation
 * of the header file, and is used to simplify indexed access.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.13, Could be declared as pointing to const.
 * Function prototype dependent on external function pointer typedef.
 */

#include <stddef.h>
#include "device_registers.h"
#include "psi5_driver.h"
#include "status.h"
#include "edma_driver.h"
#include "interrupt_manager.h"
#include "psi5_irq.h"
#include "psi5_hw.h"

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * Instance base addresses
 */
static const PSI5_MemMapPtr s_psi5BaseAddresses[PSI5_INSTANCE_COUNT] = PSI5_BASE_PTRS;

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_CountTraillingZeros
 * Description   : Counts trailing zeros in a given value
 *
 *END**************************************************************************/
static uint32_t PSI5_CountTrailingZeros(const uint32_t value)
{
    uint32_t count = 32u;

    if (value != 0u)
    {
        /* At least 31 */
        count--;

        /* We remove all bits but last (due to complement) */
        uint32_t lval = value & (uint32_t)(-((int32_t)value));

        /* Subsequent subtraction */
        if ((lval & 0x0000FFFFu) != 0u) { count -= 16u; }
        if ((lval & 0x00FF00FFu) != 0u) { count -= 8u; }
        if ((lval & 0x0F0F0F0Fu) != 0u) { count -= 4u; }
        if ((lval & 0x33333333u) != 0u) { count -= 2u; }
        if ((lval & 0x55555555u) != 0u) { count -= 1u; }
    }

    return count;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_MirrorBits
 * Description   : Reverse (mirror) 32 bits
 *
 *END**************************************************************************/
static uint32_t PSI5_MirrorBits(const uint32_t value)
{
    uint32_t lval = value;

    /* Swap 2 - by - 2, no conditionals */
    lval = ((lval & 0x55555555u) << 1u) | ((lval & 0xAAAAAAAAu) >> 1u);
    lval = ((lval & 0x33333333u) << 2u) | ((lval & 0xCCCCCCCCu) >> 2u);
    lval = ((lval & 0x0F0F0F0Fu) << 4u) | ((lval & 0xF0F0F0F0u) >> 4u);
    lval = ((lval & 0x00FF00FFu) << 8u) | ((lval & 0xFF00FF00u) >> 8u);
    lval = ((lval & 0x0000FFFFu) << 16u) | ((lval & 0xFFFF0000u) >> 16u);

    return lval;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_GetTxEvents
 * Description   : Returns active Tx events
 *
 *END**************************************************************************/
static psi5_event_t PSI5_HW_GetTxEvents(const uint32_t instance,
                                        const uint32_t channel,
                                        const psi5_state_t * state)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    psi5_event_t ev = 0u;

    /* Data shift overwrite */
    if (state->chCfg[channel].txEnabled)
    {
        uint32_t txMaskOvr = state->chCfg[channel].customTx ? PSI5_CH0_GISR_IS_DSROW_MASK : PSI5_CH0_GISR_IS_PROW_MASK;
        uint32_t txMaskRdy = state->chCfg[channel].customTx ? PSI5_CH0_GISR_DSR_RDY_MASK : PSI5_CH0_GISR_DPR_RDY_MASK;

        /* Data prep overwrite */
        if ((base->CH0_GISR & txMaskOvr) != 0u)
        {
            ev |= PSI5_EV_TX_DATA_OVR;
        }

        /* Data prep ready */
        if ((base->CH0_GISR & txMaskRdy) != 0u)
        {
            ev |= PSI5_EV_TX_DATA_RDY;
        }
    }

    return ev;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_GetSmcEvents
 * Description   : Returns active SMC events
 *
 *END**************************************************************************/
static psi5_event_t PSI5_HW_GetSmcEvents(const uint32_t instance,
                                        const uint32_t channel,
                                        psi5_state_t * state)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    psi5_event_t ev = 0u;

    /* SMC DMA / Normal */
    if (state->chCfg[channel].smcUsesDma)
    {
        /* Ready */
        if ((base->CH0_DSR & PSI5_CH0_DSR_IS_DMA_TF_SF_MASK) != 0u)
        {
            ev |= PSI5_EV_SMC_DMA_COMPLETE;
        }

        /* Underflow */
        if ((base->CH0_DSR & PSI5_CH0_DSR_IS_DMA_SFUF_MASK) != 0u)
        {
            ev |= PSI5_EV_SMC_DMA_UF;
        }
    }

    /* SMC Rx Complete */
    if ((base->CH0_GISR & PSI5_CH0_GISR_IS_NVSM_MASK) != 0u)
    {
        /* Mark event */
        ev |= PSI5_EV_SMC_RX;

        /* Update flags */
        state->chCfg[channel].smcPendingFlags |= (uint8_t)((base->CH0_GISR & PSI5_CH0_GISR_IS_NVSM_MASK) >> PSI5_CH0_GISR_IS_NVSM_SHIFT);
    }

    /* SMC Overwrite */
    if ((base->CH0_GISR & PSI5_CH0_GISR_IS_OWSM_MASK) != 0u)
    {
        ev |= PSI5_EV_SMC_OVR;
    }

    /* SMC error */
    if ((base->CH0_GISR & PSI5_CH0_GISR_IS_CESM_MASK) != 0u)
    {
        ev |= PSI5_EV_SMC_ERR;
    }

    return ev;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_GetPsi5Events
 * Description   : Returns active PSI5 events
 *
 *END**************************************************************************/
static psi5_event_t PSI5_HW_GetPsi5Events(const uint32_t instance,
                                          const uint32_t channel,
                                          psi5_state_t * state)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    psi5_event_t ev = 0u;

    /* PSI5 DMA */
    if (state->chCfg[channel].psi5UsesDma)
    {
        /* Ready */
        if ((base->CH0_DSR & PSI5_CH0_DSR_IS_DMA_TF_PM_DS_MASK) != 0u)
        {
            ev |= PSI5_EV_PSI5_DMA_COMPLETE;
        }

        /* Underflow */
        if ((base->CH0_DSR & PSI5_CH0_DSR_IS_DMA_PM_DS_UF_MASK) != 0u)
        {
            ev |= PSI5_EV_PSI5_DMA_UF;
        }

        /* Overflow */
        if ((base->CH0_DSR & PSI5_CH0_DSR_IS_DMA_PM_DS_FIFO_FULL_MASK) != 0u)
        {
            ev |= PSI5_EV_PSI5_DMA_OVF;
        }
    }

    /* PSI5 Rx Complete */
    if (base->CH0_NDSR != 0u)
    {
        /* Mark event */
        ev |= PSI5_EV_PSI5_RX;

        /* Update buffer flags */
        state->chCfg[channel].psi5PendingFlags |= base->CH0_NDSR;
    }

    /* PSI5 Overwrite */
    if (base->CH0_OWSR != 0u)
    {
        ev |= PSI5_EV_PSI5_OVR;
    }

    /* PSI5 error */
    if (base->CH0_EISR != 0u)
    {
        ev |= PSI5_EV_PSI5_ERR;
    }

    /* Global pending overflow */

    return ev;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_GetEvents
 * Description   : Returns active events
 *
 *END**************************************************************************/
psi5_event_t PSI5_HW_GetEvents(const uint32_t instance,
                               const uint32_t channel,
                               psi5_state_t * state)
{
    psi5_event_t ev;

    /* Tx events */
    ev = PSI5_HW_GetTxEvents(instance, channel, state);

    /* SMC events */
    ev |= PSI5_HW_GetSmcEvents(instance, channel, state);

    /* PSI5 events */
    ev |= PSI5_HW_GetPsi5Events(instance, channel, state);

    return ev;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ClearEvents
 * Description   : Clears all active events
 *
 *END**************************************************************************/
void PSI5_HW_ClearEvents(const uint32_t instance,
                         const uint32_t channel)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    /* DMA flags */
    base->CH0_DSR = (PSI5_CH0_DSR_IS_DMA_TF_SF_MASK | PSI5_CH0_DSR_IS_DMA_TF_PM_DS_MASK
                   | PSI5_CH0_DSR_IS_DMA_PM_DS_FIFO_FULL_MASK | PSI5_CH0_DSR_IS_DMA_SFUF_MASK
                   | PSI5_CH0_DSR_IS_DMA_PM_DS_UF_MASK);

    /* General flags */
    base->CH0_GISR = (PSI5_CH0_GISR_IS_CESM_MASK | PSI5_CH0_GISR_IS_STS_MASK
                    | PSI5_CH0_GISR_IS_DTS_MASK | PSI5_CH0_GISR_IS_DSROW_MASK
                    | PSI5_CH0_GISR_IS_BROW_MASK | PSI5_CH0_GISR_IS_PROW_MASK
                    | PSI5_CH0_GISR_IS_OWSM_MASK | PSI5_CH0_GISR_IS_NVSM_MASK);

    /* PSI5 status */
    base->CH0_NDSR = PSI5_CH0_NDSR_NDS_MASK;
    base->CH0_OWSR = PSI5_CH0_OWSR_OWS_MASK;
    base->CH0_EISR = PSI5_CH0_EISR_ERROR_MASK;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_GetRawPsi5Frame
 * Description   : Returns a raw PSI5 frame
 *
 *END**************************************************************************/
status_t PSI5_HW_GetRawPsi5Frame(const uint32_t instance,
                                 const uint32_t channel,
                                 uint32_t * bufferFlags,
                                 psi5_raw_frame_t * raw)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];
    status_t retVal;

    (void)channel; /* Reserved for future use */

    /* Update (in case we were not updated inside the interrupt handler) */
    *bufferFlags |= base->CH0_NDSR;

    /* Find location that triggered */
    uint32_t loc = PSI5_CountTrailingZeros(*bufferFlags);

    if (loc < FEATURE_PSI5_FIFO_COUNT)
    {
        /* Read message */
        (*raw)[0] = base->CH0_PMR[loc].CH0_PMRL;
        (*raw)[1] = base->CH0_PMR[loc].CH0_PMRH;

        /* Clear location */
        *bufferFlags &= (uint32_t)(~((uint32_t)1u << loc));

        /* Clear register flags also (in case they were not cleared inside the IRQ) */
        base->CH0_NDSR = (uint32_t)((uint32_t)1u << loc);
        base->CH0_OWSR = (uint32_t)((uint32_t)1u << loc);
        base->CH0_EISR = (uint32_t)((uint32_t)1u << loc);

        retVal = STATUS_SUCCESS;
    }
    else
    {
        retVal = STATUS_ERROR;
    }

    return retVal;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_GetRawSmcFrame
 * Description   : Returns a raw SMC frame
 *
 *END**************************************************************************/
status_t PSI5_HW_GetRawSmcFrame(const uint32_t instance,
                                const uint32_t channel,
                                uint8_t * bufferFlags,
                                psi5_raw_frame_t * raw)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];
    status_t retVal;

    (void)channel; /* Reserved for future use */

    /* Update (in case we were not updated inside the interrupt handler) */
    *bufferFlags |= (uint8_t)((base->CH0_GISR & PSI5_CH0_GISR_IS_NVSM_MASK) >> PSI5_CH0_GISR_IS_NVSM_SHIFT);

    /* Retrieve the slot */
    uint32_t loc = PSI5_CountTrailingZeros((uint32_t)(*bufferFlags));

    if (loc < FEATURE_PSI5_SLOT_COUNT)
    {
        /* Read message (only 32 bits in lower data buffer) */
        (*raw)[0] = base->CH0_SFR[loc];

        /* Clear location */
        *bufferFlags &= (uint8_t)(~((uint8_t)1u << loc));

        /* Clear register flags also (in case they were not cleared inside the IRQ) */
        base->CH0_GISR = (PSI5_CH0_GISR_IS_NVSM((uint32_t)1u << loc)
                        | PSI5_CH0_GISR_IS_OWSM((uint32_t)1u << loc)
                        | PSI5_CH0_GISR_IS_CESM((uint32_t)1u << loc));

        retVal = STATUS_SUCCESS;
    }
    else
    {
        retVal = STATUS_ERROR;
    }

    return retVal;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConvertRawPsi5Frame
 * Description   : Converts a raw PSI5 frame
 *
 *END**************************************************************************/
void PSI5_HW_ConvertRawPsi5Frame(psi5_psi5_frame_t * frame,
                                 psi5_raw_frame_t * raw,
                                 const psi5_slot_state_t * states)
{
    frame->CRC = (uint8_t)(((*raw)[0] & PSI5_CH0_PMRRL_CRC_MASK) >> PSI5_CH0_PMRRL_CRC_SHIFT);
    frame->C = (uint8_t)(((*raw)[0] & PSI5_CH0_PMRRL_C_MASK) >> PSI5_CH0_PMRRL_C_SHIFT);
    frame->F = (uint8_t)(((*raw)[1] & PSI5_CH0_PMRRH_F_MASK) >> PSI5_CH0_PMRRH_F_SHIFT);
    frame->EM = (uint8_t)(((*raw)[1] & PSI5_CH0_PMRRH_EM_MASK) >> PSI5_CH0_PMRRH_EM_SHIFT);
    frame->E = (uint8_t)(((*raw)[1] & PSI5_CH0_PMRRH_E_MASK) >> PSI5_CH0_PMRRH_E_SHIFT);
    frame->T = (uint8_t)(((*raw)[1] & PSI5_CH0_PMRRH_T_MASK) >> PSI5_CH0_PMRRH_T_SHIFT);
    frame->SLOT_COUNTER = (uint8_t)(((*raw)[1] & PSI5_CH0_PMRRH_SlotCounter_MASK) >> PSI5_CH0_PMRRH_SlotCounter_SHIFT);
    frame->TIME_STAMP = (uint32_t)(((*raw)[1] & PSI5_CH0_PMRRH_TimeStampValue_MASK) >> PSI5_CH0_PMRRH_TimeStampValue_SHIFT);

    /* If slot counter is 0 or any other out of range value, we assign settings from slot 1 (index 0) */
    uint32_t slotIdx = (frame->SLOT_COUNTER < 1u) ?
                        0u : (((frame->SLOT_COUNTER > FEATURE_PSI5_SLOT_COUNT) ?
                        5u : ((uint32_t)frame->SLOT_COUNTER - 1u)));

    /* 28 bits max, left aligned */
    uint32_t dataRegion = (uint32_t)((*raw)[0] & PSI5_CH0_PMRRL_DATA_REGION_MASK);

    /* Number of normalization shifts */
    const uint32_t shiftCount = (32u - (uint32_t)states[slotIdx].dataSize);

    /* If MSB first */
    if (states[slotIdx].msbFirst)
    {
        /* As we received it */
        frame->DATA_REGION = dataRegion >> shiftCount;
    }
    else
    {
        /* Compute mask based on dataSize */
        const uint32_t dataMask = (uint32_t)0xFFFFFFFFu >> shiftCount;

        /* Reverse and mask */
        frame->DATA_REGION = PSI5_MirrorBits(dataRegion) & dataMask;
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConvertRawSmcFrame
 * Description   : Converts a raw SMC frame
 *
 *END**************************************************************************/
void PSI5_HW_ConvertRawSmcFrame(psi5_smc_frame_t * frame,
                                psi5_raw_frame_t * raw,
                                const psi5_slot_state_t * states)
{
    (void)states; /* Reserved for future implementation */

    frame->SLOT_NO = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_SLOT_NO_MASK) >> PSI5_CH0_SFR_SLOT_NO_SHIFT);
    frame->CER = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_CER_MASK) >> PSI5_CH0_SFR_CER_SHIFT);
    frame->OW = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_OW_MASK) >> PSI5_CH0_SFR_OW_SHIFT);
    frame->CRC = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_CRC_MASK) >> PSI5_CH0_SFR_CRC_SHIFT);
    frame->C = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_C_MASK) >> PSI5_CH0_SFR_C_SHIFT);
    frame->IDDATA = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_IDDATA_MASK) >> PSI5_CH0_SFR_IDDATA_SHIFT);

    /* Modify fields based on the C bit */
    if ((frame->C) != 0u)
    {
        frame->ID = (uint8_t)(((*raw)[0] & PSI5_CH0_SFR_ID_MASK) >> PSI5_CH0_SFR_ID_SHIFT);
        frame->DATA = (uint16_t)(((*raw)[0] & (PSI5_CH0_SFR_IDDATA_MASK | PSI5_CH0_SFR_DATA_MASK)) >> PSI5_CH0_SFR_DATA_SHIFT);
    }
    else
    {
        frame->ID = (uint8_t)(((*raw)[0] & (PSI5_CH0_SFR_ID_MASK | PSI5_CH0_SFR_IDDATA_MASK)) >> PSI5_CH0_SFR_IDDATA_SHIFT);
        frame->DATA = (uint16_t)(((*raw)[0] & PSI5_CH0_SFR_DATA_MASK) >> PSI5_CH0_SFR_DATA_SHIFT);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConfigureSlot
 * Description   : Configures a single slot
 *
 *END**************************************************************************/
void PSI5_HW_ConfigureSlot(const uint32_t instance,
                           const uint32_t channel,
                           const psi5_slot_config_t * slot)
{
    uint8_t slotIdx = (slot->slotId - 1u);

    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    /* Boundaries */
    ((volatile uint16_t *)(&base->CH0_S1SBR))[slotIdx] = PSI5_CH0_S1SBR_S1SBT(slot->startOffs);

    /* Configuration */
    base->CH0_SFCR[slotIdx] = (PSI5_CH0_SFCR_SLOT_EN(1u) | PSI5_CH0_SFCR_TS_CAPT((slot->tsCapS0 ? (uint32_t)0u : (uint32_t)1u))
                             | PSI5_CH0_SFCR_SMCL((slot->hasSMC ? (uint32_t)1u : (uint32_t)0u)) | PSI5_CH0_SFCR_DRL(slot->dataSize)
                             | PSI5_CH0_SFCR_CRCP((slot->hasParity ? (uint32_t)1u : (uint32_t)0u)));

    /* New time frame */
    uint32_t oldImage = (base->CH0_SnEBR & (uint32_t)PSI5_CH0_SnEBR_SnEBT_MASK) >> PSI5_CH0_SnEBR_SnEBT_SHIFT;
    uint32_t newImage = PSI5_CH0_SnEBR_SnEBT((uint32_t)((uint32_t)slot->startOffs + (uint32_t)slot->slotLen));

    /* Update */
    if (newImage > oldImage)
    {
        base->CH0_SnEBR = newImage | PSI5_CH0_SnEBR_SLOT_NO(slot->slotId);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_EnterConfigMode
 * Description   : Puts the channel in configuration mode
 *
 *END**************************************************************************/
void PSI5_HW_EnterConfigMode(const uint32_t instance,
                             const uint32_t channel)
{
    (void)channel; /* Reserved for future use */

    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* Configure channel */
    base->CH0_PCCR |= (PSI5_CH0_PCCR_PSI5_CH_EN_MASK | PSI5_CH0_PCCR_PSI5_CH_CONFIG_MASK);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_EnterNormalMode
 * Description   : Puts the channel in normal mode
 *
 *END**************************************************************************/
void PSI5_HW_EnterNormalMode(const uint32_t instance,
                             const uint32_t channel)
{
    (void)channel; /* Reserved for future use */

    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* Configure channel */
    base->CH0_PCCR &= ~PSI5_CH0_PCCR_PSI5_CH_CONFIG_MASK;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConfigureTx
 * Description   : Configures transmission
 *
 *END**************************************************************************/
void PSI5_HW_ConfigureTx(const uint32_t instance,
                         const psi5_channel_config_t * chCfg)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* Set mode to synchronous */
    base->CH0_PCCR |= PSI5_CH0_PCCR_MODE_MASK;

    /* Tx mode, Data length, default bit values, auto transfer from DBR to DSR */
    base->CH0_DOBCR |= (PSI5_CH0_DOBCR_CMD_TYPE(chCfg->txMode) | PSI5_CH0_DOBCR_DATA_LENGTH((uint16_t)((uint16_t)chCfg->txSize - 1u))
                      | PSI5_CH0_DOBCR_DEFAULT_SYNC((chCfg->txDefault1 ? (uint16_t)1u : (uint16_t)0u))
                      | PSI5_CH0_DOBCR_SW_READY_MASK);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConfigureRx
 * Description   : Configures reception
 *
 *END**************************************************************************/
void PSI5_HW_ConfigureRx(const uint32_t instance,
                         const psi5_channel_config_t * chCfg)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* Buffer locations */
    base->CH0_PCCR |= PSI5_CH0_PCCR_MEM_DEPTH((uint32_t)((uint32_t)chCfg->rxBufSize - 1u));

    /* Fast clearing for both */
    base->CH0_PCCR |= PSI5_CH0_PCCR_FAST_CLR_PSI5_MASK | PSI5_CH0_PCCR_FAST_CLR_SMC_MASK;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConfigurePulseGenerator
 * Description   : Configures pulse generation
 *
 *END**************************************************************************/
void PSI5_HW_ConfigurePulseGenerator(const uint32_t instance,
                                     const psi5_channel_config_t * chCfg)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* CTC control */
    if (chCfg->syncGlobal)
    {
        base->CH0_PCCR |= PSI5_CH0_PCCR_CTC_GED_SEL_MASK;
    }

    /* GTM reset */
    if (chCfg->asyncReset)
    {
        base->CH0_PCCR |= PSI5_CH0_PCCR_GTM_RESET_ASYNC_EN_MASK;
    }

    /* Decoder offset */
    base->CH0_MDDIS_OFF = chCfg->decoderOffset;

    /* Sync states */
    if (((uint8_t)chCfg->syncState & 4u) != 0u)
    {
        base->CH0_DOBCR |= PSI5_CH0_DOBCR_GTM_TRIG_SEL_MASK;
    }

    if (((uint8_t)chCfg->syncState & 2u) != 0u)
    {
        base->CH0_DOBCR |= PSI5_CH0_DOBCR_SP_PULSE_SEL_MASK;
    }

    if (((uint8_t)chCfg->syncState & 1u) != 0u)
    {
        base->CH0_DOBCR |= PSI5_CH0_DOBCR_OP_SEL_MASK;
    }

    /* Pulse forming*/
    base->CH0_PW0D = chCfg->pulse0Width;
    base->CH0_PW1D = chCfg->pulse1Width;
    base->CH0_CIPR = chCfg->initialPulse;
    base->CH0_CTPR = chCfg->targetPulse;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ConfigureDma
 * Description   : Configures DMA
 *
 *END**************************************************************************/
void PSI5_HW_ConfigureDma(const uint32_t instance,
                          const psi5_channel_config_t * chCfg)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* Dma configuration structure */
    edma_transfer_config_t dmaConfig =
    {
        .srcAddr = 0u, /* Modified by function */
        .destAddr = 0u, /* Modified by function */
        .srcTransferSize = EDMA_TRANSFER_SIZE_4B,
        .destTransferSize = EDMA_TRANSFER_SIZE_4B,
        .srcOffset = 0,
        .destOffset = (int16_t)sizeof(uint32_t),
        .srcLastAddrAdjust = 0,
        .destLastAddrAdjust = 0, /* Modified by function */
        .srcModulo = EDMA_MODULO_OFF,
        .destModulo = EDMA_MODULO_OFF,
        .minorByteTransferCount = 0u, /* Modified by function */
        .scatterGatherEnable = false,
        .scatterGatherNextDescAddr = 0u,
        .interruptEnable = false,
        .loopTransferConfig = &(edma_loop_transfer_config_t)
        {
            .majorLoopIterationCount = 1u,
            .srcOffsetEnable = false,
            .dstOffsetEnable = true,
            .minorLoopOffset = 0,
            .minorLoopChnLinkEnable = false,
            .minorLoopChnLinkNumber = 0u,
            .majorLoopChnLinkEnable = false,
            .majorLoopChnLinkNumber = 0u
        }
    };

    /* Pre - computed */
    bool psi5DmaOk = chCfg->psi5UsesDma && (chCfg->psi5DmaBuffer != NULL);
    bool smcDmaOk = chCfg->smcUsesDma && (chCfg->smcDmaBuffer != NULL);

    /* Sizes and enablers */
    base->CH0_DCR |= (PSI5_CH0_DCR_DMA_PM_DS_WM((uint32_t)((uint32_t)chCfg->rxBufSize - 1u))
                   | (psi5DmaOk ? PSI5_CH0_DCR_DMA_PM_DS_CONFIG(2u) : 0u)
                   | (smcDmaOk ? (uint32_t)PSI5_CH0_DCR_DMA_EN_SF_MASK : 0u));

    /* DMA transfer */
    if (smcDmaOk)
    {
        /* Configure and start*/
        dmaConfig.srcAddr = (uint32_t)(&base->CH0_DSFR);
        dmaConfig.destAddr = (uint32_t)(chCfg->smcDmaBuffer);
        dmaConfig.destLastAddrAdjust = -(int32_t)sizeof(uint32_t);
        dmaConfig.minorByteTransferCount = (uint32_t)sizeof(uint32_t);
        (void)EDMA_DRV_ConfigLoopTransfer(chCfg->smcDmaChannel, &dmaConfig);
        (void)EDMA_DRV_StartChannel(chCfg->smcDmaChannel);
    }

    if (psi5DmaOk)
    {
        /* Configure and start */
        dmaConfig.srcAddr = (uint32_t)(&base->CH0_DPMR);
        dmaConfig.destAddr = (uint32_t)(chCfg->psi5DmaBuffer);
        dmaConfig.destLastAddrAdjust = -(int32_t)((int32_t)sizeof(psi5_raw_frame_t) * (int32_t)chCfg->rxBufSize);
        dmaConfig.minorByteTransferCount = (uint32_t)(sizeof(psi5_raw_frame_t) * chCfg->rxBufSize);
        (void)EDMA_DRV_ConfigLoopTransfer(chCfg->psi5DmaChannel, &dmaConfig);
        (void)EDMA_DRV_StartChannel(chCfg->psi5DmaChannel);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_MasterGlobalEnable
 * Description   : Global channel enabler
 *
 *END**************************************************************************/
void PSI5_HW_MasterGlobalEnable(const bool state)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[FEATURE_PSI5_MASTER_INSTANCE];

    base->GCR = state ? (base->GCR & ~PSI5_GCR_GLOBAL_DISABLE_REQ_MASK) : (base->GCR | PSI5_GCR_GLOBAL_DISABLE_REQ_MASK);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_MasterGlobalCtc
 * Description   : Global CTC (timer) enabler
 *
 *END**************************************************************************/
void PSI5_HW_MasterGlobalCtc(const bool state)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[FEATURE_PSI5_MASTER_INSTANCE];

    base->GCR = state ? (base->GCR | PSI5_GCR_CTC_GED_MASK) : (base->GCR & ~PSI5_GCR_CTC_GED_MASK);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_LocalChannelCtc
 * Description   : Local (channel) CTC (timer) enabler
 *
 *END**************************************************************************/
void PSI5_HW_LocalChannelCtc(const uint32_t instance,
                             const uint32_t channel,
                             const bool state)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    base->CH0_PCCR = state ? (base->CH0_PCCR | PSI5_CH0_PCCR_CTC_ED_MASK) : (base->CH0_PCCR & ~PSI5_CH0_PCCR_CTC_ED_MASK);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_ResetRegisters
 * Description   : Resets registers
 *
 *END**************************************************************************/
void PSI5_HW_ResetRegisters(const uint32_t instance)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* Control registers */
    base->CH0_DCR = 0u;
    base->CH0_GICR = 0u;
    base->CH0_NDICR = 0u;
    base->CH0_OWICR = 0u;
    base->CH0_EICR = 0u;

    /* Slot configuration */
    for (uint32_t slotIdx = 0u; slotIdx < FEATURE_PSI5_SLOT_COUNT; slotIdx++)
    {
        ((volatile uint16_t *)(&base->CH0_S1SBR))[slotIdx] = 0u;
        base->CH0_SFCR[slotIdx] = PSI5_CH0_SFCR_DRL(8u);
    }

    /* Boundary end */
    base->CH0_SnEBR = 0u;

    /* Tx block */
    base->CH0_DOBCR = 0u;
    base->CH0_MDDIS_OFF = 0u;
    base->CH0_PW0D = 0u;
    base->CH0_PW1D = 0u;
    base->CH0_CTPR = 0u;
    base->CH0_CIPR = 0u;

    /* Normal and disable */
    base->CH0_PCCR = (PSI5_CH0_PCCR_ERROR_SELECT0_MASK | PSI5_CH0_PCCR_ERROR_SELECT1_MASK
                    | PSI5_CH0_PCCR_ERROR_SELECT2_MASK | PSI5_CH0_PCCR_ERROR_SELECT3_MASK
                    | PSI5_CH0_PCCR_ERROR_SELECT4_MASK);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_WriteDataRegister
 * Description   : Writes Tx registers
 *
 *END**************************************************************************/
void PSI5_HW_WriteDataRegister(const uint32_t instance,
                               const uint32_t channel,
                               const uint64_t data,
                               const bool custom)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    (void)channel; /* Reserved for future use */

    /* Write and trigger */
    if (custom)
    {
        base->CH0_DSRH = (uint32_t)((data >> 32u) & PSI5_CH0_DSRH_DSR_MASK);
        base->CH0_DSRL = (uint32_t)(data & PSI5_CH0_DSRL_DSR_MASK);
        base->CH0_GISR |= PSI5_CH0_GISR_DSR_RDY_MASK;
    }
    else
    {
        base->CH0_DPRL = (uint32_t)(data & PSI5_CH0_DPRL_DPR_MASK);
        base->CH0_GISR |= PSI5_CH0_GISR_DPR_RDY_MASK;
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_IsDataRegisterReady
 * Description   : Returns Tx register status
 *
 *END**************************************************************************/
bool PSI5_HW_IsDataRegisterReady(const uint32_t instance,
                                 const uint32_t channel,
                                 const bool custom)
{
    const PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];
    bool ret;

    (void)channel; /* Reserved for future use */

    /* Depending on Tx type */
    if (custom)
    {
        ret = (base->CH0_GISR & PSI5_CH0_GISR_DSR_RDY_MASK) != 0u;
    }
    else
    {
        ret = (base->CH0_GISR & PSI5_CH0_GISR_DPR_RDY_MASK) != 0u;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_HW_EnableInterrupts
 * Description   : Interrupt enabler
 *
 *END**************************************************************************/
void PSI5_HW_EnableInterrupts(const uint32_t instance,
                              const psi5_channel_config_t * chCfg)
{
    PSI5_MemMapPtr base = s_psi5BaseAddresses[instance];

    /* We need to enable ALL interrupts */

    /* Error interrupts */
    base->CH0_PCCR |= (PSI5_CH0_PCCR_ERROR_SELECT0_MASK | PSI5_CH0_PCCR_ERROR_SELECT1_MASK
                     | PSI5_CH0_PCCR_ERROR_SELECT2_MASK | PSI5_CH0_PCCR_ERROR_SELECT3_MASK
                     | PSI5_CH0_PCCR_ERROR_SELECT4_MASK);

    /* DMA interrupts (Only if DMA enabled) */
    if (chCfg->psi5UsesDma)
    {
        base->CH0_DCR |= (PSI5_CH0_DCR_IE_DMA_TF_PM_DS_MASK | PSI5_CH0_DCR_IE_DMA_PM_DS_FIFO_FULL_MASK
                        | PSI5_CH0_DCR_IE_DMA_PM_DS_UF_MASK);
    }

    if (chCfg->smcUsesDma)
    {
        base->CH0_DCR |= (PSI5_CH0_DCR_IE_DMA_TF_SF_MASK | PSI5_CH0_DCR_IE_DMA_SFUF_MASK);
    }

    /* Tx interrupts */
    if ((chCfg->syncState == PSI5_SYNC_STATE_2) || (chCfg->syncState == PSI5_SYNC_STATE_4))
    {
        /* Data shift interrupts only for mode 7 */
        if (chCfg->txMode == PSI5_TX_MODE_7)
        {
            base->CH0_GICR |= (PSI5_CH0_GICR_IE_DSROW_MASK | PSI5_CH0_GICR_IE_DSRR_MASK);
        }
        else
        {
            base->CH0_GICR |= (PSI5_CH0_GICR_IE_PROW_MASK | PSI5_CH0_GICR_IE_PRR_MASK);
        }
    }

    /* PSI5 Rx interrupts */
    base->CH0_NDICR = PSI5_CH0_NDICR_IE_ND_MASK;
    base->CH0_OWICR = PSI5_CH0_OWICR_IE_OW_MASK;
    base->CH0_EICR = PSI5_CH0_EICR_IE_ERROR_MASK;
}

/*******************************************************************************
* EOF
*******************************************************************************/
