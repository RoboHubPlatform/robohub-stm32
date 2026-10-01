/**
 * @file stm32_uart.h
 * @brief UART driver for the RoboHub HAL, on the STM32 HAL.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "types.h"
#include "robohub/hal/uart.h"
#include "robohub/platform/stm32_hal.h"

/* -------------------------------------------------------------------------- */
/*  Context                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief What a UART instance is bound to.
 */
typedef struct {
    UART_HandleTypeDef* handle;
    rh_u32 baudrate;
    rh_u32 timeout_ms;
    /** Receives into this instead of the caller's buffer when the caller asks
     *  how much is waiting. A board sets it to a buffer of its own so that a
     *  byte arriving before anything asked for it is not lost. */
    rh_u8* rx_buffer;
    rh_size rx_capacity;
} RoboHubSTM32UARTContext;

#define RH_STM32_UART_DEFAULT_TIMEOUT_MS 100u

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_uart_attach(RoboHubUART* self,
                               rh_name name,
                               RoboHubSTM32UARTContext* context,
                               UART_HandleTypeDef* handle,
                               rh_u32 timeout_ms);

/**
 * @brief Take a buffer the driver may receive into before anyone asks.
 *
 * A board that polls calls this with its own buffer; a board that is driven by
 * interrupts does not need it, because its receive complete interrupt reads the
 * peripheral directly.
 */
RoboHubError stm32_uart_set_rx_buffer(RoboHubUART* self, rh_u8* buffer, rh_size capacity);

/**
 * @brief How many bytes are waiting to be read.
 *
 * Not part of the interface: a caller that polls needs to know whether a read
 * would block, and the interface's read is a blocking read of a stated length.
 */
rh_size stm32_uart_available(RoboHubUART* self);

#ifdef __cplusplus
}
#endif
