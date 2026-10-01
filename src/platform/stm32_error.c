/**
 * @file stm32_error.c
 * @brief Mapping STM32 HAL status codes onto RoboHub error codes.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/platform/stm32_error.h"

RoboHubError robohub_stm32_to_error(int hal_status)
{
    switch (hal_status) {
    case RH_STM32_HAL_OK:
        return ROBOHUB_OK;

    case RH_STM32_HAL_ERROR:
        return ROBOHUB_ERR_FAIL;

    case RH_STM32_HAL_BUSY:
        return ROBOHUB_ERR_BUSY;

    case RH_STM32_HAL_TIMEOUT:
        return ROBOHUB_ERR_TIMEOUT;

    default:
        /* A status nobody mapped is a failure nobody has described yet. It is
         * not success, and saying so is what lets a caller tell an unmapped
         * vendor status apart from a driver that worked. */
        return ROBOHUB_ERR_UNKNOWN;
    }
}

RoboHubError robohub_stm32_error_mapper(int platform_error)
{
    return robohub_stm32_to_error(platform_error);
}
