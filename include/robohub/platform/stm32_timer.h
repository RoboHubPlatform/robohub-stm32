/**
 * @file stm32_timer.h
 * @brief Timer driver for the RoboHub HAL, on the STM32 HAL.
 *
 * The interface is a clock and a pair of delays, and on an STM32 both come from
 * SysTick, which the vendor's HAL already runs as a millisecond tick. This
 * driver reads that rather than configuring a timer of its own: a second timer
 * spent counting is a timer a board cannot spend on something else, and the one
 * the HAL owns is already there.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_STM32_TIMER_H
#define INCLUDE_ROBOHUB_PLATFORM_STM32_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "types.h"
#include "robohub/hal/timer.h"

/* -------------------------------------------------------------------------- */
/*  Context                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief What a timer instance is bound to.
 *
 * There is nothing to configure: SysTick is a property of the core, and the HAL
 * starts it before any of this runs. The context exists so that an instance has
 * somewhere to hang state a later board might need — a timebase chosen from the
 * low-power clocks, say — without changing the interface.
 */
typedef struct {
    /** Divide the core clock by this to get the tick frequency. Zero means the
     *  HAL's own default, which is what a board that has not asked for
     *  anything else gets. */
    rh_u32 reload_override;
} RoboHubSTM32TimerContext;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a RoboHub timer instance.
 *
 * @param context may be NULL, in which case the HAL's default timebase is used.
 */
RoboHubError stm32_timer_attach(RoboHubTimer* self,
                                rh_name name,
                                const RoboHubSTM32TimerContext* context);

rh_millis stm32_timer_get_millis(RoboHubTimer* self);
rh_micros stm32_timer_get_micros(RoboHubTimer* self);
void stm32_timer_delay_ms(RoboHubTimer* self, rh_u32 ms);
void stm32_timer_delay_us(RoboHubTimer* self, rh_u32 us);

/**
 * @brief Start the timebase the rest of this platform reads.
 *
 * Called from `robohub_platform_init`. The HAL normally does this itself, but a
 * board that configures its own clocks has to re-establish the tick afterwards,
 * because SysTick counts the clock it was given and the clock has just changed.
 */
RoboHubError stm32_timer_start(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_STM32_TIMER_H */
