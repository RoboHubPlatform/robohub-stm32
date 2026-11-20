/**
 * @file stm32_health.c
 * @brief robohub-common's health contracts on the STM32 F4 and H7 (STM-0008).
 *
 * - **Watchdog**: the independent watchdog (IWDG; IWDG1 on the H7), clocked by
 *   the ~32 kHz LSI. The prescaler (4..256) and the 12-bit reload give 1 ms to
 *   32 768 ms; the smallest prescaler that reaches the period is taken, so the
 *   period is as close as the reload allows. Once started it cannot be stopped
 *   by anything but a reset, and it has no early warning here.
 * - **Reset reason**: the RCC reset flags (CSR on the F4, RSR on the H7), read
 *   once and then cleared (RMVF) so the next boot reads only its own. The NRST
 *   flag is set by every reset that drives the pin, internal ones too, so it is
 *   the reason only when nothing else is.
 * - **Retained memory**: the first eight RTC backup registers, made writable
 *   (PWR's DBP) at first use. A reset keeps them; losing power clears them
 *   unless the board has a VBAT battery, in which case the boot count also
 *   survives a power cut.
 * - **Faults**: the platform's fault handler (stm32_fault.c) writes the record
 *   before its report; it then stops, and a running watchdog resets the board.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/health.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive over robohub-common's
 * weak defaults. */
// @req STM-0008
const char rh_health_platform[] = "stm32";

/* Compiled for a board only (CMakeLists.txt: the bare-metal sources). */
#include "robohub/platform/stm32_hal.h"

#if defined(IWDG1)
#define RH_IWDG IWDG1
#else
#define RH_IWDG IWDG
#endif

#define RH_IWDG_KEY_START  0xCCCCu
#define RH_IWDG_KEY_UNLOCK 0x5555u
#define RH_IWDG_KEY_FEED   0xAAAAu
#define RH_LSI_HZ          32000u
#define RH_IWDG_MAX_RELOAD 0xFFFu
#define RH_IWDG_MAX_MS     32768u

static bool stm32_wdt_running;

// @req STM-0008
void rh_watchdog_caps_get(rh_watchdog_caps *caps)
{
    if (caps) {
        caps->present = true;
        caps->stoppable = false;
        caps->early_warning = false;
        caps->min_ms = 1u;
        caps->max_ms = RH_IWDG_MAX_MS;
    }
}

// @req STM-0008
RoboHubError rh_watchdog_start(uint32_t period_ms, uint32_t *chosen_ms)
{
    if (period_ms == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (period_ms > RH_IWDG_MAX_MS) {
        period_ms = RH_IWDG_MAX_MS;
    }
    /* ticks = period * LSI / (4 << pr); the first prescaler whose reload fits. */
    uint32_t pr = 0u;
    uint32_t reload = 0u;
    for (pr = 0u; pr <= 6u; pr++) {
        uint32_t divider = 4u << pr;
        reload = (uint32_t)(((uint64_t)period_ms * RH_LSI_HZ) / (1000u * divider));
        if (reload == 0u) {
            reload = 1u;
        }
        if (reload - 1u <= RH_IWDG_MAX_RELOAD) {
            break;
        }
    }
    if (pr > 6u) {
        pr = 6u;
        reload = RH_IWDG_MAX_RELOAD + 1u;
    }

    RH_IWDG->KR = RH_IWDG_KEY_START;
    RH_IWDG->KR = RH_IWDG_KEY_UNLOCK;
    RH_IWDG->PR = pr;
    RH_IWDG->RLR = reload - 1u;
    while (RH_IWDG->SR != 0u) {
        /* the new prescaler and reload reach the LSI domain */
    }
    RH_IWDG->KR = RH_IWDG_KEY_FEED;
    stm32_wdt_running = true;

    if (chosen_ms) {
        *chosen_ms = (uint32_t)(((uint64_t)reload * (4u << pr) * 1000u) / RH_LSI_HZ);
    }
    return ROBOHUB_OK;
}

// @req STM-0008
void rh_watchdog_feed(void)
{
    if (stm32_wdt_running) {
        RH_IWDG->KR = RH_IWDG_KEY_FEED;
    }
}

// @req STM-0008
RoboHubError rh_watchdog_stop(void)
{
    return ROBOHUB_ERR_NOT_SUPPORTED;
}

#if defined(RCC_RSR_RMVF)
#define RH_RESET_FLAGS (RCC->RSR)
#define RH_RMVF        RCC_RSR_RMVF
#define RH_F_IWDG      RCC_RSR_IWDG1RSTF
#define RH_F_WWDG      RCC_RSR_WWDG1RSTF
#define RH_F_SFT       RCC_RSR_SFTRSTF
#define RH_F_POR       RCC_RSR_PORRSTF
#define RH_F_BOR       RCC_RSR_BORRSTF
#define RH_F_PIN       RCC_RSR_PINRSTF
#else
#define RH_RESET_FLAGS (RCC->CSR)
#define RH_RMVF        RCC_CSR_RMVF
#define RH_F_IWDG      RCC_CSR_IWDGRSTF
#define RH_F_WWDG      RCC_CSR_WWDGRSTF
#define RH_F_SFT       RCC_CSR_SFTRSTF
#define RH_F_POR       RCC_CSR_PORRSTF
#define RH_F_BOR       RCC_CSR_BORRSTF
#define RH_F_PIN       RCC_CSR_PINRSTF
#endif

// @req STM-0008
rh_reset_reason rh_platform_reset_reason(void)
{
    uint32_t flags = RH_RESET_FLAGS;
    RH_RESET_FLAGS |= RH_RMVF;

    if (flags & (RH_F_IWDG | RH_F_WWDG)) {
        return RH_RESET_WATCHDOG;
    }
    if (flags & RH_F_SFT) {
        return RH_RESET_SOFTWARE;
    }
    if (flags & RH_F_POR) {
        return RH_RESET_POWER_ON;  /* a power-on raises BOR too */
    }
    if (flags & RH_F_BOR) {
        return RH_RESET_BROWN_OUT;
    }
    if (flags & RH_F_PIN) {
        return RH_RESET_PIN;
    }
    return RH_RESET_UNKNOWN;
}

/* The backup registers are behind the backup domain's write protection; lift it
 * once. Register writes only, so a fault handler may be the first to ask. */
// @req STM-0008
void *rh_platform_retained(void)
{
#if defined(PWR_CR1_DBP)
    RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    PWR->CR1 |= PWR_CR1_DBP;
#else
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
#endif
    return (void *)&RTC->BKP0R;
}
