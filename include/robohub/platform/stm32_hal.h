/**
 * @file stm32_hal.h
 * @brief The vendor library this platform is built against.
 *
 * Every driver in this module reaches the STM32 HAL through this header and
 * through nothing else, so that the one place the vendor's names appear is one
 * file. A driver that included `stm32h7xx_hal.h` directly would be a driver
 * that only builds for the family whose header it named, and the mistake would
 * surface as a compile error on some other board rather than as a wrong answer.
 *
 * The HAL is included by its own name, and which name that is comes from the
 * build: a target spec for a part says which configuration header that part uses
 * and defines it here. The alternative — a header that picked a name — would be
 * right for one family and wrong for every other, and the wrongness would only
 * appear on the boards nobody was holding at the time.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The build says which part this is, by naming the HAL header for it. A header
 * that picked the name itself would be right for one family and wrong for every
 * other, and the wrongness would appear only on the boards nobody had to hand.
 *
 * The HAL is included before anything else, and not the configuration before it
 * as one might expect. The vendor's headers are mutually recursive —
 * `hal_def.h` includes a legacy header that includes a module header that needs a
 * type `hal_def.h` has not defined yet — and the cycle only closes because the
 * HAL header guards itself on the way in. Including the configuration first
 * reaches the module header before that guard is set, and the build fails inside
 * the vendor's own headers with an error about a type nobody in this project
 * mentioned. */
#if !defined(ROBOHUB_STM32_HAL_HEADER)
#error "This build did not say which STM32 family it is for. A target spec for the part declares ROBOHUB_STM32_HAL_HEADER; nothing here can know it."
#endif

#include ROBOHUB_STM32_HAL_HEADER

#ifdef __cplusplus
}
#endif
