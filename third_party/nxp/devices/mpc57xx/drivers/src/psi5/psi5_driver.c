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
 * when taking the address of the near auto variable. This is not an issue since the config structure
 * validation code does not return anything through those variables and destruction is desired.
 * 
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.7, External could be made static.
 * Function is defined for usage by application code.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.9, Could define variable at block scope.
 * The variables are ROM-able and are shared between different functions in the module.
 *
 * @section [global]
 * Violates MISRA 2012 Mandatory Rule 9.1, Symbol not initialized.
 * Used for immediate assignation, portable way of clearing a structure.
 */

#include <stddef.h>
#include "psi5_driver.h"
#include "status.h"
#include "psi5_irq.h"
#include "psi5_hw.h"

/*******************************************************************************
 * Validation macros (MIN - MAX) pairs
 ******************************************************************************/
#if defined(DEV_ERROR_DETECT)

/* Decoder disable offset */
#define PSI5_VALIDATE_MAX_DECODER_OFFSET      (127u)

/* Sync pulse width */
#define PSI5_VALIDATE_MAX_PULSE_WIDTH         (127u)

/* Rx buffer size */
#define PSI5_VALIDATE_MIN_BUFFER_SIZE         (1u)
#define PSI5_VALIDATE_MAX_BUFFER_SIZE         (32u)

/* Tx size */
#define PSI5_VALIDATE_MIN_TX_SIZE             (1u)
#define PSI5_VALIDATE_MAX_TX_SIZE             (64u)

/* Slot offsets */
#define PSI5_VALIDATE_MAX_SLOT_START_OFFSET   (32767u)

/* Slot duration */
#define PSI5_VALIDATE_MIN_SLOT_DURATION	      (58u)  /* 11 bits at 189kbps */
#define PSI5_VALIDATE_MAX_SLOT_DURATION	      (397u) /* 33 bits at 83.3kbps */

/* Slot data size */
#define PSI5_VALIDATE_MIN_SLOT_SIZE           (8u)
#define PSI5_VALIDATE_MAX_SLOT_SIZE           (28u)

#endif /* defined(DEV_ERROR_DETECT) */

/*******************************************************************************
 * Default data
 ******************************************************************************/

/*!
 * @brief Default slot configuration
 */
static const psi5_slot_config_t s_psi5DefaultSlotConfig =
{
    .startOffs = 0u,
    .slotLen = 190u,
    .slotId = 1u,
    .dataSize = 16u,
    .msbFirst = false,
    .hasSMC = false,
    .tsCapS0 = true,
    .hasParity = false
};

/*!
 * @brief Default channel configuration
 */
static const psi5_channel_config_t s_psi5DefaultChannelConfig =
{
    .slotConfig = NULL,
    .numOfConfigs = 1u,
    .channelId = 0u,
    .rxMode = PSI5_RX_MODE_ASYNCHRONOUS,
    .initialPulse = 0u,
    .targetPulse = 0u,
    .decoderOffset = 0u,
    .pulse0Width = 0u,
    .pulse1Width = 0u,
    .syncGlobal = false,
    .asyncReset = false,
    .syncState = PSI5_SYNC_STATE_1,
    .txMode = PSI5_TX_MODE_1,
    .txSize = 64u,
    .txDefault1 = true,
    .rxBufSize = 1u,
    .psi5UsesDma = false,
    .smcUsesDma = false,
    .psi5DmaChannel = 0u,
    .smcDmaChannel = 0u,
    .psi5DmaBuffer = NULL,
    .smcDmaBuffer = NULL
};

/*!
 * @brief Default instance configuration
 */
static const psi5_driver_user_config_t s_psi5DefaultInstanceConfig =
{
    .channelConfig = NULL,
    .numOfConfigs = 1u,
    .globalCtcEn = false,
    .callback =
    {
        .function = NULL,
        .param = NULL
    }
};

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief Internal state buffer
 */
static psi5_state_t * s_psi5StatePtr[PSI5_INSTANCE_COUNT];

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ResetRegisters
 * Description   : Reset registers to the default state.
 *
 *END**************************************************************************/
static void PSI5_ResetRegisters(const uint32_t instance)
{
    /* Disable the master instance */
    PSI5_HW_MasterGlobalEnable(false);

    /* Put all channels in configuration mode */
    for (uint32_t chIdx = 0u; chIdx < FEATURE_PSI5_CHANNEL_COUNT; chIdx++)
    {
        /* Enter configuration mode */
        PSI5_HW_EnterConfigMode(instance, chIdx);
    }

    /* Enable the master instance */
    PSI5_HW_MasterGlobalEnable(true);

    /* Clear all registers */
    PSI5_HW_ResetRegisters(instance);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_EnterConfigMode
 * Description   : Enter CONFIG mode.
 *
 *END**************************************************************************/
static void PSI5_EnterConfigMode(const uint32_t instance,
                                 const psi5_driver_user_config_t * configPtr)
{
    /* Disable the master instance */
    PSI5_HW_MasterGlobalEnable(false);

    /* Put all channels in configuration mode */
    for (uint32_t chIdx = 0u; chIdx < configPtr->numOfConfigs; chIdx++)
    {
        const psi5_channel_config_t * chCfg = &configPtr->channelConfig[chIdx];

        /* Enter configuration mode */
        PSI5_HW_EnterConfigMode(instance, chCfg->channelId);
    }

    /* Enable the master instance */
    PSI5_HW_MasterGlobalEnable(true);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_EnterNormalMode
 * Description   : Enter NORMAL mode.
 *
 *END**************************************************************************/
static void PSI5_EnterNormalMode(const uint32_t instance,
                                 const psi5_driver_user_config_t * configPtr)
{
    /* Put all channels in normal mode */
    for (uint32_t chIdx = 0u; chIdx < configPtr->numOfConfigs; chIdx++)
    {
        const psi5_channel_config_t * chCfg = &configPtr->channelConfig[chIdx];

        /* Enter normal mode */
        PSI5_HW_EnterNormalMode(instance, chCfg->channelId);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ConfigureSlots
 * Description   : Configures a single slot.
 *
 *END**************************************************************************/
static void PSI5_ConfigureSlots(const uint32_t instance,
                                const psi5_channel_config_t * chCfg)
{
    /* For each slot configuration */
    for (uint32_t slotIdx = 0u; slotIdx < chCfg->numOfConfigs; slotIdx++)
    {
        const psi5_slot_config_t * slotCfg = &chCfg->slotConfig[slotIdx];

        /* Configure current slot */
        PSI5_HW_ConfigureSlot(instance, chCfg->channelId, slotCfg);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ConfigureChannel
 * Description   : Configures a single channel.
 *
 *END**************************************************************************/
static void PSI5_ConfigureChannel(const uint32_t instance,
                                  const psi5_channel_config_t * chCfg)
{
    /* Configure Rx parameters */
    PSI5_HW_ConfigureRx(instance, chCfg);

    /* Configure DMA */
    PSI5_HW_ConfigureDma(instance, chCfg);

    /* Configure the Tx side */
    if (chCfg->rxMode == PSI5_RX_MODE_SYNCHRONOUS)
    {
        /* Configure Tx mode */
        PSI5_HW_ConfigureTx(instance, chCfg);

        /* Configure the pulse generator */
        PSI5_HW_ConfigurePulseGenerator(instance, chCfg);
    }

    /* Configure interrupts */
    PSI5_HW_EnableInterrupts(instance, chCfg);

    /* Enable interrupts */
    PSI5_IRQ_SetState(instance, chCfg->channelId, true);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ConfigureChannels
 * Description   : Configures all the channels.
 *
 *END**************************************************************************/
static void PSI5_ConfigureChannels(const uint32_t instance,
                                   const psi5_driver_user_config_t * configPtr)
{
    /* For each channel configuration */
    for (uint32_t chIdx = 0u; chIdx < configPtr->numOfConfigs; chIdx++)
    {
        const psi5_channel_config_t * chCfg = &configPtr->channelConfig[chIdx];

        /* Configure the pulse generator */
        PSI5_ConfigureChannel(instance, chCfg);

        /* Configure the slots */
        PSI5_ConfigureSlots(instance, chCfg);
    }

    /* Global Sync start */
    PSI5_HW_MasterGlobalCtc(configPtr->globalCtcEn);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_SaveState
 * Description   : Save internal state data.
 *
 *END**************************************************************************/
static void PSI5_SaveState(const uint32_t instance,
                           const psi5_driver_user_config_t * configPtr,
                           psi5_state_t * state)
{
    /* Save state */
    s_psi5StatePtr[instance] = state;

    /* Clear the state structure */
    *state = (const psi5_state_t){.callback = {NULL, NULL}};

    /* Save the instance Id */
    state->instanceId = (uint8_t)instance;

    /* Save the instance callback */
    state->callback = configPtr->callback;

    /* For each channel configuration */
    for (uint32_t chIdx = 0u; chIdx < configPtr->numOfConfigs; chIdx++)
    {
        const psi5_channel_config_t * chCfg = &configPtr->channelConfig[chIdx];
        const uint32_t channelId = chCfg->channelId;
        psi5_channel_state_t * chState = &state->chCfg[channelId];

        /* Channel uses custom length */
        chState->customTx = (chCfg->txMode == PSI5_TX_MODE_7);

        /* Channel has Tx */
        chState->txEnabled = (chCfg->syncState == PSI5_SYNC_STATE_2)
                          || (chCfg->syncState == PSI5_SYNC_STATE_4);

        /* DMA enablers */
        chState->psi5UsesDma = chCfg->psi5UsesDma;
        chState->smcUsesDma = chCfg->smcUsesDma;

        /* Activate channel */
        chState->channelActive = true;

        /* Slot states */
        for (uint32_t slotIdx = 0u; slotIdx < chCfg->numOfConfigs; slotIdx++)
        {
            const psi5_slot_config_t * slotCfg = &chCfg->slotConfig[slotIdx];
            const uint32_t slotId = (uint32_t)slotCfg->slotId - 1u;
            psi5_slot_state_t * slotState = &chState->slotCfg[slotId];

            /* MSB first */
            slotState->msbFirst = slotCfg->msbFirst;

            /* Bit count */
            slotState->dataSize = slotCfg->dataSize;

            /* Activate slot */
            slotState->slotActive = true;
        }
    }
}

#if defined(DEV_ERROR_DETECT)
/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ValidateSlotConfig
 * Description   : Validates the slot configuration structure.
 * Only active during debug.
 *
 *END**************************************************************************/
static void PSI5_ValidateSlotConfig(const psi5_slot_config_t * slotCfg,
                                    bool * slotAssign)
{
    /* Not a null slot */
    DEV_ASSERT(slotCfg != NULL);

    /* Slot number in range */
    DEV_ASSERT(slotCfg->slotId >= 1u);
    DEV_ASSERT(slotCfg->slotId <= FEATURE_PSI5_SLOT_COUNT);

    /* Check that is not assigned and mark as assigned */
    DEV_ASSERT(slotAssign[slotCfg->slotId - 1u] == false);
    slotAssign[slotCfg->slotId - 1u] = true;

    /* Slot start */
    DEV_ASSERT(slotCfg->startOffs <= PSI5_VALIDATE_MAX_SLOT_START_OFFSET);

    /* Slot duration */
    DEV_ASSERT(slotCfg->slotLen >= PSI5_VALIDATE_MIN_SLOT_DURATION);
    DEV_ASSERT(slotCfg->slotLen <= PSI5_VALIDATE_MAX_SLOT_DURATION);

    /* Slot data size */
    DEV_ASSERT(slotCfg->dataSize >= PSI5_VALIDATE_MIN_SLOT_SIZE);
    DEV_ASSERT(slotCfg->dataSize <= PSI5_VALIDATE_MAX_SLOT_SIZE);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ValidateChannelConfig
 * Description   : Validates the channel configuration structure.
 * Only active during debug.
 *
 *END**************************************************************************/
static void PSI5_ValidateChannelConfig(const psi5_channel_config_t * chCfg,
                                       bool * channelAssign)
{
    /* Not a null channel */
    DEV_ASSERT(chCfg != NULL);

    /* Channel number in range */
    DEV_ASSERT(chCfg->channelId < FEATURE_PSI5_CHANNEL_COUNT);

    /* Check that is not assigned and mark as assigned */
    DEV_ASSERT(channelAssign[chCfg->channelId] == false);
    channelAssign[chCfg->channelId] = true;

    /* Has at least one attached slot */
    DEV_ASSERT(chCfg->slotConfig != NULL);
    DEV_ASSERT(chCfg->numOfConfigs >= 1u);

    /* Slot assignment array */
    bool slotAssignment[FEATURE_PSI5_SLOT_COUNT] = {false};

    /* Check slots */
    for (uint32_t slotIdx = 0u; slotIdx < chCfg->numOfConfigs; slotIdx++)
    {
        const psi5_slot_config_t * slotCfg = &chCfg->slotConfig[slotIdx];

        /* Validate the slot */
        PSI5_ValidateSlotConfig(slotCfg, slotAssignment);
    }

    /* Rx mode */
    DEV_ASSERT((uint8_t)chCfg->rxMode <= (uint8_t)PSI5_RX_MODE_SYNCHRONOUS);

    /* Rx buffer */
    DEV_ASSERT(chCfg->rxBufSize >= PSI5_VALIDATE_MIN_BUFFER_SIZE);
    DEV_ASSERT(chCfg->rxBufSize <= PSI5_VALIDATE_MAX_BUFFER_SIZE);

    /* PSI5 DMA */
    if (chCfg->psi5UsesDma)
    {
        DEV_ASSERT(chCfg->psi5DmaBuffer != NULL);
    }

    /* SMC DMA */
    if (chCfg->smcUsesDma)
    {
        DEV_ASSERT(chCfg->smcDmaBuffer != NULL);
    }

    /* Only if async */
    if (chCfg->rxMode == PSI5_RX_MODE_SYNCHRONOUS)
    {
        /* Tx mode in range */
        DEV_ASSERT((uint8_t)chCfg->txMode <= (uint8_t)PSI5_TX_MODE_7);

        /* Decoder disable offset */
        DEV_ASSERT(chCfg->decoderOffset <= PSI5_VALIDATE_MAX_DECODER_OFFSET);

        /* Pulse 0 width */
        DEV_ASSERT(chCfg->pulse0Width <= PSI5_VALIDATE_MAX_PULSE_WIDTH);

        /* Pulse 1 width */
        DEV_ASSERT(chCfg->pulse1Width <= PSI5_VALIDATE_MAX_PULSE_WIDTH);

        /* Tx Length */
        DEV_ASSERT(chCfg->txSize >= PSI5_VALIDATE_MIN_TX_SIZE);
        DEV_ASSERT(chCfg->txSize <= PSI5_VALIDATE_MAX_TX_SIZE);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_ValidateConfigStructure
 * Description   : Validates the configuration structure.
 * Only active during debug.
 *
 *END**************************************************************************/
static void PSI5_ValidateConfigStructure(const psi5_driver_user_config_t * configPtr)
{
    /* Has at least one attached channel */
    DEV_ASSERT(configPtr->channelConfig != NULL);
    DEV_ASSERT(configPtr->numOfConfigs >= 1u);

    /* Channel assignment array */
    bool channelAssignment[FEATURE_PSI5_CHANNEL_COUNT] = {false};

    /* For each channel configuration */
    for (uint32_t chIdx = 0u; chIdx < configPtr->numOfConfigs; chIdx++)
    {
        const psi5_channel_config_t * chCfg = &configPtr->channelConfig[chIdx];

        /* Validate the channel */
        PSI5_ValidateChannelConfig(chCfg, channelAssignment);
    }
}
#endif /* defined(DEV_ERROR_DETECT) */

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_IRQ_Handler
 * Description   : Gets called from the low level handler
 * with instance and channel as parameter.
 *
 *END**************************************************************************/
void PSI5_IRQ_Handler(const uint32_t instance,
                      const uint32_t channel)
{
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);

    psi5_state_t * cState = s_psi5StatePtr[instance];

    if (cState != NULL)
    {
        /* Call function */
        if (cState->callback.function != NULL)
        {
            psi5_event_t ev = PSI5_HW_GetEvents(instance, channel, cState);

            cState->callback.function(instance, channel, ev, cState->callback.param);
        }
    }

    /* Clear all interrupt flags */
    PSI5_HW_ClearEvents(instance, channel);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_Init
 * Description   : Initializes the driver for a given peripheral
 * according to the given configuration structure.
 *
 * Implements    : PSI5_DRV_Init_Activity
 *END**************************************************************************/
status_t PSI5_DRV_Init(const uint32_t instance,
                       const psi5_driver_user_config_t * configPtr,
                       psi5_state_t * state)
{
    /* Will guard all subsequent HW accesses */
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(configPtr != NULL);
    DEV_ASSERT(state != NULL);

    status_t ret;

    if (s_psi5StatePtr[instance] == NULL)
    {
#if defined(DEV_ERROR_DETECT)

        /* Validate the configuration structure prior to initializing anything */
        PSI5_ValidateConfigStructure(configPtr);
        
#endif /* defined(DEV_ERROR_DETECT) */

        /* Update states */
        PSI5_SaveState(instance, configPtr, state);

        /* Enter configuration mode */
        PSI5_EnterConfigMode(instance, configPtr);

        /* Configure channels */
        PSI5_ConfigureChannels(instance, configPtr);

        /* Enter normal mode */
        PSI5_EnterNormalMode(instance, configPtr);

        ret = STATUS_SUCCESS;
    }
    else
    {
        /* Already initialized */
        ret = STATUS_ERROR;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_GetRawPsi5Frame
 * Description   : Returns the last received PSI5 frame.
 *
 * Implements    : PSI5_DRV_GetRawPsi5Frame_Activity
 *END**************************************************************************/
status_t PSI5_DRV_GetRawPsi5Frame(const uint32_t instance,
                                  const uint32_t channel,
                                  psi5_raw_frame_t * frame)
{
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);
    DEV_ASSERT(frame != NULL);
    DEV_ASSERT(s_psi5StatePtr[instance] != NULL);

    status_t ret;
    psi5_channel_state_t * chState = &s_psi5StatePtr[instance]->chCfg[channel];

    /* Check for active channel */
    if (chState->channelActive)
    {
        /* Buffer flags */
        uint32_t * flags = &chState->psi5PendingFlags;

        ret = PSI5_HW_GetRawPsi5Frame(instance, channel, flags, frame);
    }
    else
    {
        ret = STATUS_ERROR;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_GetRawSmcFrame
 * Description   : Returns the last received SMC frame.
 *
 * Implements    : PSI5_DRV_GetRawSmcFrame_Activity
 *END**************************************************************************/
status_t PSI5_DRV_GetRawSmcFrame(const uint32_t instance,
                                 const uint32_t channel,
                                 psi5_raw_frame_t * frame)
{
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);
    DEV_ASSERT(frame != NULL);
    DEV_ASSERT(s_psi5StatePtr[instance] != NULL);

    status_t ret;
    psi5_channel_state_t * chState = &s_psi5StatePtr[instance]->chCfg[channel];

    /* Check for active channel */
    if (chState->channelActive)
    {
        /* Buffer flags */
        uint8_t * flags = &chState->smcPendingFlags;

        ret = PSI5_HW_GetRawSmcFrame(instance, channel, flags, frame);
    }
    else
    {
        ret = STATUS_ERROR;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_ConvertPsi5Frame
 * Description   : Converts a RAW frame to a PSI5 structure.
 *
 * Implements    : PSI5_DRV_ConvertPsi5Frame_Activity
 *END**************************************************************************/
status_t PSI5_DRV_ConvertPsi5Frame(const uint32_t instance,
                                   const uint32_t channel,
                                   psi5_psi5_frame_t * frame,
                                   psi5_raw_frame_t * raw)
{
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);
    DEV_ASSERT(frame != NULL);
    DEV_ASSERT(raw != NULL);
    DEV_ASSERT(s_psi5StatePtr[instance] != NULL);

    const psi5_slot_state_t * slotStates = s_psi5StatePtr[instance]->chCfg[channel].slotCfg;

    PSI5_HW_ConvertRawPsi5Frame(frame, raw, slotStates);
    return STATUS_SUCCESS;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_ConvertSmcFrame
 * Description   : Converts a RAW frame to a SMC structure.
 *
 * Implements    : PSI5_DRV_ConvertSmcFrame_Activity
 *END**************************************************************************/
status_t PSI5_DRV_ConvertSmcFrame(const uint32_t instance,
                                  const uint32_t channel,
                                  psi5_smc_frame_t * frame,
                                  psi5_raw_frame_t * raw)
{
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);
    DEV_ASSERT(frame != NULL);
    DEV_ASSERT(raw != NULL);
    DEV_ASSERT(s_psi5StatePtr[instance] != NULL);

    const psi5_slot_state_t * slotStates = s_psi5StatePtr[instance]->chCfg[channel].slotCfg;

    PSI5_HW_ConvertRawSmcFrame(frame, raw, slotStates);
    return STATUS_SUCCESS;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_InstallCallback
 * Description   : Installs a new application callback.
 *
 * Implements    : PSI5_DRV_InstallCallback_Activity
 *END**************************************************************************/
status_t PSI5_DRV_InstallCallback(const uint32_t instance,
                                  psi5_callback_func_t function,
                                  void * param)
{
    /* Will guard all subsequent HW accesses */
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);

    status_t ret;

    if (s_psi5StatePtr[instance] != NULL)
    {
        /* Update callbacks */
        s_psi5StatePtr[instance]->callback.function = function;
        s_psi5StatePtr[instance]->callback.param = param;

        ret = STATUS_SUCCESS;
    }
    else
    {
        ret = STATUS_ERROR;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_DeInit
 * Description   : Stops the driver and resets the internal states.
 *
 * Implements    : PSI5_DRV_DeInit_Activity
 *END**************************************************************************/
status_t PSI5_DRV_DeInit(const uint32_t instance)
{
    /* Will guard all subsequent HW accesses */
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);

    status_t ret;

    if (s_psi5StatePtr[instance] != NULL)
    {
        /* Reset the instance */
        PSI5_ResetRegisters(instance);
        s_psi5StatePtr[instance] = NULL;

        ret = STATUS_SUCCESS;
    }
    else
    {
        ret = STATUS_ERROR;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_Transmit
 * Description   : Transmits a frame (standard or custom).
 *
 * Implements    : PSI5_DRV_Transmit_Activity
 *END**************************************************************************/
status_t PSI5_DRV_Transmit(const uint32_t instance,
                           const uint32_t channel,
                           const uint64_t data)
{
    /* Will guard all subsequent HW accesses */
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);

    status_t ret;

    const psi5_state_t * cState = s_psi5StatePtr[instance];

    if (cState != NULL)
    {
        /* Only if enabled */
        if (cState->chCfg[channel].txEnabled)
        {
            /* Check if ready for Tx */
            if (!PSI5_HW_IsDataRegisterReady(instance, channel, cState->chCfg[channel].customTx))
            {
                ret = STATUS_ERROR;
            }
            else
            {
                /* Write and trigger */
                PSI5_HW_WriteDataRegister(instance, channel, data, cState->chCfg[channel].customTx);

                ret = STATUS_SUCCESS;
            }
        }
        else
        {
            ret = STATUS_UNSUPPORTED;
        }
    }
    else
    {
        ret = STATUS_ERROR;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_IsTransmitReady
 * Description   : Returns the status of the transmission part of the driver.
 *
 * Implements    : PSI5_DRV_IsTransmitReady_Activity
 *END**************************************************************************/
bool PSI5_DRV_IsTransmitReady(const uint32_t instance,
                              const uint32_t channel)
{
    /* Will guard all subsequent HW accesses */
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);

    bool ret;

    const psi5_state_t * cState = s_psi5StatePtr[instance];

    if (cState != NULL)
    {
        /* Status */
        ret = PSI5_HW_IsDataRegisterReady(instance, channel, cState->chCfg[channel].customTx);
    }
    else
    {
        ret = false;
    }

    return ret;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_SetGlobalSync
 * Description   : Sets the global Pulse Generator state.
 *
 * Implements    : PSI5_DRV_SetGlobalSync_Activity
 *END**************************************************************************/
status_t PSI5_DRV_SetGlobalSync(const bool state)
{
    PSI5_HW_MasterGlobalCtc(state);
    return STATUS_SUCCESS;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_SetChannelSync
 * Description   : Sets the local (channel) Pulse Generator state.
 *
 * Implements    : PSI5_DRV_SetChannelSync_Activity
 *END**************************************************************************/
status_t PSI5_DRV_SetChannelSync(const uint32_t instance,
                                 const uint32_t channel,
                                 const bool state)
{
    DEV_ASSERT(instance < PSI5_INSTANCE_COUNT);
    DEV_ASSERT(channel < FEATURE_PSI5_CHANNEL_COUNT);

    /* Enable the local CTC */
    PSI5_HW_LocalChannelCtc(instance, channel, state);

    return STATUS_SUCCESS;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_DRV_GetDefaultConfig
 * Description   : Returns a working configuration for a PSI5-A16CRC-500_1L sensor.
 *
 * Implements    : PSI5_DRV_GetDefaultConfig_Activity
 *END**************************************************************************/
status_t PSI5_DRV_GetDefaultConfig(psi5_driver_user_config_t * configPtr,
                                   psi5_channel_config_t * chPtr,
                                   psi5_slot_config_t * slotPtr)
{
    DEV_ASSERT(configPtr != NULL);
    DEV_ASSERT(chPtr != NULL);
    DEV_ASSERT(slotPtr != NULL);


    /* Separate variables since the main structures point to const data */
    *configPtr = s_psi5DefaultInstanceConfig;
    *chPtr = s_psi5DefaultChannelConfig;
    *slotPtr = s_psi5DefaultSlotConfig;
    configPtr->channelConfig = chPtr;
    chPtr->slotConfig = slotPtr;

    return STATUS_SUCCESS;
}
