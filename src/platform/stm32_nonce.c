/**
 * @file stm32_nonce.c
 * @brief The platform's nonce source: the part's RNG peripheral, when it has one.
 *
 * Most STM32 families carry a true random generator (F4, F7, H7, L4, G4, U5...).
 * A part with one, and a HAL configuration that enables it, fills the nonce from
 * it. A part without one answers that it cannot, so the board opens no
 * authenticated session rather than one with a predictable nonce; such a board
 * needs an external entropy source, or ALLOW_PLAIN_FRAMES on its gateway. See
 * robohub_platform_nonce in platform.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"
#include "robohub/platform/stm32_hal.h"

#if defined(HAL_RNG_MODULE_ENABLED) && defined(RNG)

static RNG_HandleTypeDef rh_rng;

int robohub_platform_nonce(rh_u8* out, rh_size length)
{
    if (rh_rng.Instance == 0) {
#if defined(RCC_RNGCLKSOURCE_HSI48)
        /* The H7's RNG runs from the 48 MHz internal oscillator, which is off
         * after reset. */
        __HAL_RCC_HSI48_ENABLE();
        for (uint32_t wait = 0; !__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) && wait < 1000000u; wait++) {
        }
        __HAL_RCC_RNG_CONFIG(RCC_RNGCLKSOURCE_HSI48);
#endif
        __HAL_RCC_RNG_CLK_ENABLE();
        rh_rng.Instance = RNG;
        if (HAL_RNG_Init(&rh_rng) != HAL_OK) {
            rh_rng.Instance = 0;
            return -1;
        }
    }
    rh_size done = 0;
    while (done < length) {
        uint32_t value = 0;
        if (HAL_RNG_GenerateRandomNumber(&rh_rng, &value) != HAL_OK) {
            return -1;
        }
        for (int i = 0; i < 4 && done < length; i++) {
            out[done++] = (rh_u8)(value >> (8 * i));
        }
    }
    return 0;
}

#else

int robohub_platform_nonce(rh_u8* out, rh_size length)
{
    (void)out;
    (void)length;
    return -1;
}

#endif
