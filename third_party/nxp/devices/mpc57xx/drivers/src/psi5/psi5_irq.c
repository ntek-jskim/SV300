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
 * Violates MISRA 2012 Required Rule 8.4, external symbol defined without a prior
 * declaration.
 * These are symbols weak symbols defined in platform startup files (.s).
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.7, external could be made static.
 * The functions are called by the interrupt controller when the appropriate event
 * occurs.
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 8.9, Could define variable at block scope.
 * The variables are ROM-able and are shared between different functions in the module.
 */

#include <stddef.h>
#include "psi5_irq.h"
#include "device_registers.h"
#include "interrupt_manager.h"

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief Interrupt mapping array
 */
const IRQn_Type s_psi5InterruptMappings[PSI5_INSTANCE_COUNT][FEATURE_PSI5_CHANNEL_COUNT] = FEATURE_PSI5_IRQS;

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI5_IRQ_SetState
 * Description   : Sets the interrupt state
 *
 *END**************************************************************************/
void PSI5_IRQ_SetState(const uint32_t instance,
                       const uint32_t channel,
                       const bool state)
{
    IRQn_Type irq = s_psi5InterruptMappings[instance][channel];

    /* Depending on state */
    if (state)
    {
        INT_SYS_EnableIRQ(irq);
    }
    else
    {
        INT_SYS_DisableIRQ(irq);
    }
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI50_SDOE_IRQHandler
 * Description   : Generic IRQ for instance 0
 *
 *END**************************************************************************/
void PSI50_SDOE_IRQHandler(void)
{
    PSI5_IRQ_Handler(0u, 0u);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : PSI51_SDOE_IRQHandler
 * Description   : Generic IRQ for instance 1
 *
 *END**************************************************************************/
void PSI51_SDOE_IRQHandler(void)
{
    PSI5_IRQ_Handler(1u, 0u);
}

/*******************************************************************************
* EOF
*******************************************************************************/
