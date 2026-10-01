/**
 * @file stm32_i2c.h
 * @brief I2C driver for the RoboHub HAL, on the STM32 HAL.
 *
 * An I2C instance is a peripheral and a bus speed. The RoboHub interface names
 * a bus by 7-bit address and transfers a buffer; everything the vendor's HAL
 * asks for beyond that — the shifted address, the timeout, the ack — is this
 * driver's business and not a caller's.
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
#include "robohub/hal/i2c.h"
#include "robohub/platform/stm32_hal.h"

/* -------------------------------------------------------------------------- */
/*  Context                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief What an I2C instance is bound to.
 *
 * The handle is the vendor's own: the HAL keeps its state there, and a driver
 * that kept its own copy of that state would be a second source of truth about
 * a peripheral the HAL is already driving.
 */
typedef struct {
    I2C_HandleTypeDef* handle;
    /** Clock speed requested by `init`, in Hz. Kept so a re-init is idempotent
     *  and so a caller can ask what the bus is running at. */
    rh_u32 speed_hz;
    /** Transfer timeout in milliseconds. Zero selects the default below. */
    rh_u32 timeout_ms;
} RoboHubSTM32I2CContext;

/** The timeout a transfer uses when the board does not choose one: long enough
 *  for a slow device to answer, short enough that a bus with nothing driving it
 *  fails rather than hanging the program that asked. */
#define RH_STM32_I2C_DEFAULT_TIMEOUT_MS 100u

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a RoboHub I2C instance to a peripheral.
 *
 * The peripheral is not configured here; `init` does that, because the bus speed
 * is a property of what is on the bus rather than of the port.
 */
RoboHubError stm32_i2c_attach(RoboHubI2C* self,
                              rh_name name,
                              I2C_HandleTypeDef* handle,
                              rh_u32 timeout_ms);

/**
 * @brief Probe whether a device acknowledges on the bus.
 *
 * Not part of the interface: a board uses it to find out what is on the bus
 * before it decides to talk to anything.
 *
 * @param addr the device's unshifted 7-bit address, as the interface states it.
 */
RoboHubError stm32_i2c_probe(RoboHubI2C* self, rh_u8 addr);

#ifdef __cplusplus
}
#endif
