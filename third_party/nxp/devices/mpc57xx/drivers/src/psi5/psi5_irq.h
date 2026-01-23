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

#ifndef PSI5_DRIVER_IRQ_H
#define PSI5_DRIVER_IRQ_H

/* Required headers */
#include <stddef.h>
#include "device_registers.h"

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
 * @brief Driver main Interrupt handler
 *
 * Called from IRQ
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 */
void PSI5_IRQ_Handler(const uint32_t instance,
                      const uint32_t channel);

/*!
 * @brief Sets interrupt state
 *
 * Enables disables peripheral interrupts
 *
 * @param[in] instance PSI5 instance
 * @param[in] channel PSI5 channel
 * @param[in] state Interrupt state
 */
void PSI5_IRQ_SetState(const uint32_t instance,
                       const uint32_t channel,
                       const bool state);

#if defined(__cplusplus)
}
#endif

/*! @}*/

/*! @}*/ /* End of addtogroup psi5 */

#endif /* PSI5_DRIVER_IRQ_H */
