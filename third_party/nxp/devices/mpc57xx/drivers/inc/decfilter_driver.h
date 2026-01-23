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
 * @file decfilter_driver.h
 *
 * @page misra_violations MISRA-C:2012 violations
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 2.3, not referenced
 * Global typedef not referenced in code.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 5.1, identifier clash
 * The supported compilers use more than 31 significant characters for identifiers.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 5.2, identifier clash
 * The supported compilers use more than 31 significant characters for identifiers.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 5.4, identifier clash
 * The supported compilers use more than 31 significant characters for identifiers.
 *
 * @section [global]
 * Violates MISRA 2012 Required Rule 5.5, identifier clash
 * The supported compilers use more than 31 significant characters for identifiers.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 2.5, not referenced
 * Macro not referenced in code.
 *
 */

#ifndef DECFILTER_DRIVER_H
#define DECFILTER_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "status.h"

#include "device_registers.h"
#include "edma_driver.h"

/*!
 * @addtogroup decfilter_driver
 * @{
 */

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*!
 * @brief Operation Mode selection
 *
 * This define all decfilter functional mode
 *
 * Implements : decfilter_mode_t_Class
 */
typedef enum
{
    DECFILTER_MODE_STANDALONE           = 0x00U, /*!< DEFAULT : data is provided by the central processor using the device slave-bus interface or DMA interface signals. */
    DECFILTER_MODE_FREEZE               = 0x01U, /*!< Freeze mode is also known as debug mode. */
    DECFILTER_MODE_LOW_POWER            = 0x02U, /*!< Low power mode corresponds to the module disable mode or stop mode */
    DECFILTER_MODE_CASCADE_HEAD         = 0x03U, /*!< filter blocks connected in a chain HEAD */
    DECFILTER_MODE_CASCADE_TAIL         = 0x04U, /*!< filter blocks connected in a chain TAIL */
    DECFILTER_MODE_CASCADE_MIDDLE       = 0x05U, /*!< filter blocks connected in a chain MIDDLE */
#if FEATURE_DECFILTER_HAS_PSI
    DECFILTER_MODE_NORMAL               = 0x06U, /*!< corresponds to the prefill/filter operation with input data supplied through any of the two PSI slave-bus
                                                  *  interfaces with output going to the same or to the other PSI interface. */
    DECFILTER_MODE_PSI_INPUT_MIXED      = 0x07U, /*!< input is selected from any of the two PSI slave-bus interfaces, but the output is directed to the device slave-bus interface,  */
    DECFILTER_MODE_PSI_OUTPUT_MIXED     = 0x08U, /*!< the input is selected from the device slave-bus interface and the output is directed to one of the two PSI slave-bus interfaces */
    DECFILTER_MODE_CASCADE_HEAD_PSI     = 0x09U, /*!< filter blocks connected in a chain HEAD input from any PSI*/
    DECFILTER_MODE_CASCADE_TAIL_PSI     = 0x0AU, /*!< filter blocks connected in a chain TAIL input from any PSI*/
#endif
} decfilter_mode_t;

/*!
 * @brief filter type selection
 *
 * Select the filter type.
 * Bypass must not be configured in cascade mode (see field CASCD).
 *
 * Implements : decfilter_filter_type_t_Class
 */
typedef enum
{
    DECFILTER_FILTER_TYPE_BYPASS    = 0x00U, /*!< Filter Bypass  */
    DECFILTER_FILTER_TYPE_IIR       = 0x01U, /*!< IIR Filter - 1 x 4th order  */
    DECFILTER_FILTER_TYPE_FIR       = 0x02U, /*!< FIR Filter - 1 x 8th order  */
} decfilter_filter_type_t;

/*!
 * @brief filter scaling factor
 *
 * Selects the scaling factor used by the filter algorithm
 *
 * Implements : decfilter_scaling_factor_t_Class
 */
typedef enum
{
    DECFILTER_SCALING_FACTOR_1      = 0x00U, /*!< Scaling Factor = 1  */
    DECFILTER_SCALING_FACTOR_4      = 0x01U, /*!< Scaling Factor = 4  */
    DECFILTER_SCALING_FACTOR_8      = 0x02U, /*!< Scaling Factor = 8  */
    DECFILTER_SCALING_FACTOR_16     = 0x03U, /*!< Scaling Factor = 16  */
} decfilter_scaling_factor_t;

#if FEATURE_DECFILTER_HAS_PSI
/*!
 * @brief Trigger Mode.
 *
 * This will selects the way the trigger signal controls the output result sampling function enabled
 *
 * Implements : decfilter_trigger_mode_t_Class
 */
typedef enum
{
    DECFILTER_TRIGGER_MODE_RISING   = 0x00U, /*!< output is posted at the rising edge of the trigger signal */
    DECFILTER_TRIGGER_MODE_LOGIC0   = 0x01U, /*!< output is posted whenever the trigger signal is a logical 0 */
    DECFILTER_TRIGGER_MODE_FALLING  = 0x02U, /*!< output is posted at the falling edge of the trigger signal */
    DECFILTER_TRIGGER_MODE_LOGIC1   = 0x03U, /*!< output is posted whenever the trigger signal is a logical 1 */
} decfilter_trigger_mode_t;
#endif

#if FEATURE_DECFILTER_HAS_PSI
/*!
 * @brief PSI Output Selection.
 *
 * This will selects the output PSI to send result filter in PSI output mixed mode.
 * In standalone mode this bit has no effect
 *
 * Implements : decfilter_select_psi_t_Class
 */
typedef enum
{
    DECFILTER_SELECT_PSI_0 = 0x00U, /*!< PSI0 is selected to send/receive result data (in PSI output mixed mode)*/
    DECFILTER_SELECT_PSI_1 = 0x01U, /*!< PSI1 is selected to send/receive result data (in PSI output mixed mode)*/
} decfilter_select_psi_t;
#endif

/*!
 * @brief Integrator Zero Control Mode Selection
 *
 * field defines the use of the integrator zero hardware input signal
 *
 * Implements : decfilter_integrator_zero_t_Class
 */
typedef enum
{
    DECFILTER_INTEGRATOR_ZERO_DISABLE   = 0x00U, /*!< hardware integrator zero request disabled */
    DECFILTER_INTEGRATOR_ZERO_TOGGLE    = 0x01U, /*!< integrator zero on toggle of hardware signal */
    DECFILTER_INTEGRATOR_ZERO_RISING    = 0x02U, /*!< integrator zero on rising edge of hardware signal */
    DECFILTER_INTEGRATOR_ZERO_FALLING   = 0x03U, /*!< integrator zero on falling edge of hardware signal */
} decfilter_integrator_zero_t;

/*!
 * @brief Integrator Halt Control Selection
 *
 * field defines the integrator halting mechanism.
 * When the integrator is halted, the integration accumulator remains unaltered on filter outputs
 * independently of the enabling selected by SENSEL.
 *
 * Implements : decfilter_integrator_halt_control_t_Class
 */
typedef enum
{
    DECFILTER_INTEGRATOR_HALT_CONTROL_DISABLE   = 0x00U, /*!< hardware halt control signal disabled */
    DECFILTER_INTEGRATOR_HALT_CONTROL_HALTED    = 0x01U, /*!< integrator halted, independently of the hardware signal */
    DECFILTER_INTEGRATOR_HALT_CONTROL_HALTED0   = 0x02U, /*!< integrator halted when signal is at logical 0 */
    DECFILTER_INTEGRATOR_HALT_CONTROL_HALTED1   = 0x03U, /*!< integrator halted when signal is at logical 1 */
} decfilter_integrator_halt_control_t;

/*!
 * @brief Integrator Output Read Request Mode Selection
 *
 * field defines the use of the integrator output request hardware input signal.
 * An integrator output request updates the registers DECFILTER_FINTVAL and DECFILTER_FINTCNT,
 * also causing a DMA or interrupt request. Note that DMA or interrupt requests due to integrator
 * output updates depend on the DECFILTER_MXCR bit SDMAE and DECFILTER_MCR bit SDIE.
 * When continuous output is on, an integrator output request is issued whenever a new filter output is accumulated.
 *
 * Implements : decfilter_integrator_output_read_rq_t_Class
 */
typedef enum
{
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_DISABLE     = 0x00U, /*!< hardware output request disabled */
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_TOGGLE      = 0x01U, /*!< integrator output request on toggle of hardware signal */
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_RISING      = 0x02U, /*!< integrator output request on rising edge of hardware signal */
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_FALLING     = 0x03U, /*!< integrator output request on falling edge of hardware signal */
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_INDEP       = 0x05U, /*!< continuous output request on, independently of hardware signal */
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_LOGIC0      = 0x06U, /*!< continuous output request on when signal is at logical 0 */
    DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_LOGIC1      = 0x07U, /*!< continuous output request on when signal is at logical 1 */
} decfilter_integrator_output_read_rq_t;

/*!
 * @brief Integrator Enable Control Selection
 *
 * field defines the integrator enabling mechanism
 * When the integrator is enabled, filter outputs selected by the SISEL bit are added to the integration accumulator.
 * When the integrator is disabled, the integration accumulator remains unaltered on filter outputs.
 *
 * Implements : decfilter_integrator_control_t_Class
 */
typedef enum
{
    DECFILTER_INTEGRATOR_CONTROL_DISABLE    = 0x00U, /*!< integrator disabled, independently of the hardware enable control signal */
    DECFILTER_INTEGRATOR_CONTROL_ENABLE     = 0x01U, /*!< integrator enabled, independently of the hardware signal*/
    DECFILTER_INTEGRATOR_CONTROL_LOGIC0     = 0x02U, /*!< integrator enabled when signal is at logical 0 */
    DECFILTER_INTEGRATOR_CONTROL_LOGIC1     = 0x03U, /*!< integrator enabled when signal is at logical 1 */
} decfilter_integrator_control_t;


/*!
 * @brief Integrator output operation
 *
 * field defines the integrator output update mechanism
 * If bits SRQ and SZRO are both written 1 at the same time, the integrator is reset only after the
 * registers DECFILTER_FINTVAL and DECFILTER_FINTCNT are updated.
 *
 * Implements : decfilter_integrator_output_operation_t_Class
 */
typedef enum
{
    DECFILTER_INTEGRATOR_OUTPUT_UPDATE          = 0x00U, /*!< Update of the integrator output*/
    DECFILTER_INTEGRATOR_OUTPUT_RESET           = 0x01U, /*!< Integrator is set to zero  */
    DECFILTER_INTEGRATOR_OUTPUT_RESET_SYNCED    = 0x02U, /*!< Integrator is reset only after the registers DECFILTER_FINTVAL and DECFILTER_FINTCNT are updated */
    DECFILTER_INTEGRATOR_OUTPUT_RESET_ALL       = 0x03U, /*!< reset current and final value and counter */
} decfilter_integrator_output_operation_t;


/* macros for clear/get status flags for decfilter
 *
 * This are used  to clear or set flags for decfilter status
 * used in DECFILTER_DRV_GetStatusFlags and DECFILTER_DRV_ClearStatusFlags
 *
 * Implements : DECFILTER_STATUS_FLAG_Class
 */
#define DECFILTER_STATUS_FLAG_INPUT_DATA              (DECFILTER_MSR_IDF_MASK)      /*!< Input Data Flag */
#define DECFILTER_STATUS_FLAG_OUTPUT_DATA             (DECFILTER_MSR_ODF_MASK)      /*!< Output Data Flag */
#define DECFILTER_STATUS_FLAG_INPUT_BUFFER_INTERRUPT  (DECFILTER_MSR_IBIF_MASK)     /*!< Input Buffer Interrupt Request */
#define DECFILTER_STATUS_FLAG_OUTPUT_BUFFER_INTERRUPT (DECFILTER_MSR_OBIF_MASK)     /*!< Output Buffer Interrupt Request */
#define DECFILTER_STATUS_FLAG_FILTER_OVERFLOW         (DECFILTER_MSR_OVF_MASK)      /*!< Filter Overflow */
#define DECFILTER_STATUS_FLAG_OUTPUT_OVERRUN         (DECFILTER_MSR_OVR_MASK)       /*!< Output Interface Buffer Overrun */
#define DECFILTER_STATUS_FLAG_INPUT_OVERRUN           (DECFILTER_MSR_IVR_MASK)      /*!< Input Interface Buffer Overrun */
#define DECFILTER_STATUS_FLAG_IS_BUSY                 (DECFILTER_MSR_BSY_MASK)      /*!< Decimation Filter Busy indication used only for DECFILTER_DRV_GetStatusFlags */
#define DECFILTER_STATUS_FLAG_DEC_COUNTER             (DECFILTER_MSR_DEC_COUNTER_MASK)     /*!< Decimation Counter used only for DECFILTER_DRV_GetStatusFlags */
#if FEATURE_DECFILTER_ENHANCED_DEBUG
#define DECFILTER_STATUS_FLAG_ENHANCED_DEBUG_OVERRUN  (DECFILTER_MSR_DIVR_MASK)     /*!< Enhanced Debug Monitor Input Data Read Overrun */
#define DECFILTER_STATUS_FLAG_ALL                     (DECFILTER_STATUS_FLAG_INPUT_DATA |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_DATA |\
                                                       DECFILTER_STATUS_FLAG_INPUT_BUFFER_INTERRUPT |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_BUFFER_INTERRUPT |\
                                                       DECFILTER_STATUS_FLAG_FILTER_OVERFLOW |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_INPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_ENHANCED_DEBUG_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_DEC_COUNTER |\
                                                       DECFILTER_STATUS_FLAG_IS_BUSY)       /*!< Combination of all status flags */
#define DECFILTER_STATUS_FLAG_ERROR                   (DECFILTER_STATUS_FLAG_FILTER_OVERFLOW |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_INPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_ENHANCED_DEBUG_OVERRUN)  /*!< Combination of all error flags */
#else /* if FEATURE_DECFILTER_ENHANCED_DEBUG */
#define DECFILTER_STATUS_FLAG_ALL                     (DECFILTER_STATUS_FLAG_INPUT_DATA |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_DATA |\
                                                       DECFILTER_STATUS_FLAG_INPUT_BUFFER_INTERRUPT |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_BUFFER_INTERRUPT |\
                                                       DECFILTER_STATUS_FLAG_FILTER_OVERFLOW |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_INPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_DEC_COUNTER |\
                                                       DECFILTER_STATUS_FLAG_IS_BUSY)       /*!< Combination of all status flags */
#define DECFILTER_STATUS_FLAG_ERROR                   (DECFILTER_STATUS_FLAG_FILTER_OVERFLOW |\
                                                       DECFILTER_STATUS_FLAG_OUTPUT_OVERRUN |\
                                                       DECFILTER_STATUS_FLAG_INPUT_OVERRUN) /*!< Combination of all error flags */
#endif /* if FEATURE_DECFILTER_ENHANCED_DEBUG */

/* macros for clear/get status flags for decfilter integrator
 *
 * This are used  to clear or set flags for decfilter integrator
 * used in DECFILTER_DRV_GetIntegratorFlags and DECFILTER_DRV_ClearIntegratorFlags
 *
 * Implements : DECFILTER_INTEGRATOR_FLAGS_Class
 */
#define DECFILTER_INTEGRATOR_FLAG_DATA_OVERRUN        (DECFILTER_MXSR_SVR_MASK)         /*!< Integrator Data Overrun */
#define DECFILTER_INTEGRATOR_FLAG_COUNT_OVERFLOW      (DECFILTER_MXSR_SCOVF_MASK)       /*!< Integrator Count Overflow Flag */
#define DECFILTER_INTEGRATOR_FLAG_SUM_OVERFLOW        (DECFILTER_MXSR_SSOVF_MASK)       /*!< Integrator Sum Overflow Flag */
#define DECFILTER_INTEGRATOR_FLAG_COUNT_EXCEPTION     (DECFILTER_MXSR_SCE_MASK)         /*!< Integrator Count Exception flag */
#define DECFILTER_INTEGRATOR_FLAG_SUM_EXCEPTION       (DECFILTER_MXSR_SSE_MASK)         /*!< Integrator Sum Exception flag */
#define DECFILTER_INTEGRATOR_FLAG_DATA                (DECFILTER_MXSR_SDF_MASK)         /*!< Integrator Data Flag */

#define DECFILTER_INTEGRATOR_FLAG_ALL                 (DECFILTER_INTEGRATOR_FLAG_DATA_OVERRUN |\
                                                       DECFILTER_INTEGRATOR_FLAG_COUNT_OVERFLOW |\
                                                       DECFILTER_INTEGRATOR_FLAG_SUM_OVERFLOW |\
                                                       DECFILTER_INTEGRATOR_FLAG_COUNT_EXCEPTION |\
                                                       DECFILTER_INTEGRATOR_FLAG_SUM_EXCEPTION |\
                                                       DECFILTER_INTEGRATOR_FLAG_DATA)  /*!< Combination of all integrator flags */

/* macros for enables/ disable interrupts for decfilter
 *
 * used in DECFILTER_DRV_EnableInterrupts and DECFILTER_DRV_DisableInterrupts functions
 *
 * Implements : DECFILTER_INTERRUPT_Class
 */
#define DECFILTER_INTERRUPT_INPUT_DATA                (DECFILTER_MCR_IDEN_MASK) /*!< Input Data Interrupt Enable */
#define DECFILTER_INTERRUPT_OUTPUT_DATA               (DECFILTER_MCR_ODEN_MASK) /*!< Output Data Interrupt Enable */
#define DECFILTER_INTERRUPT_ERROR                     (DECFILTER_MCR_ERREN_MASK) /*!< Error Interrupt Enable */
#define DECFILTER_INTERRUPT_INTEGRATOR_DATA           (DECFILTER_MCR_SDIE_MASK) /*!< Integrator Data Interrupt Enable */
#define DECFILTER_INTERRUPT_INPUT_BUFFER              (DECFILTER_MCR_IBIE_MASK) /*!< Input Buffer Interrupt Request Enable */
#define DECFILTER_INTERRUPT_OUTPUT_BUFFER             (DECFILTER_MCR_OBIE_MASK) /*!< Output Buffer Interrupt Request Enable  */
#define DECFILTER_INTERRUPT_ALL                       (DECFILTER_INTERRUPT_INPUT_DATA |\
                                                       DECFILTER_INTERRUPT_OUTPUT_DATA |\
                                                       DECFILTER_INTERRUPT_ERROR |\
                                                       DECFILTER_INTERRUPT_INTEGRATOR_DATA |\
                                                       DECFILTER_INTERRUPT_INPUT_BUFFER |\
                                                       DECFILTER_INTERRUPT_OUTPUT_BUFFER) /*!< Combination of all interrupt flags */

/*!
 * @brief Structure for config/set DMA parameters for moving output data from a result buffer into system memory.
 *
 * Implements : decfilter_dma_output_config_t_Class
 */
typedef struct
{
    uint8_t dmaChan;             /*!< DMA channel to be used */
    uint32_t * destPtr;          /*!< Pointer to the memory address where the decfilter output will be copied by the DMA, if DMA transfer is enabled */
    uint32_t destLength;         /*!< Length (num of uint32_t words) of the destination buffer pointed by destPtr */
    edma_callback_t callback;    /*!< Callback function called when the rQueue has been filled by the DMA */
    void * callbackParam;        /*!< Parameter to be passed to the callback function pointer */
    bool enHalfDestCallback;     /*!< Enable the callback to be called when half of the destination buffer has been filled */
} decfilter_dma_output_config_t;

/*!
 * @brief Structure for config/set DMA parameters for moving data into input buffer.
 *
 * Implements : decfilter_dma_input_config_t_Class
 */
typedef struct
{
    uint8_t dmaChan;             /*!< DMA channel to be used */
    uint32_t * sourcePtr;        /*!< Pointer to the memory address from where the commands will be copied by the DMA, if DMA transfer is enabled */
    uint32_t sourceLength;       /*!< Length (number of uint32_t) of the source buffer pointed by sourcePtr */
    edma_callback_t callback;    /*!< Callback function called when the rQueue has been filled by the DMA */
    void * callbackParam;        /*!< Parameter to be passed to the callback function pointer */
} decfilter_dma_input_config_t;

/*!
 * @brief Structure to configure the decfilter integrator
 *
 * Implements : decfilter_integrator_config_t_Class
 */
typedef struct
{
    bool dmaEnable; /*!< enables the DMA request when an integrator output is requested */
    bool signalFilter; /*!< defines how the filtered data signal is treated for integration
                        *  false = integrator input takes the absolute value of filter output
                        *  true = integrator input takes signaled filter output */
    bool saturatedOperation; /*!< defines how the integrator accumulator behaves in case of an overflow
                              *  false = integrator accumulator holds a modulo 2^17 value (considering the 15-bit fractional part) on an overflow.
                              *  true = integrator accumulator saturates on an overflow */
    bool counterSaturatedOperation; /*!< defines how the integrator sample counter behaves in case of an overflow.
                                     *  false = integrator sample counter holds a modulo 2^32 value on an overflow.
                                     *  true = integrator sample counter saturates on an overflow, holding a value of 0xFFFFFFFF.*/
    bool inputSelection; /*!< select the input of the integrator
                          *  false = decimated filter outputs feed the integrator
                          *  true = filter outputs before the decimation feed the integrator
                          */
    decfilter_integrator_zero_t zeroControlMode; /*!< field defines the use of the integrator zero hardware input signal */
    decfilter_integrator_halt_control_t haltControl; /*!< field defines the integrator halting mechanism */
    decfilter_integrator_output_read_rq_t outputReadRequestMode; /*!< field defines the use of the  integrator output request hardware input signal */
    decfilter_integrator_control_t enabled; /*!< field defines the integrator enabling mechanism. */

    uint8_t haltSelection;            /*!< Halt Input Select for Decimation Filter */
#if FEATURE_DECFILTER_HAS_COMBINED_ZIR
    uint8_t zirSelection;             /*!< ZIR Input Select for Decimation Filter */
#else
    uint8_t zeroSelection;            /*!< INTEG_ZERO Input Select for Decimation Filter */
    uint8_t readSelection;            /*!< INTEG_REQ Input Select for Decimation Filter */
    uint8_t enableSelection;          /*!< INTEG_EN Input Select for Decimation Filter */
#endif
} decfilter_integrator_config_t;

/*!
 * @brief Structure to configure the decifilter.
 *
 * This will determine how decfilter configure.
 *
 * Implements : decfilter_config_t_Class
 */
typedef struct
{
    decfilter_mode_t mode;                      /*!< configuration register provides configuration control bits for the Decimation Filter internal logic */
    decfilter_filter_type_t typeFilter;         /*!< select the filter type */
    decfilter_scaling_factor_t scaleFactor;     /*!< field selects the scaling factor used by the filter algorithm */

    bool inputDataInterruptEnable;              /*!< enables the Decimation Filter to generate interrupt requests on all new input data written to the Interface Input Buffer register or Input/Output Buffers register */
    bool outputDataInterruptEnable;             /*!< enables the Decimation Filter to generate interrupt requests on all new data written to the filter Output buffer. It is independent of how ISEL is set. */
    bool errorInterruptEnable;                  /*!< enables the Decimation Filter to generate interrupt requests based on the assertion of the DECFILTER_MSR register error flags OVF, DIVR, SVR, OVR, or IVR */
    bool integratorDataInterruptEnable;         /*!< enables output buffer interrupts due to integrator data result being ready */
    bool inputBufferInterruptRequestEnable;     /*!< enables the Decimation Filter to generate interrupt requests */
    bool outputBufferInterruptRequestEnable;    /*!< enables the Decimation Filter interrupt requests when outputs are directed to the device slave-bus and DMA is not selected (DSEL=0) */

    decfilter_dma_input_config_t * dmaConfigInputBuffer;   /*!< dma config for Decimation Filter X Input Buffer (fill) */
    decfilter_dma_output_config_t * dmaConfigOutputBuffer;   /*!< dma config for Decimation Filter X Output Buffer/Integrator (drain/integrator) */

    bool saturationEnable;              /*!< enables the saturation of the filter output. */
    uint8_t decimationRateSelection;    /*!< The decimation rate defines the number of data samples from the master block that is required to generate one decimated result in the Decimation Filter output. */

#if FEATURE_DECFILTER_HAS_TRIGGER
    bool triggeredOutputResultEnable;       /*!< enables an input trigger signal to force the decimation filter to send the next result of the filter back to the master block.*/
    decfilter_trigger_mode_t triggerMode;   /*!< field selects the way the trigger signal controls the output result sampling function enabled by the triggeredOutputResultEnable */
    uint8_t triggerSelection;               /*!< trigger Input Select for Decimation Filter (device specific) */
#endif
#if FEATURE_DECFILTER_ENHANCED_DEBUG
    bool enhancedDebugMonitor;
#endif

    decfilter_integrator_config_t integrator;

    uint32_t coefficients[DECFILTER_COEF_COUNT]; /*!< fields are the digital filter coefficients registers. The coefficients are fractional signed values in 2s complement format, in the range */

    uint32_t timeout; /*!< timeout for soft-reset procedure*/

} decfilter_config_t;

/*!
 * @brief Structure of input buffer register
 *
 * Implements : decfilter_input_buffer_cmd_t_Class
 */
typedef struct
{
#if FEATURE_DECFILTER_HAS_PSI
    decfilter_select_psi_t selectedPSI; /*!< only in PSI output mixed mode */
#endif
    uint8_t inTag;
    bool prefill;
    bool flush;
} decfilter_input_buffer_cmd_t;

/*!
 * @brief Structure of output buffer register
 *
 * Implements : decfilter_output_buffer_t_Class
 */
typedef struct
{
#if FEATURE_DECFILTER_HAS_PSI
    decfilter_select_psi_t selectedPSI; /*!< only in PSI output mixed mode */
#endif
    uint8_t outTag; /*!< Decimation filter input tag bits*/
    uint16_t data;
} decfilter_output_buffer_t;

/*!
 * @brief Structure for current and final value of integrator
 *
 * used for DECFILTER_DRV_GetFinalValues and DECFILTER_DRV_GetCurrentValues
 *
 * Implements : decfilter_integrator_values_t_Class
 */
typedef struct
{
    uint32_t value;
    uint32_t count;
} decfilter_integrator_values_t;

/*******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @brief Initializes the DECFILTER module.
 *
 * This function initializes all DECFILTER init registers using the configuration structure
 * This function should be called before calling any other DECFILTER driver function.
 * Note: when using DMA for input data buffer filling, after DECFILTER_DRV_Init() function, need to prepare the input data
 * buffer which is used as input by DMA, and then call EDMA_DRV_StartChannel() function to start DMA transfer.
 * This is only required when using DMA for input data transfer. For output data draining, the DMA channels are enabled by DECFILTER_DRV_Init().
 *
 * @param[in] instance DECFILTER module instance number.
 * @param[in] config Pointer to DECFILTER configuration structure
 * @return STATUS_SUCCESS - if no busy timeout
 * @return STATUS_TIMEOUT - if soft-reset procedure return timeout
 */
status_t DECFILTER_DRV_Init(const uint32_t instance,
                            const decfilter_config_t * config);

/*!
 * @brief Gets the default configuration structure
 *
 * This function gets the default configuration structure with default settings
 *
 * @param[out] config The configuration structure
 *  The initialized members are :
 *     - mode = DECFILTER_MODE_STANDALONE
 *     - typeFilter = DECFILTER_FILTER_TYPE_BYPASS
 *     - scaleFactor = DECFILTER_SCALING_FACTOR_1
 *
 *     - inputDataInterruptEnable = false;
 *     - outputDataInterruptEnable = false;
 *     - errorInterruptEnable = false;
 *
 *     - integratorDataInterruptEnable = false;
 *     - inputBufferInterruptRequestEnable = false;
 *     - outputBufferInterruptRequestEnable = false;
 *
 *     - dmaConfigInputBuffer = NULL;
 *     - dmaConfigOutputBuffer = NULL;
 *
 *     - saturationEnable = false;
 *     - decimationRateSelection = 0x00;
 *
 *     - triggeredOutputResultEnable = false; (if FEATURE_DECFILTER_HAS_TRIGGER)
 *     - triggerMode = DECFILTER_TRIGGER_MODE_RISING;  (if FEATURE_DECFILTER_HAS_TRIGGER)
 *     - triggerSelection = 0;  (if FEATURE_DECFILTER_HAS_TRIGGER)
 *
 *     - enhancedDebugMonitor = false; (if FEATURE_DECFILTER_ENHANCED_DEBUG)
 *
 *     - integrator.dmaEnable= false;
 *     - integrator.signalFilter = false;
 *     - integrator.saturatedOperation = false;
 *     - integrator.counterSaturatedOperation = false;
 *     - integrator.inputSelection = false;
 *     - integrator.zeroControlMode = DECFILTER_INTEGRATOR_ZERO_DISABLE;
 *     - integrator.haltControl = DECFILTER_INTEGRATOR_HALT_CONTROL_DISABLE;
 *     - integrator.outputReadRequestMode = DECFILTER_INTEGRATOR_OUTPUT_READ_RQ_DISABLE;
 *     - integrator.enabled = DECFILTER_INTEGRATOR_CONTROL_DISABLE;
 *
 *     - all DECFILTER_COEF_COUNT coefficients = 0x00;
 */
void DECFILTER_DRV_GetDefaultConfig(decfilter_config_t * config);

/*!
 * @brief De-Initializes the DECFILTER module.
 *
 * This function reseting all registers of DECFILTER module to default values.
 * This function stop DMA channels started in DECFILTER_DRV_Init function.
 * In order to use the DECFILTER module again, DECFILTER_DRV_Init must be called again.
 *
 * @param[in] instance DECFILTER module instance number
 */
void DECFILTER_DRV_Deinit(const uint32_t instance);

/*!
 * @brief This function enables or disables the block input
 *
 * This function enables or disables the block input of DECFILTER,
 * so that writes to the input buffer have no effect
 * and input DMA or interrupt requests are not issued.
 * Input disabling is needed to change the block configuration to or from cascade mode or init
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] state what enable or disable input block
 */
void DECFILTER_DRV_EnableInputState(const uint32_t instance,
                                    const bool state);

/*!
 * @brief This function writes data and commands to the input buffer
 *
 * This function writes data and commands to the input buffer
 * User can set NULL cmd, corresponding to none command or
 * set all parameters for decfilter_input_buffer_cmd_t structure (data and comands)
 * User can set :
 *      - command : prefill and flush
 *      - selection of PSI ( if exists )
 *      - selection of inTag ( if exists )
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] data - input sample for decfilter
 * @param[in] cmd - pointer to decfilter_input_buffer_cmd_t structure (commands)
 * @return STATUS_SUCCESS - if data is write in register with success
 * @return STATUS_BUSY - if decfilter cannot write input buffer because is busy
 * @return STATUS_ERROR - if a error flag is set
 */
status_t DECFILTER_DRV_WriteInputData(const uint32_t instance,
                                      const uint32_t data,
                                      const decfilter_input_buffer_cmd_t * cmd);

/*!
 * @brief This function return the value output buffer
 *
 * This function return the value output buffer
 * This function also is clearing DECFILTER_STATUS_FLAG_OUTPUT_DATA
 *
 *
 * @param[in] instance DECFILTER module instance number
 * @return the value of DECFILTER_OB register
 */
uint16_t DECFILTER_DRV_ReadOutputData(const uint32_t instance);

/*!
 * @brief This function return the value output buffer
 *
 * This function return the value output buffer,
 * and also the rest of the information available in the register,
 * as separate members in data
 * This function also is clearing DECFILTER_STATUS_FLAG_OUTPUT_DATA
 *
 * @param[in] instance DECFILTER module instance number
 * @param[out] data - pointer to the output data is written
 */
void DECFILTER_DRV_ReadOutputInfo(const uint32_t instance,
                                  decfilter_output_buffer_t* const data);

/*!
 * @brief This function activate soft-reset procedure
 *
 * The Soft-Reset command is requested through the self-negated bit SRES of the
 * DECFILTER_MCR register and provides the CPU with the capability to initialize the
 * decimation filter through the slave-bus interface.
 * After the call of function decfilter is ready to precess new input data.
 *
 * @param[in] instance DECFILTER module instance number
 * @return STATUS_SUCCESS - if no busy timeout
 * @return STATUS_TIMEOUT - if decfilter is still busy after timeout expire
 */
status_t DECFILTER_DRV_SoftReset(const uint32_t instance, const uint32_t timeout);

/*!
 * @brief This function enables or disables the block input
 *
 * This function enable or disables the freeze mode also known as debug mode.
 * All filter action is frozen, either through software or by the hardware SoC debug request signal.
 * If a freeze request comes when the filter is processing an input,
 * it enters freeze mode only after the processing finishes.
 * In case of a freeze mode request during the processing of an input sample, the current
 * processing is finished and then the module enters freeze mode.
 * Access to input and output buffers remain operational in freeze, as well as their related flags.
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] state what enable or disable freeze mode
 */
void DECFILTER_DRV_FreezeMode(const uint32_t instance,
                              const bool state);

/*!
 * @brief This function clears the status flags provided in the bitmask
 *
 * This function clears the status flags provided in the bitmask from MSR register
 * @note Use only flags define in DECFILTER_STATUS_FLAG
 * Flags BSY and DEC_COUNTER are excluded because cannot be cleared (read only)
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] bitmask from DECFILTER_STATUS_FLAG_Class
 */
void DECFILTER_DRV_ClearStatusFlags(const uint32_t instance,
                                    const uint32_t bitmask);

/*!
 * @brief This function returns a bitmask with all the status flags for decfilter
 *
 * This function get all  flags from MSR register
 * @note Use only flags define in DECFILTER_STATUS_FLAG_Class
 *
 * @param[in] instance DECFILTER module instance number
 * @return bitmask with all the status flags for decfilter
 */
uint32_t DECFILTER_DRV_GetStatusFlags(const uint32_t instance);

/*!
 * @brief This function clears the integrator status flags provided in the bitmask
 *
 * This function clears the integrator status flags provided in the bitmask from MXSR register
 * @note Use only flags define in DECFILTER_INTEGRATOR_FLAGS
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] bitmask from DECFILTER_INTEGRATOR_FLAGS_Class
 */
void DECFILTER_DRV_ClearIntegratorFlags(const uint32_t instance,
                                        const uint32_t bitmask);

/*!
 * @brief This functions returns s bitmask with all the integrator status flags
 *
 * This function gets all flags from MXSR register
 * @note Use only flags defined in DECFILTER_INTEGRATOR_FLAGS
 *
 * @param[in] instance DECFILTER module instance number
 * @return bitmask with all the status flags for decfilter integrator
 */
uint32_t DECFILTER_DRV_GetIntegratorFlags(const uint32_t instance);

/*!
 * @brief This functions sets the update mode of the integrator output
 *
 * This functions is used to command the update mode of the integrator output,
 * reflected in the registers DECFILTER_FINTVAL and DECFILTER_FINTCNT.
 *
 * mode of update :
 *      - DECFILTER_INTEGRATOR_OUTPUT_UPDATE : to update of the final value and counter from current value and counter
 *      - DECFILTER_INTEGRATOR_OUTPUT_RESET :  reset current value and counter
 *      - DECFILTER_INTEGRATOR_OUTPUT_RESET_SYNCED : update the fine value and counter and reset current value and counter
 *      - DECFILTER_INTEGRATOR_OUTPUT_RESET_ALL : reset current and final value and counter
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] outputOperation type of integrator update mode
 */
void DECFILTER_DRV_SetIntegratorOutputMode(const uint32_t instance,
                                           const decfilter_integrator_output_operation_t outputOperation);

#if FEATURE_DECFILTER_ENHANCED_DEBUG
/*!
 * @brief This function gets the data loaded in decimation filter input register
 *
 * this function return the data that was loaded in the decimation filter to be processed
 * by the FIR/IIR sub-block. This conversion data is supplied by any of the two PSI slave-bus
 * interfaces and it is only defined when ISEL=0.
 *
 * @param[in] instance DECFILTER module instance number
 * @return the value of EDID register
 */
uint32_t DECFILTER_DRV_GetDebugEnhancedInputData(const uint32_t instance);

#endif

/*!
 * @brief This function gets Final Integer Value and Final Integer Count
 *
 * This function gets Final Integer Value and Final Integer Count
 * must be used after DECFILTER_DRV_SetIntegratorOutputMode function is called
 *
 * The data->value field holds the sum of filtered output values.
 * The 17 most significant bits hold the integer part, and the 15 least significant ones the fractional part of the integration value
 *
 * The data->count field holds the count of filtered outputs integrated.
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] pointer to value and counter structure
 */
void DECFILTER_DRV_GetFinalValues(const uint32_t instance,
                                  decfilter_integrator_values_t * data);

/*!
 * @brief This function gets Final Integer Value and Final Integer Count
 *
 * This function gets Final Integer Value and Final Integer Count
 * must used after DECFILTER_DRV_SetIntegratorOutputMode function is called
 *
 * The data->value field holds an unsigned number representing the sum of filtered output values,
 * continuously updated as the integration proceeds.
 * Accumulation overflow mode is set on config structure : setintegrator.saturatedOperation
 * Read on this register automatically commands an update of the register
 *
 * The data->count field holds the count of filtered outputs integrated.
 * The value is updated only when register DECFILTER_CINTVAL is read,
 * to keep the coherency between the integration and count values.
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] pointer to value and counter structure
 */
void DECFILTER_DRV_GetCurrentValues(const uint32_t instance,
                                    decfilter_integrator_values_t * data);


/*!
 * @brief This function returns the value of current tap register
 *
 * The TAP register stores, on each filter node, the input sample data the filter intermediary results.
 * The Filter TAP registers providing additional debug capabilities to the Decimation Filter block.
 * return value is fractional signed values in 2s complement format and is greater than or equal
 * to -1 and less than 1
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] index the index of tap register
 * @return current value of TAP register
 */
uint32_t DECFILTER_DRV_GetFilterTap(const uint32_t instance,
                                    const uint8_t index);

/*!
 * @brief This function enables the interrupts for decfilter , corresponding to the bits set in the bitmask parameter.
 *
 * @note All unset interrupt sources in the mask will keep their current state.
 * @note use only bitmasks from DECFILTER_INTERRUPT
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] bitmask may be set using DECFILTER_INTERRUPT defines
 *
 */
void DECFILTER_DRV_EnableInterrupts(const uint32_t instance,
                                    const uint32_t bitmask);

/*!
 * @brief This function disables the interrupts for decfilter , corresponding to the bits set in bitmask parameter.
 *
 * @note All unset interrupt sources in the mask will keep their current state.
 * @note use only bitmasks from DECFILTER_INTERRUPT
 *
 * @param[in] instance DECFILTER module instance number
 * @param[in] bitmask may be set using DECFILTER_INTERRUPT defines
 *
 */
void DECFILTER_DRV_DisableInterrupts(const uint32_t instance,
                                     const uint32_t bitmask);

#if defined(__cplusplus)
}
#endif

/*! @} */
#endif /* DECFILTER_DRIVER_H */
