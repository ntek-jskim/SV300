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

#ifndef PSI5_DRIVER_HW_H
#define PSI5_DRIVER_HW_H

/* Required headers */
#include <stddef.h>
#include "device_registers.h"
#include "psi5_driver.h"

/*!
 * @addtogroup psi5
 * @{
 */

/*******************************************************************************
 * API
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @brief Getter for current instance and channel events
 *
 * Called from IRQ
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] state Current instance state
 * @return PSI5 events
 */
psi5_event_t PSI5_HW_GetEvents(const uint32_t instance,
                               const uint32_t channel,
                               psi5_state_t * state);

/*!
 * @brief Clears all active events
 *
 * Called from IRQ
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 */
void PSI5_HW_ClearEvents(const uint32_t instance,
                         const uint32_t channel);

/*!
 * @brief Getter for last received psi5 frame
 *
 * Called from IRQ
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] bufferFlags Buffer location flags
 * @param[out] raw Raw frame
 * @return STATUS_SUCCESS if frame found
 * @return STATUS_ERROR if no frame was returned by reference
 */
status_t PSI5_HW_GetRawPsi5Frame(const uint32_t instance,
                                 const uint32_t channel,
                                 uint32_t * bufferFlags,
                                 psi5_raw_frame_t * raw);

/*!
 * @brief Getter for last received smc frame
 *
 * Called from IRQ
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] bufferFlags Buffer location flags
 * @param[out] raw Raw frame
 * @return STATUS_SUCCESS if frame found
 * @return STATUS_ERROR if no frame was returned by reference
 */
status_t PSI5_HW_GetRawSmcFrame(const uint32_t instance,
                                const uint32_t channel,
                                uint8_t * bufferFlags,
                                psi5_raw_frame_t * raw);

/*!
 * @brief Converts a raw psi5 frame to a psi5 data structure
 *
 *
 * @param[in] frame PSI5 frame
 * @param[out] raw Raw frame
 * @param[in] states Pointer to the slot state array
 */
void PSI5_HW_ConvertRawPsi5Frame(psi5_psi5_frame_t * frame,
                                 psi5_raw_frame_t * raw,
                                 const psi5_slot_state_t * states);

/*!
 * @brief Converts a raw smc frame to a smc data structure
 *
 *
 * @param[in] frame SMC frame
 * @param[out] raw Raw frame
 * @param[in] states Pointer to the slot state array
 */
void PSI5_HW_ConvertRawSmcFrame(psi5_smc_frame_t * frame,
                                psi5_raw_frame_t * raw,
                                const psi5_slot_state_t * states);

/*!
 * @brief Configures a slot
 *
 * Configures the designated slot according to the config structure
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] slot Configuration structure
 */
void PSI5_HW_ConfigureSlot(const uint32_t instance,
                           const uint32_t channel,
                           const psi5_slot_config_t * slot);

/*!
 * @brief Enters configuration mode
 *
 * Transitions the given channel to configuration mode
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 */
void PSI5_HW_EnterConfigMode(const uint32_t instance,
                             const uint32_t channel);

/*!
 * @brief Enters normal mode
 *
 * Transitions the given channel to normal mode
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 */
void PSI5_HW_EnterNormalMode(const uint32_t instance,
                             const uint32_t channel);

/*!
 * @brief Configures Transmission
 *
 * Configures the transmission part of the driver according to the configuration structure
 *
 * @param[in] instance PSI5 instance
 * @param[in] chCfg Configuration structure
 */
void PSI5_HW_ConfigureTx(const uint32_t instance,
                         const psi5_channel_config_t * chCfg);

/*!
 * @brief Configures Reception
 *
 * Configures the reception part of the driver according to the configuration structure
 *
 * @param[in] instance PSI5 instance
 * @param[in] chCfg Configuration structure
 */
void PSI5_HW_ConfigureRx(const uint32_t instance,
                         const psi5_channel_config_t * chCfg);

/*!
 * @brief Configures the Pulse Generator
 *
 * Configures the pulse generation part of the driver according to the configuration structure
 *
 * @param[in] instance PSI5 instance
 * @param[in] chCfg Configuration structure
 */
void PSI5_HW_ConfigurePulseGenerator(const uint32_t instance,
                                     const psi5_channel_config_t * chCfg);

/*!
 * @brief Configures DMA
 *
 * Configures the DMA part of the driver according to the configuration structure
 *
 * @param[in] instance PSI5 instance
 * @param[in] chCfg Configuration structure
 */
void PSI5_HW_ConfigureDma(const uint32_t instance,
                          const psi5_channel_config_t * chCfg);

/*!
 * @brief Global enabler
 *
 * Enables/disables the channels on a global scope
 *
 * @param[in] state Desired state
 */
void PSI5_HW_MasterGlobalEnable(const bool state);

/*!
 * @brief Channel counter global enabler
 *
 * Enables/disables the channel target counters on a global scope
 *
 * @param[in] state Desired state
 */
void PSI5_HW_MasterGlobalCtc(const bool state);

/*!
 * @brief Channel counter local enabler
 *
 * Enables/disables the channel target counters on a channel scope
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] state Desired state
 */
void PSI5_HW_LocalChannelCtc(const uint32_t instance,
                             const uint32_t channel,
                             const bool state);

/*!
 * @brief Register rest
 *
 * Resets the PSI5 registers to a known default state
 *
 * @param[in] instance PSI5 instance
 */
void PSI5_HW_ResetRegisters(const uint32_t instance);

/*!
 * @brief Writes the Tx data registers
 *
 * Writes the Tx registers based on the configured Tx type
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] data Output data
 * @param[in] custom Uses custom frame format
 */
void PSI5_HW_WriteDataRegister(const uint32_t instance,
                               const uint32_t channel,
                               const uint64_t data,
                               const bool custom);

/*!
 * @brief Writes the Tx data registers
 *
 * Writes the Tx registers based on the configured transmission format
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] custom Uses custom frame format
 * @return true if data registers are ready
 */
bool PSI5_HW_IsDataRegisterReady(const uint32_t instance,
                                 const uint32_t channel,
                                 const bool custom);

/*!
 * @brief Configures Interrupts
 *
 * Configures the interrupts according to the configuration structure
 *
 * @param[in] instance PSI5 instance
 * @param[in] chCfg Configuration structure
 */
void PSI5_HW_EnableInterrupts(const uint32_t instance,
                              const psi5_channel_config_t * chCfg);

#if defined(__cplusplus)
}
#endif

/*! @}*/

/*! @}*/ /* End of addtogroup psi5 */

#endif /* PSI5_DRIVER_HW_H */
