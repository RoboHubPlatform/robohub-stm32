/**
 * @file stm32_spi.h
 * @brief SPI driver for the RoboHub HAL, on the STM32 HAL.
 *
 * SPI is a full-duplex bus: every clock shifts a byte out and shifts one in at
 * the same time. The RoboHub interface models a transfer as "these bytes out,
 * optionally these bytes in", which is what a caller means, and this driver
 * turns that into the vendor's full-duplex call by supplying whichever buffer
 * the caller did not.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_STM32_SPI_H
#define INCLUDE_ROBOHUB_PLATFORM_STM32_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "types.h"
#include "robohub/hal/spi.h"
#include "robohub/platform/stm32_hal.h"

/* -------------------------------------------------------------------------- */
/*  Modes                                                                      */
/* -------------------------------------------------------------------------- */
/*
 * The interface's `init` takes only a speed, so the bus shape is fixed by the
 * board. An SPI bus on a board is wired to one mode and one word size; a
 * peripheral that wanted another is on another bus. These are therefore
 * constants rather than parameters, chosen per instance in the context.
 */

/* -------------------------------------------------------------------------- */
/*  Context                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief What an SPI instance is bound to.
 *
 * Chip select is a GPIO rather than a field of the vendor's handle, because on
 * an STM32 the HAL does not drive CS: it leaves the pin alone and expects the
 * board to. A driver that did not own the pin would leave every transfer with
 * the device deselected, which is a bus that reads back as all ones.
 */
typedef struct {
    SPI_HandleTypeDef* handle;
    /** Chip select, as a GPIO instance. NULL means the board drives CS itself
     *  and this driver must not touch it. */
    struct RoboHubGPIO* chip_select;
    /** Bit mask of the chip-select pin within that instance. */
    rh_u32 chip_select_pin;
    rh_u32 speed_hz;
    rh_u32 timeout_ms;
    /** Set by a transfer so the two halves of a full-duplex call agree. */
    rh_u8 last_rx_scratch;
} RoboHubSTM32SPIContext;

#define RH_STM32_SPI_DEFAULT_TIMEOUT_MS 100u

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a RoboHub SPI instance to a peripheral and its chip select.
 *
 * @param context       the binding, which must outlive the instance
 * @param handle        the vendor's peripheral handle
 * @param chip_select   the GPIO instance driving CS, or NULL if the board drives
 *                      it itself and this driver must not touch it
 * @param chip_select_pin the mask of the CS pin within that instance
 * @param timeout_ms    transfer timeout, or zero for the default
 */
RoboHubError stm32_spi_attach(RoboHubSPI* self,
                              rh_name name,
                              RoboHubSTM32SPIContext* context,
                              SPI_HandleTypeDef* handle,
                              RoboHubGPIO* chip_select,
                              rh_u32 chip_select_pin,
                              rh_u32 timeout_ms);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_STM32_SPI_H */
