/**
 * @file stm32_platform.h
 * @brief What the platform provides that is a family's rather than a board's.
 *
 * The clock tree is here because it is a fact about the family and its drivers.
 * Which pins the console and the LEDs are on is not here, because that is the
 * board's: a board's support package asks for the clock, and owns everything else
 * about itself.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_STM32_PLATFORM_H
#define INCLUDE_ROBOHUB_PLATFORM_STM32_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Bring the part up: the MPU, the HAL, and the family clock tree.
 *
 * The order is the one the vendor's own example for a Nucleo-144 part uses, and
 * it matters: the MPU before the HAL, and the supply before the clock. */
void stm32_platform_clock_config(void);
void stm32_platform_clock_config(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_STM32_PLATFORM_H */
