/**
 * @file stm32_rs485.h
 * @brief RS485 driver for the RoboHub HAL, on the STM32 HAL.
 *
 * RS485 is a UART on a half-duplex bus: the transceiver listens and transmits
 * at the same time, and a driver enable pin decides which. The HAL can drive
 * that pin itself in hardware RS485 mode, and where it can, this driver uses
 * that — the turnaround then happens in hardware, at the instant the last stop
 * bit is on the wire, rather than in software after the driver has already
 * begun listening.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_STM32_RS485_H
#define INCLUDE_ROBOHUB_PLATFORM_STM32_RS485_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "types.h"
#include "robohub/hal/rs485.h"
#include "robohub/platform/stm32_hal.h"
#include "robohub/platform/stm32_uart.h"

/* -------------------------------------------------------------------------- */
/*  Direction                                                                  */
/* -------------------------------------------------------------------------- */
/*
 * The interface's `set_direction` takes a number without saying what the numbers
 * are, so they are stated here. These belong in robohub-common beside the
 * interface that takes them, for the same reason the GPIO modes do.
 */
typedef enum {
    RH_RS485_RX = 0,  /* listen */
    RH_RS485_TX = 1   /* transmit */
} RoboHubRS485Direction;

/* -------------------------------------------------------------------------- */
/*  Context                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief What an RS485 instance is bound to.
 *
 * The UART half is a full UART context, reused rather than copied: RS485 is a
 * UART on a different transceiver, and the two drivers then have one
 * implementation of the byte handling rather than two that can disagree.
 */
typedef struct {
    RoboHubSTM32UARTContext uart;
    /** The driver-enable pin, for the families whose hardware cannot drive it
     *  itself. NULL when the peripheral is in hardware RS485 mode. */
    struct RoboHubGPIO* driver_enable;
    rh_u32 driver_enable_pin;
    /** Which way the enable pin reads to transmit. A board wires DE either way
     *  round, and driving it the wrong way is a bus that is permanently muted. */
    rh_u32 enable_level_for_tx;
    /** Set when the driver is driving the pin itself rather than the hardware. */
    rh_u32 software_direction;
    rh_u32 last_direction;
} RoboHubSTM32RS485Context;

#define RH_STM32_RS485_DEFAULT_TIMEOUT_MS 100u

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a RoboHub RS485 instance.
 *
 * @param driver_enable   the DE pin as a GPIO instance, or NULL when the
 *                        peripheral drives it in hardware RS485 mode
 * @param enable_level_for_tx the value on that pin that selects transmit
 */
RoboHubError stm32_rs485_attach(RoboHubRS485* self,
                                rh_name name,
                                RoboHubSTM32RS485Context* context,
                                UART_HandleTypeDef* handle,
                                RoboHubGPIO* driver_enable,
                                rh_u32 driver_enable_pin,
                                rh_u32 enable_level_for_tx,
                                rh_u32 timeout_ms);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_STM32_RS485_H */
