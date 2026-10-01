/**
 * @file stm32_error.h
 * @brief Mapping STM32 HAL status codes onto RoboHub error codes.
 *
 * The STM32 HAL reports failure through HAL_StatusTypeDef, whose values are a
 * stable part of the vendor's ABI: HAL_OK is 0, HAL_ERROR is 1, HAL_BUSY is 2
 * and HAL_TIMEOUT is 3, and they have been since the F1. The mapping below is
 * written against those numbers rather than against the names so that it
 * compiles for any STM32 family — and compiles on the host, which is the only
 * way it can be tested without a board on the bench.
 *
 * A family that adds a status of its own maps it here rather than in each
 * driver, so a caller sees one set of codes whatever produced them.
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

/* -------------------------------------------------------------------------- */
/*  STM32 HAL status values                                                    */
/* -------------------------------------------------------------------------- */
/*
 * Spelled out here rather than included, because the values are the vendor's
 * published ABI and depending on a header to state them would make this file
 * unbuildable anywhere the CMSIS headers are not present — which is the host,
 * which is where it is tested.
 */
#define RH_STM32_HAL_OK       0
#define RH_STM32_HAL_ERROR    1
#define RH_STM32_HAL_BUSY     2
#define RH_STM32_HAL_TIMEOUT  3

/* -------------------------------------------------------------------------- */
/*  Mapping                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Map a HAL_StatusTypeDef value onto a RoboHubError.
 *
 * A status this platform does not recognise is reported as
 * ROBOHUB_ERR_UNKNOWN rather than as success: an unrecognised non-zero status
 * is a failure nobody has mapped yet, and calling it success is the one answer
 * that cannot be recovered from.
 */
RoboHubError robohub_stm32_to_error(int hal_status);

/**
 * @brief Map a HAL status reported through the `RoboHubErrorMapper` signature.
 *
 * Declared so the mapping can be handed to anything in robohub-common that
 * takes a mapper, rather than every caller casting to the right type first.
 */
RoboHubError robohub_stm32_error_mapper(int platform_error);

#ifdef __cplusplus
}
#endif
