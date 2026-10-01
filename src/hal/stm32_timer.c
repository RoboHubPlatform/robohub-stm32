/**
 * @file stm32_timer.c
 * @brief Timer driver for the RoboHub HAL, on the STM32 HAL.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/platform/stm32_timer.h"
#include "robohub/platform/stm32_hal.h"

/* -------------------------------------------------------------------------- */
/*  Internals                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * The tick the HAL maintains, as milliseconds, and the microsecond remainder
 * SysTick's current value carries.
 *
 * `HAL_GetTick` counts interrupts, so it has the resolution of the tick — a
 * millisecond — and a microsecond reading has to come from the counter the
 * interrupt is derived from. Reading SysTick->VAL directly gives the fraction
 * that `HAL_GetTick` cannot see, and the two together are a microsecond clock
 * that needs no timer of its own.
 */

/* The number of ticks the counter wraps at. The HAL programs SysTick with this
 * as its reload value, so the counter runs 0..RELOAD and the period is
 * RELOAD + 1 counts. */
#define RH_STM32_SYSTICK_RELOAD (SysTick->LOAD + 1u)

/* The core frequency the tick was programmed for. A board that reprograms its
 * clocks changes this, and a microsecond reading taken against the old value
 * would drift by exactly the ratio the clock moved. Zero means the core clock
 * has not been recorded, in which case microseconds are derived from the tick
 * alone and are as good as the tick. */
static rh_u32 s_core_hz = 0;

static rh_micros tick_to_micros(rh_u32 ticks)
{
    rh_u64 elapsed = (rh_u64)ticks * RH_STM32_SYSTICK_RELOAD;
    if (s_core_hz == 0) {
        return (rh_micros)elapsed;
    }
    return (rh_micros)((elapsed * 1000000u) / (rh_u64)s_core_hz);
}

/* The fraction of the current millisecond, in microseconds, read from the
 * counter. Only meaningful while the tick is running; a board that has not
 * started it reads whatever the counter happens to hold, which is why
 * `stm32_timer_start` is part of platform init. */
static rh_micros sub_millis_micros(void)
{
    rh_u32 current = RH_STM32_SYSTICK_RELOAD - (SysTick->VAL & RH_STM32_SYSTICK_RELOAD);
    return tick_to_micros(current);
}

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                       */
/* -------------------------------------------------------------------------- */

static rh_millis op_get_millis(RoboHubTimer* self)
{
    (void)self;
    return (rh_millis)HAL_GetTick();
}

static rh_micros op_get_micros(RoboHubTimer* self)
{
    (void)self;
    /* Read the tick first and the counter second. The other order can read a
     * counter that belongs to the millisecond the tick has not been counted
     * into yet, which is a reading that is up to a millisecond early — and
     * early by a different amount each time, so two readings a microsecond
     * apart can come out reversed. */
    rh_u32 millis = HAL_GetTick();
    rh_micros sub = sub_millis_micros();
    return (rh_micros)millis * 1000000u + sub;
}

static void op_delay_ms(RoboHubTimer* self, rh_u32 ms)
{
    (void)self;
    HAL_Delay(ms);
}

static void op_delay_us(RoboHubTimer* self, rh_u32 us)
{
    (void)self;

    /*
     * The HAL's own delay is milliseconds, so a microsecond delay is built from
     * the DWT cycle counter — the only counter on the core that keeps counting
     * while the CPU is in a tight loop, and the one CMSIS provides for exactly
     * this.
     *
     * The DWT is not always enabled, and a delay that is silently the wrong
     * length is worse than one that is refused, so it is turned on here. On a
     * core without the DWT — an M0, which has no cycle counter at all — the
     * delay falls back to the millisecond tick and rounds up, because a delay
     * that runs long is a delay a caller can still reason about and one that
     * returns early is not.
     */
#if defined(__CORTEX_M) && (__CORTEX_M >= 3)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    rh_u32 cycles_per_us = s_core_hz / 1000000u;
    if (cycles_per_us == 0) {
        /* The core clock was never recorded, so the cycle count cannot be
         * turned into a time. A millisecond is the smallest delay that can be
         * had without it. */
        HAL_Delay((us + 999u) / 1000u);
        return;
    }

    rh_u32 start = DWT->CYCCNT;
    rh_u32 ticks = us * cycles_per_us;
    /* The counter is 32 bits and wraps, so the comparison is a difference: a
     * delay longer than one wrap of the counter would otherwise compare a
     * small number against a small number and finish immediately. */
    while ((rh_u32)(DWT->CYCCNT - start) < ticks) {
    }
#else
    /* No cycle counter on this core. Round up to the tick that is available,
     * so the delay is never shorter than asked for. */
    if (us == 0) {
        return;
    }
    HAL_Delay(1u);
#endif
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_timer_start(void)
{
    /* Record the frequency the tick is running at before starting it, so that a
     * microsecond reading has a rate to convert with. The HAL programs SysTick
     * from the core clock and the interrupt from the same place, so the two
     * agree from here on. */
    s_core_hz = HAL_RCC_GetHCLKFreq();

    /* HAL_Init has already configured SysTick on every family this platform
     * targets, so the timebase is running. What has to be true is that it is
     * running, and a board that has changed clocks since HAL_Init has to
     * re-establish it; the caller does that by calling this after its own
     * clock setup rather than before it. */
    return (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) ? ROBOHUB_OK : ROBOHUB_ERR_STATE;
}

RoboHubError stm32_timer_attach(RoboHubTimer* self,
                                rh_name name,
                                const RoboHubSTM32TimerContext* context)
{
    if (self == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    self->name = name;
    self->context = (void*)context;
    self->get_millis = op_get_millis;
    self->get_micros = op_get_micros;
    self->delay_ms = op_delay_ms;
    self->delay_us = op_delay_us;
    return ROBOHUB_OK;
}

rh_millis stm32_timer_get_millis(RoboHubTimer* self)
{
    if (self == NULL || self->get_millis == NULL) {
        return 0;
    }
    return self->get_millis(self);
}

rh_micros stm32_timer_get_micros(RoboHubTimer* self)
{
    if (self == NULL || self->get_micros == NULL) {
        return 0;
    }
    return self->get_micros(self);
}

void stm32_timer_delay_ms(RoboHubTimer* self, rh_u32 ms)
{
    if (self == NULL || self->delay_ms == NULL) {
        return;
    }
    self->delay_ms(self, ms);
}

void stm32_timer_delay_us(RoboHubTimer* self, rh_u32 us)
{
    if (self == NULL || self->delay_us == NULL) {
        return;
    }
    self->delay_us(self, us);
}
