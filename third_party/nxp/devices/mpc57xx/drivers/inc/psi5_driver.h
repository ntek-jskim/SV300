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

#ifndef PSI5_DRIVER_H
#define PSI5_DRIVER_H

/* Required headers */
#include <stddef.h>
#include "device_registers.h"
#include "status.h"

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

/* Tx event flags */
#define PSI5_EV_TX_DATA_OVR             ((uint32_t)1u << 0) /*!< Data register overwrite */
#define PSI5_EV_TX_DATA_RDY             ((uint32_t)1u << 1) /*!< Data ready */

/* PSI5 DMA event flags */
#define PSI5_EV_PSI5_DMA_OVF            ((uint32_t)1u << 2) /*!< PSI5 DMA Overflow */
#define PSI5_EV_PSI5_DMA_UF             ((uint32_t)1u << 3) /*!< PSI5 DMA Underflow */
#define PSI5_EV_PSI5_DMA_COMPLETE       ((uint32_t)1u << 4) /*!< PSI5 DMA Complete */

/* SMC DMA event flags */
#define PSI5_EV_SMC_DMA_UF              ((uint32_t)1u << 5) /*!< SMC DMA Underflow */
#define PSI5_EV_SMC_DMA_COMPLETE        ((uint32_t)1u << 6) /*!< SMC DMA Complete */

/* PSI5 event flags */
#define PSI5_EV_PSI5_RX                 ((uint32_t)1u << 7) /*!< PSI5 Rx Event */
#define PSI5_EV_PSI5_OVR                ((uint32_t)1u << 8) /*!< PSI5 Overwrite */
#define PSI5_EV_PSI5_ERR                ((uint32_t)1u << 9) /*!< PSI5 Rx Error */

/* SMC event flags */
#define PSI5_EV_SMC_RX                  ((uint32_t)1u << 10) /*!< SMC Rx Event */
#define PSI5_EV_SMC_OVR                 ((uint32_t)1u << 11) /*!< SMC Rx Overwrite */
#define PSI5_EV_SMC_ERR                 ((uint32_t)1u << 12) /*!< SMC Rx CRC Error */

/*!
 * @brief PSI5 event data type
 *
 * Type for holding event flags
 *
 * Implements : psi5_event_t_Class
 */
typedef uint32_t psi5_event_t;

/*!
 * @brief PSI5 slot data type
 *
 * Slot configuration structure
 *
 * Implements : psi5_slot_config_t_Class
 */
typedef struct
{
    uint16_t startOffs;  /*!< Slot start offset (from sync pulse, in us) */
    uint16_t slotLen;    /*!< Slot size (in us) */
    uint8_t  slotId;     /*!< Slot id number (1-6) */
    uint8_t  dataSize;   /*!< Data region length */
    bool     msbFirst;   /*!< Data is interpreted as MSB first */
    bool     hasSMC;     /*!< Contains a Slow Message Channel in bits M0/M1 */
    bool     tsCapS0;    /*!< Capture time-stamp at S0, otherwise at sync */
    bool     hasParity;  /*!< Contains parity error detection, CRC otherwise */
} psi5_slot_config_t;

/*!
 * @brief PSI5 Rx mode
 *
 * Possible values for setting the reception mode
 *
 * Implements : psi5_rx_mode_t_Class
 */
typedef enum
{
    PSI5_RX_MODE_ASYNCHRONOUS = 0, /*!< Asynchronous mode */
    PSI5_RX_MODE_SYNCHRONOUS       /*!< Synchronous mode */
} psi5_rx_mode_t;

/*!
 * @brief PSI5 Tx mode
 *
 * Possible values for setting the transmission mode
 *
 * Implements : psi5_tx_mode_t_Class
 */
typedef enum
{
    PSI5_TX_MODE_0 = 0, /*!< Short Frame(V1.3) with 31 "1s" as the start condition */
    PSI5_TX_MODE_1,     /*!< Short Frame(V1.3) with 5 "0s" as the start condition */
    PSI5_TX_MODE_2,     /*!< Long Frame(V1.3) with 31 "1s" as the start condition */
    PSI5_TX_MODE_3,     /*!< Long Frame(V1.3) with 5 "0s" as the start condition */
    PSI5_TX_MODE_4,     /*!< X-Long Frame(V1.3) with 31 "1s" as the start condition */
    PSI5_TX_MODE_5,     /*!< X-Long Frame(V1.3) with 5 "0s" as the start condition */
    PSI5_TX_MODE_6,     /*!< XX-Long (V2.0) */
    PSI5_TX_MODE_7      /*!< Non Standard Length */
} psi5_tx_mode_t;

/*!
 * @brief PSI5 Sync state
 *
 * Possible values for setting the pulse generation state
 *
 * Implements : psi5_sync_state_t_Class
 */
typedef enum
{
    PSI5_SYNC_STATE_2 = 1, /*!< Periodic Sync Pulse Generation with ECU-to-sensor Communication */
    PSI5_SYNC_STATE_1 = 3, /*!< Periodic Sync Pulse Generation (without pulse length modulation) */
    PSI5_SYNC_STATE_5 = 4, /*!< Event triggered sync pulses, direct access */
    PSI5_SYNC_STATE_4 = 5, /*!< Event triggered sync pulse, including ECU-to-sensor communication */
    PSI5_SYNC_STATE_3 = 7  /*!< Event triggered (e.g. angle synchronous) sync pulses */
} psi5_sync_state_t;

/*!
 * @brief Raw data frame
 *
 * A raw PSI5/SMC frame. Contains hardware specific fields. Needs conversion.
 *
 * Implements : psi5_raw_frame_t_Class
 */
typedef uint32_t psi5_raw_frame_t[2];

/*!
 * @brief PSI5 data frame
 *
 * A PSI5 frame. Contains specific fields.
 *
 * Implements : psi5_psi5_frame_t_Class
 */
typedef struct
{
    uint32_t DATA_REGION;  /*!< Data region (28 bits) */
    uint32_t TIME_STAMP;   /*!< Time stamp value (24 bits) */
    uint8_t  CRC;          /*!< CRC value Parity (3 bits, 1 bit parity in C[2] if configured) */
    uint8_t  C;            /*!< CRC error (1 bit) */
    uint8_t  F;            /*!< No Frame error (1 bit) */
    uint8_t  EM;           /*!< M0/1 Error (1 bit) */
    uint8_t  E;            /*!< Electrical error (1 bit) */
    uint8_t  T;            /*!< Timing error (1 bit) */
    uint8_t  SLOT_COUNTER; /*!< Slot number (3 bits) */
} psi5_psi5_frame_t;

/*!
 * @brief SMC data frame
 *
 * A SMC frame. Contains specific fields.
 *
 * Implements : psi5_smc_frame_t_Class
 */
typedef struct
{
    uint16_t DATA;         /*!< DATA payload */
    uint8_t  SLOT_NO;      /*!< Slot number (3 bit)*/
    uint8_t  CER;          /*!< CRC error (1 bit) */
    uint8_t  OW;           /*!< Overwrite status (1 bit) */
    uint8_t  CRC;          /*!< CRC (6 bit) */
    uint8_t  C;            /*!< Configuration bit (1 bit) */
    uint8_t  ID;           /*!< Message ID: If C = '0' indicates ID[7:4], if C = '1' indicates ID[3:0] */
    uint8_t  IDDATA;       /*!< Message ID/DATA: If C = '0' indicates ID[3:0], if C = '1' indicates DATA[15:12] */
} psi5_smc_frame_t;

/*!
 * @brief Channel configuration structure
 *
 * Contains configuration data for one channel.
 *
 * Implements : psi5_channel_config_t_Class
 */
typedef struct
{
    psi5_raw_frame_t *         psi5DmaBuffer;  /*!< DMA buffer for PSI5 messages */
    psi5_raw_frame_t *         smcDmaBuffer;   /*!< DMA buffer for SMC messages */
    const psi5_slot_config_t * slotConfig;     /*!< Pointer to a slot configuration list */
    uint8_t                    numOfConfigs;   /*!< Number of configurations in the slot configuration list */
    uint8_t                    channelId;      /*!< Channel id number (0-7) */
    uint16_t                   initialPulse;   /*!< Initial reset reload value for the integrated pulse generator */
    uint16_t                   targetPulse;    /*!< Subsequent reload values for the integrated pulse generator */
    uint8_t                    decoderOffset;  /*!< Time in us for which the manchester decoder is disabled after the falling edge of a sync pulse */
    uint8_t                    pulse0Width;    /*!< Width (in us) for a "0" output pulse */
    uint8_t                    pulse1Width;    /*!< Width (in us) for a "1" output pulse */
    bool                       syncGlobal;     /*!< Sync generator controlled by the global instance (master) */
    bool                       asyncReset;     /*!< GTM reset is treated asynchronous */
    psi5_sync_state_t          syncState;      /*!< Pulse generator state, please refer to RM */
    psi5_rx_mode_t             rxMode;         /*!< Communication mode */
    psi5_tx_mode_t             txMode;         /*!< Transmitter mode, please refer to RM */
    uint8_t                    txSize;         /*!< Tx data length (only for TX_MODE_7) */
    bool                       txDefault1;     /*!< All bits in Tx registers will default to "1" */
    uint8_t                    rxBufSize;      /*!< Size of RX buffer for PSI5 messages (1 - 32) */
    bool                       psi5UsesDma;    /*!< DMA enabled for PSI5 frames */
    bool                       smcUsesDma;     /*!< DMA enabled for SMC frames */
    uint8_t                    psi5DmaChannel; /*!< Assigned DMA channel for PSI5 frames */
    uint8_t                    smcDmaChannel;  /*!< Assigned DMA channel for SMC frames */
} psi5_channel_config_t;

/*!
 * @brief Callback function
 *
 * PSI5 callback prototype. Called from IRQ.
 *
 * Implements : psi5_callback_func_t_Class
 */
typedef void(* psi5_callback_func_t)(const uint32_t instance,
                                     const uint32_t channel,
                                     const psi5_event_t events,
                                     void * param);

/*!
 * @brief Callback structure
 *
 * PSI5 callback structure. contains callback and user parameter.
 *
 * Implements : psi5_callback_t_Class
 */
typedef struct
{
    psi5_callback_func_t function; /*!< Callback function */
    void *               param;    /*!< User parameter */
} psi5_callback_t;

/*!
 * @brief Instance configuration structure
 *
 * PSI5 instance configuration structure. Contains all data required to configure the instance.
 *
 * Implements : psi5_driver_user_config_t_Class
 */
typedef struct
{
    const psi5_channel_config_t * channelConfig; /*!< Pointer to a channel configuration list */
    uint8_t                       numOfConfigs;  /*!< Number of configurations in the channel configuration list */
    bool                          globalCtcEn;   /*!< Pulse generation automatically enabled at init (outcome depends on channel configuration) */
    psi5_callback_t               callback;      /*!< Callback structure */
} psi5_driver_user_config_t;

/*!
 * @brief Slot state structure
 *
 * Internal slot state data.
 *
 * Implements : psi5_slot_state_t_Class
 */
typedef struct
{
/*! @cond DRIVER_INTERNAL_USE_ONLY */
    bool    slotActive; /*!< Marks a slot as configured and active */
    bool    msbFirst;   /*!< Data is interpreted as MSB first */
    uint8_t dataSize;   /*! Number of configured bits in the data frame */
/*! @endcond */
} psi5_slot_state_t;

/*!
 * @brief Channel state structure
 *
 * Internal channel state data.
 *
 * Implements : psi5_channel_state_t_Class
 */
typedef struct
{
/*! @cond DRIVER_INTERNAL_USE_ONLY */
    psi5_slot_state_t slotCfg[FEATURE_PSI5_SLOT_COUNT]; /*!< Slot state */
    uint32_t          psi5PendingFlags;                 /*!< Pending buffer flags for PSI5 frames */
    uint8_t           smcPendingFlags;                  /*!< Pending buffer flags for SMC frames */
    bool              channelActive;                    /*!< Marks a channel as configured and active */
    bool              txEnabled;                        /*!< Tx configured */
    bool              customTx;                         /*!< Uses custom Tx command */
    bool              psi5UsesDma;                      /*!< DMA enabled for PSI5 frames */
    bool              smcUsesDma;                       /*!< DMA enabled for SMC frames */
/*! @endcond */
} psi5_channel_state_t;

/*!
 * @brief State structure
 *
 * Internal driver state data.
 *
 * Implements : psi5_state_t_Class
 */
typedef struct
{
/*! @cond DRIVER_INTERNAL_USE_ONLY */
    psi5_callback_t      callback;                          /*!< Callback data */
    psi5_channel_state_t chCfg[FEATURE_PSI5_CHANNEL_COUNT]; /*!< Channel state */
    uint8_t              instanceId;                        /*!< Instance Id number */
/*! @endcond */
} psi5_state_t;

/*!
 * @brief Main initializer for the driver
 *
 * Initializes the driver for a given peripheral
 * according to the given configuration structure.
 *
 * @param[in] instance Instance of the PSI5 peripheral
 * @param[in] configPtr Pointer to the configuration structure
 * @param[out] state Pointer to the state structure to populate
 * @return STATUS_SUCCES If initialization succeeded
 * @return STATUS_ERROR If initialization failed
 */
status_t PSI5_DRV_Init(const uint32_t instance,
                       const psi5_driver_user_config_t * configPtr,
                       psi5_state_t * state);

/*!
 * @brief Reset the peripheral.
 *
 * De-Initializes the peripheral and brings it's registers into a reset state.
 *
 * @param[in] instance Peripheral instance number
 * @return STATUS_SUCCESS If de-init succeeded
 * @return STATUS_ERROR If not configured before calling de-init
 */
status_t PSI5_DRV_DeInit(const uint32_t instance);

/*!
 * @brief Set peripheral notification function
 *
 * Sets a notification function.
 *
 * @param[in] instance Peripheral instance number
 * @param[in] function Callback function
 * @param[in] param Callback parameter
 * @return STATUS_SUCCESS If install succeeded
 * @return STATUS_ERROR If not configured before calling install
 */
status_t PSI5_DRV_InstallCallback(const uint32_t instance,
                                  psi5_callback_func_t function,
                                  void * param);

/*!
 * @brief Transmit a data frame
 *
 * Transmits a data frame according to configuration.
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @param[in] data Data to send
 * @return STATUS_SUCCESS If transmit succeeded
 * @return STATUS_ERROR If not configured or not enabled or not ready
 */
status_t PSI5_DRV_Transmit(const uint32_t instance,
                           const uint32_t channel,
                           const uint64_t data);

/*!
 * @brief Transmission status
 *
 * Returns the status of the transmission part of the driver.
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @return true If transmission ready
 * @return false If transmission pending
 */
bool PSI5_DRV_IsTransmitReady(const uint32_t instance,
                              const uint32_t channel);

/*!
 * @brief Gets a raw PSI5 frame
 *
 * Returns the last received PSI5 frame.
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @param[out] frame Target variable
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_GetRawPsi5Frame(const uint32_t instance,
                                  const uint32_t channel,
                                  psi5_raw_frame_t * frame);

/*!
 * @brief Gets a raw SMC frame
 *
 * Returns the last received SMC frame.
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @param[out] frame Target variable
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_GetRawSmcFrame(const uint32_t instance,
                                 const uint32_t channel,
                                 psi5_raw_frame_t * frame);

/*!
 * @brief Converts a PSI5 frame
 *
 * Converts a raw PSI5 frame to a PSI5 data structure
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @param[out] frame Target variable
 * @param[in] raw Source data
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_ConvertPsi5Frame(const uint32_t instance,
                                   const uint32_t channel,
                                   psi5_psi5_frame_t * frame,
                                   psi5_raw_frame_t * raw);

/*!
 * @brief Converts a SMC frame
 *
 * Converts a raw SMC frame to a SMC data structure
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @param[out] frame Target variable
 * @param[in] raw Source data
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_ConvertSmcFrame(const uint32_t instance,
                                  const uint32_t channel,
                                  psi5_smc_frame_t * frame,
                                  psi5_raw_frame_t * raw);

/*!
 * @brief Global sync state
 *
 * Changes the global sync pulse generator state
 *
 * @param[in] state Desired state
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_SetGlobalSync(const bool state);

/*!
 * @brief Channel sync state
 *
 * Changes the channel sync pulse generator state
 *
 * @param[in] instance Peripheral instance number
 * @param[in] channel Instance channel number
 * @param[in] state Desired state
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_SetChannelSync(const uint32_t instance,
                                 const uint32_t channel,
                                 const bool state);

/*!
 * @brief Default working configuration
 *
 * Returns a working configuration for a PSI5-A16CRC-500_1L sensor
 *
 * @param[out] configPtr User configuration variable
 * @param[out] chPtr Channel configuration variable
 * @param[out] slotPtr Slot configuration variable
 * @return STATUS_SUCCESS Always
 */
status_t PSI5_DRV_GetDefaultConfig(psi5_driver_user_config_t * configPtr,
                                   psi5_channel_config_t * chPtr,
                                   psi5_slot_config_t * slotPtr);

#if defined(__cplusplus)
}
#endif

/*! @}*/

/*! @}*/ /* End of addtogroup psi5 */

#endif /* PSI5_DRIVER_H */
